#pragma once

#include <cstddef>
#include <juce_audio_processors/juce_audio_processors.h>

#include "DSP/PhysicalMaterial.h"
#include "Parameters/ParameterIDs.h"
#include "Parameters/ParameterLayout.h"

namespace aethr::presets
{

enum class RandomMode { safe, musical, experimental, chaotic };

struct FactoryPreset
{
    const char* name;
    const char* category;
    void (*apply) (juce::AudioProcessorValueTreeState&);
};

inline void set (juce::AudioProcessorValueTreeState& state, const char* id, float value)
{
    if (auto* parameter = state.getParameter (id))
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

inline void init (juce::AudioProcessorValueTreeState& s)
{
    set (s, params::exciter::type, 0.0f);
    set (s, params::exciter::burstTime, 5.0f);
    set (s, params::exciter::attack, 12.0f);
    set (s, params::exciter::colour, 8.0f);
    set (s, params::exciter::brightness, 72.0f);
    set (s, params::exciter::level, 82.0f);
    set (s, params::exciter::randomAmount, 4.0f);
    set (s, params::exciter::stereoSpread, 12.0f);
    set (s, params::exciter::seedLocked, 0.0f);
    set (s, params::exciter::seed, 1.0f);

    set (s, params::resonator::decayTime, 1.85f);
    set (s, params::resonator::releaseTime, 0.35f);
    set (s, params::resonator::damping, 32.0f);
    set (s, params::resonator::brightness, 62.0f);
    set (s, params::resonator::stiffness, 4.0f);
    set (s, params::resonator::loopFilterMode, 1.0f);
    set (s, params::resonator::interpolation, 2.0f);
    set (s, params::resonator::feedback, 100.0f);

    set (s, params::material::type, 0.0f);
    set (s, params::body::preset, 1.0f); // wood — subtle body presence
    set (s, params::body::modes, 2.0f);
    set (s, params::body::mix, 12.0f);
    set (s, params::body::decay, 42.0f);
    set (s, params::body::brightness, 55.0f);

    set (s, params::layer::bEnable, 0.0f);
    set (s, params::layer::bInterval, 12.0f);
    set (s, params::layer::bDetune, 0.0f);
    set (s, params::layer::bLevel, 0.0f);

    set (s, params::lfo1::wave, 0.0f);
    set (s, params::lfo1::rate, 0.8f);
    set (s, params::lfo1::sync, 0.0f);
    set (s, params::lfo1::division, 2.0f);
    set (s, params::lfo1::depth, 0.0f);
    set (s, params::lfo1::dest, 0.0f);
    set (s, params::lfo2::wave, 1.0f);
    set (s, params::lfo2::rate, 0.25f);
    set (s, params::lfo2::sync, 0.0f);
    set (s, params::lfo2::division, 2.0f);
    set (s, params::lfo2::depth, 0.0f);
    set (s, params::lfo2::dest, 0.0f);
    set (s, params::env::attack, 0.01f);
    set (s, params::env::decay, 0.2f);
    set (s, params::env::sustain, 70.0f);
    set (s, params::env::release, 0.25f);
    set (s, params::env::depth, 0.0f);
    set (s, params::env::dest, 0.0f);
    set (s, params::chaos::amount, 0.0f);
    set (s, params::chaos::rate, 0.35f);
    set (s, params::chaos::sync, 0.0f);
    set (s, params::chaos::division, 2.0f);
    set (s, params::chaos::bias, 0.0f);

    set (s, params::macros::material, 0.0f);
    set (s, params::macros::attack, 0.0f);
    set (s, params::macros::decay, 0.0f);
    set (s, params::macros::brightness, 0.0f);
    set (s, params::macros::body, 0.0f);
    set (s, params::macros::chaos, 0.0f);
    set (s, params::macros::space, 0.0f);
    set (s, params::macros::drive, 0.0f);

    set (s, params::fx::filterType, 0.0f);
    set (s, params::fx::filterCutoff, 8000.0f);
    set (s, params::fx::filterResonance, 15.0f);
    set (s, params::fx::filterMix, 0.0f);
    set (s, params::fx::satMode, 1.0f);
    set (s, params::fx::satDrive, 0.0f);
    set (s, params::fx::satTone, 50.0f);
    set (s, params::fx::satMix, 0.0f);
    set (s, params::fx::delayTimeL, 0.28f);
    set (s, params::fx::delayTimeR, 0.36f);
    set (s, params::fx::delaySync, 0.0f);
    set (s, params::fx::delayDivisionL, 3.0f);
    set (s, params::fx::delayDivisionR, 8.0f);
    set (s, params::fx::delayFeedback, 35.0f);
    set (s, params::fx::delayMix, 0.0f);
    set (s, params::fx::delayDamp, 25.0f);
    set (s, params::fx::chorusRate, 0.8f);
    set (s, params::fx::chorusSync, 0.0f);
    set (s, params::fx::chorusDivision, 2.0f);
    set (s, params::fx::chorusDepth, 35.0f);
    set (s, params::fx::chorusMix, 0.0f);
    set (s, params::fx::phaserRate, 0.3f);
    set (s, params::fx::phaserSync, 0.0f);
    set (s, params::fx::phaserDivision, 2.0f);
    set (s, params::fx::phaserDepth, 50.0f);
    set (s, params::fx::phaserFeedback, 25.0f);
    set (s, params::fx::phaserMix, 0.0f);
    set (s, params::fx::reverbSize, 48.0f);
    set (s, params::fx::reverbDecay, 38.0f);
    set (s, params::fx::reverbDamp, 42.0f);
    set (s, params::fx::reverbWidth, 78.0f);
    set (s, params::fx::reverbMix, 10.0f);

    set (s, params::output::gain, 0.0f);
    set (s, params::output::ceiling, 0.98f);
    set (s, params::master::tuneOctave, 0.0f);
    set (s, params::master::tuneSemitones, 0.0f);
    set (s, params::master::tuneCents, 0.0f);
    set (s, params::voice::polyphony, 1.0f);
    set (s, params::voice::velocityAmount, 75.0f);
    set (s, params::engine::type, 0.0f);
    set (s, params::engine::controlA, 50.0f);
    set (s, params::engine::controlB, 50.0f);
    set (s, params::engine::controlC, 50.0f);
    set (s, params::engine::controlD, 50.0f);
    set (s, params::arp::enable, 0.0f);
    set (s, params::arp::mode, 0.0f);
    set (s, params::arp::division, 3.0f);
    set (s, params::arp::octaves, 1.0f);
    set (s, params::arp::gate, 55.0f);
    set (s, params::arp::swing, 0.0f);
    set (s, params::arp::latch, 0.0f);
    set (s, params::arp::pattern, 65535.0f);
}

inline void plucked (juce::AudioProcessorValueTreeState& s)
{
    params::applyMaterialToState (s, static_cast<int> (dsp::Material::string));
    set (s, params::engine::type, 1.0f); // Pluck
    set (s, params::exciter::type, 0);
    set (s, params::exciter::burstTime, 5.0f);
    set (s, params::resonator::decayTime, 1.4f);
    set (s, params::fx::reverbMix, 12.0f);
}

inline void nylon (juce::AudioProcessorValueTreeState& s)
{
    params::applyMaterialToState (s, static_cast<int> (dsp::Material::nylon));
    set (s, params::fx::chorusMix, 8.0f);
}

inline void steel (juce::AudioProcessorValueTreeState& s)
{
    params::applyMaterialToState (s, static_cast<int> (dsp::Material::steel));
    set (s, params::fx::delayMix, 10.0f);
}

inline void malletWood (juce::AudioProcessorValueTreeState& s)
{
    params::applyMaterialToState (s, static_cast<int> (dsp::Material::wood));
    set (s, params::engine::type, 5.0f); // Membrane
    set (s, params::exciter::type, 3);
    set (s, params::fx::reverbMix, 18.0f);
}

inline void glassBell (juce::AudioProcessorValueTreeState& s)
{
    params::applyMaterialToState (s, static_cast<int> (dsp::Material::glass));
    set (s, params::engine::type, 3.0f); // Bell
    set (s, params::fx::reverbSize, 70.0f);
    set (s, params::fx::reverbMix, 28.0f);
}

inline void metalHit (juce::AudioProcessorValueTreeState& s)
{
    params::applyMaterialToState (s, static_cast<int> (dsp::Material::metal));
    set (s, params::engine::type, 4.0f); // Plate
    set (s, params::exciter::type, 5);
    set (s, params::fx::satMix, 18.0f);
}

inline void crystalPad (juce::AudioProcessorValueTreeState& s)
{
    params::applyMaterialToState (s, static_cast<int> (dsp::Material::crystal));
    set (s, params::engine::type, 11.0f); // Spectral
    set (s, params::exciter::type, 7);
    set (s, params::resonator::decayTime, 10.0f);
    set (s, params::fx::reverbMix, 40.0f);
    set (s, params::lfo1::depth, 18.0f);
    set (s, params::lfo1::dest, 4); // brightness
}

inline void drone (juce::AudioProcessorValueTreeState& s)
{
    params::applyMaterialToState (s, static_cast<int> (dsp::Material::synthetic));
    set (s, params::engine::type, 2.0f); // Bowed
    set (s, params::exciter::type, 6);
    set (s, params::resonator::decayTime, 20.0f);
    set (s, params::fx::phaserMix, 22.0f);
    set (s, params::fx::reverbMix, 35.0f);
}

inline void darkBass (juce::AudioProcessorValueTreeState& s)
{
    set (s, params::exciter::type, 0);
    set (s, params::resonator::damping, 55.0f);
    set (s, params::resonator::brightness, 28.0f);
    set (s, params::resonator::decayTime, 2.2f);
    set (s, params::fx::satMode, 2);
    set (s, params::fx::satDrive, 35.0f);
    set (s, params::fx::satMix, 40.0f);
    set (s, params::fx::filterType, 0);
    set (s, params::fx::filterCutoff, 900.0f);
    set (s, params::fx::filterMix, 55.0f);
}

inline void psychedelic (juce::AudioProcessorValueTreeState& s)
{
    params::applyMaterialToState (s, static_cast<int> (dsp::Material::alien));
    set (s, params::engine::type, 12.0f); // Hybrid
    set (s, params::layer::bEnable, 1.0f);
    set (s, params::layer::bInterval, 7.0f);
    set (s, params::layer::bLevel, 45.0f);
    set (s, params::fx::delayMix, 32.0f);
    set (s, params::fx::phaserMix, 28.0f);
    set (s, params::chaos::amount, 22.0f);
    set (s, params::lfo1::depth, 30.0f);
    set (s, params::lfo1::dest, 1);
}

inline void atmosphere (juce::AudioProcessorValueTreeState& s)
{
    set (s, params::exciter::type, 2);
    set (s, params::resonator::decayTime, 12.0f);
    set (s, params::body::mix, 30.0f);
    set (s, params::fx::reverbMix, 48.0f);
    set (s, params::fx::chorusMix, 16.0f);
}

inline void percussion (juce::AudioProcessorValueTreeState& s)
{
    params::applyMaterialToState (s, static_cast<int> (dsp::Material::rubber));
    set (s, params::exciter::type, 4);
    set (s, params::exciter::burstTime, 1.2f);
    set (s, params::body::preset, 5);
    set (s, params::body::mix, 40.0f);
}

// ---- Sprint 4 curated expansions (first 13 indices stay stable for tests) ----

inline void softNylon (juce::AudioProcessorValueTreeState& s)
{
    params::applyMaterialToState (s, static_cast<int> (dsp::Material::nylon));
    set (s, params::engine::type, 1.0f);
    set (s, params::exciter::brightness, 48.0f);
    set (s, params::resonator::decayTime, 2.4f);
    set (s, params::fx::chorusMix, 14.0f);
    set (s, params::fx::reverbMix, 16.0f);
}

inline void harpLike (juce::AudioProcessorValueTreeState& s)
{
    params::applyMaterialToState (s, static_cast<int> (dsp::Material::string));
    set (s, params::engine::type, 0.0f);
    set (s, params::exciter::burstTime, 3.0f);
    set (s, params::resonator::decayTime, 3.2f);
    set (s, params::resonator::brightness, 70.0f);
    set (s, params::fx::reverbMix, 22.0f);
}

inline void mutedWire (juce::AudioProcessorValueTreeState& s)
{
    params::applyMaterialToState (s, static_cast<int> (dsp::Material::steel));
    set (s, params::engine::type, 1.0f);
    set (s, params::resonator::damping, 62.0f);
    set (s, params::resonator::brightness, 40.0f);
    set (s, params::resonator::decayTime, 0.55f);
}

inline void fingerStyle (juce::AudioProcessorValueTreeState& s)
{
    params::applyMaterialToState (s, static_cast<int> (dsp::Material::nylon));
    set (s, params::engine::type, 1.0f);
    set (s, params::exciter::type, 0);
    set (s, params::exciter::colour, 22.0f);
    set (s, params::body::mix, 18.0f);
    set (s, params::fx::reverbMix, 8.0f);
}

inline void brassPlate (juce::AudioProcessorValueTreeState& s)
{
    params::applyMaterialToState (s, static_cast<int> (dsp::Material::metal));
    set (s, params::engine::type, 4.0f);
    set (s, params::exciter::type, 5);
    set (s, params::resonator::decayTime, 4.5f);
    set (s, params::fx::satMix, 12.0f);
    set (s, params::fx::reverbMix, 20.0f);
}

inline void tinCan (juce::AudioProcessorValueTreeState& s)
{
    params::applyMaterialToState (s, static_cast<int> (dsp::Material::hollow));
    set (s, params::engine::type, 7.0f); // Cavity
    set (s, params::exciter::type, 4);
    set (s, params::body::mix, 35.0f);
    set (s, params::fx::filterCutoff, 3200.0f);
    set (s, params::fx::filterMix, 40.0f);
}

inline void steelDrum (juce::AudioProcessorValueTreeState& s)
{
    params::applyMaterialToState (s, static_cast<int> (dsp::Material::metal));
    set (s, params::engine::type, 5.0f);
    set (s, params::exciter::type, 3);
    set (s, params::resonator::decayTime, 2.8f);
    set (s, params::fx::reverbMix, 24.0f);
}

inline void ceramicChime (juce::AudioProcessorValueTreeState& s)
{
    params::applyMaterialToState (s, static_cast<int> (dsp::Material::ceramic));
    set (s, params::engine::type, 3.0f);
    set (s, params::resonator::decayTime, 5.0f);
    set (s, params::fx::reverbSize, 62.0f);
    set (s, params::fx::reverbMix, 30.0f);
}

inline void templeBell (juce::AudioProcessorValueTreeState& s)
{
    params::applyMaterialToState (s, static_cast<int> (dsp::Material::metal));
    set (s, params::engine::type, 3.0f);
    set (s, params::exciter::type, 5);
    set (s, params::resonator::decayTime, 8.0f);
    set (s, params::fx::reverbMix, 36.0f);
    set (s, params::fx::delayMix, 8.0f);
}

inline void iceBell (juce::AudioProcessorValueTreeState& s)
{
    params::applyMaterialToState (s, static_cast<int> (dsp::Material::glass));
    set (s, params::engine::type, 3.0f);
    set (s, params::resonator::brightness, 82.0f);
    set (s, params::fx::chorusMix, 10.0f);
    set (s, params::fx::reverbMix, 32.0f);
}

inline void woodBlock (juce::AudioProcessorValueTreeState& s)
{
    params::applyMaterialToState (s, static_cast<int> (dsp::Material::wood));
    set (s, params::engine::type, 5.0f);
    set (s, params::exciter::burstTime, 0.8f);
    set (s, params::resonator::decayTime, 0.35f);
    set (s, params::resonator::damping, 70.0f);
    set (s, params::body::mix, 28.0f);
}

inline void tablaHit (juce::AudioProcessorValueTreeState& s)
{
    params::applyMaterialToState (s, static_cast<int> (dsp::Material::rubber));
    set (s, params::engine::type, 5.0f);
    set (s, params::exciter::type, 4);
    set (s, params::exciter::burstTime, 2.0f);
    set (s, params::resonator::decayTime, 0.9f);
    set (s, params::body::mix, 45.0f);
}

inline void rimShot (juce::AudioProcessorValueTreeState& s)
{
    params::applyMaterialToState (s, static_cast<int> (dsp::Material::wood));
    set (s, params::engine::type, 1.0f);
    set (s, params::exciter::burstTime, 0.6f);
    set (s, params::resonator::decayTime, 0.22f);
    set (s, params::fx::satDrive, 25.0f);
    set (s, params::fx::satMix, 22.0f);
}

inline void frameDrum (juce::AudioProcessorValueTreeState& s)
{
    params::applyMaterialToState (s, static_cast<int> (dsp::Material::rubber));
    set (s, params::engine::type, 5.0f);
    set (s, params::resonator::decayTime, 1.6f);
    set (s, params::body::preset, 5);
    set (s, params::body::mix, 50.0f);
    set (s, params::fx::reverbMix, 14.0f);
}

inline void subPluck (juce::AudioProcessorValueTreeState& s)
{
    set (s, params::engine::type, 1.0f);
    set (s, params::exciter::type, 0);
    set (s, params::resonator::damping, 48.0f);
    set (s, params::resonator::brightness, 22.0f);
    set (s, params::resonator::decayTime, 1.8f);
    set (s, params::fx::filterType, 0);
    set (s, params::fx::filterCutoff, 420.0f);
    set (s, params::fx::filterMix, 65.0f);
    set (s, params::fx::satMix, 28.0f);
}

inline void growlingBass (juce::AudioProcessorValueTreeState& s)
{
    params::applyMaterialToState (s, static_cast<int> (dsp::Material::synthetic));
    set (s, params::engine::type, 8.0f); // Waveguide
    set (s, params::resonator::brightness, 30.0f);
    set (s, params::fx::satMode, 2);
    set (s, params::fx::satDrive, 45.0f);
    set (s, params::fx::satMix, 50.0f);
    set (s, params::fx::filterCutoff, 700.0f);
    set (s, params::fx::filterMix, 45.0f);
}

inline void uprightGhost (juce::AudioProcessorValueTreeState& s)
{
    params::applyMaterialToState (s, static_cast<int> (dsp::Material::wood));
    set (s, params::engine::type, 0.0f);
    set (s, params::resonator::decayTime, 1.1f);
    set (s, params::resonator::brightness, 35.0f);
    set (s, params::body::mix, 22.0f);
    set (s, params::fx::reverbMix, 10.0f);
}

inline void celloBow (juce::AudioProcessorValueTreeState& s)
{
    params::applyMaterialToState (s, static_cast<int> (dsp::Material::string));
    set (s, params::engine::type, 2.0f);
    set (s, params::exciter::type, 6);
    set (s, params::resonator::decayTime, 6.0f);
    set (s, params::fx::reverbMix, 18.0f);
    set (s, params::fx::chorusMix, 6.0f);
}

inline void glassHarmonica (juce::AudioProcessorValueTreeState& s)
{
    params::applyMaterialToState (s, static_cast<int> (dsp::Material::glass));
    set (s, params::engine::type, 2.0f);
    set (s, params::exciter::type, 6);
    set (s, params::resonator::decayTime, 9.0f);
    set (s, params::fx::reverbMix, 40.0f);
    set (s, params::lfo1::depth, 12.0f);
    set (s, params::lfo1::dest, 4);
}

inline void organPipe (juce::AudioProcessorValueTreeState& s)
{
    params::applyMaterialToState (s, static_cast<int> (dsp::Material::hollow));
    set (s, params::engine::type, 6.0f); // Tube
    set (s, params::exciter::type, 6);
    set (s, params::resonator::decayTime, 4.0f);
    set (s, params::fx::reverbMix, 20.0f);
}

inline void fluteTube (juce::AudioProcessorValueTreeState& s)
{
    params::applyMaterialToState (s, static_cast<int> (dsp::Material::wood));
    set (s, params::engine::type, 6.0f);
    set (s, params::exciter::brightness, 68.0f);
    set (s, params::resonator::decayTime, 1.4f);
    set (s, params::fx::reverbMix, 16.0f);
}

inline void choirPad (juce::AudioProcessorValueTreeState& s)
{
    params::applyMaterialToState (s, static_cast<int> (dsp::Material::synthetic));
    set (s, params::engine::type, 9.0f); // Modal
    set (s, params::layer::bEnable, 1.0f);
    set (s, params::layer::bInterval, 7.0f);
    set (s, params::layer::bDetune, 8.0f);
    set (s, params::layer::bLevel, 38.0f);
    set (s, params::resonator::decayTime, 8.0f);
    set (s, params::fx::chorusMix, 22.0f);
    set (s, params::fx::reverbMix, 42.0f);
}

inline void cloudPad (juce::AudioProcessorValueTreeState& s)
{
    params::applyMaterialToState (s, static_cast<int> (dsp::Material::crystal));
    set (s, params::engine::type, 11.0f);
    set (s, params::resonator::decayTime, 14.0f);
    set (s, params::fx::reverbSize, 80.0f);
    set (s, params::fx::reverbMix, 55.0f);
    set (s, params::fx::chorusMix, 18.0f);
    set (s, params::lfo2::depth, 15.0f);
    set (s, params::lfo2::dest, 4);
}

inline void softKeys (juce::AudioProcessorValueTreeState& s)
{
    params::applyMaterialToState (s, static_cast<int> (dsp::Material::nylon));
    set (s, params::engine::type, 9.0f);
    set (s, params::exciter::burstTime, 8.0f);
    set (s, params::resonator::decayTime, 2.0f);
    set (s, params::env::attack, 0.04f);
    set (s, params::fx::reverbMix, 18.0f);
}

inline void brightLead (juce::AudioProcessorValueTreeState& s)
{
    params::applyMaterialToState (s, static_cast<int> (dsp::Material::steel));
    set (s, params::engine::type, 8.0f);
    set (s, params::resonator::brightness, 78.0f);
    set (s, params::fx::satMix, 20.0f);
    set (s, params::fx::delayMix, 18.0f);
    set (s, params::fx::delayFeedback, 42.0f);
}

inline void tapeEchoLead (juce::AudioProcessorValueTreeState& s)
{
    params::applyMaterialToState (s, static_cast<int> (dsp::Material::string));
    set (s, params::engine::type, 1.0f);
    set (s, params::fx::delaySync, 1.0f);
    set (s, params::fx::delayDivisionL, 3.0f);
    set (s, params::fx::delayDivisionR, 8.0f);
    set (s, params::fx::delayMix, 38.0f);
    set (s, params::fx::delayFeedback, 48.0f);
    set (s, params::fx::satMix, 12.0f);
}

inline void frozenDrone (juce::AudioProcessorValueTreeState& s)
{
    params::applyMaterialToState (s, static_cast<int> (dsp::Material::crystal));
    set (s, params::engine::type, 2.0f);
    set (s, params::resonator::decayTime, 24.0f);
    set (s, params::fx::reverbMix, 50.0f);
    set (s, params::fx::phaserMix, 16.0f);
    set (s, params::lfo1::rate, 0.12f);
    set (s, params::lfo1::depth, 20.0f);
    set (s, params::lfo1::dest, 4);
}

inline void hollowWind (juce::AudioProcessorValueTreeState& s)
{
    params::applyMaterialToState (s, static_cast<int> (dsp::Material::hollow));
    set (s, params::engine::type, 7.0f);
    set (s, params::exciter::type, 2);
    set (s, params::resonator::decayTime, 11.0f);
    set (s, params::fx::filterCutoff, 1800.0f);
    set (s, params::fx::filterMix, 35.0f);
    set (s, params::fx::reverbMix, 45.0f);
}

inline void alienSwarm (juce::AudioProcessorValueTreeState& s)
{
    params::applyMaterialToState (s, static_cast<int> (dsp::Material::alien));
    set (s, params::engine::type, 12.0f);
    set (s, params::layer::bEnable, 1.0f);
    set (s, params::layer::bInterval, 5.0f);
    set (s, params::layer::bDetune, 18.0f);
    set (s, params::layer::bLevel, 55.0f);
    set (s, params::chaos::amount, 35.0f);
    set (s, params::fx::phaserMix, 34.0f);
    set (s, params::fx::delayMix, 28.0f);
}

inline void grainCloud (juce::AudioProcessorValueTreeState& s)
{
    params::applyMaterialToState (s, static_cast<int> (dsp::Material::synthetic));
    set (s, params::engine::type, 10.0f); // Granular
    set (s, params::engine::controlA, 62.0f);
    set (s, params::engine::controlB, 40.0f);
    set (s, params::resonator::decayTime, 7.0f);
    set (s, params::fx::reverbMix, 38.0f);
    set (s, params::chaos::amount, 12.0f);
}

inline void spectralWash (juce::AudioProcessorValueTreeState& s)
{
    params::applyMaterialToState (s, static_cast<int> (dsp::Material::crystal));
    set (s, params::engine::type, 11.0f);
    set (s, params::exciter::type, 7);
    set (s, params::resonator::decayTime, 16.0f);
    set (s, params::fx::reverbMix, 48.0f);
    set (s, params::fx::phaserMix, 18.0f);
}

inline void hybridClash (juce::AudioProcessorValueTreeState& s)
{
    params::applyMaterialToState (s, static_cast<int> (dsp::Material::alien));
    set (s, params::engine::type, 12.0f);
    set (s, params::engine::controlA, 70.0f);
    set (s, params::engine::controlC, 55.0f);
    set (s, params::fx::satMix, 30.0f);
    set (s, params::fx::delayMix, 22.0f);
    set (s, params::chaos::amount, 28.0f);
}

inline void nightPad (juce::AudioProcessorValueTreeState& s)
{
    params::applyMaterialToState (s, static_cast<int> (dsp::Material::glass));
    set (s, params::engine::type, 11.0f);
    set (s, params::resonator::brightness, 45.0f);
    set (s, params::resonator::decayTime, 12.0f);
    set (s, params::fx::filterCutoff, 2400.0f);
    set (s, params::fx::filterMix, 30.0f);
    set (s, params::fx::reverbMix, 44.0f);
}

inline void dawnPluck (juce::AudioProcessorValueTreeState& s)
{
    params::applyMaterialToState (s, static_cast<int> (dsp::Material::nylon));
    set (s, params::engine::type, 1.0f);
    set (s, params::resonator::brightness, 72.0f);
    set (s, params::fx::delayMix, 14.0f);
    set (s, params::fx::reverbMix, 20.0f);
    set (s, params::fx::chorusMix, 10.0f);
}

inline void mistRoom (juce::AudioProcessorValueTreeState& s)
{
    set (s, params::exciter::type, 2);
    set (s, params::engine::type, 7.0f);
    set (s, params::resonator::decayTime, 9.0f);
    set (s, params::body::mix, 25.0f);
    set (s, params::fx::reverbSize, 72.0f);
    set (s, params::fx::reverbMix, 58.0f);
    set (s, params::fx::chorusMix, 12.0f);
}

inline void pulseArp (juce::AudioProcessorValueTreeState& s)
{
    params::applyMaterialToState (s, static_cast<int> (dsp::Material::steel));
    set (s, params::engine::type, 1.0f);
    set (s, params::resonator::decayTime, 0.7f);
    set (s, params::arp::enable, 1.0f);
    set (s, params::arp::mode, 0.0f);
    set (s, params::arp::division, 3.0f);
    set (s, params::arp::octaves, 2.0f);
    set (s, params::arp::gate, 45.0f);
    set (s, params::fx::delayMix, 20.0f);
}

inline void cascadeArp (juce::AudioProcessorValueTreeState& s)
{
    params::applyMaterialToState (s, static_cast<int> (dsp::Material::glass));
    set (s, params::engine::type, 3.0f);
    set (s, params::arp::enable, 1.0f);
    set (s, params::arp::mode, 1.0f);
    set (s, params::arp::division, 2.0f);
    set (s, params::arp::octaves, 3.0f);
    set (s, params::arp::swing, 18.0f);
    set (s, params::fx::reverbMix, 28.0f);
}

inline void rubberBand (juce::AudioProcessorValueTreeState& s)
{
    params::applyMaterialToState (s, static_cast<int> (dsp::Material::rubber));
    set (s, params::engine::type, 0.0f);
    set (s, params::resonator::stiffness, 35.0f);
    set (s, params::resonator::decayTime, 1.2f);
    set (s, params::fx::satMix, 15.0f);
}

inline void waveguideLead (juce::AudioProcessorValueTreeState& s)
{
    params::applyMaterialToState (s, static_cast<int> (dsp::Material::synthetic));
    set (s, params::engine::type, 8.0f);
    set (s, params::engine::controlA, 58.0f);
    set (s, params::resonator::brightness, 68.0f);
    set (s, params::fx::delayMix, 16.0f);
    set (s, params::fx::chorusMix, 12.0f);
}

inline void modalKeys (juce::AudioProcessorValueTreeState& s)
{
    params::applyMaterialToState (s, static_cast<int> (dsp::Material::ceramic));
    set (s, params::engine::type, 9.0f);
    set (s, params::exciter::type, 3);
    set (s, params::resonator::decayTime, 3.5f);
    set (s, params::fx::reverbMix, 22.0f);
}

inline void cavityBoom (juce::AudioProcessorValueTreeState& s)
{
    params::applyMaterialToState (s, static_cast<int> (dsp::Material::hollow));
    set (s, params::engine::type, 7.0f);
    set (s, params::exciter::type, 5);
    set (s, params::resonator::brightness, 28.0f);
    set (s, params::resonator::decayTime, 2.5f);
    set (s, params::fx::satMix, 25.0f);
    set (s, params::fx::reverbMix, 15.0f);
}

inline void shimmerGlass (juce::AudioProcessorValueTreeState& s)
{
    params::applyMaterialToState (s, static_cast<int> (dsp::Material::glass));
    set (s, params::engine::type, 11.0f);
    set (s, params::layer::bEnable, 1.0f);
    set (s, params::layer::bInterval, 12.0f);
    set (s, params::layer::bLevel, 30.0f);
    set (s, params::fx::chorusMix, 20.0f);
    set (s, params::fx::reverbMix, 36.0f);
    set (s, params::lfo1::depth, 22.0f);
    set (s, params::lfo1::dest, 4);
}

inline void deepPlate (juce::AudioProcessorValueTreeState& s)
{
    params::applyMaterialToState (s, static_cast<int> (dsp::Material::metal));
    set (s, params::engine::type, 4.0f);
    set (s, params::resonator::decayTime, 7.0f);
    set (s, params::resonator::brightness, 42.0f);
    set (s, params::fx::reverbMix, 30.0f);
    set (s, params::fx::delayMix, 10.0f);
}

inline void noisyStrike (juce::AudioProcessorValueTreeState& s)
{
    params::applyMaterialToState (s, static_cast<int> (dsp::Material::alien));
    set (s, params::engine::type, 4.0f);
    set (s, params::exciter::type, 7);
    set (s, params::exciter::randomAmount, 40.0f);
    set (s, params::chaos::amount, 18.0f);
    set (s, params::fx::satMix, 35.0f);
}

inline void warmRoom (juce::AudioProcessorValueTreeState& s)
{
    params::applyMaterialToState (s, static_cast<int> (dsp::Material::wood));
    set (s, params::engine::type, 0.0f);
    set (s, params::body::mix, 28.0f);
    set (s, params::fx::reverbSize, 40.0f);
    set (s, params::fx::reverbDamp, 55.0f);
    set (s, params::fx::reverbMix, 28.0f);
}

inline void crystalRain (juce::AudioProcessorValueTreeState& s)
{
    params::applyMaterialToState (s, static_cast<int> (dsp::Material::crystal));
    set (s, params::engine::type, 3.0f);
    set (s, params::arp::enable, 1.0f);
    set (s, params::arp::division, 4.0f);
    set (s, params::arp::octaves, 2.0f);
    set (s, params::arp::gate, 35.0f);
    set (s, params::fx::delayMix, 24.0f);
    set (s, params::fx::reverbMix, 32.0f);
}

inline void bowedMetal (juce::AudioProcessorValueTreeState& s)
{
    params::applyMaterialToState (s, static_cast<int> (dsp::Material::metal));
    set (s, params::engine::type, 2.0f);
    set (s, params::exciter::type, 6);
    set (s, params::resonator::decayTime, 10.0f);
    set (s, params::fx::satMix, 10.0f);
    set (s, params::fx::reverbMix, 26.0f);
}

inline void vinylGhost (juce::AudioProcessorValueTreeState& s)
{
    params::applyMaterialToState (s, static_cast<int> (dsp::Material::nylon));
    set (s, params::engine::type, 10.0f);
    set (s, params::exciter::type, 2);
    set (s, params::fx::filterCutoff, 2800.0f);
    set (s, params::fx::filterMix, 45.0f);
    set (s, params::fx::satMix, 18.0f);
    set (s, params::fx::reverbMix, 22.0f);
    set (s, params::chaos::amount, 8.0f);
}

inline constexpr FactoryPreset factory[]
{
    { "Init",            "Init",         init },
    { "Clean Pluck",     "Plucked",      plucked },
    { "Nylon",           "Plucked",      nylon },
    { "Steel Wire",      "Metallic",     steel },
    { "Wooden Mallet",   "Percussion",   malletWood },
    { "Glass Bell",      "Bells",        glassBell },
    { "Metal Strike",    "Metallic",     metalHit },
    { "Crystal Pad",     "Pads",         crystalPad },
    { "Bowed Drone",     "Drones",       drone },
    { "Dark Bass",       "Bass",         darkBass },
    { "Psychedelic",     "Experimental", psychedelic },
    { "Atmosphere",      "Atmosphere",   atmosphere },
    { "Skin Hit",        "Percussion",   percussion },
    // Sprint 4 expansions
    { "Soft Nylon",      "Plucked",      softNylon },
    { "Harp Sweep",      "Plucked",      harpLike },
    { "Muted Wire",      "Plucked",      mutedWire },
    { "Finger Style",    "Plucked",      fingerStyle },
    { "Dawn Pluck",      "Plucked",      dawnPluck },
    { "Brass Plate",     "Metallic",     brassPlate },
    { "Tin Can",         "Metallic",     tinCan },
    { "Steel Drum",      "Metallic",     steelDrum },
    { "Deep Plate",      "Metallic",     deepPlate },
    { "Bowed Metal",     "Metallic",     bowedMetal },
    { "Ceramic Chime",   "Bells",        ceramicChime },
    { "Temple Bell",     "Bells",        templeBell },
    { "Ice Bell",        "Bells",        iceBell },
    { "Crystal Rain",    "Bells",        crystalRain },
    { "Wood Block",      "Percussion",   woodBlock },
    { "Tabla Hit",       "Percussion",   tablaHit },
    { "Rim Shot",        "Percussion",   rimShot },
    { "Frame Drum",      "Percussion",   frameDrum },
    { "Sub Pluck",       "Bass",         subPluck },
    { "Growling Bass",   "Bass",         growlingBass },
    { "Upright Ghost",   "Bass",         uprightGhost },
    { "Cavity Boom",     "Bass",         cavityBoom },
    { "Cello Bow",       "Strings",      celloBow },
    { "Glass Harmonica", "Strings",      glassHarmonica },
    { "Organ Pipe",      "Keys",         organPipe },
    { "Flute Tube",      "Keys",         fluteTube },
    { "Soft Keys",       "Keys",         softKeys },
    { "Modal Keys",      "Keys",         modalKeys },
    { "Choir Pad",       "Pads",         choirPad },
    { "Cloud Pad",       "Pads",         cloudPad },
    { "Night Pad",       "Pads",         nightPad },
    { "Shimmer Glass",   "Pads",         shimmerGlass },
    { "Bright Lead",     "Leads",        brightLead },
    { "Tape Echo Lead",  "Leads",        tapeEchoLead },
    { "Waveguide Lead",  "Leads",        waveguideLead },
    { "Frozen Drone",    "Drones",       frozenDrone },
    { "Hollow Wind",     "Atmosphere",   hollowWind },
    { "Mist Room",       "Atmosphere",   mistRoom },
    { "Warm Room",       "Atmosphere",   warmRoom },
    { "Alien Swarm",     "Experimental", alienSwarm },
    { "Grain Cloud",     "Experimental", grainCloud },
    { "Spectral Wash",   "Experimental", spectralWash },
    { "Hybrid Clash",    "Experimental", hybridClash },
    { "Noisy Strike",    "Experimental", noisyStrike },
    { "Vinyl Ghost",     "Experimental", vinylGhost },
    { "Pulse Arp",       "Arp",          pulseArp },
    { "Cascade Arp",     "Arp",          cascadeArp },
    { "Rubber Band",     "Plucked",      rubberBand },
};

inline int numFactoryPresets() noexcept { return static_cast<int> (std::size (factory)); }

inline void applyFactory (juce::AudioProcessorValueTreeState& state, int index)
{
    if (index < 0 || index >= numFactoryPresets())
        return;

    // Always start from a known-clean Init snapshot so factory patches cannot
    // leak Layer B / FX / chaos from the previously loaded sound.
    if (index != 0)
        init (state);

    factory[static_cast<std::size_t> (index)].apply (state);
}

inline void randomize (juce::AudioProcessorValueTreeState& state, RandomMode mode, juce::Random& rng)
{
    const auto range = [&] (float a, float b) { return a + rng.nextFloat() * (b - a); };

    switch (mode)
    {
        case RandomMode::safe:
            set (state, params::exciter::type, static_cast<float> (rng.nextInt (6)));
            set (state, params::resonator::decayTime, range (0.4f, 3.0f));
            set (state, params::resonator::damping, range (20.0f, 55.0f));
            set (state, params::resonator::brightness, range (40.0f, 75.0f));
            break;
        case RandomMode::musical:
            params::applyMaterialToState (state, rng.nextInt (params::numMaterials));
            set (state, params::engine::type, static_cast<float> (rng.nextInt (8))); // physical family
            set (state, params::resonator::decayTime, range (0.8f, 4.5f));
            set (state, params::resonator::damping, range (18.0f, 50.0f));
            set (state, params::resonator::brightness, range (45.0f, 78.0f));
            set (state, params::body::mix, range (0.0f, 28.0f));
            set (state, params::fx::reverbMix, range (6.0f, 28.0f));
            set (state, params::fx::delayMix, range (0.0f, 18.0f));
            set (state, params::fx::satMix, 0.0f);
            set (state, params::chaos::amount, 0.0f);
            set (state, params::layer::bEnable, 0.0f);
            break;
        case RandomMode::experimental:
            params::applyMaterialToState (state, rng.nextInt (params::numMaterials));
            set (state, params::resonator::stiffness, range (10.0f, 80.0f));
            set (state, params::body::mix, range (10.0f, 50.0f));
            set (state, params::fx::delayMix, range (10.0f, 40.0f));
            set (state, params::lfo1::depth, range (10.0f, 40.0f));
            set (state, params::lfo1::dest, static_cast<float> (1 + rng.nextInt (8)));
            set (state, params::engine::type, static_cast<float> (rng.nextInt (params::numEngineTypes)));
            break;
        case RandomMode::chaotic:
            params::applyMaterialToState (state, rng.nextInt (params::numMaterials));
            set (state, params::engine::type, static_cast<float> (rng.nextInt (params::numEngineTypes)));
            set (state, params::chaos::amount, range (20.0f, 70.0f));
            set (state, params::layer::bEnable, 1.0f);
            set (state, params::layer::bLevel, range (20.0f, 70.0f));
            set (state, params::fx::phaserMix, range (10.0f, 50.0f));
            set (state, params::fx::satMix, range (10.0f, 50.0f));
            break;
    }
}

inline void mutate (juce::AudioProcessorValueTreeState& state, juce::Random& rng, float amount = 0.15f)
{
    const auto nudge = [&] (const char* id, float span)
    {
        auto* parameter = state.getParameter (id);

        if (parameter == nullptr)
            return;

        const auto current = parameter->convertFrom0to1 (parameter->getValue());
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (current + (rng.nextFloat() - 0.5f) * 2.0f * span * amount));
    };

    nudge (params::resonator::damping, 20.0f);
    nudge (params::resonator::brightness, 20.0f);
    nudge (params::resonator::decayTime, 1.5f);
    nudge (params::exciter::colour, 30.0f);
    nudge (params::body::mix, 15.0f);
}

} // namespace aethr::presets
