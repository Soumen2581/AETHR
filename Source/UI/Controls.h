#pragma once

#include <functional>
#include <vector>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "UI/Theme.h"

namespace aethr::ui
{

class AethrPanel : public juce::Component
{
public:
    AethrPanel (juce::String titleToUse, juce::String subtitleToUse, Glyph glyphToUse, juce::String codeToUse,
                Rank rankToUse = Rank::major)
        : title (std::move (titleToUse)),
          subtitle (std::move (subtitleToUse)),
          glyph (glyphToUse),
          code (std::move (codeToUse)),
          rank (rankToUse)
    {
        setOpaque (false);
        setInterceptsMouseClicks (false, true);
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        drawTechnicalFrame (g, bounds, rank);

        auto header = bounds.removeFromTop (rank == Rank::hero ? 32.0f : 28.0f).reduced (12.0f, 4.0f);
        auto glyphArea = header.removeFromLeft (18.0f);
        drawGlyph (g, glyph, glyphArea);
        header.removeFromLeft (6.0f);

        g.setColour (juce::Colour (Theme::gold).withAlpha (rank == Rank::minor ? 0.72f : 1.0f));
        g.setFont (titleFont (rank == Rank::hero ? 15.0f : 13.0f));
        g.drawText (title, header.removeFromTop (16.0f), juce::Justification::centredLeft, false);

        g.setColour (juce::Colour (Theme::textSecondary).withAlpha (0.78f));
        g.setFont (labelFont (8.0f));
        g.drawText (subtitle, header, juce::Justification::centredLeft, false);

        g.setColour (juce::Colour (Theme::goldDim).withAlpha (0.75f));
        g.setFont (valueFont (8.0f));
        g.drawText (code, getLocalBounds().toFloat().removeFromTop (22.0f).reduced (14.0f, 4.0f),
                    juce::Justification::centredRight, false);

        g.setColour (juce::Colour (Theme::gold).withAlpha (0.22f));
        g.fillRect (bounds.getX() + 12.0f, rank == Rank::hero ? 31.0f : 27.0f, bounds.getWidth() - 24.0f, 0.8f);

        g.setColour (juce::Colour (Theme::goldDim).withAlpha (0.2f));
        for (int i = 0; i < 5; ++i)
            g.fillRect (bounds.getRight() - 28.0f + static_cast<float> (i) * 4.0f,
                        bounds.getBottom() - 11.0f, 1.2f, 5.0f);
    }

