#include "Engine/VoiceEngine.h"

#include <algorithm>
#include <limits>

namespace aethr::engine
{

namespace
{
    /** Odd 32-bit multipliers, so seed derivation is a bijection and cannot collapse. */
    constexpr std::uint32_t seedMultiplier = 2654435761u;
    constexpr std::uint32_t noteMultiplier = 40503u;
    constexpr std::uint32_t counterMultiplier = 2246822519u;
}

VoiceEngine::VoiceEngine()
    : voices (std::make_unique<VoicePool>())
{
}

VoiceEngine::~VoiceEngine() = default;
VoiceEngine::VoiceEngine (VoiceEngine&&) noexcept = default;
VoiceEngine& VoiceEngine::operator= (VoiceEngine&&) noexcept = default;

void VoiceEngine::prepare (double sampleRate)
{
    for (auto& voice : *voices)
        voice.prepare (sampleRate);

    startOrderCounter = 0;
    seedCounter = 0;
}

void VoiceEngine::reset() noexcept
{
    for (auto& voice : *voices)
        voice.reset();

    pitchBendSemitones = 0.0;
}

void VoiceEngine::setSettings (const Settings& newSettings) noexcept
{
    settings = newSettings;

    // Only sounding voices need updating; an idle voice will pick the settings up when
    // it is next started, and touching it here would waste transcendental maths per block.
    for (auto& voice : *voices)
        if (voice.isActive())
            voice.applySettings (settings);
}

int VoiceEngine::getVoiceLimit() const noexcept
{
    return std::clamp (settings.numVoices, 1, maximumVoices);
}

std::uint32_t VoiceEngine::seedForNote (int midiNoteNumber) noexcept
{
    const auto base = static_cast<std::uint32_t> (settings.seed) * seedMultiplier;

    // Locked: the seed depends only on the key, so the same note always excites the
    // string identically and a preset reproduces exactly. Unlocked: a running counter,
    // so repeated notes vary the way a real instrument does.
    if (settings.seedLocked)
        return base ^ (static_cast<std::uint32_t> (midiNoteNumber) * noteMultiplier + 1u);

    return base ^ (++seedCounter * counterMultiplier);
}

Voice* VoiceEngine::findVoiceToUse (int midiNoteNumber) noexcept
{
    const auto limit = getVoiceLimit();

    // Retriggering a key that is already sounding re-excites that string rather than
    // starting a second one, which is both what a string does and what stops a fast
    // trill from eating the whole pool.
    for (int i = 0; i < limit; ++i)
    {
        auto& voice = (*voices)[static_cast<std::size_t> (i)];

        if (voice.isActive() && voice.isHeld() && voice.getNoteNumber() == midiNoteNumber)
            return &voice;
    }

    for (int i = 0; i < limit; ++i)
    {
        auto& voice = (*voices)[static_cast<std::size_t> (i)];

        if (! voice.isActive())
            return &voice;
    }

    // Steal: prefer a released voice, and among candidates take the oldest.
    Voice* oldestReleased = nullptr;
    Voice* oldest = nullptr;
    auto oldestReleasedOrder = std::numeric_limits<std::uint64_t>::max();
    auto oldestOrder = std::numeric_limits<std::uint64_t>::max();

    for (int i = 0; i < limit; ++i)
    {
        auto& voice = (*voices)[static_cast<std::size_t> (i)];
        const auto order = voice.getStartOrder();

        if (! voice.isHeld() && order < oldestReleasedOrder)
        {
            oldestReleasedOrder = order;
            oldestReleased = &voice;
        }

        if (order < oldestOrder)
        {
            oldestOrder = order;
            oldest = &voice;
        }
    }

    return oldestReleased != nullptr ? oldestReleased : oldest;
}

//==============================================================================
void VoiceEngine::noteOn (int midiNoteNumber, double velocity) noexcept
{
    auto* voice = findVoiceToUse (midiNoteNumber);

    if (voice == nullptr)
        return;

    voice->setStartOrder (++startOrderCounter);
    voice->setPitchOffsetSemitones (pitchBendSemitones);
    voice->noteOn (midiNoteNumber, velocity, seedForNote (midiNoteNumber), settings);
}

void VoiceEngine::noteOff (int midiNoteNumber) noexcept
{
    for (auto& voice : *voices)
    {
        if (! (voice.isActive() && voice.getNoteNumber() == midiNoteNumber && voice.isKeyDown()))
            continue;

        voice.markKeyUp();

        if (! sustainPedalDown)
            voice.noteOff();
    }
}

void VoiceEngine::setSustainPedal (bool down) noexcept
{
    sustainPedalDown = down;

    if (down)
        return;

    for (auto& voice : *voices)
        if (voice.isActive() && ! voice.isKeyDown())
            voice.noteOff();
}

void VoiceEngine::allNotesOff (bool allowTailOff) noexcept
{
    for (auto& voice : *voices)
    {
        if (! voice.isActive())
            continue;

        if (allowTailOff)
            voice.noteOff();
        else
            voice.kill();
    }
}

void VoiceEngine::setPitchBendSemitones (double semitones) noexcept
{
    pitchBendSemitones = semitones;

    for (auto& voice : *voices)
        if (voice.isActive())
            voice.setPitchOffsetSemitones (semitones);
}

//==============================================================================
void VoiceEngine::renderAdding (double* left, double* right, int numSamples) noexcept
{
    for (auto& voice : *voices)
        if (voice.isActive())
            voice.renderAdding (left, right, numSamples);
}

int VoiceEngine::getActiveVoiceCount() const noexcept
{
    auto count = 0;

    for (const auto& voice : *voices)
        if (voice.isActive())
            ++count;

    return count;
}

const Voice* VoiceEngine::getVoicePlayingNote (int midiNoteNumber) const noexcept
{
    for (const auto& voice : *voices)
        if (voice.isActive() && voice.getNoteNumber() == midiNoteNumber)
            return &voice;

    return nullptr;
}

} // namespace aethr::engine
