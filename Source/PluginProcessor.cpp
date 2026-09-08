#include "PluginProcessor.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "Core/AudioMath.h"
#include "Core/Branding.h"
#include "Core/RealtimeGuards.h"
#include "Core/TempoSync.h"
#include "Parameters/ParameterIDs.h"
#include "PluginEditor.h"

namespace aethr
{

namespace
{
    /** Output gain ramp length. Long enough to be inaudible, short enough to feel immediate. */
    constexpr double outputGainRampSeconds = 0.02;

    /** Below this dB value the output parameter means true silence. Matches the parameter's minimum. */
    constexpr float outputGainMinusInfinityDb = -60.0f;

    /**
        Fixed headroom applied to the engine mix.

        Voices sum without normalisation, because normalising by voice count makes a
        held chord change level as notes are released. -6 dB of standing headroom is
        the cheaper trade: a single note peaks well below full scale and a handful of
        notes still fits.
    */
    constexpr double engineOutputTrim = 0.5;

    /** DC blocker corner for the engine mix, in Hz. */
    constexpr double dcBlockerCornerHz = 12.0;

    /** Percentages arrive from the host in 0-100; the engine works in 0-1. */
    [[nodiscard]] double readNormalisedPercent (const std::atomic<float>* parameter, double fallback) noexcept
    {
        if (parameter == nullptr)
            return fallback;

        return static_cast<double> (parameter->load (std::memory_order_relaxed)) * 0.01;
    }

    [[nodiscard]] double readValue (const std::atomic<float>* parameter, double fallback) noexcept
    {
        if (parameter == nullptr)
            return fallback;

        return static_cast<double> (parameter->load (std::memory_order_relaxed));
    }

    /** Reads a choice parameter as an enum index, clamped to the valid range. */
    [[nodiscard]] int readChoiceIndex (const std::atomic<float>* parameter, int numChoices, int fallback) noexcept
    {
        if (parameter == nullptr)
            return fallback;

        const auto raw = static_cast<int> (std::lround (parameter->load (std::memory_order_relaxed)));

        return std::clamp (raw, 0, numChoices - 1);
    }

    [[nodiscard]] bool readFlag (const std::atomic<float>* parameter, bool fallback) noexcept
    {
        if (parameter == nullptr)
            return fallback;

        return parameter->load (std::memory_order_relaxed) >= 0.5f;
    }

    [[nodiscard]] double resolvedRateHz (const std::atomic<float>* syncFlag,
                                         const std::atomic<float>* division,
                                         const std::atomic<float>* freeHz,
                                         double fallbackHz,
                                         double bpm) noexcept
    {
        if (readFlag (syncFlag, false))
            return sync::hertz (readChoiceIndex (division, sync::numDivisions, sync::defaultDivisionIndex), bpm);

        return readValue (freeHz, fallbackHz);
    }

    [[nodiscard]] double resolvedDelaySeconds (const std::atomic<float>* syncFlag,
                                               const std::atomic<float>* division,
                                               const std::atomic<float>* freeSeconds,
                                               double fallbackSeconds,
                                               double bpm,
                                               int defaultDivision) noexcept
    {
        if (readFlag (syncFlag, false))
            return sync::delaySeconds (readChoiceIndex (division, sync::numDivisions, defaultDivision), bpm);

        return readValue (freeSeconds, fallbackSeconds);
    }
} // namespace

//==============================================================================
juce::AudioProcessor::BusesProperties AethrProcessor::makeBusesProperties()
{
    // Instrument: no inputs. Stereo is the default output; mono is accepted so
    // the plugin remains usable on mono instrument tracks.
    return BusesProperties()
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true);
}

//==============================================================================
AethrProcessor::AethrProcessor()
    : juce::AudioProcessor (makeBusesProperties()),
      valueTreeState (*this, &undoManager, params::stateTreeType, params::createParameterLayout())
{
    resolveParameterHandles();
}

AethrProcessor::~AethrProcessor() = default;

