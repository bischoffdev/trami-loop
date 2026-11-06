#include "ui/IconButton.h"

#include <cmath>

namespace trami
{

IconButton::IconButton(const juce::String& buttonName, Icon icon, const juce::String& label)
    : juce::Button(buttonName), icon_(icon), label_(label)
{
}

void IconButton::setIcon(Icon icon)
{
    if (icon_ != icon)
    {
        icon_ = icon;
        repaint();
    }
}

void IconButton::setLabel(const juce::String& label)
{
    if (label_ != label)
    {
        label_ = label;
        repaint();
    }
}

void IconButton::setActiveColour(juce::Colour colour)
{
    activeColour_ = colour;
    repaint();
}

void IconButton::paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    auto bounds = getLocalBounds().toFloat().reduced(0.5f);
    const bool isOn = getToggleState();

    juce::Colour bg;
    juce::Colour border;
    juce::Colour iconCol;
    juce::Colour textCol;

    if (isOn)
    {
        bg = activeColour_.withAlpha(0.22f);
        border = activeColour_.withAlpha(0.85f);
        iconCol = activeColour_;
        textCol = juce::Colours::white;
    }
    else if (shouldDrawButtonAsDown)
    {
        bg = Palette::surface.darker(0.2f);
        border = Palette::gridLine.brighter(0.4f);
        iconCol = Palette::text;
        textCol = Palette::text;
    }
    else if (shouldDrawButtonAsHighlighted)
    {
        bg = Palette::surface.brighter(0.12f);
        border = Palette::gridLine.brighter(0.35f);
        iconCol = Palette::text.brighter(0.2f);
        textCol = Palette::text;
    }
    else
    {
        bg = Palette::surface;
        border = Palette::gridLine.brighter(0.08f);
        iconCol = Palette::textDim.brighter(0.3f);
        textCol = Palette::textDim.brighter(0.35f);
    }

    g.setColour(bg);
    g.fillRoundedRectangle(bounds, 5.0f);

    g.setColour(border);
    g.drawRoundedRectangle(bounds, 5.0f, isOn ? 1.5f : 1.0f);

    // Layout: top is icon, bottom is text label underneath
    const float totalH = bounds.getHeight();
    const float iconH = std::round(totalH * 0.58f);
    auto iconArea = bounds.removeFromTop(iconH);
    auto labelArea = bounds;

    if (shouldDrawButtonAsDown)
    {
        iconArea.translate(0.0f, 1.0f);
        labelArea.translate(0.0f, 1.0f);
    }

    drawIcon(g, iconArea, iconCol);

    g.setColour(textCol);
    g.setFont(Typography::bold(10.5f));
    g.drawText(label_, labelArea.reduced(2.0f, 0.0f), juce::Justification::centred, true);
}

