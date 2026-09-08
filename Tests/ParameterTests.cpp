#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <set>

#include "Parameters/ParameterIDs.h"
#include "PluginProcessor.h"
#include "Presets/PresetManager.h"

using Catch::Approx;

namespace
{
    /** Every parameter the processor exposes, as RangedAudioParameter. */
    std::vector<juce::RangedAudioParameter*> rangedParameters (juce::AudioProcessor& processor)
    {
        std::vector<juce::RangedAudioParameter*> result;

        for (auto* parameter : processor.getParameters())
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter))
                result.push_back (ranged);

        return result;
    }
} // namespace

TEST_CASE ("Every parameter is ranged, uniquely identified and consistently named", "[parameters]")
{
    aethr::AethrProcessor processor;

    const auto& all = processor.getParameters();
    REQUIRE_FALSE (all.isEmpty());

    const auto ranged = rangedParameters (processor);

    // A non-ranged parameter would not round-trip through APVTS state.
    REQUIRE (ranged.size() == static_cast<std::size_t> (all.size()));

    std::set<juce::String> seenIds;

    for (auto* parameter : ranged)
    {
        const auto id = parameter->getParameterID();

        REQUIRE_FALSE (id.isEmpty());
        REQUIRE (seenIds.insert (id).second);              // IDs must be unique
        REQUIRE (id == id.toLowerCase());                  // ID convention: lowercase
        REQUIRE (id.containsChar ('.'));                   // ID convention: namespaced
        REQUIRE_FALSE (parameter->getName (64).isEmpty()); // hosts need a display name
    }
}

TEST_CASE ("Parameter defaults lie inside their declared ranges", "[parameters]")
{
    aethr::AethrProcessor processor;

    for (auto* parameter : rangedParameters (processor))
    {
        const auto defaultNormalised = parameter->getDefaultValue();

        REQUIRE (defaultNormalised >= 0.0f);
        REQUIRE (defaultNormalised <= 1.0f);

        const auto& range = parameter->getNormalisableRange();
        const auto defaultValue = range.convertFrom0to1 (defaultNormalised);

        REQUIRE (defaultValue >= range.start);
        REQUIRE (defaultValue <= range.end);

        // The normalised/denormalised conversion must be a true inverse pair, or
        // host automation and our own UI will disagree about a parameter's value.
        REQUIRE (range.convertTo0to1 (defaultValue) == Approx (defaultNormalised).margin (1.0e-5));
    }
}

TEST_CASE ("Known parameters exist under their published IDs", "[parameters]")
{
    aethr::AethrProcessor processor;
    auto& apvts = processor.getValueTreeState();

    const char* const expected[]
    {
        aethr::params::output::gain,
        aethr::params::master::tuneOctave,
        aethr::params::master::tuneSemitones,
        aethr::params::master::tuneCents,
        aethr::params::voice::polyphony,
        aethr::params::voice::velocityAmount,
        aethr::params::resonator::feedback,
        aethr::params::lfo1::sync,
        aethr::params::lfo1::division,
        aethr::params::fx::delaySync,
        aethr::params::fx::chorusSync,
        aethr::params::fx::phaserSync,
        aethr::params::engine::type,
        aethr::params::engine::controlA,
        aethr::params::engine::controlB,
        aethr::params::engine::controlC,
        aethr::params::engine::controlD,
        aethr::params::arp::enable,
        aethr::params::arp::mode,
        aethr::params::arp::division,
        aethr::params::arp::octaves,
        aethr::params::arp::gate,
        aethr::params::arp::swing,
        aethr::params::arp::latch,
        aethr::params::arp::pattern
    };

    for (const auto* id : expected)
    {
        REQUIRE (apvts.getParameter (id) != nullptr);
        REQUIRE (apvts.getRawParameterValue (id) != nullptr);
    }
}

TEST_CASE ("Polyphony choice maps onto the documented voice counts", "[parameters][voices]")
{
    aethr::AethrProcessor processor;
    auto* polyphony = processor.getValueTreeState().getParameter (aethr::params::voice::polyphony);

    REQUIRE (polyphony != nullptr);

    // Default must be the documented default index.
    REQUIRE (processor.getSelectedPolyphony()
             == aethr::params::polyphonyOptions[aethr::params::defaultPolyphonyIndex]);

    for (int index = 0; index < aethr::params::numPolyphonyOptions; ++index)
    {
        polyphony->setValueNotifyingHost (polyphony->convertTo0to1 (static_cast<float> (index)));
        REQUIRE (processor.getSelectedPolyphony() == aethr::params::polyphonyOptions[index]);
    }
}