void AethrProcessor::resolveParameterHandles()
{
    const auto handleFor = [this] (const char* identifier)
    {
        auto* pointer = valueTreeState.getRawParameterValue (identifier);

        // A null pointer means an ID in ParameterIDs.h and one in ParameterLayout.cpp
        // have drifted apart. That is a programming error, not a runtime condition.
        jassert (pointer != nullptr);

        return pointer;
    };

    handles.outputGainDb    = handleFor (params::output::gain);
    handles.polyphony       = handleFor (params::voice::polyphony);
    handles.engineType      = handleFor (params::engine::type);
    handles.controlA        = handleFor (params::engine::controlA);
    handles.controlB        = handleFor (params::engine::controlB);
    handles.controlC        = handleFor (params::engine::controlC);
    handles.controlD        = handleFor (params::engine::controlD);
    handles.arpEnable       = handleFor (params::arp::enable);
    handles.arpMode         = handleFor (params::arp::mode);
    handles.arpDivision     = handleFor (params::arp::division);
    handles.arpOctaves      = handleFor (params::arp::octaves);
    handles.arpGate         = handleFor (params::arp::gate);
    handles.arpSwing        = handleFor (params::arp::swing);
    handles.arpLatch        = handleFor (params::arp::latch);
    handles.arpPattern      = handleFor (params::arp::pattern);
    handles.velocityAmount  = handleFor (params::voice::velocityAmount);

    handles.tuneOctave      = handleFor (params::master::tuneOctave);
    handles.tuneSemitones   = handleFor (params::master::tuneSemitones);
    handles.tuneCents       = handleFor (params::master::tuneCents);

    handles.decayTime           = handleFor (params::resonator::decayTime);
    handles.releaseTime         = handleFor (params::resonator::releaseTime);
    handles.damping             = handleFor (params::resonator::damping);
    handles.resonatorBrightness = handleFor (params::resonator::brightness);
    handles.loopFilterMode      = handleFor (params::resonator::loopFilterMode);
    handles.interpolation       = handleFor (params::resonator::interpolation);
    handles.loopFeedback        = handleFor (params::resonator::feedback);

    handles.exciterType       = handleFor (params::exciter::type);
    handles.burstTime         = handleFor (params::exciter::burstTime);
    handles.exciterAttack     = handleFor (params::exciter::attack);
    handles.exciterColour     = handleFor (params::exciter::colour);
    handles.exciterBrightness = handleFor (params::exciter::brightness);
    handles.exciterLevel      = handleFor (params::exciter::level);
    handles.randomAmount      = handleFor (params::exciter::randomAmount);
    handles.stereoSpread      = handleFor (params::exciter::stereoSpread);
    handles.seedLocked        = handleFor (params::exciter::seedLocked);
    handles.seed              = handleFor (params::exciter::seed);

    handles.stiffness         = handleFor (params::resonator::stiffness);
    handles.bodyMix           = handleFor (params::body::mix);
    handles.bodyDecay         = handleFor (params::body::decay);
    handles.bodyBrightness    = handleFor (params::body::brightness);
    handles.bodyModes         = handleFor (params::body::modes);
    handles.bodyPreset        = handleFor (params::body::preset);

    handles.layerBEnable      = handleFor (params::layer::bEnable);
    handles.layerBInterval    = handleFor (params::layer::bInterval);
    handles.layerBDetune      = handleFor (params::layer::bDetune);
    handles.layerBLevel       = handleFor (params::layer::bLevel);

    handles.lfo1Wave          = handleFor (params::lfo1::wave);
    handles.lfo1Rate          = handleFor (params::lfo1::rate);
    handles.lfo1Sync          = handleFor (params::lfo1::sync);
    handles.lfo1Division      = handleFor (params::lfo1::division);
    handles.lfo1Depth         = handleFor (params::lfo1::depth);
    handles.lfo1Dest          = handleFor (params::lfo1::dest);
    handles.lfo2Wave          = handleFor (params::lfo2::wave);
    handles.lfo2Rate          = handleFor (params::lfo2::rate);
    handles.lfo2Sync          = handleFor (params::lfo2::sync);
    handles.lfo2Division      = handleFor (params::lfo2::division);
    handles.lfo2Depth         = handleFor (params::lfo2::depth);
    handles.lfo2Dest          = handleFor (params::lfo2::dest);

    handles.envAttack         = handleFor (params::env::attack);
    handles.envDecay          = handleFor (params::env::decay);
    handles.envSustain        = handleFor (params::env::sustain);
    handles.envRelease        = handleFor (params::env::release);
    handles.envDepth          = handleFor (params::env::depth);
    handles.envDest           = handleFor (params::env::dest);

    handles.chaosAmount       = handleFor (params::chaos::amount);
    handles.chaosRate         = handleFor (params::chaos::rate);
    handles.chaosSync         = handleFor (params::chaos::sync);
    handles.chaosDivision     = handleFor (params::chaos::division);
    handles.chaosBias         = handleFor (params::chaos::bias);

    handles.macroMaterial     = handleFor (params::macros::material);
    handles.macroAttack       = handleFor (params::macros::attack);
    handles.macroDecay        = handleFor (params::macros::decay);
    handles.macroBrightness   = handleFor (params::macros::brightness);
    handles.macroBody         = handleFor (params::macros::body);
    handles.macroChaos        = handleFor (params::macros::chaos);
    handles.macroSpace        = handleFor (params::macros::space);
    handles.macroDrive        = handleFor (params::macros::drive);

    handles.filterType        = handleFor (params::fx::filterType);
    handles.filterCutoff      = handleFor (params::fx::filterCutoff);
    handles.filterRes         = handleFor (params::fx::filterResonance);
    handles.filterMix         = handleFor (params::fx::filterMix);
    handles.satMode           = handleFor (params::fx::satMode);
    handles.satDrive          = handleFor (params::fx::satDrive);
    handles.satTone           = handleFor (params::fx::satTone);
    handles.satMix            = handleFor (params::fx::satMix);
    handles.delayTimeL        = handleFor (params::fx::delayTimeL);
    handles.delayTimeR        = handleFor (params::fx::delayTimeR);
    handles.delaySync         = handleFor (params::fx::delaySync);
    handles.delayDivisionL    = handleFor (params::fx::delayDivisionL);
    handles.delayDivisionR    = handleFor (params::fx::delayDivisionR);
    handles.delayFeedback     = handleFor (params::fx::delayFeedback);
    handles.delayMix          = handleFor (params::fx::delayMix);
    handles.delayDamp         = handleFor (params::fx::delayDamp);
    handles.chorusRate        = handleFor (params::fx::chorusRate);
    handles.chorusSync        = handleFor (params::fx::chorusSync);
    handles.chorusDivision    = handleFor (params::fx::chorusDivision);
    handles.chorusDepth       = handleFor (params::fx::chorusDepth);
    handles.chorusMix         = handleFor (params::fx::chorusMix);
    handles.phaserRate        = handleFor (params::fx::phaserRate);
    handles.phaserSync        = handleFor (params::fx::phaserSync);
    handles.phaserDivision    = handleFor (params::fx::phaserDivision);
    handles.phaserDepth       = handleFor (params::fx::phaserDepth);
    handles.phaserFeedback    = handleFor (params::fx::phaserFeedback);
    handles.phaserMix         = handleFor (params::fx::phaserMix);
    handles.reverbSize        = handleFor (params::fx::reverbSize);
    handles.reverbDecay       = handleFor (params::fx::reverbDecay);
    handles.reverbDamp        = handleFor (params::fx::reverbDamp);
    handles.reverbWidth       = handleFor (params::fx::reverbWidth);
    handles.reverbMix         = handleFor (params::fx::reverbMix);
    handles.ceiling           = handleFor (params::output::ceiling);
}

