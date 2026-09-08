#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include "Core/RealtimeGuards.h"
#include "DSP/PhaseDelay.h"

namespace aethr::dsp
{

/**
    Interpolation strategies for the fractional part of the loop delay.

    The choice matters far more inside a feedback loop than in a plain delay effect,
    because whatever the interpolator does to the magnitude response happens once per
    round trip and therefore compounds. See docs/DSP_NOTES.md for the comparison and
    the measured results that decided the default.
*/
enum class InterpolationMode
{
    linear = 0,      /**< Cheapest. Its loss varies with the fractional part, so decay time varies with pitch. */
    lagrangeCubic,   /**< Four-tap, third-order. Nearly flat, stateless with respect to the fraction. */
    allpass,         /**< First-order allpass. Exactly unity magnitude, so decay is set by the loop filter alone. */
    numModes
};

/**
    Delay line with fractional read, sized once and never reallocated afterwards.

    Usage in the resonator loop is: `setDelay` (per note or per block), then per
    sample `read()` followed by `write()`. Reading before writing means the shortest
    available delay is one sample, which is never a constraint here — the loop delay
    is always several samples.

    The class also answers analysis questions about itself (`phaseDelayFor`,
    `magnitudeAt`) so the tuning solver and the decay compensation can query the
    exact same maths the audio path uses, rather than a duplicate approximation.
*/
class FractionalDelayLine
{
public:
    FractionalDelayLine() = default;

    /**
        Allocates capacity for `maximumDelaySamples` of delay.

        Called from prepareToPlay only. The buffer is rounded up to a power of two so
        the read/write wrap is a mask rather than a modulo or a branch.
    */
    void prepare (int maximumDelaySamples)
    {
        const auto required = std::max (16, maximumDelaySamples + guardSamples);

        auto size = 16;
        while (size < required)
            size *= 2;

        buffer.assign (static_cast<std::size_t> (size), 0.0);
        bufferSize = size;
        mask = size - 1;
        maximumDelay = static_cast<double> (size - guardSamples);

        reset();
    }

    /** Clears the delay memory and the interpolator state. Does not change the delay setting. */
    void reset() noexcept
    {
        std::fill (buffer.begin(), buffer.end(), 0.0);
        writeIndex = 0;
        allpassLastInput = 0.0;
        allpassLastOutput = 0.0;
    }

    /** Clears only the interpolator's own state. Used when the mode changes. */
    void resetInterpolatorState() noexcept
    {
        allpassLastInput = 0.0;
        allpassLastOutput = 0.0;
    }

    void setInterpolation (InterpolationMode newMode) noexcept
    {
        if (newMode == mode)
            return;

        mode = newMode;

        // The allpass carries state that only means something for the coefficient it
        // was produced with, so switching modes must not carry it across.
        resetInterpolatorState();
        updateSplit();
    }

    [[nodiscard]] InterpolationMode getInterpolation() const noexcept { return mode; }

    /**
        Frequency, in radians per sample, that the allpass coefficient is designed for.

        The allpass is the only mode whose coefficient depends on frequency: its
        fractional delay is exact at one frequency and drifts either side of it. For a
        resonator loop that frequency is the fundamental, which is the frequency whose
        tuning matters. The interpolating modes ignore this.
    */
    void setTuningOmega (double omega) noexcept
    {
        if (std::abs (omega - tuningOmega) < 1.0e-15)
            return;

        tuningOmega = omega;

        if (mode == InterpolationMode::allpass)
            updateSplit();
    }

    [[nodiscard]] double getTuningOmega() const noexcept { return tuningOmega; }

    /** Shortest total delay this mode can read, given the taps it needs. */
    [[nodiscard]] double getMinimumDelay() const noexcept { return minimumDelayFor (mode); }

    [[nodiscard]] double getMaximumDelay() const noexcept { return maximumDelay; }

    /** Sets the total delay in samples. Clamped to what the mode and buffer can provide. */
    void setDelay (double delaySamples) noexcept
    {
        const auto clamped = std::clamp (delaySamples, minimumDelayFor (mode), maximumDelay);

        if (std::abs (clamped - requestedDelay) < 1.0e-12)
            return;

        requestedDelay = clamped;
        updateSplit();
    }

    [[nodiscard]] double getDelay() const noexcept { return requestedDelay; }

    //==============================================================================
    void write (double value) noexcept
    {
        buffer[static_cast<std::size_t> (writeIndex)] = value;
        writeIndex = (writeIndex + 1) & mask;
    }

