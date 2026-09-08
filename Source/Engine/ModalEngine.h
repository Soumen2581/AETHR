#pragma once

#include <algorithm>
#include <cmath>

#include "DSP/BodyResonator.h"
#include "DSP/Exciter.h"
#include "Engine/EngineSettings.h"
#include "Engine/EngineType.h"

#include "Core/AudioMath.h"

namespace aethr::engine
{

/**
    Primary modal / inharmonic engine.

    Used by Bell, Plate, Membrane, Modal, Cavity and Spectral. The body bank is
    the resonator, not a colouration sitting after a string.
*/
class ModalEngine
{
public:
    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
        exciter.prepare (sampleRate);
        bodyLeft.prepare (sampleRate);
        bodyRight.prepare (sampleRate);
        silenceHoldSamples = std::max (1, static_cast<int> (0.5 * sampleRate));
        reset();
    }

    void reset() noexcept
    {
        exciter.reset();
        bodyLeft.reset();
        bodyRight.reset();
        quietSamples = 0;
        sleeping = false;
    }

    void kill() noexcept
    {
        reset();
    }

    void applySettings (const Settings& settings,
                        int midiNote,
                        double pitchOffset,
                        bool noteHeld) noexcept
    {
        currentSettings = settings;
        noteNumber = midiNote;
        pitchOffsetSemitones = pitchOffset;
        held = noteHeld;

        auto exciterSettings = settings.exciter;
        shapeExciter (exciterSettings, settings.engineType, settings.controlA);
        exciter.setSettings (exciterSettings);

        dsp::BodyResonator::Settings body = settings.body;
        body.mix = 1.0;
        body.preset = presetFor (settings.engineType);
        body.activeModes = modeCountFor (settings.engineType, settings.body.activeModes, settings.controlA);
        body.decay = std::clamp ((noteHeld ? settings.body.decay : settings.body.decay * 0.45)
                                     + settings.controlD * 0.35,
                                 0.05, 1.0);
        body.brightness = std::clamp (settings.body.brightness + settings.controlD * 0.25, 0.0, 1.0);
        body.inharmonicity = std::clamp (settings.controlB + settings.stiffness, 0.0, 1.0);
        body.fundamentalHz = math::midiNoteToHertz (
            static_cast<double> (noteNumber) + settings.tuningOffsetSemitones + pitchOffsetSemitones);

        if (settings.engineType == EngineType::cavity)
            body.fundamentalHz *= 0.5 + 0.7 * settings.controlA;

        if (settings.engineType == EngineType::plate)
            body.fundamentalHz *= 0.7 + 0.5 * settings.controlA;

        if (settings.engineType == EngineType::membrane)
            body.fundamentalHz *= 0.55 + settings.controlB * 0.8;

        bodyLeft.setSettings (body);
        body.brightness = std::clamp (body.brightness + 0.08 * settings.controlC, 0.0, 1.0);
        bodyRight.setSettings (body);
    }

    void noteOn (int midiNote,
                 double velocity,
                 std::uint32_t seed,
                 const Settings& settings,
                 double pitchOffset) noexcept
    {
        sleeping = false;
        quietSamples = 0;
        applySettings (settings, midiNote, pitchOffset, true);
        exciter.noteOn (velocity, seed);
    }

    void noteOff() noexcept
    {
        exciter.noteOff();
        applySettings (currentSettings, noteNumber, pitchOffsetSemitones, false);
    }

    void setPitchOffsetSemitones (double semitones) noexcept
    {
        pitchOffsetSemitones = semitones;
        applySettings (currentSettings, noteNumber, semitones, held);
    }

    void renderAdding (double* left, double* right, int numSamples) noexcept
    {
        auto peak = 0.0;

        for (int i = 0; i < numSamples; ++i)
        {
            const auto excitation = exciter.processSample();
            const auto l = bodyLeft.process (excitation.left);
            const auto r = bodyRight.process (excitation.right);
            left[i] += l;
            right[i] += r;
            peak = std::max (peak, std::max (std::abs (l), std::abs (r)));
        }

        updateSleep (peak, numSamples);
    }

