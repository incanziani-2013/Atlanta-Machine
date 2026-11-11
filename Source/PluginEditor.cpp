#include "PluginEditor.h"
#include "FactoryKits.h"

namespace am
{
namespace
{
const char* kAmtIdsUi[4] = { "revAmt", "dlyAmt", "choAmt", "disAmt" };
const char* kMixIdsUi[4] = { "revMix", "dlyMix", "choMix", "disMix" };
const char* kFxNames[4]  = { "REVERB", "DELAY", "CHORUS", "DISTORTION" };

juce::String clip (const juce::String& s, int maxChars)
{
    return s.length() <= maxChars ? s : s.substring (0, maxChars - 1) + ".";
}
} // namespace

// ============================================================================ construction

AtlantaEditor::AtlantaEditor (AtlantaProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&laf);
    setWantsKeyboardFocus (true);

    addAndMakeVisible (surface);
    surface.onPaint = [this] (juce::Graphics& g) { paintSurface (g); };
    surface.onClick = [this] { grabKeyboardFocus(); };

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    juce::Component::SafePointer<AtlantaEditor> safe (this);

    // ---------------------------------------------------------------- header / kits
    factoryBox.addItemList (DrumKitManager::factoryKitNames(), 1);
    factoryBox.setTextWhenNothingSelected ("FACTORY KITS");
    factoryBox.onChange = [this, safe]
    {
        const int id = factoryBox.getSelectedId();
        if (id <= 0)
            return;
        factoryBox.setSelectedId (0, juce::dontSendNotification);
        confirm ("LOAD FACTORY KIT", "Replace all pads and patterns with the factory kit \""
                                         + DrumKitManager::factoryKitNames()[id - 1] + "\"?",
                [safe, id]
                {
                    if (safe == nullptr)
                        return;
                    safe->proc.kits.loadFactoryKit (id - 1);
                    safe->proc.missingSamples.clear();
                    safe->refreshAll();
                    safe->setStatus ("KIT LOADED");
                });
    };
    addChild (factoryBox);

    for (auto* b : { &btnNew, &btnLoad, &btnSave, &btnSaveAs, &btnReset })
        addChild (*b);
    btnNew.onClick    = [this] { kitNew(); };
    btnLoad.onClick   = [this] { kitLoad(); };
    btnSave.onClick   = [this] { kitSave(); };
    btnSaveAs.onClick = [this] { kitSaveAs(); };
    btnReset.onClick  = [this] { kitReset(); };

