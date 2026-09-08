#include "ParameterLayout.h"

#include "DSP/BodyResonator.h"
#include "DSP/Exciter.h"
#include "DSP/FractionalDelayLine.h"
#include "DSP/FxRack.h"
#include "DSP/LoopFilter.h"
#include "DSP/Modulation.h"
#include "DSP/PhysicalMaterial.h"
#include "Engine/Arpeggiator.h"
#include "Engine/EngineType.h"
#include "ParameterIDs.h"
#include "Core/TempoSync.h"

namespace aethr::params
{

// Choice indices are cast directly to these enums, so a mismatch would silently
// select the wrong mode rather than fail to compile. It fails to compile instead.
static_assert (numExcitationTypes == static_cast<int> (dsp::ExcitationType::numTypes),
               "exciter.type choices are out of step with ExcitationType");
static_assert (numLoopFilterModes == static_cast<int> (dsp::LoopFilterMode::numModes),
               "resonator.loopfilter.mode choices are out of step with LoopFilterMode");
static_assert (numInterpolationModes == static_cast<int> (dsp::InterpolationMode::numModes),
               "resonator.interpolation choices are out of step with InterpolationMode");
static_assert (defaultInterpolationIndex == static_cast<int> (dsp::InterpolationMode::allpass),
               "default interpolation index must name the allpass mode");
static_assert (numMaterials == static_cast<int> (dsp::Material::numTypes),
               "material.type choices are out of step with Material");
static_assert (numBodyPresets == static_cast<int> (dsp::BodyPreset::numPresets),
               "body.preset choices are out of step with BodyPreset");
static_assert (numLfoWaves == static_cast<int> (dsp::LfoWave::numWaves),
               "lfo wave choices are out of step with LfoWave");
static_assert (numModDestinations == static_cast<int> (dsp::ModDestination::numDestinations),
               "mod dest choices are out of step with ModDestination");
static_assert (numFilterTypes == static_cast<int> (dsp::FilterType::numTypes),
               "filter type choices are out of step with FilterType");
static_assert (numSatModes == static_cast<int> (dsp::SaturationMode::numModes),
               "sat mode choices are out of step with SaturationMode");
static_assert (numTimeDivisions == sync::numDivisions,
               "time-division choices are out of step with TempoSync");
static_assert (defaultTimeDivisionIndex == sync::defaultDivisionIndex,
               "default time-division index must name 1/4");
static_assert (defaultDelayDivisionLIndex == sync::defaultDelayDivisionL,
               "default delay L division must name 1/8");
static_assert (defaultDelayDivisionRIndex == sync::defaultDelayDivisionR,
               "default delay R division must name 1/8 dotted");
static_assert (numEngineTypes == ::aethr::engine::numEngineTypes,
               "engine.type choices are out of step with EngineType");
static_assert (defaultEngineIndex == ::aethr::engine::defaultEngineIndex,
               "default engine index must name STRING");
static_assert (numArpModes == static_cast<int> (::aethr::engine::ArpMode::numModes),
               "arp.mode choices are out of step with ArpMode");
static_assert (defaultArpDivisionIndex == 3,
               "default arp division must name 1/8");

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

    std::unique_ptr<juce::AudioParameterFloat> makePercentParameter (const char* identifier,
                                                                    const char* name,
                                                                    float defaultPercent,
                                                                    float minimumPercent = percentMin);
    std::unique_ptr<juce::AudioParameterBool> makeSync (const char* identifier, const char* name);
    std::unique_ptr<juce::AudioParameterChoice> makeDivision (const char* identifier,
                                                             const char* name,
                                                             int defaultIndex = defaultTimeDivisionIndex);

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

    constexpr float decayTimeMinSeconds     = 0.02f;
    constexpr float decayTimeMaxSeconds     = 30.0f;
    constexpr float decayTimeDefaultSeconds = 1.6f;
    constexpr float decayTimeCentreSeconds  = 1.5f;

    constexpr float releaseTimeMinSeconds     = 0.005f;
    constexpr float releaseTimeMaxSeconds     = 10.0f;
    constexpr float releaseTimeDefaultSeconds = 0.25f;
    constexpr float releaseTimeCentreSeconds  = 0.3f;

    constexpr float timeStepSeconds = 0.001f;

