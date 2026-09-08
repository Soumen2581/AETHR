#pragma once

#include <cstdint>

namespace aethr::dsp
{

/**
    Excitation sources.

    A Karplus–Strong loop is only as interesting as what is put into it: the loop
    decides the pitch and the decay, but the excitation decides the attack and which
    partials are present to begin with. The first six are one-shot bursts; `bow` and
    `blow` are continuous and run for as long as the note is held, which is what turns
    the same resonator into a drone engine.
*/
enum class ExcitationType
{
    pluck = 0,    /**< Short noise burst with a fast attack. The reference plucked-string attack. */
    noiseBurst,   /**< Flat white burst, longer and blunter than a pluck. */
    pinkBurst,    /**< Pink-weighted burst. Weights energy towards low partials; warmer. */
    mallet,       /**< Lowpassed burst with a soft attack. Struck rather than plucked. */
    click,        /**< Near-impulse. Excites every partial equally; the brightest, thinnest attack. */
    metallic,     /**< Differentiated noise. Rising spectrum, emphasises high partials. */
    bow,          /**< Continuous low-level friction noise while held. Sustains indefinitely. */
    blow,         /**< Continuous air noise with a soft attack and a darker spectrum. */
    numTypes
};

/** True for excitations that continue for as long as the note is held. */
[[nodiscard]] constexpr bool isSustainedExcitation (ExcitationType type) noexcept
{
    return type == ExcitationType::bow || type == ExcitationType::blow;
}

//==============================================================================
/**
    Generates the signal injected into the resonator, for both channels at once.

    Both channels are produced together on purpose. Stereo width comes from
    *decorrelating* the excitation rather than from detuning or delaying one side:
    two resonators of identical length fed slightly different noise ring with
    different partial phases, which is wide and mono-compatible, and leaves the
    pitch identical in both channels. Detuning would widen it too, but at the cost
    of the tuning accuracy the whole engine is built around.

    Randomisation is driven by an explicitly seeded generator, so a preset that asks
    for variation still reproduces exactly when the seed is locked.
*/
class Exciter
{
public:
    /** Everything the host can change. Copied in per block; never read from the audio thread twice. */
    struct Settings
    {
        ExcitationType type { ExcitationType::pluck };
        double burstMilliseconds { 6.0 };
        double attack { 0.15 };         /**< 0 = instant, 1 = attack fills the whole burst. */
        double colour { 0.0 };          /**< -1 dark, 0 neutral, +1 bright spectral tilt. */
        double brightness { 0.7 };      /**< Lowpass corner, 0 = dull, 1 = open. */
        double level { 0.8 };
        double randomAmount { 0.15 };   /**< Per-note variation of duration, level and brightness. */
        double stereoSpread { 0.35 };   /**< 0 = identical channels, 1 = independent. */
        double velocityAmount { 0.75 }; /**< How much velocity scales level and brightness. */
    };

    struct StereoSample
    {
        double left { 0.0 };
        double right { 0.0 };
    };

    //==============================================================================
    void prepare (double newSampleRate) noexcept;
    void reset() noexcept;

    void setSettings (const Settings& newSettings) noexcept { settings = newSettings; }

    /**
        Starts a new excitation.

        @param velocity   0 to 1.
        @param seed       PRNG seed. The caller decides whether this is derived from a
                          locked user seed (reproducible) or from a running counter.
    */
    void noteOn (double velocity, std::uint32_t seed) noexcept;

    /** Ends a sustained excitation. One-shot excitations ignore this and finish naturally. */
    void noteOff() noexcept;

    /** Silences the excitation immediately, without a release. */
    void kill() noexcept;

    [[nodiscard]] StereoSample processSample() noexcept;

    [[nodiscard]] bool isActive() const noexcept { return active; }

    /** True while a sustained excitation is still being driven, so the voice must stay awake. */
    [[nodiscard]] bool isDriving() const noexcept { return active && isSustainedExcitation (settings.type); }

private:
    //==============================================================================
    /** Spectral tilt pivot for the colour control. Roughly the middle of the useful range. */
    static constexpr double colourPivotHz = 900.0;

    /** Brightness lowpass corner at brightness 0 and 1. */
    static constexpr double brightnessCornerMinHz = 250.0;
    static constexpr double brightnessCornerMaxHz = 18000.0;

    /** DC blocker corner. Excitation must be DC-free: the loop's damping filter passes DC. */
    static constexpr double dcBlockerCornerHz = 15.0;

    /** Release applied when a sustained excitation is let go. */
    static constexpr double sustainReleaseMilliseconds = 12.0;

    static constexpr double minimumBurstMilliseconds = 0.05;
    static constexpr double clickBurstMilliseconds = 0.15;

    /** Per-note random spread at randomAmount = 1. */
    static constexpr double randomDurationRange = 0.5;    // +/- 50 %
    static constexpr double randomLevelRange = 0.35;
    static constexpr double randomBrightnessRange = 0.4;

    //==============================================================================
    /** Per-channel filter state for the shaping chain. */
    struct ChannelState
    {
        double colourLowpass { 0.0 };
        double brightnessLowpass { 0.0 };
        double malletLowpass { 0.0 };
        double differentiatorLast { 0.0 };
        double dcBlockerLastInput { 0.0 };
        double dcBlockerLastOutput { 0.0 };
        double pinkStageOne { 0.0 };
        double pinkStageTwo { 0.0 };
        double pinkStageThree { 0.0 };
    };

    /**
        xorshift32. Chosen for being exactly reproducible from a 32-bit seed, allocation
        free and fast; excitation noise does not need cryptographic quality, it needs
        to sound the same twice.
    */
    struct NoiseSource
    {
        std::uint32_t state { 1 };

        void seed (std::uint32_t newSeed) noexcept
        {
            state = newSeed != 0 ? newSeed : 0x9e3779b9u;
        }

        [[nodiscard]] std::uint32_t nextBits() noexcept
        {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            return state;
        }

        /** Uniform in [-1, 1). */
        [[nodiscard]] double nextBipolar() noexcept
        {
            constexpr auto scale = 2.0 / 4294967296.0;
            return static_cast<double> (nextBits()) * scale - 1.0;
        }

        /** Uniform in [-1, 1), used for per-note parameter variation. */
        [[nodiscard]] double nextVariation() noexcept { return nextBipolar(); }
    };

    //==============================================================================
    [[nodiscard]] double rawSource (ChannelState& state, double whiteNoise) noexcept;
    [[nodiscard]] double shape (ChannelState& state, double input) noexcept;
    [[nodiscard]] double nextEnvelope() noexcept;
    void updateDerivedCoefficients() noexcept;

    //==============================================================================
    Settings settings;

    double sampleRate { 44100.0 };

    ChannelState leftState;
    ChannelState rightState;

    NoiseSource commonNoise;
    NoiseSource leftNoise;
    NoiseSource rightNoise;

    // Envelope
    bool active { false };
    bool releasing { false };
    int  attackSamples { 1 };
    int  decaySamples { 1 };
    int  position { 0 };
    double releaseLevel { 1.0 };
    double releaseCoefficient { 0.0 };

    // Per-note values, frozen at note-on so mid-note automation cannot re-randomise them.
    double noteLevel { 1.0 };
    double noteBrightness { 0.7 };
    double noteColour { 0.0 };

    // Coefficients derived from noteBrightness / settings.
    double colourCoefficient { 0.0 };
    double brightnessCoefficient { 0.0 };
    double malletCoefficient { 0.0 };
    double dcBlockerCoefficient { 0.0 };
    double spreadNormalisation { 1.0 };
};

} // namespace aethr::dsp