void IconButton::drawIcon(juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour)
{
    g.setColour(colour);
    const float cx = std::round(area.getCentreX());
    const float cy = std::round(area.getCentreY()) + 1.0f;

    switch (icon_)
    {
    case Icon::Play:
    {
        juce::Path p;
        p.addTriangle(cx - 5.0f, cy - 6.5f, cx + 6.5f, cy, cx - 5.0f, cy + 6.5f);
        g.fillPath(p);
        break;
    }
    case Icon::Stop:
    {
        g.fillRoundedRectangle(cx - 5.5f, cy - 5.5f, 11.0f, 11.0f, 1.5f);
        break;
    }
    case Icon::Record:
    {
        g.fillEllipse(cx - 5.5f, cy - 5.5f, 11.0f, 11.0f);
        break;
    }
    case Icon::Rewind:
    {
        juce::Path p;
        p.addTriangle(cx, cy - 5.5f, cx - 6.0f, cy, cx, cy + 5.5f);
        p.addTriangle(cx + 6.0f, cy - 5.5f, cx, cy, cx + 6.0f, cy + 5.5f);
        g.fillPath(p);
        g.fillRect(cx - 7.5f, cy - 5.5f, 2.0f, 11.0f);
        break;
    }
    case Icon::Metronome:
    {
        juce::Path p;
        p.startNewSubPath(cx - 6.0f, cy + 6.5f);
        p.lineTo(cx + 6.0f, cy + 6.5f);
        p.lineTo(cx + 2.5f, cy - 6.5f);
        p.lineTo(cx - 2.5f, cy - 6.5f);
        p.closeSubPath();
        g.strokePath(p, juce::PathStrokeType(1.4f));

        g.drawLine(cx, cy + 5.0f, cx + 3.5f, cy - 4.5f, 1.6f);
        g.fillEllipse(cx + 2.0f, cy - 3.5f, 3.2f, 3.2f);
        break;
    }
    case Icon::Swing:
    {
        juce::Path p;
        p.startNewSubPath(cx - 7.0f, cy);
        p.cubicTo(cx - 3.5f, cy - 6.5f, cx, cy - 6.5f, cx, cy);
        p.cubicTo(cx, cy + 6.5f, cx + 3.5f, cy + 6.5f, cx + 7.0f, cy);
        g.strokePath(p, juce::PathStrokeType(1.8f));
        break;
    }
    case Icon::Browser:
    {
        g.drawRoundedRectangle(cx - 7.0f, cy - 6.0f, 14.0f, 12.0f, 2.0f, 1.4f);
        g.fillRect(cx - 7.0f, cy - 6.0f, 4.5f, 12.0f);
        break;
    }
    case Icon::PatternMode:
    {
        const float s = 4.0f;
        g.fillRoundedRectangle(cx - s - 1.5f, cy - s - 1.5f, s, s, 1.0f);
        g.fillRoundedRectangle(cx + 1.5f, cy - s - 1.5f, s, s, 1.0f);
        g.fillRoundedRectangle(cx - s - 1.5f, cy + 1.5f, s, s, 1.0f);
        g.fillRoundedRectangle(cx + 1.5f, cy + 1.5f, s, s, 1.0f);
        break;
    }
    case Icon::ArrangerMode:
    {
        g.fillRoundedRectangle(cx - 7.0f, cy - 5.5f, 6.0f, 2.8f, 1.0f);
        g.fillRoundedRectangle(cx + 0.5f, cy - 5.5f, 6.5f, 2.8f, 1.0f);
        g.fillRoundedRectangle(cx - 4.5f, cy - 1.4f, 10.5f, 2.8f, 1.0f);
        g.fillRoundedRectangle(cx - 7.0f, cy + 2.7f, 9.0f, 2.8f, 1.0f);
        break;
    }
    case Icon::Undo:
    {
        juce::Path p;
        p.addCentredArc(cx, cy + 0.5f, 5.0f, 5.0f, 0.0f, 0.8f, 5.2f, true);
        g.strokePath(p, juce::PathStrokeType(1.6f));
        juce::Path ah;
        ah.addTriangle(cx - 6.5f, cy - 1.5f, cx - 2.5f, cy - 5.5f, cx - 2.5f, cy + 2.0f);
        g.fillPath(ah);
        break;
    }
    case Icon::Redo:
    {
        juce::Path p;
        p.addCentredArc(cx, cy + 0.5f, 5.0f, 5.0f, 0.0f, -5.2f, -0.8f, true);
        g.strokePath(p, juce::PathStrokeType(1.6f));
        juce::Path ah;
        ah.addTriangle(cx + 6.5f, cy - 1.5f, cx + 2.5f, cy - 5.5f, cx + 2.5f, cy + 2.0f);
        g.fillPath(ah);
        break;
    }
    case Icon::Save:
    {
        juce::Path p;
        p.startNewSubPath(cx - 6.0f, cy - 6.0f);
        p.lineTo(cx + 3.0f, cy - 6.0f);
        p.lineTo(cx + 6.0f, cy - 3.0f);
        p.lineTo(cx + 6.0f, cy + 6.0f);
        p.lineTo(cx - 6.0f, cy + 6.0f);
        p.closeSubPath();
        g.strokePath(p, juce::PathStrokeType(1.4f));
        g.fillRect(cx - 3.5f, cy - 5.0f, 5.0f, 3.0f);
        g.fillRect(cx - 3.5f, cy + 1.0f, 7.0f, 4.0f);
        break;
    }
    case Icon::Open:
    {
        juce::Path p;
        p.startNewSubPath(cx - 6.5f, cy - 4.5f);
        p.lineTo(cx - 2.5f, cy - 4.5f);
        p.lineTo(cx - 1.0f, cy - 2.5f);
        p.lineTo(cx + 6.5f, cy - 2.5f);
        p.lineTo(cx + 6.5f, cy + 5.5f);
        p.lineTo(cx - 6.5f, cy + 5.5f);
        p.closeSubPath();
        g.strokePath(p, juce::PathStrokeType(1.4f));
        break;
    }
    case Icon::Export:
    {
        g.drawHorizontalLine(static_cast<int>(cy + 5.5f), cx - 6.0f, cx + 6.0f);
        g.drawLine(cx, cy - 6.0f, cx, cy + 2.0f, 1.8f);
        juce::Path ah;
        ah.addTriangle(cx, cy + 3.5f, cx - 4.0f, cy - 0.5f, cx + 4.0f, cy - 0.5f);
        g.fillPath(ah);
        break;
    }
    case Icon::Settings:
    {
        g.drawLine(cx - 4.5f, cy - 5.5f, cx - 4.5f, cy + 5.5f, 1.4f);
        g.drawLine(cx, cy - 5.5f, cx, cy + 5.5f, 1.4f);
        g.drawLine(cx + 4.5f, cy - 5.5f, cx + 4.5f, cy + 5.5f, 1.4f);
        g.fillEllipse(cx - 6.0f, cy - 2.5f, 3.0f, 3.0f);
        g.fillEllipse(cx - 1.5f, cy + 1.0f, 3.0f, 3.0f);
        g.fillEllipse(cx + 3.0f, cy - 1.5f, 3.0f, 3.0f);
        break;
    }
    case Icon::Plus:
    {
        g.drawLine(cx, cy - 5.0f, cx, cy + 5.0f, 2.0f);
        g.drawLine(cx - 5.0f, cy, cx + 5.0f, cy, 2.0f);
        break;
    }
    case Icon::Duplicate:
    {
        g.drawRoundedRectangle(cx - 5.5f, cy - 3.5f, 8.0f, 8.0f, 1.5f, 1.3f);
        g.drawRoundedRectangle(cx - 2.5f, cy - 6.5f, 8.0f, 8.0f, 1.5f, 1.3f);
        break;
    }
    case Icon::Delete:
    {
        g.drawLine(cx - 5.5f, cy - 4.5f, cx + 5.5f, cy - 4.5f, 1.4f);
        g.drawRoundedRectangle(cx - 4.5f, cy - 3.5f, 9.0f, 9.0f, 1.0f, 1.3f);
        g.drawLine(cx - 1.5f, cy - 1.5f, cx - 1.5f, cy + 3.5f, 1.2f);
        g.drawLine(cx + 1.5f, cy - 1.5f, cx + 1.5f, cy + 3.5f, 1.2f);
        break;
    }
    case Icon::Edit:
    {
        juce::Path p;
        p.startNewSubPath(cx + 4.5f, cy - 5.5f);
        p.lineTo(cx + 6.0f, cy - 4.0f);
        p.lineTo(cx - 2.5f, cy + 4.5f);
        p.lineTo(cx - 5.5f, cy + 5.5f);
        p.lineTo(cx - 4.5f, cy + 2.5f);
        p.closeSubPath();
        g.strokePath(p, juce::PathStrokeType(1.4f));
        break;
    }
    case Icon::ZoomIn:
    {
        g.drawEllipse(cx - 5.0f, cy - 6.0f, 9.0f, 9.0f, 1.4f);
        g.drawLine(cx + 2.0f, cy + 1.0f, cx + 5.5f, cy + 4.5f, 2.0f);
        g.drawLine(cx - 2.5f, cy - 1.5f, cx + 1.5f, cy - 1.5f, 1.4f);
        g.drawLine(cx - 0.5f, cy - 3.5f, cx - 0.5f, cy + 0.5f, 1.4f);
        break;
    }
    case Icon::ZoomOut:
    {
        g.drawEllipse(cx - 5.0f, cy - 6.0f, 9.0f, 9.0f, 1.4f);
        g.drawLine(cx + 2.0f, cy + 1.0f, cx + 5.5f, cy + 4.5f, 2.0f);
        g.drawLine(cx - 2.5f, cy - 1.5f, cx + 1.5f, cy - 1.5f, 1.4f);
        break;
    }
    case Icon::ZoomReset:
    {
        g.drawEllipse(cx - 5.0f, cy - 6.0f, 9.0f, 9.0f, 1.4f);
        g.drawLine(cx + 2.0f, cy + 1.0f, cx + 5.5f, cy + 4.5f, 2.0f);
        break;
    }
    }
}

} // namespace trami

