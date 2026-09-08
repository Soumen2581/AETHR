#pragma once

#include <cmath>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "UI/Theme.h"
#include "Core/TempoSync.h"

namespace aethr::ui
{

inline float param01 (juce::AudioProcessorValueTreeState& state, const char* id, float fallback = 0.0f)
{
    if (auto* p = state.getParameter (id))
        return p->getValue();

    return fallback;
}

inline float paramValue (juce::AudioProcessorValueTreeState& state, const char* id, float fallback)
{
    if (auto* parameter = state.getParameter (id))
        return parameter->convertFrom0to1 (parameter->getValue());

    return fallback;
}

inline int paramChoice (juce::AudioProcessorValueTreeState& state, const char* id, int fallback)
{
    if (auto* raw = state.getRawParameterValue (id))
        return static_cast<int> (std::lround (static_cast<double> (raw->load())));

    return fallback;
}

inline bool paramOn (juce::AudioProcessorValueTreeState& state, const char* id)
{
    if (auto* raw = state.getRawParameterValue (id))
        return raw->load() >= 0.5f;

    return false;
}

inline float syncedOrFreeHz (juce::AudioProcessorValueTreeState& state,
                             const char* syncId,
                             const char* divisionId,
                             const char* rateId,
                             float fallbackHz,
                             double hostBpm)
{
    if (paramOn (state, syncId))
        return static_cast<float> (sync::hertz (paramChoice (state, divisionId, sync::defaultDivisionIndex), hostBpm));

    return paramValue (state, rateId, fallbackHz);
}

class ExciterView : public juce::Component
{
public:
    explicit ExciterView (juce::AudioProcessorValueTreeState& s) : state (s) { setOpaque (false); }

    void setEnergy (float e) noexcept { energy = e; }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (2.0f);
        drawGlassWell (g, r);

        const auto type = param01 (state, "exciter.type");
        const auto burst = param01 (state, "exciter.burst.time", 0.3f);
        const auto colour = param01 (state, "exciter.colour", 0.5f);
        const auto t = (float) juce::Time::getMillisecondCounterHiRes() * 0.001f;
        const auto amp = 0.18f + energy * 0.72f;

        juce::Path path;
        const auto mid = r.getCentreY();
        path.startNewSubPath (r.getX(), mid);

        for (int i = 1; i <= 48; ++i)
        {
            const auto x = r.getX() + r.getWidth() * (float) i / 48.0f;
            const auto u = (float) i / 48.0f;
            float y = 0.0f;

            if (type < 0.2f)
                y = u < burst * 0.35f + 0.04f ? std::exp (-u * 18.0f) * std::sin (u * 70.0f) : 0.0f;
            else if (type < 0.5f)
                y = (u < 0.12f ? 1.0f - u * 8.0f : 0.0f) * (0.6f + 0.4f * colour);
            else
                y = (juce::Random (0x31u + (juce::uint32) (u * 40.0f)).nextFloat() - 0.5f) * (0.4f + colour);

            y *= amp * std::cos (t * 9.0f + u * 4.0f) * 0.25f + amp;
            path.lineTo (x, mid - y * r.getHeight() * 0.42f);
        }

        g.setColour (juce::Colour (Theme::gold).withAlpha (0.75f));
        g.strokePath (path, juce::PathStrokeType (1.05f));
    }

private:
    juce::AudioProcessorValueTreeState& state;
    float energy { 0.0f };
};

class ResonatorView : public juce::Component
{
public:
    explicit ResonatorView (juce::AudioProcessorValueTreeState& s) : state (s) { setOpaque (false); }

    void setState (float e, float hz) noexcept { energy = e; noteHz = hz; }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (2.0f);
        drawGlassWell (g, r);
        r = r.reduced (4.0f);

