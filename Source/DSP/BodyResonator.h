#pragma once

#include <algorithm>
#include <array>
#include <cmath>

#include "Core/AudioMath.h"
#include "Core/RealtimeGuards.h"

namespace aethr::dsp
{

enum class BodyPreset
{
    wooden = 0,
    metal,
    glass,
    hollow,
    bell,
    drum,
    crystalline,
    numPresets
};

/**
    Parallel modal body sitting after the string.

    Each mode is a resonant bandpass. Inactive modes are skipped, so a 1-mode
    wooden body is cheap and a 16-mode bell is paid for only when asked for.
    Mix 0 is a true bypass.
*/
class BodyResonator
{
public:
    static constexpr int maximumModes = 16;

    struct Settings
    {
        double mix { 0.0 };
        double decay { 0.4 };
        double brightness { 0.55 };
        int    activeModes { 4 };
        BodyPreset preset { BodyPreset::wooden };
        double fundamentalHz { 220.0 };
        double inharmonicity { 0.0 };
    };

    void prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
        reset();
    }

    void reset() noexcept
    {
        for (auto& mode : modes)
        {
            mode.z1 = 0.0;
            mode.z2 = 0.0;
        }
    }

    void setSettings (const Settings& newSettings) noexcept
    {
        settings = newSettings;
        settings.mix = std::clamp (settings.mix, 0.0, 1.0);
        settings.activeModes = std::clamp (settings.activeModes, 0, maximumModes);
        updateCoefficients();
    }

    [[nodiscard]] double process (double input) noexcept
    {
        if (settings.mix <= 1.0e-6 || settings.activeModes <= 0)
            return input;

        auto sum = 0.0;

        for (int i = 0; i < settings.activeModes; ++i)
        {
            auto& mode = modes[static_cast<std::size_t> (i)];
            const auto y = mode.b0 * input + mode.z1;
            mode.z1 = mode.b1 * input - mode.a1 * y + mode.z2;
            mode.z2 = mode.b2 * input - mode.a2 * y;
            mode.z1 = guards::sanitiseState (mode.z1);
            mode.z2 = guards::sanitiseState (mode.z2);
            sum += y * mode.gain;
        }

        const auto wet = guards::sanitiseState (sum);
        return input * (1.0 - settings.mix) + wet * settings.mix;
    }

    [[nodiscard]] bool isActive() const noexcept
    {
        return settings.mix > 1.0e-6 && settings.activeModes > 0;
    }

private:
    struct Mode
    {
        double b0 { 0.0 }, b1 { 0.0 }, b2 { 0.0 };
        double a1 { 0.0 }, a2 { 0.0 };
        double z1 { 0.0 }, z2 { 0.0 };
        double gain { 0.0 };
    };

    void updateCoefficients() noexcept
    {
        const auto ratios = ratiosFor (settings.preset);
        const auto nyquist = 0.5 * sampleRate;
        const auto q = 2.0 + settings.decay * 28.0;

        for (int i = 0; i < settings.activeModes; ++i)
        {
            const auto ratio = ratios[static_cast<std::size_t> (i)];
            const auto stretch = 1.0 + settings.inharmonicity * 0.55 * static_cast<double> (i);
            auto hz = settings.fundamentalHz * ratio * stretch;
            hz = std::clamp (hz, 20.0, nyquist * 0.95);

            const auto omega = math::twoPi * hz / sampleRate;
            const auto alpha = std::sin (omega) / (2.0 * q);
            const auto cosw = std::cos (omega);
            const auto a0 = 1.0 + alpha;

            auto& mode = modes[static_cast<std::size_t> (i)];
            mode.b0 = alpha / a0;
            mode.b1 = 0.0;
            mode.b2 = -alpha / a0;
            mode.a1 = -2.0 * cosw / a0;
            mode.a2 = (1.0 - alpha) / a0;

            const auto tilt = std::pow (ratio, -0.4 + settings.brightness);
            mode.gain = tilt / static_cast<double> (std::max (1, settings.activeModes));
        }
    }

    [[nodiscard]] static std::array<double, static_cast<std::size_t> (maximumModes)> ratiosFor (BodyPreset preset) noexcept
    {
        std::array<double, static_cast<std::size_t> (maximumModes)> ratios {};

        switch (preset)
        {
            case BodyPreset::wooden:
                ratios = { 1.00, 2.01, 3.05, 4.12, 5.20, 6.35, 7.50, 8.70,
                           10.0, 11.4, 12.9, 14.5, 16.2, 18.0, 20.0, 22.2 };
                break;
            case BodyPreset::metal:
                ratios = { 1.00, 2.76, 5.40, 8.93, 13.3, 18.6, 24.8, 31.9,
                           40.0, 49.0, 59.0, 70.0, 82.0, 95.0, 109.0, 124.0 };
                break;
            case BodyPreset::glass:
                ratios = { 1.00, 2.32, 4.10, 6.25, 8.80, 11.7, 15.0, 18.7,
                           22.8, 27.3, 32.2, 37.5, 43.2, 49.3, 55.8, 62.7 };
                break;
            case BodyPreset::hollow:
                ratios = { 1.00, 1.47, 2.09, 2.76, 3.53, 4.36, 5.26, 6.22,
                           7.24, 8.32, 9.46, 10.7, 11.9, 13.3, 14.7, 16.2 };
                break;
            case BodyPreset::bell:
                ratios = { 0.50, 1.00, 1.20, 1.50, 2.00, 2.50, 3.00, 4.00,
                           5.33, 6.66, 8.00, 10.0, 12.0, 14.0, 16.0, 18.0 };
                break;
            case BodyPreset::drum:
                ratios = { 1.00, 1.59, 2.14, 2.30, 2.92, 3.16, 3.50, 3.60,
                           4.05, 4.15, 4.64, 4.83, 5.06, 5.40, 5.54, 5.98 };
                break;
            case BodyPreset::crystalline:
                ratios = { 1.00, 1.41, 2.00, 2.83, 4.00, 5.66, 8.00, 11.3,
                           16.0, 22.6, 32.0, 45.3, 64.0, 90.5, 128.0, 181.0 };
                break;
            case BodyPreset::numPresets:
            default:
                ratios = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };
                break;
        }

        return ratios;
    }

    double sampleRate { 44100.0 };
    Settings settings;
    std::array<Mode, static_cast<std::size_t> (maximumModes)> modes {};
};

} // namespace aethr::dsp
