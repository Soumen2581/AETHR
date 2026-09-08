#pragma once

#include <array>
#include <cmath>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "Engine/EngineType.h"
#include "Parameters/ParameterIDs.h"
#include "UI/Theme.h"

namespace aethr::ui
{

/**
    Physical taxonomy of engines. Not a ComboBox.

    Every engine is selectable and sounds distinct: Karplus–Strong character
    for string-family models, a modal bank for bell/plate/membrane/cavity/
    spectral/modal, and a hybrid of both.
*/
class AethrEngineStrip : public juce::Component,
                         public juce::SettableTooltipClient,
                         private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit AethrEngineStrip (juce::AudioProcessorValueTreeState& stateToUse)
        : state (stateToUse)
    {
        setOpaque (false);
        setTitle ("Engine selector");
        setDescription ("Choose a synthesis engine. Physical, synthetic, and experimental families.");
        state.addParameterListener (params::engine::type, this);
        selected = currentIndex();
        updateTooltip();
    }

    ~AethrEngineStrip() override
    {
        state.removeParameterListener (params::engine::type, this);
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (juce::Colour (Theme::glass));
        g.fillRect (bounds);
        g.setColour (juce::Colour (Theme::goldDim).withAlpha (0.55f));
        g.drawRect (bounds, 1.0f);

        auto inner = bounds.reduced (8.0f, 4.0f);
        const auto cellW = inner.getWidth() / static_cast<float> (engine::numEngineTypes + 3);

        auto drawFamily = [&] (const char* label, juce::Rectangle<float> area)
        {
            g.setColour (juce::Colour (Theme::goldDim).withAlpha (0.85f));
            g.setFont (labelFont (8.0f));
            g.drawText (label, area, juce::Justification::centredLeft, false);
        };

        auto x = inner.getX();
        const auto y = inner.getY();
        const auto h = inner.getHeight();

        drawFamily ("PHYSICAL", { x, y, cellW, h });
        x += cellW;

        for (int i = 0; i < engine::numEngineTypes; ++i)
        {
            const auto& info = engine::engineCatalogue[i];

            if (i == static_cast<int> (engine::EngineType::waveguide))
            {
                drawFamily ("SYNTHETIC", { x, y, cellW, h });
                x += cellW;
            }
            else if (i == static_cast<int> (engine::EngineType::granular))
            {
                drawFamily ("EXPERIMENTAL", { x, y, cellW, h });
                x += cellW;
            }

            const auto cell = juce::Rectangle<float> (x, y, cellW, h).reduced (1.0f, 2.0f);
            cells[static_cast<std::size_t> (i)] = cell.toNearestInt();

            const bool isSelected = i == selected;
            const auto alpha = info.implemented ? 1.0f : 0.42f;

            if (isSelected)
            {
                g.setColour (juce::Colour (Theme::goldDeep).withAlpha (0.9f));
                g.fillRect (cell);
                g.setColour (juce::Colour (Theme::gold).withAlpha (alpha));
                g.drawRect (cell, 1.0f);
            }

            g.setColour (juce::Colour (isSelected ? Theme::gold : Theme::textSecondary).withAlpha (alpha));
            g.setFont (labelFont (isSelected ? 10.0f : 9.0f));
            g.drawText (info.name, cell, juce::Justification::centred, false);
            x += cellW;
        }
    }

    void mouseUp (const juce::MouseEvent& event) override
    {
        if (! event.mouseWasClicked())
            return;

        for (int i = 0; i < engine::numEngineTypes; ++i)
        {
            if (cells[static_cast<std::size_t> (i)].contains (event.getPosition()))
            {
                if (auto* parameter = state.getParameter (params::engine::type))
                    parameter->setValueNotifyingHost (
                        parameter->convertTo0to1 (static_cast<float> (i)));
                break;
            }
        }
    }

private:
    void parameterChanged (const juce::String&, float) override
    {
        selected = currentIndex();
        updateTooltip();
        repaint();
    }

    void updateTooltip()
    {
        const auto& info = engine::infoFor (engine::engineTypeFromIndex (selected));
        const char* family = "Physical";

        if (info.family == engine::EngineFamily::synthetic)
            family = "Synthetic";
        else if (info.family == engine::EngineFamily::experimental)
            family = "Experimental";

        setTooltip (juce::String (family) + " · " + info.name
                    + "\n\nSelects the active synthesis engine. Character models share Karplus–Strong and modal cores.");
    }

    [[nodiscard]] int currentIndex() const
    {
        if (auto* raw = state.getRawParameterValue (params::engine::type))
            return juce::jlimit (0, engine::numEngineTypes - 1,
                                 static_cast<int> (std::lround (static_cast<double> (raw->load()))));

        return engine::defaultEngineIndex;
    }

    juce::AudioProcessorValueTreeState& state;
    int selected { engine::defaultEngineIndex };
    std::array<juce::Rectangle<int>, static_cast<std::size_t> (engine::numEngineTypes)> cells {};
};

} // namespace aethr::ui