        const auto decay = param01 (state, "resonator.decay.time", 0.4f);
        const auto damp = param01 (state, "resonator.damping", 0.4f);
        const auto bright = param01 (state, "resonator.brightness", 0.6f);
        const auto t = static_cast<float> (juce::Time::getMillisecondCounterHiRes() * 0.001);
        const auto modes = 3 + static_cast<int> (std::round (bright * 6.0f));
        const auto amp = energy * (0.35f + decay * 0.55f);
        const auto left = r.getX() + 10.0f;
        const auto right = r.getRight() - 10.0f;
        const auto mid = r.getCentreY();

        g.setColour (juce::Colour (Theme::goldDim).withAlpha (0.35f));
        for (int i = 1; i < 8; ++i)
        {
            const auto x = left + (right - left) * static_cast<float> (i) / 8.0f;
            g.drawLine (x, r.getY() + 4.0f, x, r.getBottom() - 4.0f, 0.5f);
        }

        g.setColour (juce::Colour (Theme::goldDim).withAlpha (0.55f));
        g.fillRect (left - 4.0f, mid - 10.0f, 4.0f, 20.0f);
        g.fillRect (right, mid - 10.0f, 4.0f, 20.0f);

        g.setColour (juce::Colour (Theme::gold).withAlpha (0.12f));
        g.drawLine (left, mid, right, mid, 0.7f);

        for (int m = 1; m <= modes; ++m)
        {
            juce::Path partial;
            partial.startNewSubPath (left, mid);
            const auto harm = 1.0f / static_cast<float> (m);
            for (int i = 1; i <= 72; ++i)
            {
                const auto u = static_cast<float> (i) / 72.0f;
                const auto y = harm * amp * std::sin (u * juce::MathConstants<float>::pi * static_cast<float> (m))
                               * std::sin (t * (5.5f + noteHz * 0.018f) * static_cast<float> (m))
                               * (1.0f - damp * 0.55f);
                partial.lineTo (left + u * (right - left), mid - y * r.getHeight() * 0.42f);
            }
            g.setColour (juce::Colour (Theme::gold).withAlpha (0.12f + harm * 0.35f));
            g.strokePath (partial, juce::PathStrokeType (m == 1 ? 1.35f : 0.7f));
        }

        auto footer = r.removeFromBottom (13.0f);
        g.setColour (juce::Colour (Theme::textSecondary).withAlpha (0.75f));
        g.setFont (valueFont (9.0f));
        g.drawText (noteHz > 1.0f ? juce::String (noteHz, 1) + " Hz" : "IDLE",
                    footer, juce::Justification::centredLeft, false);
        g.drawText ("STRING  /  LOOP", footer, juce::Justification::centredRight, false);
    }

private:
    juce::AudioProcessorValueTreeState& state;
    float energy { 0.0f };
    float noteHz { 0.0f };
};

class BodyView : public juce::Component
{
public:
    explicit BodyView (juce::AudioProcessorValueTreeState& s) : state (s) { setOpaque (false); }

    void setEnergy (float e) noexcept { energy = e; }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (3.0f);
        drawGlassWell (g, r);

        const auto modes = 2 + (int) std::round (param01 (state, "body.modes", 0.4f) * 6.0f);
        const auto mix = param01 (state, "body.mix");
        const auto c = r.getCentre();
        const auto rad = juce::jmin (r.getWidth(), r.getHeight()) * 0.36f;
        const auto t = (float) juce::Time::getMillisecondCounterHiRes() * 0.001f;

        g.setColour (juce::Colour (Theme::goldDim).withAlpha (0.55f));
        g.drawEllipse (c.x - rad, c.y - rad, rad * 2.0f, rad * 2.0f, 0.9f);
        g.drawEllipse (c.x - rad * 0.55f, c.y - rad * 0.55f, rad * 1.1f, rad * 1.1f, 0.7f);