//==============================================================================
engine::Settings AethrProcessor::buildEngineSettings() const
{
    engine::Settings settings;

    settings.engineType = engine::engineTypeFromIndex (
        readChoiceIndex (handles.engineType, engine::numEngineTypes, engine::defaultEngineIndex));

    const auto exciterTypeIndex = readChoiceIndex (handles.exciterType,
                                                   params::numExcitationTypes,
                                                   params::defaultExcitationIndex);

    settings.exciter.type = static_cast<dsp::ExcitationType> (exciterTypeIndex);
    settings.exciter.burstMilliseconds = readValue (handles.burstTime, settings.exciter.burstMilliseconds);
    settings.exciter.attack = readNormalisedPercent (handles.exciterAttack, settings.exciter.attack);
    settings.exciter.colour = readNormalisedPercent (handles.exciterColour, settings.exciter.colour);
    settings.exciter.brightness = readNormalisedPercent (handles.exciterBrightness, settings.exciter.brightness);
    settings.exciter.level = readNormalisedPercent (handles.exciterLevel, settings.exciter.level);
    settings.exciter.randomAmount = readNormalisedPercent (handles.randomAmount, settings.exciter.randomAmount);
    settings.exciter.stereoSpread = readNormalisedPercent (handles.stereoSpread, settings.exciter.stereoSpread);
    settings.exciter.velocityAmount = readNormalisedPercent (handles.velocityAmount, settings.exciter.velocityAmount);

    settings.decayTimeSeconds = readValue (handles.decayTime, settings.decayTimeSeconds);
    settings.releaseTimeSeconds = readValue (handles.releaseTime, settings.releaseTimeSeconds);
    settings.damping = readNormalisedPercent (handles.damping, settings.damping);
    settings.brightness = readNormalisedPercent (handles.resonatorBrightness, settings.brightness);

    settings.loopFilterMode = static_cast<dsp::LoopFilterMode> (
        readChoiceIndex (handles.loopFilterMode, params::numLoopFilterModes, params::defaultLoopFilterIndex));

    settings.interpolation = static_cast<dsp::InterpolationMode> (
        readChoiceIndex (handles.interpolation, params::numInterpolationModes, params::defaultInterpolationIndex));

    settings.stiffness = readNormalisedPercent (handles.stiffness, 0.0);
    settings.loopFeedback = readNormalisedPercent (handles.loopFeedback, 1.0);
    settings.controlA = readNormalisedPercent (handles.controlA, 0.5);
    settings.controlB = readNormalisedPercent (handles.controlB, 0.5);
    settings.controlC = readNormalisedPercent (handles.controlC, 0.5);
    settings.controlD = readNormalisedPercent (handles.controlD, 0.5);

    const auto bodyModeIndex = readChoiceIndex (handles.bodyModes, params::numBodyModeChoices, 2);
    settings.body.mix = readNormalisedPercent (handles.bodyMix, 0.0);
    settings.body.decay = readNormalisedPercent (handles.bodyDecay, 0.4);
    settings.body.brightness = readNormalisedPercent (handles.bodyBrightness, 0.55);
    settings.body.activeModes = params::bodyModeCounts[bodyModeIndex];
    settings.body.preset = static_cast<dsp::BodyPreset> (
        readChoiceIndex (handles.bodyPreset, params::numBodyPresets, 0));

    settings.layerBEnabled = readValue (handles.layerBEnable, 0.0) > 0.5;
    settings.layerBIntervalSemitones = readValue (handles.layerBInterval, 12.0);
    settings.layerBDetuneCents = readValue (handles.layerBDetune, 6.0);
    settings.layerBLevel = readNormalisedPercent (handles.layerBLevel, 0.0);

    const auto octaves = readValue (handles.tuneOctave, 0.0);
    const auto semitones = readValue (handles.tuneSemitones, 0.0);
    const auto cents = readValue (handles.tuneCents, 0.0);

    settings.tuningOffsetSemitones = octaves * math::semitonesPerOctave
                                   + semitones
                                   + cents / math::centsPerSemitone;

    settings.numVoices = getSelectedPolyphony();
    settings.seedLocked = readValue (handles.seedLocked, 0.0) > 0.5;
    settings.seed = static_cast<int> (std::lround (readValue (handles.seed, 1.0)));

    const auto macroAttack = readNormalisedPercent (handles.macroAttack, 0.0);
    const auto macroDecay = readNormalisedPercent (handles.macroDecay, 0.0);
    const auto macroBright = readNormalisedPercent (handles.macroBrightness, 0.0);
    const auto macroBody = readNormalisedPercent (handles.macroBody, 0.0);
    const auto macroMaterial = readNormalisedPercent (handles.macroMaterial, 0.0);

    settings.exciter.burstMilliseconds *= 1.0 + macroAttack * 2.0;
    settings.exciter.attack = std::clamp (settings.exciter.attack + macroAttack * 0.4, 0.0, 1.0);
    settings.decayTimeSeconds *= 1.0 + macroDecay * 3.0;
    settings.brightness = std::clamp (settings.brightness + macroBright * 0.45, 0.0, 1.0);
    settings.body.mix = std::clamp (settings.body.mix + macroBody, 0.0, 1.0);
    settings.stiffness = std::clamp (settings.stiffness + macroMaterial * 0.6, 0.0, 1.0);

    return settings;
}

