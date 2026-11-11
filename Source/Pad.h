#pragma once
#include "Common.h"

namespace am
{
// Immutable once published: the audio thread only ever reads it.
struct Sample
{
    juce::AudioBuffer<float> data;
    double sampleRate = 44100.0;
    juce::String name;
    juce::File file, cacheFile;
    juce::String factoryId;   // "kit:slot" for built-in synthesized sounds
    float peak = 1.0f;

    int length() const noexcept { return data.getNumSamples(); }
};

using SamplePtr = std::shared_ptr<const Sample>;

struct PadSnapshot
{
    float volume = 1.f, pitch = 0.f, pan = 0.f, start = 0.f, end = 1.f;
    bool reverse = false, normalize = false;
    int chokeGroup = 0, midiNote = 36, playMode = 0;
    juce::String name, missingPath;
    SamplePtr sample;
};

// One drum pad. Numeric settings are atomics (written by the GUI, read by the audio thread).
// The sample pointer is swapped atomically; replaced samples are parked in a "retired" list
// on the message thread so the audio thread never frees memory.
class Pad
{
public:
    Pad() = default;
    Pad (const Pad&) = delete;
    Pad& operator= (const Pad&) = delete;

    void init (int padIndex) { index = padIndex; resetSettings(); }

    std::atomic<float> volume { 1.f }, pitch { 0.f }, pan { 0.f }, start { 0.f }, end { 1.f };
    std::atomic<bool>  reverse { false }, normalize { false };
    std::atomic<int>   chokeGroup { 0 };   // 0 = none, 1..4
    std::atomic<int>   midiNote { 36 };
    std::atomic<int>   playMode { 0 };     // 0 = one-shot, 1 = gate (note-off stops)

    SamplePtr getSample() const noexcept { return std::atomic_load (&sample); }
    void setSample (SamplePtr s);          // message thread only
    void purgeRetired();                   // message thread only

    PadSnapshot snapshot() const;
    void restore (const PadSnapshot&);
    void resetSettings();
    void clear();

    juce::String name;          // message thread only
    juce::String missingPath;   // non-empty when a referenced sample could not be found

private:
    int index = 0;
    SamplePtr sample;
    std::vector<SamplePtr> retired;
};
} // namespace am
