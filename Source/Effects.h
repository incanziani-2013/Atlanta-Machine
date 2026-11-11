#pragma once
#include "Common.h"

namespace am
{
// Series chain: distortion -> chorus -> delay -> reverb. Each stage is skipped when its mix is ~0.
// Index order for parameters: 0 reverb, 1 delay, 2 chorus, 3 distortion.
class FXChain
{
public:
    void prepare (double sampleRate, int maxBlock);
    void process (float* L, float* R, int numSamples, const float* amount, const float* mix, double bpm);

private:
    double sr = 44100.0;
    juce::SmoothedValue<float> sAmt[4], sMix[4];

    std::vector<float> chL, chR;
    int chLen = 0, chW = 0;
    float chPhase = 0.f;

    std::vector<float> dlL, dlR;
    int dlLen = 0, dlW = 0;
    juce::SmoothedValue<float> sDelay;

    juce::Reverb reverb;
};
} // namespace am
