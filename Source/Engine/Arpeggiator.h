#pragma once

#include <algorithm>
#include <array>
#include <cstdint>

#include <juce_audio_basics/juce_audio_basics.h>

#include "Core/TempoSync.h"

namespace aethr::engine
{

enum class ArpMode : std::uint8_t
{
    up = 0,
    down,
    upDown,
    asPlayed,
    random,
    chord,
    numModes
};

struct ArpSettings
{
    bool enabled { false };
    ArpMode mode { ArpMode::up };
    int divisionIndex { 3 };
    int octaves { 1 };
    double gate { 0.55 };
    double swing { 0.0 };
    bool latch { false };
    std::uint16_t pattern { 0xFFFFu };
};

/**
    Tempo-synced arpeggiator / 16-step gate.

    Held notes are captured; the arp emits sample-accurate note-ons into a
    private MIDI buffer. Other MIDI (pitch bend, CC, sustain) is passed through.
*/
class Arpeggiator
{
public:
    static constexpr int maxHeld = 16;
    static constexpr int numSteps = 16;

    void prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
        reset();
    }

    void reset() noexcept
    {
        heldCount = 0;
        stepIndex = 0;
        walk = 0;
        walkDirection = 1;
        samplesUntilStep = 0;
        gateSamplesLeft = 0;
        evenStep = true;
        soundingCount = 0;
        noteOrder = 0;
        rng = 0xA57E11u;
    }

    void panic() noexcept
    {
        reset();
    }

    void setSettings (const ArpSettings& next) noexcept { settings = next; }

    [[nodiscard]] int currentStep() const noexcept { return stepIndex; }

    void process (const juce::MidiBuffer& incoming,
                  juce::MidiBuffer& outgoing,
                  int numSamples,
                  double bpm) noexcept
    {
        outgoing.clear();

        if (! settings.enabled)
        {
            outgoing.addEvents (incoming, 0, numSamples, 0);
            return;
        }

        const auto stepSamples = std::max (1, static_cast<int> (
            sync::seconds (settings.divisionIndex, bpm) * sampleRate));

        auto incomingIt = incoming.begin();
        const auto incomingEnd = incoming.end();
        auto cursor = 0;

        const auto handleIncoming = [this, &outgoing] (const juce::MidiMessage& message, int time)
        {
            if (message.isNoteOn())
            {
                addHeld (message.getNoteNumber(), message.getFloatVelocity());
            }
            else if (message.isNoteOff())
            {
                if (! settings.latch)
                    removeHeld (message.getNoteNumber());
            }
            else if (message.isAllNotesOff() || message.isAllSoundOff())
            {
                heldCount = 0;
                releaseSounding (outgoing, time);
            }
            else
            {
                outgoing.addEvent (message, time);
            }
        };

        while (cursor < numSamples)
        {
            int nextIncoming = numSamples;

            if (incomingIt != incomingEnd)
            {
                const auto time = (*incomingIt).samplePosition;
                if (time < numSamples)
                    nextIncoming = std::max (cursor, time);
            }

            const auto nextClock = cursor + samplesUntilStep;
            const auto nextEvent = std::min (nextIncoming, std::min (nextClock, numSamples));

            if (gateSamplesLeft > 0)
            {
                const auto consumed = nextEvent - cursor;
                gateSamplesLeft -= consumed;

                if (gateSamplesLeft <= 0)
                {
                    releaseSounding (outgoing, nextEvent + gateSamplesLeft);
                    gateSamplesLeft = 0;
                }
            }

            if (nextEvent == nextIncoming && incomingIt != incomingEnd)
            {
                handleIncoming ((*incomingIt).getMessage(), nextIncoming);
                ++incomingIt;
            }

            samplesUntilStep -= nextEvent - cursor;
            cursor = nextEvent;

            if (cursor >= numSamples)
                break;

            if (samplesUntilStep <= 0)
            {
                const auto swing = evenStep ? 1.0 : (1.0 + std::clamp (settings.swing, 0.0, 0.75));
                samplesUntilStep = std::max (1, static_cast<int> (static_cast<double> (stepSamples) * swing));
                evenStep = ! evenStep;
                advanceStep (outgoing, cursor, stepSamples);
            }
        }

        while (incomingIt != incomingEnd)
        {
            const auto time = std::clamp ((*incomingIt).samplePosition, 0, numSamples - 1);
            handleIncoming ((*incomingIt).getMessage(), time);
            ++incomingIt;
        }
    }

    void collectOffEvents (juce::MidiBuffer& outgoing) noexcept
    {
        releaseSounding (outgoing, 0);
    }

private:
    struct HeldNote
    {
        int note { 0 };
        float velocity { 1.0f };
        std::uint32_t order { 0 };
    };