    void renderFrom (const double* inLeft, const double* inRight,
                     double* outLeft, double* outRight, int numSamples, double mix) noexcept
    {
        const auto wet = std::clamp (mix, 0.0, 1.0);
        auto peak = 0.0;

        for (int i = 0; i < numSamples; ++i)
        {
            const auto l = inLeft[i] * (1.0 - wet) + bodyLeft.process (inLeft[i]) * wet;
            const auto r = inRight[i] * (1.0 - wet) + bodyRight.process (inRight[i]) * wet;
            outLeft[i] += l;
            outRight[i] += r;
            peak = std::max (peak, std::max (std::abs (l), std::abs (r)));
        }

        updateSleep (peak, numSamples);
    }

    [[nodiscard]] bool wantsToSleep() const noexcept { return sleeping; }

private:
    static constexpr double silenceThreshold = 1.0e-5;

    static dsp::BodyPreset presetFor (EngineType type) noexcept
    {
        switch (type)
        {
            case EngineType::bell:      return dsp::BodyPreset::bell;
            case EngineType::plate:     return dsp::BodyPreset::metal;
            case EngineType::membrane:  return dsp::BodyPreset::drum;
            case EngineType::cavity:    return dsp::BodyPreset::hollow;
            case EngineType::spectral:  return dsp::BodyPreset::crystalline;
            case EngineType::modal:
            case EngineType::string:
            case EngineType::pluck:
            case EngineType::bowed:
            case EngineType::tube:
            case EngineType::waveguide:
            case EngineType::granular:
            case EngineType::hybrid:
            case EngineType::numTypes:
                return dsp::BodyPreset::wooden;
        }

        return dsp::BodyPreset::wooden;
    }

    static int modeCountFor (EngineType type, int bodyModes, double controlA) noexcept
    {
        auto count = bodyModes > 0 ? bodyModes : 8;

        switch (type)
        {
            case EngineType::bell:      count = std::max (count, 8); break;
            case EngineType::plate:     count = std::max (count, 10); break;
            case EngineType::membrane:  count = std::max (count, 8); break;
            case EngineType::cavity:    count = std::clamp (2 + static_cast<int> (controlA * 6.0), 2, 8); break;
            case EngineType::spectral:  count = dsp::BodyResonator::maximumModes; break;
            case EngineType::modal:     count = std::clamp (4 + static_cast<int> (controlA * 12.0), 4, 16); break;
            case EngineType::string:
            case EngineType::pluck:
            case EngineType::bowed:
            case EngineType::tube:
            case EngineType::waveguide:
            case EngineType::granular:
            case EngineType::hybrid:
            case EngineType::numTypes:
                break;
        }

        return std::clamp (count, 1, dsp::BodyResonator::maximumModes);
    }

    static void shapeExciter (dsp::Exciter::Settings& exciterSettings, EngineType type, double strike) noexcept
    {
        if (type == EngineType::bell || type == EngineType::plate || type == EngineType::membrane)
        {
            if (exciterSettings.type == dsp::ExcitationType::pluck)
                exciterSettings.type = dsp::ExcitationType::mallet;

            exciterSettings.burstMilliseconds = 0.8 + strike * 28.0;
            exciterSettings.brightness = std::clamp (exciterSettings.brightness * 0.5 + strike * 0.6, 0.0, 1.0);
        }
    }

    void updateSleep (double peak, int numSamples) noexcept
    {
        if (! exciter.isActive() && peak < silenceThreshold)
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

    dsp::Exciter exciter;
    dsp::BodyResonator bodyLeft;
    dsp::BodyResonator bodyRight;
    Settings currentSettings;
    double sampleRate { 44100.0 };
    int noteNumber { 60 };
    double pitchOffsetSemitones { 0.0 };
    bool held { true };
    bool sleeping { false };
    int quietSamples { 0 };
    int silenceHoldSamples { 1 };
};

} // namespace aethr::engine