    // ---------------------------------------------------------------- banks / pads
    for (int b = 0; b < kNumBanks; ++b)
    {
        bankBtn[b].setButtonText (juce::String::charToString ((juce::juce_wchar) ('A' + b)));
        bankBtn[b].setClickingTogglesState (true);
        bankBtn[b].setRadioGroupId (101);
        bankBtn[b].onClick = [this, b] { selectBank (b); };
        addChild (bankBtn[b]);
    }
    addChild (btnUndo);
    addChild (btnKeys);
    btnUndo.onClick = [this]
    {
        int p = 0;
        if (proc.undo (p))
        {
            selectPad (p);
            setStatus ("UNDO");
        }
    };
    btnKeys.onClick = [this, safe]
    {
        juce::PopupMenu m;
        m.addSectionHeader ("COMPUTER KEYBOARD  (pick a pad slot, then press a key)");
        for (int i = 0; i < 16; ++i)
            m.addItem (100 + i, "Slot " + juce::String (i + 1) + "   [" + keyName (i) + "]");
        m.addSeparator();
        m.addItem (1, "Reset keys to default");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&btnKeys), [safe] (int r)
        {
            if (safe == nullptr || r == 0)
                return;
            if (r == 1)
            {
                safe->proc.resetKeyDefaults();
                safe->refreshPadGrid();
            }
            else
                safe->learnKey (r - 100);
        });
    };

    for (int i = 0; i < kPadsPerBank; ++i)
    {
        padBtn[i].index = i;
        padBtn[i].onDown = [this] (int slot, float vel) { padDown (slot, vel); };
        padBtn[i].onUp = [this] (int slot) { padUp (slot); };
        padBtn[i].onRightClick = [this] (int slot) { padMenu (slot); };
        padBtn[i].onFiles = [this] (int slot, const juce::StringArray& f) { padFilesDropped (slot, f); };
        addChild (padBtn[i]);
    }

    // ---------------------------------------------------------------- FX
    for (int i = 0; i < 4; ++i)
    {
        styleKnob (fxAmt[i]);
        styleKnob (fxMix[i]);
        addChild (fxAmt[i]);
        addChild (fxMix[i]);
        sliderAtt.push_back (std::make_unique<SA> (proc.apvts, kAmtIdsUi[i], fxAmt[i]));
        sliderAtt.push_back (std::make_unique<SA> (proc.apvts, kMixIdsUi[i], fxMix[i]));
        fxAmt[i].setDoubleClickReturnValue (true, 0.5);
        fxMix[i].setDoubleClickReturnValue (true, 0.0);
        styleLabel (fxName[i], kFxNames[i], 14.f, juce::Justification::centred);
        styleLabel (fxAmtLbl[i], "AMOUNT", 11.f, juce::Justification::centred);
        styleLabel (fxMixLbl[i], "MIX", 11.f, juce::Justification::centred);
        addChild (fxName[i]);
        addChild (fxAmtLbl[i]);
        addChild (fxMixLbl[i]);
    }

    // ---------------------------------------------------------------- selected pad controls
    styleLinear (volSl, 0.0, 1.5, 0.001, 1.0);
    styleLinear (pitchSl, -24.0, 24.0, 0.01, 0.0, " st");
    styleLinear (panSl, -1.0, 1.0, 0.001, 0.0);
    for (auto* s : { &volSl, &pitchSl, &panSl })
    {
        addChild (*s);
        s->onDragStart = [this] { proc.pushUndo (selPad()); };
    }
    volSl.onValueChange   = [this] { pad().volume = (float) volSl.getValue(); };
    pitchSl.onValueChange = [this] { pad().pitch  = (float) pitchSl.getValue(); };
    panSl.onValueChange   = [this] { pad().pan    = (float) panSl.getValue(); };

    styleLabel (lblVolP, "VOLUME");
    styleLabel (lblPitchP, "PITCH");
    styleLabel (lblPanP, "PAN");
    styleLabel (lblChoke, "CHOKE", 12.f, juce::Justification::centredRight);
    styleLabel (lblMidi, "MIDI", 12.f, juce::Justification::centredRight);
    for (auto* l : { &lblVolP, &lblPitchP, &lblPanP, &lblChoke, &lblMidi })
        addChild (*l);

    for (auto* b : { &btnReverse, &btnNormalize, &btnGate })
    {
        b->setClickingTogglesState (true);
        b->getProperties().set ("led", true);
        addChild (*b);
    }
    btnReverse.onClick   = [this] { proc.pushUndo (selPad()); pad().reverse = btnReverse.getToggleState(); };
    btnNormalize.onClick = [this] { proc.pushUndo (selPad()); pad().normalize = btnNormalize.getToggleState(); };
    btnGate.onClick      = [this] { proc.pushUndo (selPad()); pad().playMode = btnGate.getToggleState() ? 1 : 0; };

    chokeBox.addItem ("NONE", 1);
    for (int i = 1; i <= 4; ++i)
        chokeBox.addItem ("GROUP " + juce::String (i), i + 1);
    chokeBox.onChange = [this] { proc.pushUndo (selPad()); pad().chokeGroup = chokeBox.getSelectedId() - 1; };
    addChild (chokeBox);

    midiText.setJustification (juce::Justification::centred);
    midiText.setFont (monoFont (15.f));
    midiText.setInputRestrictions (5, "0123456789ABCDEFGabcdefg#-");
    midiText.onReturnKey = [this] { commitMidiText(); grabKeyboardFocus(); };
    midiText.onFocusLost = [this] { commitMidiText(); };
    midiText.onEscapeKey = [this] { refreshSelectedControls(); grabKeyboardFocus(); };
    addChild (midiText);

    btnLearn.setClickingTogglesState (false);
    btnLearn.getProperties().set ("led", true);
    btnLearn.onClick = [this]
    {
        const int sel = selPad();
        proc.midi.learnPad.store (proc.midi.learnPad.load() == sel ? -1 : sel);
        setStatus ("PLAY A MIDI NOTE");
    };
    addChild (btnLearn);

    btnLoadSample.onClick = [this] { chooseSampleFor (selPad()); };
    btnClearPad.onClick = [this]
    {
        proc.pushUndo (selPad());
        pad().clear();
        refreshAll();
    };
    btnAudition.onClick = [this] { proc.uiTrigger (selPad(), 0.9f); };
    for (auto* b : { &btnLoadSample, &btnClearPad, &btnAudition })
        addChild (*b);

    for (int o = -4; o <= 4; ++o)
        octaveBox.addItem ("OCT " + juce::String (o > 0 ? "+" : "") + juce::String (o), o + 5);
    octaveBox.onChange = [this] { proc.midi.octaveShift.store (octaveBox.getSelectedId() - 5); refreshPadGrid(); refreshSelectedControls(); };
    addChild (octaveBox);
    for (int c = 3; c <= 5; ++c)
        middleCBox.addItem ("MIDDLE C = C" + juce::String (c), c);
    middleCBox.onChange = [this] { proc.midi.middleC.store (middleCBox.getSelectedId()); refreshPadGrid(); refreshSelectedControls(); };
    addChild (middleCBox);

    // ---------------------------------------------------------------- waveform
    wave.onDragStart = [this] { proc.pushUndo (selPad()); };
    wave.onRangeChanged = [this] (float s, float e) { pad().start = s; pad().end = e; };
    addChild (wave);

    // ---------------------------------------------------------------- sequencer
    for (int i = 0; i < kNumSteps; ++i)
    {
        steps[i].beatMarker = (i % 4) == 0;
        steps[i].onClick = [this, i] { proc.seq.toggleStep (selPad(), i); refreshSteps(); };
        addChild (steps[i]);
    }
    for (auto* b : { &btnPlay, &btnStop, &btnRec })
        addChild (*b);
    btnPlay.getProperties().set ("led", true);
    btnRec.getProperties().set ("red", true);
    btnRec.getProperties().set ("led", true);
    btnPlay.onClick = [this] { proc.startSequencer(); };
    btnStop.onClick = [this] { proc.stopSequencer(); };
    btnRec.onClick  = [this] { proc.seq.recording.store (! proc.seq.recording.load()); };

    for (int i = 1; i <= kNumPatterns; ++i)
        patternBox.addItem ("PATTERN " + juce::String (i), i);
    patternBox.onChange = [this] { proc.seq.activePattern.store (patternBox.getSelectedId() - 1); refreshSteps(); };
    addChild (patternBox);
    for (int i = 1; i <= kNumSteps; ++i)
        lengthBox.addItem (juce::String (i) + " STEPS", i);
    lengthBox.onChange = [this] { proc.seq.length.store (lengthBox.getSelectedId()); refreshSteps(); };
    addChild (lengthBox);

    btnClearPattern.onClick = [this, safe]
    {
        confirm ("CLEAR PATTERN", "Erase every step of the current pattern on all pads?", [safe]
        {
            if (safe == nullptr) return;
            safe->proc.seq.clearPattern (safe->proc.seq.activePattern.load());
            safe->refreshSteps();
        });
    };
    btnClearPadSteps.onClick = [this]
    {
        proc.seq.clearPad (proc.seq.activePattern.load(), selPad());
        refreshSteps();
    };
    addChild (btnClearPattern);
    addChild (btnClearPadSteps);

    // ---------------------------------------------------------------- main / tempo
    for (auto* s : { &mainVol, &mainPan, &bpmSlider, &swingSlider })
        addChild (*s);
    styleLinear (mainVol, 0.0, 1.25, 0.001, 0.8);
    styleLinear (mainPan, -1.0, 1.0, 0.001, 0.0);
    styleLinear (bpmSlider, 20.0, 300.0, 0.01, 140.0);
    styleLinear (swingSlider, 0.0, 1.0, 0.001, 0.0);
    sliderAtt.push_back (std::make_unique<SA> (proc.apvts, "mainVol", mainVol));
    sliderAtt.push_back (std::make_unique<SA> (proc.apvts, "mainPan", mainPan));
    sliderAtt.push_back (std::make_unique<SA> (proc.apvts, "bpm", bpmSlider));
    sliderAtt.push_back (std::make_unique<SA> (proc.apvts, "swing", swingSlider));
    hostSyncBtn.setButtonText ("HOST SYNC");
    buttonAtt.push_back (std::make_unique<BA> (proc.apvts, "hostSync", hostSyncBtn));
    addChild (hostSyncBtn);
    styleLabel (lblBpm, "BPM", 12.f, juce::Justification::centredRight);
    styleLabel (lblVol, "VOL", 12.f, juce::Justification::centredRight);
    styleLabel (lblPan, "PAN", 12.f, juce::Justification::centredRight);
    styleLabel (lblSwing, "SWING", 12.f, juce::Justification::centredRight);
    styleLabel (lblPattern, "PATTERN", 12.f, juce::Justification::centredRight);
    styleLabel (lblLength, "LENGTH", 12.f, juce::Justification::centredRight);
    for (auto* l : { &lblBpm, &lblVol, &lblPan, &lblSwing, &lblPattern, &lblLength })
        addChild (*l);

    // ---------------------------------------------------------------- initial state
    octaveBox.setSelectedId (proc.midi.octaveShift.load() + 5, juce::dontSendNotification);
    middleCBox.setSelectedId (proc.midi.middleC.load(), juce::dontSendNotification);
    layout();
    refreshAll();

    setResizable (true, true);
    setResizeLimits (560, 440, 1680, 1320);
    getConstrainer()->setFixedAspectRatio ((double) kW / (double) kH);
    setSize (896, 704);

    startTimerHz (30);
}

