#include "Engine/KarplusStringEngine.h"

#include <algorithm>
#include <cmath>

#include "Core/AudioMath.h"
#include "Core/RealtimeGuards.h"

namespace aethr::engine
{

void KarplusStringEngine::prepare (double newSampleRate)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    exciter.prepare (sampleRate);
    leftResonator.prepare (sampleRate);
    rightResonator.prepare (sampleRate);
    layerBLeft.prepare (sampleRate);
    layerBRight.prepare (sampleRate);
    bodyLeft.prepare (sampleRate);
    bodyRight.prepare (sampleRate);

    silenceHoldSamples = std::max (1, static_cast<int> (silenceHoldSeconds * sampleRate));

    reset();
}

void KarplusStringEngine::reset() noexcept
{
    exciter.reset();
    leftResonator.reset();
    rightResonator.reset();
    layerBLeft.reset();
    layerBRight.reset();
    bodyLeft.reset();
    bodyRight.reset();

    quietSamples = 0;
    sleeping = false;
    pitchOffsetSemitones = 0.0;
    lastOutL = 0.0;
    lastOutR = 0.0;
    grainPhase = 0.0;
    posL.fill (0.0);
    posR.fill (0.0);
    posWrite = 0;
}

void KarplusStringEngine::kill() noexcept
{
    exciter.kill();
    leftResonator.reset();
    rightResonator.reset();
    layerBLeft.reset();
    layerBRight.reset();
    bodyLeft.reset();
    bodyRight.reset();

    quietSamples = 0;
    sleeping = false;
}

void KarplusStringEngine::applyCharacter() noexcept
{
    auto& s = currentSettings;

    switch (s.engineType)
    {
        case EngineType::pluck:
            s.exciter.type = dsp::ExcitationType::pluck;
            s.stiffness = std::clamp (s.stiffness + s.controlC * 0.45, 0.0, 1.0);
            break;
        case EngineType::bowed:
            s.exciter.type = dsp::ExcitationType::bow;
            s.loopFeedback = std::max (s.loopFeedback, 0.88);
            break;
        case EngineType::tube:
            s.exciter.type = dsp::ExcitationType::blow;
            s.damping = std::clamp (s.damping + (1.0 - s.controlC) * 0.28, 0.0, 1.0);
            s.exciter.level = std::clamp (s.exciter.level + s.controlB * 0.25, 0.0, 1.5);
            break;
        case EngineType::waveguide:
            s.stiffness = std::clamp (s.stiffness + s.controlB * 0.7, 0.0, 1.0);
            s.layerBEnabled = true;
            s.layerBLevel = std::max (s.layerBLevel, 0.18 + s.controlA * 0.55);
            s.layerBDetuneCents = 1.5 + s.controlC * 24.0;
            s.loopFeedback = std::max (s.loopFeedback, 0.7 + s.controlD * 0.28);
            break;
        case EngineType::granular:
            s.exciter.burstMilliseconds = 0.8 + s.controlA * 48.0;
            s.exciter.randomAmount = std::max (s.exciter.randomAmount, 0.2 + s.controlD * 0.6);
            break;
        case EngineType::hybrid:
            s.body.mix = std::max (s.body.mix, 0.25 + s.controlA * 0.6);
            s.layerBEnabled = true;
            s.layerBLevel = std::max (s.layerBLevel, 0.12 + s.controlC * 0.4);
            break;
        case EngineType::bell:
        case EngineType::plate:
        case EngineType::membrane:
        case EngineType::cavity:
        case EngineType::modal:
        case EngineType::spectral:
        case EngineType::numTypes:
            break;
        case EngineType::string:
            s.stiffness = std::clamp (s.stiffness + s.controlA * 0.25, 0.0, 1.0);
            s.damping = std::clamp (s.damping + s.controlC * 0.2, 0.0, 1.0);
            break;
    }
}

dsp::Exciter::StereoSample KarplusStringEngine::colourExcitation (dsp::Exciter::StereoSample excitation) noexcept
{
    const auto type = currentSettings.engineType;
    const auto a = currentSettings.controlA;
    const auto b = currentSettings.controlB;
    const auto c = currentSettings.controlC;
    const auto period = sampleRate / std::max (20.0, leftResonator.getTunedFrequency());
    const auto delaySamples = std::clamp (1 + static_cast<int> (b * 0.45 * period), 1, 2047);
    const auto read = (posWrite - delaySamples + 2048) % 2048;

    if (type == EngineType::pluck || type == EngineType::string || type == EngineType::bowed)
    {
        const auto delayedL = posL[static_cast<std::size_t> (read)];
        const auto delayedR = posR[static_cast<std::size_t> (read)];
        posL[static_cast<std::size_t> (posWrite)] = excitation.left;
        posR[static_cast<std::size_t> (posWrite)] = excitation.right;
        const auto mix = (type == EngineType::string) ? b * 0.45 : b;
        excitation.left -= mix * delayedL;
        excitation.right -= mix * delayedR;
        posWrite = (posWrite + 1) & 2047;
    }

    if (type == EngineType::bowed && held)
    {
        const auto pressure = 0.25 + a * 2.2;
        const auto speed = 0.04 + b * 0.35;
        excitation.left += std::tanh (pressure * (speed - lastOutL)) * (0.12 + c * 0.55);
        excitation.right += std::tanh (pressure * (speed - lastOutR)) * (0.12 + c * 0.55);
    }

    if (type == EngineType::granular)
    {
        grainPhase += (0.4 + c * 22.0) / sampleRate;

        if (grainPhase >= 1.0)
        {
            grainPhase -= 1.0;
            exciter.noteOn (0.55 + a * 0.4, ++grainSeed);
        }
    }

    if (type == EngineType::tube)
        excitation.right = -excitation.right * (0.35 + currentSettings.controlC * 0.65);

    return excitation;
}

