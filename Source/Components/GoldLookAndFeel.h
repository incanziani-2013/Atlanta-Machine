#pragma once
#include "../Common.h"

namespace am
{
namespace palette
{
    inline const juce::Colour gold      { 0xffd9aa3c };
    inline const juce::Colour goldLight { 0xfff6dc84 };
    inline const juce::Colour goldDark  { 0xff7c5a14 };
    inline const juce::Colour ink       { 0xff0b0b0a };
    inline const juce::Colour panel     { 0xff151513 };
    inline const juce::Colour metal     { 0xff2a2a27 };
    inline const juce::Colour led       { 0xff7dff2a };
    inline const juce::Colour lcdBg     { 0xff82b625 };
    inline const juce::Colour lcdFg     { 0xff14290a };
    inline const juce::Colour red       { 0xffe02a1f };
}

inline juce::Font uiFont (float h, bool bold = true)
{
    return juce::Font (juce::FontOptions ("Segoe UI", h, bold ? juce::Font::bold : juce::Font::plain));
}
inline juce::Font monoFont (float h)
{
    return juce::Font (juce::FontOptions ("Consolas", h, juce::Font::bold));
}

class GoldLookAndFeel : public juce::LookAndFeel_V4
{
public:
    GoldLookAndFeel()
    {
        using namespace palette;
        setColour (juce::PopupMenu::backgroundColourId, panel);
        setColour (juce::PopupMenu::textColourId, gold);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, goldDark);
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour (juce::PopupMenu::headerTextColourId, goldLight);
        setColour (juce::TextEditor::backgroundColourId, ink);
        setColour (juce::TextEditor::textColourId, goldLight);
        setColour (juce::TextEditor::outlineColourId, goldDark);
        setColour (juce::TextEditor::focusedOutlineColourId, gold);
        setColour (juce::TextEditor::highlightColourId, goldDark);
        setColour (juce::CaretComponent::caretColourId, gold);
        setColour (juce::ComboBox::backgroundColourId, ink);
        setColour (juce::ComboBox::textColourId, goldLight);
        setColour (juce::ComboBox::outlineColourId, goldDark);
        setColour (juce::ComboBox::arrowColourId, gold);
        setColour (juce::Label::textColourId, gold);
        setColour (juce::Slider::trackColourId, goldDark.withAlpha (0.55f));
        setColour (juce::Slider::thumbColourId, gold);
        setColour (juce::Slider::backgroundColourId, ink);
        setColour (juce::Slider::textBoxTextColourId, goldLight);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::AlertWindow::backgroundColourId, panel);
        setColour (juce::AlertWindow::textColourId, goldLight);
        setColour (juce::AlertWindow::outlineColourId, gold);
        setColour (juce::TextButton::buttonColourId, metal);
        setColour (juce::TextButton::textColourOffId, gold);
        setColour (juce::TextButton::textColourOnId, led);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override
    {
        return uiFont (juce::jmin (17.f, (float) buttonHeight * 0.42f));
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startA, float endA, juce::Slider&) override
    {
        using namespace palette;
        auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.f);
        const float d = juce::jmin (b.getWidth(), b.getHeight());
        const auto c = b.getCentre();
        const float r = d * 0.5f;

        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillEllipse (c.x - r, c.y - r + 2.f, d, d);

        g.setGradientFill (juce::ColourGradient (goldLight, c.x - r, c.y - r, goldDark, c.x + r, c.y + r, false));
        g.fillEllipse (c.x - r, c.y - r, d, d);