AtlantaEditor::~AtlantaEditor()
{
    stopTimer();
    for (int i = 0; i < 16; ++i)
        if (keyHeld[i])
            proc.uiRelease (bank() * kPadsPerBank + i);
    if (proc.midi.learnPad.load() >= 0)
        proc.midi.learnPad.store (-1);
    setLookAndFeel (nullptr);
}

// ============================================================================ look

void AtlantaEditor::styleLabel (juce::Label& l, const juce::String& text, float size, juce::Justification j)
{
    l.setText (text, juce::dontSendNotification);
    l.setFont (uiFont (size));
    l.setJustificationType (j);
    l.setColour (juce::Label::textColourId, palette::gold);
    l.setInterceptsMouseClicks (false, false);
}

void AtlantaEditor::styleLinear (juce::Slider& s, double lo, double hi, double step, double def, const juce::String& suffix)
{
    s.setSliderStyle (juce::Slider::LinearHorizontal);
    s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 76, 24);
    s.setRange (lo, hi, step);
    s.setDoubleClickReturnValue (true, def);
    s.setValue (def, juce::dontSendNotification);
    if (suffix.isNotEmpty())
        s.setTextValueSuffix (suffix);
    s.setNumDecimalPlacesToDisplay (2);
}

void AtlantaEditor::styleKnob (juce::Slider& s)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    s.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
    s.setRange (0.0, 1.0, 0.001);
}

void AtlantaEditor::paint (juce::Graphics& g)
{
    g.fillAll (palette::ink);
}

void AtlantaEditor::resized()
{
    surface.setBounds (0, 0, kW, kH);
    surface.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) kW));
}