engine::ArpSettings AethrProcessor::buildArpSettings() const
{
    engine::ArpSettings settings;
    settings.enabled = readFlag (handles.arpEnable, false);
    settings.mode = static_cast<engine::ArpMode> (
        readChoiceIndex (handles.arpMode, params::numArpModes, 0));
    settings.divisionIndex = readChoiceIndex (handles.arpDivision,
                                              params::numTimeDivisions,
                                              params::defaultArpDivisionIndex);
    settings.octaves = std::clamp (static_cast<int> (std::lround (readValue (handles.arpOctaves, 1.0))), 1, 4);
    settings.gate = readNormalisedPercent (handles.arpGate, 0.55);
    settings.swing = readNormalisedPercent (handles.arpSwing, 0.0);
    settings.latch = readFlag (handles.arpLatch, false);
    settings.pattern = static_cast<std::uint16_t> (
        std::clamp (static_cast<int> (std::lround (readValue (handles.arpPattern, 65535.0))), 0, 65535));
    return settings;
}

dsp::FxRack::Settings AethrProcessor::buildFxSettings() const
{
    dsp::FxRack::Settings fx;

    fx.filterType = static_cast<dsp::FilterType> (
        readChoiceIndex (handles.filterType, params::numFilterTypes, 0));
    fx.filterCutoffHz = readValue (handles.filterCutoff, 8000.0);
    fx.filterResonance = readNormalisedPercent (handles.filterRes, 0.15);
    fx.filterMix = readNormalisedPercent (handles.filterMix, 0.0);

    fx.satMode = static_cast<dsp::SaturationMode> (
        readChoiceIndex (handles.satMode, params::numSatModes, 1));
    fx.satDrive = readNormalisedPercent (handles.satDrive, 0.0);
    fx.satTone = readNormalisedPercent (handles.satTone, 0.5);
    fx.satMix = readNormalisedPercent (handles.satMix, 0.0);

    const auto bpm = hostTempoBpm.load (std::memory_order_relaxed);

    fx.delayTimeL = resolvedDelaySeconds (handles.delaySync, handles.delayDivisionL,
                                          handles.delayTimeL, 0.28, bpm, sync::defaultDelayDivisionL);
    fx.delayTimeR = resolvedDelaySeconds (handles.delaySync, handles.delayDivisionR,
                                          handles.delayTimeR, 0.36, bpm, sync::defaultDelayDivisionR);
    fx.delayFeedback = readNormalisedPercent (handles.delayFeedback, 0.35);
    fx.delayMix = readNormalisedPercent (handles.delayMix, 0.0);
    fx.delayDamping = readNormalisedPercent (handles.delayDamp, 0.25);

    fx.chorusRate = resolvedRateHz (handles.chorusSync, handles.chorusDivision,
                                    handles.chorusRate, 0.8, bpm);
    fx.chorusDepth = readNormalisedPercent (handles.chorusDepth, 0.35);
    fx.chorusMix = readNormalisedPercent (handles.chorusMix, 0.0);

    fx.phaserRate = resolvedRateHz (handles.phaserSync, handles.phaserDivision,
                                    handles.phaserRate, 0.3, bpm);
    fx.phaserDepth = readNormalisedPercent (handles.phaserDepth, 0.5);
    fx.phaserFeedback = readNormalisedPercent (handles.phaserFeedback, 0.25);
    fx.phaserMix = readNormalisedPercent (handles.phaserMix, 0.0);

    fx.reverbSize = readNormalisedPercent (handles.reverbSize, 0.55);
    fx.reverbDecay = readNormalisedPercent (handles.reverbDecay, 0.45);
    fx.reverbDamping = readNormalisedPercent (handles.reverbDamp, 0.4);
    fx.reverbWidth = readNormalisedPercent (handles.reverbWidth, 0.8);
    fx.reverbMix = readNormalisedPercent (handles.reverbMix, 0.0);

    fx.ceiling = readValue (handles.ceiling, 0.98);

    const auto space = readNormalisedPercent (handles.macroSpace, 0.0);
    const auto drive = readNormalisedPercent (handles.macroDrive, 0.0);
    fx.delayMix = std::clamp (fx.delayMix + space * 0.4, 0.0, 1.0);
    fx.reverbMix = std::clamp (fx.reverbMix + space * 0.6, 0.0, 1.0);
    fx.satDrive = std::clamp (fx.satDrive + drive, 0.0, 1.0);
    fx.satMix = std::clamp (fx.satMix + drive * 0.5, 0.0, 1.0);

    return fx;
}

