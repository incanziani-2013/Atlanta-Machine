#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <juce_data_structures/juce_data_structures.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <memory>
#include <vector>

namespace am
{
constexpr int kNumBanks     = 4;
constexpr int kPadsPerBank  = 16;
constexpr int kNumPads      = kNumBanks * kPadsPerBank;
constexpr int kNumSteps     = 16;
constexpr int kNumPatterns  = 8;
constexpr int kMaxVoices    = 48;

inline juce::String brandName()
{
    return juce::String (juce::CharPointer_UTF8 ("GOLDMINE AUDIO\xc2\xae"));
}
} // namespace am
