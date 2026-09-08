#pragma once

#include <algorithm>
#include <array>
#include <cmath>

#include "Core/RealtimeGuards.h"
#include "DSP/PhaseDelay.h"

namespace aethr::dsp
{

/**
    In-loop allpass cascade that makes upper partials inharmonic.

    Stiffness 0 is a true bypass: identity, zero extra phase, so the pitch tests
    and a clean string are unaffected. Non-zero stiffness inserts first-order
    allpasses whose phase delay rises with frequency, which is the classic
    stiff-string / bar / plate behaviour. The fundamental is still compensated
    in the resonator tuner; only the partials walk.
*/
class Dispersion
{
public:
    static constexpr int maximumStages = 8;

    void reset() noexcept
    {
        x1.fill (0.0);
        y1.fill (0.0);
    }

    void setAmount (double newAmount) noexcept
    {
        amount = std::clamp (newAmount, 0.0, 1.0);
        // Coefficient stays well inside (-1, 1). 0.55 at full stiffness is metallic
        // without the pole sitting on the unit circle.
        coefficient = amount * 0.55;
        activeStages = amount <= 1.0e-6 ? 0
                                        : 1 + static_cast<int> (std::round (amount * static_cast<double> (maximumStages - 1)));
        activeStages = std::clamp (activeStages, 0, maximumStages);
    }

    [[nodiscard]] double process (double input) noexcept
    {
        if (activeStages <= 0)
            return input;

        auto x = input;

        for (int stage = 0; stage < activeStages; ++stage)
        {
            const auto index = static_cast<std::size_t> (stage);
            const auto y = coefficient * (x - y1[index]) + x1[index];
            x1[index] = x;
            y1[index] = guards::sanitiseState (y);
            x = y1[index];
        }

        return x;
    }

    /** Extra phase delay at `omega`, in samples. Zero when bypassed. */
    [[nodiscard]] double phaseDelayAt (double omega) const noexcept
    {
        if (activeStages <= 0)
            return 0.0;

        return static_cast<double> (activeStages) * phase::allpassPhaseDelay (coefficient, omega);
    }

    [[nodiscard]] bool isActive() const noexcept { return activeStages > 0; }

private:
    double amount { 0.0 };
    double coefficient { 0.0 };
    int activeStages { 0 };

    std::array<double, static_cast<std::size_t> (maximumStages)> x1 {};
    std::array<double, static_cast<std::size_t> (maximumStages)> y1 {};
};

} // namespace aethr::dsp