    juce::Rectangle<int> content() const
    {
        return getLocalBounds().reduced (8, 6).withTrimmedTop (rank == Rank::hero ? 30 : 26);
    }

private:
    juce::String title;
    juce::String subtitle;
    Glyph glyph;
    juce::String code;
    Rank rank;
};

enum class KnobSize { primary, secondary, micro };

class AethrKnob : public juce::Component
{
public:
    AethrKnob (juce::AudioProcessorValueTreeState& state, const juce::String& paramId,
               juce::String nameToUse, KnobSize sizeToUse, juce::String hintToUse)
        : name (std::move (nameToUse)), size (sizeToUse), hint (std::move (hintToUse))
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        slider.setRotaryParameters (startAngle, endAngle, true);
        slider.setSliderSnapsToMousePosition (false);
        slider.setMouseDragSensitivity (size == KnobSize::micro ? 140 : 200);
        slider.setOpaque (false);
        slider.setColour (juce::Slider::rotarySliderFillColourId, juce::Colours::transparentBlack);
        slider.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colours::transparentBlack);
        slider.setColour (juce::Slider::thumbColourId, juce::Colours::transparentBlack);
        slider.setPopupMenuEnabled (true);
        slider.setScrollWheelEnabled (true);
        slider.onValueChange = [this]
        {
            glow = 1.0f;
            repaint();
        };
        addAndMakeVisible (slider);

        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramId, slider);
        parameter = state.getParameter (paramId);

        if (parameter != nullptr)
        {
            slider.setDoubleClickReturnValue (true, parameter->convertFrom0to1 (parameter->getDefaultValue()));
            slider.setTooltip (name + "\n\n" + hint);
        }

        setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    }

    void setLabel (juce::String newName)
    {
        if (name == newName)
            return;

        name = std::move (newName);

        if (parameter != nullptr)
            slider.setTooltip (name + "\n\n" + hint);

        repaint();
    }

    void setEnabledVisual (bool shouldBeEnabled)
    {
        setEnabled (shouldBeEnabled);
        slider.setEnabled (shouldBeEnabled);
        setAlpha (shouldBeEnabled ? 1.0f : 0.38f);
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        const auto labelH = size == KnobSize::micro ? 11.0f : 13.0f;
        const auto valueH = size == KnobSize::micro ? 11.0f : 13.0f;

        g.setColour (juce::Colour (Theme::gold).withAlpha (0.9f));
        g.setFont (labelFont (size == KnobSize::primary ? 10.0f : 9.0f));
        g.drawText (name.toUpperCase(), bounds.removeFromTop (labelH), juce::Justification::centred, false);

        auto readout = bounds.removeFromBottom (valueH);
        g.setColour (juce::Colour (Theme::text).withAlpha (0.88f));
        g.setFont (valueFont (size == KnobSize::micro ? 9.0f : 10.0f));
        g.drawText (currentText(), readout, juce::Justification::centred, false);

        const auto ringArea = bounds.reduced (size == KnobSize::primary ? 2.0f : 4.0f);
        const auto radius = juce::jmin (ringArea.getWidth(), ringArea.getHeight())
                            * (size == KnobSize::primary ? 0.48f : 0.42f);
        const auto centre = ringArea.getCentre();
        const auto t = static_cast<float> (slider.getNormalisableRange().convertTo0to1 (slider.getValue()));
        const auto toAngle = startAngle + t * (endAngle - startAngle);
        const auto ticks = size == KnobSize::primary ? 24 : (size == KnobSize::secondary ? 16 : 10);

        g.setColour (juce::Colour (0xff050608).withAlpha (0.85f));
        g.fillEllipse (centre.x - radius - 4.0f, centre.y - radius - 4.0f,
                       (radius + 4.0f) * 2.0f, (radius + 4.0f) * 2.0f);

        if (glow > 0.02f || size == KnobSize::primary)
        {
            g.setColour (juce::Colour (Theme::gold).withAlpha (0.07f + 0.10f * glow));
            g.fillEllipse (centre.x - radius - 6.0f, centre.y - radius - 6.0f,
                           (radius + 6.0f) * 2.0f, (radius + 6.0f) * 2.0f);
        }

        g.setColour (juce::Colour (Theme::goldDim).withAlpha (0.62f));
        for (int i = 0; i <= ticks; ++i)
        {
            const auto a = startAngle + (static_cast<float> (i) / static_cast<float> (ticks)) * (endAngle - startAngle);
            const auto major = (i % 4 == 0);
            const auto inner = centre.getPointOnCircumference (radius + 1.0f, a);
            const auto outer = centre.getPointOnCircumference (radius + (major ? 5.0f : 2.6f), a);
            g.setColour (juce::Colour (Theme::gold).withAlpha (major ? 0.7f : 0.35f));
            g.drawLine ({ inner, outer }, major ? 1.0f : 0.7f);
        }

        juce::Path track;
        track.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, startAngle, endAngle, true);
        g.setColour (juce::Colour (Theme::goldDeep));
        g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        if (t > 0.002f)
        {
            juce::Path arc;
            arc.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, startAngle, toAngle, true);
            g.setColour (juce::Colour (Theme::gold).withAlpha (0.95f));
            g.strokePath (arc, juce::PathStrokeType (1.7f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        juce::ColourGradient disc (juce::Colour (0xff32343a), centre.x, centre.y - radius * 0.75f,
                                   juce::Colour (0xff0a0b0d), centre.x, centre.y + radius * 0.85f, false);
        g.setGradientFill (disc);
        g.fillEllipse (centre.x - radius + 5.5f, centre.y - radius + 5.5f,
                       (radius - 5.5f) * 2.0f, (radius - 5.5f) * 2.0f);

        g.setColour (juce::Colour (Theme::goldDim).withAlpha (0.55f));
        g.drawEllipse (centre.x - radius + 5.5f, centre.y - radius + 5.5f,
                       (radius - 5.5f) * 2.0f, (radius - 5.5f) * 2.0f, 1.05f);
        g.setColour (juce::Colour (Theme::gold).withAlpha (0.12f));
        g.drawEllipse (centre.x - radius + 8.0f, centre.y - radius + 8.0f,
                       (radius - 8.0f) * 2.0f, (radius - 8.0f) * 2.0f, 0.8f);

        g.setColour (juce::Colour (Theme::text).withAlpha (0.14f));
        g.fillEllipse (centre.x - radius * 0.24f, centre.y - radius * 0.58f,
                       radius * 0.58f, radius * 0.28f);

        const auto needle = centre.getPointOnCircumference (radius - 8.5f, toAngle);
        g.setColour (juce::Colour (Theme::gold));
        g.drawLine (centre.x, centre.y, needle.x, needle.y, 1.2f);
        g.fillEllipse (needle.x - 1.8f, needle.y - 1.8f, 3.6f, 3.6f);
        g.setColour (juce::Colour (Theme::text).withAlpha (0.85f));
        g.fillEllipse (centre.x - 1.9f, centre.y - 1.9f, 3.8f, 3.8f);
    }

    void resized() override
    {
        const auto trimTop = size == KnobSize::micro ? 11 : 13;
        const auto trimBottom = size == KnobSize::micro ? 11 : 13;
        slider.setBounds (getLocalBounds().withTrimmedTop (trimTop).withTrimmedBottom (trimBottom));
    }

    void decayGlow (float amount) noexcept
    {
        if (glow > 0.001f)
        {
            glow *= amount;
            repaint();
        }
    }

private:
    juce::String currentText() const
    {
        if (parameter != nullptr)
            return parameter->getCurrentValueAsText();

        return juce::String (slider.getValue(), 2);
    }

    static constexpr float startAngle = juce::MathConstants<float>::pi * 1.18f;
    static constexpr float endAngle   = juce::MathConstants<float>::pi * 2.82f;

    juce::String name;
    KnobSize size;
    juce::String hint;
    juce::Slider slider;
    juce::RangedAudioParameter* parameter { nullptr };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    float glow { 0.0f };
};

