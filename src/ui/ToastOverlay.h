#pragma once

#include "ui/TramiLookAndFeel.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace trami
{

class ToastOverlay : public juce::Component, private juce::Timer
{
public:
    ToastOverlay()
    {
        setInterceptsMouseClicks(false, false);
        setAlwaysOnTop(true);
    }

    void show(const juce::String& message, bool isError = false)
    {
        message_ = message;
        isError_ = isError;
        alpha_ = 1.0f;
        fadeCounter_ = 50; // ~1.65 seconds hold at 30 fps
        setVisible(true);
        toFront(true);
        startTimerHz(30);
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        if (alpha_ <= 0.01f || message_.isEmpty())
            return;

        const auto bounds = getLocalBounds().toFloat().reduced(1.0f);
        g.setColour(juce::Colour(0xff181f2a).withAlpha(0.96f * alpha_));
        g.fillRoundedRectangle(bounds, 6.0f);

        const auto accentCol = isError_ ? Palette::danger : Palette::accent;
        g.setColour(accentCol.withAlpha(0.85f * alpha_));
        g.drawRoundedRectangle(bounds, 6.0f, 1.4f);

        g.setFont(Typography::bold(12.5f));
        g.setColour(juce::Colours::white.withAlpha(alpha_));
        g.drawText(message_, getLocalBounds(), juce::Justification::centred);
    }

private:
    void timerCallback() override
    {
        if (fadeCounter_ > 0)
        {
            --fadeCounter_;
        }
        else
        {
            alpha_ -= 0.08f;
            if (alpha_ <= 0.0f)
            {
                alpha_ = 0.0f;
                stopTimer();
                setVisible(false);
            }
        }
        repaint();
    }

    juce::String message_;
    bool isError_ = false;
    float alpha_ = 0.0f;
    int fadeCounter_ = 0;
};

} // namespace trami