void AethrProcessor::applyRealtimeModulation (engine::Settings& settings,
                                              dsp::FxRack::Settings& fx,
                                              int numSamples) noexcept
{
    const auto bpm = hostTempoBpm.load (std::memory_order_relaxed);

    lfo1.set (static_cast<dsp::LfoWave> (readChoiceIndex (handles.lfo1Wave, params::numLfoWaves, 0)),
              resolvedRateHz (handles.lfo1Sync, handles.lfo1Division, handles.lfo1Rate, 0.8, bpm),
              readNormalisedPercent (handles.lfo1Depth, 0.0));
    lfo2.set (static_cast<dsp::LfoWave> (readChoiceIndex (handles.lfo2Wave, params::numLfoWaves, 1)),
              resolvedRateHz (handles.lfo2Sync, handles.lfo2Division, handles.lfo2Rate, 0.25, bpm),
              readNormalisedPercent (handles.lfo2Depth, 0.0));

    auxEnv.set (readValue (handles.envAttack, 0.01),
                readValue (handles.envDecay, 0.2),
                readNormalisedPercent (handles.envSustain, 0.7),
                readValue (handles.envRelease, 0.25),
                readNormalisedPercent (handles.envDepth, 0.0));

    const auto chaosAmount = std::clamp (readNormalisedPercent (handles.chaosAmount, 0.0)
                                         + readNormalisedPercent (handles.macroChaos, 0.0),
                                         0.0, 1.0);
    chaos.set (chaosAmount,
               resolvedRateHz (handles.chaosSync, handles.chaosDivision, handles.chaosRate, 0.35, bpm),
               readNormalisedPercent (handles.chaosBias, 0.0));

    dsp::ModulationOffsets offsets;
    dsp::applyDestination (offsets,
                           static_cast<dsp::ModDestination> (readChoiceIndex (handles.lfo1Dest, params::numModDestinations, 0)),
                           lfo1.processBlock (numSamples));
    dsp::applyDestination (offsets,
                           static_cast<dsp::ModDestination> (readChoiceIndex (handles.lfo2Dest, params::numModDestinations, 0)),
                           lfo2.processBlock (numSamples));
    dsp::applyDestination (offsets,
                           static_cast<dsp::ModDestination> (readChoiceIndex (handles.envDest, params::numModDestinations, 0)),
                           auxEnv.processBlock (numSamples));

    const auto chaosValue = chaos.processBlock (numSamples);
    settings.stiffness = std::clamp (settings.stiffness + chaosValue * 0.25 + offsets.stiffnessOffset, 0.0, 1.0);
    settings.damping = std::clamp (settings.damping + chaosValue * 0.15 + offsets.dampingOffset, 0.0, 1.0);
    settings.brightness = std::clamp (settings.brightness + offsets.brightnessOffset, 0.0, 1.0);
    settings.decayTimeSeconds = std::max (0.02, settings.decayTimeSeconds * offsets.decayScale);
    settings.tuningOffsetSemitones += offsets.pitchSemitones + chaosValue * 0.15;
    settings.exciter.level = std::clamp (settings.exciter.level + offsets.excitationOffset, 0.0, 1.5);
    settings.exciter.stereoSpread = std::clamp (settings.exciter.stereoSpread + offsets.stereoOffset, 0.0, 1.0);
    settings.layerBLevel = std::clamp (settings.layerBLevel + offsets.layerMixOffset, 0.0, 1.0);

    fx.filterCutoffHz = std::clamp (fx.filterCutoffHz * std::exp2 (offsets.filterCutoffOffset * 2.0), 40.0, 18000.0);
    fx.delayMix = std::clamp (fx.delayMix + offsets.delayMixOffset, 0.0, 1.0);
    fx.reverbMix = std::clamp (fx.reverbMix + offsets.reverbMixOffset, 0.0, 1.0);
    fx.satDrive = std::clamp (fx.satDrive + offsets.driveOffset, 0.0, 1.0);
}

const juce::String AethrProcessor::getName() const
{
    return branding::productName;
}

