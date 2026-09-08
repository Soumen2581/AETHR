#include "PluginEditor.h"

#include <algorithm>
#include <functional>

#include "Core/Branding.h"
#include "Parameters/ParameterIDs.h"
#include "Parameters/ParameterLayout.h"
#include "Engine/EngineType.h"
#include "PluginProcessor.h"
#include "Presets/PresetManager.h"

namespace aethr
{

namespace
{
    constexpr int editorWidth = 1440;
    constexpr int editorHeight = 940;
    constexpr int refreshHz = 36;

    juce::Array<juce::Component*> childrenOf (juce::Component& parent)
    {
        juce::Array<juce::Component*> items;

        for (auto* child : parent.getChildren())
            if (child != nullptr)
                items.add (child);

        return items;
    }

    void place (juce::Component* component, juce::Rectangle<int> area)
    {
        if (component != nullptr)
            component->setBounds (area);
    }
}

AethrEditor::AethrEditor (AethrProcessor& processorToUse)
    : juce::AudioProcessorEditor (&processorToUse),
      aethrProcessor (processorToUse),
      engineStrip (processorToUse.getValueTreeState()),
      arpStrip (processorToUse.getValueTreeState()),
      exciterView (processorToUse.getValueTreeState()),
      resonatorView (processorToUse.getValueTreeState()),
      bodyView (processorToUse.getValueTreeState()),
      filterView (processorToUse.getValueTreeState()),
      driveView (processorToUse.getValueTreeState()),
      layerView (processorToUse.getValueTreeState()),
      lfo1View (processorToUse.getValueTreeState(), params::lfo1::wave, params::lfo1::rate,
                params::lfo1::sync, params::lfo1::division),
      lfo2View (processorToUse.getValueTreeState(), params::lfo2::wave, params::lfo2::rate,
                params::lfo2::sync, params::lfo2::division),
      delayView (processorToUse.getValueTreeState()),
      motionView (processorToUse.getValueTreeState()),
      chamberView (processorToUse.getValueTreeState()),
      modMatrix (processorToUse.getValueTreeState())
{
    setLookAndFeel (&lookAndFeel);
    setResizable (true, true);
    setResizeLimits (1100, 700, 1920, 1080);

    auto& state = aethrProcessor.getValueTreeState();

    presetField.onPrev = [this]
    {
        applyPreset ((currentPreset + presets::numFactoryPresets() - 1) % presets::numFactoryPresets());
    };
    presetField.onNext = [this]
    {
        applyPreset ((currentPreset + 1) % presets::numFactoryPresets());
    };
    presetField.onOpenBrowser = [this] { showBrowser (true); };
    addAndMakeVisible (presetField);
    presetField.setText (presets::factory[0].name, presets::factory[0].category);

    initButton.onClick = [this] { applyPreset (0); };
    saveButton.onClick = [this]
    {
        aethrProcessor.getUndoManager().beginNewTransaction ("AETHR save");
        headerPulse = 1.0f;
    };
    randomButton.onClick = [this]
    {
        const auto mode = static_cast<presets::RandomMode> (std::clamp (laboratoryBox.getSelectedItemIndex(), 0, 3));
        presets::randomize (aethrProcessor.getValueTreeState(), mode, rng);
        pulseLaboratory();
    };
    mutateButton.onClick = [this]
    {
        presets::mutate (aethrProcessor.getValueTreeState(), rng);
        headerPulse = 0.7f;
    };
    undoButton.onClick = [this] { aethrProcessor.getUndoManager().undo(); };
    redoButton.onClick = [this] { aethrProcessor.getUndoManager().redo(); };

    for (auto* button : { &initButton, &saveButton, &randomButton, &mutateButton, &undoButton, &redoButton })
        addAndMakeVisible (*button);

    laboratoryBox.addItemList ({ "Safe", "Musical", "Experimental", "Chaotic" }, 1);
    laboratoryBox.setSelectedId (2, juce::dontSendNotification);
    addAndMakeVisible (laboratoryBox);
    addAndMakeVisible (engineStrip);
    addAndMakeVisible (arpStrip);

    const auto addPanel = [this] (ui::AethrPanel& panel) { addAndMakeVisible (panel); };
    addPanel (exciterSection);
    addPanel (resonatorSection);
    addPanel (materialSection);
    addPanel (filterSection);
    addPanel (driveSection);
    addPanel (layerSection);
    addPanel (macroSection);
    addPanel (modSection);
    addPanel (delaySection);
    addPanel (motionSection);
    addPanel (reverbSection);
    addPanel (outputSection);

    const auto knob = [this, &state] (juce::Component& parent, const char* id, const juce::String& name,
                                      ui::KnobSize size, const juce::String& hint) -> ui::AethrKnob&
    {
        auto item = std::make_unique<ui::AethrKnob> (state, id, name, size, hint);
        parent.addAndMakeVisible (*item);
        knobs.push_back (std::move (item));
        return *knobs.back();
    };
    const auto menu = [this, &state] (juce::Component& parent, const char* id, const juce::String& name)
        -> ui::AethrCombo&
    {
        auto item = std::make_unique<ui::AethrCombo> (state, id, name);
        parent.addAndMakeVisible (*item);
        auto& ref = *item;
        combos.push_back (std::move (item));
        return ref;
    };
    const auto toggle = [this, &state] (juce::Component& parent, const char* id, const juce::String& name)
    {
        auto item = std::make_unique<ui::AethrToggle> (state, id, name);
        parent.addAndMakeVisible (*item);
        toggles.push_back (std::move (item));
    };

    exciterSection.addAndMakeVisible (exciterView);
    menu (exciterSection, params::exciter::type, "Source");
    knob (exciterSection, params::exciter::burstTime, "Burst", ui::KnobSize::secondary, "Excitation burst length.");
    knob (exciterSection, params::exciter::attack, "Attack", ui::KnobSize::secondary, "Impulse attack time.");
    knob (exciterSection, params::exciter::colour, "Colour", ui::KnobSize::secondary, "Spectral tilt of the strike.");
    knob (exciterSection, params::exciter::brightness, "Bright", ui::KnobSize::secondary, "High-frequency energy in the strike.");
    knob (exciterSection, params::exciter::level, "Level", ui::KnobSize::secondary, "Excitation amplitude.");
    knob (exciterSection, params::exciter::randomAmount, "Scatter", ui::KnobSize::micro, "Strike-to-strike variation.");
    knob (exciterSection, params::exciter::stereoSpread, "Spread", ui::KnobSize::micro, "Left/right excitation offset.");

    resonatorSection.addAndMakeVisible (resonatorView);
    knob (resonatorSection, params::resonator::feedback, "Feedback", ui::KnobSize::primary,
          "Karplus–Strong loop recirculation. 100% is the full decay-derived gain; lower values open the loop.");
    knob (resonatorSection, params::resonator::decayTime, "Decay", ui::KnobSize::primary, "T60 of the loop while a note is held.");
    knob (resonatorSection, params::resonator::damping, "Damping", ui::KnobSize::primary, "High-frequency loss per loop.");
    knob (resonatorSection, params::resonator::brightness, "Bright", ui::KnobSize::primary, "Loop brightness.");
    knob (resonatorSection, params::resonator::releaseTime, "Release", ui::KnobSize::secondary, "Release after note-off.");
    knob (resonatorSection, params::resonator::stiffness, "Stiffness", ui::KnobSize::secondary, "Inharmonic dispersion.");
    menu (resonatorSection, params::resonator::loopFilterMode, "Loop Filter");
    menu (resonatorSection, params::resonator::interpolation, "Interp");
    engineControlA = &knob (resonatorSection, params::engine::controlA, "Tension", ui::KnobSize::secondary,
                            "Engine-specific control A. The label follows the selected engine.");
    engineControlB = &knob (resonatorSection, params::engine::controlB, "Position", ui::KnobSize::secondary,
                            "Engine-specific control B. The label follows the selected engine.");
    engineControlC = &knob (resonatorSection, params::engine::controlC, "Loss", ui::KnobSize::secondary,
                            "Engine-specific control C. The label follows the selected engine.");
    engineControlD = &knob (resonatorSection, params::engine::controlD, "Pickup", ui::KnobSize::secondary,
                            "Engine-specific control D. The label follows the selected engine.");

    displayedEngine = aethrProcessor.getSelectedEngineType();
    {
        const auto names = engine::controlNamesFor (displayedEngine);
        engineControlA->setLabel (names.a);
        engineControlB->setLabel (names.b);
        engineControlC->setLabel (names.c);
        engineControlD->setLabel (names.d);
    }

    materialSection.addAndMakeVisible (bodyView);
    auto& materialMenu = menu (materialSection, params::material::type, "Material");
    materialMenu.getCombo().onChange = [this]
    {
        if (applyingMaterial)
            return;

        applyingMaterial = true;
        const auto index = aethrProcessor.getValueTreeState().getParameter (params::material::type);

        if (index != nullptr)
            params::applyMaterialToState (aethrProcessor.getValueTreeState(),
                                          juce::roundToInt (index->convertFrom0to1 (index->getValue())));

        applyingMaterial = false;
    };
    menu (materialSection, params::body::preset, "Body");
    menu (materialSection, params::body::modes, "Modes");
    knob (materialSection, params::body::mix, "Mix", ui::KnobSize::primary, "Body resonator blend.");
    knob (materialSection, params::body::decay, "Decay", ui::KnobSize::secondary, "Body mode decay.");
    knob (materialSection, params::body::brightness, "Bright", ui::KnobSize::secondary, "Body brightness.");

    filterSection.addAndMakeVisible (filterView);
    menu (filterSection, params::fx::filterType, "Type");
    knob (filterSection, params::fx::filterCutoff, "Cutoff", ui::KnobSize::secondary, "Filter cutoff frequency.");
    knob (filterSection, params::fx::filterResonance, "Res", ui::KnobSize::secondary, "Filter resonance.");
    knob (filterSection, params::fx::filterMix, "Mix", ui::KnobSize::micro, "Filter wet/dry.");

    driveSection.addAndMakeVisible (driveView);
    menu (driveSection, params::fx::satMode, "Mode");
    knob (driveSection, params::fx::satDrive, "Drive", ui::KnobSize::primary, "Saturation amount.");
    knob (driveSection, params::fx::satTone, "Tone", ui::KnobSize::secondary, "Saturation tone.");
    knob (driveSection, params::fx::satMix, "Mix", ui::KnobSize::micro, "Saturation wet/dry.");

    layerSection.addAndMakeVisible (layerView);
    toggle (layerSection, params::layer::bEnable, "Layer B");
    knob (layerSection, params::layer::bInterval, "Interval", ui::KnobSize::secondary, "Layer B pitch interval.");
    knob (layerSection, params::layer::bDetune, "Detune", ui::KnobSize::micro, "Layer B detune.");
    knob (layerSection, params::layer::bLevel, "Level", ui::KnobSize::secondary, "Layer B level.");

    knob (macroSection, params::macros::material, "Material", ui::KnobSize::primary, "Material macro.");
    knob (macroSection, params::macros::attack, "Attack", ui::KnobSize::primary, "Attack macro.");
    knob (macroSection, params::macros::decay, "Decay", ui::KnobSize::primary, "Decay macro.");
    knob (macroSection, params::macros::brightness, "Bright", ui::KnobSize::primary, "Brightness macro.");
    knob (macroSection, params::macros::body, "Body", ui::KnobSize::primary, "Body macro.");
    knob (macroSection, params::macros::chaos, "Chaos", ui::KnobSize::primary, "Chaos macro.");
    knob (macroSection, params::macros::space, "Space", ui::KnobSize::primary, "Space macro.");
    knob (macroSection, params::macros::drive, "Drive", ui::KnobSize::primary, "Drive macro.");

    modSection.addAndMakeVisible (lfo1View);
    menu (modSection, params::lfo1::wave, "LFO 1");
    toggle (modSection, params::lfo1::sync, "Sync");
    menu (modSection, params::lfo1::division, "Div");
    knob (modSection, params::lfo1::rate, "Rate", ui::KnobSize::secondary, "LFO 1 free rate when Sync is off.");
    knob (modSection, params::lfo1::depth, "Amount", ui::KnobSize::secondary, "LFO 1 amount.");
    menu (modSection, params::lfo1::dest, "Dest");
    modSection.addAndMakeVisible (lfo2View);
    menu (modSection, params::lfo2::wave, "LFO 2");
    toggle (modSection, params::lfo2::sync, "Sync");
    menu (modSection, params::lfo2::division, "Div");
    knob (modSection, params::lfo2::rate, "Rate", ui::KnobSize::secondary, "LFO 2 free rate when Sync is off.");
    knob (modSection, params::lfo2::depth, "Amount", ui::KnobSize::secondary, "LFO 2 amount.");
    menu (modSection, params::lfo2::dest, "Dest");
    knob (modSection, params::env::attack, "A", ui::KnobSize::micro, "Aux envelope attack.");
    knob (modSection, params::env::decay, "D", ui::KnobSize::micro, "Aux envelope decay.");
    knob (modSection, params::env::sustain, "S", ui::KnobSize::micro, "Aux envelope sustain.");
    knob (modSection, params::env::release, "R", ui::KnobSize::micro, "Aux envelope release.");
    knob (modSection, params::env::depth, "Env", ui::KnobSize::secondary, "Aux envelope amount.");
    menu (modSection, params::env::dest, "Env Dest");
    knob (modSection, params::chaos::amount, "Chaos", ui::KnobSize::secondary, "Chaotic modulation amount.");
    toggle (modSection, params::chaos::sync, "Sync");
    menu (modSection, params::chaos::division, "Div");
    knob (modSection, params::chaos::rate, "Rate", ui::KnobSize::micro, "Chaos free rate when Sync is off.");
    knob (modSection, params::chaos::bias, "Bias", ui::KnobSize::micro, "Chaos bias.");
    modSection.addAndMakeVisible (modMatrix);

    delaySection.addAndMakeVisible (delayView);
    toggle (delaySection, params::fx::delaySync, "Sync");
    menu (delaySection, params::fx::delayDivisionL, "Div L");
    knob (delaySection, params::fx::delayTimeL, "Time L", ui::KnobSize::secondary, "Left delay time when Sync is off.");
    menu (delaySection, params::fx::delayDivisionR, "Div R");
    knob (delaySection, params::fx::delayTimeR, "Time R", ui::KnobSize::secondary, "Right delay time when Sync is off.");
    knob (delaySection, params::fx::delayFeedback, "Feedback", ui::KnobSize::secondary, "Delay feedback.");
    knob (delaySection, params::fx::delayDamp, "Damp", ui::KnobSize::micro, "Delay damping.");
    knob (delaySection, params::fx::delayMix, "Mix", ui::KnobSize::secondary, "Delay mix.");

    motionSection.addAndMakeVisible (motionView);
    toggle (motionSection, params::fx::chorusSync, "Sync");
    menu (motionSection, params::fx::chorusDivision, "Div");
    knob (motionSection, params::fx::chorusRate, "Chorus", ui::KnobSize::secondary, "Chorus free rate when Sync is off.");
    knob (motionSection, params::fx::chorusDepth, "Depth", ui::KnobSize::micro, "Chorus depth.");
    knob (motionSection, params::fx::chorusMix, "Mix", ui::KnobSize::micro, "Chorus mix.");
    toggle (motionSection, params::fx::phaserSync, "Sync");
    menu (motionSection, params::fx::phaserDivision, "Div");
    knob (motionSection, params::fx::phaserRate, "Phaser", ui::KnobSize::secondary, "Phaser free rate when Sync is off.");
    knob (motionSection, params::fx::phaserDepth, "Depth", ui::KnobSize::micro, "Phaser depth.");
    knob (motionSection, params::fx::phaserMix, "Mix", ui::KnobSize::micro, "Phaser mix.");

    reverbSection.addAndMakeVisible (chamberView);
    knob (reverbSection, params::fx::reverbSize, "Size", ui::KnobSize::primary, "Chamber size.");
    knob (reverbSection, params::fx::reverbDecay, "Decay", ui::KnobSize::secondary, "Chamber decay.");
    knob (reverbSection, params::fx::reverbDamp, "Damp", ui::KnobSize::micro, "Chamber damping.");
    knob (reverbSection, params::fx::reverbWidth, "Width", ui::KnobSize::secondary, "Chamber width.");
    knob (reverbSection, params::fx::reverbMix, "Mix", ui::KnobSize::primary, "Chamber mix.");

    outputSection.addAndMakeVisible (meterView);
    knob (outputSection, params::output::gain, "Level", ui::KnobSize::primary, "Master output level.");
    knob (outputSection, params::output::ceiling, "Ceiling", ui::KnobSize::secondary, "Output ceiling.");
    knob (outputSection, params::master::tuneOctave, "Oct", ui::KnobSize::micro, "Global octave.");
    knob (outputSection, params::master::tuneSemitones, "Semi", ui::KnobSize::micro, "Global semitone.");
    knob (outputSection, params::master::tuneCents, "Cents", ui::KnobSize::micro, "Global cents.");
    knob (outputSection, params::voice::velocityAmount, "Vel", ui::KnobSize::micro, "Velocity to amplitude.");
    menu (outputSection, params::voice::polyphony, "Poly");
    toggle (outputSection, params::exciter::seedLocked, "Lock Seed");

    browser.onChoose = [this] (int index)
    {
        applyPreset (index);
        showBrowser (false);
    };
    browser.onDismiss = [this] { showBrowser (false); };
    addChildComponent (browser);

    setSize (editorWidth, editorHeight);
    resized();
    startTimerHz (refreshHz);
}

AethrEditor::~AethrEditor()
{
    setLookAndFeel (nullptr);
}

void AethrEditor::applyPreset (int index)
{
    currentPreset = juce::jlimit (0, presets::numFactoryPresets() - 1, index);
    aethrProcessor.getUndoManager().beginNewTransaction ("AETHR preset");
    presets::applyFactory (aethrProcessor.getValueTreeState(), currentPreset);
    presetField.setText (presets::factory[static_cast<std::size_t> (currentPreset)].name,
                         presets::factory[static_cast<std::size_t> (currentPreset)].category);
    headerPulse = 0.55f;
}

void AethrEditor::showBrowser (bool show)
{
    browser.setVisible (show);
    browser.setBounds (getLocalBounds());
    browser.toFront (false);
}

void AethrEditor::pulseLaboratory()
{
    headerPulse = 1.0f;
    randomButton.setToggleState (true, juce::dontSendNotification);
}

void AethrEditor::paint (juce::Graphics& g)
{
    ui::paintChassis (g, getLocalBounds(), headerPulse);

    auto header = juce::Rectangle<float> (16.0f, 8.0f, 250.0f, 52.0f);
    ui::drawAethrMark (g, header.removeFromLeft (46.0f));
    header.removeFromLeft (6.0f);
    ui::drawWordmark (g, header.removeFromTop (30.0f));
    g.setColour (juce::Colour (ui::Theme::textSecondary));
    g.setFont (ui::labelFont (10.0f));
    g.drawText (juce::String (branding::tagline).toUpperCase(), header, juce::Justification::centredLeft, false);

    g.setColour (juce::Colour (ui::Theme::goldDim).withAlpha (0.55f));
    g.fillRect (16.0f, 66.0f, static_cast<float> (getWidth()) - 32.0f, 0.8f);
    for (int i = 0; i < 24; ++i)
    {
        const auto x = 24.0f + static_cast<float> (i) * ((static_cast<float> (getWidth()) - 48.0f) / 23.0f);
        g.setColour (juce::Colour (ui::Theme::gold).withAlpha (i % 4 == 0 ? 0.35f : 0.12f));
        g.drawLine (x, 64.0f, x, i % 4 == 0 ? 71.0f : 68.0f, 0.8f);
    }

    const auto join = [&g] (juce::Component& a, juce::Component& b)
    {
        const auto pa = a.getBounds().toFloat();
        const auto pb = b.getBounds().toFloat();
        ui::drawBusNode (g, { pa.getCentreX(), pa.getBottom() }, { pb.getCentreX(), pb.getY() });
    };
    join (exciterSection, filterSection);
    join (resonatorSection, driveSection);
    join (materialSection, macroSection);
    join (filterSection, delaySection);
    join (macroSection, outputSection);

    auto footer = getLocalBounds().removeFromBottom (22).reduced (18, 4);
    g.setColour (juce::Colour (ui::Theme::textSecondary));
    g.setFont (ui::valueFont (10.0f));
    g.drawText (juce::String (branding::companyName).toUpperCase()
                    + "   ·   PHYSICAL RESONANCE ENGINE   ·   "
                    + juce::String (engine::infoFor (aethrProcessor.getSelectedEngineType()).name).toUpperCase(),
                footer, juce::Justification::centredLeft, false);
    g.drawText ("VOICES " + juce::String (displayedVoices)
                    + "   MIDI " + juce::String (aethrProcessor.getMidiEventCount())
                    + "   " + juce::String (aethrProcessor.getHostTempoBpm(), 1) + " BPM"
                    + "   " + juce::String (aethrProcessor.getCurrentSampleRate() / 1000.0, 1) + " kHz"
                    + "   v" + branding::version,
                footer, juce::Justification::centredRight, false);
}

void AethrEditor::resized()
{
    auto bounds = getLocalBounds().reduced (10, 8);
    auto header = bounds.removeFromTop (52);
    header.removeFromLeft (248);

    presetField.setBounds (header.removeFromLeft (248).reduced (4, 8));
    laboratoryBox.setBounds (header.removeFromLeft (108).reduced (4, 12));
    initButton.setBounds (header.removeFromLeft (52).reduced (3, 12));
    saveButton.setBounds (header.removeFromLeft (54).reduced (3, 12));
    randomButton.setBounds (header.removeFromLeft (62).reduced (3, 12));
    mutateButton.setBounds (header.removeFromLeft (72).reduced (3, 12));
    undoButton.setBounds (header.removeFromLeft (58).reduced (3, 12));
    redoButton.setBounds (header.removeFromLeft (58).reduced (3, 12));

    auto engineRow = bounds.removeFromTop (30);
    engineStrip.setBounds (engineRow.reduced (0, 2));
    auto arpRow = bounds.removeFromTop (38);
    arpStrip.setBounds (arpRow.reduced (0, 1));

    bounds.removeFromBottom (22);
    bounds.removeFromTop (6);

    const auto h = bounds.getHeight();
    auto row1 = bounds.removeFromTop (h * 32 / 100);
    auto row2 = bounds.removeFromTop (h * 20 / 100);
    auto row3 = bounds.removeFromTop (h * 25 / 100);
    auto row4 = bounds;

    const auto split = [] (juce::Rectangle<int> row, int parts, int index, int span = 1)
    {
        const auto w = row.getWidth() / parts;
        return juce::Rectangle<int> (row.getX() + w * index,
                                     row.getY(),
                                     w * span + (index + span == parts ? row.getWidth() - w * parts : 0),
                                     row.getHeight());
    };

    {
        const auto w = row1.getWidth();
        exciterSection.setBounds (row1.removeFromLeft (w * 26 / 100).reduced (1));
        resonatorSection.setBounds (row1.removeFromLeft (w * 48 / 100).reduced (1));
        materialSection.setBounds (row1.reduced (1));
    }

    filterSection.setBounds (split (row2, 9, 0, 2).reduced (1));
    driveSection.setBounds (split (row2, 9, 2, 2).reduced (1));
    layerSection.setBounds (split (row2, 9, 4, 2).reduced (1));
    macroSection.setBounds (split (row2, 9, 6, 3).reduced (1));

    modSection.setBounds (split (row3, 1, 0).reduced (1));

    delaySection.setBounds (split (row4, 4, 0).reduced (1));
    motionSection.setBounds (split (row4, 4, 1).reduced (1));
    reverbSection.setBounds (split (row4, 4, 2).reduced (1));
    outputSection.setBounds (split (row4, 4, 3).reduced (1));

    const bool showViz = getHeight() >= 760;

    auto layoutExciter = [&] ()
    {
        auto area = exciterSection.content();
        auto items = childrenOf (exciterSection);
        if (items.size() < 9)
            return;

        items[0]->setVisible (showViz);
        if (showViz)
            place (items[0], area.removeFromLeft (juce::jmax (90, area.getWidth() * 28 / 100)).reduced (2));

        place (items[1], area.removeFromTop (34).reduced (2, 1));
        ui::placeRow (area.removeFromTop (area.getHeight() / 2), { items[2], items[3], items[4] });
        ui::placeRow (area, { items[5], items[6], items[7], items[8] });
    };

    auto layoutResonator = [&] ()
    {
        auto area = resonatorSection.content();
        auto items = childrenOf (resonatorSection);
        if (items.size() < 13)
            return;

        items[0]->setVisible (showViz);
        if (showViz)
            place (items[0], area.removeFromTop (area.getHeight() * 38 / 100).reduced (2));

        ui::placeRow (area.removeFromTop (area.getHeight() * 40 / 100), { items[1], items[2], items[3], items[4] });
        ui::placeRow (area.removeFromTop (area.getHeight() * 50 / 100), { items[5], items[6], items[7], items[8] });
        ui::placeRow (area, { items[9], items[10], items[11], items[12] });
    };

    auto layoutBody = [&] ()
    {
        auto area = materialSection.content();
        auto items = childrenOf (materialSection);
        if (items.size() < 7)
            return;

        items[0]->setVisible (showViz);
        if (showViz)
            place (items[0], area.removeFromLeft (juce::jmax (80, area.getWidth() * 32 / 100)).reduced (2));

        ui::placeRow (area.removeFromTop (36), { items[1], items[2], items[3] });
        ui::placeRow (area, { items[4], items[5], items[6] });
    };

    auto layoutFilter = [&] ()
    {
        auto area = filterSection.content();
        auto items = childrenOf (filterSection);
        if (items.size() < 5)
            return;

        items[0]->setVisible (showViz);
        if (showViz)
            place (items[0], area.removeFromTop (36).reduced (2));

        place (items[1], area.removeFromTop (32).reduced (2, 0));
        ui::placeRow (area, { items[2], items[3], items[4] });
    };

    auto layoutDrive = [&] ()
    {
        auto area = driveSection.content();
        auto items = childrenOf (driveSection);
        if (items.size() < 5)
            return;

        items[0]->setVisible (showViz);
        if (showViz)
            place (items[0], area.removeFromTop (36).reduced (2));

        place (items[1], area.removeFromTop (32).reduced (2, 0));
        ui::placeRow (area, { items[2], items[3], items[4] });
    };

    auto layoutLayers = [&] ()
    {
        auto area = layerSection.content();
        auto items = childrenOf (layerSection);
        if (items.size() < 5)
            return;

        items[0]->setVisible (showViz);
        if (showViz)
            place (items[0], area.removeFromLeft (76).reduced (2));

        place (items[1], area.removeFromTop (28).reduced (2));
        ui::placeRow (area, { items[2], items[3], items[4] });
    };

    auto layoutMacros = [&] ()
    {
        auto area = macroSection.content();
        auto items = childrenOf (macroSection);
        if (items.size() < 8)
            return;

        ui::placeRow (area.removeFromTop (area.getHeight() / 2), { items[0], items[1], items[2], items[3] });
        ui::placeRow (area, { items[4], items[5], items[6], items[7] });
    };

    auto layoutMod = [&] ()
    {
        auto area = modSection.content();
        auto items = childrenOf (modSection);
        if (items.size() < 26)
            return;

        auto matrix = area.removeFromRight (juce::jmax (180, area.getWidth() * 28 / 100));
        place (items[25], matrix.reduced (4, 2));

        auto col = area.removeFromLeft (area.getWidth() / 3);
        items[0]->setVisible (showViz);
        if (showViz)
            place (items[0], col.removeFromTop (28).reduced (2));
        place (items[1], col.removeFromTop (26).reduced (1));
        ui::placeRow (col.removeFromTop (col.getHeight() / 2), { items[2], items[3], items[4] });
        ui::placeRow (col, { items[5], items[6] });

        col = area.removeFromLeft (area.getWidth() / 2);
        items[7]->setVisible (showViz);
        if (showViz)
            place (items[7], col.removeFromTop (28).reduced (2));
        place (items[8], col.removeFromTop (26).reduced (1));
        ui::placeRow (col.removeFromTop (col.getHeight() / 2), { items[9], items[10], items[11] });
        ui::placeRow (col, { items[12], items[13] });

        ui::placeRow (area.removeFromTop (area.getHeight() / 2), { items[14], items[15], items[16], items[17], items[18], items[19] });
        ui::placeRow (area, { items[20], items[21], items[22], items[23], items[24] });
    };

    auto layoutDelay = [&] ()
    {
        auto area = delaySection.content();
        auto items = childrenOf (delaySection);
        if (items.size() < 9)
            return;

        items[0]->setVisible (showViz);
        if (showViz)
            place (items[0], area.removeFromTop (28).reduced (2));

        ui::placeRow (area.removeFromTop (area.getHeight() * 58 / 100),
                      { items[1], items[2], items[3], items[4], items[5] });
        ui::placeRow (area, { items[6], items[7], items[8] });
    };

    auto layoutMotion = [&] ()
    {
        auto area = motionSection.content();
        auto items = childrenOf (motionSection);
        if (items.size() < 11)
            return;

        items[0]->setVisible (showViz);
        if (showViz)
            place (items[0], area.removeFromTop (28).reduced (2));

        ui::placeRow (area.removeFromTop (area.getHeight() / 2),
                      { items[1], items[2], items[3], items[4], items[5] });
        ui::placeRow (area, { items[6], items[7], items[8], items[9], items[10] });
    };

    auto layoutChamber = [&] ()
    {
        auto area = reverbSection.content();
        auto items = childrenOf (reverbSection);
        if (items.size() < 6)
            return;

        items[0]->setVisible (showViz);
        if (showViz)
            place (items[0], area.removeFromTop (28).reduced (2));

        ui::placeRow (area, { items[1], items[2], items[3], items[4], items[5] });
    };

    auto layoutMaster = [&] ()
    {
        auto area = outputSection.content();
        auto items = childrenOf (outputSection);
        if (items.size() < 9)
            return;

        place (items[0], area.removeFromRight (18).reduced (1));
        ui::placeRow (area.removeFromTop (area.getHeight() * 55 / 100), { items[1], items[2], items[3], items[4] });
        ui::placeRow (area, { items[5], items[6], items[7], items[8] });
    };

    layoutExciter();
    layoutResonator();
    layoutBody();
    layoutFilter();
    layoutDrive();
    layoutLayers();
    layoutMacros();
    layoutMod();
    layoutDelay();
    layoutMotion();
    layoutChamber();
    layoutMaster();

    if (browser.isVisible())
        browser.setBounds (getLocalBounds());
}

void AethrEditor::timerCallback()
{
    displayedLevel = std::max (aethrProcessor.getOutputPeakLevel(), displayedLevel * 0.88f);
    displayedVoices = aethrProcessor.getActiveVoiceCount();
    headerPulse *= 0.90f;

    const auto bpm = aethrProcessor.getHostTempoBpm();
    lfo1View.setHostTempoBpm (bpm);
    lfo2View.setHostTempoBpm (bpm);
    delayView.setHostTempoBpm (bpm);
    motionView.setHostTempoBpm (bpm);
    arpStrip.setChaseStep (aethrProcessor.getArpStep());

    const auto engineType = aethrProcessor.getSelectedEngineType();

    if (engineType != displayedEngine)
    {
        displayedEngine = engineType;
        const auto names = engine::controlNamesFor (engineType);

        if (engineControlA != nullptr) engineControlA->setLabel (names.a);
        if (engineControlB != nullptr) engineControlB->setLabel (names.b);
        if (engineControlC != nullptr) engineControlC->setLabel (names.c);
        if (engineControlD != nullptr) engineControlD->setLabel (names.d);
    }

    const auto energy = juce::jlimit (0.0f, 1.0f, displayedLevel * 1.8f
                                      + (displayedVoices > 0 ? 0.25f : 0.0f));
    exciterView.setEnergy (energy);
    resonatorView.setState (energy, aethrProcessor.getLastNoteHz());
    bodyView.setEnergy (energy);
    meterView.setLevel (displayedLevel);

    for (auto& knob : knobs)
        knob->decayGlow (0.82f);

    if (headerPulse < 0.08f)
        randomButton.setToggleState (false, juce::dontSendNotification);

    repaint();
}

} // namespace aethr
