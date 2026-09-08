#pragma once

#include <cstddef>
#include <vector>

namespace aethr::testing
{

/** Result of a fundamental-frequency measurement. */
struct PitchEstimate
{
    double frequencyHz { 0.0 };
    double peakMagnitude { 0.0 };
    bool   valid { false };
};

/**
    Measures the fundamental of a decaying harmonic signal.

    Method: a windowed discrete-time Fourier transform evaluated on a frequency grid
    around `expectedHz`, refined by parabolic interpolation of the magnitude peak.

    The choice of method matters more than it looks. The obvious approach — bandpass
    around the expected pitch, then time the zero crossings — is *biased*: a resonant
    bandpass fed broadband content rings at its own centre frequency, which drags the
    measurement towards the very value the test is trying to verify. A DTFT magnitude
    peak has no such prior. `expectedHz` here only sets where to look, never what to
    find, and the search span is wide enough that a genuinely mistuned loop is
    measured rather than snapped to.

    A decaying sinusoid has a magnitude spectrum that is symmetric about its
    frequency, so the peak is an unbiased estimator of it.

    @param samples      analysis window; should start after the attack transient.
    @param expectedHz   centre of the search.
    @param searchCents  half-width of the search, in cents.
*/
[[nodiscard]] PitchEstimate estimateFundamental (const double* samples,
                                                 std::size_t numSamples,
                                                 double sampleRate,
                                                 double expectedHz,
                                                 double searchCents = 300.0);

/** Convenience overload for a whole vector. */
[[nodiscard]] PitchEstimate estimateFundamental (const std::vector<double>& samples,
                                                 double sampleRate,
                                                 double expectedHz,
                                                 double searchCents = 300.0);

} // namespace aethr::testing
