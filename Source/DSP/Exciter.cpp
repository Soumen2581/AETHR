#include "DSP/Exciter.h"

#include <algorithm>
#include <cmath>

#include "Core/AudioMath.h"
#include "Core/RealtimeGuards.h"

namespace aethr::dsp
{

namespace
{
    /** One-pole lowpass coefficient for a corner frequency. */
    [[nodiscard]] double onePoleCoefficient (double cornerHz, double sampleRate) noexcept
    {
        if (sampleRate <= 0.0)
            return 0.0;

        const auto clamped = std::clamp (cornerHz, 1.0, 0.49 * sampleRate);
        return std::exp (-math::twoPi * clamped / sampleRate);
    }

    /** Level compensation for the mallet's extra lowpass, which otherwise sounds much quieter. */
    constexpr double malletMakeUpGain = 3.2;

    /** The differentiator halves in amplitude on average; put it back. */
    constexpr double metallicMakeUpGain = 0.7;

    /** Pink filter coefficients (Kellett's economy approximation) and its output trim. */
    constexpr double pinkPoleOne = 0.99765;
    constexpr double pinkPoleTwo = 0.96300;
    constexpr double pinkPoleThree = 0.57000;
    constexpr double pinkGainOne = 0.0990460;
    constexpr double pinkGainTwo = 0.2965164;
    constexpr double pinkGainThree = 1.0526913;
    constexpr double pinkDirectGain = 0.1848;
    constexpr double pinkOutputTrim = 0.25;
}

//==============================================================================
void Exciter::prepare (double newSampleRate) noexcept
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    dcBlockerCoefficient = onePoleCoefficient (dcBlockerCornerHz, sampleRate);
    reset();
}

void Exciter::reset() noexcept
{
    leftState = ChannelState {};
    rightState = ChannelState {};

    active = false;
    releasing = false;
    position = 0;
    releaseLevel = 1.0;
}

void Exciter::kill() noexcept
{
    active = false;
    releasing = false;
    position = 0;
}

//==============================================================================
void Exciter::updateDerivedCoefficients() noexcept
{
    colourCoefficient = onePoleCoefficient (colourPivotHz, sampleRate);

    const auto span = brightnessCornerMaxHz / brightnessCornerMinHz;
    const auto cornerHz = brightnessCornerMinHz * std::pow (span, std::clamp (noteBrightness, 0.0, 1.0));
    brightnessCoefficient = onePoleCoefficient (cornerHz, sampleRate);

    // The mallet's own lowpass tracks the brightness control but stays well below it,
    // which is what separates "struck felt" from "plucked nail".
    malletCoefficient = onePoleCoefficient (cornerHz * 0.18, sampleRate);

    // Blending a common stream with an independent one halves the variance at the
    // midpoint, so the level would dip in the middle of the spread control.
    const auto spread = std::clamp (settings.stereoSpread, 0.0, 1.0);
    const auto variance = (1.0 - spread) * (1.0 - spread) + spread * spread;
    spreadNormalisation = variance > 0.0 ? 1.0 / std::sqrt (variance) : 1.0;
}