        for (int i = 0; i < modes; ++i)
        {
            const auto a = (float) i / (float) juce::jmax (1, modes) * juce::MathConstants<float>::twoPi + t * 0.15f;
            const auto pulse = 0.75f + energy * 0.25f * std::sin (t * 8.0f + (float) i);
            const auto p = c.getPointOnCircumference (rad * pulse * (0.7f + mix * 0.25f), a);
            g.setColour (juce::Colour (Theme::gold).withAlpha (0.45f + mix * 0.4f));
            g.fillEllipse (p.x - 2.4f, p.y - 2.4f, 4.8f, 4.8f);
        }
    }

private:
    juce::AudioProcessorValueTreeState& state;
    float energy { 0.0f };
};

class FilterView : public juce::Component
{
public:
    explicit FilterView (juce::AudioProcessorValueTreeState& s) : state (s) { setOpaque (false); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (2.0f);
        g.setColour (juce::Colour (Theme::glass));
        g.fillRect (r);

        const auto cutoff = param01 (state, "fx.filter.cutoff", 0.7f);
        const auto res = param01 (state, "fx.filter.res", 0.2f);
        const auto type = param01 (state, "fx.filter.type");

        juce::Path path;
        path.startNewSubPath (r.getX(), r.getBottom() - 3.0f);

        for (int i = 0; i <= 32; ++i)
        {
            const auto x = (float) i / 32.0f;
            float mag = 1.0f;
            const auto d = x - cutoff;

            if (type < 0.34f)
                mag = 1.0f / (1.0f + std::exp ((x - cutoff) * 18.0f));
            else if (type < 0.67f)
                mag = 1.0f / (1.0f + std::exp ((cutoff - x) * 18.0f));
            else
                mag = 1.0f / (1.0f + 40.0f * d * d);

            mag += res * 0.55f * std::exp (-d * d * 80.0f);
            path.lineTo (r.getX() + x * r.getWidth(), r.getBottom() - 3.0f - mag * (r.getHeight() - 8.0f) * 0.75f);
        }

        g.setColour (juce::Colour (Theme::gold).withAlpha (0.8f));
        g.strokePath (path, juce::PathStrokeType (1.1f));
    }

private:
    juce::AudioProcessorValueTreeState& state;
};

class DriveView : public juce::Component
{
public:
    explicit DriveView (juce::AudioProcessorValueTreeState& s) : state (s) { setOpaque (false); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (2.0f);
        g.setColour (juce::Colour (Theme::glass));
        g.fillRect (r);

        const auto drive = 0.4f + param01 (state, "fx.sat.drive") * 3.5f;
        juce::Path path;
        path.startNewSubPath (r.getX() + 2.0f, r.getBottom() - 2.0f);

        for (int i = 0; i <= 28; ++i)
        {
            const auto x = (float) i / 28.0f * 2.0f - 1.0f;
            const auto y = std::tanh (x * drive);
            path.lineTo (r.getX() + 2.0f + (x + 1.0f) * 0.5f * (r.getWidth() - 4.0f),
                         r.getCentreY() - y * (r.getHeight() * 0.42f));
        }

        g.setColour (juce::Colour (Theme::gold).withAlpha (0.8f));
        g.strokePath (path, juce::PathStrokeType (1.1f));
        g.setColour (juce::Colour (Theme::goldDim).withAlpha (0.4f));
        g.drawLine (r.getX(), r.getCentreY(), r.getRight(), r.getCentreY(), 0.6f);
    }

private:
    juce::AudioProcessorValueTreeState& state;
};