    constexpr float burstTimeMinMs     = 0.1f;
    constexpr float burstTimeMaxMs     = 200.0f;
    constexpr float burstTimeDefaultMs = 6.0f;
    constexpr float burstTimeCentreMs  = 10.0f;
    constexpr float burstTimeStepMs    = 0.01f;

    constexpr float bipolarPercentMin = -100.0f;

    constexpr int seedMin     = 0;
    constexpr int seedMax     = 9999;
    constexpr int seedDefault = 1;

    /**
        Range whose normalised midpoint lands on `centre`.

        Time and level controls need this: a linear 0.02–30 s decay fader spends its
        first millimetre covering everything musically useful.
    */
    juce::NormalisableRange<float> skewedRange (float minimum, float maximum, float interval, float centre)
    {
        juce::NormalisableRange<float> range { minimum, maximum, interval };
        range.setSkewForCentre (centre);

        return range;
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

    juce::StringArray engineChoices()
    {
        juce::StringArray choices;

        for (const auto& info : ::aethr::engine::engineCatalogue)
            choices.add (info.name);

        return choices;
    }

    std::unique_ptr<juce::AudioProcessorParameterGroup> makeEngineGroup()
    {
        // Topology change, same rationale as polyphony: hosts must not sweep it.
        auto type = std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { engine::type, versionHint },
            "Engine",
            engineChoices(),
            defaultEngineIndex,
            juce::AudioParameterChoiceAttributes().withAutomatable (false));

        return std::make_unique<juce::AudioProcessorParameterGroup> (
            "engine", "Engine", "|",
            std::move (type),
            makePercentParameter (engine::controlA, "Engine A", 50.0f),
            makePercentParameter (engine::controlB, "Engine B", 50.0f),
            makePercentParameter (engine::controlC, "Engine C", 50.0f),
            makePercentParameter (engine::controlD, "Engine D", 50.0f));
    }

    std::unique_ptr<juce::AudioProcessorParameterGroup> makeArpGroup()
    {
        auto mode = std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { arp::mode, versionHint }, "Arp Mode",
            juce::StringArray { "Up", "Down", "Up-Down", "Played", "Random", "Chord" }, 0);

