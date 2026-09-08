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
    setTitle ("AETHR Physical Resonance Engine");
    setDescription ("ixmuk physical-modelling instrument editor.");

    auto& state = aethrProcessor.getValueTreeState();

    presetField.setTitle ("Preset");
    presetField.setDescription ("Previous and next browse factory presets. Click the centre to open the library.");
    presetField.setTooltip ("Preset\n\nLeft/right edges step factory presets. Centre opens the category library.");
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
    currentPreset = aethrProcessor.getFactoryPresetIndex();
    presetField.setText (presets::factory[static_cast<std::size_t> (currentPreset)].name,
                         presets::factory[static_cast<std::size_t> (currentPreset)].category);

    initButton.onClick = [this] { applyPreset (0); };
    initButton.setTooltip ("INIT\n\nReset every parameter to the factory Init snapshot.");
    saveButton.onClick = [this] { saveUserPreset(); };
    saveButton.setTooltip ("SAVE\n\nWrite the current sound as a user .aethr preset file.");
    randomButton.onClick = [this]
    {
        const auto mode = static_cast<presets::RandomMode> (std::clamp (laboratoryBox.getSelectedItemIndex(), 0, 3));
        presets::randomize (aethrProcessor.getValueTreeState(), mode, rng);
        pulseLaboratory();
    };
    randomButton.setTooltip ("RAND\n\nMusically constrained randomisation. Depth follows the Laboratory mode.");
    mutateButton.onClick = [this]
    {
        presets::mutate (aethrProcessor.getValueTreeState(), rng);
        headerPulse = 0.7f;
    };
    mutateButton.setTooltip ("MUTATE\n\nNudge the current sound slightly without destroying its character.");
    undoButton.onClick = [this] { aethrProcessor.getUndoManager().undo(); };
    undoButton.setTooltip ("UNDO\n\nUndo the last parameter or preset change.");
    redoButton.onClick = [this] { aethrProcessor.getUndoManager().redo(); };
    redoButton.setTooltip ("REDO\n\nRedo the last undone change.");
    advancedButton.setClickingTogglesState (true);
    advancedButton.setTooltip ("ADV\n\nEngineering mode: show the full modulation and motion sections.\nPerformance mode keeps the chassis focused on core sound shaping.");
    advancedButton.onClick = [this]
    {
        advancedMode = advancedButton.getToggleState();
        resized();
    };

    for (auto* button : { &initButton, &saveButton, &randomButton, &mutateButton, &undoButton, &redoButton, &advancedButton })
        addAndMakeVisible (*button);

    initButton.setTitle ("Initialise");
    saveButton.setTitle ("Save user preset");
    randomButton.setTitle ("Randomise");
    mutateButton.setTitle ("Mutate");
    undoButton.setTitle ("Undo");
    redoButton.setTitle ("Redo");
    advancedButton.setTitle ("Advanced engineering mode");

    laboratoryBox.addItemList ({ "Safe", "Musical", "Experimental", "Chaotic" }, 1);
    laboratoryBox.setSelectedId (2, juce::dontSendNotification);
    laboratoryBox.setTooltip ("LABORATORY\n\nControls how aggressive RAND is: Safe stays playable, Chaotic explores extremes.");
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
    const auto menu = [this, &state] (juce::Component& parent, const char* id, const juce::String& name,
                                      juce::String hint = {}) -> ui::AethrCombo&
    {
        auto item = std::make_unique<ui::AethrCombo> (state, id, name, std::move (hint));
        parent.addAndMakeVisible (*item);
        auto& ref = *item;
        combos.push_back (std::move (item));
        return ref;
    };
    const auto toggle = [this, &state] (juce::Component& parent, const char* id, const juce::String& name,
                                        juce::String hint = {})
    {
        auto item = std::make_unique<ui::AethrToggle> (state, id, name, std::move (hint));
        parent.addAndMakeVisible (*item);
        toggles.push_back (std::move (item));
    };

    exciterSection.addAndMakeVisible (exciterView);
    menu (exciterSection, params::exciter::type, "Source",
          "Excitation source. Pluck is the default physical strike; others reshape the transient.");
    knob (exciterSection, params::exciter::burstTime, "Burst", ui::KnobSize::secondary,
          "Excitation burst length. Shorter values feel more percussive; longer values smear the strike.");
    knob (exciterSection, params::exciter::attack, "Attack", ui::KnobSize::secondary,
          "Impulse attack time. Softens the leading edge of the excitation.");
    knob (exciterSection, params::exciter::colour, "Colour", ui::KnobSize::secondary,
          "Spectral tilt of the strike. Positive values brighten the impulse.");
    knob (exciterSection, params::exciter::brightness, "Bright", ui::KnobSize::secondary,
          "High-frequency energy in the strike before it enters the resonator.");
    knob (exciterSection, params::exciter::level, "Level", ui::KnobSize::secondary,
          "Excitation amplitude into the physical model.");
    knob (exciterSection, params::exciter::randomAmount, "Scatter", ui::KnobSize::micro,
          "Strike-to-strike variation. Keeps repeated notes from sounding mechanical.");
    knob (exciterSection, params::exciter::stereoSpread, "Spread", ui::KnobSize::micro,
          "Left/right excitation offset for a wider stereo image.");
    knob (exciterSection, params::exciter::seed, "Seed", ui::KnobSize::micro,
          "Noise seed for excitation. Lock Seed freezes the pattern for reproducible strikes.");

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
    menu (modSection, params::lfo1::wave, "LFO 1", "Waveform for LFO 1.");
    toggle (modSection, params::lfo1::sync, "Sync", "When on, LFO 1 follows host tempo via Div.");
    lfo1DivCombo = &menu (modSection, params::lfo1::division, "Div", "Tempo division for LFO 1 when Sync is on.");
    lfo1RateKnob = &knob (modSection, params::lfo1::rate, "Rate", ui::KnobSize::secondary, "LFO 1 free rate when Sync is off.");
    knob (modSection, params::lfo1::depth, "Amount", ui::KnobSize::secondary, "LFO 1 modulation depth.");
    menu (modSection, params::lfo1::dest, "Dest", "Destination for LFO 1.");
    modSection.addAndMakeVisible (lfo2View);
    menu (modSection, params::lfo2::wave, "LFO 2", "Waveform for LFO 2.");
    toggle (modSection, params::lfo2::sync, "Sync", "When on, LFO 2 follows host tempo via Div.");
    lfo2DivCombo = &menu (modSection, params::lfo2::division, "Div", "Tempo division for LFO 2 when Sync is on.");
    lfo2RateKnob = &knob (modSection, params::lfo2::rate, "Rate", ui::KnobSize::secondary, "LFO 2 free rate when Sync is off.");
    knob (modSection, params::lfo2::depth, "Amount", ui::KnobSize::secondary, "LFO 2 modulation depth.");
    menu (modSection, params::lfo2::dest, "Dest", "Destination for LFO 2.");
    knob (modSection, params::env::attack, "A", ui::KnobSize::micro, "Aux envelope attack.");
    knob (modSection, params::env::decay, "D", ui::KnobSize::micro, "Aux envelope decay.");
    knob (modSection, params::env::sustain, "S", ui::KnobSize::micro, "Aux envelope sustain.");
    knob (modSection, params::env::release, "R", ui::KnobSize::micro, "Aux envelope release.");
    knob (modSection, params::env::depth, "Env", ui::KnobSize::secondary, "Aux envelope amount.");
    menu (modSection, params::env::dest, "Env Dest", "Destination for the aux envelope.");
    knob (modSection, params::chaos::amount, "Chaos", ui::KnobSize::secondary, "Chaotic modulation amount.");
    toggle (modSection, params::chaos::sync, "Sync", "When on, Chaos follows host tempo via Div.");
    chaosDivCombo = &menu (modSection, params::chaos::division, "Div", "Tempo division for Chaos when Sync is on.");
    chaosRateKnob = &knob (modSection, params::chaos::rate, "Rate", ui::KnobSize::micro, "Chaos free rate when Sync is off.");
    knob (modSection, params::chaos::bias, "Bias", ui::KnobSize::micro, "Chaos bias.");
    modSection.addAndMakeVisible (modMatrix);

    delaySection.addAndMakeVisible (delayView);
    toggle (delaySection, params::fx::delaySync, "Sync", "When on, delay times follow Div L/R against host tempo.");
    delayDivLCombo = &menu (delaySection, params::fx::delayDivisionL, "Div L", "Left delay tempo division when Sync is on.");
    delayTimeLKnob = &knob (delaySection, params::fx::delayTimeL, "Time L", ui::KnobSize::secondary, "Left delay time when Sync is off.");
    delayDivRCombo = &menu (delaySection, params::fx::delayDivisionR, "Div R", "Right delay tempo division when Sync is on.");
    delayTimeRKnob = &knob (delaySection, params::fx::delayTimeR, "Time R", ui::KnobSize::secondary, "Right delay time when Sync is off.");
    knob (delaySection, params::fx::delayFeedback, "Feedback", ui::KnobSize::secondary, "Delay feedback.");
    knob (delaySection, params::fx::delayDamp, "Damp", ui::KnobSize::micro, "Delay damping.");
    knob (delaySection, params::fx::delayMix, "Mix", ui::KnobSize::secondary, "Delay mix.");

    motionSection.addAndMakeVisible (motionView);
    toggle (motionSection, params::fx::chorusSync, "Sync", "When on, chorus rate follows Div against host tempo.");
    chorusDivCombo = &menu (motionSection, params::fx::chorusDivision, "Div", "Chorus tempo division when Sync is on.");
    chorusRateKnob = &knob (motionSection, params::fx::chorusRate, "Chorus", ui::KnobSize::secondary, "Chorus free rate when Sync is off.");
    knob (motionSection, params::fx::chorusDepth, "Depth", ui::KnobSize::micro, "Chorus depth.");
    knob (motionSection, params::fx::chorusMix, "Mix", ui::KnobSize::micro, "Chorus mix.");
    toggle (motionSection, params::fx::phaserSync, "Sync", "When on, phaser rate follows Div against host tempo.");
    phaserDivCombo = &menu (motionSection, params::fx::phaserDivision, "Div", "Phaser tempo division when Sync is on.");
    phaserRateKnob = &knob (motionSection, params::fx::phaserRate, "Phaser", ui::KnobSize::secondary, "Phaser free rate when Sync is off.");
    knob (motionSection, params::fx::phaserDepth, "Depth", ui::KnobSize::micro, "Phaser depth.");
    knob (motionSection, params::fx::phaserFeedback, "Fdbk", ui::KnobSize::micro,
          "Phaser feedback. Higher values emphasise the notches and can push toward resonance.");
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
    aethrProcessor.applyFactoryProgram (currentPreset);
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