    /** Reads at the delay set by setDelay(). Advances interpolator state in allpass mode. */
    [[nodiscard]] double read() noexcept
    {
        switch (mode)
        {
            case InterpolationMode::linear:
                return coefficients[0] * tap (integerDelay)
                     + coefficients[1] * tap (integerDelay + 1);

            case InterpolationMode::lagrangeCubic:
                return coefficients[0] * tap (integerDelay - 1)
                     + coefficients[1] * tap (integerDelay)
                     + coefficients[2] * tap (integerDelay + 1)
                     + coefficients[3] * tap (integerDelay + 2);

            case InterpolationMode::allpass:
            {
                const auto input = tap (integerDelay);

                // y[n] = c x[n] + x[n-1] - c y[n-1]
                const auto output = allpassCoefficient * input
                                  + allpassLastInput
                                  - allpassCoefficient * allpassLastOutput;

                allpassLastInput = input;
                allpassLastOutput = guards::sanitiseState (output);

                return allpassLastOutput;
            }

            case InterpolationMode::numModes:
            default:
                return tap (integerDelay);
        }
    }

    //==============================================================================
    /**
        Phase delay actually achieved for a requested total delay at frequency `omega`.

        This is the function the tuning solver inverts. It deliberately mirrors the
        tap layout used by `read()`, so the answer describes the real signal path
        rather than an idealised one.
    */
    [[nodiscard]] double phaseDelayFor (double requested, double omega) const noexcept
    {
        const auto clamped = std::clamp (requested, minimumDelayFor (mode), maximumDelay);

        switch (mode)
        {
            case InterpolationMode::linear:
            {
                const auto base = std::floor (clamped);
                const auto fraction = clamped - base;

                double taps[2];
                phase::linearCoefficients (fraction, taps);

                return phase::firPhaseDelay (taps, 2, base, omega, clamped);
            }

            case InterpolationMode::lagrangeCubic:
            {
                const auto base = std::floor (clamped);
                const auto fraction = clamped - base;

                double taps[4];
                phase::lagrangeCubicCoefficients (fraction, taps);

                return phase::firPhaseDelay (taps, 4, base - 1.0, omega, clamped);
            }

            case InterpolationMode::allpass:
            {
                const auto base = std::floor (clamped - allpassFractionOffset);
                const auto delta = clamped - base;

                // Designed at the tuning frequency, then evaluated at whatever `omega`
                // was asked about - so this reports the real response, including the
                // drift away from the frequency the coefficient was designed for.
                return base + phase::allpassPhaseDelay (allpassCoefficientFor (delta), omega);
            }

            case InterpolationMode::numModes:
            default:
                return clamped;
        }
    }

    /**
        Magnitude response at `omega` for the delay currently set.

        Unity for the allpass by construction; below unity for the interpolating
        modes, which is why the decay compensation has to know about it.
    */
    [[nodiscard]] double magnitudeAt (double omega) const noexcept
    {
        switch (mode)
        {
            case InterpolationMode::linear:
                return phase::firMagnitude (coefficients, 2, omega);

            case InterpolationMode::lagrangeCubic:
                return phase::firMagnitude (coefficients, 4, omega);

            case InterpolationMode::allpass:
            case InterpolationMode::numModes:
            default:
                return 1.0;
        }
    }

    /**
        Magnitude response at `omega` for an arbitrary requested delay.

        Needed by the decay compensation, which has to know the loss of the delay
        setting it is *about* to use rather than the one currently in effect.
    */
    [[nodiscard]] double magnitudeFor (double requested, double omega) const noexcept
    {
        const auto clamped = std::clamp (requested, minimumDelayFor (mode), maximumDelay);
        const auto base = std::floor (clamped);
        const auto fraction = clamped - base;

        switch (mode)
        {
            case InterpolationMode::linear:
            {
                double taps[2];
                phase::linearCoefficients (fraction, taps);
                return phase::firMagnitude (taps, 2, omega);
            }

            case InterpolationMode::lagrangeCubic:
            {
                double taps[4];
                phase::lagrangeCubicCoefficients (fraction, taps);
                return phase::firMagnitude (taps, 4, omega);
            }

            case InterpolationMode::allpass:
            case InterpolationMode::numModes:
            default:
                return 1.0;
        }
    }