void Exciter::noteOn (double velocity, std::uint32_t seed) noexcept
{
    const auto clampedVelocity = std::clamp (velocity, 0.0, 1.0);

    // Three streams from one seed: derived by multiplying with odd 32-bit constants so
    // the channel streams are decorrelated but still fully determined by the seed.
    commonNoise.seed (seed);
    leftNoise.seed (seed * 0x9e3779b9u + 0x85ebca6bu);
    rightNoise.seed (seed * 0xc2b2ae35u + 0x27d4eb2fu);

    // Draw variation from the common stream so it is reproducible with the seed.
    const auto randomAmount = std::clamp (settings.randomAmount, 0.0, 1.0);
    const auto durationVariation = 1.0 + randomAmount * randomDurationRange * commonNoise.nextVariation();
    const auto levelVariation = 1.0 + randomAmount * randomLevelRange * commonNoise.nextVariation();
    const auto brightnessVariation = randomAmount * randomBrightnessRange * commonNoise.nextVariation();

    // Velocity curve: slightly concave, which tracks perceived loudness better than a
    // straight line. `velocityAmount` at 0 means the engine ignores velocity entirely.
    const auto velocityCurve = std::pow (clampedVelocity, 1.4);
    const auto velocityGain = math::lerp (1.0, velocityCurve, std::clamp (settings.velocityAmount, 0.0, 1.0));

    noteLevel = std::clamp (settings.level, 0.0, 1.0) * velocityGain * std::max (0.0, levelVariation);

    // Harder playing also opens the excitation up, which is what makes velocity read
    // as effort rather than just as volume.
    const auto velocityBrightness = std::clamp (settings.velocityAmount, 0.0, 1.0) * (clampedVelocity - 0.5) * 0.4;
    noteBrightness = std::clamp (settings.brightness + velocityBrightness + brightnessVariation, 0.0, 1.0);
    noteColour = std::clamp (settings.colour, -1.0, 1.0);

    updateDerivedCoefficients();

    const auto requestedMilliseconds = settings.type == ExcitationType::click
                                         ? clickBurstMilliseconds
                                         : std::max (minimumBurstMilliseconds, settings.burstMilliseconds);
    const auto burstMilliseconds = std::max (minimumBurstMilliseconds, requestedMilliseconds * durationVariation);
    const auto burstSamples = std::max (2.0, burstMilliseconds * 0.001 * sampleRate);

    const auto attackFraction = std::clamp (settings.attack, 0.0, 1.0);

    if (isSustainedExcitation (settings.type))
    {
        // Sustained: the burst length becomes the fade-in time, and the note holds.
        attackSamples = std::max (1, static_cast<int> (burstSamples * math::lerp (0.15, 4.0, attackFraction)));
        decaySamples = 0;
    }
    else
    {
        // Reserve at least a little decay: an envelope that is pure attack ends on a
        // step, and a step into a feedback loop is a click that never fully leaves.
        attackSamples = std::max (1, static_cast<int> (burstSamples * attackFraction * 0.9));
        decaySamples = std::max (1, static_cast<int> (burstSamples) - attackSamples);
    }

    releaseCoefficient = onePoleCoefficient (1000.0 / sustainReleaseMilliseconds, sampleRate);
    releaseLevel = 1.0;
    releasing = false;
    position = 0;
    active = true;
}

void Exciter::noteOff() noexcept
{
    if (active && isSustainedExcitation (settings.type))
        releasing = true;
}

//==============================================================================
double Exciter::nextEnvelope() noexcept
{
    if (! active)
        return 0.0;

    if (isSustainedExcitation (settings.type))
    {
        // Release is applied even mid-attack, so letting go of a slow bow stops it
        // rising rather than making it complete a swell first.
        if (releasing)
        {
            releaseLevel *= releaseCoefficient;

            if (releaseLevel < math::silenceGain)
            {
                active = false;
                return 0.0;
            }
        }

        auto attackValue = 1.0;

        if (position < attackSamples)
        {
            const auto progress = static_cast<double> (position) / static_cast<double> (attackSamples);
            attackValue = 0.5 - 0.5 * std::cos (math::pi * progress);
            ++position;
        }

        return attackValue * releaseLevel;
    }

    if (position < attackSamples)
    {
        // Raised cosine: starts and ends with zero slope, so it cannot click.
        const auto progress = static_cast<double> (position) / static_cast<double> (attackSamples);
        ++position;

        return 0.5 - 0.5 * std::cos (math::pi * progress);
    }

    const auto decayPosition = position - attackSamples;

    if (decayPosition >= decaySamples)
    {
        active = false;
        return 0.0;
    }

    ++position;

    const auto progress = static_cast<double> (decayPosition) / static_cast<double> (decaySamples);

    // Falling half of a raised cosine: reaches exactly zero at the end of the burst.
    return 0.5 + 0.5 * std::cos (math::pi * progress);
}