class LayerView : public juce::Component
{
public:
    explicit LayerView (juce::AudioProcessorValueTreeState& s) : state (s) { setOpaque (false); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (2.0f);
        g.setColour (juce::Colour (Theme::glass));
        g.fillRect (r);

        const auto enabled = param01 (state, "layer.b.enable") > 0.5f;
        const auto level = param01 (state, "layer.b.level", 0.4f);
        const auto detune = param01 (state, "layer.b.detune");
        const auto t = (float) juce::Time::getMillisecondCounterHiRes() * 0.001f;
        const auto c = r.getCentre();

        g.setColour (juce::Colour (Theme::goldDim));
        g.drawEllipse (c.x - 18.0f, c.y - 18.0f, 36.0f, 36.0f, 0.8f);
        g.setColour (juce::Colour (Theme::gold));
        g.fillEllipse (c.x - 3.0f, c.y - 14.0f, 6.0f, 6.0f);
        g.setFont (labelFont (8.0f));
        g.drawText ("A", juce::Rectangle<float> (c.x - 16.0f, c.y - 26.0f, 32.0f, 10.0f),
                    juce::Justification::centred, false);

        if (enabled)
        {
            const auto a = t * (1.2f + detune);
            const auto p = juce::Point<float> (c.x + std::cos (a) * (16.0f + level * 8.0f),
                                               c.y + std::sin (a) * (10.0f + level * 6.0f));
            g.setColour (juce::Colour (Theme::gold).withAlpha (0.85f));
            g.drawLine (c.x, c.y - 11.0f, p.x, p.y, 0.7f);
            g.fillEllipse (p.x - 3.0f, p.y - 3.0f, 6.0f, 6.0f);
            g.setFont (labelFont (8.0f));
            g.drawText ("B", juce::Rectangle<float> (p.x - 10.0f, p.y + 5.0f, 20.0f, 10.0f),
                        juce::Justification::centred, false);
        }
    }

private:
    juce::AudioProcessorValueTreeState& state;
};

class LfoView : public juce::Component
{
public:
    LfoView (juce::AudioProcessorValueTreeState& s, const char* waveId, const char* rateId,
             const char* syncId, const char* divisionId)
        : state (s), wave (waveId), rate (rateId), sync (syncId), division (divisionId)
    {
        setOpaque (false);
    }

    void setHostTempoBpm (double bpm) noexcept { hostBpm = bpm; }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (juce::Colour (Theme::glass));
        g.fillRect (r);

        const auto wave01 = param01 (state, wave);
        const auto rateHz = syncedOrFreeHz (state, sync, division, rate, 0.8f, hostBpm);
        const auto t = (float) juce::Time::getMillisecondCounterHiRes() * 0.001f * rateHz;

        juce::Path path;
        path.startNewSubPath (r.getX(), r.getCentreY());

        for (int i = 1; i <= 32; ++i)
        {
            const auto u = (float) i / 32.0f;
            const auto ph = std::fmod (u + t, 1.0f);
            float y = 0.0f;
            if (wave01 < 0.14f)
                y = std::sin (ph * juce::MathConstants<float>::twoPi);
            else if (wave01 < 0.28f)
                y = ph < 0.5f ? ph * 4.0f - 1.0f : 3.0f - ph * 4.0f;
            else if (wave01 < 0.42f)
                y = ph * 2.0f - 1.0f;
            else if (wave01 < 0.7f)
                y = ph < 0.5f ? 1.0f : -1.0f;
            else
                y = std::sin (ph * 9.0f);

            path.lineTo (r.getX() + u * r.getWidth(), r.getCentreY() - y * r.getHeight() * 0.38f);
        }

        g.setColour (juce::Colour (Theme::gold).withAlpha (0.85f));
        g.strokePath (path, juce::PathStrokeType (1.0f));
    }

private:
    juce::AudioProcessorValueTreeState& state;
    const char* wave;
    const char* rate;
    const char* sync;
    const char* division;
    double hostBpm { sync::defaultBpm };
};

class DelayView : public juce::Component
{
public:
    explicit DelayView (juce::AudioProcessorValueTreeState& s) : state (s) { setOpaque (false); }

    void setHostTempoBpm (double bpm) noexcept { hostBpm = bpm; }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (3.0f);
        g.setColour (juce::Colour (Theme::glass));
        g.fillRect (r);

