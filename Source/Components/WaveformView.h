#pragma once
#include "GoldLookAndFeel.h"
#include "../Pad.h"

namespace am
{
// Waveform of the selected pad with draggable START / END handles and a live playhead.
class WaveformView : public juce::Component
{
public:
    std::function<void()> onDragStart;                    // called once when a handle is grabbed (for undo)
    std::function<void (float, float)> onRangeChanged;    // start, end (0..1)

    void setSample (SamplePtr s, float st, float en)
    {
        sample = std::move (s);
        startN = st;
        endN = en;
        rebuildPeaks();
        repaint();
    }

    void setRange (float st, float en)
    {
        if (st != startN || en != endN)
        {
            startN = st;
            endN = en;
            repaint();
        }
    }

    void setPlayFraction (float f)
    {
        if (std::abs (f - playFrac) > 1.0e-5f)
        {
            playFrac = f;
            repaint();
        }
    }

    void resized() override { rebuildPeaks(); }

    void paint (juce::Graphics& g) override
    {
        using namespace palette;
        auto r = getLocalBounds().toFloat();
        g.setColour (ink);
        g.fillRoundedRectangle (r, 5.f);
        g.setColour (gold.withAlpha (0.8f));
        g.drawRoundedRectangle (r.reduced (0.7f), 5.f, 1.4f);

        const float midY = r.getCentreY();
        g.setColour (goldDark.withAlpha (0.5f));
        g.drawHorizontalLine ((int) midY, r.getX() + 2.f, r.getRight() - 2.f);

        if (sample == nullptr || peaks.empty())
        {
            g.setColour (gold.withAlpha (0.45f));
            g.setFont (uiFont (16.f));
            g.drawText ("DROP A SAMPLE ON A PAD  /  LOAD SAMPLE", getLocalBounds(), juce::Justification::centred);
            return;
        }

        const float w = r.getWidth();
        const float x0 = startN * w, x1 = endN * w;

        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRect (0.f, 0.f, x0, r.getHeight());
        g.fillRect (x1, 0.f, w - x1, r.getHeight());

        const float half = r.getHeight() * 0.46f;
        for (int x = 0; x < (int) peaks.size(); ++x)
        {
            const bool inside = (float) x >= x0 && (float) x <= x1;
            g.setColour (inside ? led.withAlpha (0.9f) : goldDark.withAlpha (0.6f));
            const float top = midY - peaks[(size_t) x].second * half;
            const float bot = midY - peaks[(size_t) x].first * half;
            g.drawVerticalLine (x, top, juce::jmax (top + 1.f, bot));
        }

        // handles
        g.setColour (goldLight);
        g.fillRect (x0 - 1.f, 0.f, 3.f, r.getHeight());
        g.fillRect (x1 - 2.f, 0.f, 3.f, r.getHeight());
        g.setFont (uiFont (12.f));
        g.drawText ("START", juce::Rectangle<float> (x0 + 4.f, 2.f, 50.f, 14.f), juce::Justification::left);
        g.drawText ("END", juce::Rectangle<float> (x1 - 54.f, 2.f, 50.f, 14.f), juce::Justification::right);

        if (playFrac >= 0.f)
        {
            g.setColour (juce::Colours::white);
            g.fillRect (playFrac * w - 1.f, 0.f, 2.f, r.getHeight());
        }
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        setMouseCursor (sample != nullptr && nearest (e.x) >= 0 ? juce::MouseCursor::LeftRightResizeCursor
                                                                  : juce::MouseCursor::NormalCursor);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (sample == nullptr || getWidth() < 2)
            return;
        dragging = nearest (e.x);
        if (dragging >= 0 && onDragStart)
            onDragStart();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (sample == nullptr || dragging < 0 || getWidth() < 2)
            return;
        const float v = juce::jlimit (0.f, 1.f, (float) e.x / (float) getWidth());
        const float minGap = 0.004f;
        if (dragging == 0) startN = juce::jmin (v, endN - minGap);
        else               endN   = juce::jmax (v, startN + minGap);
        startN = juce::jlimit (0.f, 1.f, startN);
        endN = juce::jlimit (0.f, 1.f, endN);
        repaint();
        if (onRangeChanged)
            onRangeChanged (startN, endN);
    }

    void mouseUp (const juce::MouseEvent&) override { dragging = -1; }

private:
    // 0 = start handle, 1 = end handle, -1 = nothing within reach (picks the closest one when close enough)
    int nearest (int x) const
    {
        if (getWidth() < 2)
            return -1;
        const float ds = std::abs ((float) x - startN * (float) getWidth());
        const float de = std::abs ((float) x - endN * (float) getWidth());
        const float reach = 14.f;
        if (ds > reach && de > reach)
            return -1;
        return ds <= de ? 0 : 1;
    }

    void rebuildPeaks()
    {
        peaks.clear();
        if (sample == nullptr || getWidth() < 2 || sample->length() < 2)
            return;

        const int w = getWidth();
        const int n = sample->length();
        const int nCh = sample->data.getNumChannels();
        peaks.assign ((size_t) w, { 0.f, 0.f });
        float maxAbs = 1.0e-6f;

        for (int x = 0; x < w; ++x)
        {
            const int a = (int) ((juce::int64) x * n / w);
            const int b = juce::jmax (a + 1, (int) ((juce::int64) (x + 1) * n / w));
            float lo = 0.f, hi = 0.f;
            for (int ch = 0; ch < nCh; ++ch)
            {
                const float* d = sample->data.getReadPointer (ch);
                for (int i = a; i < juce::jmin (b, n); ++i)
                {
                    lo = juce::jmin (lo, d[i]);
                    hi = juce::jmax (hi, d[i]);
                }
            }
            peaks[(size_t) x] = { lo, hi };
            maxAbs = juce::jmax (maxAbs, std::abs (lo), std::abs (hi));
        }
        for (auto& p : peaks)
        {
            p.first /= maxAbs;
            p.second /= maxAbs;
        }
    }

    SamplePtr sample;
    std::vector<std::pair<float, float>> peaks;   // (min, max) per pixel column, normalised
    float startN = 0.f, endN = 1.f, playFrac = -1.f;
    int dragging = -1;
};
} // namespace am