double AethrProcessor::getTailLengthSeconds() const
{
    // Hosts truncate offline renders at this length, so report a conservative upper
    // bound of every stage that can still ring after note-off.
    const auto release = readValue (handles.releaseTime, 0.25);
    const auto decay = readValue (handles.decayTime, 1.6);
    const auto delayMix = readValue (handles.delayMix, 0.0);
    const auto delayTime = std::max (readValue (handles.delayTimeL, 0.0),
                                     readValue (handles.delayTimeR, 0.0));
    const auto delayFeedback = readValue (handles.delayFeedback, 0.0) * 0.01;
    const auto reverbMix = readValue (handles.reverbMix, 0.0);
    const auto reverbDecay = readValue (handles.reverbDecay, 0.0) * 0.01;
    const auto reverbSize = readValue (handles.reverbSize, 0.0) * 0.01;

    auto delayTail = 0.0;
    if (delayMix > 1.0e-3 && delayFeedback > 1.0e-3)
        delayTail = delayTime * (1.0 + 8.0 * delayFeedback);

    auto reverbTail = 0.0;
    if (reverbMix > 1.0e-3)
        reverbTail = 0.4 + 6.0 * reverbDecay * (0.35 + reverbSize);

    return std::max ({ decay, release, delayTail, reverbTail }) + 0.25;
}

int AethrProcessor::getSelectedPolyphony() const noexcept
{
    const auto index = readChoiceIndex (handles.polyphony,
                                        params::numPolyphonyOptions,
                                        params::defaultPolyphonyIndex);

    return params::polyphonyOptions[index];
}

engine::EngineType AethrProcessor::getSelectedEngineType() const noexcept
{
    return engine::engineTypeFromIndex (
        readChoiceIndex (handles.engineType, engine::numEngineTypes, engine::defaultEngineIndex));
}

void AethrProcessor::captureHostTempo() noexcept
{
    auto bpm = hostTempoBpm.load (std::memory_order_relaxed);

    if (auto* head = getPlayHead())
    {
        if (const auto position = head->getPosition())
        {
            if (const auto reported = position->getBpm(); reported.hasValue())
                bpm = sync::clampBpm (*reported);
        }
    }

    hostTempoBpm.store (bpm, std::memory_order_relaxed);
}

//==============================================================================
void AethrProcessor::prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize  = maximumExpectedSamplesPerBlock;

    // Sized with headroom because some hosts occasionally deliver a block larger
    // than the maximum they promised; processBlock() must never reallocate.
    const auto scratchSize = static_cast<std::size_t> (std::max (1, maximumExpectedSamplesPerBlock)) * 2;

    outputGainRamp.assign (scratchSize, 0.0);
    engineLeft.assign (scratchSize, 0.0);
    engineRight.assign (scratchSize, 0.0);

    // Every allocation the engine will ever need happens here: each voice sizes its
    // delay lines for the lowest fundamental it can be asked to play.
    voiceEngine.prepare (sampleRate);
    voiceEngine.reset();
    voiceEngine.setSettings (buildEngineSettings());

    arpeggiator.prepare (sampleRate);
    arpMidi.ensureSize (65536);
    arpWasEnabled = false;
    arpStep.store (0, std::memory_order_relaxed);

    fxRack.prepare (sampleRate, maximumExpectedSamplesPerBlock);
    lfo1.prepare (sampleRate);
    lfo2.prepare (sampleRate);
    auxEnv.prepare (sampleRate);
    chaos.prepare (sampleRate);

    dcBlockerCoefficient = std::exp (-math::twoPi * dcBlockerCornerHz / std::max (1.0, sampleRate));
    dcBlockerLeft.reset();
    dcBlockerRight.reset();

    outputGain.reset (sampleRate, outputGainRampSeconds);

    const auto initialGain = math::decibelsToGain (readValue (handles.outputGainDb, 0.0),
                                                   static_cast<double> (outputGainMinusInfinityDb));
    outputGain.setCurrentAndTargetValue (initialGain);

    outputPeakLevel.store (0.0f, std::memory_order_relaxed);
    midiEventCount.store (0, std::memory_order_relaxed);
    activeVoiceCount.store (0, std::memory_order_relaxed);
    blockSizeOverruns.store (0, std::memory_order_relaxed);
}

void AethrProcessor::releaseResources()
{
    // Nothing is held that must be freed between playback sessions; buffers are
    // sized in prepareToPlay() and reused.
}

void AethrProcessor::reset()
{
    // Hosts call this to drop tails, for example when the transport relocates. It must
    // silence the engine without reallocating anything.
    voiceEngine.reset();
    arpeggiator.reset();
    arpWasEnabled = false;
    arpStep.store (0, std::memory_order_relaxed);
    fxRack.reset();
    lfo1.reset();
    lfo2.reset();
    dcBlockerLeft.reset();
    dcBlockerRight.reset();

    outputPeakLevel.store (0.0f, std::memory_order_relaxed);
    activeVoiceCount.store (0, std::memory_order_relaxed);
    lastNoteHz.store (0.0f, std::memory_order_relaxed);
}

