#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include <vector>

#include "Engine/EngineType.h"
#include "UI/ArpStrip.h"
#include "UI/Controls.h"
#include "UI/EngineBrowser.h"
#include "UI/LookAndFeel.h"
#include "UI/PresetBrowser.h"
#include "UI/Visualizers.h"

namespace aethr
{

class AethrProcessor;

class AethrEditor final : public juce::AudioProcessorEditor,
                          private juce::Timer
{
public:
    explicit AethrEditor (AethrProcessor&);
    ~AethrEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void applyPreset (int index);
    void showBrowser (bool show);
    void pulseLaboratory();
    void updateSyncEnableState();
    void saveUserPreset();

    AethrProcessor& aethrProcessor;
    ui::LookAndFeel lookAndFeel;
    juce::TooltipWindow tooltipWindow { this, 700 };

    ui::AethrPresetField presetField;
    ui::AethrPlate initButton { "INIT" };
    ui::AethrPlate saveButton { "SAVE" };
    ui::AethrPlate randomButton { "RAND" };
    ui::AethrPlate mutateButton { "MUTATE" };
    ui::AethrPlate undoButton { "UNDO" };
    ui::AethrPlate redoButton { "REDO" };
    ui::AethrPlate advancedButton { "ADV" };
    juce::ComboBox laboratoryBox;
    ui::AethrEngineStrip engineStrip;
    ui::AethrArpStrip arpStrip;

    ui::AethrPanel exciterSection  { "EXCITER",    "EXCITATION SYSTEM",     ui::Glyph::impulse,    "SRC / 01", ui::Rank::major };
    ui::AethrPanel resonatorSection{ "RESONATOR",  "PHYSICAL MODEL CORE",   ui::Glyph::resonator,  "RES / 02", ui::Rank::hero };
    ui::AethrPanel materialSection { "BODY",       "RESONANT STRUCTURE",    ui::Glyph::body,       "BDY / 03", ui::Rank::major };
    ui::AethrPanel filterSection   { "FILTER",     "SPECTRAL GATE",         ui::Glyph::filter,     "FLT / 04", ui::Rank::minor };
    ui::AethrPanel driveSection    { "DRIVE",      "NONLINEAR STAGE",       ui::Glyph::drive,      "DRV / 05", ui::Rank::minor };
    ui::AethrPanel layerSection    { "LAYERS",     "DUAL ORBIT",            ui::Glyph::layers,     "LYR / 06", ui::Rank::minor };
    ui::AethrPanel macroSection    { "MACROS",     "PERFORMANCE CORE",      ui::Glyph::macros,     "MCR / 07", ui::Rank::major };
    ui::AethrPanel modSection      { "MODULATION", "ROUTING MATRIX",        ui::Glyph::modulation, "MOD / 08", ui::Rank::major };
    ui::AethrPanel delaySection    { "DELAY",      "SYNCED STEREO TIME",     ui::Glyph::delay,      "DLY / 09", ui::Rank::minor };
    ui::AethrPanel motionSection   { "MOTION",     "SYNCED CHORUS / PHASER", ui::Glyph::motion,     "MOT / 10", ui::Rank::minor };
    ui::AethrPanel reverbSection   { "CHAMBER",    "SPATIAL FIELD",         ui::Glyph::chamber,    "CMB / 11", ui::Rank::minor };
    ui::AethrPanel outputSection   { "MASTER",     "OUTPUT STAGE",          ui::Glyph::master,     "MST / 12", ui::Rank::major };

    ui::ExciterView     exciterView;
    ui::ResonatorView   resonatorView;
    ui::BodyView        bodyView;
    ui::FilterView      filterView;
    ui::DriveView       driveView;
    ui::LayerView       layerView;
    ui::LfoView         lfo1View;
    ui::LfoView         lfo2View;
    ui::DelayView       delayView;
    ui::MotionView      motionView;
    ui::ChamberView     chamberView;
    ui::MeterView       meterView;
    ui::ModulationMatrix modMatrix;
    ui::AethrPresetBrowser browser;

    std::vector<std::unique_ptr<ui::AethrKnob>> knobs;
    std::vector<std::unique_ptr<ui::AethrCombo>> combos;
    std::vector<std::unique_ptr<ui::AethrToggle>> toggles;

    ui::AethrKnob* engineControlA { nullptr };
    ui::AethrKnob* engineControlB { nullptr };
    ui::AethrKnob* engineControlC { nullptr };
    ui::AethrKnob* engineControlD { nullptr };
    ui::AethrKnob* lfo1RateKnob { nullptr };
    ui::AethrKnob* lfo2RateKnob { nullptr };
    ui::AethrKnob* chaosRateKnob { nullptr };
    ui::AethrKnob* delayTimeLKnob { nullptr };
    ui::AethrKnob* delayTimeRKnob { nullptr };
    ui::AethrKnob* chorusRateKnob { nullptr };
    ui::AethrKnob* phaserRateKnob { nullptr };
    ui::AethrCombo* lfo1DivCombo { nullptr };
    ui::AethrCombo* lfo2DivCombo { nullptr };
    ui::AethrCombo* chaosDivCombo { nullptr };
    ui::AethrCombo* delayDivLCombo { nullptr };
    ui::AethrCombo* delayDivRCombo { nullptr };
    ui::AethrCombo* chorusDivCombo { nullptr };
    ui::AethrCombo* phaserDivCombo { nullptr };
    engine::EngineType displayedEngine { engine::EngineType::string };

    int currentPreset { 0 };
    float displayedLevel { 0.0f };
    float headerPulse { 0.0f };
    int displayedVoices { 0 };
    bool applyingMaterial { false };
    bool advancedMode { false };
    juce::Random rng { 0xae711u };
    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AethrEditor)
};

} // namespace aethr
