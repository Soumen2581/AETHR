#pragma once

#include <functional>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

#include "Presets/PresetManager.h"
#include "UI/Theme.h"

namespace aethr::ui
{

/** Factory library overlay grouped by category. Esc or click-outside dismisses. */
class AethrPresetBrowser : public juce::Component
{
public:
    std::function<void (int)> onChoose;
    std::function<void()> onDismiss;

    AethrPresetBrowser()
    {
        setOpaque (false);
        setWantsKeyboardFocus (true);
        rebuildRows();
        setTitle ("AETHR factory preset library");
        setDescription ("Choose a factory preset. Press Escape to close.");
    }

    bool keyPressed (const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::escapeKey)
        {
            if (onDismiss)
                onDismiss();

            return true;
        }

        return false;
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
        g.drawText ("FACTORY BY CATEGORY  ·  ESC TO CLOSE", panel.removeFromTop (16.0f),
                    juce::Justification::centred, false);

        auto list = panel.reduced (16.0f, 8.0f);
        const auto rowH = list.getHeight() / static_cast<float> (juce::jmax (1, static_cast<int> (rows.size())));

        for (int i = 0; i < static_cast<int> (rows.size()); ++i)
        {
            auto row = list.removeFromTop (rowH);
            const auto& entry = rows[static_cast<std::size_t> (i)];

            if (entry.isHeader)
            {
                g.setColour (juce::Colour (Theme::goldDim).withAlpha (0.9f));
                g.setFont (labelFont (9.0f));
                g.drawText (entry.label.toUpperCase(), row.reduced (8.0f, 0.0f),
                            juce::Justification::centredLeft, false);
                g.setColour (juce::Colour (Theme::gold).withAlpha (0.2f));
                g.fillRect (row.getX() + 8.0f, row.getBottom() - 1.0f, row.getWidth() - 16.0f, 0.8f);
                continue;
            }

            const bool hot = hover == i;

            if (hot)
            {
                g.setColour (juce::Colour (Theme::gold).withAlpha (0.12f));
                g.fillRect (row);
            }

            g.setColour (juce::Colour (hot ? Theme::gold : Theme::text));
            g.setFont (labelFont (12.0f));
            g.drawText (entry.label, row.reduced (16.0f, 0.0f), juce::Justification::centredLeft, false);
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

        if (index >= 0 && ! rows[static_cast<std::size_t> (index)].isHeader && onChoose)
            onChoose (rows[static_cast<std::size_t> (index)].factoryIndex);
        else if (onDismiss)
            onDismiss();
    }

    void visibilityChanged() override
    {
        if (isVisible())
            grabKeyboardFocus();
    }

private:
    struct Row
    {
        bool isHeader { false };
        int factoryIndex { -1 };
        juce::String label;
    };

    void rebuildRows()
    {
        rows.clear();
        juce::StringArray seenCategories;

        for (int i = 0; i < presets::numFactoryPresets(); ++i)
        {
            const auto category = juce::String (presets::factory[static_cast<std::size_t> (i)].category);

            if (! seenCategories.contains (category))
            {
                seenCategories.add (category);
                rows.push_back ({ true, -1, category });

                for (int j = 0; j < presets::numFactoryPresets(); ++j)
                {
                    if (category == presets::factory[static_cast<std::size_t> (j)].category)
                        rows.push_back ({ false, j, presets::factory[static_cast<std::size_t> (j)].name });
                }
            }
        }
    }

    int indexAt (juce::Point<int> p) const
    {
        auto panel = getLocalBounds().reduced (getWidth() / 5, getHeight() / 7);
        panel.removeFromTop (44);
        auto list = panel.reduced (16, 8);

        if (! list.contains (p) || rows.empty())
            return -1;

        const auto rowH = juce::jmax (1, list.getHeight() / static_cast<int> (rows.size()));
        return juce::jlimit (0, static_cast<int> (rows.size()) - 1, (p.y - list.getY()) / rowH);
    }

    std::vector<Row> rows;
    int hover { -1 };
};

} // namespace aethr::ui