bool AethrProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainInputChannels() != 0)
        return false;

    const auto& output = layouts.getMainOutputChannelSet();

    return output == juce::AudioChannelSet::mono()
        || output == juce::AudioChannelSet::stereo();
}

//==============================================================================
void AethrProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    processBlockInternal (buffer, midiMessages);
}

void AethrProcessor::processBlock (juce::AudioBuffer<double>& buffer, juce::MidiBuffer& midiMessages)
{
    processBlockInternal (buffer, midiMessages);
}

template <typename FloatType>
void AethrProcessor::processBlockInternal (juce::AudioBuffer<FloatType>& buffer, juce::MidiBuffer& midiMessages)
{
    const juce::ScopedNoDenormals noDenormals;

    // Instruments must clear anything the host left in the buffer, including
    // channels beyond the ones the engine writes to.
    buffer.clear();

    renderEngine (buffer, midiMessages);
    applyOutputStage (buffer);
}

void AethrProcessor::handleMidiMessage (const juce::MidiMessage& message) noexcept
{
    if (message.isNoteOn())
    {
        voiceEngine.noteOn (message.getNoteNumber(), static_cast<double> (message.getFloatVelocity()));
        auxEnv.noteOn();
        lastNoteHz.store (static_cast<float> (juce::MidiMessage::getMidiNoteInHertz (message.getNoteNumber())),
                          std::memory_order_relaxed);
    }
    else if (message.isNoteOff())
    {
        voiceEngine.noteOff (message.getNoteNumber());

        if (voiceEngine.getActiveVoiceCount() == 0)
            auxEnv.noteOff();
    }
    else if (message.isAllNotesOff() || message.isAllSoundOff())
    {
        // All-sound-off means stop now; all-notes-off means release and let it ring.
        voiceEngine.allNotesOff (! message.isAllSoundOff());
    }
    else if (message.isPitchWheel())
    {
        constexpr auto pitchWheelCentre = 8192.0;
        constexpr auto pitchWheelRangeSemitones = 2.0;

        const auto normalised = (static_cast<double> (message.getPitchWheelValue()) - pitchWheelCentre) / pitchWheelCentre;
        voiceEngine.setPitchBendSemitones (normalised * pitchWheelRangeSemitones);
    }
    else if (message.isSustainPedalOn())
    {
        voiceEngine.setSustainPedal (true);
    }
    else if (message.isSustainPedalOff())
    {
        voiceEngine.setSustainPedal (false);
    }
}

void AethrProcessor::renderEngineSegment (int segmentStart, int segmentLength, const juce::MidiBuffer& midiMessages)
{
    std::fill_n (engineLeft.data(), static_cast<std::size_t> (segmentLength), 0.0);
    std::fill_n (engineRight.data(), static_cast<std::size_t> (segmentLength), 0.0);

    // Render up to each event, act on it, then continue. This is what makes note
    // timing sample-accurate instead of quantised to the block size, which at 512
    // samples would be 11 ms of jitter on every note.
    auto position = 0;

    for (const auto metadata : midiMessages)
    {
        const auto eventTime = metadata.samplePosition - segmentStart;

        if (eventTime < 0)
            continue;   // already handled in an earlier segment

        if (eventTime >= segmentLength)
            break;      // belongs to a later segment; the buffer is time-ordered

        if (eventTime > position)
        {
            voiceEngine.renderAdding (engineLeft.data() + position,
                                      engineRight.data() + position,
                                      eventTime - position);
            position = eventTime;
        }

        handleMidiMessage (metadata.getMessage());
    }

    if (position < segmentLength)
        voiceEngine.renderAdding (engineLeft.data() + position,
                                  engineRight.data() + position,
                                  segmentLength - position);
}

template <typename FloatType>
void AethrProcessor::mixSegmentInto (juce::AudioBuffer<FloatType>& buffer, int segmentStart, int segmentLength)
{
    const auto numChannels = buffer.getNumChannels();

    auto* left = buffer.getWritePointer (0) + segmentStart;
    auto* right = numChannels > 1 ? buffer.getWritePointer (1) + segmentStart : nullptr;

    for (int i = 0; i < segmentLength; ++i)
    {
        const auto index = static_cast<std::size_t> (i);

        const auto leftSample = dcBlockerLeft.process (engineLeft[index], dcBlockerCoefficient) * engineOutputTrim;
        const auto rightSample = dcBlockerRight.process (engineRight[index], dcBlockerCoefficient) * engineOutputTrim;

        if (right != nullptr)
        {
            left[i] += static_cast<FloatType> (leftSample);
            right[i] += static_cast<FloatType> (rightSample);
        }
        else
        {
            // Mono host: fold down rather than dropping a channel, so the decorrelated
            // stereo excitation still contributes all of its energy.
            left[i] += static_cast<FloatType> (0.5 * (leftSample + rightSample));
        }
    }
}