        const float r2 = r * 0.86f;
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4d4d48), c.x - r2 * 0.4f, c.y - r2 * 0.5f,
                                                 juce::Colour (0xff0d0d0c), c.x + r2, c.y + r2, true));
        g.fillEllipse (c.x - r2, c.y - r2, r2 * 2.f, r2 * 2.f);

        g.setColour (juce::Colours::white.withAlpha (0.05f));
        for (int i = 0; i < 24; ++i)
        {
            const float a = (float) i * juce::MathConstants<float>::twoPi / 24.f;
            g.drawLine (c.x + std::sin (a) * r2 * 0.6f, c.y - std::cos (a) * r2 * 0.6f,
                        c.x + std::sin (a) * r2, c.y - std::cos (a) * r2, 1.f);
        }

        const float ang = startA + pos * (endA - startA);
        g.setColour (goldLight);
        g.drawLine (c.x + std::sin (ang) * r2 * 0.3f, c.y - std::cos (ang) * r2 * 0.3f,
                    c.x + std::sin (ang) * r2 * 0.92f, c.y - std::cos (ang) * r2 * 0.92f, juce::jmax (2.f, r * 0.08f));
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        using namespace palette;
        auto r = b.getLocalBounds().toFloat().reduced (1.f);
        const bool on = b.getToggleState();
        const bool isRed = (bool) b.getProperties()["red"];
        const bool isLed = (bool) b.getProperties()["led"];
        const bool hasIcon = (bool) b.getProperties()["icon"];

        juce::Colour base = metal;
        if (down) base = base.darker (0.4f);
        else if (over) base = base.brighter (0.15f);

        g.setGradientFill (juce::ColourGradient (base.brighter (0.25f), 0.f, r.getY(), base.darker (0.5f), 0.f, r.getBottom(), false));
        g.fillRoundedRectangle (r, 4.f);

        if (on)
        {
            const auto glow = isRed ? red : led;
            g.setColour (glow.withAlpha (0.22f));
            g.fillRoundedRectangle (r, 4.f);
            g.setColour (glow);
            g.drawRoundedRectangle (r, 4.f, 2.f);
        }
        else
        {
            g.setColour (gold.withAlpha (0.8f));
            g.drawRoundedRectangle (r, 4.f, 1.4f);
        }

        if (isLed)
        {
            auto bar = r.withSizeKeepingCentre (r.getWidth() * 0.5f, r.getHeight() * 0.2f);
            g.setColour (on ? led : juce::Colour (0xff1c3a10));
            g.fillRoundedRectangle (bar, 2.f);
        }

        if (hasIcon)
        {
            auto ic = r.withWidth (r.getHeight());
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.fillRoundedRectangle (ic, 4.f);
            g.setColour (gold.withAlpha (0.5f));
            g.drawVerticalLine ((int) ic.getRight(), r.getY() + 3.f, r.getBottom() - 3.f);
            g.setGradientFill (juce::ColourGradient (goldLight, ic.getCentreX() - 5.f, ic.getCentreY() - 5.f,
                                                     goldDark, ic.getCentreX() + 5.f, ic.getCentreY() + 5.f, false));
            g.fillEllipse (ic.getCentreX() - 6.f, ic.getCentreY() - 6.f, 12.f, 12.f);
        }
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        using namespace palette;
        const bool on = b.getToggleState();
        const bool isRed = (bool) b.getProperties()["red"];
        g.setFont (getTextButtonFont (b, b.getHeight()));
        g.setColour (on ? (isRed ? juce::Colour (0xffff7a6e) : goldLight.interpolatedWith (led, 0.6f)) : gold);
        auto area = b.getLocalBounds().reduced (4, 2);
        if ((bool) b.getProperties()["icon"])
            area.removeFromLeft (b.getHeight());
        g.drawFittedText (b.getButtonText(), area, juce::Justification::centred, 1);
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                           juce::Slider::SliderStyle style, juce::Slider& s) override
    {
        using namespace palette;
        if (style == juce::Slider::LinearBar)
        {
            auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (1.f);
            g.setColour (ink);
            g.fillRoundedRectangle (r, 4.f);
            g.setColour (goldDark.withAlpha (0.35f));
            g.fillRoundedRectangle (r.withWidth (juce::jmax (0.f, pos - (float) x)), 4.f);
            g.setColour (gold.withAlpha (0.8f));
            g.drawRoundedRectangle (r, 4.f, 1.4f);
            return;
        }

        const float cy = (float) y + (float) h * 0.5f;
        g.setColour (ink);
        g.fillRoundedRectangle ((float) x, cy - 3.f, (float) w, 6.f, 3.f);
        g.setColour (goldDark);
        g.fillRoundedRectangle ((float) x, cy - 3.f, pos - (float) x, 6.f, 3.f);
        g.setColour (s.findColour (juce::Slider::thumbColourId));
        g.fillRoundedRectangle (pos - 5.f, cy - 10.f, 10.f, 20.f, 3.f);
    }

    juce::Label* createSliderTextBox (juce::Slider& s) override
    {
        auto* l = LookAndFeel_V4::createSliderTextBox (s);
        l->setFont (monoFont (15.f));
        l->setJustificationType (juce::Justification::centred);
        return l;
    }
};
} // namespace am
