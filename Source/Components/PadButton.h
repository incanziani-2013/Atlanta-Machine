#pragma once
#include "GoldLookAndFeel.h"

namespace am
{
class PadButton : public juce::Component, public juce::FileDragAndDropTarget
{
public:
    int index = 0;
    std::function<void (int, float)> onDown;
    std::function<void (int)> onUp, onRightClick;
    std::function<void (int, const juce::StringArray&)> onFiles;

    void setState (const juce::String& t, const juce::String& sample, const juce::String& midi, const juce::String& key,
                   bool isLit, bool isSelected, bool isMissing)
    {
        if (title != t || sampleName != sample || midiText != midi || keyText != key
            || lit != isLit || selected != isSelected || missing != isMissing)
        {
            title = t; sampleName = sample; midiText = midi; keyText = key;
            lit = isLit; selected = isSelected; missing = isMissing;
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        using namespace palette;
        auto r = getLocalBounds().toFloat().reduced (3.f);
        const float cr = 6.f;

        g.setColour (juce::Colours::black);
        g.fillRoundedRectangle (r.expanded (2.f), cr + 2.f);

        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2b2a2c), r.getX(), r.getY(), juce::Colour (0xff0f0f10), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, cr);

        if (lit)
        {
            g.setColour (led.withAlpha (0.20f));
            g.fillRoundedRectangle (r.expanded (4.f), cr + 3.f);
            g.setColour (led.withAlpha (0.30f));
            g.fillRoundedRectangle (r, cr);
            g.setColour (led);
            g.drawRoundedRectangle (r, cr, 3.f);
            g.setColour (led.withAlpha (0.5f));
            g.drawRoundedRectangle (r.reduced (3.f), cr, 1.5f);
        }
        else
        {
            g.setColour (selected ? goldLight : gold.withAlpha (0.85f));
            g.drawRoundedRectangle (r, cr, selected ? 3.f : 1.8f);
        }

        if (dragHover)
        {
            g.setColour (goldLight.withAlpha (0.25f));
            g.fillRoundedRectangle (r, cr);
        }

        const float s = (float) getWidth() / 175.f;
        g.setColour (lit ? juce::Colours::white : goldLight);
        g.setFont (uiFont (22.f * s));
        g.drawText (title, r.reduced (10.f * s, 8.f * s).withHeight (28.f * s), juce::Justification::topLeft, false);

        g.setFont (monoFont (14.f * s));
        g.setColour (missing ? red : goldLight.withAlpha (0.9f));
        g.drawFittedText (missing ? "MISSING: " + sampleName : sampleName,
                          juce::Rectangle<int> ((int) (r.getX() + 10.f * s), (int) (r.getBottom() - 56.f * s), (int) (r.getWidth() - 20.f * s), (int) (24.f * s)),
                          juce::Justification::centredLeft, 1);

        g.setFont (uiFont (12.5f * s, false));
        g.setColour (gold.withAlpha (0.65f));
        g.drawText (midiText, r.reduced (10.f * s, 8.f * s).withTrimmedBottom (0.f), juce::Justification::bottomLeft, false);
        g.drawText (keyText, r.reduced (10.f * s, 8.f * s), juce::Justification::bottomRight, false);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())
        {
            if (onRightClick) onRightClick (index);
            return;
        }
        // Click height sets velocity: top = softer, bottom = harder.
        const float vel = 0.6f + 0.4f * juce::jlimit (0.f, 1.f, (float) e.y / (float) juce::jmax (1, getHeight()));
        if (onDown) onDown (index, vel);
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! e.mods.isPopupMenu() && onUp)
            onUp (index);
    }

    bool isInterestedInFileDrag (const juce::StringArray& files) override
    {
        for (auto& f : files)
        {
            const auto ext = juce::File (f).getFileExtension().toLowerCase();
            if (ext == ".wav" || ext == ".aif" || ext == ".aiff" || ext == ".mp3" || ext == ".flac" || ext == ".ogg")
                return true;
        }
        return false;
    }
    void fileDragEnter (const juce::StringArray&, int, int) override { dragHover = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dragHover = false; repaint(); }
    void filesDropped (const juce::StringArray& files, int, int) override
    {
        dragHover = false;
        repaint();
        if (onFiles) onFiles (index, files);
    }

private:
    juce::String title, sampleName, midiText, keyText;
    bool lit = false, selected = false, missing = false, dragHover = false;
};
} // namespace am
