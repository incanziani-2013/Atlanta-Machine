#pragma once
#include "Pad.h"

namespace am
{
struct SampleVoice
{
    bool active = false, fresh = true, releasing = false, reverse = false, gate = false;
    int pad = -1, choke = 0, attackPos = 0, attackLen = 64;
    SamplePtr sample;
    double pos = 0.0, baseRate = 1.0, regionStart = 0.0, regionEnd = 0.0;
    float velGain = 1.f, sampleGain = 1.f, gainL = 0.f, gainR = 0.f, relGain = 1.f, relStep = 0.f;
    juce::uint64 order = 0;

    void start (SamplePtr s, int padIdx, double srcStart, double srcEnd, bool rev, double rate,
                float vel, float smpGain, int chokeGroup, bool gateMode, int attackSamples);
    void release (int numSamples) noexcept;
    // Adds into outL/outR. tgtL/tgtR are the gains to ramp to across this block.
    void render (float* outL, float* outR, int numSamples, float tgtL, float tgtR, double inc) noexcept;
};
} // namespace am
