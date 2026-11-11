#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace am
{
static const char* kAmtIds[4] = { "revAmt", "dlyAmt", "choAmt", "disAmt" };
static const char* kMixIds[4] = { "revMix", "dlyMix", "choMix", "disMix" };

juce::AudioProcessorValueTreeState::ParameterLayout AtlantaProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "mainVol", 1 }, "Main Volume",
                                                        NormalisableRange<float> (0.f, 1.25f, 0.001f), 0.8f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "mainPan", 1 }, "Main Pan",
                                                        NormalisableRange<float> (-1.f, 1.f, 0.001f), 0.f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "bpm", 1 }, "BPM",
                                                        NormalisableRange<float> (20.f, 300.f, 0.01f), 140.f));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "hostSync", 1 }, "Host Sync", true));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "swing", 1 }, "Swing",
                                                        NormalisableRange<float> (0.f, 1.f, 0.001f), 0.f));

    const char* names[4] = { "Reverb", "Delay", "Chorus", "Distortion" };
    for (int i = 0; i < 4; ++i)
    {
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { kAmtIds[i], 1 }, String (names[i]) + " Amount",
                                                            NormalisableRange<float> (0.f, 1.f, 0.001f), 0.5f));
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { kMixIds[i], 1 }, String (names[i]) + " Mix",
                                                            NormalisableRange<float> (0.f, 1.f, 0.001f), 0.f));
    }
    return { p.begin(), p.end() };
}

AtlantaProcessor::AtlantaProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      engine (pads),
      midi (pads),
      kits (pads, seq),
      apvts (*this, nullptr, "STATE", createLayout())
{
    for (int i = 0; i < kNumPads; ++i)
        pads[(size_t) i].init (i);

    resetKeyDefaults();

    pMainVol = apvts.getRawParameterValue ("mainVol");
    pMainPan = apvts.getRawParameterValue ("mainPan");
    pBpm     = apvts.getRawParameterValue ("bpm");
    pSync    = apvts.getRawParameterValue ("hostSync");
    pSwing   = apvts.getRawParameterValue ("swing");
    for (int i = 0; i < 4; ++i)
    {
        pAmt[i] = apvts.getRawParameterValue (kAmtIds[i]);
        pMix[i] = apvts.getRawParameterValue (kMixIds[i]);
    }

    kits.loadFactoryKit (3);   // GOLDMINE 808S
}

void AtlantaProcessor::resetKeyDefaults()
{
    static const char* keys = "1234QWERASDFZXCV";
    for (int i = 0; i < 16; ++i)
        keyCodes[(size_t) i].store ((int) keys[i]);
}

bool AtlantaProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void AtlantaProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sampleRateHz = sampleRate;
    engine.prepare (sampleRate);
    fx.prepare (sampleRate, samplesPerBlock);
    smVol.reset (sampleRate, 0.02);
    smPan.reset (sampleRate, 0.02);
    smVol.setCurrentAndTargetValue (pMainVol->load());
    smPan.setCurrentAndTargetValue (pMainPan->load());
    scratch.setSize (2, std::max (samplesPerBlock, 16));
    internalPpq = 0.0;
}

// ------------------------------------------------------------------ GUI -> audio

void AtlantaProcessor::pushUi (UiEvent e)
{
    int s1, n1, s2, n2;
    fifo.prepareToWrite (1, s1, n1, s2, n2);
    if (n1 > 0)
    {
        fifoBuf[s1] = e;
        fifo.finishedWrite (1);
    }
}

void AtlantaProcessor::uiTrigger (int pad, float velocity) { pushUi ({ pad, velocity, 0 }); }
void AtlantaProcessor::uiRelease (int pad)                  { pushUi ({ pad, 0.f, 1 }); }

void AtlantaProcessor::startSequencer()
{
    resetInternalPpq.store (true);
    seq.running.store (true);
}

void AtlantaProcessor::stopSequencer()
{
    seq.running.store (false);
}

// ------------------------------------------------------------------ audio

void AtlantaProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    if (buffer.getNumChannels() < 2)
    {
        midiMessages.clear();
        return;
    }
    renderInternal (buffer.getWritePointer (0), buffer.getWritePointer (1), buffer.getNumSamples(), midiMessages);
}

void AtlantaProcessor::processBlock (juce::AudioBuffer<double>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    if (buffer.getNumChannels() < 2)
    {
        midiMessages.clear();
        return;
    }
    if (scratch.getNumSamples() < n)
        scratch.setSize (2, n, false, false, true);   // only if the host exceeds the promised block size
    scratch.clear();
    renderInternal (scratch.getWritePointer (0), scratch.getWritePointer (1), n, midiMessages);
    for (int ch = 0; ch < 2; ++ch)
    {
        auto* d = buffer.getWritePointer (ch);
        const auto* s = scratch.getReadPointer (ch);
        for (int i = 0; i < n; ++i)
            d[i] = (double) s[i];
    }
}

