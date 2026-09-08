#include "DSP/KarplusResonator.h"

#include <algorithm>
#include <cmath>

#include "Core/AudioMath.h"

namespace aethr::dsp
{

//==============================================================================
double KarplusResonator::smoothingCoefficientFor (double seconds, double rate) noexcept
{
    if (seconds <= 0.0 || rate <= 0.0)
        return 1.0;

    // One-pole step response reaching 1 - 1/e in `seconds`.
    return 1.0 - std::exp (-1.0 / (seconds * rate));
}

void KarplusResonator::prepare (double newSampleRate)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    nyquist = 0.5 * sampleRate;

    // Sized for the lowest fundamental the engine admits, plus room for the
    // interpolator's outer taps. Allocation happens here and nowhere else.
    const auto longestLoop = static_cast<int> (std::ceil (sampleRate / minimumFrequencyHz)) + 8;

    delayLine.prepare (longestLoop);
    loopFilter.prepare (sampleRate);
    dispersion.reset();

    gainSmoothingCoefficient   = smoothingCoefficientFor (gainSmoothingSeconds, sampleRate);
    timbreSmoothingCoefficient = smoothingCoefficientFor (timbreSmoothingSeconds, sampleRate);
    pitchSmoothingCoefficient  = smoothingCoefficientFor (pitchSmoothingSeconds, sampleRate);

    updateCoefficients();
    snapToTargets();
    reset();
}

void KarplusResonator::reset() noexcept
{
    delayLine.reset();
    loopFilter.reset();
    dispersion.reset();

    currentLoopGain = targetLoopGain;
    currentFilterCoefficient = targetFilterCoefficient;
    currentDelay = targetDelay;

    delayLine.setDelay (currentDelay);
    loopFilter.setCoefficient (currentFilterCoefficient);
}

//==============================================================================
void KarplusResonator::setInterpolationMode (InterpolationMode mode) noexcept
{
    if (mode == delayLine.getInterpolation())
        return;

    delayLine.setInterpolation (mode);

    // Tap layout and phase response both changed, so the tuning is stale.
    updateCoefficients();
}

void KarplusResonator::setLoopFilterMode (LoopFilterMode mode) noexcept
{
    if (mode == loopFilter.getMode())
        return;

    loopFilter.setMode (mode);

    // Mode change resets the filter state and rescales what the coefficient means.
    currentFilterCoefficient = 0.0;
    updateCoefficients();
    currentFilterCoefficient = targetFilterCoefficient;
    loopFilter.setCoefficient (currentFilterCoefficient);
    updateCoefficients();
}

void KarplusResonator::setFrequency (double frequencyHz) noexcept
{
    const auto highestTunable = sampleRate / minimumLoopSamples;
    targetFrequency = std::clamp (frequencyHz, minimumFrequencyHz, highestTunable);
}

void KarplusResonator::setDecayTime (double seconds) noexcept
{
    decayTimeSeconds = std::max (0.0, seconds);
}

void KarplusResonator::setDamping (double amount) noexcept
{
    damping = std::clamp (amount, 0.0, 1.0);
}

void KarplusResonator::setBrightness (double amount) noexcept
{
    brightness = std::clamp (amount, 0.0, 1.0);
}

void KarplusResonator::setStiffness (double amount) noexcept
{
    stiffness = std::clamp (amount, 0.0, 1.0);
}

void KarplusResonator::setLoopFeedback (double amount) noexcept
{
    loopFeedback = std::clamp (amount, 0.0, 1.0);
}

//==============================================================================
void KarplusResonator::updateFilterCoefficientTarget() noexcept
{
    // The damping corner tracks the fundamental, so a chord holds its timbre instead
    // of the top notes turning into sine waves.
    const auto ratioSpan = dampingCornerRatioAtBrightest / dampingCornerRatioAtDarkest;
    const auto cornerRatio = dampingCornerRatioAtDarkest * std::pow (ratioSpan, brightness);
    const auto cornerHz = std::clamp (targetFrequency * cornerRatio, minimumFrequencyHz, nyquist * 0.999);

    // Scaling by `damping` is what makes damping = 0 an exact bypass. That matters:
    // a filter that can only *nearly* be bypassed puts a floor under the loop loss,
    // and at the top of the keyboard that floor alone would cap the decay at a
    // fraction of a second no matter what decay time was asked for.
    targetFilterCoefficient = damping * loopFilter.coefficientForCutoff (cornerHz);
}

