#include "SampleVoice.h"

namespace am
{
void SampleVoice::start (SamplePtr s, int padIdx, double srcStart, double srcEnd, bool rev, double rate,
                         float vel, float smpGain, int chokeGroup, bool gateMode, int attackSamples)
{
    sample = std::move (s);
    pad = padIdx;
    regionStart = srcStart;
    regionEnd = srcEnd;
    reverse = rev;
    baseRate = rate;
    velGain = vel;
    sampleGain = smpGain;
    choke = chokeGroup;
    gate = gateMode;
    pos = rev ? std::max (srcStart, srcEnd - 1.0) : srcStart;
    attackLen = std::max (1, attackSamples);
    attackPos = 0;
    releasing = false;
    relGain = 1.f;
    relStep = 0.f;
    fresh = true;
    gainL = gainR = 0.f;
    active = true;
}

void SampleVoice::release (int numSamples) noexcept
{
    if (! active || releasing)
        return;
    releasing = true;
    relStep = relGain / (float) std::max (1, numSamples);
}

void SampleVoice::render (float* outL, float* outR, int numSamples, float tgtL, float tgtR, double inc) noexcept
{
    if (! active || sample == nullptr)
        return;

    const auto& buf = sample->data;
    const int n = buf.getNumSamples();
    const float* cl = buf.getReadPointer (0);
    const float* cr = buf.getNumChannels() > 1 ? buf.getReadPointer (1) : cl;

    float gl = fresh ? tgtL : gainL;
    float gr = fresh ? tgtR : gainR;
    fresh = false;
    const float dl = (tgtL - gl) / (float) numSamples;
    const float dr = (tgtR - gr) / (float) numSamples;
    const double tailLen = std::max (1.0, std::min (64.0, (regionEnd - regionStart) * 0.25));

    for (int i = 0; i < numSamples; ++i)
    {
        if (reverse ? (pos < regionStart) : (pos >= regionEnd))
        {
            active = false;
            break;
        }

        const int i0 = (int) pos;
        const float fr = (float) (pos - (double) i0);
        const int i1 = std::min (i0 + 1, n - 1);
        const float sl = cl[i0] + (cl[i1] - cl[i0]) * fr;
        const float sr = cr[i0] + (cr[i1] - cr[i0]) * fr;

        float env = 1.f;
        if (attackPos < attackLen)
            env = (float) attackPos++ / (float) attackLen;

        const double remain = reverse ? (pos - regionStart) : (regionEnd - pos);
        if (remain < tailLen)
            env *= (float) (remain / tailLen);

        if (releasing)
        {
            relGain -= relStep;
            if (relGain <= 0.f)
            {
                active = false;
                break;
            }
            env *= relGain;
        }

        gl += dl;
        gr += dr;
        outL[i] += sl * gl * env;
        outR[i] += sr * gr * env;
        pos += reverse ? -inc : inc;
    }

    gainL = tgtL;
    gainR = tgtR;
}
} // namespace am