TEST_CASE ("Plugin state survives a save and restore into a fresh instance", "[parameters][state]")
{
    // Values chosen to be different from every default so a failure to restore is visible.
    struct Setting { const char* id; float value; };

    const Setting settings[]
    {
        { aethr::params::output::gain,          -12.5f },
        { aethr::params::master::tuneOctave,     -1.0f },
        { aethr::params::master::tuneSemitones,   7.0f },
        { aethr::params::master::tuneCents,     -33.3f },
        { aethr::params::voice::polyphony,        3.0f },
        { aethr::params::voice::velocityAmount,  42.0f }
    };

    juce::MemoryBlock savedState;

    {
        aethr::AethrProcessor source;

        for (const auto& setting : settings)
        {
            auto* parameter = source.getValueTreeState().getParameter (setting.id);
            REQUIRE (parameter != nullptr);
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (setting.value));
        }

        source.getStateInformation (savedState);
        REQUIRE (savedState.getSize() > 0);
    }

    aethr::AethrProcessor destination;
    destination.setStateInformation (savedState.getData(), static_cast<int> (savedState.getSize()));

    for (const auto& setting : settings)
    {
        const auto* raw = destination.getValueTreeState().getRawParameterValue (setting.id);
        REQUIRE (raw != nullptr);
        REQUIRE (raw->load() == Approx (setting.value).margin (0.05));
    }
}

TEST_CASE ("Malformed or foreign state is rejected without changing current values", "[parameters][state]")
{
    aethr::AethrProcessor processor;

    auto* gain = processor.getValueTreeState().getParameter (aethr::params::output::gain);
    REQUIRE (gain != nullptr);
    gain->setValueNotifyingHost (gain->convertTo0to1 (-9.0f));

    const auto valueBefore = processor.getValueTreeState().getRawParameterValue (aethr::params::output::gain)->load();

    SECTION ("empty buffer")
    {
        processor.setStateInformation (nullptr, 0);
    }

    SECTION ("random bytes")
    {
        const std::array<char, 16> garbage { 'n', 'o', 't', ' ', 'x', 'm', 'l', 0, 1, 2, 3, 4, 5, 6, 7, 8 };
        processor.setStateInformation (garbage.data(), static_cast<int> (garbage.size()));
    }

    SECTION ("valid XML belonging to another plugin")
    {
        juce::XmlElement foreign ("SOME_OTHER_PLUGIN");
        foreign.setAttribute ("nonsense", 1);

        juce::MemoryBlock block;
        processor.copyXmlToBinary (foreign, block);
        processor.setStateInformation (block.getData(), static_cast<int> (block.getSize()));
    }

    const auto valueAfter = processor.getValueTreeState().getRawParameterValue (aethr::params::output::gain)->load();
    REQUIRE (valueAfter == Approx (valueBefore));
}

TEST_CASE ("Factory presets start from Init so earlier patches do not leak", "[parameters][presets]")
{
    aethr::AethrProcessor processor;
    auto& state = processor.getValueTreeState();

    aethr::presets::applyFactory (state, 10); // Psychedelic — enables Layer B / chaos / phaser

    REQUIRE (state.getRawParameterValue (aethr::params::layer::bEnable)->load()
             == Catch::Approx (1.0f).margin (0.05f));

    aethr::presets::applyFactory (state, 2); // Nylon — must not keep Layer B / chaos

    REQUIRE (state.getRawParameterValue (aethr::params::layer::bEnable)->load()
             == Catch::Approx (0.0f).margin (0.05f));
    REQUIRE (state.getRawParameterValue (aethr::params::chaos::amount)->load()
             == Catch::Approx (0.0f).margin (0.05f));
    REQUIRE (state.getRawParameterValue (aethr::params::fx::phaserMix)->load()
             == Catch::Approx (0.0f).margin (0.05f));
}
