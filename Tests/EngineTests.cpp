#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

#include "Core/AudioMath.h"
#include "Engine/EngineType.h"
#include "EngineTestSupport.h"
#include "PitchEstimator.h"

/**
    Whole-engine tests, driven through the processor exactly as a host drives it.

    The first of these is the one that matters most as an acceptance test: the plugin
    must make sound. Phase 1 rendered silence by design, and a regression back to
    silence would otherwise pass every other test in the suite.
*/

using namespace aethr;

namespace
{
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;

    [[nodiscard]] double rmsOf (const std::vector<double>& samples, std::size_t from, std::size_t to)
    {
        if (from >= samples.size())
            return 0.0;

        const auto last = std::min (to, samples.size());
        auto sum = 0.0;

        for (auto n = from; n < last; ++n)
            sum += samples[n] * samples[n];

        const auto count = last > from ? static_cast<double> (last - from) : 1.0;

        return std::sqrt (sum / count);
    }
}

//==============================================================================
TEST_CASE ("A MIDI note produces finite, non-silent audio at the right pitch", "[engine][acceptance]")
{
    AethrProcessor processor;
    testing::applyTuningTestSettings (processor);

    constexpr int midiNote = 69;   // A4, 440 Hz
    const auto rendered = testing::renderNote (processor, midiNote, 1.0f, sampleRate, blockSize, 1.0);

    REQUIRE (rendered.isFinite());
    REQUIRE (rendered.peak() > 0.01);

    // Both channels must be carrying signal, not just the first.
    REQUIRE (rmsOf (rendered.left, 0, rendered.size()) > 1.0e-3);
    REQUIRE (rmsOf (rendered.right, 0, rendered.size()) > 1.0e-3);

    const auto window = rendered.leftWindow (static_cast<std::size_t> (0.1 * sampleRate),
                                             static_cast<std::size_t> (0.6 * sampleRate));
    const auto estimate = testing::estimateFundamental (window, sampleRate, 440.0);
    const auto centsError = math::ratioToCents (estimate.frequencyHz, 440.0);

    CAPTURE (estimate.frequencyHz, centsError, rendered.peak());

    REQUIRE (estimate.valid);
    REQUIRE (std::abs (centsError) < 1.0);
}

TEST_CASE ("Output stays silent until a note arrives", "[engine]")
{
    AethrProcessor processor;
    processor.prepareToPlay (sampleRate, blockSize);

    juce::AudioBuffer<float> buffer { 2, blockSize };
    juce::MidiBuffer midi;

    for (auto block = 0; block < 20; ++block)
    {
        buffer.clear();
        processor.processBlock (buffer, midi);

        REQUIRE (processor.getOutputPeakLevel() == 0.0f);
    }

    REQUIRE (processor.getActiveVoiceCount() == 0);
}

TEST_CASE ("Releasing a note lets it ring out and then frees the voice", "[engine]")
{
    AethrProcessor processor;
    testing::applyTuningTestSettings (processor);

    // A short release, so the test does not have to render for half a minute.
    testing::setParameter (processor, params::resonator::decayTime, 8.0f);
    testing::setParameter (processor, params::resonator::releaseTime, 0.05f);

    const auto rendered = testing::renderNote (processor, 60, 1.0f, sampleRate, blockSize, 2.0, 0.5);

    REQUIRE (rendered.isFinite());

    const auto whileHeld = rmsOf (rendered.left,
                                  static_cast<std::size_t> (0.2 * sampleRate),
                                  static_cast<std::size_t> (0.45 * sampleRate));

    const auto afterRelease = rmsOf (rendered.left,
                                     static_cast<std::size_t> (1.2 * sampleRate),
                                     rendered.size());

    CAPTURE (whileHeld, afterRelease);

    REQUIRE (whileHeld > 1.0e-3);
    REQUIRE (afterRelease < whileHeld * 0.01);
    REQUIRE (processor.getActiveVoiceCount() == 0);
}

