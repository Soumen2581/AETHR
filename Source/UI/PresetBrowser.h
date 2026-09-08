#pragma once

#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>

#include "Presets/PresetManager.h"
#include "UI/Theme.h"

namespace aethr::ui
{

class AethrPresetBrowser : public juce::Component
{
public:
    std::function<void (int)> onChoose;
    std::function<void()> onDismiss;

    AethrPresetBrowser()
    {
        setOpaque (false);
        for (int i = 0; i < presets::numFactoryPresets(); ++i)
            names.add (presets::factory[static_cast<std::size_t> (i)].name);
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRect (getLocalBounds());

        auto panel = getLocalBounds().reduced (getWidth() / 5, getHeight() / 7).toFloat();
        drawTechnicalFrame (g, panel, Rank::hero);

        g.setColour (juce::Colour (Theme::gold));
        g.setFont (titleFont (16.0f));
        g.drawText ("LIBRARY", panel.removeFromTop (28.0f), juce::Justification::centred, false);

        g.setColour (juce::Colour (Theme::textSecondary));
        g.setFont (labelFont (10.0f));
        g.drawText ("FACTORY INSTRUMENTS", panel.removeFromTop (16.0f), juce::Justification::centred, false);

        auto list = panel.reduced (16.0f, 8.0f);
        const auto rowH = list.getHeight() / (float) juce::jmax (1, names.size());

        for (int i = 0; i < names.size(); ++i)
        {
            auto row = list.removeFromTop (rowH);
            const bool hot = hover == i;
            if (hot)
            {
                g.setColour (juce::Colour (Theme::gold).withAlpha (0.12f));
                g.fillRect (row);
            }

            g.setColour (juce::Colour (hot ? Theme::gold : Theme::text));
            g.setFont (labelFont (12.0f));
            g.drawText (names[i], row.reduced (12.0f, 0.0f), juce::Justification::centredLeft, false);
            g.setColour (juce::Colour (Theme::textSecondary));
            g.setFont (labelFont (10.0f));
            g.drawText (presets::factory[static_cast<std::size_t> (i)].category, row.reduced (12.0f, 0.0f),
                        juce::Justification::centredRight, false);
        }
    }

    void mouseMove (const juce::MouseEvent& event) override
    {
        hover = indexAt (event.getPosition());
        repaint();
    }

    void mouseUp (const juce::MouseEvent& event) override
    {
        const auto index = indexAt (event.getPosition());

        if (index >= 0 && onChoose)
            onChoose (index);
        else if (onDismiss)
            onDismiss();
    }

private:
    int indexAt (juce::Point<int> p) const
    {
        auto panel = getLocalBounds().reduced (getWidth() / 5, getHeight() / 7);
        panel.removeFromTop (44);
        auto list = panel.reduced (16, 8);

        if (! list.contains (p) || names.isEmpty())
            return -1;

        const auto rowH = juce::jmax (1, list.getHeight() / names.size());
        return juce::jlimit (0, names.size() - 1, (p.y - list.getY()) / rowH);
    }

    juce::StringArray names;
    int hover { -1 };
};

} // namespace aethr::ui
