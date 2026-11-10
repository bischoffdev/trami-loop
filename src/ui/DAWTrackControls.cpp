#include "ui/DAWTrackControls.h"

#include <algorithm>

namespace trami
{

// ==============================================================================
// DAWMuteButton
// ==============================================================================
DAWMuteButton::DAWMuteButton()
    : juce::Button("Mute")
{
    setClickingTogglesState(true);
    setTooltip("Mute Track");
}

void DAWMuteButton::paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    const auto r = getLocalBounds().toFloat().reduced(0.5f);
    constexpr float radius = 3.5f;

    juce::Path p;
    // Rounded on top-left and bottom-left, flat on the right
    p.addRoundedRectangle(r.getX(), r.getY(), r.getWidth(), r.getHeight(), radius, radius, true, false, true, false);

    const bool isOn = getToggleState();
    juce::Colour bgCol;
    juce::Colour borderCol;
    juce::Colour textCol;

    if (isOn)
    {
        bgCol = juce::Colour(0xffd32f2f);
        borderCol = juce::Colour(0xffef5350);
        textCol = juce::Colours::white;
    }
    else
    {
        bgCol = shouldDrawButtonAsDown ? juce::Colour(0xff22252a)
              : shouldDrawButtonAsHighlighted ? juce::Colour(0xff3c4149)
              : juce::Colour(0xff30343a);
        borderCol = juce::Colour(0xff434952);
        textCol = shouldDrawButtonAsHighlighted ? juce::Colours::white : juce::Colour(0xffc5cad2);
    }

    g.setColour(bgCol);
    g.fillPath(p);

    g.setColour(borderCol);
    g.strokePath(p, juce::PathStrokeType(1.0f));

    g.setColour(textCol);
    g.setFont(Typography::bold(12.0f));
    g.drawText("M", getLocalBounds(), juce::Justification::centred, false);
}

// ==============================================================================
// DAWSoloButton
// ==============================================================================
DAWSoloButton::DAWSoloButton()
    : juce::Button("Solo")
{
    setClickingTogglesState(true);
    setTooltip("Solo Track");
}

void DAWSoloButton::paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    const auto r = getLocalBounds().toFloat().reduced(0.5f);
    constexpr float radius = 3.5f;

    juce::Path p;
    // Flat on left, rounded on top-right and bottom-right
    p.addRoundedRectangle(r.getX(), r.getY(), r.getWidth(), r.getHeight(), radius, radius, false, true, false, true);

    const bool isOn = getToggleState();
    juce::Colour bgCol;
    juce::Colour borderCol;
    juce::Colour textCol;

    if (isOn)
    {
        bgCol = juce::Colour(0xfffbc02d);
        borderCol = juce::Colour(0xffffee58);
        textCol = juce::Colour(0xff111418);
    }
    else
    {
        bgCol = shouldDrawButtonAsDown ? juce::Colour(0xff22252a)
              : shouldDrawButtonAsHighlighted ? juce::Colour(0xff3c4149)
              : juce::Colour(0xff30343a);
        borderCol = juce::Colour(0xff434952);
        textCol = shouldDrawButtonAsHighlighted ? juce::Colours::white : juce::Colour(0xffc5cad2);
    }

    g.setColour(bgCol);
    g.fillPath(p);

    g.setColour(borderCol);
    g.strokePath(p, juce::PathStrokeType(1.0f));

    g.setColour(textCol);
    g.setFont(Typography::bold(12.0f));
    g.drawText("S", getLocalBounds(), juce::Justification::centred, false);
}

// ==============================================================================
// DAWArmButton
// ==============================================================================
DAWArmButton::DAWArmButton()
    : juce::Button("Arm")
{
    setClickingTogglesState(true);
    setTooltip("Arm Track for Recording");
}

