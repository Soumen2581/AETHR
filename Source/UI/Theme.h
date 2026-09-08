#pragma once

#include <cmath>
#include <juce_gui_basics/juce_gui_basics.h>

namespace aethr::ui
{

/** Single source of truth for the V2 visual language. */
struct Theme
{
    static constexpr juce::uint32 background    = 0xff07080a;
    static constexpr juce::uint32 chassis       = 0xff0b0c0f;
    static constexpr juce::uint32 panel         = 0xff101114;
    static constexpr juce::uint32 panelInner    = 0xff16151a;
    static constexpr juce::uint32 glass         = 0xff0c0d11;
    static constexpr juce::uint32 border        = 0xff5c4a2c;
    static constexpr juce::uint32 gold          = 0xffc9b07a;
    static constexpr juce::uint32 goldDim       = 0xff7a6640;
    static constexpr juce::uint32 goldDeep      = 0xff3a3224;
    static constexpr juce::uint32 text          = 0xffe8e0d2;
    static constexpr juce::uint32 textSecondary = 0xff9a9080;
    static constexpr juce::uint32 highlight     = 0xffd8c49a;
    static constexpr juce::uint32 amber         = 0xffc4a46a;
};

inline juce::Font logoFont (float height)
{
    return juce::FontOptions ("Palatino", height, juce::Font::italic);
}

inline juce::Font titleFont (float height)
{
    return juce::FontOptions ("Palatino", height, juce::Font::plain);
}

inline juce::Font labelFont (float height)
{
    return juce::FontOptions (juce::Font::getDefaultSansSerifFontName(), height, juce::Font::plain);
}

inline juce::Font valueFont (float height)
{
    return juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), height, juce::Font::plain);
}

enum class Glyph
{
    impulse,
    resonator,
    body,
    filter,
    drive,
    layers,
    macros,
    modulation,
    delay,
    motion,
    chamber,
    master
};

enum class Rank { hero, major, minor };

inline void drawCornerMotif (juce::Graphics& g, juce::Rectangle<float> bounds, float length = 8.0f)
{
    const auto x0 = bounds.getX();
    const auto y0 = bounds.getY();
    const auto x1 = bounds.getRight();
    const auto y1 = bounds.getBottom();
    const auto slash = length * 0.45f;

    g.setColour (juce::Colour (Theme::gold).withAlpha (0.72f));
    g.drawLine (x0, y0, x0 + length, y0, 1.15f);
    g.drawLine (x0, y0, x0, y0 + length, 1.15f);
    g.drawLine (x0 + length, y0, x0 + length - slash, y0 + slash, 0.9f);
    g.drawLine (x1, y0, x1 - length, y0, 1.15f);
    g.drawLine (x1, y0, x1, y0 + length, 1.15f);
    g.drawLine (x1 - length, y0, x1 - length + slash, y0 + slash, 0.9f);
    g.drawLine (x0, y1, x0 + length, y1, 1.15f);
    g.drawLine (x0, y1, x0, y1 - length, 1.15f);
    g.drawLine (x0 + length, y1, x0 + length - slash, y1 - slash, 0.9f);
    g.drawLine (x1, y1, x1 - length, y1, 1.15f);
    g.drawLine (x1, y1, x1, y1 - length, 1.15f);
    g.drawLine (x1 - length, y1, x1 - length + slash, y1 - slash, 0.9f);
}

inline void drawCornerMarks (juce::Graphics& g, juce::Rectangle<float> bounds, float length = 7.0f)
{
    drawCornerMotif (g, bounds, length);
}

inline void drawEdgeTicks (juce::Graphics& g, juce::Rectangle<float> bounds, int count = 12)
{
    g.setColour (juce::Colour (Theme::goldDim).withAlpha (0.35f));
    const auto step = bounds.getWidth() / static_cast<float> (juce::jmax (2, count));

    for (int i = 1; i < count; ++i)
    {
        const auto x = bounds.getX() + step * static_cast<float> (i);
        const auto tall = (i % 3 == 0);
        g.drawLine (x, bounds.getY() + 1.0f, x, bounds.getY() + (tall ? 5.0f : 3.0f), 0.7f);
        g.drawLine (x, bounds.getBottom() - 1.0f, x, bounds.getBottom() - (tall ? 5.0f : 3.0f), 0.7f);
    }
}

