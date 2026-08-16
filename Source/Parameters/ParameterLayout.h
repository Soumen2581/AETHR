#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace strata::params
{

/** Identifier of the root ValueTree node used for plugin state. */
inline constexpr auto stateTreeType = "STRATA_STATE";

/** Number of selectable polyphony settings, indexed by the voice.polyphony choice parameter. */
inline constexpr int numPolyphonyOptions = 4;

/** Voice counts offered by the voice.polyphony parameter, in choice-index order. */
inline constexpr int polyphonyOptions[numPolyphonyOptions] { 8, 16, 32, 64 };

/** Default polyphony choice index (16 voices). */
inline constexpr int defaultPolyphonyIndex = 1;

/**
    Builds the full parameter layout.

    Called exactly once, from the AudioProcessor constructor. Parameters are
    grouped so hosts that display a tree (Logic, Cubase) show something sane, and
    so the UI can be driven from the same grouping later.
*/
[[nodiscard]] juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

} // namespace strata::params