        float l = param01 (state, "fx.delay.timel", 0.3f);
        float rr = param01 (state, "fx.delay.timer", 0.35f);

        if (paramOn (state, "fx.delay.sync"))
        {
            const auto tL = sync::delaySeconds (paramChoice (state, "fx.delay.divisionl", sync::defaultDelayDivisionL), hostBpm);
            const auto tR = sync::delaySeconds (paramChoice (state, "fx.delay.divisionr", sync::defaultDelayDivisionR), hostBpm);
            l = static_cast<float> (tL / 1.5);
            rr = static_cast<float> (tR / 1.5);
        }

        l = juce::jlimit (0.0f, 1.0f, l);
        rr = juce::jlimit (0.0f, 1.0f, rr);
        const auto yL = r.getY() + r.getHeight() * 0.35f;
        const auto yR = r.getY() + r.getHeight() * 0.65f;

        g.setColour (juce::Colour (Theme::goldDim));
        g.drawLine (r.getX() + 14.0f, yL, r.getRight() - 8.0f, yL, 0.8f);
        g.drawLine (r.getX() + 14.0f, yR, r.getRight() - 8.0f, yR, 0.8f);
        g.setColour (juce::Colour (Theme::gold));
        g.fillEllipse (r.getX() + 14.0f + l * (r.getWidth() - 30.0f) - 3.0f, yL - 3.0f, 6.0f, 6.0f);
        g.fillEllipse (r.getX() + 14.0f + rr * (r.getWidth() - 30.0f) - 3.0f, yR - 3.0f, 6.0f, 6.0f);
        g.setFont (labelFont (8.0f));
        g.drawText ("L", juce::Rectangle<float> (r.getX() + 2.0f, yL - 6.0f, 12.0f, 12.0f),
                    juce::Justification::centred, false);
        g.drawText ("R", juce::Rectangle<float> (r.getX() + 2.0f, yR - 6.0f, 12.0f, 12.0f),
                    juce::Justification::centred, false);
    }

private:
    juce::AudioProcessorValueTreeState& state;
    double hostBpm { sync::defaultBpm };
};

class MotionView : public juce::Component
{
public:
    explicit MotionView (juce::AudioProcessorValueTreeState& s) : state (s) { setOpaque (false); }

    void setHostTempoBpm (double bpm) noexcept { hostBpm = bpm; }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (2.0f);
        g.setColour (juce::Colour (Theme::glass));
        g.fillRect (r);
        const auto c = r.getCentre();
        const auto t = (float) juce::Time::getMillisecondCounterHiRes() * 0.001f;
        const auto chorus = syncedOrFreeHz (state, "fx.chorus.sync", "fx.chorus.division",
                                            "fx.chorus.rate", 0.8f, hostBpm);
        const auto phaser = syncedOrFreeHz (state, "fx.phaser.sync", "fx.phaser.division",
                                            "fx.phaser.rate", 0.3f, hostBpm);
        g.setColour (juce::Colour (Theme::goldDim));
        g.drawEllipse (c.x - 16.0f, c.y - 10.0f, 32.0f, 20.0f, 0.8f);
        const auto a = juce::Point<float> (c.x + std::cos (t * (1.0f + chorus)) * 16.0f,
                                          c.y + std::sin (t * (1.0f + chorus)) * 10.0f);
        const auto b = juce::Point<float> (c.x + std::cos (t * (0.7f + phaser) + 1.7f) * 10.0f,
                                          c.y + std::sin (t * (0.7f + phaser) + 1.7f) * 6.0f);
        g.setColour (juce::Colour (Theme::gold));
        g.fillEllipse (a.x - 2.2f, a.y - 2.2f, 4.4f, 4.4f);
        g.fillEllipse (b.x - 1.8f, b.y - 1.8f, 3.6f, 3.6f);
    }

private:
    juce::AudioProcessorValueTreeState& state;
    double hostBpm { sync::defaultBpm };
};

