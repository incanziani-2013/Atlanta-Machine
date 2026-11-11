#include "DrumKitManager.h"
#include "FactoryKits.h"

namespace am
{
DrumKitManager::DrumKitManager (std::array<Pad, kNumPads>& padArray, Sequencer& sequencer)
    : pads (padArray), seq (sequencer)
{
    formats.registerBasicFormats();   // WAV, AIFF, FLAC, OGG, and MP3 (JUCE_USE_MP3AUDIOFORMAT=1)
}

juce::StringArray DrumKitManager::factoryKitNames() { return Factory::kitNames(); }

juce::File DrumKitManager::defaultKitsFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
               .getChildFile ("GOLDMINE AUDIO").getChildFile ("ATLANTA MACHINE").getChildFile ("Kits");
}

juce::File DrumKitManager::cacheFolder()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("GOLDMINE AUDIO").getChildFile ("ATLANTA MACHINE").getChildFile ("SampleCache");
}

juce::File DrumKitManager::makeCacheCopy (const juce::File& f) const
{
    if (f.isAChildOf (cacheFolder()))
        return f;
    const auto c = cacheFolder().getChildFile (juce::String::toHexString (f.getFullPathName().hashCode64()) + "_" + f.getFileName());
    if (! c.existsAsFile() && f.getSize() < 64 * 1024 * 1024)
    {
        c.getParentDirectory().createDirectory();
        f.copyFileTo (c);
    }
    return c.existsAsFile() ? c : juce::File();
}

SamplePtr DrumKitManager::loadSampleFile (const juce::File& f, juce::String& error) const
{
    if (! f.existsAsFile())
    {
        error = "File not found: " + f.getFullPathName();
        return nullptr;
    }

    std::unique_ptr<juce::AudioFormatReader> r (formats.createReaderFor (f));
    if (r == nullptr || r->lengthInSamples <= 1)
    {
        error = "Unsupported or empty audio file: " + f.getFileName();
        return nullptr;
    }
    if (r->lengthInSamples > (juce::int64) (r->sampleRate * 600.0))
    {
        error = "Sample is longer than 10 minutes: " + f.getFileName();
        return nullptr;
    }

    const int nCh = (int) juce::jmin (2u, r->numChannels);
    const int len = (int) r->lengthInSamples;
    auto s = std::make_shared<Sample>();
    s->data.setSize (nCh, len);
    if (! r->read (&s->data, 0, len, 0, true, nCh > 1))
    {
        error = "Could not read audio data: " + f.getFileName();
        return nullptr;
    }
    s->sampleRate = r->sampleRate;
    s->name = f.getFileNameWithoutExtension().toUpperCase();
    s->file = f;
    s->cacheFile = makeCacheCopy (f);
    s->peak = s->data.getMagnitude (0, len);
    return s;
}

bool DrumKitManager::writeWav (const juce::File& f, const Sample& s)
{
    f.deleteFile();
    auto fos = std::make_unique<juce::FileOutputStream> (f);
    if (! fos->openedOk())
        return false;

    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (fos.get(), s.sampleRate,
                                                                    (unsigned int) s.data.getNumChannels(), 24, {}, 0));
    if (w == nullptr)
        return false;
    fos.release();   // the writer owns the stream now
    return w->writeFromAudioSampleBuffer (s.data, 0, s.data.getNumSamples());
}

// ---------------------------------------------------------------- kit operations

void DrumKitManager::loadFactoryKit (int index)
{
    const int count = Factory::kitNames().size();
    index = juce::jlimit (0, count - 1, index);

    for (int bank = 0; bank < kNumBanks; ++bank)
    {
        const int kit = (index + bank) % count;   // bank A = chosen kit, B..D = the following factory kits
        for (int slot = 0; slot < kPadsPerBank; ++slot)
        {
            Pad& p = pads[(size_t) (bank * kPadsPerBank + slot)];
            p.clear();
            p.name = Factory::slotName (slot);
            p.setSample (Factory::makeSample (kit, slot));
            if (slot == 4 || slot == 5)
                p.chokeGroup = 1;   // closed/open hat choke each other
        }
    }

    seq.clearAll();
    seq.length.store (16);
    seq.activePattern.store (0);
    Factory::applyDemoPattern (seq, index);

    kitName = Factory::kitNames()[index];
    kitFolder = juce::File();
    factoryIndex = index;
}

void DrumKitManager::newKit()
{
    for (auto& p : pads)
        p.clear();
    seq.clearAll();
    kitName = "NEW KIT";
    kitFolder = juce::File();
    factoryIndex = -1;
}