void KarplusStringEngine::updateFrequency() noexcept
{
    const auto note = static_cast<double> (noteNumber) + tuningOffsetSemitones + pitchOffsetSemitones;
    const auto frequency = math::midiNoteToHertz (note);

    leftResonator.setFrequency (frequency);
    rightResonator.setFrequency (frequency);

    const auto layerNote = note + currentSettings.layerBIntervalSemitones
                         + currentSettings.layerBDetuneCents / math::centsPerSemitone;
    const auto layerFrequency = math::midiNoteToHertz (layerNote);
    layerBLeft.setFrequency (layerFrequency);
    layerBRight.setFrequency (layerFrequency);
}

void KarplusStringEngine::applySettings (const Settings& settings,
                                         int midiNote,
                                         double pitchOffset,
                                         bool noteHeld) noexcept
{
    hostSettings = settings;
    currentSettings = settings;
    noteNumber = midiNote;
    pitchOffsetSemitones = pitchOffset;
    held = noteHeld;
    tuningOffsetSemitones = settings.tuningOffsetSemitones;
    applyCharacter();

    exciter.setSettings (currentSettings.exciter);

    const auto decay = held ? currentSettings.decayTimeSeconds
                            : std::min (currentSettings.releaseTimeSeconds, currentSettings.decayTimeSeconds);

    const auto configure = [&] (dsp::KarplusResonator& resonator)
    {
        resonator.setInterpolationMode (currentSettings.interpolation);
        resonator.setLoopFilterMode (currentSettings.loopFilterMode);
        resonator.setDecayTime (decay);
        resonator.setDamping (currentSettings.damping);
        resonator.setBrightness (currentSettings.brightness);
        resonator.setStiffness (currentSettings.stiffness);
        resonator.setLoopFeedback (currentSettings.loopFeedback);
    };

    configure (leftResonator);
    configure (rightResonator);

    if (currentSettings.layerBEnabled && currentSettings.layerBLevel > 1.0e-4)
    {
        configure (layerBLeft);
        configure (layerBRight);
    }

    updateFrequency();

    leftResonator.updateCoefficients();
    rightResonator.updateCoefficients();

    if (currentSettings.layerBEnabled && currentSettings.layerBLevel > 1.0e-4)
    {
        layerBLeft.updateCoefficients();
        layerBRight.updateCoefficients();
    }

    auto bodySettings = currentSettings.body;
    bodySettings.fundamentalHz = math::midiNoteToHertz (
        static_cast<double> (noteNumber) + tuningOffsetSemitones + pitchOffsetSemitones);
    bodyLeft.setSettings (bodySettings);
    bodyRight.setSettings (bodySettings);
}

void KarplusStringEngine::noteOn (int midiNoteNumber,
                                  double velocity,
                                  std::uint32_t seed,
                                  const Settings& settings,
                                  double pitchOffset) noexcept
{
    sleeping = false;
    quietSamples = 0;
    applySettings (settings, midiNoteNumber, pitchOffset, true);

    leftResonator.snapToTargets();
    rightResonator.snapToTargets();

    if (currentSettings.layerBEnabled && currentSettings.layerBLevel > 1.0e-4)
    {
        layerBLeft.snapToTargets();
        layerBRight.snapToTargets();
    }

    exciter.noteOn (velocity, seed);
}

void KarplusStringEngine::noteOff() noexcept
{
    exciter.noteOff();
    applySettings (hostSettings, noteNumber, pitchOffsetSemitones, false);
}

void KarplusStringEngine::setPitchOffsetSemitones (double semitones) noexcept
{
    if (std::abs (semitones - pitchOffsetSemitones) < 1.0e-9)
        return;

    pitchOffsetSemitones = semitones;
    updateFrequency();

    leftResonator.updateCoefficients();
    rightResonator.updateCoefficients();

    if (currentSettings.layerBEnabled && currentSettings.layerBLevel > 1.0e-4)
    {
        layerBLeft.updateCoefficients();
        layerBRight.updateCoefficients();
    }
}

void KarplusStringEngine::renderAdding (double* left, double* right, int numSamples) noexcept
{
    const auto drive = exciter.isDriving()
                         ? std::max (minimumSustainedDrive,
                                     std::sqrt (std::max (0.0, 1.0 - leftResonator.getLoopGain()
                                                                    * leftResonator.getLoopGain())))
                         : 1.0;

    auto blockPeak = 0.0;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto excitation = colourExcitation (exciter.processSample());

        const auto leftSampleA = leftResonator.processSample (excitation.left * drive);
        const auto rightSampleA = rightResonator.processSample (excitation.right * drive);

        auto leftSample = leftSampleA;
        auto rightSample = rightSampleA;

        if (currentSettings.layerBEnabled && currentSettings.layerBLevel > 1.0e-4)
        {
            const auto layerGain = currentSettings.layerBLevel;
            leftSample  += layerBLeft.processSample (excitation.left * drive) * layerGain;
            rightSample += layerBRight.processSample (excitation.right * drive) * layerGain;
        }

        leftSample = bodyLeft.process (leftSample);
        rightSample = bodyRight.process (rightSample);

        left[i] += leftSample;
        right[i] += rightSample;
        lastOutL = leftSample;
        lastOutR = rightSample;

        blockPeak = std::max (blockPeak, std::max (std::abs (leftSample), std::abs (rightSample)));
    }

    if (! exciter.isActive() && blockPeak < silenceThreshold)
    {
        quietSamples += numSamples;
        sleeping = quietSamples >= silenceHoldSamples;
    }
    else
    {
        quietSamples = 0;
        sleeping = false;
    }
}

} // namespace aethr::engine
