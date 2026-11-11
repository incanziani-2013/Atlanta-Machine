#pragma once
#include "PluginProcessor.h"
#include "Components/GoldLookAndFeel.h"
#include "Components/PadButton.h"
#include "Components/StepButton.h"
#include "Components/WaveformView.h"

namespace am
{
class AtlantaEditor : public juce::AudioProcessorEditor,
                      private juce::Timer
{
public:
    explicit AtlantaEditor (AtlantaProcessor&);
    ~AtlantaEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    bool keyStateChanged (bool isDown) override;
    void mouseDown (const juce::MouseEvent&) override { grabKeyboardFocus(); }

private:
    static constexpr int kW = 1120, kH = 880;

    // All widgets live on this fixed-size surface, which is scaled to the window size.
    struct Surface : public juce::Component
    {
        std::function<void (juce::Graphics&)> onPaint;
        std::function<void()> onClick;
        void paint (juce::Graphics& g) override { if (onPaint) onPaint (g); }
        void mouseDown (const juce::MouseEvent&) override { if (onClick) onClick(); }
    };

    // ---- helpers
    void timerCallback() override;
    void paintSurface (juce::Graphics&);
    void layout();
    void styleLabel (juce::Label&, const juce::String& text, float size = 13.f,
                     juce::Justification j = juce::Justification::centredLeft);
    void styleLinear (juce::Slider&, double lo, double hi, double step, double def, const juce::String& suffix = {});
    void styleKnob (juce::Slider&);
    void addChild (juce::Component& c) { surface.addAndMakeVisible (c); }

    int selPad() const { return juce::jlimit (0, kNumPads - 1, proc.selectedPad.load()); }
    int bank() const   { return juce::jlimit (0, kNumBanks - 1, proc.selectedBank.load()); }
    Pad& pad()         { return proc.pads[(size_t) selPad()]; }

    void selectPad (int index);
    void selectBank (int b);
    void refreshAll();
    void refreshPadGrid();
    void refreshSelectedControls();
    void refreshSteps();
    void refreshWaveform();

    void padDown (int slot, float vel);
    void padUp (int slot);
    void padMenu (int slot);
    void padFilesDropped (int slot, const juce::StringArray& files);

    void chooseSampleFor (int padIndex);
    void showError (const juce::String& title, const juce::String& msg);
    void warnMissing();

    void kitNew();
    void kitLoad();
    void kitSave();
    void kitSaveAs();
    void kitReset();
    void doSaveAs (const juce::String& name);
    void confirm (const juce::String& title, const juce::String& msg, std::function<void()> yes);
    void learnKey (int slot);
    void commitMidiText();

    juce::String padTitle (int padIndex) const;
    juce::String keyName (int slot) const;

    AtlantaProcessor& proc;
    GoldLookAndFeel laf;
    Surface surface;

    // header
    juce::ComboBox factoryBox;
    juce::TextButton btnNew { "NEW" }, btnLoad { "LOAD" }, btnSave { "SAVE" }, btnSaveAs { "SAVE AS" }, btnReset { "RESET" };

    // pads
    juce::TextButton bankBtn[kNumBanks];
    juce::TextButton btnUndo { "UNDO" }, btnKeys { "KEYS" };
    PadButton padBtn[kPadsPerBank];

    // FX + main
    juce::Slider fxAmt[4], fxMix[4];
    juce::Label fxName[4], fxAmtLbl[4], fxMixLbl[4];
    juce::Slider mainVol, mainPan, bpmSlider, swingSlider;
    juce::ToggleButton hostSyncBtn { "HOST SYNC" };
    juce::Label lblBpm, lblVol, lblPan, lblSwing;

    // sample controls
    juce::Slider volSl, pitchSl, panSl;
    juce::Label lblVolP, lblPitchP, lblPanP, lblChoke, lblMidi;
    juce::TextButton btnReverse { "REVERSE" }, btnNormalize { "NORMALIZE" }, btnGate { "GATE MODE" };
    juce::ComboBox chokeBox;
    juce::TextEditor midiText;
    juce::TextButton btnLearn { "LEARN" }, btnLoadSample { "LOAD SAMPLE" }, btnClearPad { "CLEAR PAD" }, btnAudition { "AUDITION" };
    juce::ComboBox octaveBox, middleCBox;

    // waveform
    WaveformView wave;

    // sequencer
    StepButton steps[kNumSteps];
    juce::TextButton btnPlay { "PLAY" }, btnStop { "STOP" }, btnRec { "REC" };
    juce::ComboBox patternBox, lengthBox;
    juce::Label lblPattern, lblLength;
    juce::TextButton btnClearPattern { "CLEAR PATTERN" }, btnClearPadSteps { "CLEAR PAD" };

    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAtt;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAtt;

    std::unique_ptr<juce::FileChooser> chooser;
    bool keyHeld[16] {};
    int keyLearnSlot = -1;
    int litFrames[kNumPads] {};
    int lastSelected = -1, lastBank = -1;
    const Sample* lastSamplePtr = nullptr;
    juce::String statusText;
    int statusFrames = 0;
    int tick = 0;
    void setStatus (const juce::String& s) { statusText = s; statusFrames = 90; }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AtlantaEditor)
};
} // namespace am