        auto octaves = std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID { arp::octaves, versionHint }, "Arp Octaves",
            1, 4, 1);

        auto gate = std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { arp::gate, versionHint }, "Arp Gate",
            juce::NormalisableRange<float> { 5.0f, 100.0f, 0.1f }, 55.0f,
            juce::AudioParameterFloatAttributes().withLabel ("%"));

        auto swing = std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { arp::swing, versionHint }, "Arp Swing",
            juce::NormalisableRange<float> { 0.0f, 75.0f, 0.1f }, 0.0f,
            juce::AudioParameterFloatAttributes().withLabel ("%"));

        auto pattern = std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID { arp::pattern, versionHint }, "Arp Pattern",
            0, 65535, 65535);

        return std::make_unique<juce::AudioProcessorParameterGroup> (
            "arp", "Arpeggiator", "|",
            makeSync (arp::enable, "Arp"),
            std::move (mode),
            makeDivision (arp::division, "Arp Div", defaultArpDivisionIndex),
            std::move (octaves),
            std::move (gate),
            std::move (swing),
            makeSync (arp::latch, "Arp Latch"),
            std::move (pattern));
    }

    /** Percentage parameter, which most of the engine's controls are. */
    std::unique_ptr<juce::AudioParameterFloat> makePercentParameter (const char* identifier,
                                                                    const char* name,
                                                                    float defaultPercent,
                                                                    float minimumPercent)
    {
        return std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { identifier, versionHint },
            name,
            juce::NormalisableRange<float> { minimumPercent, percentMax, percentStep },
            defaultPercent,
            juce::AudioParameterFloatAttributes()
                .withLabel ("%")
                .withStringFromValueFunction ([minimumPercent] (float value, int)
                                              {
                                                  return minimumPercent < 0.0f ? signedWithUnit (value, 0, "%")
                                                                               : withUnit (value, 0, "%");
                                              }));
    }

    /** Formats a duration in seconds as ms below one second, which is where plucks live. */
    juce::String formatSeconds (float seconds)
    {
        return seconds < 1.0f ? withUnit (seconds * 1000.0f, 0, "ms")
                              : withUnit (seconds, 2, "s");
    }

    std::unique_ptr<juce::AudioProcessorParameterGroup> makeResonatorGroup()
    {
        auto decay = std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { resonator::decayTime, versionHint },
            "Decay",
            skewedRange (decayTimeMinSeconds, decayTimeMaxSeconds, timeStepSeconds, decayTimeCentreSeconds),
            decayTimeDefaultSeconds,
            juce::AudioParameterFloatAttributes()
                .withLabel ("s")
                .withStringFromValueFunction ([] (float value, int) { return formatSeconds (value); }));

        auto release = std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { resonator::releaseTime, versionHint },
            "Release",
            skewedRange (releaseTimeMinSeconds, releaseTimeMaxSeconds, timeStepSeconds, releaseTimeCentreSeconds),
            releaseTimeDefaultSeconds,
            juce::AudioParameterFloatAttributes()
                .withLabel ("s")
                .withStringFromValueFunction ([] (float value, int) { return formatSeconds (value); }));

        auto filterMode = std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { resonator::loopFilterMode, versionHint },
            "Loop Filter",
            juce::StringArray { "Classic (1-zero)", "One-pole", "Two-pole" },
            defaultLoopFilterIndex);

        // Interpolation changes the loop's tap layout, so it is exposed but marked
        // non-automatable: sweeping it from an automation lane is a category error,
        // not a sound design move.
        auto interpolation = std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { resonator::interpolation, versionHint },
            "Interpolation",
            juce::StringArray { "Linear", "Lagrange 3", "Allpass" },
            defaultInterpolationIndex,
            juce::AudioParameterChoiceAttributes().withAutomatable (false));

        return std::make_unique<juce::AudioProcessorParameterGroup> (
            "resonator", "Resonator", "|",
            std::move (decay),
            std::move (release),
            makePercentParameter (resonator::damping, "Damping", 35.0f),
            makePercentParameter (resonator::brightness, "Brightness", 60.0f),
            makePercentParameter (resonator::stiffness, "Stiffness", 0.0f),
            makePercentParameter (resonator::feedback, "Feedback", 100.0f),
            std::move (filterMode),
            std::move (interpolation));
    }

    std::unique_ptr<juce::AudioProcessorParameterGroup> makeExciterGroup()
    {
        auto type = std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { exciter::type, versionHint },
            "Exciter",
            juce::StringArray { "Pluck", "Noise", "Pink", "Mallet", "Click", "Metallic", "Bow", "Blow" },
            defaultExcitationIndex);

        auto burst = std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { exciter::burstTime, versionHint },
            "Burst",
            skewedRange (burstTimeMinMs, burstTimeMaxMs, burstTimeStepMs, burstTimeCentreMs),
            burstTimeDefaultMs,
            juce::AudioParameterFloatAttributes()
                .withLabel ("ms")
                .withStringFromValueFunction ([] (float value, int)
                                              {
                                                  return withUnit (value, value < 10.0f ? 2 : 1, "ms");
                                              }));

        auto seedLocked = std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { exciter::seedLocked, versionHint },
            "Lock Seed",
            false,
            juce::AudioParameterBoolAttributes().withAutomatable (false));

        auto seed = std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID { exciter::seed, versionHint },
            "Seed",
            seedMin, seedMax, seedDefault,
            juce::AudioParameterIntAttributes().withAutomatable (false));

        return std::make_unique<juce::AudioProcessorParameterGroup> (
            "exciter", "Exciter", "|",
            std::move (type),
            std::move (burst),
            makePercentParameter (exciter::attack, "Attack", 15.0f),
            makePercentParameter (exciter::colour, "Colour", 0.0f, bipolarPercentMin),
            makePercentParameter (exciter::brightness, "Exciter Brightness", 70.0f),
            makePercentParameter (exciter::level, "Exciter Level", 80.0f),
            makePercentParameter (exciter::randomAmount, "Randomise", 15.0f),
            makePercentParameter (exciter::stereoSpread, "Stereo Spread", 35.0f),
            std::move (seedLocked),
            std::move (seed));
    }

    juce::StringArray modDestinations()
    {
        return { "Off", "Pitch", "Decay", "Damping", "Brightness", "Exciter",
                 "Stiffness", "Filter", "Delay Mix", "Reverb Mix", "Layer Mix",
                 "Drive", "Width" };
    }

    juce::StringArray lfoWaves()
    {
        return { "Sine", "Triangle", "Saw", "Rev Saw", "Square", "S&H", "Smooth" };
    }

    std::unique_ptr<juce::AudioProcessorParameterGroup> makeMaterialGroup()
    {
        auto type = std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { material::type, versionHint },
            "Material",
            juce::StringArray { "String", "Nylon", "Steel", "Wood", "Glass", "Metal",
                                "Ceramic", "Rubber", "Crystal", "Hollow", "Synthetic", "Alien" },
            0);

        return std::make_unique<juce::AudioProcessorParameterGroup> ("material", "Material", "|",
                                                                     std::move (type));
    }

    std::unique_ptr<juce::AudioProcessorParameterGroup> makeBodyGroup()
    {
        auto modes = std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { body::modes, versionHint },
            "Body Modes",
            juce::StringArray { "1", "2", "4", "8", "16" },
            2);

        auto preset = std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { body::preset, versionHint },
            "Body",
            juce::StringArray { "Wood", "Metal", "Glass", "Hollow", "Bell", "Drum", "Crystal" },
            0);

        return std::make_unique<juce::AudioProcessorParameterGroup> (
            "body", "Body", "|",
            makePercentParameter (body::mix, "Body Mix", 0.0f),
            makePercentParameter (body::decay, "Body Decay", 40.0f),
            makePercentParameter (body::brightness, "Body Brightness", 55.0f),
            std::move (modes),
            std::move (preset));
    }

    std::unique_ptr<juce::AudioProcessorParameterGroup> makeLayerGroup()
    {
        auto enable = std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { layer::bEnable, versionHint },
            "Layer B",
            false);

        auto interval = std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID { layer::bInterval, versionHint },
            "Interval",
            -24, 24, 12,
            juce::AudioParameterIntAttributes().withLabel ("st"));

        auto detune = std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { layer::bDetune, versionHint },
            "Detune",
            juce::NormalisableRange<float> { -50.0f, 50.0f, 0.1f },
            6.0f,
            juce::AudioParameterFloatAttributes().withLabel ("ct"));

        return std::make_unique<juce::AudioProcessorParameterGroup> (
            "layer", "Layer B", "|",
            std::move (enable),
            std::move (interval),
            std::move (detune),
            makePercentParameter (layer::bLevel, "Layer B Level", 0.0f));
    }

    juce::StringArray timeDivisionChoices()
    {
        juce::StringArray choices;

        for (int i = 0; i < numTimeDivisions; ++i)
            choices.add (sync::labels[i]);

        return choices;
    }

    std::unique_ptr<juce::AudioParameterBool> makeSync (const char* identifier, const char* name)
    {
        return std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { identifier, versionHint }, name, false);
    }

    std::unique_ptr<juce::AudioParameterChoice> makeDivision (const char* identifier,
                                                             const char* name,
                                                             int defaultIndex)
    {
        return std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { identifier, versionHint }, name, timeDivisionChoices(), defaultIndex);
    }

    std::unique_ptr<juce::AudioParameterChoice> makeDest (const char* id, const char* name)
    {
        return std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { id, versionHint }, name, modDestinations(), 0);
    }

    std::unique_ptr<juce::AudioProcessorParameterGroup> makeModGroup()
    {
        auto lfo1Wave = std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { lfo1::wave, versionHint }, "LFO 1 Wave", lfoWaves(), 0);
        auto lfo2Wave = std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { lfo2::wave, versionHint }, "LFO 2 Wave", lfoWaves(), 1);

        auto lfo1Rate = std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { lfo1::rate, versionHint }, "LFO 1 Rate",
            skewedRange (0.05f, 20.0f, 0.01f, 1.0f), 0.8f,
            juce::AudioParameterFloatAttributes().withLabel ("Hz"));
        auto lfo2Rate = std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { lfo2::rate, versionHint }, "LFO 2 Rate",
            skewedRange (0.05f, 20.0f, 0.01f, 1.0f), 0.25f,
            juce::AudioParameterFloatAttributes().withLabel ("Hz"));

        auto envA = std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { env::attack, versionHint }, "Env Attack",
            skewedRange (0.001f, 4.0f, 0.001f, 0.05f), 0.01f,
            juce::AudioParameterFloatAttributes().withLabel ("s"));
        auto envD = std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { env::decay, versionHint }, "Env Decay",
            skewedRange (0.01f, 8.0f, 0.001f, 0.2f), 0.2f,
            juce::AudioParameterFloatAttributes().withLabel ("s"));
        auto envR = std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { env::release, versionHint }, "Env Release",
            skewedRange (0.01f, 8.0f, 0.001f, 0.3f), 0.25f,
            juce::AudioParameterFloatAttributes().withLabel ("s"));

        auto chaosRate = std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { chaos::rate, versionHint }, "Chaos Rate",
            skewedRange (0.05f, 12.0f, 0.01f, 0.4f), 0.35f,
            juce::AudioParameterFloatAttributes().withLabel ("Hz"));

        return std::make_unique<juce::AudioProcessorParameterGroup> (
            "mod", "Modulation", "|",
            std::move (lfo1Wave), std::move (lfo1Rate),
            makeSync (lfo1::sync, "LFO 1 Sync"),
            makeDivision (lfo1::division, "LFO 1 Div"),
            makePercentParameter (lfo1::depth, "LFO 1 Depth", 0.0f),
            makeDest (lfo1::dest, "LFO 1 Dest"),
            std::move (lfo2Wave), std::move (lfo2Rate),
            makeSync (lfo2::sync, "LFO 2 Sync"),
            makeDivision (lfo2::division, "LFO 2 Div"),
            makePercentParameter (lfo2::depth, "LFO 2 Depth", 0.0f),
            makeDest (lfo2::dest, "LFO 2 Dest"),
            std::move (envA), std::move (envD),
            makePercentParameter (env::sustain, "Env Sustain", 70.0f),
            std::move (envR),
            makePercentParameter (env::depth, "Env Depth", 0.0f),
            makeDest (env::dest, "Env Dest"),
            makePercentParameter (chaos::amount, "Chaos", 0.0f),
            std::move (chaosRate),
            makeSync (chaos::sync, "Chaos Sync"),
            makeDivision (chaos::division, "Chaos Div"),
            makePercentParameter (chaos::bias, "Chaos Bias", 0.0f, -100.0f));
    }

    std::unique_ptr<juce::AudioProcessorParameterGroup> makeMacroGroup()
    {
        return std::make_unique<juce::AudioProcessorParameterGroup> (
            "macro", "Macros", "|",
            makePercentParameter (macros::material, "Material Macro", 0.0f),
            makePercentParameter (macros::attack, "Attack Macro", 0.0f),
            makePercentParameter (macros::decay, "Decay Macro", 0.0f),
            makePercentParameter (macros::brightness, "Bright Macro", 0.0f),
            makePercentParameter (macros::body, "Body Macro", 0.0f),
            makePercentParameter (macros::chaos, "Chaos Macro", 0.0f),
            makePercentParameter (macros::space, "Space Macro", 0.0f),
            makePercentParameter (macros::drive, "Drive Macro", 0.0f));
    }

    std::unique_ptr<juce::AudioProcessorParameterGroup> makeFxGroup()
    {
        auto filterType = std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { fx::filterType, versionHint }, "Filter",
            juce::StringArray { "Lowpass", "Highpass", "Bandpass", "Notch", "Comb", "Morph" }, 0);

        auto cutoff = std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { fx::filterCutoff, versionHint }, "Cutoff",
            skewedRange (40.0f, 18000.0f, 1.0f, 2000.0f), 8000.0f,
            juce::AudioParameterFloatAttributes().withLabel ("Hz"));

        auto satMode = std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { fx::satMode, versionHint }, "Saturation",
            juce::StringArray { "Clean", "Soft", "Tube", "Tape", "Fold", "Asym", "Hard", "Diode" }, 1);

        auto delayL = std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { fx::delayTimeL, versionHint }, "Delay L",
            skewedRange (0.01f, 1.5f, 0.001f, 0.25f), 0.28f,
            juce::AudioParameterFloatAttributes().withLabel ("s"));
        auto delayR = std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { fx::delayTimeR, versionHint }, "Delay R",
            skewedRange (0.01f, 1.5f, 0.001f, 0.25f), 0.36f,
            juce::AudioParameterFloatAttributes().withLabel ("s"));

        auto chorusRate = std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { fx::chorusRate, versionHint }, "Chorus Rate",
            skewedRange (0.05f, 8.0f, 0.01f, 0.8f), 0.8f,
            juce::AudioParameterFloatAttributes().withLabel ("Hz"));
        auto phaserRate = std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { fx::phaserRate, versionHint }, "Phaser Rate",
            skewedRange (0.05f, 8.0f, 0.01f, 0.3f), 0.3f,
            juce::AudioParameterFloatAttributes().withLabel ("Hz"));

        auto ceiling = std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { output::ceiling, versionHint }, "Ceiling",
            juce::NormalisableRange<float> { 0.2f, 1.0f, 0.01f }, 0.98f);

        return std::make_unique<juce::AudioProcessorParameterGroup> (
            "fx", "Effects", "|",
            std::move (filterType), std::move (cutoff),
            makePercentParameter (fx::filterResonance, "Resonance", 15.0f),
            makePercentParameter (fx::filterMix, "Filter Mix", 0.0f),
            std::move (satMode),
            makePercentParameter (fx::satDrive, "Drive", 0.0f),
            makePercentParameter (fx::satTone, "Sat Tone", 50.0f),
            makePercentParameter (fx::satMix, "Sat Mix", 0.0f),
            std::move (delayL), std::move (delayR),
            makeSync (fx::delaySync, "Delay Sync"),
            makeDivision (fx::delayDivisionL, "Delay Div L", defaultDelayDivisionLIndex),
            makeDivision (fx::delayDivisionR, "Delay Div R", defaultDelayDivisionRIndex),
            makePercentParameter (fx::delayFeedback, "Delay Feedback", 35.0f),
            makePercentParameter (fx::delayMix, "Delay Mix", 0.0f),
            makePercentParameter (fx::delayDamp, "Delay Damp", 25.0f),
            std::move (chorusRate),
            makeSync (fx::chorusSync, "Chorus Sync"),
            makeDivision (fx::chorusDivision, "Chorus Div"),
            makePercentParameter (fx::chorusDepth, "Chorus Depth", 35.0f),
            makePercentParameter (fx::chorusMix, "Chorus Mix", 0.0f),
            std::move (phaserRate),
            makeSync (fx::phaserSync, "Phaser Sync"),
            makeDivision (fx::phaserDivision, "Phaser Div"),
            makePercentParameter (fx::phaserDepth, "Phaser Depth", 50.0f),
            makePercentParameter (fx::phaserFeedback, "Phaser Feedback", 25.0f),
            makePercentParameter (fx::phaserMix, "Phaser Mix", 0.0f),
            makePercentParameter (fx::reverbSize, "Reverb Size", 55.0f),
            makePercentParameter (fx::reverbDecay, "Reverb Decay", 45.0f),
            makePercentParameter (fx::reverbDamp, "Reverb Damp", 40.0f),
            makePercentParameter (fx::reverbWidth, "Reverb Width", 80.0f),
            makePercentParameter (fx::reverbMix, "Reverb Mix", 0.0f),
            std::move (ceiling));
    }
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (makeExciterGroup());
    layout.add (makeResonatorGroup());
    layout.add (makeMaterialGroup());
    layout.add (makeBodyGroup());
    layout.add (makeLayerGroup());
    layout.add (makeModGroup());
    layout.add (makeMacroGroup());
    layout.add (makeFxGroup());
    layout.add (makeMasterTuningGroup());
    layout.add (makeVoiceGroup());
    layout.add (makeEngineGroup());
    layout.add (makeArpGroup());
    layout.add (makeOutputGroup());

    return layout;
}

void applyMaterialToState (juce::AudioProcessorValueTreeState& state, int materialIndex)
{
    const auto material = static_cast<dsp::Material> (
        std::clamp (materialIndex, 0, numMaterials - 1));
    const auto profile = dsp::profileFor (material);

    const auto setFloat = [&state] (const char* id, float value)
    {
        if (auto* parameter = state.getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    };

    setFloat (resonator::damping, profile.dampingPercent);
    setFloat (resonator::brightness, profile.brightnessPercent);
    setFloat (resonator::decayTime, profile.decaySeconds);
    setFloat (resonator::releaseTime, profile.releaseSeconds);
    setFloat (resonator::stiffness, profile.stiffnessPercent);
    setFloat (body::mix, profile.bodyMixPercent);
    setFloat (body::decay, profile.bodyDecayPercent);
    setFloat (body::preset, static_cast<float> (profile.bodyPresetIndex));
    setFloat (body::modes, static_cast<float> (profile.bodyModesIndex));
    setFloat (resonator::loopFilterMode, static_cast<float> (profile.loopFilterIndex));
    setFloat (exciter::type, static_cast<float> (profile.exciterTypeIndex));
}

} // namespace aethr::params
