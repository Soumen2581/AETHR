#include "PluginEditor.h"

#include "Core/AudioMath.h"
#include "Core/Branding.h"
#include "PluginProcessor.h"

namespace strata
{

namespace
{
    constexpr int defaultEditorWidth  = 900;
    constexpr int defaultEditorHeight = 560;
    constexpr int minimumEditorWidth  = 700;
    constexpr int minimumEditorHeight = 440;
    constexpr int maximumEditorWidth  = 1800;
    constexpr int maximumEditorHeight = 1120;

    /** Refresh rate for meters and indicators. 30 Hz reads as smooth and costs little. */
    constexpr int refreshRateHz = 30;

    /** Meter fall-back per frame. Slow enough to read a transient, fast enough to feel live. */
    constexpr float meterDecayPerFrame = 0.06f;

    constexpr int   edgeMargin      = 28;
    constexpr int   meterWidth      = 8;
    constexpr int   meterHeight     = 120;
    constexpr float meterFloorDb    = -60.0f;

    // Palette: near-black panel, cool grey text, single cyan accent. Kept in one
    // place so the Phase 13 LookAndFeel can adopt it wholesale.
    const juce::Colour panelTop      { 0xff141719 };
    const juce::Colour panelBottom   { 0xff0b0d0e };
    const juce::Colour hairline      { 0xff2a3033 };
    const juce::Colour textPrimary   { 0xffe6ecef };
    const juce::Colour textSecondary { 0xff7d8a91 };
    const juce::Colour accent        { 0xff35d0d8 };
} // namespace

//==============================================================================
StrataEditor::StrataEditor (StrataProcessor& processorToUse)
    : juce::AudioProcessorEditor (&processorToUse),
      strataProcessor (processorToUse)
{
    setResizable (true, true);
    setResizeLimits (minimumEditorWidth, minimumEditorHeight, maximumEditorWidth, maximumEditorHeight);
    setSize (defaultEditorWidth, defaultEditorHeight);

    startTimerHz (refreshRateHz);
}

StrataEditor::~StrataEditor()
{
    stopTimer();
}

//==============================================================================
juce::Rectangle<int> StrataEditor::getMeterBounds() const
{
    return { getWidth() - edgeMargin - meterWidth, edgeMargin, meterWidth, meterHeight };
}

juce::Rectangle<int> StrataEditor::getStatusBounds() const
{
    const auto lineHeight = 18;
    return { edgeMargin, getHeight() - edgeMargin - lineHeight, getWidth() - 2 * edgeMargin, lineHeight };
}

//==============================================================================
void StrataEditor::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    g.setGradientFill (juce::ColourGradient::vertical (panelTop, bounds.getY(),
                                                       panelBottom, bounds.getBottom()));
    g.fillAll();

    // Title block
    auto titleArea = getLocalBounds().reduced (edgeMargin, edgeMargin);
    titleArea = titleArea.removeFromTop (110);

    g.setColour (textPrimary);
    g.setFont (juce::FontOptions().withHeight (44.0f).withStyle ("Bold"));
    g.drawText (branding::productName, titleArea.removeFromTop (52),
                juce::Justification::topLeft, false);

    g.setColour (accent);
    g.setFont (juce::FontOptions().withHeight (13.0f));
    g.drawText (juce::String (branding::tagline).toUpperCase(), titleArea.removeFromTop (20),
                juce::Justification::topLeft, false);

    // Hairline under the header
    const auto ruleY = static_cast<float> (edgeMargin + 100);
    g.setColour (hairline);
    g.fillRect (static_cast<float> (edgeMargin), ruleY,
                static_cast<float> (getWidth() - 2 * edgeMargin), 1.0f);

    // Meter frame
    const auto meter = getMeterBounds();
    g.setColour (hairline);
    g.drawRect (meter, 1);

    if (displayedLevel > 0.0f)
    {
        const auto levelDb = static_cast<float> (math::gainToDecibels (static_cast<double> (displayedLevel),
                                                                      static_cast<double> (meterFloorDb)));
        const auto normalised = juce::jlimit (0.0f, 1.0f, (levelDb - meterFloorDb) / -meterFloorDb);
        const auto filledHeight = juce::roundToInt (normalised * static_cast<float> (meter.getHeight()));

        g.setColour (accent.withAlpha (0.85f));
        g.fillRect (meter.reduced (1).removeFromBottom (filledHeight));
    }

    // Footer: build identity and live status
    g.setColour (textSecondary);
    g.setFont (juce::FontOptions().withHeight (12.0f));

    const auto footer = juce::String (branding::companyName) + "   v" + branding::version
                      + "   \xe2\x80\xa2   Phase 1 build: parameters, state and host integration only";
    g.drawText (footer, getStatusBounds(), juce::Justification::centredLeft, false);

    g.setColour (textSecondary);
    const auto status = juce::String ("MIDI events: ") + juce::String (displayedMidiCount)
                      + "    Voices: " + juce::String (strataProcessor.getSelectedPolyphony());
    g.drawText (status, getStatusBounds().translated (0, -20), juce::Justification::centredLeft, false);
}

void StrataEditor::resized()
{
    // No child components yet; the layout arrives with the Phase 13 interface.
}

//==============================================================================
void StrataEditor::timerCallback()
{
    const auto level = strataProcessor.getOutputPeakLevel();

    // Rise instantly, fall gradually.
    const auto decayed = std::max (0.0f, displayedLevel - meterDecayPerFrame);
    const auto updatedLevel = std::max (level, decayed);

    if (std::abs (updatedLevel - displayedLevel) > 1.0e-4f)
    {
        displayedLevel = updatedLevel;
        repaint (getMeterBounds());
    }

    const auto midiCount = strataProcessor.getMidiEventCount();

    if (midiCount != displayedMidiCount)
    {
        displayedMidiCount = midiCount;
        repaint (getStatusBounds().translated (0, -20));
    }
}

} // namespace strata
