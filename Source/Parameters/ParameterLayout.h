#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace aethr::params
{

/** Identifier of the root ValueTree node used for plugin state. */
inline constexpr auto stateTreeType = "AETHR_STATE";

/** Number of selectable polyphony settings, indexed by the voice.polyphony choice parameter. */
inline constexpr int numPolyphonyOptions = 4;

/** Voice counts offered by the voice.polyphony parameter, in choice-index order. */
inline constexpr int polyphonyOptions[numPolyphonyOptions] { 8, 16, 32, 64 };

/** Default polyphony choice index (16 voices). */
inline constexpr int defaultPolyphonyIndex = 1;

//==============================================================================
/**
    Choice-parameter cardinalities.

    Each of these must match the corresponding DSP enum exactly, because the choice
    index is cast straight to the enum. ParameterLayout.cpp static-asserts that.
*/
inline constexpr int numExcitationTypes = 8;
inline constexpr int numLoopFilterModes = 3;
inline constexpr int numInterpolationModes = 3;
inline constexpr int numMaterials = 12;
inline constexpr int numBodyPresets = 7;
inline constexpr int numBodyModeChoices = 5;
inline constexpr int numLfoWaves = 7;
inline constexpr int numModDestinations = 13;
inline constexpr int numFilterTypes = 6;
inline constexpr int numSatModes = 8;
inline constexpr int numTimeDivisions = 14;
inline constexpr int numEngineTypes = 13;
inline constexpr int numArpModes = 6;

inline constexpr int defaultTimeDivisionIndex = 2;
inline constexpr int defaultEngineIndex = 0;
inline constexpr int defaultArpDivisionIndex = 3;
inline constexpr int defaultDelayDivisionLIndex = 3;
inline constexpr int defaultDelayDivisionRIndex = 8;

inline constexpr int defaultInterpolationIndex = 2;
inline constexpr int defaultLoopFilterIndex = 1;
inline constexpr int defaultExcitationIndex = 0;

inline constexpr int bodyModeCounts[numBodyModeChoices] { 1, 2, 4, 8, 16 };

void applyMaterialToState (juce::AudioProcessorValueTreeState&, int materialIndex);

[[nodiscard]] juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

} // namespace aethr::params
