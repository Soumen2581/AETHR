#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

#include "Core/AudioMath.h"
#include "Core/RealtimeGuards.h"

namespace aethr::dsp
{

enum class FilterType
{
    lowpass = 0,
    highpass,
    bandpass,
    notch,
    comb,
    morph,
    numTypes
};

enum class SaturationMode
{
    clean = 0,
    soft,
    tube,
    tape,
    wavefold,
    asymmetric,
    hardClip,
    diode,
    numModes
};

class FxRack
{
public:
    struct Settings
    {
        FilterType filterType { FilterType::lowpass };
        double filterCutoffHz { 8000.0 };
        double filterResonance { 0.15 };
        double filterMix { 0.0 };

        SaturationMode satMode { SaturationMode::soft };
        double satDrive { 0.0 };
        double satTone { 0.5 };
        double satMix { 0.0 };

        double delayTimeL { 0.28 };
        double delayTimeR { 0.36 };
        double delayFeedback { 0.35 };
        double delayMix { 0.0 };
        double delayDamping { 0.25 };

        double chorusRate { 0.8 };
        double chorusDepth { 0.35 };
        double chorusMix { 0.0 };

        double phaserRate { 0.3 };
        double phaserDepth { 0.5 };
        double phaserFeedback { 0.25 };
        double phaserMix { 0.0 };

        double reverbSize { 0.55 };
        double reverbDecay { 0.45 };
        double reverbDamping { 0.4 };
        double reverbWidth { 0.8 };
        double reverbMix { 0.0 };

        double ceiling { 0.98 };
    };

    void prepare (double newSampleRate, int maximumBlockSize)
    {
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
        const auto delaySamples = static_cast<int> (sampleRate * 2.0) + 8;

        delayL.assign (static_cast<std::size_t> (delaySamples), 0.0);
        delayR.assign (static_cast<std::size_t> (delaySamples), 0.0);
        chorusL.assign (2048, 0.0);
        chorusR.assign (2048, 0.0);

        combL.assign (4096, 0.0);
        combR.assign (4096, 0.0);

        for (auto& line : reverbDelay)
            line.assign (8192, 0.0);

        // ~20 ms equal-power-ish crossfade when delay time jumps under automation.
        delayFadeStep = 1.0 / std::max (1.0, sampleRate * 0.02);
        reset();
        static_cast<void> (maximumBlockSize);
    }

    void reset() noexcept
    {
        std::fill (delayL.begin(), delayL.end(), 0.0);
        std::fill (delayR.begin(), delayR.end(), 0.0);
        std::fill (chorusL.begin(), chorusL.end(), 0.0);
        std::fill (chorusR.begin(), chorusR.end(), 0.0);
        std::fill (combL.begin(), combL.end(), 0.0);
        std::fill (combR.begin(), combR.end(), 0.0);

        for (auto& line : reverbDelay)
            std::fill (line.begin(), line.end(), 0.0);

        delayWrite = chorusWrite = combWriteL = combWriteR = 0;
        reverbWrite = 0;
        delayTargetSamplesL = delayTargetSamplesR = 0;
        delayPrevSamplesL = delayPrevSamplesR = 1;
        delayFadeL = delayFadeR = 0.0;
        svfL = svfR = {};
        phaserL.fill (0.0);
        phaserR.fill (0.0);
        lfoPhase = 0.0;
        toneLpL = toneLpR = 0.0;
        delayLpL = delayLpR = 0.0;
    }

    void setSettings (const Settings& newSettings) noexcept { settings = newSettings; }

