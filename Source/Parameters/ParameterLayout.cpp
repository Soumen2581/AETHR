#include "ParameterLayout.h"

#include "ParameterIDs.h"

namespace strata::params
{

namespace
{
    /** Skew factor giving a gain fader roughly linear-in-dB feel around unity. */
    constexpr float outputGainDefaultDb = 0.0f;
    constexpr float outputGainMinDb     = -60.0f;
    constexpr float outputGainMaxDb     = 12.0f;
    constexpr float outputGainStepDb    = 0.1f;

    constexpr int   tuneOctaveRange    = 2;
    constexpr int   tuneSemitoneRange  = 12;
    constexpr float tuneCentsRange     = 100.0f;
    constexpr float tuneCentsStep      = 0.1f;

    constexpr float percentMin  = 0.0f;
    constexpr float percentMax  = 100.0f;
    constexpr float percentStep = 0.1f;

    /** Formats a value with a fixed number of decimals and a trailing unit. */
    juce::String withUnit (float value, int decimals, const char* unit)
    {
        return juce::String (value, decimals) + " " + unit;
    }

    /** Formats a signed value, always showing the sign, with a trailing unit. */
    juce::String signedWithUnit (float value, int decimals, const char* unit)
    {
        const auto text = juce::String (value, decimals);
        return (value > 0.0f ? "+" + text : text) + " " + unit;
    }

    juce::StringArray polyphonyChoices()
    {
        juce::StringArray choices;

        for (const auto voices : polyphonyOptions)
            choices.add (juce::String (voices));

        return choices;
    }

    std::unique_ptr<juce::AudioProcessorParameterGroup> makeOutputGroup()
    {
        auto gainParam = std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { output::gain, versionHint },
            "Output Level",
            juce::NormalisableRange<float> { outputGainMinDb, outputGainMaxDb, outputGainStepDb },
            outputGainDefaultDb,
            juce::AudioParameterFloatAttributes()
                .withLabel ("dB")
                .withStringFromValueFunction ([] (float value, int)
                                              {
                                                  return value <= outputGainMinDb ? juce::String ("-inf dB")
                                                                                  : signedWithUnit (value, 1, "dB");
                                              }));

        return std::make_unique<juce::AudioProcessorParameterGroup> ("output", "Output", "|",
                                                                     std::move (gainParam));
    }

    std::unique_ptr<juce::AudioProcessorParameterGroup> makeMasterTuningGroup()
    {
        auto octave = std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID { master::tuneOctave, versionHint },
            "Octave",
            -tuneOctaveRange, tuneOctaveRange, 0,
            juce::AudioParameterIntAttributes()
                .withStringFromValueFunction ([] (int value, int)
                                              {
                                                  return value > 0 ? "+" + juce::String (value) : juce::String (value);
                                              }));

        auto semitones = std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID { master::tuneSemitones, versionHint },
            "Semitones",
            -tuneSemitoneRange, tuneSemitoneRange, 0,
            juce::AudioParameterIntAttributes()
                .withLabel ("st")
                .withStringFromValueFunction ([] (int value, int)
                                              {
                                                  return (value > 0 ? "+" + juce::String (value) : juce::String (value)) + " st";
                                              }));

        auto cents = std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { master::tuneCents, versionHint },
            "Fine Tune",
            juce::NormalisableRange<float> { -tuneCentsRange, tuneCentsRange, tuneCentsStep },
            0.0f,
            juce::AudioParameterFloatAttributes()
                .withLabel ("ct")
                .withStringFromValueFunction ([] (float value, int) { return signedWithUnit (value, 1, "ct"); }));

        return std::make_unique<juce::AudioProcessorParameterGroup> ("master", "Master Tuning", "|",
                                                                     std::move (octave),
                                                                     std::move (semitones),
                                                                     std::move (cents));
    }

    std::unique_ptr<juce::AudioProcessorParameterGroup> makeVoiceGroup()
    {
        // Polyphony resizes the voice pool, which is a message-thread operation.
        // Marking it non-automatable keeps hosts from sweeping it from automation
        // lanes, while it still saves and restores with the session.
        auto polyphonyParam = std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { voice::polyphony, versionHint },
            "Polyphony",
            polyphonyChoices(),
            defaultPolyphonyIndex,
            juce::AudioParameterChoiceAttributes()
                .withLabel ("voices")
                .withAutomatable (false));

        auto velocityParam = std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { voice::velocityAmount, versionHint },
            "Velocity Amount",
            juce::NormalisableRange<float> { percentMin, percentMax, percentStep },
            75.0f,
            juce::AudioParameterFloatAttributes()
                .withLabel ("%")
                .withStringFromValueFunction ([] (float value, int) { return withUnit (value, 0, "%"); }));

        return std::make_unique<juce::AudioProcessorParameterGroup> ("voice", "Voice", "|",
                                                                     std::move (polyphonyParam),
                                                                     std::move (velocityParam));
    }
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (makeMasterTuningGroup());
    layout.add (makeVoiceGroup());
    layout.add (makeOutputGroup());

    return layout;
}

} // namespace strata::params
