#include "Sequencer.h"

namespace am
{
juce::String Sequencer::patternToString (int p) const
{
    juce::String s;
    for (int pad = 0; pad < kNumPads; ++pad)
        s << juce::String::toHexString ((int) masks[p][pad].load()).paddedLeft ('0', 4);
    return s;
}

void Sequencer::patternFromString (int p, const juce::String& s)
{
    for (int pad = 0; pad < kNumPads; ++pad)
    {
        const auto chunk = s.substring (pad * 4, pad * 4 + 4);
        masks[p][pad].store (chunk.length() == 4 ? (uint16_t) (chunk.getHexValue32() & 0xffff) : (uint16_t) 0);
    }
}

void Sequencer::writeXml (juce::XmlElement& parent) const
{
    auto* e = parent.createNewChildElement ("SEQ");
    e->setAttribute ("length", length.load());
    e->setAttribute ("active", activePattern.load());
    for (int p = 0; p < kNumPatterns; ++p)
    {
        auto* pe = e->createNewChildElement ("PAT");
        pe->setAttribute ("i", p);
        pe->setAttribute ("data", patternToString (p));
    }
}

void Sequencer::readXml (const juce::XmlElement& parent)
{
    clearAll();
    if (auto* e = parent.getChildByName ("SEQ"))
    {
        length.store (juce::jlimit (1, kNumSteps, e->getIntAttribute ("length", 16)));
        activePattern.store (juce::jlimit (0, kNumPatterns - 1, e->getIntAttribute ("active", 0)));
        for (auto* pe : e->getChildWithTagNameIterator ("PAT"))
        {
            const int i = pe->getIntAttribute ("i", -1);
            if (i >= 0 && i < kNumPatterns)
                patternFromString (i, pe->getStringAttribute ("data"));
        }
    }
}
} // namespace am