    /**
        Finds the delay setting whose *phase* delay equals `targetPhaseDelay` at `omega`.

        Newton's method is unnecessary: d(phase delay)/d(requested delay) is
        essentially 1 for every mode, so plain functional iteration converges in two
        or three steps. Three are used, which measures as sub-0.001 cent.

        Does not commit the result: the resonator needs the answer in order to derive its
        loop gain before it changes the delay, and the change itself is smoothed.

        It does, however, set the tuning frequency, because designing the allpass for one
        frequency and solving at another silently reintroduces the gaps in the achievable
        delays that the closed form exists to remove.
    */
    [[nodiscard]] double solveDelayForPhaseDelay (double targetPhaseDelay, double omega) noexcept
    {
        setTuningOmega (omega);

        auto request = targetPhaseDelay;

        for (int iteration = 0; iteration < tuningIterations; ++iteration)
        {
            const auto error = targetPhaseDelay - phaseDelayFor (request, omega);

            if (std::abs (error) < 1.0e-12)
                break;

            request += error;
        }

        return std::clamp (request, minimumDelayFor (mode), maximumDelay);
    }

private:
    static constexpr int guardSamples = 4;      // headroom for the outer Lagrange taps

    /**
        Iteration budget for the tuning solve.

        The allpass mode is exact and exits after one evaluation. The interpolating
        modes converge geometrically at roughly a factor of twenty per step, and at high
        frequencies they start further out, so a generous budget costs nothing: this runs
        when a note or a parameter changes, never per sample.
    */
    static constexpr int tuningIterations = 8;

    /** The allpass is well-conditioned for a fractional delay in [0.5, 1.5). */
    static constexpr double allpassFractionOffset = 0.5;

    /** Largest allpass coefficient magnitude permitted. Below 1 keeps the section stable. */
    static constexpr double maximumAllpassCoefficient = 0.999;

    /** Allpass coefficient for a fractional delay, designed at the tuning frequency. */
    [[nodiscard]] double allpassCoefficientFor (double fractionalDelay) const noexcept
    {
        const auto coefficient = phase::allpassCoefficientForPhaseDelay (fractionalDelay, tuningOmega);

        if (! std::isfinite (coefficient))
            return 0.0;

        return std::clamp (coefficient, -maximumAllpassCoefficient, maximumAllpassCoefficient);
    }

    [[nodiscard]] static double minimumDelayFor (InterpolationMode m) noexcept
    {
        switch (m)
        {
            case InterpolationMode::linear:        return 1.0;   // taps at I, I+1
            case InterpolationMode::lagrangeCubic: return 2.0;   // taps at I-1 ... I+2
            case InterpolationMode::allpass:       return 1.5;   // integer tap >= 1
            case InterpolationMode::numModes:
            default:                               return 1.0;
        }
    }

    [[nodiscard]] double tap (int delaySamples) const noexcept
    {
        const auto index = (writeIndex - delaySamples + bufferSize) & mask;
        return buffer[static_cast<std::size_t> (index)];
    }

    void updateSplit() noexcept
    {
        switch (mode)
        {
            case InterpolationMode::linear:
            {
                const auto base = std::floor (requestedDelay);
                integerDelay = static_cast<int> (base);
                phase::linearCoefficients (requestedDelay - base, coefficients);
                break;
            }

            case InterpolationMode::lagrangeCubic:
            {
                const auto base = std::floor (requestedDelay);
                integerDelay = static_cast<int> (base);
                phase::lagrangeCubicCoefficients (requestedDelay - base, coefficients);
                break;
            }

            case InterpolationMode::allpass:
            {
                const auto base = std::floor (requestedDelay - allpassFractionOffset);
                integerDelay = static_cast<int> (base);
                allpassCoefficient = allpassCoefficientFor (requestedDelay - base);
                break;
            }

            case InterpolationMode::numModes:
            default:
                integerDelay = static_cast<int> (std::floor (requestedDelay));
                break;
        }
    }

    std::vector<double> buffer;
    int bufferSize { 0 };
    int mask { 0 };
    int writeIndex { 0 };

    InterpolationMode mode { InterpolationMode::allpass };

    double requestedDelay { 2.0 };
    double maximumDelay { 0.0 };
    double tuningOmega { 0.0 };
    int    integerDelay { 2 };

    double coefficients[4] { 0.0, 1.0, 0.0, 0.0 };

    double allpassCoefficient { 0.0 };
    double allpassLastInput { 0.0 };
    double allpassLastOutput { 0.0 };
};

} // namespace aethr::dsp
