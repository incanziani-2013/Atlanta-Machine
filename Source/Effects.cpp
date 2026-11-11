#include "Effects.h"

namespace am
{
static inline float readInterp (const std::vector<float>& b, int w, float delay)
{
    const int len = (int) b.size();
    float rp = (float) w - delay;
    while (rp < 0.f) rp += (float) len;
    int i0 = (int) rp;
    const float fr = rp - (float) i0;
    if (i0 >= len) i0 -= len;
    int i1 = i0 + 1;
    if (i1 >= len) i1 -= len;
    return b[(size_t) i0] + (b[(size_t) i1] - b[(size_t) i0]) * fr;
}

void FXChain::prepare (double sampleRate, int)
{
    sr = sampleRate;
    for (int i = 0; i < 4; ++i)
    {
        sAmt[i].reset (sr, 0.03);
        sMix[i].reset (sr, 0.03);
        sAmt[i].setCurrentAndTargetValue (0.f);
        sMix[i].setCurrentAndTargetValue (0.f);
    }

    chLen = (int) (0.06 * sr) + 4;
    chL.assign ((size_t) chLen, 0.f);
    chR.assign ((size_t) chLen, 0.f);
    chW = 0;
    chPhase = 0.f;

    dlLen = (int) (2.0 * sr) + 4;
    dlL.assign ((size_t) dlLen, 0.f);
    dlR.assign ((size_t) dlLen, 0.f);
    dlW = 0;
    sDelay.reset (sr, 0.08);
    sDelay.setCurrentAndTargetValue ((float) (0.375 * sr));

    reverb.setSampleRate (sr);
    reverb.reset();
}

void FXChain::process (float* L, float* R, int n, const float* amt, const float* mix, double bpm)
{
    for (int k = 0; k < 4; ++k)
    {
        sAmt[k].setTargetValue (amt[k]);
        sMix[k].setTargetValue (mix[k]);
    }

    auto active = [this] (int k) { return sMix[k].getTargetValue() > 0.0005f || sMix[k].isSmoothing(); };

    // ---- distortion
    if (active (3))
    {
        for (int i = 0; i < n; ++i)
        {
            const float a = sAmt[3].getNextValue();
            const float m = sMix[3].getNextValue();
            const float drive = 1.f + a * 30.f;
            const float comp = 1.f / (1.f + a * 0.8f);
            L[i] = L[i] * (1.f - m) + std::tanh (L[i] * drive) * comp * m;
            R[i] = R[i] * (1.f - m) + std::tanh (R[i] * drive) * comp * m;
        }
    }

    // ---- chorus
    if (active (2))
    {
        const float rate = 0.6f / (float) sr;
        for (int i = 0; i < n; ++i)
        {
            const float a = sAmt[2].getNextValue();
            const float m = sMix[2].getNextValue();
            chL[(size_t) chW] = L[i];
            chR[(size_t) chW] = R[i];
            const float lfoL = std::sin (6.2831853f * chPhase);
            const float lfoR = std::sin (6.2831853f * (chPhase + 0.25f));
            const float dL = (0.014f + a * 0.006f * lfoL) * (float) sr;
            const float dR = (0.014f + a * 0.006f * lfoR) * (float) sr;
            const float wl = readInterp (chL, chW, dL);
            const float wr = readInterp (chR, chW, dR);
            L[i] = L[i] * (1.f - 0.5f * m) + wl * m * 0.8f;
            R[i] = R[i] * (1.f - 0.5f * m) + wr * m * 0.8f;
            if (++chW >= chLen) chW = 0;
            chPhase += rate;
            if (chPhase >= 1.f) chPhase -= 1.f;
        }
    }

    // ---- delay (dotted eighth, ping-pong feedback)
    if (active (1))
    {
        const double safeBpm = juce::jlimit (20.0, 400.0, bpm);
        sDelay.setTargetValue (juce::jlimit (1.f, (float) dlLen - 4.f, (float) (60.0 / safeBpm * 0.75 * sr)));
        for (int i = 0; i < n; ++i)
        {
            const float a = sAmt[1].getNextValue();
            const float m = sMix[1].getNextValue();
            const float d = sDelay.getNextValue();
            const float fb = a * 0.85f;
            const float wl = readInterp (dlL, dlW, d);
            const float wr = readInterp (dlR, dlW, d);
            dlL[(size_t) dlW] = L[i] + wr * fb;
            dlR[(size_t) dlW] = R[i] + wl * fb;
            L[i] += wl * m;
            R[i] += wr * m;
            if (++dlW >= dlLen) dlW = 0;
        }
    }

    // ---- reverb
    if (active (0))
    {
        const float a = sAmt[0].getTargetValue();
        const float m = sMix[0].getTargetValue();
        sAmt[0].skip (n);
        sMix[0].skip (n);
        juce::Reverb::Parameters rp;
        rp.roomSize = 0.35f + 0.6f * a;
        rp.damping = 0.45f;
        rp.width = 1.0f;
        rp.wetLevel = m * 0.33f;
        rp.dryLevel = 0.5f;
        rp.freezeMode = 0.f;
        reverb.setParameters (rp);
        reverb.processStereo (L, R, n);
    }
}
} // namespace am
