#include "FactoryKits.h"

// All factory sounds are synthesized here from scratch (sine sweeps, filtered noise, FM).
// They are original placeholder sounds: no commercial samples are included.
namespace am::Factory
{
namespace
{
constexpr double kSR = 44100.0;
constexpr float kTwoPi = 6.28318530718f;

struct Style
{
    float kickF0, kickF1, kickDecay, kickClick, snareTone, snareNoise, snareDecay,
          hatC, hatO, bassF, bassDecay, drive, percF, clapGap;
};

Style styleFor (int kit)
{
    switch (kit)
    {
        case 0:  return { 165.f, 46.f, 6.5f, 0.30f, 185.f, 0.95f, 24.f, 70.f,  9.f, 43.f, 1.7f, 2.4f, 520.f, 0.011f }; // ATLANTA 09
        case 1:  return { 140.f, 52.f, 8.0f, 0.45f, 210.f, 1.10f, 30.f, 85.f, 11.f, 48.f, 2.2f, 3.2f, 640.f, 0.009f }; // BRICK SQUAD
        case 2:  return { 120.f, 40.f, 5.0f, 0.20f, 170.f, 0.80f, 20.f, 55.f,  7.f, 36.f, 1.2f, 3.8f, 410.f, 0.014f }; // DIRTY SOUTH
        case 3:  return { 150.f, 38.f, 4.2f, 0.18f, 195.f, 0.85f, 26.f, 75.f, 10.f, 32.f, 0.9f, 4.5f, 480.f, 0.010f }; // GOLDMINE 808S
        default: return { 180.f, 60.f, 9.0f, 0.35f, 260.f, 0.70f, 34.f, 95.f, 14.f, 55.f, 3.0f, 2.0f, 760.f, 0.008f }; // STREET PERCUSSION
    }
}

juce::AudioBuffer<float> mk (double sec)
{
    juce::AudioBuffer<float> b (1, (int) (sec * kSR));
    b.clear();
    return b;
}

struct HP
{
    float lp = 0.f, a = 0.1f;
    explicit HP (float fc) { a = 1.f - std::exp (-kTwoPi * fc / (float) kSR); }
    float run (float x) { lp += a * (x - lp); return x - lp; }
};

inline float tsec (int i) { return (float) i / (float) kSR; }

void finish (juce::AudioBuffer<float>& b)
{
    auto* d = b.getWritePointer (0);
    const int n = b.getNumSamples();
    float pk = 0.f;
    for (int i = 0; i < n; ++i) pk = std::max (pk, std::abs (d[i]));
    if (pk > 1.0e-6f)
    {
        const float g = 0.89f / pk;
        for (int i = 0; i < n; ++i) d[i] *= g;
    }
    const int fi = (int) (0.001 * kSR), fo = (int) (0.006 * kSR);
    for (int i = 0; i < fi && i < n; ++i) d[i] *= (float) i / (float) fi;
    for (int i = 0; i < fo && i < n; ++i) d[n - 1 - i] *= (float) i / (float) fo;
}

float noise (juce::Random& r) { return r.nextFloat() * 2.f - 1.f; }

juce::AudioBuffer<float> genKick (const Style& s, bool sub)
{
    auto b = mk (sub ? 0.9 : 0.55);
    auto* d = b.getWritePointer (0);
    juce::Random rng (11);
    float ph = 0.f;
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const float t = tsec (i);
        const float f = s.kickF1 + (s.kickF0 - s.kickF1) * std::exp (-t * 40.f);
        ph += kTwoPi * f / (float) kSR;
        const float env = std::exp (-t * s.kickDecay * (sub ? 0.55f : 1.f));
        float x = std::sin (ph) * env;
        x += noise (rng) * std::exp (-t * 600.f) * s.kickClick;
        d[i] = std::tanh (x * s.drive * (sub ? 0.8f : 1.f));
    }
    finish (b);
    return b;
}

juce::AudioBuffer<float> genSnare (const Style& s)
{
    auto b = mk (0.35);
    auto* d = b.getWritePointer (0);
    juce::Random rng (23);
    HP hp (1800.f);
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const float t = tsec (i);
        const float tone = std::sin (kTwoPi * s.snareTone * t) * 0.6f + std::sin (kTwoPi * s.snareTone * 1.75f * t) * 0.3f;
        const float n = hp.run (noise (rng)) * std::exp (-t * s.snareDecay) * s.snareNoise;
        d[i] = tone * std::exp (-t * s.snareDecay * 1.4f) + n;
    }
    finish (b);
    return b;
}