void DAWArmButton::paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    const auto b = getLocalBounds().toFloat();
    const float diameter = std::min(b.getWidth(), b.getHeight()) - 2.0f;
    const auto r = juce::Rectangle<float>(b.getCentreX() - diameter * 0.5f,
                                          b.getCentreY() - diameter * 0.5f,
                                          diameter, diameter);

    const bool isOn = getToggleState();
    juce::Colour bgCol;
    juce::Colour borderCol;
    juce::Colour dotCol;

    if (isOn)
    {
        bgCol = juce::Colour(0xff3d1c1e);
        borderCol = juce::Colour(0xffe53935);
        dotCol = juce::Colour(0xffe53935);
    }
    else
    {
        bgCol = shouldDrawButtonAsDown ? juce::Colour(0xff22252a)
              : shouldDrawButtonAsHighlighted ? juce::Colour(0xff3c4149)
              : juce::Colour(0xff30343a);
        borderCol = juce::Colour(0xff434952);
        dotCol = shouldDrawButtonAsHighlighted ? juce::Colour(0xffb0b8c4) : juce::Colour(0xff7c838e);
    }

    g.setColour(bgCol);
    g.fillEllipse(r);

    g.setColour(borderCol);
    g.drawEllipse(r, 1.0f);

    const float dotRadius = 3.2f;
    g.setColour(dotCol);
    g.fillEllipse(r.getCentreX() - dotRadius, r.getCentreY() - dotRadius, dotRadius * 2.0f, dotRadius * 2.0f);
}

// ==============================================================================
// DAWTrackIcons
// ==============================================================================
namespace DAWTrackIcons
{

void drawTrackFoldChevron(juce::Graphics& g, float cx, float cy, float size)
{
    juce::Path chevron;
    const float halfW = size * 0.5f;
    const float h = size * 0.55f;
    chevron.startNewSubPath(cx - halfW, cy - h * 0.5f);
    chevron.lineTo(cx + halfW, cy - h * 0.5f);
    chevron.lineTo(cx, cy + h * 0.5f);
    chevron.closeSubPath();
    g.setColour(juce::Colours::black.withAlpha(0.65f));
    g.fillPath(chevron);
}

void drawAudioWaveformIcon(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour colour)
{
    const float cx = bounds.getCentreX();
    const float cy = bounds.getCentreY();
    constexpr float barW = 1.8f;
    constexpr float heights[] = { 4.5f, 9.5f, 15.0f, 9.0f, 4.0f };
    constexpr float offsets[] = { -5.5f, -2.75f, 0.0f, 2.75f, 5.5f };

    g.setColour(colour);
    for (int i = 0; i < 5; ++i)
    {
        const float h = heights[i];
        g.fillRoundedRectangle(cx + offsets[i] - barW * 0.5f, cy - h * 0.5f, barW, h, barW * 0.5f);
    }
}

} // namespace DAWTrackIcons

namespace DAWLoopRuler
{

void drawStudioOneLoopIndicator(juce::Graphics& g, int loopStartX, int loopEndX, int rulerHeight)
{
    juce::ignoreUnused(rulerHeight);
    const float sx = static_cast<float>(loopStartX);
    const float ex = static_cast<float>(loopEndX);
    const float w = std::max(8.0f, ex - sx);

    const juce::Colour loopCyan(0xff42b4e6);
    constexpr float topH = 6.0f;
    constexpr float flagH = 15.0f;
    constexpr float flagW = 3.5f;
    constexpr float chamferW = 5.0f;

    juce::Path p;
    p.startNewSubPath(sx, 0.0f);
    p.lineTo(sx + w, 0.0f);
    p.lineTo(sx + w, flagH);
    p.lineTo(sx + w - flagW, flagH);
    p.lineTo(sx + w - flagW - chamferW, topH);
    p.lineTo(sx + flagW + chamferW, topH);
    p.lineTo(sx + flagW, flagH);
    p.lineTo(sx, flagH);
    p.closeSubPath();

    g.setColour(loopCyan);
    g.fillPath(p);

    g.setColour(loopCyan.brighter(0.2f));
    g.strokePath(p, juce::PathStrokeType(0.75f));
}

} // namespace DAWLoopRuler

} // namespace trami

