#pragma once
#include "Common.h"
#include "Pad.h"
#include "AudioEngine.h"
#include "Sequencer.h"
#include "MidiManager.h"
#include "DrumKitManager.h"
#include "Effects.h"

namespace am
{
class AtlantaProcessor : public juce::AudioProcessor
{
public:
    AtlantaProcessor();
    ~AtlantaProcessor() override = default;

    // ---- AudioProcessor
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;
    bool supportsDoublePrecisionProcessing() const override { return true; }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "ATLANTA MACHINE"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- shared model (GUI uses these directly)
    std::array<Pad, kNumPads> pads;
    AudioEngine engine;
    Sequencer seq;
    MidiManager midi;
    DrumKitManager kits;
    juce::AudioProcessorValueTreeState apvts;

    std::array<std::atomic<int>, 16> keyCodes;   // computer-keyboard shortcut per pad slot
    std::atomic<int> selectedBank { 0 }, selectedPad { 0 };
    std::atomic<double> displayBpm { 140.0 };

    juce::StringArray missingSamples;   // filled when a project/kit is restored with unreachable files
    bool missingWarned = false;

    // ---- GUI -> audio thread (lock-free)
    void uiTrigger (int pad, float velocity);
    void uiRelease (int pad);
    void startSequencer();
    void stopSequencer();

    // ---- editing helpers (message thread)
    bool loadSampleToPad (int pad, const juce::File& file, juce::String& error);
    void pushUndo (int pad);
    bool undo (int& padOut);
    void purgeRetired();
    void resetKeyDefaults();

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    struct UiEvent { int pad; float vel; int kind; };   // kind: 0 = on, 1 = off
    struct Ev { int offset; int pad; float vel; int kind; }; // kind: 0 on(gate ok), 1 off, 2 sequencer on

    void renderInternal (float* L, float* R, int n, juce::MidiBuffer& midiMessages);
    void pushUi (UiEvent e);

    FXChain fx;
    juce::AbstractFifo fifo { 256 };
    UiEvent fifoBuf[256];
    std::array<Ev, 2048> evs;

    double sampleRateHz = 44100.0;
    double internalPpq = 0.0;
    std::atomic<bool> resetInternalPpq { false };

    juce::SmoothedValue<float> smVol, smPan;
    juce::AudioBuffer<float> scratch;

    std::atomic<float> *pMainVol = nullptr, *pMainPan = nullptr, *pBpm = nullptr, *pSync = nullptr, *pSwing = nullptr;
    std::atomic<float> *pAmt[4] {}, *pMix[4] {};

    struct UndoEntry { int pad; PadSnapshot snap; };
    std::vector<UndoEntry> undoStack;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AtlantaProcessor)
};
} // namespace am
