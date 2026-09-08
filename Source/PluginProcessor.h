#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include <atomic>
#include <vector>

#include "Engine/VoiceEngine.h"
#include "Engine/Arpeggiator.h"
#include "DSP/FxRack.h"
#include "DSP/Modulation.h"
#include "Parameters/ParameterLayout.h"

namespace aethr
{

/**
    Top-level AudioProcessor for AETHR.

    Realtime contract for processBlock() and everything it calls:

      - no heap allocation or deallocation
      - no locks that the message thread can hold
      - no file, network or logging I/O
      - no calls into juce::Component or any other UI type
      - no unbounded loops; work is proportional to block size and active voices

    Communication with the editor is one-way and lock-free: the audio thread
    publishes measurements into `std::atomic` members, and the editor polls them
    from a timer. Parameter values flow the other way through
    AudioProcessorValueTreeState, whose raw value pointers are read atomically.

    The processor owns the engine, hands it a settings snapshot once per block, and
    splits each block at MIDI event boundaries so note timing is sample-accurate
    rather than quantised to the block size.

    @see docs/ARCHITECTURE.md
*/
class AethrProcessor final : public juce::AudioProcessor
{
public:
    AethrProcessor();
    ~AethrProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;

    using juce::AudioProcessor::processBlock;

    void reset() override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override;

