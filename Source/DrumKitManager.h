#pragma once
#include "Pad.h"
#include "Sequencer.h"

namespace am
{
// Kit format: a folder "<name>.amkit/" containing kit.xml + Samples/ (every sample is copied into the kit,
// so kits stay valid when the originals move). Missing samples are reported and kept as "MISSING" pads.
class DrumKitManager
{
public:
    DrumKitManager (std::array<Pad, kNumPads>& padArray, Sequencer& sequencer);

    juce::String kitName { "INIT KIT" };
    juce::File   kitFolder;           // folder of the currently loaded/saved user kit (invalid for factory/new kits)
    int          factoryIndex = -1;   // >= 0 when a factory kit is loaded

    static juce::StringArray factoryKitNames();
    static juce::File defaultKitsFolder();
    static juce::File cacheFolder();

    SamplePtr loadSampleFile (const juce::File& file, juce::String& error) const;

    void loadFactoryKit (int index);
    void newKit();
    bool resetKit (juce::StringArray& missing);
    bool saveKit (const juce::File& folder, juce::String& error);
    bool loadKit (const juce::File& folderOrXml, juce::StringArray& missing, juce::String& error);

    // Plugin state (used by get/setStateInformation)
    void writeState (juce::XmlElement& e) const;
    void readState (const juce::XmlElement& e, juce::StringArray& missing);

private:
    std::unique_ptr<juce::XmlElement> makePadXml (int i, const juce::File& kitDir, bool copySamples, juce::String& error) const;
    void readPad (const juce::XmlElement& e, const juce::File& kitDir, juce::StringArray& missing);
    SamplePtr resolveSample (const juce::XmlElement& e, const juce::File& kitDir, juce::String& missingPath) const;
    juce::File makeCacheCopy (const juce::File& f) const;
    static bool writeWav (const juce::File& f, const Sample& s);

    std::array<Pad, kNumPads>& pads;
    Sequencer& seq;
    mutable juce::AudioFormatManager formats;
};
} // namespace am
