#pragma once

#include "DSP/Dispersion.h"
#include "DSP/FractionalDelayLine.h"
#include "DSP/LoopFilter.h"

namespace aethr::dsp
{

/**
    One string. A delay line, a damping filter and a loss factor, closed into a loop.

    This is a generalised Karplus–Strong resonator rather than the 1983 original: the
    delay is fractional and phase-compensated, the damping filter is selectable and
    reports its own phase contribution, and the loss factor is derived from a decay
    time in seconds instead of being a magic number.

    Signal flow per sample:

        loopInput = excitation + loopGain * loopFilter(delayLine.read())
        delayLine.write(loopInput)
        output    = loopInput

    Taking the output at the loop *input* rather than the delay output matters: read
    at the delay output the excitation would not appear until one full period had
    elapsed, which at A0 is 36 ms of latency on the attack transient.

    ### Tuning

    The loop rings at f0 when the total phase delay around it is one period:

        fs / f0 = delayPhaseDelay(w0) + loopFilterPhaseDelay(w0)

    so the filter's phase delay is measured first and subtracted, and the delay line
    is then asked to realise the remainder as *phase* delay rather than as length.

    ### Decay

    The loop gain needed for a given T60 is `10^(-3 D / (T60 fs))`, where D is the
    real loop length. That has to be divided by the magnitude the filter and
    interpolator impose at f0, otherwise their loss shortens the decay. The result is
    clamped strictly below unity, which is what makes runaway impossible rather than
    merely unlikely.

    One resonator is one channel of one voice. It owns all of its state and allocates
    only in prepare(), so a voice can hold as many as it needs.
*/
class KarplusResonator
{
public:
    //==============================================================================
    /** Lowest fundamental the delay line is sized for. A0 transposed down two octaves is 6.9 Hz. */
    static constexpr double minimumFrequencyHz = 6.5;

    /** Shortest loop the tuner will attempt. Below this there is no room for the filter's phase. */
    static constexpr double minimumLoopSamples = 4.0;

    /**
        Hard ceiling on the loop gain. Strictly below 1, so total loop energy cannot grow.

        The margin has to be small, not merely safe-looking. Loop gain applies once per
        round trip, and a short loop makes many round trips per second: at C8 and 48 kHz
        the loop is 11.5 samples, so 4180 trips a second. A 0.9995 ceiling — which sounds
        conservative — would cap the decay there at 3.3 seconds no matter what the decay
        control said. 1e-5 of headroom lifts that to about 165 seconds while leaving the
        loop provably contracting.

        The resonant amplification of a continuous input is bounded by 1/(1-g), so this
        margin implies a worst case of 1e5. Two things make that safe rather than
        theoretical: the excitation is DC-blocked by a filter with an exact zero at DC, so
        there is no bias to amplify, and the loop state is clamped as a backstop.
    */
    static constexpr double maximumLoopGain = 0.99999;

    //==============================================================================
    void prepare (double newSampleRate);
    void reset() noexcept;

    //==============================================================================
    void setInterpolationMode (InterpolationMode mode) noexcept;
    void setLoopFilterMode (LoopFilterMode mode) noexcept;

    /** Target fundamental in Hz. Clamped to what the delay line can tune. */
    void setFrequency (double frequencyHz) noexcept;

    /** T60 of the fundamental, in seconds. */
    void setDecayTime (double seconds) noexcept;

    /** 0 = loop filter transparent (uniform decay), 1 = full damping at the chosen corner. */
    void setDamping (double amount) noexcept;

    /** Moves the damping corner up in frequency, relative to the fundamental. */
    void setBrightness (double amount) noexcept;

    /** Inharmonicity of upper partials. 0 is an ideal string. */
    void setStiffness (double amount) noexcept;

    /**
        Loop recirculation, 0–1. Scales the T60-derived loop gain.
        1 is full decay-compensated feedback; 0 is an open delay (no ring).
    */
    void setLoopFeedback (double amount) noexcept;

    /**
        Recomputes filter coefficient, tuning and loop gain.

        Call at block or chunk boundaries, never per sample: it evaluates `atan` and
        `pow`. Between calls the smoothed values drift towards these targets, which
        is inaudible over a block but would be a tuning error if left for seconds,
        hence "per chunk".
    */
    void updateCoefficients() noexcept;

    /** Jumps smoothed values to their targets. Used on note-on so a new note starts in tune. */
    void snapToTargets() noexcept;

    //==============================================================================
    /** Advances the loop by one sample, injecting `excitation`. Returns the loop input. */
    [[nodiscard]] double processSample (double excitation) noexcept;

    //==============================================================================
    /** Total loop phase delay in samples at the tuned frequency: the thing that sets pitch. */
    [[nodiscard]] double getLoopPhaseDelay() const noexcept { return loopPhaseDelay; }

    /** Frequency the loop is actually tuned to, derived from the realised phase delay. */
    [[nodiscard]] double getTunedFrequency() const noexcept;

    [[nodiscard]] double getLoopGain() const noexcept { return targetLoopGain; }

    /** T60 the loop will really deliver, which is shorter than requested if damping dominates. */
    [[nodiscard]] double getAchievedDecayTime() const noexcept;

    /** True when filter or interpolator loss, not the loop gain, is setting the decay. */
    [[nodiscard]] bool isDecayLimitedByLoss() const noexcept { return decayLimitedByLoss; }

    [[nodiscard]] double getFilterCoefficient() const noexcept { return loopFilter.getCoefficient(); }

    [[nodiscard]] const FractionalDelayLine& getDelayLine() const noexcept { return delayLine; }

private:
    //==============================================================================
    /** Damping corner as a multiple of the fundamental, at brightness 0 and 1. */
    static constexpr double dampingCornerRatioAtDarkest = 1.5;
    static constexpr double dampingCornerRatioAtBrightest = 96.0;

    /** Smoothing time constants. Short enough to feel immediate, long enough not to zipper. */
    static constexpr double gainSmoothingSeconds = 0.010;
    static constexpr double timbreSmoothingSeconds = 0.015;
    static constexpr double pitchSmoothingSeconds = 0.004;

    /** Below this difference a smoothed value is snapped, which also kills denormal tails. */
    static constexpr double smoothingSnapEpsilon = 1.0e-9;

    [[nodiscard]] static double smoothingCoefficientFor (double seconds, double sampleRate) noexcept;

    void updateFilterCoefficientTarget() noexcept;
    void updateTuning() noexcept;
    void updateLoopGain() noexcept;

    //==============================================================================
    FractionalDelayLine delayLine;
    LoopFilter loopFilter;
    Dispersion dispersion;

    double sampleRate { 44100.0 };
    double nyquist { 22050.0 };

    // Targets, set by the parameter layer.
    double targetFrequency { 220.0 };
    double decayTimeSeconds { 1.0 };
    double damping { 0.35 };
    double brightness { 0.6 };
    double stiffness { 0.0 };
    double loopFeedback { 1.0 };

    // Derived once per update.
    double targetDelay { 100.0 };
    double targetLoopGain { 0.0 };
    double targetFilterCoefficient { 0.0 };
    double loopPhaseDelay { 100.0 };
    double loopLossAtFundamental { 1.0 };
    bool   decayLimitedByLoss { false };

    // Smoothed, advanced per sample.
    double currentDelay { 100.0 };
    double currentLoopGain { 0.0 };
    double currentFilterCoefficient { 0.0 };

    double gainSmoothingCoefficient { 0.0 };
    double timbreSmoothingCoefficient { 0.0 };
    double pitchSmoothingCoefficient { 0.0 };
};

} // namespace aethr::dsp