    void process (double* left, double* right, int numSamples) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
        {
            auto l = left[i];
            auto r = right != nullptr ? right[i] : l;

            l = processFilter (l, svfL, false);
            r = processFilter (r, svfR, true);

            l = processSaturation (l, toneLpL);
            r = processSaturation (r, toneLpR);

            processDelay (l, r);
            processChorus (l, r);
            processPhaser (l, r);
            processReverb (l, r);

            l = std::clamp (l, -settings.ceiling, settings.ceiling);
            r = std::clamp (r, -settings.ceiling, settings.ceiling);
            l = guards::sanitiseState (l);
            r = guards::sanitiseState (r);

            left[i] = l;

            if (right != nullptr)
                right[i] = r;
        }
    }

private:
    struct SvfState { double ic1 { 0.0 }; double ic2 { 0.0 }; };

    [[nodiscard]] double processFilter (double input, SvfState& state, bool rightChannel) noexcept
    {
        if (settings.filterMix <= 1.0e-6)
            return input;

        const auto g = std::tan (math::pi * std::clamp (settings.filterCutoffHz, 20.0, sampleRate * 0.45) / sampleRate);
        const auto k = 2.0 - 1.9 * settings.filterResonance;
        const auto a1 = 1.0 / (1.0 + g * (g + k));
        const auto a2 = g * a1;
        const auto a3 = g * a2;

        const auto v3 = input - state.ic2;
        const auto v1 = a1 * state.ic1 + a2 * v3;
        const auto v2 = state.ic2 + a2 * state.ic1 + a3 * v3;
        state.ic1 = 2.0 * v1 - state.ic1;
        state.ic2 = 2.0 * v2 - state.ic2;

        double filtered = v2;

        switch (settings.filterType)
        {
            case FilterType::lowpass:  filtered = v2; break;
            case FilterType::highpass: filtered = input - k * v1 - v2; break;
            case FilterType::bandpass: filtered = v1; break;
            case FilterType::notch:    filtered = input - k * v1; break;
            case FilterType::comb:
            {
                auto& buffer = rightChannel ? combR : combL;
                auto& writeIndex = rightChannel ? combWriteR : combWriteL;

                if (buffer.empty())
                    break;

                const auto delay = static_cast<int> (sampleRate / std::max (40.0, settings.filterCutoffHz));
                const auto size = static_cast<int> (buffer.size());
                const auto read = (writeIndex - std::clamp (delay, 1, size - 1) + size) % size;
                filtered = input + 0.6 * buffer[static_cast<std::size_t> (read)];
                buffer[static_cast<std::size_t> (writeIndex)] = guards::sanitiseState (filtered);
                writeIndex = (writeIndex + 1) % size;
                break;
            }
            case FilterType::morph:
                filtered = v2 * (1.0 - settings.filterResonance) + v1 * settings.filterResonance;
                break;
            case FilterType::numTypes:
            default:
                break;
        }

        return input * (1.0 - settings.filterMix) + filtered * settings.filterMix;
    }

    [[nodiscard]] double processSaturation (double input, double& toneState) noexcept
    {
        if (settings.satMix <= 1.0e-6 || settings.satDrive <= 1.0e-6)
            return input;

        const auto gain = 1.0 + settings.satDrive * 8.0;
        auto x = input * gain;
        double y = x;

        switch (settings.satMode)
        {
            case SaturationMode::clean:      y = x; break;
            case SaturationMode::soft:       y = std::tanh (x); break;
            case SaturationMode::tube:       y = x < 0.0 ? x / (1.0 - 0.7 * x) : std::tanh (x); break;
            case SaturationMode::tape:       y = std::tanh (x * 0.7) + 0.15 * std::tanh (x * 3.0); break;
            case SaturationMode::wavefold:   y = std::sin (x); break;
            case SaturationMode::asymmetric: y = std::tanh (x) + 0.12 * x * x; break;
            case SaturationMode::hardClip:   y = std::clamp (x, -1.0, 1.0); break;
            case SaturationMode::diode:      y = x / (1.0 + std::abs (x)); break;
            case SaturationMode::numModes:
            default: break;
        }

        toneState += 0.15 * (y - toneState);
        const auto toned = y * settings.satTone + toneState * (1.0 - settings.satTone);
        return input * (1.0 - settings.satMix) + toned * settings.satMix;
    }

