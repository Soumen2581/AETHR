#pragma once

#include <array>
#include <cmath>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "Parameters/ParameterIDs.h"
#include "UI/Controls.h"
#include "UI/Theme.h"

namespace aethr::ui
{

/**
    Tempo-synced arpeggiator and 16-step gate.

    Lives under the engine taxonomy so it is always reachable without burying
    another row of synthesis controls.
*/
class AethrArpStrip : public juce::Component,
                      private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit AethrArpStrip (juce::AudioProcessorValueTreeState& stateToUse)
        : state (stateToUse)
    {
        setOpaque (false);

        enable.setButtonText ("ARP");
        enable.setClickingTogglesState (true);
        addAndMakeVisible (enable);
        enableAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            state, params::arp::enable, enable);

        latch.setButtonText ("LATCH");
        latch.setClickingTogglesState (true);
        addAndMakeVisible (latch);
        latchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            state, params::arp::latch, latch);

        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (params::arp::mode)))
            mode.addItemList (choice->choices, 1);

        addAndMakeVisible (mode);
        modeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            state, params::arp::mode, mode);

        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (params::arp::division)))
            division.addItemList (choice->choices, 1);

        addAndMakeVisible (division);
        divisionAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            state, params::arp::division, division);

        octaves.setSliderStyle (juce::Slider::LinearBar);
        octaves.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 28, 16);
        octaves.setColour (juce::Slider::trackColourId, juce::Colour (Theme::goldDeep));
        octaves.setColour (juce::Slider::textBoxTextColourId, juce::Colour (Theme::text));
        octaves.setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (Theme::goldDim));
        addAndMakeVisible (octaves);
        octavesAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            state, params::arp::octaves, octaves);

        gate.setSliderStyle (juce::Slider::LinearBar);
        gate.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        gate.setColour (juce::Slider::trackColourId, juce::Colour (Theme::goldDeep));
        addAndMakeVisible (gate);
        gateAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            state, params::arp::gate, gate);

        swing.setSliderStyle (juce::Slider::LinearBar);
        swing.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        swing.setColour (juce::Slider::trackColourId, juce::Colour (Theme::goldDeep));
        addAndMakeVisible (swing);
        swingAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            state, params::arp::swing, swing);

        for (int i = 0; i < 16; ++i)
        {
            auto& step = steps[static_cast<std::size_t> (i)];
            step.setClickingTogglesState (false);
            step.setButtonText (juce::String (i + 1));
            step.onClick = [this, i] { toggleStep (i); };
            addAndMakeVisible (step);
        }

        state.addParameterListener (params::arp::pattern, this);
        refreshSteps();
    }

    ~AethrArpStrip() override
    {
        state.removeParameterListener (params::arp::pattern, this);
    }

    void setChaseStep (int step) noexcept
    {
        const auto next = juce::jlimit (0, 15, step);

        if (next == chase)
            return;

        chase = next;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (juce::Colour (Theme::glass));
        g.fillRect (bounds);
        g.setColour (juce::Colour (Theme::goldDim).withAlpha (0.55f));
        g.drawRect (bounds, 1.0f);

        g.setColour (juce::Colour (Theme::goldDim).withAlpha (0.9f));
        g.setFont (labelFont (8.0f));
        g.drawText ("SEQ", juce::Rectangle<float> (8.0f, 4.0f, 28.0f, 10.0f),
                    juce::Justification::centredLeft, false);
        g.drawText ("MODE", juce::Rectangle<float> (static_cast<float> (mode.getX()), 3.0f,
                                                    static_cast<float> (mode.getWidth()), 10.0f),
                    juce::Justification::centredLeft, false);
        g.drawText ("DIV", juce::Rectangle<float> (static_cast<float> (division.getX()), 3.0f,
                                                   static_cast<float> (division.getWidth()), 10.0f),
                    juce::Justification::centredLeft, false);
        g.drawText ("OCT", juce::Rectangle<float> (static_cast<float> (octaves.getX()), 3.0f,
                                                   static_cast<float> (octaves.getWidth()), 10.0f),
                    juce::Justification::centredLeft, false);
        g.drawText ("GATE", juce::Rectangle<float> (static_cast<float> (gate.getX()), 3.0f,
                                                    static_cast<float> (gate.getWidth()), 10.0f),
                    juce::Justification::centredLeft, false);
        g.drawText ("SWING", juce::Rectangle<float> (static_cast<float> (swing.getX()), 3.0f,
                                                     static_cast<float> (swing.getWidth()), 10.0f),
                    juce::Justification::centredLeft, false);

        if (chase >= 0 && chase < 16)
        {
            const auto cell = steps[static_cast<std::size_t> (chase)].getBounds().toFloat().expanded (1.0f);
            g.setColour (juce::Colour (Theme::gold).withAlpha (0.35f));
            g.drawRect (cell, 1.2f);
        }
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (6, 4);
        enable.setBounds (area.removeFromLeft (52).withTrimmedTop (10));
        area.removeFromLeft (6);
        latch.setBounds (area.removeFromLeft (58).withTrimmedTop (10));
        area.removeFromLeft (8);

        auto placeLabeled = [&area] (juce::Component& component, int width)
        {
            auto cell = area.removeFromLeft (width);
            cell.removeFromTop (10);
            component.setBounds (cell);
            area.removeFromLeft (6);
        };

        placeLabeled (mode, 88);
        placeLabeled (division, 72);
        placeLabeled (octaves, 64);
        placeLabeled (gate, 72);
        placeLabeled (swing, 72);

        area.removeFromLeft (4);
        const auto stepW = juce::jmax (16, area.getWidth() / 16);

        for (int i = 0; i < 16; ++i)
            steps[static_cast<std::size_t> (i)].setBounds (area.removeFromLeft (stepW).reduced (1, 2));
    }

private:
    void parameterChanged (const juce::String&, float) override
    {
        refreshSteps();
    }

    [[nodiscard]] int currentPattern() const
    {
        if (auto* parameter = state.getParameter (params::arp::pattern))
            return juce::jlimit (0, 65535,
                                 juce::roundToInt (parameter->convertFrom0to1 (parameter->getValue())));

        return 65535;
    }

    void toggleStep (int index)
    {
        auto* parameter = state.getParameter (params::arp::pattern);

        if (parameter == nullptr)
            return;

        const auto bit = 1 << index;
        const auto next = currentPattern() ^ bit;
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (static_cast<float> (next)));
    }

    void refreshSteps()
    {
        const auto pattern = currentPattern();

        for (int i = 0; i < 16; ++i)
        {
            const bool on = (pattern & (1 << i)) != 0;
            steps[static_cast<std::size_t> (i)].setToggleState (on, juce::dontSendNotification);
        }

        repaint();
    }

    juce::AudioProcessorValueTreeState& state;
    AethrPlate enable { "ARP" };
    AethrPlate latch { "LATCH" };
    juce::ComboBox mode;
    juce::ComboBox division;
    juce::Slider octaves;
    juce::Slider gate;
    juce::Slider swing;
    std::array<AethrPlate, 16> steps;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enableAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> latchAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> divisionAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> octavesAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gateAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> swingAttachment;
    int chase { 0 };
};

} // namespace aethr::ui
