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
        setDescription ("Choose a factory preset. Scroll the list. Press Escape to close.");
    }

    bool keyPressed (const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::escapeKey)
        {
            if (onDismiss)
                onDismiss();

            return true;
        }

        if (key == juce::KeyPress::upKey || key == juce::KeyPress::downKey)
        {
            const auto delta = key == juce::KeyPress::upKey ? -rowHeight : rowHeight;
            scrollBy (static_cast<float> (delta));
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
        g.drawText ("FACTORY BY CATEGORY  ·  SCROLL  ·  ESC TO CLOSE", panel.removeFromTop (16.0f),
                    juce::Justification::centred, false);

        listBounds = panel.reduced (16.0f, 8.0f).toNearestInt();
        clampScroll();

        g.reduceClipRegion (listBounds);

        auto y = static_cast<float> (listBounds.getY()) - scrollY;

        for (int i = 0; i < static_cast<int> (rows.size()); ++i)
        {
            const auto row = juce::Rectangle<float> (static_cast<float> (listBounds.getX()),
                                                     y,
                                                     static_cast<float> (listBounds.getWidth()),
                                                     static_cast<float> (rowHeight));
            y += static_cast<float> (rowHeight);

            if (row.getBottom() < static_cast<float> (listBounds.getY())
                || row.getY() > static_cast<float> (listBounds.getBottom()))
                continue;

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

        // Scroll hint when content overflows
        if (contentHeight() > listBounds.getHeight())
        {
            g.setColour (juce::Colour (Theme::gold).withAlpha (0.35f));
            const auto track = juce::Rectangle<float> (static_cast<float> (listBounds.getRight() - 3),
                                                       static_cast<float> (listBounds.getY()),
                                                       2.0f,
                                                       static_cast<float> (listBounds.getHeight()));
            const auto thumbH = juce::jmax (18.0f, track.getHeight() * track.getHeight()
                                                   / static_cast<float> (contentHeight()));
            const auto maxScroll = static_cast<float> (juce::jmax (0, contentHeight() - listBounds.getHeight()));
            const auto thumbY = track.getY() + (maxScroll > 0.0f ? (scrollY / maxScroll) * (track.getHeight() - thumbH)
                                                                 : 0.0f);
            g.fillRect (track.getX(), thumbY, track.getWidth(), thumbH);
        }
    }

    void mouseMove (const juce::MouseEvent& event) override
    {
        hover = indexAt (event.getPosition());
        repaint();
    }

    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override
    {
        scrollBy (-wheel.deltaY * static_cast<float> (rowHeight) * 3.0f);
    }

    void mouseUp (const juce::MouseEvent& event) override
    {
        const auto index = indexAt (event.getPosition());

        if (index >= 0 && ! rows[static_cast<std::size_t> (index)].isHeader && onChoose)
            onChoose (rows[static_cast<std::size_t> (index)].factoryIndex);
        else if (! listBounds.contains (event.getPosition()) && onDismiss)
            onDismiss();
    }

    void visibilityChanged() override
    {
        if (isVisible())
        {
            scrollY = 0.0f;
            grabKeyboardFocus();
            repaint();
        }
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

    int contentHeight() const noexcept
    {
        return static_cast<int> (rows.size()) * rowHeight;
    }

    void clampScroll()
    {
        const auto maxScroll = static_cast<float> (juce::jmax (0, contentHeight() - listBounds.getHeight()));
        scrollY = juce::jlimit (0.0f, maxScroll, scrollY);
    }

    void scrollBy (float delta)
    {
        scrollY += delta;
        clampScroll();
        repaint();
    }

    int indexAt (juce::Point<int> p) const
    {
        if (! listBounds.contains (p) || rows.empty())
            return -1;

        const auto localY = p.y - listBounds.getY() + static_cast<int> (scrollY);
        const auto index = localY / rowHeight;

        if (index < 0 || index >= static_cast<int> (rows.size()))
            return -1;

        return index;
    }

    static constexpr int rowHeight = 22;

    std::vector<Row> rows;
    juce::Rectangle<int> listBounds;
    float scrollY { 0.0f };
    int hover { -1 };
};

} // namespace aethr::ui