    void processDelay (double& left, double& right) noexcept
    {
        if (settings.delayMix <= 1.0e-6 || delayL.empty())
            return;

        const auto size = static_cast<int> (delayL.size());
        const auto targetL = std::clamp (static_cast<int> (settings.delayTimeL * sampleRate), 1, size - 2);
        const auto targetR = std::clamp (static_cast<int> (settings.delayTimeR * sampleRate), 1, size - 2);

        if (targetL != delayTargetSamplesL)
        {
            delayPrevSamplesL = delayTargetSamplesL > 0 ? delayTargetSamplesL : targetL;
            delayTargetSamplesL = targetL;
            delayFadeL = 1.0;
        }

        if (targetR != delayTargetSamplesR)
        {
            delayPrevSamplesR = delayTargetSamplesR > 0 ? delayTargetSamplesR : targetR;
            delayTargetSamplesR = targetR;
            delayFadeR = 1.0;
        }

        const auto readNewL = (delayWrite - delayTargetSamplesL + size) % size;
        const auto readNewR = (delayWrite - delayTargetSamplesR + size) % size;
        const auto readOldL = (delayWrite - delayPrevSamplesL + size) % size;
        const auto readOldR = (delayWrite - delayPrevSamplesR + size) % size;

        auto wetL = delayL[static_cast<std::size_t> (readNewL)];
        auto wetR = delayR[static_cast<std::size_t> (readNewR)];

        if (delayFadeL > 0.0)
        {
            const auto old = delayL[static_cast<std::size_t> (readOldL)];
            wetL = wetL * (1.0 - delayFadeL) + old * delayFadeL;
            delayFadeL = std::max (0.0, delayFadeL - delayFadeStep);
        }

        if (delayFadeR > 0.0)
        {
            const auto old = delayR[static_cast<std::size_t> (readOldR)];
            wetR = wetR * (1.0 - delayFadeR) + old * delayFadeR;
            delayFadeR = std::max (0.0, delayFadeR - delayFadeStep);
        }

        delayLpL += (0.15 + 0.8 * settings.delayDamping) * (wetL - delayLpL);
        delayLpR += (0.15 + 0.8 * settings.delayDamping) * (wetR - delayLpR);

        delayL[static_cast<std::size_t> (delayWrite)] = guards::sanitiseState (left + delayLpR * settings.delayFeedback);
        delayR[static_cast<std::size_t> (delayWrite)] = guards::sanitiseState (right + delayLpL * settings.delayFeedback);
        delayWrite = (delayWrite + 1) % size;

        left  = left  * (1.0 - settings.delayMix) + delayLpL * settings.delayMix;
        right = right * (1.0 - settings.delayMix) + delayLpR * settings.delayMix;
    }

    void processChorus (double& left, double& right) noexcept
    {
        if (settings.chorusMix <= 1.0e-6 || chorusL.empty())
            return;

        const auto size = static_cast<int> (chorusL.size());
        lfoPhase += settings.chorusRate / sampleRate;

        if (lfoPhase >= 1.0)
            lfoPhase -= 1.0;

        const auto mod = std::sin (math::twoPi * lfoPhase) * settings.chorusDepth * 12.0 + 18.0;
        const auto delay = std::clamp (static_cast<int> (mod), 1, size - 2);
        const auto read = (chorusWrite - delay + size) % size;

        chorusL[static_cast<std::size_t> (chorusWrite)] = left;
        chorusR[static_cast<std::size_t> (chorusWrite)] = right;
        chorusWrite = (chorusWrite + 1) % size;

        left  = left  * (1.0 - settings.chorusMix) + chorusL[static_cast<std::size_t> (read)] * settings.chorusMix;
        right = right * (1.0 - settings.chorusMix) + chorusR[static_cast<std::size_t> ((read + 7) % size)] * settings.chorusMix;
    }

