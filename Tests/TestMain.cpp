/**
    Test-suite entry support.

    Catch2's own main() comes from Catch2::Catch2WithMain. This translation unit
    only installs process-wide JUCE initialisation: constructing an
    AudioProcessor brings up ValueTree listeners and timers that expect a
    MessageManager to exist, so one is created for the lifetime of the run.
*/

#include <catch2/catch_session.hpp>
#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>

#include <juce_events/juce_events.h>

namespace
{
    class JuceEnvironment final : public Catch::EventListenerBase
    {
    public:
        using Catch::EventListenerBase::EventListenerBase;

        void testRunStarting (const Catch::TestRunInfo&) override
        {
            initialiser = std::make_unique<juce::ScopedJuceInitialiser_GUI>();
        }

        void testRunEnded (const Catch::TestRunStats&) override
        {
            initialiser.reset();
        }

    private:
        std::unique_ptr<juce::ScopedJuceInitialiser_GUI> initialiser;
    };
} // namespace

CATCH_REGISTER_LISTENER (JuceEnvironment)
