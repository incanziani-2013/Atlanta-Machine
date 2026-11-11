#pragma once
#include "Common.h"

namespace am
{
// 16 steps x 8 patterns x 64 pads. Steps are stored as 16-bit masks (lock-free, GUI and audio thread share them).
// Timing comes only from the audio callback (host PPQ or the internal clock); no GUI timers involved.
class Sequencer
{
public:
    Sequencer() { clearAll(); }

    std::atomic<uint16_t> masks[kNumPatterns][kNumPads];
    std::atomic<int>  activePattern { 0 }, length { 16 }, currentStep { -1 };
    std::atomic<bool> running { false }, recording { false }, followingHost { false };

    void clearAll()
    {
        for (auto& pat : masks)
            for (auto& m : pat)
                m.store (0);
    }
    void clearPattern (int p) { for (auto& m : masks[p]) m.store (0); }
    void clearPad (int p, int pad) { masks[p][pad].store (0); }

    bool getStep (int pad, int step) const { return ((masks[activePattern.load()][pad].load() >> step) & 1) != 0; }
    void toggleStep (int pad, int step) { masks[activePattern.load()][pad].fetch_xor ((uint16_t) (1u << step)); }

    void recordHit (int pad, double ppq)
    {
        const int len = juce::jlimit (1, kNumSteps, length.load());
        const long long k = (long long) std::llround (ppq / 0.25);
        if (k < 0) return;
        const int step = (int) (k % len);
        masks[activePattern.load()][pad].fetch_or ((uint16_t) (1u << step));
    }

    // Calls fn(sampleOffset, padIndex, velocity) for every step event inside [ppqStart, ppqStart + n*ppqPerSample).
    // swing 0..1 delays odd steps by up to a third of a step.
    template <class Fn>
    void scheduleBlock (double ppqStart, double ppqPerSample, int n, float swing, Fn&& fn)
    {
        constexpr double stepPpq = 0.25;
        const int len = juce::jlimit (1, kNumSteps, length.load());
        const int pat = juce::jlimit (0, kNumPatterns - 1, activePattern.load());
        const double ppqEnd = ppqStart + ppqPerSample * (double) n;
        const double swingPpq = (double) swing * stepPpq / 3.0;

        if (ppqStart >= 0.0)
            currentStep.store ((int) ((long long) std::floor (ppqStart / stepPpq) % len));

        const long long kFirst = (long long) std::floor (ppqStart / stepPpq) - 1;
        const long long kLast  = (long long) std::floor (ppqEnd / stepPpq) + 1;

        for (long long k = kFirst; k <= kLast; ++k)
        {
            if (k < 0) continue;
            const double t = (double) k * stepPpq + ((k & 1) ? swingPpq : 0.0);
            if (t < ppqStart || t >= ppqEnd) continue;

            int offset = (int) ((t - ppqStart) / ppqPerSample);
            offset = juce::jlimit (0, n - 1, offset);
            const int step = (int) (k % len);
            currentStep.store (step);

            for (int pad = 0; pad < kNumPads; ++pad)
                if ((masks[pat][pad].load (std::memory_order_relaxed) >> step) & 1)
                    fn (offset, pad, 0.85f);
        }
    }

    juce::String patternToString (int p) const;
    void patternFromString (int p, const juce::String& s);
    void writeXml (juce::XmlElement& parent) const;
    void readXml (const juce::XmlElement& parent);
};
} // namespace am