    void processPhaser (double& left, double& right) noexcept
    {
        if (settings.phaserMix <= 1.0e-6)
            return;

        lfoPhase += settings.phaserRate / sampleRate;

        if (lfoPhase >= 1.0)
            lfoPhase -= 1.0;

        const auto coeff = 0.3 + 0.6 * (0.5 + 0.5 * std::sin (math::twoPi * lfoPhase)) * settings.phaserDepth;

        auto processAllpass = [coeff] (double input, std::array<double, 6>& state) noexcept
        {
            auto x = input;

            for (std::size_t s = 0; s < state.size(); ++s)
            {
                const auto out = coeff * x + state[s];
                state[s] = x - coeff * out;
                x = out;
            }

            return x;
        };

        const auto wetL = processAllpass (left + phaserL[0] * settings.phaserFeedback, phaserL);
        const auto wetR = processAllpass (right + phaserR[0] * settings.phaserFeedback, phaserR);

        left  = left  * (1.0 - settings.phaserMix) + wetL * settings.phaserMix;
        right = right * (1.0 - settings.phaserMix) + wetR * settings.phaserMix;
    }

    void processReverb (double& left, double& right) noexcept
    {
        if (settings.reverbMix <= 1.0e-6)
            return;

        constexpr int taps[8] { 1557, 1617, 1491, 1422, 1277, 1356, 1188, 1116 };
        const auto sizeScale = 0.35 + settings.reverbSize * 0.9;
        const auto feedback = 0.55 + settings.reverbDecay * 0.4;

        auto accL = 0.0;
        auto accR = 0.0;

        for (int t = 0; t < 8; ++t)
        {
            auto& line = reverbDelay[static_cast<std::size_t> (t)];
            const auto size = static_cast<int> (line.size());
            const auto delay = std::clamp (static_cast<int> (static_cast<double> (taps[t]) * sizeScale), 2, size - 2);
            const auto read = (reverbWrite - delay + size) % size;
            auto sample = line[static_cast<std::size_t> (read)];
            sample *= (1.0 - settings.reverbDamping * 0.4);
            line[static_cast<std::size_t> (reverbWrite)] = guards::sanitiseState (
                ((t & 1) == 0 ? left : right) + sample * feedback);

            if ((t & 1) == 0)
                accL += sample;
            else
                accR += sample;
        }

        reverbWrite = (reverbWrite + 1) % static_cast<int> (reverbDelay[0].size());

        accL *= 0.2;
        accR *= 0.2;

        const auto mid = 0.5 * (accL + accR);
        const auto side = 0.5 * (accL - accR) * settings.reverbWidth;
        const auto wetL = mid + side;
        const auto wetR = mid - side;

        left  = left  * (1.0 - settings.reverbMix) + wetL * settings.reverbMix;
        right = right * (1.0 - settings.reverbMix) + wetR * settings.reverbMix;
    }

    double sampleRate { 44100.0 };
    Settings settings;

    std::vector<double> delayL, delayR, chorusL, chorusR, combL, combR;
    std::array<std::vector<double>, 8> reverbDelay {};
    int delayWrite { 0 }, chorusWrite { 0 }, combWriteL { 0 }, combWriteR { 0 }, reverbWrite { 0 };
    int delayTargetSamplesL { 0 }, delayTargetSamplesR { 0 };
    int delayPrevSamplesL { 1 }, delayPrevSamplesR { 1 };
    double delayFadeL { 0.0 }, delayFadeR { 0.0 };
    double delayFadeStep { 1.0 / 882.0 };

    SvfState svfL, svfR;
    std::array<double, 6> phaserL {};
    std::array<double, 6> phaserR {};
    double lfoPhase { 0.0 };
    double toneLpL { 0.0 }, toneLpR { 0.0 };
    double delayLpL { 0.0 }, delayLpR { 0.0 };
};

} // namespace aethr::dsp