template <typename FloatType>
void AethrProcessor::renderEngine (juce::AudioBuffer<FloatType>& buffer, juce::MidiBuffer& midiMessages)
{
    const auto numSamples = buffer.getNumSamples();

    if (numSamples <= 0 || buffer.getNumChannels() <= 0)
        return;

    const auto seen = midiEventCount.load (std::memory_order_relaxed);
    midiEventCount.store (seen + midiMessages.getNumEvents(), std::memory_order_relaxed);

    captureHostTempo();

    // One snapshot per block. Reading a parameter again further down would risk
    // deriving a loop gain from one decay time and a delay length from another.
    auto engineSettings = buildEngineSettings();
    auto fxSettings = buildFxSettings();
    applyRealtimeModulation (engineSettings, fxSettings, numSamples);
    voiceEngine.setSettings (engineSettings);
    fxRack.setSettings (fxSettings);

    const juce::MidiBuffer* midi = &midiMessages;
    const auto arpSettings = buildArpSettings();

    if (arpSettings.enabled)
    {
        arpeggiator.setSettings (arpSettings);
        arpeggiator.process (midiMessages, arpMidi, numSamples,
                             hostTempoBpm.load (std::memory_order_relaxed));
        midi = &arpMidi;
        arpWasEnabled = true;
        arpStep.store (arpeggiator.currentStep(), std::memory_order_relaxed);
    }
    else if (arpWasEnabled)
    {
        arpMidi.clear();
        arpeggiator.collectOffEvents (arpMidi);
        arpMidi.addEvents (midiMessages, 0, numSamples, 0);
        arpeggiator.reset();
        midi = &arpMidi;
        arpWasEnabled = false;
        arpStep.store (0, std::memory_order_relaxed);
    }

    const auto capacity = static_cast<int> (engineLeft.size());

    if (capacity <= 0)
        return;

    // Segmented so that a host overshooting its promised block size still renders
    // correctly, rather than being clamped or triggering an allocation.
    for (auto offset = 0; offset < numSamples;)
    {
        const auto segmentLength = std::min (capacity, numSamples - offset);

        renderEngineSegment (offset, segmentLength, *midi);
        fxRack.process (engineLeft.data(), engineRight.data(), segmentLength);
        mixSegmentInto (buffer, offset, segmentLength);

        offset += segmentLength;
    }

    activeVoiceCount.store (voiceEngine.getActiveVoiceCount(), std::memory_order_relaxed);
}

template <typename FloatType>
void AethrProcessor::applyOutputStage (juce::AudioBuffer<FloatType>& buffer)
{
    const auto numSamples  = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();

    if (numSamples <= 0 || numChannels <= 0)
        return;

    outputGain.setTargetValue (math::decibelsToGain (readValue (handles.outputGainDb, 0.0),
                                                     static_cast<double> (outputGainMinusInfinityDb)));

    const auto rampSamples = static_cast<std::size_t> (numSamples);

    if (rampSamples <= outputGainRamp.size())
    {
        // The smoother advances once per sample, so the ramp is materialised once
        // and then applied to every channel. Doing it per channel instead would
        // desynchronise the channels.
        for (std::size_t i = 0; i < rampSamples; ++i)
            outputGainRamp[i] = outputGain.getNextValue();

        for (int channel = 0; channel < numChannels; ++channel)
        {
            auto* samples = buffer.getWritePointer (channel);

            for (std::size_t i = 0; i < rampSamples; ++i)
                samples[i] *= static_cast<FloatType> (outputGainRamp[i]);
        }
    }
    else
    {
        // The host exceeded the block size it promised in prepareToPlay(). Skip the
        // ramp rather than allocate: a single block at the target gain is far less
        // harmful than a malloc on the audio thread. The occurrence is counted so it
        // is visible to diagnostics and to tests - deliberately not asserted, since
        // jassertfalse would log from the audio thread.
        blockSizeOverruns.fetch_add (1, std::memory_order_relaxed);

        const auto target = outputGain.getTargetValue();
        outputGain.setCurrentAndTargetValue (target);

        for (int channel = 0; channel < numChannels; ++channel)
            buffer.applyGain (channel, 0, numSamples, static_cast<FloatType> (target));
    }

    auto peak = 0.0f;

    for (int channel = 0; channel < numChannels; ++channel)
    {
        auto* samples = buffer.getWritePointer (channel);

        // Last line of defence: a non-finite sample must never reach the host,
        // where it would poison downstream plugins and metering.
        guards::sanitiseBlock (samples, static_cast<std::size_t> (numSamples));
        peak = std::max (peak,
                         static_cast<float> (guards::peakMagnitude (samples, static_cast<std::size_t> (numSamples))));
    }

    outputPeakLevel.store (peak, std::memory_order_relaxed);
}

//==============================================================================
juce::AudioProcessorEditor* AethrProcessor::createEditor()
{
    return new AethrEditor (*this);
}

//==============================================================================
void AethrProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // Called on the message thread. Copying the tree first keeps the snapshot
    // consistent if a parameter changes while we serialise.
    const auto state = valueTreeState.copyState();

    if (const auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void AethrProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary (data, sizeInBytes);

    if (xml == nullptr)
        return;

    // Reject state belonging to another plugin, and state whose root node we do
    // not recognise, rather than half-applying it.
    if (! xml->hasTagName (valueTreeState.state.getType()))
        return;

    valueTreeState.replaceState (juce::ValueTree::fromXml (*xml));
}

} // namespace aethr

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new aethr::AethrProcessor();
}