void AtlantaProcessor::renderInternal (float* L, float* R, int n, juce::MidiBuffer& midiMessages)
{
    juce::FloatVectorOperations::clear (L, n);
    juce::FloatVectorOperations::clear (R, n);

    // ---- transport / tempo
    const bool hostSync = pSync->load() > 0.5f;
    double bpm = (double) pBpm->load();
    bool hostPlaying = false;
    juce::Optional<double> hostPpq;
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            hostPlaying = pos->getIsPlaying();
            hostPpq = pos->getPpqPosition();
            if (hostSync)
                if (auto hb = pos->getBpm())
                    bpm = *hb;
        }
    }
    bpm = juce::jlimit (20.0, 400.0, bpm);
    displayBpm.store (bpm);

    const bool follow = hostSync && hostPlaying && hostPpq.hasValue();
    if (resetInternalPpq.exchange (false))
        internalPpq = 0.0;
    const bool runInternal = ! follow && seq.running.load();
    const bool seqActive = follow || runInternal;
    seq.followingHost.store (follow);

    const double ppqPerSample = bpm / 60.0 / sampleRateHz;
    const double blockPpq = follow ? *hostPpq : internalPpq;
    const bool rec = seq.recording.load();

    // ---- gather events (fixed-size array, no allocation)
    int numEv = 0;
    auto push = [&] (int off, int pad, float vel, int kind)
    {
        if (numEv < (int) evs.size())
            evs[(size_t) numEv++] = { off, pad, vel, kind };
        if (kind == 0 && rec && seqActive)
            seq.recordHit (pad, blockPpq + (double) off * ppqPerSample);
    };

    // GUI / computer keyboard
    {
        const int ready = fifo.getNumReady();
        if (ready > 0)
        {
            int s1, n1, s2, n2;
            fifo.prepareToRead (ready, s1, n1, s2, n2);
            for (int i = 0; i < n1; ++i) push (0, fifoBuf[s1 + i].pad, fifoBuf[s1 + i].vel, fifoBuf[s1 + i].kind);
            for (int i = 0; i < n2; ++i) push (0, fifoBuf[s2 + i].pad, fifoBuf[s2 + i].vel, fifoBuf[s2 + i].kind);
            fifo.finishedRead (n1 + n2);
        }
    }

    // MIDI
    for (const auto meta : midiMessages)
    {
        const auto m = meta.getMessage();
        const int off = juce::jlimit (0, n - 1, meta.samplePosition);
        int found[kNumPads];

        if (m.isNoteOn())
        {
            const int note = m.getNoteNumber();
            const int lp = midi.learnPad.load();
            if (lp >= 0 && lp < kNumPads)
            {
                pads[(size_t) lp].midiNote = juce::jlimit (0, 127, note - 12 * midi.octaveShift.load());
                midi.learnPad.store (-1);
            }
            const int c = midi.findPads (note, found);
            for (int k = 0; k < c; ++k)
                push (off, found[k], m.getFloatVelocity(), 0);
        }
        else if (m.isNoteOff())
        {
            const int c = midi.findPads (m.getNoteNumber(), found);
            for (int k = 0; k < c; ++k)
                push (off, found[k], 0.f, 1);
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff())
        {
            engine.stopAll();
        }
    }

    // Sequencer (driven by the audio clock only)
    if (seqActive)
    {
        seq.scheduleBlock (blockPpq, ppqPerSample, n, pSwing->load(),
                           [&] (int off, int pad, float vel) { push (off, pad, vel, 2); });
        if (runInternal)
            internalPpq += ppqPerSample * (double) n;
    }
    else
    {
        seq.currentStep.store (-1);
    }

    // stable insertion sort by offset
    for (int i = 1; i < numEv; ++i)
    {
        const Ev e = evs[(size_t) i];
        int j = i - 1;
        while (j >= 0 && evs[(size_t) j].offset > e.offset)
        {
            evs[(size_t) j + 1] = evs[(size_t) j];
            --j;
        }
        evs[(size_t) j + 1] = e;
    }

    // ---- render, splitting the block at every event so timing is sample-accurate
    int pos = 0;
    for (int i = 0; i < numEv; ++i)
    {
        const Ev& e = evs[(size_t) i];
        const int off = juce::jlimit (pos, n, e.offset);
        if (off > pos)
        {
            engine.render (L + pos, R + pos, off - pos);
            pos = off;
        }
        if (e.kind == 0)      engine.trigger (e.pad, e.vel, true);
        else if (e.kind == 2) engine.trigger (e.pad, e.vel, false);
        else                  engine.releasePad (e.pad);
    }
    if (pos < n)
        engine.render (L + pos, R + pos, n - pos);

    // ---- master volume / pan
    smVol.setTargetValue (pMainVol->load());
    smPan.setTargetValue (pMainPan->load());
    for (int i = 0; i < n; ++i)
    {
        const float v = smVol.getNextValue();
        const float p = smPan.getNextValue();
        L[i] *= v * std::min (1.f, 1.f - p);
        R[i] *= v * std::min (1.f, 1.f + p);
    }

    // ---- FX
    float amt[4], mix[4];
    for (int i = 0; i < 4; ++i)
    {
        amt[i] = pAmt[i]->load();
        mix[i] = pMix[i]->load();
    }
    fx.process (L, R, n, amt, mix, bpm);

    midiMessages.clear();
}