void KarplusResonator::updateTuning() noexcept
{
    const auto omega = math::twoPi * targetFrequency / sampleRate;
    const auto periodInSamples = sampleRate / targetFrequency;

    // Everything in the loop contributes phase, so the delay line only has to supply
    // what is left over after the damping filter has taken its share.
    const auto filterPhaseDelay = loopFilter.phaseDelayAt (omega);
    const auto dispersionDelay = dispersion.phaseDelayAt (omega);
    const auto delayShare = std::max (delayLine.getMinimumDelay(),
                                      periodInSamples - filterPhaseDelay - dispersionDelay);

    // Solving also designs the allpass interpolator for this frequency, so the phase
    // delay queried afterwards is the one the loop will actually have.
    targetDelay = delayLine.solveDelayForPhaseDelay (delayShare, omega);
    loopPhaseDelay = delayLine.phaseDelayFor (targetDelay, omega) + filterPhaseDelay + dispersionDelay;
}

void KarplusResonator::updateLoopGain() noexcept
{
    const auto omega = math::twoPi * targetFrequency / sampleRate;

    // Loss the fundamental suffers per round trip from everything that is not the
    // loop gain itself. The interpolator contributes here too unless it is allpass.
    loopLossAtFundamental = loopFilter.magnitudeAt (omega) * delayLine.magnitudeFor (targetDelay, omega);

    const auto requiredPerLoopGain = math::decayTimeToLoopGain (decayTimeSeconds, loopPhaseDelay, sampleRate);
    const auto compensated = loopLossAtFundamental > math::silenceGain
                               ? requiredPerLoopGain / loopLossAtFundamental
                               : 0.0;

    // Requesting a long decay from a heavily damped loop asks for a gain above unity,
    // which is exactly the request that must never be granted.
    decayLimitedByLoss = compensated > maximumLoopGain;
    targetLoopGain = guards::clampLoopGain (compensated * loopFeedback, maximumLoopGain);
}

void KarplusResonator::updateCoefficients() noexcept
{
    dispersion.setAmount (stiffness);
    updateFilterCoefficientTarget();

    // Tune against the coefficient the filter is really running, not the one it is
    // heading towards, so that a timbre ramp does not drag the pitch with it.
    loopFilter.setCoefficient (currentFilterCoefficient);

    updateTuning();
    updateLoopGain();
}

void KarplusResonator::snapToTargets() noexcept
{
    currentFilterCoefficient = targetFilterCoefficient;
    loopFilter.setCoefficient (currentFilterCoefficient);

    // Re-solve now that the filter is at its final coefficient: its phase delay has
    // changed, so the delay length that was correct a moment ago no longer is.
    updateTuning();
    updateLoopGain();

    currentLoopGain = targetLoopGain;
    currentDelay = targetDelay;
    delayLine.setDelay (currentDelay);
}

//==============================================================================
double KarplusResonator::processSample (double excitation) noexcept
{
    // Pitch is smoothed rather than stepped: a jump in read position is a
    // discontinuity in the delay output, which is audible as a click.
    const auto delayError = targetDelay - currentDelay;

    if (std::abs (delayError) > smoothingSnapEpsilon)
    {
        currentDelay += delayError * pitchSmoothingCoefficient;
        delayLine.setDelay (currentDelay);
    }
    else if (std::abs (delayError) > 0.0)
    {
        currentDelay = targetDelay;
        delayLine.setDelay (currentDelay);
    }

    const auto gainError = targetLoopGain - currentLoopGain;
    currentLoopGain = std::abs (gainError) > smoothingSnapEpsilon
                        ? currentLoopGain + gainError * gainSmoothingCoefficient
                        : targetLoopGain;

    const auto coefficientError = targetFilterCoefficient - currentFilterCoefficient;

    if (std::abs (coefficientError) > smoothingSnapEpsilon)
    {
        currentFilterCoefficient += coefficientError * timbreSmoothingCoefficient;
        loopFilter.setCoefficient (currentFilterCoefficient);
    }

    const auto delayed = delayLine.read();
    const auto filtered = loopFilter.process (delayed);
    const auto dispersed = dispersion.process (filtered);

    // Three independent stability measures, in order of how much they are relied on:
    // the loop gain is provably below unity, the state is sanitised so a NaN cannot
    // circulate, and the ceiling is a backstop that should never be reached.
    const auto loopInput = guards::sanitiseState (excitation + currentLoopGain * dispersed);

    delayLine.write (loopInput);

    return loopInput;
}

//==============================================================================
double KarplusResonator::getTunedFrequency() const noexcept
{
    return loopPhaseDelay > 0.0 ? sampleRate / loopPhaseDelay : 0.0;
}

double KarplusResonator::getAchievedDecayTime() const noexcept
{
    return math::loopGainToDecayTime (targetLoopGain * loopLossAtFundamental, loopPhaseDelay, sampleRate);
}

} // namespace aethr::dsp
