#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "UI/Theme.h"

namespace aethr::ui
{

class LookAndFeel final : public juce::LookAndFeel_V4
{
public:
    LookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId, juce::Colour (Theme::background));
        setColour (juce::Label::textColourId, juce::Colour (Theme::text));
        setColour (juce::ComboBox::backgroundColourId, juce::Colour (Theme::glass));
        setColour (juce::ComboBox::textColourId, juce::Colour (Theme::text));
        setColour (juce::ComboBox::outlineColourId, juce::Colour (Theme::goldDim));
        setColour (juce::ComboBox::arrowColourId, juce::Colour (Theme::gold));
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (Theme::panel));
        setColour (juce::PopupMenu::textColourId, juce::Colour (Theme::text));
        setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff2a2418));
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colour (Theme::gold));
        setColour (juce::TextButton::buttonColourId, juce::Colour (Theme::panelInner));
        setColour (juce::TextButton::textColourOffId, juce::Colour (Theme::text));
        setColour (juce::TextButton::textColourOnId, juce::Colour (Theme::gold));
        setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff2a2418));
        setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (Theme::panel));
        setColour (juce::TooltipWindow::textColourId, juce::Colour (Theme::text));
        setColour (juce::TooltipWindow::outlineColourId, juce::Colour (Theme::goldDim));
        setColour (juce::CaretComponent::caretColourId, juce::Colour (Theme::gold));
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return labelFont (11.0f); }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return labelFont (10.5f); }
    juce::Font getPopupMenuFont() override { return labelFont (12.0f); }

    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override {}

    void drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box) override
    {
        auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);
        g.setColour (juce::Colour (Theme::glass));
        g.fillRect (bounds);
        g.setColour (box.hasKeyboardFocus (true) ? juce::Colour (Theme::gold) : juce::Colour (Theme::goldDim));
        g.drawRect (bounds, 1.0f);

        juce::Path arrow;
        const auto cx = (float) width - 10.0f;
        const auto cy = (float) height * 0.5f;
        arrow.addTriangle (cx - 3.4f, cy - 1.8f, cx + 3.4f, cy - 1.8f, cx, cy + 2.4f);
        g.setColour (juce::Colour (Theme::gold));
        g.fillPath (arrow);
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (6, 1, juce::jmax (1, box.getWidth() - 20), juce::jmax (1, box.getHeight() - 2));
        label.setFont (getComboBoxFont (box));
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                               bool highlighted, bool down) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
        auto fill = juce::Colour (Theme::panelInner);

        if (button.getToggleState() || down)
            fill = juce::Colour (0xff2a2418);
        else if (highlighted)
            fill = fill.brighter (0.07f);

        g.setColour (fill);
        g.fillRect (bounds);
        g.setColour (button.getToggleState() ? juce::Colour (Theme::gold) : juce::Colour (Theme::goldDim));
        g.drawRect (bounds, 1.0f);
    }

    void drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height) override
    {
        auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height);
        g.setColour (juce::Colour (Theme::panel));
        g.fillRect (bounds);
        g.setColour (juce::Colour (Theme::goldDim));
        g.drawRect (bounds, 1.0f);
        g.setColour (juce::Colour (Theme::text));
        g.setFont (labelFont (12.0f));
        g.drawFittedText (text, bounds.reduced (8.0f).toNearestInt(), juce::Justification::centredLeft, 6);
    }
};

} // namespace aethr::ui
