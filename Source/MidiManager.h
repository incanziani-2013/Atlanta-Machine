#pragma once
#include "Pad.h"

namespace am
{
// MIDI note <-> pad mapping. Every pad stores its own note (default: pad index + 36 = C1 with C3 = 60 naming).
// octaveShift moves all assignments at lookup time, middleC only changes how notes are *named* (C3/C4/C5 = note 60).
class MidiManager
{
public:
    explicit MidiManager (std::array<Pad, kNumPads>& p) : pads (p) {}

    std::atomic<int> learnPad { -1 };     // pad waiting for the next note-on, -1 = off
    std::atomic<int> octaveShift { 0 };
    std::atomic<int> middleC { 3 };       // octave number that MIDI note 60 is displayed as (3, 4 or 5)

    int findPads (int note, int* out) const noexcept
    {
        int c = 0;
        const int sh = 12 * octaveShift.load();
        for (int i = 0; i < kNumPads; ++i)
            if (pads[(size_t) i].midiNote.load() + sh == note)
                out[c++] = i;
        return c;
    }

    juce::String noteName (int note) const
    {
        static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        if (note < 0 || note > 127) return "--";
        const int oct = note / 12 - 5 + middleC.load();
        return juce::String (names[note % 12]) + juce::String (oct);
    }

    // Accepts "36", "C1", "c#2", "Db3". Returns -1 when invalid.
    int parseNote (const juce::String& text) const
    {
        auto t = text.trim().toUpperCase();
        if (t.isEmpty()) return -1;
        if (t.containsOnly ("0123456789"))
        {
            const int v = t.getIntValue();
            return (v >= 0 && v <= 127) ? v : -1;
        }

        static const int base[] = { 9, 11, 0, 2, 4, 5, 7 }; // A B C D E F G
        const juce::juce_wchar letter = t[0];
        if (letter < 'A' || letter > 'G') return -1;
        int semi = base[letter - 'A'];
        int idx = 1;
        if (t[idx] == '#') { ++semi; ++idx; }
        else if (t[idx] == 'B' && t.length() > idx + 1) { --semi; ++idx; }

        const auto octText = t.substring (idx);
        if (octText.isEmpty() || ! octText.containsOnly ("-0123456789")) return -1;
        const int oct = octText.getIntValue();
        const int note = 12 * (oct + 5 - middleC.load()) + semi;
        return (note >= 0 && note <= 127) ? note : -1;
    }

private:
    std::array<Pad, kNumPads>& pads;
};
} // namespace am