juce::AudioBuffer<float> genClap (const Style& s)
{
    auto b = mk (0.4);
    auto* d = b.getWritePointer (0);
    juce::Random rng (31);
    HP hp (900.f);
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const float t = tsec (i);
        float env = 0.f;
        for (int k = 0; k < 4; ++k)
        {
            const float tt = t - (float) k * s.clapGap;
            if (tt >= 0.f) env += std::exp (-tt * 170.f);
        }
        env += std::exp (-t * 14.f) * 0.35f;
        d[i] = hp.run (noise (rng)) * env;
    }
    finish (b);
    return b;
}

juce::AudioBuffer<float> genHat (const Style& s, bool open)
{
    auto b = mk (open ? 0.5 : 0.12);
    auto* d = b.getWritePointer (0);
    juce::Random rng (open ? 47 : 41);
    HP hp (6500.f);
    const float dec = open ? s.hatO : s.hatC;
    static const float fr[] = { 3200.f, 4100.f, 5300.f, 6700.f, 8100.f, 9400.f };
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const float t = tsec (i);
        float m = 0.f;
        for (float f : fr) m += std::sin (kTwoPi * f * t) > 0.f ? 0.1f : -0.1f;
        d[i] = (hp.run (noise (rng)) * 0.7f + m) * std::exp (-t * dec);
    }
    finish (b);
    return b;
}

juce::AudioBuffer<float> genRim()
{
    auto b = mk (0.08);
    auto* d = b.getWritePointer (0);
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const float t = tsec (i);
        d[i] = (std::sin (kTwoPi * 1700.f * t) + std::sin (kTwoPi * 480.f * t) * 0.8f) * std::exp (-t * 90.f);
    }
    finish (b);
    return b;
}

juce::AudioBuffer<float> genTom (float f)
{
    auto b = mk (0.5);
    auto* d = b.getWritePointer (0);
    float ph = 0.f;
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const float t = tsec (i);
        const float fq = f + f * 0.4f * std::exp (-t * 30.f);
        ph += kTwoPi * fq / (float) kSR;
        d[i] = std::sin (ph) * std::exp (-t * 9.f);
    }
    finish (b);
    return b;
}

juce::AudioBuffer<float> gen808 (const Style& s, float f)
{
    auto b = mk (1.6);
    auto* d = b.getWritePointer (0);
    float ph = 0.f;
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const float t = tsec (i);
        const float fq = f + f * 0.8f * std::exp (-t * 70.f);
        ph += kTwoPi * fq / (float) kSR;
        d[i] = std::tanh (std::sin (ph) * std::exp (-t * s.bassDecay) * s.drive);
    }
    finish (b);
    return b;
}

juce::AudioBuffer<float> genPerc (const Style& s)
{
    auto b = mk (0.3);
    auto* d = b.getWritePointer (0);
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const float t = tsec (i);
        d[i] = std::sin (kTwoPi * s.percF * t + 3.f * std::sin (kTwoPi * s.percF * 1.5f * t)) * std::exp (-t * 18.f);
    }
    finish (b);
    return b;
}

juce::AudioBuffer<float> genCowbell()
{
    auto b = mk (0.35);
    auto* d = b.getWritePointer (0);
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const float t = tsec (i);
        d[i] = std::tanh (std::sin (kTwoPi * 540.f * t) + std::sin (kTwoPi * 800.f * t)) * std::exp (-t * 9.f);
    }
    finish (b);
    return b;
}

juce::AudioBuffer<float> genShaker()
{
    auto b = mk (0.2);
    auto* d = b.getWritePointer (0);
    juce::Random rng (53);
    HP hp (5000.f);
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const float t = tsec (i);
        const float env = t < 0.02f ? t / 0.02f : std::exp (-(t - 0.02f) * 18.f);
        d[i] = hp.run (noise (rng)) * env;
    }
    finish (b);
    return b;
}

juce::AudioBuffer<float> genCrash()
{
    auto b = mk (1.4);
    auto* d = b.getWritePointer (0);
    juce::Random rng (59);
    HP hp (4500.f);
    static const float fr[] = { 2900.f, 4300.f, 5600.f, 7300.f, 9100.f };
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const float t = tsec (i);
        float m = 0.f;
        for (float f : fr) m += std::sin (kTwoPi * f * t) > 0.f ? 0.08f : -0.08f;
        d[i] = (hp.run (noise (rng)) * 0.6f + m) * std::exp (-t * 2.5f);
    }
    finish (b);
    return b;
}