class AethrCombo : public juce::Component
{
public:
    AethrCombo (juce::AudioProcessorValueTreeState& state,
                const juce::String& paramId,
                juce::String nameToUse,
                juce::String hint = {})
        : name (std::move (nameToUse))
    {
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (paramId)))
            combo.addItemList (choice->choices, 1);

        combo.setColour (juce::ComboBox::backgroundColourId, juce::Colour (Theme::glass));
        combo.setColour (juce::ComboBox::textColourId, juce::Colour (Theme::text));
        combo.setColour (juce::ComboBox::outlineColourId, juce::Colour (Theme::goldDim));
        combo.setColour (juce::ComboBox::arrowColourId, juce::Colour (Theme::gold));

        if (hint.isNotEmpty())
            combo.setTooltip (name + "\n\n" + hint);
        else
            combo.setTooltip (name);

        addAndMakeVisible (combo);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, paramId, combo);
    }

    juce::ComboBox& getCombo() noexcept { return combo; }

    void setEnabledVisual (bool shouldBeEnabled)
    {
        setEnabled (shouldBeEnabled);
        combo.setEnabled (shouldBeEnabled);
        setAlpha (shouldBeEnabled ? 1.0f : 0.38f);
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (juce::Colour (Theme::gold).withAlpha (0.85f));
        g.setFont (labelFont (9.0f));
        g.drawText (name.toUpperCase(), getLocalBounds().toFloat().removeFromTop (12.0f).reduced (2.0f, 0.0f),
                    juce::Justification::centredLeft, false);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced (1, 0);
        bounds.removeFromTop (12);
        combo.setBounds (bounds.removeFromTop (20));
    }

private:
    juce::String name;
    juce::ComboBox combo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
};

class AethrPlate : public juce::TextButton
{
public:
    AethrPlate() = default;
    explicit AethrPlate (juce::String text) : juce::TextButton (std::move (text)) {}

    void paintButton (juce::Graphics& g, bool highlighted, bool down) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (0.5f);
        auto fill = juce::Colour (Theme::panelInner);

        if (getToggleState() || down)
            fill = juce::Colour (0xff2a2418);
        else if (highlighted)
            fill = fill.brighter (0.06f);

        g.setColour (fill);
        g.fillRect (bounds);
        g.setColour (juce::Colour (Theme::gold).withAlpha (0.12f));
        g.fillRect (bounds.getX() + 1.0f, bounds.getY() + 1.0f, bounds.getWidth() - 2.0f, 1.0f);
        g.setColour (getToggleState() ? juce::Colour (Theme::gold) : juce::Colour (Theme::goldDim));
        g.drawRect (bounds, 1.0f);
        drawCornerMotif (g, bounds, 3.5f);