void AtlantaEditor::layout()
{
    // header
    factoryBox.setBounds (810, 12, 290, 32);
    {
        int x = 810;
        for (auto* b : { &btnNew, &btnLoad, &btnSave, &btnSaveAs, &btnReset })
        {
            b->setBounds (x, 52, 54, 32);
            x += 59;
        }
    }

    // banks + pads
    for (int b = 0; b < kNumBanks; ++b)
        bankBtn[b].setBounds (20 + b * 64, 104, 58, 32);
    btnKeys.setBounds (480, 104, 70, 32);
    btnUndo.setBounds (560, 104, 80, 32);
    for (int i = 0; i < kPadsPerBank; ++i)
    {
        const int col = i % 4, row = 3 - (i / 4);   // pad 1 bottom-left, like a hardware sampler
        padBtn[i].setBounds (20 + col * 155, 144 + row * 107, 155, 107);
    }

    // FX panel (x 660..1100, y 104..314)
    for (int i = 0; i < 4; ++i)
    {
        const int cx = 660 + i * 110;
        fxName[i].setBounds (cx, 134, 110, 20);
        fxAmt[i].setBounds (cx + 15, 156, 80, 80);
        fxAmtLbl[i].setBounds (cx, 236, 110, 14);
        fxMix[i].setBounds (cx + 27, 252, 56, 56);
        fxMixLbl[i].setBounds (cx, 306, 110, 14);
    }

    // sample controls (x 660..1100, y 332..574)
    const int lx = 672, sx = 740, sw = 348;
    lblVolP.setBounds (lx, 358, 66, 26);   volSl.setBounds (sx, 358, sw, 26);
    lblPitchP.setBounds (lx, 388, 66, 26); pitchSl.setBounds (sx, 388, sw, 26);
    lblPanP.setBounds (lx, 418, 66, 26);   panSl.setBounds (sx, 418, sw, 26);
    btnReverse.setBounds (672, 452, 132, 30);
    btnNormalize.setBounds (810, 452, 132, 30);
    btnGate.setBounds (948, 452, 140, 30);
    lblChoke.setBounds (672, 488, 52, 28);
    chokeBox.setBounds (728, 488, 110, 28);
    lblMidi.setBounds (842, 488, 42, 28);
    midiText.setBounds (888, 488, 62, 28);
    btnLearn.setBounds (956, 488, 132, 28);
    btnLoadSample.setBounds (672, 524, 160, 32);
    btnClearPad.setBounds (840, 524, 120, 32);
    btnAudition.setBounds (968, 524, 120, 32);

    // waveform
    wave.setBounds (20, 606, 1080, 100);

    // sequencer
    for (int i = 0; i < kNumSteps; ++i)
        steps[i].setBounds (20 + i * 52, 738, 50, 50);
    btnPlay.setBounds (868, 738, 72, 50);
    btnStop.setBounds (944, 738, 72, 50);
    btnRec.setBounds (1020, 738, 80, 50);

    lblPattern.setBounds (20, 798, 66, 28);
    patternBox.setBounds (90, 798, 120, 28);
    lblLength.setBounds (216, 798, 60, 28);
    lengthBox.setBounds (280, 798, 110, 28);
    btnClearPattern.setBounds (400, 798, 140, 28);
    btnClearPadSteps.setBounds (546, 798, 110, 28);
    lblSwing.setBounds (676, 798, 60, 28);
    swingSlider.setBounds (740, 798, 360, 28);

    lblBpm.setBounds (20, 838, 40, 28);
    bpmSlider.setBounds (62, 838, 250, 28);
    hostSyncBtn.setBounds (322, 838, 120, 28);
    lblVol.setBounds (448, 838, 40, 28);
    mainVol.setBounds (490, 838, 180, 28);
    lblPan.setBounds (676, 838, 40, 28);
    mainPan.setBounds (718, 838, 150, 28);
    octaveBox.setBounds (878, 838, 90, 28);
    middleCBox.setBounds (974, 838, 126, 28);
}

