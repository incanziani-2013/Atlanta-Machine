#include "Pad.h"

namespace am
{
void Pad::purgeRetired()
{
    retired.erase (std::remove_if (retired.begin(), retired.end(),
                                   [] (const SamplePtr& p) { return p.use_count() <= 1; }),
                   retired.end());
}

void Pad::setSample (SamplePtr s)
{
    purgeRetired();
    auto old = std::atomic_exchange (&sample, std::move (s));
    if (old != nullptr)
        retired.push_back (std::move (old));
}

PadSnapshot Pad::snapshot() const
{
    PadSnapshot s;
    s.volume = volume; s.pitch = pitch; s.pan = pan; s.start = start; s.end = end;
    s.reverse = reverse; s.normalize = normalize;
    s.chokeGroup = chokeGroup; s.midiNote = midiNote; s.playMode = playMode;
    s.name = name; s.missingPath = missingPath; s.sample = getSample();
    return s;
}

void Pad::restore (const PadSnapshot& s)
{
    volume = s.volume; pitch = s.pitch; pan = s.pan; start = s.start; end = s.end;
    reverse = s.reverse; normalize = s.normalize;
    chokeGroup = s.chokeGroup; midiNote = s.midiNote; playMode = s.playMode;
    name = s.name; missingPath = s.missingPath;
    setSample (s.sample);
}

void Pad::resetSettings()
{
    volume = 1.f; pitch = 0.f; pan = 0.f; start = 0.f; end = 1.f;
    reverse = false; normalize = false; chokeGroup = 0; playMode = 0;
    midiNote = 36 + index;
}

void Pad::clear()
{
    resetSettings();
    name = {};
    missingPath = {};
    setSample (nullptr);
}
} // namespace am