TEST_CASE ("Held notes sum into a chord", "[engine][voices]")
{
    AethrProcessor processor;
    testing::applyTuningTestSettings (processor);
    processor.prepareToPlay (sampleRate, blockSize);

    juce::AudioBuffer<float> buffer { 2, blockSize };

    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, 1.0f), 0);
    midi.addEvent (juce::MidiMessage::noteOn (1, 64, 1.0f), 8);
    midi.addEvent (juce::MidiMessage::noteOn (1, 67, 1.0f), 16);

    buffer.clear();
    processor.processBlock (buffer, midi);

    REQUIRE (processor.getActiveVoiceCount() == 3);

    // Retriggering an already-sounding key re-excites that string rather than
    // consuming another voice.
    juce::MidiBuffer retrigger;
    retrigger.addEvent (juce::MidiMessage::noteOn (1, 64, 1.0f), 0);

    buffer.clear();
    processor.processBlock (buffer, retrigger);

    REQUIRE (processor.getActiveVoiceCount() == 3);

    // All-sound-off must stop everything immediately.
    juce::MidiBuffer panic;
    panic.addEvent (juce::MidiMessage::allSoundOff (1), 0);

    buffer.clear();
    processor.processBlock (buffer, panic);

    REQUIRE (processor.getActiveVoiceCount() == 0);
}