        g.setColour (juce::Colour (Theme::text).withAlpha (down ? 1.0f : 0.88f));
        g.setFont (labelFont (10.0f));
        g.drawText (getButtonText(), bounds, juce::Justification::centred, false);
    }
};

class AethrToggle : public juce::Component
{
public:
    AethrToggle (juce::AudioProcessorValueTreeState& state,
                 const juce::String& paramId,
                 juce::String nameToUse,
                 juce::String hint = {})
    {
        button.setButtonText (nameToUse.toUpperCase());
        button.setClickingTogglesState (true);

        if (hint.isNotEmpty())
            button.setTooltip (nameToUse + "\n\n" + hint);
        else
            button.setTooltip (nameToUse);

        addAndMakeVisible (button);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, paramId, button);
    }

    void setEnabledVisual (bool shouldBeEnabled)
    {
        setEnabled (shouldBeEnabled);
        button.setEnabled (shouldBeEnabled);
        setAlpha (shouldBeEnabled ? 1.0f : 0.38f);
    }

    void resized() override { button.setBounds (getLocalBounds().reduced (4, 6)); }

private:
    AethrPlate button { "TOGGLE" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
};

class AethrPresetField : public juce::Component,
                         public juce::SettableTooltipClient
{
public:
    std::function<void()> onOpenBrowser;
    std::function<void()> onPrev;
    std::function<void()> onNext;

    void setText (juce::String nameToUse, juce::String categoryToUse)
    {
        name = std::move (nameToUse);
        category = std::move (categoryToUse);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (0.5f);
        juce::ColourGradient glass (juce::Colour (0xff16141a), bounds.getX(), bounds.getY(),
                                    juce::Colour (0xff0a0b0e), bounds.getX(), bounds.getBottom(), false);
        g.setGradientFill (glass);
        g.fillRect (bounds);
        g.setColour (juce::Colour (Theme::goldDim));
        g.drawRect (bounds, 1.0f);
        drawCornerMotif (g, bounds, 5.0f);

        g.setColour (juce::Colour (Theme::textSecondary));
        g.setFont (labelFont (8.0f));
        g.drawText (category.toUpperCase(), bounds.removeFromTop (11.0f), juce::Justification::centred, false);

        // Palatino has no ◀/▶ glyphs — UTF-8 lead bytes render as "â". Draw geometry instead.
        g.setColour (juce::Colour (Theme::gold));
        g.setFont (titleFont (14.0f));
        g.drawText (name, bounds, juce::Justification::centred, false);

        const auto midY = bounds.getCentreY();
        const auto arrow = 4.5f;
        juce::Path left;
        left.addTriangle (bounds.getX() + 10.0f, midY,
                          bounds.getX() + 10.0f + arrow, midY - arrow,
                          bounds.getX() + 10.0f + arrow, midY + arrow);
        juce::Path right;
        right.addTriangle (bounds.getRight() - 10.0f, midY,
                           bounds.getRight() - 10.0f - arrow, midY - arrow,
                           bounds.getRight() - 10.0f - arrow, midY + arrow);
        g.fillPath (left);
        g.fillPath (right);
    }

    void mouseUp (const juce::MouseEvent& event) override
    {
        if (! event.mouseWasClicked())
            return;

        if (event.x < getWidth() / 5 && onPrev)
            onPrev();
        else if (event.x > getWidth() * 4 / 5 && onNext)
            onNext();
        else if (onOpenBrowser)
            onOpenBrowser();
    }

private:
    juce::String name { "Init" };
    juce::String category { "Physical" };
};

inline void placeRow (juce::Rectangle<int> area, const std::vector<juce::Component*>& items)
{
    if (items.empty() || area.getWidth() <= 0)
        return;

    const auto w = juce::jmax (1, area.getWidth() / static_cast<int> (items.size()));

    for (int i = 0; i < static_cast<int> (items.size()); ++i)
        if (items[static_cast<std::size_t> (i)] != nullptr)
            items[static_cast<std::size_t> (i)]->setBounds (area.getX() + i * w, area.getY(), w, area.getHeight());
}

} // namespace aethr::ui