void AtlantaEditor::paintSurface (juce::Graphics& g)
{
    using namespace palette;
    const juce::Rectangle<float> all (0.f, 0.f, (float) kW, (float) kH);

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1a1a18), 0.f, 0.f, juce::Colour (0xff070706), 0.f, (float) kH, false));
    g.fillRect (all);
    g.setColour (gold);
    g.drawRoundedRectangle (all.reduced (3.f), 8.f, 2.5f);
    g.setColour (goldDark);
    g.drawRoundedRectangle (all.reduced (8.f), 6.f, 1.f);

    // logo
    g.setFont (juce::Font (juce::FontOptions ("Impact", 38.f, juce::Font::bold)));
    g.setGradientFill (juce::ColourGradient (goldLight, 20.f, 12.f, goldDark, 20.f, 56.f, false));
    g.drawText ("ATLANTA MACHINE", juce::Rectangle<float> (20.f, 8.f, 350.f, 48.f), juce::Justification::centredLeft, false);
    g.setColour (gold);
    g.setFont (uiFont (13.f));
    g.drawText (brandName() + "   V1", juce::Rectangle<float> (22.f, 58.f, 340.f, 22.f), juce::Justification::centredLeft, false);

    // LCD
    const juce::Rectangle<float> lcd (370.f, 12.f, 420.f, 72.f);
    g.setColour (juce::Colours::black);
    g.fillRoundedRectangle (lcd.expanded (3.f), 6.f);
    g.setGradientFill (juce::ColourGradient (lcdBg.brighter (0.1f), lcd.getX(), lcd.getY(), lcdBg.darker (0.25f), lcd.getX(), lcd.getBottom(), false));
    g.fillRoundedRectangle (lcd, 4.f);

    const int sel = selPad();
    const auto& p = proc.pads[(size_t) sel];
    const double bpm = proc.displayBpm.load();
    const juce::String sync = proc.seq.followingHost.load() ? "HOST" : (proc.seq.running.load() ? "INT" : "STOP");
    const int step = proc.seq.currentStep.load();
    const juce::String stepTxt = step >= 0 ? juce::String (step + 1).paddedLeft ('0', 2) : "--";

    juce::String l1 = "KIT  " + clip (proc.kits.kitName.toUpperCase(), 26) + (proc.missingSamples.isEmpty() ? "" : "  !");
    juce::String l2 = "BPM " + juce::String (bpm, 1) + "  " + sync + "  STEP " + stepTxt + "/" + juce::String (proc.seq.length.load()).paddedLeft ('0', 2);
    juce::String l3;
    if (keyLearnSlot >= 0)
        l3 = "PRESS A KEY FOR SLOT " + juce::String (keyLearnSlot + 1) + "  (ESC=CANCEL)";
    else if (statusFrames > 0)
        l3 = statusText;
    else
        l3 = "PAD " + padTitle (sel) + " " + clip (p.getSample() != nullptr ? p.name.toUpperCase() : juce::String ("EMPTY"), 10)
             + "  " + proc.midi.noteName (p.midiNote.load() + 12 * proc.midi.octaveShift.load())
             + "  KEY " + keyName (sel % 16);

    g.setColour (lcdFg);
    g.setFont (monoFont (16.f));
    g.drawText (l1, lcd.reduced (10.f, 0.f).withTrimmedBottom (48.f), juce::Justification::centredLeft, false);
    g.drawText (l2, lcd.reduced (10.f, 0.f).withTrimmedTop (24.f).withTrimmedBottom (24.f), juce::Justification::centredLeft, false);
    g.drawText (l3, lcd.reduced (10.f, 0.f).withTrimmedTop (48.f), juce::Justification::centredLeft, false);

    auto panel = [&g] (juce::Rectangle<float> r, const juce::String& title)
    {
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (r, 6.f);
        g.setColour (goldDark);
        g.drawRoundedRectangle (r, 6.f, 1.2f);
        g.setColour (gold);
        g.setFont (uiFont (14.f));
        g.drawText (title, r.withTrimmedLeft (12.f).withHeight (24.f), juce::Justification::centredLeft, false);
    };

    panel ({ 660.f, 104.f, 440.f, 222.f }, "EFFECTS");
    panel ({ 660.f, 332.f, 440.f, 242.f }, "SAMPLE CONTROLS  -  PAD " + padTitle (sel));
    panel ({ 20.f, 582.f, 1080.f, 128.f }, "SAMPLE EDITOR  -  " + (p.getSample() != nullptr ? p.name.toUpperCase() : juce::String ("EMPTY")));
    g.setColour (gold);
    g.setFont (uiFont (14.f));
    g.drawText ("SEQUENCER  -  PAD " + padTitle (sel), juce::Rectangle<float> (20.f, 714.f, 400.f, 22.f), juce::Justification::centredLeft, false);
}

// ============================================================================ helpers

juce::String AtlantaEditor::padTitle (int padIndex) const
{
    return juce::String::charToString ((juce::juce_wchar) ('A' + padIndex / kPadsPerBank)) + juce::String (padIndex % kPadsPerBank + 1);
}

juce::String AtlantaEditor::keyName (int slot) const
{
    const int code = proc.keyCodes[(size_t) juce::jlimit (0, 15, slot)].load();
    if (code <= 32)
        return "-";
    return juce::String::charToString ((juce::juce_wchar) code);
}

void AtlantaEditor::showError (const juce::String& title, const juce::String& msg)
{
    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, title, msg, "OK", this);
}

void AtlantaEditor::confirm (const juce::String& title, const juce::String& msg, std::function<void()> yes)
{
    auto opts = juce::MessageBoxOptions::makeOptionsOkCancel (juce::MessageBoxIconType::QuestionIcon, title, msg, "OK", "CANCEL", this);
    juce::AlertWindow::showAsync (opts, [yes = std::move (yes)] (int result)
    {
        if (result == 1 && yes)
            yes();
    });
}

void AtlantaEditor::warnMissing()
{
    if (proc.missingSamples.isEmpty() || proc.missingWarned)
        return;
    proc.missingWarned = true;
    juce::String msg = "These samples could not be found. Their pads are marked MISSING:\n\n";
    for (int i = 0; i < juce::jmin (8, proc.missingSamples.size()); ++i)
        msg << "  " << proc.missingSamples[i] << "\n";
    if (proc.missingSamples.size() > 8)
        msg << "  ... and " << (proc.missingSamples.size() - 8) << " more";
    showError ("MISSING SAMPLES", msg);
}

// ============================================================================ selection / refresh

void AtlantaEditor::selectPad (int index)
{
    index = juce::jlimit (0, kNumPads - 1, index);
    proc.selectedPad.store (index);
    proc.selectedBank.store (index / kPadsPerBank);
    refreshAll();
}