TEST_CASE ("Voice allocation cannot be overrun", "[engine][voices]")
{
    AethrProcessor processor;
    testing::applyTuningTestSettings (processor);
    processor.prepareToPlay (sampleRate, blockSize);

    juce::AudioBuffer<float> buffer { 2, blockSize };
    juce::MidiBuffer midi;

    // Far more notes than the pool has slots.
    for (auto note = 24; note < 100; ++note)
        midi.addEvent (juce::MidiMessage::noteOn (1, note, 0.9f), note - 24);

    buffer.clear();
    processor.processBlock (buffer, midi);

    REQUIRE (processor.getActiveVoiceCount() <= engine::VoiceEngine::maximumVoices);

    for (auto channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (auto i = 0; i < buffer.getNumSamples(); ++i)
            REQUIRE (std::isfinite (buffer.getSample (channel, i)));
}

TEST_CASE ("Pitch bend moves the sounding pitch", "[engine]")
{
    AethrProcessor processor;
    testing::applyTuningTestSettings (processor);
    processor.prepareToPlay (sampleRate, blockSize);

    juce::AudioBuffer<double> buffer { 2, blockSize };
    std::vector<double> captured;

    juce::MidiBuffer opening;
    opening.addEvent (juce::MidiMessage::noteOn (1, 69, 1.0f), 0);
    opening.addEvent (juce::MidiMessage::pitchWheel (1, 16383), 1);   // full up: +2 semitones

    const auto totalBlocks = static_cast<int> (0.8 * sampleRate) / blockSize;

    for (auto block = 0; block < totalBlocks; ++block)
    {
        auto midi = block == 0 ? opening : juce::MidiBuffer {};

        buffer.clear();
        processor.processBlock (buffer, midi);

        const auto* samples = buffer.getReadPointer (0);

        for (auto i = 0; i < blockSize; ++i)
            captured.push_back (samples[i]);
    }

    // Analysis starts after the bend has finished gliding.
    const auto expectedHz = math::midiNoteToHertz (69.0 + 2.0);
    const auto start = static_cast<std::size_t> (0.2 * sampleRate);
    const std::vector<double> window { captured.begin() + static_cast<std::ptrdiff_t> (start), captured.end() };

    const auto estimate = testing::estimateFundamental (window, sampleRate, expectedHz);
    const auto centsError = math::ratioToCents (estimate.frequencyHz, expectedHz);

    CAPTURE (expectedHz, estimate.frequencyHz, centsError);

    REQUIRE (estimate.valid);
    REQUIRE (std::abs (centsError) < 5.0);
}

TEST_CASE ("Every excitation type is audible through the full engine", "[engine]")
{
    for (auto typeIndex = 0; typeIndex < params::numExcitationTypes; ++typeIndex)
    {
        AethrProcessor processor;
        testing::applyTuningTestSettings (processor);
        testing::setParameter (processor, params::exciter::type, static_cast<float> (typeIndex));
        testing::setParameter (processor, params::resonator::decayTime, 2.0f);

        const auto rendered = testing::renderNote (processor, 57, 1.0f, sampleRate, blockSize, 0.6);

        CAPTURE (typeIndex);

        REQUIRE (rendered.isFinite());
        REQUIRE (rendered.peak() > 1.0e-3);
        REQUIRE (rendered.peak() < 8.0);
    }
}

TEST_CASE ("Every loop filter mode is stable and audible", "[engine]")
{
    for (auto filterIndex = 0; filterIndex < params::numLoopFilterModes; ++filterIndex)
    {
        for (const auto damping : { 0.0f, 60.0f, 100.0f })
        {
            AethrProcessor processor;
            testing::applyTuningTestSettings (processor, damping);
            testing::setParameter (processor, params::resonator::loopFilterMode, static_cast<float> (filterIndex));
            testing::setParameter (processor, params::resonator::decayTime, 3.0f);

            const auto rendered = testing::renderNote (processor, 60, 1.0f, sampleRate, blockSize, 0.8);

            CAPTURE (filterIndex, damping, rendered.peak());

            REQUIRE (rendered.isFinite());
            REQUIRE (rendered.peak() > 1.0e-4);
            REQUIRE (rendered.peak() < 8.0);
        }
    }
}

//==============================================================================
TEST_CASE ("No parameter combination can poison the buffer", "[engine][fuzz]")
{
    // Randomised over the entire normalised parameter space, with a fixed seed so a
    // failure is reproducible. Chords are played throughout, and the settings change
    // every block, which also exercises the smoothing paths.
    std::mt19937 generator { 0xC0FFEEu };
    std::uniform_real_distribution<float> normalised { 0.0f, 1.0f };

    for (const auto rate : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        AethrProcessor processor;
        processor.prepareToPlay (rate, blockSize);

        juce::AudioBuffer<float> buffer { 2, blockSize };
        auto highestPeak = 0.0f;

        for (auto block = 0; block < 60; ++block)
        {
            for (auto* parameter : processor.getParameters())
                parameter->setValueNotifyingHost (normalised (generator));

            juce::MidiBuffer midi;

            if (block % 5 == 0)
                for (const auto note : { 33, 45, 57, 69, 81 })
                    midi.addEvent (juce::MidiMessage::noteOn (1, note, 1.0f), 0);

            if (block % 7 == 3)
                midi.addEvent (juce::MidiMessage::noteOff (1, 45), 0);

            buffer.clear();
            processor.processBlock (buffer, midi);

            for (auto channel = 0; channel < buffer.getNumChannels(); ++channel)
            {
                const auto* samples = buffer.getReadPointer (channel);

                for (auto i = 0; i < blockSize; ++i)
                {
                    REQUIRE (std::isfinite (samples[i]));
                    highestPeak = std::max (highestPeak, std::abs (samples[i]));
                }
            }
        }

        // The output stage can legitimately be driven loud - there is a +12 dB gain
        // parameter - but it must stay bounded rather than diverging.
        CAPTURE (rate, highestPeak);
        REQUIRE (highestPeak < 64.0f);
    }
}

TEST_CASE ("Sustained excitation reaches a steady level rather than growing", "[engine][stability]")
{
    // A continuous drive into a high-gain loop is the configuration that would run
    // away if the drive were not normalised against the loop gain.
    AethrProcessor processor;
    testing::applyTuningTestSettings (processor);
    testing::setParameter (processor, params::exciter::type, 6.0f);   // bow
    testing::setParameter (processor, params::resonator::decayTime, 30.0f);
    testing::setParameter (processor, params::resonator::damping, 0.0f);
    testing::setParameter (processor, params::exciter::level, 100.0f);

    const auto rendered = testing::renderNote (processor, 45, 1.0f, sampleRate, blockSize, 6.0);

    REQUIRE (rendered.isFinite());

    const auto early = rmsOf (rendered.left,
                              static_cast<std::size_t> (2.0 * sampleRate),
                              static_cast<std::size_t> (3.0 * sampleRate));

    const auto late = rmsOf (rendered.left,
                             static_cast<std::size_t> (5.0 * sampleRate),
                             static_cast<std::size_t> (6.0 * sampleRate));

    CAPTURE (early, late, rendered.peak());

    REQUIRE (early > 1.0e-4);
    REQUIRE (rendered.peak() < 4.0);

    // Settled: no more than a couple of dB of drift over the last three seconds.
    REQUIRE (late < early * 2.0);
}

TEST_CASE ("Sample rate and block size changes do not break the engine", "[engine]")
{
    AethrProcessor processor;
    testing::applyTuningTestSettings (processor);

    for (const auto rate : { 44100.0, 96000.0, 48000.0 })
    {
        for (const auto size : { 16, 64, 512, 1024, 2048 })
        {
            processor.setRateAndBufferSizeDetails (rate, size);
            processor.prepareToPlay (rate, size);

            juce::AudioBuffer<float> buffer { 2, size };
            auto peak = 0.0f;

            for (auto block = 0; block < 12; ++block)
            {
                juce::MidiBuffer midi;

                if (block == 0)
                    midi.addEvent (juce::MidiMessage::noteOn (1, 72, 1.0f), 0);

                buffer.clear();
                processor.processBlock (buffer, midi);

                peak = std::max (peak, buffer.getMagnitude (0, size));
            }

            CAPTURE (rate, size, peak);

            REQUIRE (peak > 1.0e-4f);
            REQUIRE (std::isfinite (peak));
        }
    }
}

TEST_CASE ("Engine selector defaults to STRING and every engine sounds", "[engine][architecture]")
{
    AethrProcessor processor;
    REQUIRE (processor.getSelectedEngineType() == engine::EngineType::string);
    REQUIRE (engine::isEngineImplemented (engine::EngineType::string));
    REQUIRE (engine::isEngineImplemented (engine::EngineType::bell));
    REQUIRE (engine::resolvedEngineType (engine::EngineType::bell) == engine::EngineType::bell);

    auto* parameter = processor.getValueTreeState().getParameter (params::engine::type);
    REQUIRE (parameter != nullptr);

    for (int index = 0; index < engine::numEngineTypes; ++index)
    {
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (static_cast<float> (index)));
        REQUIRE (processor.getSelectedEngineType() == engine::engineTypeFromIndex (index));

        testing::applyTuningTestSettings (processor);
        const auto rendered = testing::renderNote (processor, 69, 1.0f, sampleRate, blockSize, 1.0);
        CAPTURE (index, engine::engineCatalogue[index].name, rendered.peak());
        REQUIRE (rendered.isFinite());
        REQUIRE (rendered.peak() > 0.005);
    }
}

