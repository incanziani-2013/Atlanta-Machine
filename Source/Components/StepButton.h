#pragma once
#include "GoldLookAndFeel.h"

namespace am
{
class StepButton : public juce::Component
{
public:
    bool on = false, playhead = false, beatMarker = false;
    std::function<void()> onClick;

    void setState (bool isOn, bool isPlayhead)
    {
        if (on != isOn || playhead != isPlayhead)
        {
            on = isOn;
            playhead = isPlayhead;
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        using namespace palette;
        auto r = getLocalBounds().toFloat().reduced (1.f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2b2b28), 0.f, r.getY(), juce::Colour (0xff111110), 0.f, r.getBottom(), false));
        g.fillRoundedRectangle (r, 3.f);
        if (on)
        {
            g.setColour (led.withAlpha (0.85f));
            g.fillRoundedRectangle (r.reduced (4.f), 2.f);
            g.setColour (led.withAlpha (0.25f));
            g.fillRoundedRectangle (r, 3.f);
        }
        g.setColour (beatMarker ? gold : gold.withAlpha (0.55f));
        g.drawRoundedRectangle (r, 3.f, beatMarker ? 1.8f : 1.2f);
        if (playhead)
        {
            g.setColour (juce::Colours::white.withAlpha (0.9f));
            g.drawRoundedRectangle (r.reduced (1.f), 3.f, 2.f);
        }
    }

    void mouseDown (const juce::MouseEvent&) override { if (onClick) onClick(); }
};
} // namespace am