void AtlantaEditor::selectBank (int b)
{
    b = juce::jlimit (0, kNumBanks - 1, b);
    proc.selectedBank.store (b);
    proc.selectedPad.store (b * kPadsPerBank + (selPad() % kPadsPerBank));
    refreshAll();
}

void AtlantaEditor::refreshAll()
{
    lastSelected = selPad();
    lastBank = bank();
    for (int b = 0; b < kNumBanks; ++b)
        bankBtn[b].setToggleState (b == bank(), juce::dontSendNotification);
    patternBox.setSelectedId (proc.seq.activePattern.load() + 1, juce::dontSendNotification);
    lengthBox.setSelectedId (proc.seq.length.load(), juce::dontSendNotification);
    refreshPadGrid();
    refreshSelectedControls();
    refreshSteps();
    refreshWaveform();
    surface.repaint();
}

void AtlantaEditor::refreshPadGrid()
{
    const juce::uint64 active = proc.engine.getActiveMask();
    const int shift = 12 * proc.midi.octaveShift.load();
    for (int slot = 0; slot < kPadsPerBank; ++slot)
    {
        const int idx = bank() * kPadsPerBank + slot;
        const Pad& p = proc.pads[(size_t) idx];
        const bool hasSample = p.getSample() != nullptr;
        const bool missing = p.missingPath.isNotEmpty();
        juce::String name = hasSample ? p.name : (missing ? juce::File (p.missingPath).getFileName() : juce::String ("EMPTY"));
        const bool lit = litFrames[idx] > 0 || ((active >> idx) & 1) != 0;
        padBtn[slot].setState (padTitle (idx), clip (name.toUpperCase(), 14),
                               proc.midi.noteName (p.midiNote.load() + shift), keyName (slot),
                               lit, idx == selPad(), missing && ! hasSample);
    }
}

void AtlantaEditor::refreshSelectedControls()
{
    const Pad& p = pad();
    volSl.setValue (p.volume.load(), juce::dontSendNotification);
    pitchSl.setValue (p.pitch.load(), juce::dontSendNotification);
    panSl.setValue (p.pan.load(), juce::dontSendNotification);
    btnReverse.setToggleState (p.reverse.load(), juce::dontSendNotification);
    btnNormalize.setToggleState (p.normalize.load(), juce::dontSendNotification);
    btnGate.setToggleState (p.playMode.load() == 1, juce::dontSendNotification);
    chokeBox.setSelectedId (juce::jlimit (0, 4, p.chokeGroup.load()) + 1, juce::dontSendNotification);
    if (! midiText.hasKeyboardFocus (true))
        midiText.setText (proc.midi.noteName (p.midiNote.load() + 12 * proc.midi.octaveShift.load()), false);
    octaveBox.setSelectedId (proc.midi.octaveShift.load() + 5, juce::dontSendNotification);
    middleCBox.setSelectedId (proc.midi.middleC.load(), juce::dontSendNotification);
}

void AtlantaEditor::refreshSteps()
{
    const int sel = selPad();
    const int len = proc.seq.length.load();
    const int cur = proc.seq.currentStep.load();
    const bool live = proc.seq.running.load() || proc.seq.followingHost.load();
    for (int i = 0; i < kNumSteps; ++i)
    {
        steps[i].setState (proc.seq.getStep (sel, i), live && cur == i);
        steps[i].setAlpha (i < len ? 1.0f : 0.35f);
    }
    btnPlay.setToggleState (proc.seq.running.load(), juce::dontSendNotification);
    btnRec.setToggleState (proc.seq.recording.load(), juce::dontSendNotification);
}

void AtlantaEditor::refreshWaveform()
{
    auto s = pad().getSample();
    lastSamplePtr = s.get();
    wave.setSample (s, pad().start.load(), pad().end.load());
}

void AtlantaEditor::commitMidiText()
{
    const int note = proc.midi.parseNote (midiText.getText());
    if (note >= 0)
    {
        proc.pushUndo (selPad());
        pad().midiNote = juce::jlimit (0, 127, note - 12 * proc.midi.octaveShift.load());
    }
    midiText.setText (proc.midi.noteName (pad().midiNote.load() + 12 * proc.midi.octaveShift.load()), false);
    refreshPadGrid();
}

// ============================================================================ timer

void AtlantaEditor::timerCallback()
{
    ++tick;

    // state restored / pad selected from elsewhere
    if (lastSelected != selPad() || lastBank != bank())
        refreshAll();

    for (auto& f : litFrames)
        if (f > 0) --f;
    const auto trig = proc.engine.consumeTriggerMask();
    for (int i = 0; i < kNumPads; ++i)
        if (((trig >> i) & 1) != 0)
            litFrames[i] = 5;

    refreshPadGrid();
    refreshSteps();

    // sample replaced (load / undo / kit change) -> redraw waveform
    if (pad().getSample().get() != lastSamplePtr)
        refreshWaveform();
    wave.setRange (pad().start.load(), pad().end.load());
    wave.setPlayFraction (proc.engine.getPlayFraction (selPad()));

    // MIDI learn finished?
    const bool learning = proc.midi.learnPad.load() == selPad();
    btnLearn.setToggleState (learning, juce::dontSendNotification);
    if (! midiText.hasKeyboardFocus (true))
    {
        const auto t = proc.midi.noteName (pad().midiNote.load() + 12 * proc.midi.octaveShift.load());
        if (midiText.getText() != t)
            midiText.setText (t, false);
    }

    // release keys whose physical key is no longer down
    for (int i = 0; i < 16; ++i)
        if (keyHeld[i] && ! juce::KeyPress::isKeyCurrentlyDown (proc.keyCodes[(size_t) i].load()))
        {
            keyHeld[i] = false;
            proc.uiRelease (bank() * kPadsPerBank + i);
        }

    if (statusFrames > 0)
        --statusFrames;

    if ((tick % 30) == 0)
        proc.purgeRetired();

    if (tick > 15 && ! proc.missingWarned && ! proc.missingSamples.isEmpty() && isShowing())
        warnMissing();

    surface.repaint (juce::Rectangle<int> (370, 12, 420, 72));
}