TEST_CASE ("Arpeggiator emits finite audio from a held chord", "[engine][arp]")
{
    AethrProcessor processor;
    testing::applyTuningTestSettings (processor);
    REQUIRE (testing::setParameter (processor, params::arp::enable, 1.0f));
    REQUIRE (testing::setParameter (processor, params::arp::mode, 0.0f));
    REQUIRE (testing::setParameter (processor, params::arp::octaves, 2.0f));

    processor.prepareToPlay (sampleRate, blockSize);
    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, 1.0f), 0);
    midi.addEvent (juce::MidiMessage::noteOn (1, 64, 1.0f), 0);
    midi.addEvent (juce::MidiMessage::noteOn (1, 67, 1.0f), 0);

    auto peak = 0.0f;

    for (int block = 0; block < 40; ++block)
    {
        buffer.clear();
        juce::MidiBuffer incoming;

        if (block == 0)
            incoming = midi;

        processor.processBlock (buffer, incoming);
        peak = std::max (peak, buffer.getMagnitude (0, blockSize));
    }

    REQUIRE (std::isfinite (peak));
    REQUIRE (peak > 1.0e-4f);
    REQUIRE (processor.getArpStep() >= 0);
    REQUIRE (processor.getArpStep() < 16);
}

TEST_CASE ("Switching engines mid-session stays finite and responds to notes", "[engine][architecture][regression]")
{
    AethrProcessor processor;
    testing::applyTuningTestSettings (processor);
    auto* engineParam = processor.getValueTreeState().getParameter (params::engine::type);
    REQUIRE (engineParam != nullptr);

    processor.prepareToPlay (sampleRate, blockSize);
    juce::AudioBuffer<float> buffer (2, blockSize);

    for (int index = 0; index < engine::numEngineTypes; ++index)
    {
        engineParam->setValueNotifyingHost (engineParam->convertTo0to1 (static_cast<float> (index)));
        processor.reset();

        for (int burst = 0; burst < 3; ++burst)
        {
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, 48 + (burst * 7), 0.9f), 0);
            midi.addEvent (juce::MidiMessage::noteOff (1, 48 + (burst * 7)), blockSize / 2);

            auto peak = 0.0f;

            for (int block = 0; block < 8; ++block)
            {
                buffer.clear();
                juce::MidiBuffer incoming;

                if (block == 0)
                    incoming = midi;

                processor.processBlock (buffer, incoming);
                peak = std::max (peak, buffer.getMagnitude (0, blockSize));
                REQUIRE (std::isfinite (peak));
            }

            CAPTURE (index, engine::engineCatalogue[index].name, burst, peak);
            REQUIRE (peak > 1.0e-5f);
        }
    }
}