void AethrEditor::saveUserPreset()
{
    auto directory = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                         .getChildFile (branding::companyName)
                         .getChildFile (branding::productName)
                         .getChildFile ("UserPresets");
    directory.createDirectory();

    fileChooser = std::make_unique<juce::FileChooser> ("Save AETHR preset",
                                                       directory.getChildFile ("Preset.aethr"),
                                                       "*.aethr");

    constexpr auto flags = juce::FileBrowserComponent::saveMode
                         | juce::FileBrowserComponent::canSelectFiles
                         | juce::FileBrowserComponent::warnAboutOverwriting;

    fileChooser->launchAsync (flags, [this] (const juce::FileChooser& chooser)
    {
        auto file = chooser.getResult();

        if (file == juce::File{})
            return;

        if (! file.hasFileExtension (".aethr"))
            file = file.withFileExtension (".aethr");

        if (auto xml = aethrProcessor.getValueTreeState().copyState().createXml())
        {
            xml->setTagName ("AETHRPreset");
            xml->setAttribute ("product", branding::productName);
            xml->setAttribute ("version", branding::version);

            if (xml->writeTo (file))
            {
                headerPulse = 1.0f;
                presetField.setText (file.getFileNameWithoutExtension(), "User");
            }
        }
    });
}

void AethrEditor::updateSyncEnableState()
{
    auto& state = aethrProcessor.getValueTreeState();
    const auto synced = [&state] (const char* id) -> bool
    {
        if (auto* parameter = state.getParameter (id))
            return parameter->getValue() >= 0.5f;

        return false;
    };

    const auto gate = [] (ui::AethrKnob* freeControl, ui::AethrCombo* syncControl, bool useSync)
    {
        if (freeControl != nullptr)
            freeControl->setEnabledVisual (! useSync);

        if (syncControl != nullptr)
            syncControl->setEnabledVisual (useSync);
    };

    gate (lfo1RateKnob, lfo1DivCombo, synced (params::lfo1::sync));
    gate (lfo2RateKnob, lfo2DivCombo, synced (params::lfo2::sync));
    gate (chaosRateKnob, chaosDivCombo, synced (params::chaos::sync));
    gate (delayTimeLKnob, delayDivLCombo, synced (params::fx::delaySync));
    gate (delayTimeRKnob, delayDivRCombo, synced (params::fx::delaySync));
    gate (chorusRateKnob, chorusDivCombo, synced (params::fx::chorusSync));
    gate (phaserRateKnob, phaserDivCombo, synced (params::fx::phaserSync));
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

    presetField.setBounds (header.removeFromLeft (230).reduced (4, 8));
    laboratoryBox.setBounds (header.removeFromLeft (100).reduced (4, 12));
    initButton.setBounds (header.removeFromLeft (48).reduced (2, 12));
    saveButton.setBounds (header.removeFromLeft (50).reduced (2, 12));
    randomButton.setBounds (header.removeFromLeft (56).reduced (2, 12));
    mutateButton.setBounds (header.removeFromLeft (66).reduced (2, 12));
    undoButton.setBounds (header.removeFromLeft (52).reduced (2, 12));
    redoButton.setBounds (header.removeFromLeft (52).reduced (2, 12));
    advancedButton.setBounds (header.removeFromLeft (46).reduced (2, 12));
    advancedButton.setToggleState (advancedMode, juce::dontSendNotification);

    auto engineRow = bounds.removeFromTop (30);
    engineStrip.setBounds (engineRow.reduced (0, 2));
    auto arpRow = bounds.removeFromTop (38);
    arpStrip.setBounds (arpRow.reduced (0, 1));

    bounds.removeFromBottom (22);
    bounds.removeFromTop (6);

    const auto h = bounds.getHeight();
    modSection.setVisible (advancedMode);
    motionSection.setVisible (advancedMode);

    if (advancedMode)
    {
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
    }
    else
    {
        // Performance mode: core sound path first, hide engineering modulation/motion.
        auto row1 = bounds.removeFromTop (h * 42 / 100);
        auto row2 = bounds.removeFromTop (h * 28 / 100);
        auto row3 = bounds;

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

        delaySection.setBounds (split (row3, 3, 0).reduced (1));
        reverbSection.setBounds (split (row3, 3, 1).reduced (1));
        outputSection.setBounds (split (row3, 3, 2).reduced (1));
        motionSection.setBounds ({});
        modSection.setBounds ({});
    }

    const bool showViz = getHeight() >= 760;

    auto layoutExciter = [&] ()
    {
        auto area = exciterSection.content();
        auto items = childrenOf (exciterSection);
        if (items.size() < 10)
            return;

        items[0]->setVisible (showViz);
        if (showViz)
            place (items[0], area.removeFromLeft (juce::jmax (90, area.getWidth() * 28 / 100)).reduced (2));

        place (items[1], area.removeFromTop (34).reduced (2, 1));
        ui::placeRow (area.removeFromTop (area.getHeight() / 2), { items[2], items[3], items[4], items[5] });
        ui::placeRow (area, { items[6], items[7], items[8], items[9] });
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
        if (items.size() < 12)
            return;

        items[0]->setVisible (showViz);
        if (showViz)
            place (items[0], area.removeFromTop (28).reduced (2));

        ui::placeRow (area.removeFromTop (area.getHeight() / 2),
                      { items[1], items[2], items[3], items[4], items[5] });
        ui::placeRow (area, { items[6], items[7], items[8], items[9], items[10], items[11] });
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
    const auto previousVoices = displayedVoices;
    const auto previousLevel = displayedLevel;
    const auto previousPulse = headerPulse;
    const auto previousEngine = displayedEngine;

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
    resonatorView.setState (energy, aethrProcessor.getLastNoteHz(),
                            engine::infoFor (engineType).name);
    bodyView.setEnergy (energy);
    meterView.setLevel (displayedLevel);

    for (auto& knob : knobs)
        knob->decayGlow (0.82f);

    if (headerPulse < 0.08f)
        randomButton.setToggleState (false, juce::dontSendNotification);

    updateSyncEnableState();

    // Reflect host / MIDI program changes applied on the message thread.
    const auto programIndex = aethrProcessor.getFactoryPresetIndex();

    if (programIndex != currentPreset)
    {
        currentPreset = juce::jlimit (0, presets::numFactoryPresets() - 1, programIndex);
        presetField.setText (presets::factory[static_cast<std::size_t> (currentPreset)].name,
                             presets::factory[static_cast<std::size_t> (currentPreset)].category);
        headerPulse = 0.35f;
    }

    // Telemetry views animate every tick; chassis/footer only when chrome changes
    // or on a low-rate heartbeat (avoids full-editor storms at 36 Hz).
    exciterView.repaint();
    resonatorView.repaint();
    bodyView.repaint();
    filterView.repaint();
    driveView.repaint();
    layerView.repaint();
    lfo1View.repaint();
    lfo2View.repaint();
    delayView.repaint();
    motionView.repaint();
    chamberView.repaint();
    meterView.repaint();
    modMatrix.repaint();
    arpStrip.repaint();

    ++chromeFrame;

    const bool chromeDirty = headerPulse > 0.02f
                          || previousPulse > 0.02f
                          || displayedVoices != previousVoices
                          || displayedEngine != previousEngine
                          || std::abs (displayedLevel - previousLevel) > 0.02f
                          || (chromeFrame % 6) == 0;

    if (chromeDirty)
        repaint();
}

} // namespace aethr