inline void drawRivet (juce::Graphics& g, juce::Point<float> centre, float radius = 2.4f)
{
    juce::ColourGradient well (juce::Colour (0xff1c1810), centre.x, centre.y - radius,
                               juce::Colour (0xff050506), centre.x, centre.y + radius, false);
    g.setGradientFill (well);
    g.fillEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
    g.setColour (juce::Colour (Theme::goldDim).withAlpha (0.85f));
    g.drawEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, 0.8f);
    g.setColour (juce::Colour (Theme::gold).withAlpha (0.35f));
    g.drawLine (centre.x - radius * 0.45f, centre.y - radius * 0.45f,
                centre.x + radius * 0.45f, centre.y + radius * 0.45f, 0.7f);
}

inline void drawGlassWell (juce::Graphics& g, juce::Rectangle<float> bounds)
{
    juce::ColourGradient glass (juce::Colour (0xff14151a), bounds.getX(), bounds.getY(),
                                juce::Colour (0xff07080b), bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill (glass);
    g.fillRect (bounds);
    g.setColour (juce::Colour (0x22000000));
    g.fillRect (bounds.withHeight (3.0f));
    g.setColour (juce::Colour (Theme::gold).withAlpha (0.08f));
    g.drawRect (bounds.reduced (0.5f), 1.0f);
    g.setColour (juce::Colour (Theme::goldDeep));
    g.drawRect (bounds, 1.0f);
}

inline void drawTechnicalFrame (juce::Graphics& g, juce::Rectangle<float> bounds, Rank rank = Rank::major)
{
    const auto hero = rank == Rank::hero;
    const auto minor = rank == Rank::minor;

    juce::ColourGradient plate (juce::Colour (hero ? 0xff16141a : 0xff121318), bounds.getX(), bounds.getY(),
                                juce::Colour (0xff0c0d10), bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill (plate);
    g.fillRect (bounds);

    g.setColour (juce::Colour (Theme::gold).withAlpha (hero ? 0.22f : 0.08f));
    g.fillRect (bounds.getX() + 1.0f, bounds.getY() + 1.0f, bounds.getWidth() - 2.0f, 1.0f);

    g.setColour (juce::Colour (Theme::goldDeep).withAlpha (minor ? 0.55f : 0.95f));
    g.drawRect (bounds, 1.0f);
    g.setColour (juce::Colour (Theme::goldDim).withAlpha (hero ? 0.95f : 0.55f));
    g.drawRect (bounds.reduced (hero ? 2.5f : 3.5f), hero ? 1.2f : 0.8f);

    drawCornerMotif (g, bounds.reduced (1.0f), hero ? 11.0f : 7.0f);
    drawEdgeTicks (g, bounds.reduced (8.0f, 0.0f), hero ? 18 : 10);

    drawRivet (g, { bounds.getX() + 6.0f, bounds.getY() + 6.0f });
    drawRivet (g, { bounds.getRight() - 6.0f, bounds.getY() + 6.0f });
    drawRivet (g, { bounds.getX() + 6.0f, bounds.getBottom() - 6.0f });
    drawRivet (g, { bounds.getRight() - 6.0f, bounds.getBottom() - 6.0f });
}

inline void drawBusNode (juce::Graphics& g, juce::Point<float> a, juce::Point<float> b)
{
    g.setColour (juce::Colour (Theme::goldDim).withAlpha (0.55f));
    g.drawLine (a.x, a.y, b.x, b.y, 0.8f);
    g.setColour (juce::Colour (Theme::gold).withAlpha (0.45f));
    g.fillEllipse (a.x - 1.6f, a.y - 1.6f, 3.2f, 3.2f);
    g.fillEllipse (b.x - 1.6f, b.y - 1.6f, 3.2f, 3.2f);
}

inline void drawGlyph (juce::Graphics& g, Glyph glyph, juce::Rectangle<float> area)
{
    g.setColour (juce::Colour (Theme::gold).withAlpha (0.85f));
    const auto c = area.getCentre();
    const auto r = juce::jmin (area.getWidth(), area.getHeight()) * 0.42f;

    switch (glyph)
    {
        case Glyph::impulse:
            g.drawLine (c.x, c.y + r, c.x, c.y - r, 1.1f);
            g.drawLine (c.x - r * 0.55f, c.y - r * 0.15f, c.x, c.y - r, 1.1f);
            g.drawLine (c.x + r * 0.55f, c.y - r * 0.15f, c.x, c.y - r, 1.1f);
            break;
        case Glyph::resonator:
        {
            juce::Path wave;
            wave.startNewSubPath (c.x - r, c.y);
            for (int i = 1; i <= 16; ++i)
            {
                const auto t = (float) i / 16.0f;
                wave.lineTo (c.x - r + t * r * 2.0f, c.y + std::sin (t * juce::MathConstants<float>::twoPi * 2.0f) * r * 0.45f);
            }
            g.strokePath (wave, juce::PathStrokeType (1.0f));
            break;
        }
        case Glyph::body:
            g.drawEllipse (c.x - r, c.y - r * 0.72f, r * 2.0f, r * 1.44f, 1.0f);
            g.drawEllipse (c.x - r * 0.45f, c.y - r * 0.32f, r * 0.9f, r * 0.64f, 0.8f);
            break;
        case Glyph::filter:
        {
            juce::Path curve;
            curve.startNewSubPath (c.x - r, c.y + r * 0.4f);
            curve.quadraticTo (c.x - r * 0.1f, c.y - r, c.x + r * 0.15f, c.y);
            curve.lineTo (c.x + r, c.y + r * 0.55f);
            g.strokePath (curve, juce::PathStrokeType (1.0f));
            break;
        }
        case Glyph::drive:
        {
            juce::Path curve;
            curve.startNewSubPath (c.x - r, c.y + r * 0.6f);
            curve.cubicTo (c.x - r * 0.2f, c.y + r * 0.6f, c.x, c.y - r, c.x + r, c.y - r * 0.55f);
            g.strokePath (curve, juce::PathStrokeType (1.0f));
            break;
        }
        case Glyph::layers:
            g.drawEllipse (c.x - r * 0.85f, c.y - r * 0.35f, r * 0.7f, r * 0.7f, 1.0f);
            g.drawEllipse (c.x + r * 0.15f, c.y - r * 0.35f, r * 0.7f, r * 0.7f, 1.0f);
            break;
        case Glyph::macros:
            for (int i = 0; i < 8; ++i)
            {
                const auto a = (float) i * juce::MathConstants<float>::twoPi / 8.0f;
                g.fillEllipse (c.x + std::cos (a) * r - 1.2f, c.y + std::sin (a) * r - 1.2f, 2.4f, 2.4f);
            }
            break;
        case Glyph::modulation:
            g.drawEllipse (c.x - r, c.y - r * 0.55f, r * 2.0f, r * 1.1f, 0.9f);
            g.fillEllipse (c.x + r * 0.55f - 1.6f, c.y - 1.6f, 3.2f, 3.2f);
            break;
        case Glyph::delay:
            g.drawLine (c.x - r, c.y - r * 0.25f, c.x + r, c.y - r * 0.25f, 1.0f);
            g.drawLine (c.x - r, c.y + r * 0.25f, c.x + r * 0.45f, c.y + r * 0.25f, 1.0f);
            break;
        case Glyph::motion:
            g.drawEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f, 0.8f);
            g.drawEllipse (c.x - r * 0.55f, c.y - r * 0.35f, r * 1.1f, r * 0.7f, 0.8f);
            break;
        case Glyph::chamber:
            g.drawRect (c.x - r * 0.7f, c.y - r * 0.45f, r * 1.4f, r * 0.9f, 1.0f);
            g.drawLine (c.x - r * 0.7f, c.y - r * 0.45f, c.x - r * 0.2f, c.y - r, 0.8f);
            g.drawLine (c.x + r * 0.7f, c.y - r * 0.45f, c.x + r * 0.2f, c.y - r, 0.8f);
            break;
        case Glyph::master:
            g.drawLine (c.x - r, c.y, c.x + r * 0.15f, c.y, 1.0f);
            g.drawLine (c.x + r * 0.15f, c.y - r * 0.55f, c.x + r * 0.15f, c.y + r * 0.55f, 1.0f);
            g.drawLine (c.x + r * 0.15f, c.y - r * 0.55f, c.x + r, c.y, 1.0f);
            g.drawLine (c.x + r * 0.15f, c.y + r * 0.55f, c.x + r, c.y, 1.0f);
            break;
    }
}

inline void drawAethrMark (juce::Graphics& g, juce::Rectangle<float> area)
{
    const auto c = area.getCentre();
    const auto r = juce::jmin (area.getWidth(), area.getHeight()) * 0.46f;

    g.setColour (juce::Colour (Theme::gold).withAlpha (0.12f));
    g.fillEllipse (c.x - r, c.y - r * 0.72f, r * 2.0f, r * 1.44f);
    g.setColour (juce::Colour (Theme::goldDim).withAlpha (0.95f));
    g.drawEllipse (c.x - r, c.y - r * 0.72f, r * 2.0f, r * 1.44f, 1.2f);
    g.drawEllipse (c.x - r * 0.62f, c.y - r * 0.44f, r * 1.24f, r * 0.88f, 0.9f);
    g.setColour (juce::Colour (Theme::goldDim).withAlpha (0.45f));
    for (int i = 0; i < 5; ++i)
    {
        const auto a = -1.2f + static_cast<float> (i) * 0.6f;
        g.fillEllipse (c.x + std::sin (a) * r * 0.55f - 1.1f, c.y + std::cos (a) * r * 0.28f - 1.1f, 2.2f, 2.2f);
    }
    g.setColour (juce::Colour (Theme::gold));
    g.drawLine (c.x, c.y + r * 0.82f, c.x, c.y - r * 0.82f, 1.25f);
    g.fillEllipse (c.x - 1.7f, c.y - 1.7f, 3.4f, 3.4f);
}

inline void drawWordmark (juce::Graphics& g, juce::Rectangle<float> bounds)
{
    g.setColour (juce::Colour (Theme::gold));
    g.setFont (logoFont (bounds.getHeight() * 0.82f));
    g.drawText ("AETHR", bounds, juce::Justification::centredLeft, false);

    g.setColour (juce::Colour (Theme::goldDim).withAlpha (0.8f));
    g.fillRect (bounds.getX() + 2.0f, bounds.getBottom() - 3.0f, 86.0f, 0.8f);
}

inline juce::Image makeNoiseTexture()
{
    juce::Image image (juce::Image::ARGB, 256, 256, true);
    juce::Random rng { 0xae71u };

    for (int y = 0; y < 256; ++y)
    {
        for (int x = 0; x < 256; ++x)
        {
            const auto v = static_cast<juce::uint8> (rng.nextInt (18));
            image.setPixelAt (x, y, juce::Colour::fromRGBA (v, v, v, 18));
        }
    }

    return image;
}

inline void paintChassis (juce::Graphics& g, juce::Rectangle<int> bounds, float headerGlow = 0.0f)
{
    g.fillAll (juce::Colour (Theme::background));

    juce::ColourGradient vignette (juce::Colour (0xff17141c),
                                   static_cast<float> (bounds.getCentreX()) * 0.42f, 36.0f,
                                   juce::Colour (Theme::background),
                                   static_cast<float> (bounds.getCentreX()),
                                   static_cast<float> (bounds.getHeight()),
                                   true);
    g.setGradientFill (vignette);
    g.fillRect (bounds);

    static const juce::Image noise = makeNoiseTexture();
    g.setOpacity (0.28f);
    for (int y = bounds.getY(); y < bounds.getBottom(); y += noise.getHeight())
        for (int x = bounds.getX(); x < bounds.getRight(); x += noise.getWidth())
            g.drawImageAt (noise, x, y);
    g.setOpacity (1.0f);

    const auto cx = static_cast<float> (bounds.getCentreX());
    const auto cy = 86.0f;
    g.setColour (juce::Colour (Theme::gold).withAlpha (0.035f));
    for (int i = 0; i < 7; ++i)
    {
        const auto radius = 40.0f + static_cast<float> (i) * 22.0f;
        g.drawEllipse (cx - radius, cy - radius * 0.22f, radius * 2.0f, radius * 0.44f, 0.8f);
    }

    g.setColour (juce::Colour (Theme::gold).withAlpha (0.055f));
    juce::Path standing;
    standing.startNewSubPath (48.0f, static_cast<float> (bounds.getHeight()) * 0.52f);
    for (int i = 1; i <= 64; ++i)
    {
        const auto u = static_cast<float> (i) / 64.0f;
        standing.lineTo (48.0f + u * (static_cast<float> (bounds.getWidth()) - 96.0f),
                         static_cast<float> (bounds.getHeight()) * 0.52f
                             + std::sin (u * juce::MathConstants<float>::pi * 6.0f) * 18.0f);
    }
    g.strokePath (standing, juce::PathStrokeType (0.9f));

    g.setColour (juce::Colour (Theme::gold).withAlpha (0.07f));
    const float traceY = 22.0f;
    g.drawLine (178.0f, traceY, 340.0f, traceY, 0.8f);
    g.drawLine (340.0f, traceY, 358.0f, 48.0f, 0.8f);
    g.drawLine (178.0f, 12.0f, 178.0f, 52.0f, 0.8f);
    g.drawLine (210.0f, 12.0f, 210.0f, 22.0f, 0.8f);
    g.fillEllipse (356.0f, 46.0f, 3.4f, 3.4f);
    g.drawLine (static_cast<float> (bounds.getWidth()) - 240.0f, 18.0f,
                static_cast<float> (bounds.getWidth()) - 80.0f, 18.0f, 0.8f);
    g.drawLine (static_cast<float> (bounds.getWidth()) - 80.0f, 18.0f,
                static_cast<float> (bounds.getWidth()) - 64.0f, 40.0f, 0.8f);

    juce::Random stars { 0x5eedu };
    for (int i = 0; i < 64; ++i)
    {
        const auto x = 36.0f + stars.nextFloat() * (static_cast<float> (bounds.getWidth()) - 72.0f);
        const auto y = 8.0f + stars.nextFloat() * 58.0f;
        g.setColour (juce::Colour (Theme::text).withAlpha (0.05f + stars.nextFloat() * 0.07f));
        g.fillEllipse (x, y, 1.15f, 1.15f);
    }

    if (headerGlow > 0.001f)
    {
        g.setColour (juce::Colour (Theme::gold).withAlpha (0.10f * headerGlow));
        g.fillRect (0.0f, 0.0f, static_cast<float> (bounds.getWidth()), 70.0f);
    }

    auto outer = bounds.toFloat().reduced (3.0f);
    g.setColour (juce::Colour (Theme::goldDim).withAlpha (0.9f));
    g.drawRect (outer, 1.3f);
    g.setColour (juce::Colour (Theme::gold).withAlpha (0.22f));
    g.drawRect (outer.reduced (3.0f), 1.0f);
    drawCornerMotif (g, outer.reduced (1.0f), 14.0f);
    drawEdgeTicks (g, outer.reduced (18.0f, 2.0f), 32);

    g.setColour (juce::Colour (Theme::goldDim).withAlpha (0.4f));
    g.setFont (valueFont (8.0f));
    g.drawText ("MODEL  /  KARPLUS  /  MODAL",
                juce::Rectangle<float> (18.0f, static_cast<float> (bounds.getHeight()) - 36.0f, 260.0f, 10.0f),
                juce::Justification::centredLeft, false);
}

} // namespace aethr::ui
