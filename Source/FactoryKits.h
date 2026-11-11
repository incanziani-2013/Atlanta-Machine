#pragma once
#include "Pad.h"
#include "Sequencer.h"

namespace am::Factory
{
juce::StringArray kitNames();
juce::String slotName (int slot);                         // 0..15
std::shared_ptr<Sample> makeSample (int kit, int slot);   // original, procedurally synthesized sounds
void applyDemoPattern (Sequencer& seq, int kit);          // writes pattern 1 for bank A pads
}