juce::AudioBuffer<float> genSnap()
{
    auto b = mk (0.1);
    auto* d = b.getWritePointer (0);
    juce::Random rng (61);
    HP hp (3000.f);
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const float t = tsec (i);
        d[i] = hp.run (noise (rng)) * std::exp (-t * 220.f) + std::sin (kTwoPi * 2400.f * t) * std::exp (-t * 150.f) * 0.5f;
    }
    finish (b);
    return b;
}

juce::AudioBuffer<float> genChop (const Style& s)
{
    auto b = mk (0.4);
    auto* d = b.getWritePointer (0);
    float ph = 0.f;
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const float t = tsec (i);
        const float f = s.percF * 0.5f * (1.f + 0.5f * std::exp (-t * 12.f)) + 6.f * std::sin (kTwoPi * 6.f * t);
        ph += kTwoPi * f / (float) kSR;
        d[i] = std::tanh (std::sin (ph) * 2.f + std::sin (ph * 2.f) * 0.5f) * std::exp (-t * 8.f);
    }
    finish (b);
    return b;
}

const char* kSlotNames[16] = { "KICK", "KICK SUB", "SNARE", "CLAP", "HAT CL", "HAT OP", "RIM", "TOM",
                               "808 LOW", "808 MID", "PERC", "COWBELL", "SHAKER", "CRASH", "SNAP", "CHOP" };
} // namespace

juce::StringArray kitNames()
{
    return { "ATLANTA 09", "BRICK SQUAD", "DIRTY SOUTH", "GOLDMINE 808S", "STREET PERCUSSION" };
}

juce::String slotName (int slot) { return kSlotNames[juce::jlimit (0, 15, slot)]; }

std::shared_ptr<Sample> makeSample (int kit, int slot)
{
    kit = juce::jlimit (0, 4, kit);
    slot = juce::jlimit (0, 15, slot);
    const Style s = styleFor (kit);

    juce::AudioBuffer<float> b;
    switch (slot)
    {
        case 0:  b = genKick (s, false); break;
        case 1:  b = genKick (s, true);  break;
        case 2:  b = genSnare (s);       break;
        case 3:  b = genClap (s);        break;
        case 4:  b = genHat (s, false);  break;
        case 5:  b = genHat (s, true);   break;
        case 6:  b = genRim();           break;
        case 7:  b = genTom (s.snareTone * 0.7f); break;
        case 8:  b = gen808 (s, s.bassF);          break;
        case 9:  b = gen808 (s, s.bassF * 1.5f);   break;
        case 10: b = genPerc (s);        break;
        case 11: b = genCowbell();       break;
        case 12: b = genShaker();        break;
        case 13: b = genCrash();         break;
        case 14: b = genSnap();          break;
        default: b = genChop (s);        break;
    }
    finish (b);

    auto smp = std::make_shared<Sample>();
    smp->data = std::move (b);
    smp->sampleRate = kSR;
    smp->name = slotName (slot);
    smp->factoryId = juce::String (kit) + ":" + juce::String (slot);
    smp->peak = smp->data.getMagnitude (0, smp->data.getNumSamples());
    return smp;
}

void applyDemoPattern (Sequencer& seq, int kit)
{
    auto setPad = [&seq] (int pad, std::initializer_list<int> steps)
    {
        uint16_t m = 0;
        for (int st : steps) m = (uint16_t) (m | (1u << st));
        seq.masks[0][pad].store (m);
    };

    seq.clearPattern (0);
    switch (kit)
    {
        case 0:  setPad (0, { 0, 7, 10 });   setPad (2, { 4, 12 });  setPad (4, { 0, 2, 4, 6, 8, 10, 12, 14 }); setPad (5, { 14 }); setPad (8, { 0, 10 }); break;
        case 1:  setPad (0, { 0, 3, 8, 11 }); setPad (3, { 4, 12 }); setPad (4, { 0, 2, 3, 4, 6, 8, 10, 11, 12, 14 }); setPad (9, { 6 }); break;
        case 2:  setPad (0, { 0, 5, 8, 13 }); setPad (2, { 4, 12 }); setPad (4, { 2, 6, 10, 14 }); setPad (5, { 15 }); setPad (10, { 7, 11 }); break;
        case 3:  setPad (8, { 0, 6, 10 });   setPad (3, { 4, 12 });  setPad (4, { 0, 2, 4, 6, 8, 10, 12, 14 }); setPad (14, { 15 }); break;
        default: setPad (0, { 0, 8 });       setPad (6, { 4, 12 });  setPad (12, { 2, 6, 10, 14 }); setPad (11, { 0, 3, 8, 11 }); setPad (7, { 13, 15 }); break;
    }
}
} // namespace am::Factory