double Exciter::rawSource (ChannelState& state, double whiteNoise) noexcept
{
    switch (settings.type)
    {
        case ExcitationType::pluck:
        case ExcitationType::noiseBurst:
        case ExcitationType::bow:
            return whiteNoise;

        case ExcitationType::click:
            // The envelope is already only a few samples long, so the source can stay
            // flat; shaping a click any further just removes the partials it exists to excite.
            return whiteNoise;

        case ExcitationType::pinkBurst:
        case ExcitationType::blow:
        {
            state.pinkStageOne   = pinkPoleOne   * state.pinkStageOne   + whiteNoise * pinkGainOne;
            state.pinkStageTwo   = pinkPoleTwo   * state.pinkStageTwo   + whiteNoise * pinkGainTwo;
            state.pinkStageThree = pinkPoleThree * state.pinkStageThree + whiteNoise * pinkGainThree;

            const auto pink = state.pinkStageOne + state.pinkStageTwo + state.pinkStageThree
                            + whiteNoise * pinkDirectGain;

            return pinkOutputTrim * guards::sanitiseState (pink);
        }

        case ExcitationType::mallet:
        {
            state.malletLowpass = malletCoefficient * state.malletLowpass
                                + (1.0 - malletCoefficient) * whiteNoise;
            state.malletLowpass = guards::sanitiseState (state.malletLowpass);

            return malletMakeUpGain * state.malletLowpass;
        }

        case ExcitationType::metallic:
        {
            const auto differentiated = whiteNoise - state.differentiatorLast;
            state.differentiatorLast = whiteNoise;

            return metallicMakeUpGain * differentiated;
        }

        case ExcitationType::numTypes:
        default:
            return whiteNoise;
    }
}

double Exciter::shape (ChannelState& state, double input) noexcept
{
    // Spectral tilt. Splitting into lowpass and its complement and recombining with a
    // signed weight keeps unity gain at the centre position, which a plain shelf would not.
    state.colourLowpass = colourCoefficient * state.colourLowpass + (1.0 - colourCoefficient) * input;
    state.colourLowpass = guards::sanitiseState (state.colourLowpass);

    const auto low = state.colourLowpass;
    const auto high = input - low;
    const auto tilted = (input + noteColour * (high - low)) / (1.0 + std::abs (noteColour));

    // Brightness: plain lowpass, normalised so that closing it does not just fade out.
    state.brightnessLowpass = brightnessCoefficient * state.brightnessLowpass
                            + (1.0 - brightnessCoefficient) * tilted;
    state.brightnessLowpass = guards::sanitiseState (state.brightnessLowpass);

    const auto brightnessMakeUp = 1.0 / std::max (0.15, 1.0 - brightnessCoefficient);
    const auto filtered = state.brightnessLowpass * std::min (brightnessMakeUp, 4.0);

    // DC blocker. The loop's damping filter has unity gain at DC, so any DC offset in
    // the excitation would sit in the loop decaying only at the loop-gain rate and
    // would bias the whole resonator.
    const auto blocked = filtered - state.dcBlockerLastInput
                       + dcBlockerCoefficient * state.dcBlockerLastOutput;

    state.dcBlockerLastInput = filtered;
    state.dcBlockerLastOutput = guards::sanitiseState (blocked);

    return state.dcBlockerLastOutput;
}

Exciter::StereoSample Exciter::processSample() noexcept
{
    const auto envelope = nextEnvelope();

    if (! active && envelope <= 0.0)
        return {};

    const auto spread = std::clamp (settings.stereoSpread, 0.0, 1.0);

    const auto common = commonNoise.nextBipolar();
    const auto independentLeft = leftNoise.nextBipolar();
    const auto independentRight = rightNoise.nextBipolar();

    const auto whiteLeft = math::lerp (common, independentLeft, spread) * spreadNormalisation;
    const auto whiteRight = math::lerp (common, independentRight, spread) * spreadNormalisation;

    const auto gain = envelope * noteLevel;

    return { gain * shape (leftState, rawSource (leftState, whiteLeft)),
             gain * shape (rightState, rawSource (rightState, whiteRight)) };
}

} // namespace aethr::dsp
