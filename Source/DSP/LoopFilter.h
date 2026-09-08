#pragma once

#include <algorithm>
#include <cmath>

#include "Core/RealtimeGuards.h"
#include "DSP/PhaseDelay.h"

namespace aethr::dsp
{

/**
    Damping filters available inside the resonator loop.

    All three are lowpass and minimum-phase with a DC gain of exactly one, which is
    the property that makes them safe in a feedback path: if the loop gain is below
    unity then no partial can grow, because no partial sees more gain than the loop
    gain itself.
*/
enum class LoopFilterMode
{
    oneZero = 0,   /**< (1-b) + b z^-1. The classic Karplus–Strong averager. Cheapest, gentlest, always stable. */
    onePole,       /**< (1-a)/(1 - a z^-1). Controllable cutoff, reaches much darker settings. */
    twoPole,       /**< Two cascaded one-poles. Steeper high-frequency loss for fast, dull decays. */
    numModes
};

/**
    The loop's damping filter.

    Every partial's decay rate is set by the product of the loop gain and this
    filter's magnitude at that partial's frequency, so this is where "bright metallic
    ring" versus "dull thud" actually lives.

    Coefficients are set directly rather than as a cutoff frequency in the audio path,
    because the mapping from cutoff to coefficient needs `exp` and would otherwise be
    evaluated per sample when smoothing. The caller ramps the coefficient instead;
    both mappings are monotonic, so a coefficient ramp is a well-behaved timbre ramp.
*/
class LoopFilter
{
public:
    /** Highest usable pole radius. Staying below 1 keeps the filter stable and finite. */
    static constexpr double maximumPoleRadius = 0.9995;

    /** The one-zero form cannot damp harder than a plain two-point average. */
    static constexpr double maximumZeroWeight = 0.5;

    void prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate;
        reset();
    }

    void reset() noexcept
    {
        lastInput = 0.0;
        stageOneState = 0.0;
        stageTwoState = 0.0;
    }

    void setMode (LoopFilterMode newMode) noexcept
    {
        if (newMode == mode)
            return;

        mode = newMode;
        reset();
    }

    [[nodiscard]] LoopFilterMode getMode() const noexcept { return mode; }

    /**
        Maps a cutoff frequency to this mode's coefficient.

        Not called from the audio path per sample; the resonator calls it once per
        block and ramps the result.
    */
    [[nodiscard]] double coefficientForCutoff (double cutoffHz) const noexcept
    {
        const auto nyquist = 0.5 * sampleRate;
        const auto clampedCutoff = std::clamp (cutoffHz, 1.0, nyquist * 0.999);
        const auto omega = phase::pi * clampedCutoff / nyquist;

        switch (mode)
        {
            case LoopFilterMode::oneZero:
            {
                // Solving |H(wc)|^2 = 1/2 for H = (1-b) + b z^-1 gives
                //     b^2 - b + k = 0,   k = 1 / (4 (1 - cos wc))
                // which only has a real root for wc >= pi/2. Below that the one-zero
                // form simply cannot lose 3 dB, so it saturates at the averager.
                const auto oneMinusCos = 1.0 - std::cos (omega);

                if (oneMinusCos <= 0.0)
                    return 0.0;

                const auto k = 0.25 / oneMinusCos;
                const auto discriminant = 1.0 - 4.0 * k;

                if (discriminant <= 0.0)
                    return maximumZeroWeight;

                return std::clamp (0.5 * (1.0 - std::sqrt (discriminant)), 0.0, maximumZeroWeight);
            }

            case LoopFilterMode::onePole:
            case LoopFilterMode::twoPole:
            case LoopFilterMode::numModes:
            default:
            {
                // Standard one-pole mapping: a = exp(-2 pi fc / fs).
                const auto a = std::exp (-2.0 * phase::pi * clampedCutoff / sampleRate);
                return std::clamp (a, 0.0, maximumPoleRadius);
            }
        }
    }

    /** Sets the coefficient directly. Values outside the stable range are clamped, not trusted. */
    void setCoefficient (double newCoefficient) noexcept
    {
        const auto limit = (mode == LoopFilterMode::oneZero) ? maximumZeroWeight : maximumPoleRadius;

        coefficient = guards::isValidCoefficient (newCoefficient)
                        ? std::clamp (newCoefficient, 0.0, limit)
                        : 0.0;
    }

    [[nodiscard]] double getCoefficient() const noexcept { return coefficient; }

    //==============================================================================
    [[nodiscard]] double process (double input) noexcept
    {
        switch (mode)
        {
            case LoopFilterMode::oneZero:
            {
                const auto output = (1.0 - coefficient) * input + coefficient * lastInput;
                lastInput = input;
                return output;
            }

            case LoopFilterMode::onePole:
            {
                stageOneState = (1.0 - coefficient) * input + coefficient * stageOneState;
                stageOneState = guards::sanitiseState (stageOneState);
                return stageOneState;
            }

            case LoopFilterMode::twoPole:
            {
                stageOneState = (1.0 - coefficient) * input + coefficient * stageOneState;
                stageOneState = guards::sanitiseState (stageOneState);
                stageTwoState = (1.0 - coefficient) * stageOneState + coefficient * stageTwoState;
                stageTwoState = guards::sanitiseState (stageTwoState);
                return stageTwoState;
            }

            case LoopFilterMode::numModes:
            default:
                return input;
        }
    }

    //==============================================================================
    /** Phase delay contributed to the loop at `omega`, in samples. */
    [[nodiscard]] double phaseDelayAt (double omega) const noexcept
    {
        switch (mode)
        {
            case LoopFilterMode::oneZero:
            {
                const double taps[2] { 1.0 - coefficient, coefficient };
                return phase::firPhaseDelay (taps, 2, 0.0, omega, coefficient);
            }

            case LoopFilterMode::onePole:
                return phase::onePolePhaseDelay (coefficient, omega);

            case LoopFilterMode::twoPole:
                return 2.0 * phase::onePolePhaseDelay (coefficient, omega);

            case LoopFilterMode::numModes:
            default:
                return 0.0;
        }
    }

    /** Magnitude at `omega`. The loop gain is divided by this to hit a requested decay time. */
    [[nodiscard]] double magnitudeAt (double omega) const noexcept
    {
        switch (mode)
        {
            case LoopFilterMode::oneZero:
            {
                const double taps[2] { 1.0 - coefficient, coefficient };
                return phase::firMagnitude (taps, 2, omega);
            }

            case LoopFilterMode::onePole:
                return phase::onePoleMagnitude (coefficient, omega);

            case LoopFilterMode::twoPole:
            {
                const auto single = phase::onePoleMagnitude (coefficient, omega);
                return single * single;
            }

            case LoopFilterMode::numModes:
            default:
                return 1.0;
        }
    }

private:
    double sampleRate { 44100.0 };
    LoopFilterMode mode { LoopFilterMode::onePole };

    double coefficient { 0.0 };

    double lastInput { 0.0 };
    double stageOneState { 0.0 };
    double stageTwoState { 0.0 };
};

} // namespace aethr::dsp