class ChamberView : public juce::Component
{
public:
    explicit ChamberView (juce::AudioProcessorValueTreeState& s) : state (s) { setOpaque (false); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (3.0f);
        g.setColour (juce::Colour (Theme::glass));
        g.fillRect (r);
        const auto size = 0.35f + param01 (state, "fx.reverb.size") * 0.5f;
        const auto width = 0.4f + param01 (state, "fx.reverb.width") * 0.5f;
        const auto cx = r.getCentreX();
        const auto cy = r.getCentreY() + 4.0f;
        const auto w = r.getWidth() * 0.28f * width;
        const auto d = r.getHeight() * 0.22f * size;
        juce::Path room;
        room.addQuadrilateral (cx - w, cy + d, cx + w, cy + d, cx + w * 0.55f, cy - d, cx - w * 0.55f, cy - d);
        g.setColour (juce::Colour (Theme::gold).withAlpha (0.7f));
        g.strokePath (room, juce::PathStrokeType (1.0f));
        g.setColour (juce::Colour (Theme::gold).withAlpha (0.25f));
        g.fillEllipse (cx - 2.0f, cy - 2.0f, 4.0f, 4.0f);
    }

private:
    juce::AudioProcessorValueTreeState& state;
};

class MeterView : public juce::Component
{
public:
    void setLevel (float l) noexcept { level = juce::jlimit (0.0f, 1.0f, l); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (juce::Colour (Theme::glass));
        g.fillRect (r);
        g.setColour (juce::Colour (Theme::goldDeep));
        g.drawRect (r, 1.0f);
        auto fill = r.reduced (2.0f);
        fill = fill.removeFromBottom (fill.getHeight() * level);
        g.setColour (juce::Colour (Theme::gold).withAlpha (0.85f));
        g.fillRect (fill);
    }

private:
    float level { 0.0f };
};

class ModulationMatrix : public juce::Component
{
public:
    explicit ModulationMatrix (juce::AudioProcessorValueTreeState& s) : state (s) { setOpaque (false); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (6.0f, 2.0f);
        const char* sources[] = { "LFO 1", "LFO 2", "ENV", "CHAOS" };
        const char* destIds[] = { "lfo1.dest", "lfo2.dest", "env.dest" };
        const char* destNames[] = { "OFF", "PITCH", "DECAY", "DAMP", "BRIGHT", "EXCITE",
                                    "STIFF", "FILTER", "DELAY", "ROOM", "LAYER", "DRIVE", "STEREO" };

        g.setFont (labelFont (8.0f));
        for (int i = 0; i < 4; ++i)
        {
            const auto y = r.getY() + 10.0f + (float) i * 13.0f;
            g.setColour (juce::Colour (Theme::goldDim));
            g.drawText (sources[i], juce::Rectangle<float> (r.getX(), y - 6.0f, 48.0f, 12.0f),
                        juce::Justification::centredLeft, false);
            g.drawLine (r.getX() + 50.0f, y, r.getRight() - 70.0f, y, 0.6f);
        }

        for (int i = 0; i < 3; ++i)
        {
            const auto dest = (int) std::round (param01 (state, destIds[i]) * 12.0f);
            const auto y = r.getY() + 10.0f + (float) i * 13.0f;
            g.setColour (juce::Colour (Theme::gold).withAlpha (dest > 0 ? 0.9f : 0.35f));
            g.drawLine (r.getX() + 50.0f, y, r.getRight() - 70.0f, y, dest > 0 ? 1.1f : 0.5f);
            g.drawText (destNames[juce::jlimit (0, 12, dest)],
                        juce::Rectangle<float> (r.getRight() - 68.0f, y - 6.0f, 66.0f, 12.0f),
                        juce::Justification::centredLeft, false);
        }
    }

private:
    juce::AudioProcessorValueTreeState& state;
};

} // namespace aethr::ui
