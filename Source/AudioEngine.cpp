#include "AudioEngine.h"

namespace am
{
AudioEngine::AudioEngine (std::array<Pad, kNumPads>& padArray) : pads (padArray)
{
    for (auto& f : playFrac)
        f.store (-1.f);
}

void AudioEngine::prepare (double sampleRate)
{
    sr = sampleRate;
    attackSamples  = std::max (8, (int) (0.0015 * sr));
    chokeSamples   = std::max (16, (int) (0.008 * sr));
    releaseSamples = std::max (16, (int) (0.012 * sr));
    stopAll();
}

void AudioEngine::stopAll()
{
    for (auto& v : voices)
        v.active = false;
    activeMask.store (0);
}

void AudioEngine::trigger (int padIndex, float velocity, bool honourGate)
{
    if (padIndex < 0 || padIndex >= kNumPads)
        return;

    Pad& pad = pads[(size_t) padIndex];
    SamplePtr smp = pad.getSample();
    if (smp == nullptr || smp->length() < 2)
        return;

    const int n = smp->length();
    double s = juce::jlimit (0.0, 1.0, (double) pad.start.load()) * n;
    double e = juce::jlimit (0.0, 1.0, (double) pad.end.load()) * n;
    if (e < s + 2.0)
    {
        e = std::min ((double) n, s + 2.0);
        s = std::max (0.0, e - 2.0);
    }

    const int choke = pad.chokeGroup.load();
    if (choke > 0)
        for (auto& v : voices)
            if (v.active && v.choke == choke && ! v.releasing)
                v.release (chokeSamples);

    SampleVoice* voice = nullptr;
    for (auto& v : voices)
        if (! v.active) { voice = &v; break; }

    if (voice == nullptr)
    {
        voice = &voices[0];
        for (auto& v : voices)
            if (v.order < voice->order)
                voice = &v;
    }

    const float vel = std::pow (juce::jlimit (0.f, 1.f, velocity), 1.3f);
    const float sg = (pad.normalize.load() && smp->peak > 1.0e-6f) ? 1.f / smp->peak : 1.f;
    const double rate = smp->sampleRate / sr;
    const bool gate = honourGate && pad.playMode.load() == 1;

    voice->start (std::move (smp), padIndex, s, e, pad.reverse.load(), rate, vel, sg, choke, gate, attackSamples);
    voice->order = ++counter;
    triggerMask.fetch_or ((juce::uint64) 1 << padIndex);
}

void AudioEngine::releasePad (int padIndex)
{
    for (auto& v : voices)
        if (v.active && v.pad == padIndex && v.gate)
            v.release (releaseSamples);
}

void AudioEngine::render (float* L, float* R, int n)
{
    for (auto& f : playFrac)
        f.store (-1.f, std::memory_order_relaxed);

    juce::uint64 mask = 0;

    for (auto& v : voices)
    {
        if (! v.active)
            continue;

        const Pad& pad = pads[(size_t) v.pad];
        const float pan = pad.pan.load();
        const float g = pad.volume.load() * v.velGain * v.sampleGain;
        const float tl = g * std::min (1.f, 1.f - pan);
        const float tr = g * std::min (1.f, 1.f + pan);
        const double inc = v.baseRate * std::pow (2.0, (double) pad.pitch.load() / 12.0);

        v.render (L, R, n, tl, tr, inc);

        if (v.active)
        {
            mask |= (juce::uint64) 1 << v.pad;
            const int len = v.sample->length();
            playFrac[(size_t) v.pad].store (len > 0 ? (float) (v.pos / (double) len) : 0.f, std::memory_order_relaxed);
        }
    }

    activeMask.store (mask);
}
} // namespace am
