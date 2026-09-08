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
    set (s, params::exciter::burstTime, 6.0f);
    set (s, params::exciter::attack, 15.0f);
    set (s, params::exciter::colour, 0.0f);
    set (s, params::exciter::brightness, 70.0f);
    set (s, params::exciter::level, 80.0f);
    set (s, params::exciter::randomAmount, 0.0f);
    set (s, params::exciter::stereoSpread, 0.0f);
    set (s, params::exciter::seedLocked, 0.0f);
    set (s, params::exciter::seed, 1.0f);

    set (s, params::resonator::decayTime, 1.6f);
    set (s, params::resonator::releaseTime, 0.25f);
    set (s, params::resonator::damping, 35.0f);
    set (s, params::resonator::brightness, 60.0f);
    set (s, params::resonator::stiffness, 0.0f);
    set (s, params::resonator::loopFilterMode, 1.0f);
    set (s, params::resonator::interpolation, 2.0f);
    set (s, params::resonator::feedback, 100.0f);

    set (s, params::material::type, 0.0f);
    set (s, params::body::preset, 0.0f);
    set (s, params::body::modes, 2.0f);
    set (s, params::body::mix, 0.0f);
    set (s, params::body::decay, 40.0f);
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
    set (s, params::fx::reverbSize, 55.0f);
    set (s, params::fx::reverbDecay, 45.0f);
    set (s, params::fx::reverbDamp, 40.0f);
    set (s, params::fx::reverbWidth, 80.0f);
    set (s, params::fx::reverbMix, 0.0f);

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

inline constexpr FactoryPreset factory[]
{
    { "Init",            "Init",         init },
    { "Clean Pluck",     "Plucked",      plucked },
    { "Nylon",           "Plucked",      nylon },
    { "Steel Wire",      "Metallic",     steel },
    { "Wooden Mallet",   "Percussion",   malletWood },
    { "Glass Bell",      "Bells",        glassBell },
    { "Metal Strike",    "Metallic",     metalHit },
    { "Crystal Pad",     "Glass",        crystalPad },
    { "Bowed Drone",     "Drones",       drone },
    { "Dark Bass",       "Bass",         darkBass },
    { "Psychedelic",     "Psychedelic",  psychedelic },
    { "Atmosphere",      "Atmosphere",   atmosphere },
    { "Skin Hit",        "Percussion",   percussion },
};

inline int numFactoryPresets() noexcept { return static_cast<int> (std::size (factory)); }

inline void applyFactory (juce::AudioProcessorValueTreeState& state, int index)
{
    if (index < 0 || index >= numFactoryPresets())
        return;

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
            set (state, params::fx::reverbMix, range (5.0f, 30.0f));
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