TEST_CASE ("Switching engines while a note is held stays finite without reset", "[engine][architecture][regression]")
{
    AethrProcessor processor;
    testing::applyTuningTestSettings (processor);
    auto* engineParam = processor.getValueTreeState().getParameter (params::engine::type);
    REQUIRE (engineParam != nullptr);

    processor.prepareToPlay (sampleRate, blockSize);
    juce::AudioBuffer<float> buffer (2, blockSize);

    {
        juce::MidiBuffer noteOn;
        noteOn.addEvent (juce::MidiMessage::noteOn (1, 60, 0.95f), 0);
        buffer.clear();
        processor.processBlock (buffer, noteOn);
    }

    auto peak = 0.0f;

    for (int index = 0; index < engine::numEngineTypes; ++index)
    {
        engineParam->setValueNotifyingHost (engineParam->convertTo0to1 (static_cast<float> (index)));

        for (int block = 0; block < 6; ++block)
        {
            juce::MidiBuffer empty;
            buffer.clear();
            processor.processBlock (buffer, empty);
            const auto blockPeak = buffer.getMagnitude (0, blockSize);
            REQUIRE (std::isfinite (blockPeak));
            peak = std::max (peak, blockPeak);

            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                for (int i = 0; i < buffer.getNumSamples(); ++i)
                    REQUIRE (std::isfinite (buffer.getSample (ch, i)));
        }
    }

    CAPTURE (peak);
    REQUIRE (peak > 1.0e-5f);
}

TEST_CASE ("Hard saturation modes stay finite at high drive", "[engine][stability][fx]")
{
    AethrProcessor processor;
    testing::applyTuningTestSettings (processor);
    auto& state = processor.getValueTreeState();

    testing::setParameter (processor, params::fx::satMode, 6.0f); // hard clip
    testing::setParameter (processor, params::fx::satDrive, 100.0f);
    testing::setParameter (processor, params::fx::satMix, 100.0f);
    testing::setParameter (processor, params::fx::filterMix, 0.0f);

    processor.prepareToPlay (sampleRate, blockSize);
    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 48, 1.0f), 0);

    for (int block = 0; block < 16; ++block)
    {
        buffer.clear();
        juce::MidiBuffer incoming;

        if (block == 0)
            incoming = midi;

        processor.processBlock (buffer, incoming);

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                REQUIRE (std::isfinite (buffer.getSample (ch, i)));
    }

    // Wavefold mode
    testing::setParameter (processor, params::fx::satMode, 4.0f);
    midi.clear();
    midi.addEvent (juce::MidiMessage::noteOn (1, 55, 1.0f), 0);

    for (int block = 0; block < 16; ++block)
    {
        buffer.clear();
        juce::MidiBuffer incoming;

        if (block == 0)
            incoming = midi;

        processor.processBlock (buffer, incoming);

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                REQUIRE (std::isfinite (buffer.getSample (ch, i)));
    }

    juce::ignoreUnused (state);
}

TEST_CASE ("Extreme resonator settings stay finite across engines", "[engine][stability][regression]")
{
    AethrProcessor processor;
    auto* engineParam = processor.getValueTreeState().getParameter (params::engine::type);
    REQUIRE (engineParam != nullptr);

    struct Extreme { const char* id; float value; };

    const Extreme extremes[]
    {
        { params::resonator::decayTime, 0.02f },
        { params::resonator::decayTime, 30.0f },
        { params::resonator::damping, 0.0f },
        { params::resonator::damping, 100.0f },
        { params::resonator::brightness, 0.0f },
        { params::resonator::brightness, 100.0f },
        { params::resonator::feedback, 0.0f },
        { params::resonator::feedback, 100.0f },
        { params::resonator::stiffness, 100.0f },
        { params::exciter::level, 0.0f },
        { params::exciter::level, 100.0f },
    };

    // Exercise STRING (KS) and BELL (modal) as representatives of both cores.
    for (const auto engineIndex : { 0, 3 })
    {
        engineParam->setValueNotifyingHost (engineParam->convertTo0to1 (static_cast<float> (engineIndex)));
        testing::applyTuningTestSettings (processor);

        for (const auto& extreme : extremes)
        {
            REQUIRE (testing::setParameter (processor, extreme.id, extreme.value));
            const auto rendered = testing::renderNote (processor, 36, 1.0f, sampleRate, blockSize, 0.35);
            CAPTURE (engineIndex, extreme.id, extreme.value, rendered.peak());
            REQUIRE (rendered.isFinite());
        }
    }
}
