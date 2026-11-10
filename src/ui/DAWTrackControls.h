#pragma once

#include "ui/TramiLookAndFeel.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace trami
{

class DAWMuteButton : public juce::Button
{
public:
    DAWMuteButton();
    ~DAWMuteButton() override = default;

    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
};

class DAWSoloButton : public juce::Button
{
public:
    DAWSoloButton();
    ~DAWSoloButton() override = default;

    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
};

class DAWArmButton : public juce::Button
{
public:
    DAWArmButton();
    ~DAWArmButton() override = default;

    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
};

namespace DAWTrackIcons
{
void drawTrackFoldChevron(juce::Graphics& g, float cx, float cy, float size = 7.0f);
void drawAudioWaveformIcon(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour colour = juce::Colour(0xff9aa0aa));
} // namespace DAWTrackIcons

namespace DAWLoopRuler
{
void drawStudioOneLoopIndicator(juce::Graphics& g, int loopStartX, int loopEndX, int rulerHeight);
} // namespace DAWLoopRuler

} // namespace trami

