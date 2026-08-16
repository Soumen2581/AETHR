#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include <atomic>
#include <vector>

#include "Parameters/ParameterLayout.h"

namespace strata
{

/**
    Top-level AudioProcessor for STRATA.

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

    Phase 1 scope: the processor is a fully wired, host-valid shell. It manages
    parameters and state, honours bus layouts, and renders silence through the
    output stage. The excitation/resonator engine arrives in Phase 2, behind
    renderEngine().

    @see docs/ARCHITECTURE.md
*/
class StrataProcessor final : public juce::AudioProcessor
{
public:
    StrataProcessor();
    ~StrataProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;

    using juce::AudioProcessor::processBlock;

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
    juce::AudioProcessorValueTreeState& getValueTreeState() noexcept { return valueTreeState; }

    /** Most recent output peak, in linear gain, for the editor's meter. Lock-free. */
    [[nodiscard]] float getOutputPeakLevel() const noexcept { return outputPeakLevel.load (std::memory_order_relaxed); }

    /** Total MIDI messages seen since the last prepareToPlay(). Lets the UI prove MIDI is arriving. */
    [[nodiscard]] int getMidiEventCount() const noexcept { return midiEventCount.load (std::memory_order_relaxed); }

    /** Voice count currently selected by the (non-automatable) polyphony parameter. */
    [[nodiscard]] int getSelectedPolyphony() const noexcept;

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
        Bus configuration for the instrument: no inputs, stereo output by default.

        This has to be a member because AudioProcessor::BusesProperties is a
        protected nested type, so a free helper function cannot name it.
    */
    [[nodiscard]] static BusesProperties makeBusesProperties();

    /** Shared implementation of the float and double processBlock() overloads. */
    template <typename FloatType>
    void processBlockInternal (juce::AudioBuffer<FloatType>&, juce::MidiBuffer&);

    /**
        Renders the instrument into `buffer`, which arrives cleared.

        Phase 1 leaves it silent; Phase 2 onwards this dispatches MIDI to the
        voice manager and sums the active voices.
    */
    template <typename FloatType>
    void renderEngine (juce::AudioBuffer<FloatType>& buffer, juce::MidiBuffer& midiMessages);

    /** Applies the smoothed output gain and publishes the peak level for the UI. */
    template <typename FloatType>
    void applyOutputStage (juce::AudioBuffer<FloatType>& buffer);

    //==============================================================================
    juce::AudioProcessorValueTreeState valueTreeState;

    // Cached raw parameter pointers. APVTS guarantees these stay valid for the
    // processor's lifetime, so the audio thread never looks parameters up by name.
    std::atomic<float>* outputGainDbParam { nullptr };
    std::atomic<float>* polyphonyParam    { nullptr };

    // The gain smoother runs in double so the same ramp serves both the float and
    // double processing paths. Per-sample values are written into a scratch buffer
    // that is sized in prepareToPlay(), never in processBlock().
    juce::SmoothedValue<double, juce::ValueSmoothingTypes::Linear> outputGain;
    std::vector<double> outputGainRamp;

    double currentSampleRate  { 0.0 };
    int    currentBlockSize   { 0 };

    std::atomic<float> outputPeakLevel   { 0.0f };
    std::atomic<int>   midiEventCount    { 0 };
    std::atomic<int>   blockSizeOverruns { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StrataProcessor)
};

} // namespace strata