// ============================================================================ pads

void AtlantaEditor::padDown (int slot, float vel)
{
    const int idx = bank() * kPadsPerBank + slot;
    if (proc.midi.learnPad.load() >= 0)
        proc.midi.learnPad.store (idx);   // while learning, clicking a pad picks which pad learns
    if (selPad() != idx)
        selectPad (idx);
    proc.uiTrigger (idx, vel);
    grabKeyboardFocus();
}

void AtlantaEditor::padUp (int slot)
{
    proc.uiRelease (bank() * kPadsPerBank + slot);
}

void AtlantaEditor::padMenu (int slot)
{
    const int idx = bank() * kPadsPerBank + slot;
    selectPad (idx);
    juce::PopupMenu m;
    m.addSectionHeader ("PAD " + padTitle (idx));
    m.addItem (1, "Load sample...");
    m.addItem (2, "Clear pad", proc.pads[(size_t) idx].getSample() != nullptr || proc.pads[(size_t) idx].missingPath.isNotEmpty());
    m.addSeparator();
    m.addItem (3, "MIDI learn");
    m.addItem (4, "Assign computer key to slot " + juce::String (slot + 1) + "...");
    m.addItem (5, "Reset pad settings");
    juce::Component::SafePointer<AtlantaEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&padBtn[slot]), [safe, idx, slot] (int r)
    {
        if (safe == nullptr || r == 0)
            return;
        auto& pd = safe->proc.pads[(size_t) idx];
        switch (r)
        {
            case 1: safe->chooseSampleFor (idx); break;
            case 2: safe->proc.pushUndo (idx); pd.clear(); safe->refreshAll(); break;
            case 3: safe->proc.midi.learnPad.store (idx); safe->setStatus ("PLAY A MIDI NOTE"); break;
            case 4: safe->learnKey (slot); break;
            case 5:
            {
                safe->proc.pushUndo (idx);
                const juce::String keepName = pd.name;
                const auto s = pd.getSample();
                pd.resetSettings();
                pd.name = keepName;
                juce::ignoreUnused (s);
                safe->refreshAll();
                break;
            }
            default: break;
        }
    });
}

void AtlantaEditor::padFilesDropped (int slot, const juce::StringArray& files)
{
    const int first = bank() * kPadsPerBank + slot;
    const int last = (bank() + 1) * kPadsPerBank;   // stay inside the current bank
    juce::String firstError;
    int loaded = 0;

    for (int i = 0; i < files.size() && first + i < last; ++i)
    {
        juce::String err;
        if (proc.loadSampleToPad (first + i, juce::File (files[i]), err))
            ++loaded;
        else if (firstError.isEmpty())
            firstError = err;
    }

    selectPad (first);
    if (firstError.isNotEmpty())
        showError ("COULD NOT LOAD SAMPLE", firstError);
    else if (loaded > 0)
        setStatus (juce::String (loaded) + " SAMPLE" + (loaded > 1 ? "S" : "") + " LOADED");
    if (files.size() > last - first)
        setStatus ("ONLY " + juce::String (last - first) + " PADS LEFT IN BANK");
}

void AtlantaEditor::chooseSampleFor (int padIndex)
{
    chooser = std::make_unique<juce::FileChooser> ("Load sample", juce::File(), "*.wav;*.aif;*.aiff;*.mp3;*.flac;*.ogg");
    juce::Component::SafePointer<AtlantaEditor> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safe, padIndex] (const juce::FileChooser& fc)
    {
        const auto f = fc.getResult();
        if (safe == nullptr || f == juce::File())
            return;
        juce::String err;
        if (! safe->proc.loadSampleToPad (padIndex, f, err))
        {
            safe->showError ("COULD NOT LOAD SAMPLE", err);
            return;
        }
        safe->selectPad (padIndex);
        safe->setStatus ("SAMPLE LOADED");
    });
}

// ============================================================================ kits

void AtlantaEditor::kitNew()
{
    juce::Component::SafePointer<AtlantaEditor> safe (this);
    confirm ("NEW KIT", "Clear all pads and patterns and start an empty kit?", [safe]
    {
        if (safe == nullptr) return;
        safe->proc.kits.newKit();
        safe->proc.missingSamples.clear();
        safe->refreshAll();
        safe->setStatus ("NEW KIT");
    });
}