// ------------------------------------------------------------------ editing helpers

void AtlantaProcessor::pushUndo (int pad)
{
    if (pad < 0 || pad >= kNumPads)
        return;
    undoStack.push_back ({ pad, pads[(size_t) pad].snapshot() });
    if (undoStack.size() > 64)
        undoStack.erase (undoStack.begin());
}

bool AtlantaProcessor::undo (int& padOut)
{
    if (undoStack.empty())
        return false;
    auto e = std::move (undoStack.back());
    undoStack.pop_back();
    pads[(size_t) e.pad].restore (e.snap);
    padOut = e.pad;
    return true;
}

void AtlantaProcessor::purgeRetired()
{
    for (auto& p : pads)
        p.purgeRetired();
}

bool AtlantaProcessor::loadSampleToPad (int padIndex, const juce::File& file, juce::String& error)
{
    if (padIndex < 0 || padIndex >= kNumPads)
        return false;
    auto s = kits.loadSampleFile (file, error);
    if (s == nullptr)
        return false;

    pushUndo (padIndex);
    Pad& p = pads[(size_t) padIndex];
    p.setSample (s);
    p.name = s->name;
    p.start = 0.f;
    p.end = 1.f;
    p.missingPath = {};
    return true;
}

// ------------------------------------------------------------------ state

void AtlantaProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    juce::XmlElement root ("ATLANTAMACHINE");
    root.setAttribute ("version", 1);

    if (auto xml = apvts.copyState().createXml())
        root.addChildElement (xml.release());

    kits.writeState (*root.createNewChildElement ("KIT"));

    auto* st = root.createNewChildElement ("SETTINGS");
    st->setAttribute ("bank", selectedBank.load());
    st->setAttribute ("pad", selectedPad.load());
    st->setAttribute ("octave", midi.octaveShift.load());
    st->setAttribute ("middleC", midi.middleC.load());
    juce::StringArray keys;
    for (auto& k : keyCodes)
        keys.add (juce::String (k.load()));
    st->setAttribute ("keys", keys.joinIntoString (","));

    copyXmlToBinary (root, dest);
}

void AtlantaProcessor::setStateInformation (const void* data, int size)
{
    auto root = getXmlFromBinary (data, size);
    if (root == nullptr || ! root->hasTagName ("ATLANTAMACHINE"))
        return;

    if (auto* params = root->getChildByName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*params));

    missingSamples.clear();
    missingWarned = false;
    if (auto* kit = root->getChildByName ("KIT"))
        kits.readState (*kit, missingSamples);

    if (auto* st = root->getChildByName ("SETTINGS"))
    {
        selectedBank.store (juce::jlimit (0, kNumBanks - 1, st->getIntAttribute ("bank", 0)));
        selectedPad.store (juce::jlimit (0, kNumPads - 1, st->getIntAttribute ("pad", 0)));
        midi.octaveShift.store (juce::jlimit (-4, 4, st->getIntAttribute ("octave", 0)));
        midi.middleC.store (juce::jlimit (3, 5, st->getIntAttribute ("middleC", 3)));
        juce::StringArray keys;
        keys.addTokens (st->getStringAttribute ("keys"), ",", "");
        if (keys.size() == 16)
            for (int i = 0; i < 16; ++i)
                keyCodes[(size_t) i].store (keys[i].getIntValue());
    }
}

juce::AudioProcessorEditor* AtlantaProcessor::createEditor()
{
    return new AtlantaEditor (*this);
}
} // namespace am

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new am::AtlantaProcessor();
}