bool DrumKitManager::resetKit (juce::StringArray& missing)
{
    if (factoryIndex >= 0)
    {
        loadFactoryKit (factoryIndex);
        return true;
    }
    if (kitFolder.isDirectory())
    {
        juce::String err;
        return loadKit (kitFolder, missing, err);
    }
    newKit();
    return true;
}

std::unique_ptr<juce::XmlElement> DrumKitManager::makePadXml (int i, const juce::File& kitDir, bool copySamples, juce::String& error) const
{
    const Pad& p = pads[(size_t) i];
    auto e = std::make_unique<juce::XmlElement> ("PAD");
    e->setAttribute ("index", i);
    e->setAttribute ("name", p.name);
    e->setAttribute ("vol", (double) p.volume.load());
    e->setAttribute ("pitch", (double) p.pitch.load());
    e->setAttribute ("pan", (double) p.pan.load());
    e->setAttribute ("start", (double) p.start.load());
    e->setAttribute ("end", (double) p.end.load());
    e->setAttribute ("rev", p.reverse.load() ? 1 : 0);
    e->setAttribute ("norm", p.normalize.load() ? 1 : 0);
    e->setAttribute ("choke", p.chokeGroup.load());
    e->setAttribute ("note", p.midiNote.load());
    e->setAttribute ("mode", p.playMode.load());

    auto s = p.getSample();
    if (s == nullptr)
    {
        if (p.missingPath.isNotEmpty())
            e->setAttribute ("path", p.missingPath);
        return e;
    }

    if (copySamples)
    {
        // Kit file: every sample is stored inside the kit folder.
        const auto samplesDir = kitDir.getChildFile ("Samples");
        samplesDir.createDirectory();
        juce::String fileName = juce::File::createLegalFileName (s->name.isNotEmpty() ? s->name : "SAMPLE");
        const juce::String prefix = juce::String (i).paddedLeft ('0', 2) + "_";
        const bool copyOriginal = s->file.existsAsFile();
        const juce::String ext = copyOriginal ? s->file.getFileExtension() : juce::String (".wav");
        const juce::String rel = "Samples/" + prefix + fileName + ext;
        const auto dest = kitDir.getChildFile (rel);

        bool ok = true;
        if (copyOriginal)
        {
            if (dest != s->file)
                ok = s->file.copyFileTo (dest);
        }
        else
        {
            ok = writeWav (dest, *s);
        }
        if (! ok)
            error = "Could not write sample for pad " + juce::String (i + 1) + ": " + dest.getFullPathName();
        e->setAttribute ("rel", rel);
        e->setAttribute ("path", dest.getFullPathName());
    }
    else
    {
        // Plugin state: factory sounds are regenerated, user samples keep original + cached path.
        if (s->factoryId.isNotEmpty())
            e->setAttribute ("factory", s->factoryId);
        else
        {
            e->setAttribute ("path", s->file.getFullPathName());
            if (s->cacheFile != juce::File())
                e->setAttribute ("cache", s->cacheFile.getFullPathName());
        }
    }
    return e;
}

SamplePtr DrumKitManager::resolveSample (const juce::XmlElement& e, const juce::File& kitDir, juce::String& missingPath) const
{
    const auto fac = e.getStringAttribute ("factory");
    if (fac.isNotEmpty())
    {
        const int colon = fac.indexOfChar (':');
        if (colon > 0)
            return Factory::makeSample (fac.substring (0, colon).getIntValue(), fac.substring (colon + 1).getIntValue());
    }

    juce::StringArray candidates;
    const auto rel = e.getStringAttribute ("rel");
    if (rel.isNotEmpty() && kitDir != juce::File())
        candidates.add (kitDir.getChildFile (rel).getFullPathName());
    if (e.getStringAttribute ("path").isNotEmpty())
        candidates.add (e.getStringAttribute ("path"));
    if (e.getStringAttribute ("cache").isNotEmpty())
        candidates.add (e.getStringAttribute ("cache"));

    for (const auto& c : candidates)
    {
        juce::File f (c);
        if (f.existsAsFile())
        {
            juce::String err;
            if (auto s = loadSampleFile (f, err))
                return s;
        }
    }

    missingPath = candidates.isEmpty() ? juce::String() : candidates[0];
    return nullptr;
}