    bool acceptsMidi() const override                     { return true; }
    bool producesMidi() const override                    { return false; }
    bool isMidiEffect() const override                    { return false; }
    bool supportsDoublePrecisionProcessing() const override { return true; }
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override                          { return 1; }
    int getCurrentProgram() override                        { return 0; }
    void setCurrentProgram (int) override                   {}
    const juce::String getProgramName (int) override        { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    /** Parameter tree. Safe to read from either thread; mutate only from the message thread. */
    juce::UndoManager& getUndoManager() noexcept { return undoManager; }

    juce::AudioProcessorValueTreeState& getValueTreeState() noexcept { return valueTreeState; }

    /** Most recent output peak, in linear gain, for the editor's meter. Lock-free. */
    [[nodiscard]] float getOutputPeakLevel() const noexcept { return outputPeakLevel.load (std::memory_order_relaxed); }

    /** Total MIDI messages seen since the last prepareToPlay(). Lets the UI prove MIDI is arriving. */
    [[nodiscard]] int getMidiEventCount() const noexcept { return midiEventCount.load (std::memory_order_relaxed); }

    /** Voices currently sounding, published for the editor and for tests. */
    [[nodiscard]] int getActiveVoiceCount() const noexcept { return activeVoiceCount.load (std::memory_order_relaxed); }

    /** Last note-on frequency in Hz, for the resonator visualiser. Lock-free. */
    [[nodiscard]] float getLastNoteHz() const noexcept { return lastNoteHz.load (std::memory_order_relaxed); }

    /** Sample rate last passed to prepareToPlay(), for the footer readout. */
    [[nodiscard]] double getCurrentSampleRate() const noexcept { return currentSampleRate; }

    /** Host tempo last seen on the audio thread, falling back to 120 BPM. */
    [[nodiscard]] double getHostTempoBpm() const noexcept { return hostTempoBpm.load (std::memory_order_relaxed); }

    /** Engine currently selected by engine.type. */
    [[nodiscard]] engine::EngineType getSelectedEngineType() const noexcept;

    /** Current arp/sequencer step (0-15), published for the editor chase lights. */
    [[nodiscard]] int getArpStep() const noexcept { return arpStep.load (std::memory_order_relaxed); }

    /** Voice count currently selected by the (non-automatable) polyphony parameter. */
    [[nodiscard]] int getSelectedPolyphony() const noexcept;

    /**
        The engine, for tests that need to inspect tuning.

        Not for the editor: everything the UI needs is published through atomics, and
        reaching into voice state from the message thread would be a data race.
    */
    [[nodiscard]] const engine::VoiceEngine& getVoiceEngine() const noexcept { return voiceEngine; }

    /**
        Number of blocks the host delivered that were larger than the maximum it
        declared in prepareToPlay().

        This is reported as a counter rather than asserted on, for two reasons: it
        is a host misbehaviour we must survive rather than a programming error, and
        `jassertfalse` calls `juce::logAssertion`, which would mean logging from the
        audio thread. Non-zero means the output stage fell back to an unramped gain
        for those blocks.
    */
    [[nodiscard]] int getBlockSizeOverrunCount() const noexcept { return blockSizeOverruns.load (std::memory_order_relaxed); }

private:
    //==============================================================================
    /**
        Every parameter the audio thread reads, resolved once at construction.

        APVTS guarantees these pointers outlive the processor, so the audio thread
        never looks a parameter up by name. Grouping them keeps the per-block
        snapshot honest: it is obvious at a glance that each one is read exactly once.
    */
    struct ParameterHandles
    {
        std::atomic<float>* outputGainDb { nullptr };
        std::atomic<float>* polyphony { nullptr };
        std::atomic<float>* engineType { nullptr };
        std::atomic<float>* controlA { nullptr };
        std::atomic<float>* controlB { nullptr };
        std::atomic<float>* controlC { nullptr };
        std::atomic<float>* controlD { nullptr };

        std::atomic<float>* arpEnable { nullptr };
        std::atomic<float>* arpMode { nullptr };
        std::atomic<float>* arpDivision { nullptr };
        std::atomic<float>* arpOctaves { nullptr };
        std::atomic<float>* arpGate { nullptr };
        std::atomic<float>* arpSwing { nullptr };
        std::atomic<float>* arpLatch { nullptr };
        std::atomic<float>* arpPattern { nullptr };

        std::atomic<float>* velocityAmount { nullptr };

        std::atomic<float>* tuneOctave { nullptr };
        std::atomic<float>* tuneSemitones { nullptr };
        std::atomic<float>* tuneCents { nullptr };

        std::atomic<float>* decayTime { nullptr };
        std::atomic<float>* releaseTime { nullptr };
        std::atomic<float>* damping { nullptr };
        std::atomic<float>* resonatorBrightness { nullptr };
        std::atomic<float>* loopFilterMode { nullptr };
        std::atomic<float>* interpolation { nullptr };
        std::atomic<float>* loopFeedback { nullptr };

        std::atomic<float>* exciterType { nullptr };
        std::atomic<float>* burstTime { nullptr };
        std::atomic<float>* exciterAttack { nullptr };
        std::atomic<float>* exciterColour { nullptr };
        std::atomic<float>* exciterBrightness { nullptr };
        std::atomic<float>* exciterLevel { nullptr };
        std::atomic<float>* randomAmount { nullptr };
        std::atomic<float>* stereoSpread { nullptr };
        std::atomic<float>* seedLocked { nullptr };
        std::atomic<float>* seed { nullptr };

        std::atomic<float>* stiffness { nullptr };
        std::atomic<float>* bodyMix { nullptr };
        std::atomic<float>* bodyDecay { nullptr };
        std::atomic<float>* bodyBrightness { nullptr };
        std::atomic<float>* bodyModes { nullptr };
        std::atomic<float>* bodyPreset { nullptr };

        std::atomic<float>* layerBEnable { nullptr };
        std::atomic<float>* layerBInterval { nullptr };
        std::atomic<float>* layerBDetune { nullptr };
        std::atomic<float>* layerBLevel { nullptr };

        std::atomic<float>* lfo1Wave { nullptr };
        std::atomic<float>* lfo1Rate { nullptr };
        std::atomic<float>* lfo1Sync { nullptr };
        std::atomic<float>* lfo1Division { nullptr };
        std::atomic<float>* lfo1Depth { nullptr };
        std::atomic<float>* lfo1Dest { nullptr };
        std::atomic<float>* lfo2Wave { nullptr };
        std::atomic<float>* lfo2Rate { nullptr };
        std::atomic<float>* lfo2Sync { nullptr };
        std::atomic<float>* lfo2Division { nullptr };
        std::atomic<float>* lfo2Depth { nullptr };
        std::atomic<float>* lfo2Dest { nullptr };

        std::atomic<float>* envAttack { nullptr };
        std::atomic<float>* envDecay { nullptr };
        std::atomic<float>* envSustain { nullptr };
        std::atomic<float>* envRelease { nullptr };
        std::atomic<float>* envDepth { nullptr };
        std::atomic<float>* envDest { nullptr };

        std::atomic<float>* chaosAmount { nullptr };
        std::atomic<float>* chaosRate { nullptr };
        std::atomic<float>* chaosSync { nullptr };
        std::atomic<float>* chaosDivision { nullptr };
        std::atomic<float>* chaosBias { nullptr };

        std::atomic<float>* macroMaterial { nullptr };
        std::atomic<float>* macroAttack { nullptr };
        std::atomic<float>* macroDecay { nullptr };
        std::atomic<float>* macroBrightness { nullptr };
        std::atomic<float>* macroBody { nullptr };
        std::atomic<float>* macroChaos { nullptr };
        std::atomic<float>* macroSpace { nullptr };
        std::atomic<float>* macroDrive { nullptr };

        std::atomic<float>* filterType { nullptr };
        std::atomic<float>* filterCutoff { nullptr };
        std::atomic<float>* filterRes { nullptr };
        std::atomic<float>* filterMix { nullptr };
        std::atomic<float>* satMode { nullptr };
        std::atomic<float>* satDrive { nullptr };
        std::atomic<float>* satTone { nullptr };
        std::atomic<float>* satMix { nullptr };
        std::atomic<float>* delayTimeL { nullptr };
        std::atomic<float>* delayTimeR { nullptr };
        std::atomic<float>* delaySync { nullptr };
        std::atomic<float>* delayDivisionL { nullptr };
        std::atomic<float>* delayDivisionR { nullptr };
        std::atomic<float>* delayFeedback { nullptr };
        std::atomic<float>* delayMix { nullptr };
        std::atomic<float>* delayDamp { nullptr };
        std::atomic<float>* chorusRate { nullptr };
        std::atomic<float>* chorusSync { nullptr };
        std::atomic<float>* chorusDivision { nullptr };
        std::atomic<float>* chorusDepth { nullptr };
        std::atomic<float>* chorusMix { nullptr };
        std::atomic<float>* phaserRate { nullptr };
        std::atomic<float>* phaserSync { nullptr };
        std::atomic<float>* phaserDivision { nullptr };
        std::atomic<float>* phaserDepth { nullptr };
        std::atomic<float>* phaserFeedback { nullptr };
        std::atomic<float>* phaserMix { nullptr };
        std::atomic<float>* reverbSize { nullptr };
        std::atomic<float>* reverbDecay { nullptr };
        std::atomic<float>* reverbDamp { nullptr };
        std::atomic<float>* reverbWidth { nullptr };
        std::atomic<float>* reverbMix { nullptr };
        std::atomic<float>* ceiling { nullptr };
    };

    /** One-pole DC blocker for the engine mix. Outside the loop, so it cannot affect tuning. */
    struct DcBlocker
    {
        double lastInput { 0.0 };
        double lastOutput { 0.0 };

        void reset() noexcept { lastInput = 0.0; lastOutput = 0.0; }

        [[nodiscard]] double process (double input, double coefficient) noexcept
        {
            lastOutput = input - lastInput + coefficient * lastOutput;
            lastInput = input;

            return lastOutput;
        }
    };

    //==============================================================================
    /**
        Bus configuration for the instrument: no inputs, stereo output by default.

        This has to be a member because AudioProcessor::BusesProperties is a
        protected nested type, so a free helper function cannot name it.
    */
    [[nodiscard]] static BusesProperties makeBusesProperties();

    void resolveParameterHandles();

    /** Reads every parameter exactly once and returns the block's settings snapshot. */
    [[nodiscard]] engine::Settings buildEngineSettings() const;
    [[nodiscard]] engine::ArpSettings buildArpSettings() const;
    [[nodiscard]] dsp::FxRack::Settings buildFxSettings() const;

    void applyRealtimeModulation (engine::Settings&, dsp::FxRack::Settings&, int numSamples) noexcept;

    /** Reads the host playhead once per block. Safe on the audio thread. */
    void captureHostTempo() noexcept;

    /** Shared implementation of the float and double processBlock() overloads. */
    template <typename FloatType>
    void processBlockInternal (juce::AudioBuffer<FloatType>&, juce::MidiBuffer&);

    /** Renders the instrument into `buffer`, which arrives cleared. */
    template <typename FloatType>
    void renderEngine (juce::AudioBuffer<FloatType>& buffer, juce::MidiBuffer& midiMessages);

    /** Fills the double scratch buffers for one segment, splitting at MIDI events. */
    void renderEngineSegment (int segmentStart, int segmentLength, const juce::MidiBuffer& midiMessages);

    /** DC-blocks the scratch buffers and adds them into the output at `segmentStart`. */
    template <typename FloatType>
    void mixSegmentInto (juce::AudioBuffer<FloatType>& buffer, int segmentStart, int segmentLength);

    void handleMidiMessage (const juce::MidiMessage& message) noexcept;

    /** Applies the smoothed output gain and publishes the peak level for the UI. */
    template <typename FloatType>
    void applyOutputStage (juce::AudioBuffer<FloatType>& buffer);

    //==============================================================================
    juce::UndoManager undoManager;
    juce::AudioProcessorValueTreeState valueTreeState;
    ParameterHandles handles;

    engine::VoiceEngine voiceEngine;
    engine::Arpeggiator arpeggiator;
    juce::MidiBuffer arpMidi;
    bool arpWasEnabled { false };
    dsp::FxRack fxRack;
    dsp::Lfo lfo1;
    dsp::Lfo lfo2;
    dsp::EnvelopeFollower auxEnv;
    dsp::ChaosWalk chaos;

    // Engine mix scratch, in double regardless of the host's precision. The resonator
    // runs in double because a 1-cent error at 20 Hz is finer than float resolves once
    // a delay length has been derived from it; downgrading here would waste that.
    std::vector<double> engineLeft;
    std::vector<double> engineRight;

    DcBlocker dcBlockerLeft;
    DcBlocker dcBlockerRight;
    double dcBlockerCoefficient { 0.0 };

    // The gain smoother runs in double so the same ramp serves both the float and
    // double processing paths. Per-sample values are written into a scratch buffer
    // that is sized in prepareToPlay(), never in processBlock().
    juce::SmoothedValue<double, juce::ValueSmoothingTypes::Linear> outputGain;
    std::vector<double> outputGainRamp;

    double currentSampleRate  { 0.0 };
    int    currentBlockSize   { 0 };

    std::atomic<float> outputPeakLevel   { 0.0f };
    std::atomic<int>   midiEventCount    { 0 };
    std::atomic<int>   activeVoiceCount  { 0 };
    std::atomic<int>   blockSizeOverruns { 0 };
    std::atomic<float> lastNoteHz        { 0.0f };
    std::atomic<double> hostTempoBpm     { 120.0 };
    std::atomic<int>   arpStep           { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AethrProcessor)
};

} // namespace aethr