void AtlantaEditor::kitLoad()
{
    const auto folder = DrumKitManager::defaultKitsFolder();
    folder.createDirectory();
    chooser = std::make_unique<juce::FileChooser> ("Select a kit folder (.amkit)", folder, "*");
    juce::Component::SafePointer<AtlantaEditor> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                          [safe] (const juce::FileChooser& fc)
    {
        const auto f = fc.getResult();
        if (safe == nullptr || f == juce::File())
            return;
        juce::StringArray missing;
        juce::String err;
        if (! safe->proc.kits.loadKit (f, missing, err))
        {
            safe->showError ("COULD NOT LOAD KIT", err);
            return;
        }
        safe->proc.missingSamples = missing;
        safe->proc.missingWarned = false;
        safe->refreshAll();
        safe->setStatus ("KIT LOADED");
        safe->warnMissing();
    });
}

void AtlantaEditor::kitSave()
{
    if (proc.kits.factoryIndex >= 0 || ! proc.kits.kitFolder.isDirectory())
    {
        kitSaveAs();
        return;
    }
    juce::String err;
    if (proc.kits.saveKit (proc.kits.kitFolder, err))
        setStatus ("KIT SAVED");
    else
        showError ("COULD NOT SAVE KIT", err);
}

void AtlantaEditor::kitSaveAs()
{
    auto* w = new juce::AlertWindow ("SAVE KIT AS", "Kit name (saved in Documents/GOLDMINE AUDIO/ATLANTA MACHINE/Kits):",
                                     juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("name", proc.kits.kitName == "NEW KIT" || proc.kits.kitName == "INIT KIT" ? juce::String ("MY KIT") : proc.kits.kitName);
    w->addButton ("SAVE", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("CANCEL", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<AtlantaEditor> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int r)
    {
        if (r == 1 && safe != nullptr)
            safe->doSaveAs (w->getTextEditorContents ("name"));
    }), true);
}

void AtlantaEditor::doSaveAs (const juce::String& rawName)
{
    const auto name = juce::File::createLegalFileName (rawName.trim());
    if (name.isEmpty())
    {
        showError ("SAVE KIT", "Please type a valid kit name.");
        return;
    }
    const auto folder = DrumKitManager::defaultKitsFolder().getChildFile (name + ".amkit");
    juce::String err;
    if (proc.kits.saveKit (folder, err))
    {
        setStatus ("SAVED " + clip (name.toUpperCase(), 20));
        surface.repaint();
    }
    else
        showError ("COULD NOT SAVE KIT", err);
}

void AtlantaEditor::kitReset()
{
    juce::Component::SafePointer<AtlantaEditor> safe (this);
    confirm ("RESET KIT", "Discard changes and reload the kit from its saved state?", [safe]
    {
        if (safe == nullptr) return;
        juce::StringArray missing;
        const bool ok = safe->proc.kits.resetKit (missing);
        safe->proc.missingSamples = missing;
        safe->proc.missingWarned = false;
        safe->refreshAll();
        safe->setStatus (ok ? "KIT RESET" : "RESET FAILED");
        safe->warnMissing();
    });
}

// ============================================================================ computer keyboard

void AtlantaEditor::learnKey (int slot)
{
    keyLearnSlot = juce::jlimit (0, 15, slot);
    grabKeyboardFocus();
}

bool AtlantaEditor::keyPressed (const juce::KeyPress& key)
{
    if (keyLearnSlot >= 0)
    {
        if (key == juce::KeyPress::escapeKey)
        {
            keyLearnSlot = -1;
            return true;
        }
        int code = (int) juce::CharacterFunctions::toUpperCase (key.getTextCharacter());
        if (code <= 32)
            code = key.getKeyCode();
        if (code > 32)
        {
            for (int i = 0; i < 16; ++i)
                if (i != keyLearnSlot && proc.keyCodes[(size_t) i].load() == code)
                    proc.keyCodes[(size_t) i].store (0);
            proc.keyCodes[(size_t) keyLearnSlot].store (code);
            setStatus ("SLOT " + juce::String (keyLearnSlot + 1) + " = KEY " + keyName (keyLearnSlot));
            keyLearnSlot = -1;
            refreshPadGrid();
        }
        return true;
    }

    if (key.getModifiers().isCommandDown() || key.getModifiers().isCtrlDown() || key.getModifiers().isAltDown())
        return false;

    int code = (int) juce::CharacterFunctions::toUpperCase (key.getTextCharacter());
    if (code <= 32)
        code = key.getKeyCode();

    for (int i = 0; i < 16; ++i)
        if (proc.keyCodes[(size_t) i].load() == code)
        {
            if (! keyHeld[i])
            {
                keyHeld[i] = true;
                proc.uiTrigger (bank() * kPadsPerBank + i, 0.85f);
            }
            return true;
        }
    return false;
}

bool AtlantaEditor::keyStateChanged (bool)
{
    if (keyLearnSlot >= 0 || midiText.hasKeyboardFocus (true))
        return false;

    bool used = false;
    for (int i = 0; i < 16; ++i)
    {
        const int code = proc.keyCodes[(size_t) i].load();
        if (code <= 32)
            continue;
        const bool down = juce::KeyPress::isKeyCurrentlyDown (code);
        if (down && ! keyHeld[i])
        {
            keyHeld[i] = true;
            proc.uiTrigger (bank() * kPadsPerBank + i, 0.85f);
            used = true;
        }
        else if (! down && keyHeld[i])
        {
            keyHeld[i] = false;
            proc.uiRelease (bank() * kPadsPerBank + i);
            used = true;
        }
    }
    return used;
}
} // namespace am