    void addHeld (int note, float velocity) noexcept
    {
        for (int i = 0; i < heldCount; ++i)
            if (held[static_cast<std::size_t> (i)].note == note)
            {
                held[static_cast<std::size_t> (i)].velocity = velocity;
                return;
            }

        if (heldCount >= maxHeld)
            return;

        held[static_cast<std::size_t> (heldCount)] = { note, velocity, ++noteOrder };
        ++heldCount;
        sortHeld();
    }

    void removeHeld (int note) noexcept
    {
        for (int i = 0; i < heldCount; ++i)
        {
            if (held[static_cast<std::size_t> (i)].note != note)
                continue;

            for (int j = i; j < heldCount - 1; ++j)
                held[static_cast<std::size_t> (j)] = held[static_cast<std::size_t> (j + 1)];

            --heldCount;
            return;
        }
    }

    void sortHeld() noexcept
    {
        for (int i = 1; i < heldCount; ++i)
        {
            auto key = held[static_cast<std::size_t> (i)];
            auto j = i - 1;

            while (j >= 0 && held[static_cast<std::size_t> (j)].note > key.note)
            {
                held[static_cast<std::size_t> (j + 1)] = held[static_cast<std::size_t> (j)];
                --j;
            }

            held[static_cast<std::size_t> (j + 1)] = key;
        }
    }

    void releaseSounding (juce::MidiBuffer& outgoing, int time) noexcept
    {
        for (int i = 0; i < soundingCount; ++i)
            outgoing.addEvent (juce::MidiMessage::noteOff (1, sounding[static_cast<std::size_t> (i)]),
                               std::max (0, time));

        soundingCount = 0;
    }

    void triggerNote (juce::MidiBuffer& outgoing, int time, int note, float velocity) noexcept
    {
        if (soundingCount >= maxHeld)
            return;

        outgoing.addEvent (juce::MidiMessage::noteOn (1, note, velocity), std::max (0, time));
        sounding[static_cast<std::size_t> (soundingCount++)] = note;
    }

    void advanceStep (juce::MidiBuffer& outgoing, int time, int stepSamples) noexcept
    {
        releaseSounding (outgoing, time);

        if (heldCount <= 0)
        {
            stepIndex = (stepIndex + 1) % numSteps;
            return;
        }

        const auto mask = static_cast<std::uint16_t> (1u << static_cast<unsigned> (stepIndex));
        const bool stepOn = (settings.pattern & mask) != 0;
        stepIndex = (stepIndex + 1) % numSteps;

        if (! stepOn)
            return;

        const auto octaves = std::clamp (settings.octaves, 1, 4);
        const auto gate = std::clamp (settings.gate, 0.05, 1.0);
        gateSamplesLeft = std::max (1, static_cast<int> (static_cast<double> (stepSamples) * gate));

        if (settings.mode == ArpMode::chord)
        {
            for (int i = 0; i < heldCount; ++i)
            {
                const auto octave = (walk + i) % octaves;
                const auto note = std::clamp (held[static_cast<std::size_t> (i)].note + 12 * octave, 0, 127);
                triggerNote (outgoing, time, note, held[static_cast<std::size_t> (i)].velocity);
            }

            ++walk;
            return;
        }

        const auto index = nextIndex();
        const auto octave = walk % octaves;
        const auto source = held[static_cast<std::size_t> (index)];
        const auto note = std::clamp (source.note + 12 * octave, 0, 127);
        triggerNote (outgoing, time, note, source.velocity);

        if (index == heldCount - 1 || (settings.mode == ArpMode::down && index == 0))
            ++walk;
    }

    [[nodiscard]] int nextIndex() noexcept
    {
        const auto last = std::max (0, heldCount - 1);

        switch (settings.mode)
        {
            case ArpMode::down:
                walk = (walk <= 0) ? last : walk - 1;
                return std::clamp (walk, 0, last);

            case ArpMode::upDown:
                if (heldCount <= 1)
                    return 0;
                walk += walkDirection;
                if (walk >= last)
                {
                    walk = last;
                    walkDirection = -1;
                }
                else if (walk <= 0)
                {
                    walk = 0;
                    walkDirection = 1;
                }
                return walk;

            case ArpMode::asPlayed:
                walk = (walk + 1) % heldCount;
                return walk;

            case ArpMode::random:
                rng = rng * 1664525u + 1013904223u;
                return static_cast<int> (rng % static_cast<std::uint32_t> (heldCount));

            case ArpMode::up:
            case ArpMode::chord:
            case ArpMode::numModes:
            default:
                walk = (walk + 1) % heldCount;
                return walk;
        }
    }

    ArpSettings settings;
    std::array<HeldNote, maxHeld> held {};
    std::array<int, maxHeld> sounding {};
    int heldCount { 0 };
    int soundingCount { 0 };
    int stepIndex { 0 };
    int walk { 0 };
    int walkDirection { 1 };
    int samplesUntilStep { 0 };
    int gateSamplesLeft { 0 };
    bool evenStep { true };
    double sampleRate { 44100.0 };
    std::uint32_t noteOrder { 0 };
    std::uint32_t rng { 0xA57E11u };
};

} // namespace aethr::engine
