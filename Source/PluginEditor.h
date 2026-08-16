#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace strata
{

class StrataProcessor;

/**
    Phase 1 editor.

    Deliberately minimal: it establishes the visual language (dark instrument
    panel, restrained accent colour, engineering typography) and the update
    discipline that the full interface will follow, without pre-committing to a
    layout that the DSP work has not yet justified.

    UI performance rules already enforced here:

      - a single timer drives all animation, at a fixed modest rate
      - only the regions whose data changed are repainted, never the whole editor
      - the audio thread is never called into; the editor polls atomics

    The complete interface, including the resonator and spectrum visualisers,
    arrives in Phase 13.

    @see docs/ARCHITECTURE.md ("UI architecture")
*/
class StrataEditor final : public juce::AudioProcessorEditor,
                           private juce::Timer
{
public:
    explicit StrataEditor (StrataProcessor&);
    ~StrataEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    /** Bounds of the level meter, so the timer can repaint just that strip. */
    [[nodiscard]] juce::Rectangle<int> getMeterBounds() const;

    /** Bounds of the status line showing MIDI activity. */
    [[nodiscard]] juce::Rectangle<int> getStatusBounds() const;

    // Named to avoid shadowing AudioProcessorEditor::processor, which is a
    // reference to the base AudioProcessor type.
    StrataProcessor& strataProcessor;

    /** Decayed peak level, so the meter falls smoothly instead of flickering. */
    float displayedLevel { 0.0f };
    int   displayedMidiCount { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StrataEditor)
};

} // namespace strata