void DrumKitManager::readPad (const juce::XmlElement& e, const juce::File& kitDir, juce::StringArray& missing)
{
    const int idx = e.getIntAttribute ("index", -1);
    if (idx < 0 || idx >= kNumPads)
        return;

    Pad& p = pads[(size_t) idx];
    p.resetSettings();
    p.volume = (float) e.getDoubleAttribute ("vol", 1.0);
    p.pitch = (float) e.getDoubleAttribute ("pitch", 0.0);
    p.pan = (float) e.getDoubleAttribute ("pan", 0.0);
    p.start = (float) e.getDoubleAttribute ("start", 0.0);
    p.end = (float) e.getDoubleAttribute ("end", 1.0);
    p.reverse = e.getIntAttribute ("rev", 0) != 0;
    p.normalize = e.getIntAttribute ("norm", 0) != 0;
    p.chokeGroup = juce::jlimit (0, 4, e.getIntAttribute ("choke", 0));
    p.midiNote = juce::jlimit (0, 127, e.getIntAttribute ("note", 36 + idx));
    p.playMode = e.getIntAttribute ("mode", 0) != 0 ? 1 : 0;
    p.name = e.getStringAttribute ("name");
    p.missingPath = {};

    const bool referencesSample = e.hasAttribute ("factory") || e.hasAttribute ("rel")
                                  || e.hasAttribute ("path") || e.hasAttribute ("cache");
    juce::String miss;
    auto s = referencesSample ? resolveSample (e, kitDir, miss) : nullptr;
    p.setSample (s);

    if (s == nullptr && referencesSample)
    {
        p.missingPath = miss.isNotEmpty() ? miss : juce::String ("(unknown file)");
        missing.add ("Bank " + juce::String::charToString ("ABCD"[idx / kPadsPerBank]) + " pad "
                     + juce::String (idx % kPadsPerBank + 1) + ": " + p.missingPath);
    }
}

bool DrumKitManager::saveKit (const juce::File& folder, juce::String& error)
{
    if (! folder.createDirectory())
    {
        error = "Could not create kit folder: " + folder.getFullPathName();
        return false;
    }

    juce::XmlElement root ("ATLANTAKIT");
    root.setAttribute ("version", 1);
    root.setAttribute ("name", folder.getFileNameWithoutExtension());
    root.setAttribute ("factory", factoryIndex);

    for (int i = 0; i < kNumPads; ++i)
    {
        juce::String err;
        if (auto e = makePadXml (i, folder, true, err))
            root.addChildElement (e.release());
        if (err.isNotEmpty() && error.isEmpty())
            error = err;
    }
    seq.writeXml (root);

    if (error.isNotEmpty())
        return false;
    if (! root.writeTo (folder.getChildFile ("kit.xml")))
    {
        error = "Could not write kit.xml";
        return false;
    }

    kitName = folder.getFileNameWithoutExtension().toUpperCase();
    kitFolder = folder;
    factoryIndex = -1;
    return true;
}

bool DrumKitManager::loadKit (const juce::File& folderOrXml, juce::StringArray& missing, juce::String& error)
{
    const auto folder = folderOrXml.isDirectory() ? folderOrXml : folderOrXml.getParentDirectory();
    const auto xmlFile = folderOrXml.isDirectory() ? folder.getChildFile ("kit.xml") : folderOrXml;
    if (! xmlFile.existsAsFile())
    {
        error = "No kit.xml found in " + folder.getFullPathName();
        return false;
    }

    auto xml = juce::XmlDocument::parse (xmlFile);
    if (xml == nullptr || ! xml->hasTagName ("ATLANTAKIT"))
    {
        error = "Not a valid ATLANTA MACHINE kit";
        return false;
    }

    for (auto& p : pads)
        p.clear();
    for (auto* e : xml->getChildWithTagNameIterator ("PAD"))
        readPad (*e, folder, missing);
    seq.readXml (*xml);

    kitName = folder.getFileNameWithoutExtension().toUpperCase();
    kitFolder = folder;
    factoryIndex = -1;
    return true;
}

void DrumKitManager::writeState (juce::XmlElement& e) const
{
    e.setAttribute ("name", kitName);
    e.setAttribute ("folder", kitFolder.getFullPathName());
    e.setAttribute ("factory", factoryIndex);
    for (int i = 0; i < kNumPads; ++i)
    {
        juce::String err;
        if (auto p = makePadXml (i, {}, false, err))
            e.addChildElement (p.release());
    }
    seq.writeXml (e);
}

void DrumKitManager::readState (const juce::XmlElement& e, juce::StringArray& missing)
{
    kitName = e.getStringAttribute ("name", "INIT KIT");
    kitFolder = e.getStringAttribute ("folder").isNotEmpty() ? juce::File (e.getStringAttribute ("folder")) : juce::File();
    factoryIndex = e.getIntAttribute ("factory", -1);

    for (auto& p : pads)
        p.clear();
    for (auto* pe : e.getChildWithTagNameIterator ("PAD"))
        readPad (*pe, kitFolder, missing);
    seq.readXml (e);
}
} // namespace am
