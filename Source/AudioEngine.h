#pragma once
#include "SampleVoice.h"

namespace am
{
class AudioEngine
{
public:
    explicit AudioEngine (std::array<Pad, kNumPads>& padArray);

    void prepare (double sampleRate);
    // honourGate: true for MIDI/GUI/keyboard (note-off may stop gate pads), false for the sequencer.
    void trigger (int padIndex, float velocity, bool honourGate);
    void releasePad (int padIndex);
    void stopAll();
    void render (float* L, float* R, int numSamples);   // adds into L/R

    juce::uint64 getActiveMask() const noexcept { return activeMask.load(); }
    juce::uint64 consumeTriggerMask() noexcept   { return triggerMask.exchange (0); }
    float getPlayFraction (int pad) const noexcept { return playFrac[(size_t) pad].load(); }

private:
    std::array<Pad, kNumPads>& pads;
    std::array<SampleVoice, kMaxVoices> voices;
    std::array<std::atomic<float>, kNumPads> playFrac;
    std::atomic<juce::uint64> activeMask { 0 }, triggerMask { 0 };
    double sr = 44100.0;
    juce::uint64 counter = 0;
    int attackSamples = 64, chokeSamples = 256, releaseSamples = 512;
};
} // namespace am
