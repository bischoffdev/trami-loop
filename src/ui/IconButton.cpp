#include "ui/IconButton.h"

#include <cmath>

namespace trami
{

IconButton::IconButton(const juce::String& buttonName, Icon icon, const juce::String& label)
    : juce::Button(buttonName), icon_(icon), label_(label)
{
    if (label.isEmpty() && getTooltip().isEmpty() && !buttonName.isEmpty())
        setTooltip(buttonName);
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
        if (label_.isEmpty() && getTooltip().isEmpty() && !getName().isEmpty())
            setTooltip(getName());
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
    constexpr float radius = 5.5f;
    const bool isOn = getToggleState();

    juce::Colour topCol;
    juce::Colour botCol;
    juce::Colour border;
    juce::Colour iconCol = juce::Colour(0xffe0e0e8);
    juce::Colour textCol = juce::Colour(0xffe8e8f0);

    if (isOn)
    {
        topCol = activeColour_.interpolatedWith(juce::Colour(0xff2c2c34), 0.58f);
        botCol = activeColour_.interpolatedWith(juce::Colour(0xff1d1d24), 0.70f);
        border = activeColour_.withAlpha(0.85f);
        iconCol = activeColour_.brighter(0.25f);
        textCol = juce::Colours::white;
    }
    else if (shouldDrawButtonAsDown)
    {
        topCol = juce::Colour(0xff222227);
        botCol = juce::Colour(0xff2d2d33);
        border = juce::Colour(0xff121216);
        iconCol = juce::Colours::white;
        textCol = juce::Colours::white;
    }
    else if (shouldDrawButtonAsHighlighted)
    {
        topCol = juce::Colour(0xff4e4e57);
        botCol = juce::Colour(0xff34343c);
        border = juce::Colour(0xff686874);
        iconCol = juce::Colours::white;
        textCol = juce::Colours::white;
    }
    else
    {
        topCol = juce::Colour(0xff43434b);
        botCol = juce::Colour(0xff2a2a30);
        border = juce::Colour(0xff141418);
    }

    // Standard DAW colors for key transport icons
    if (icon_ == Icon::Record)
    {
        if (isOn)
        {
            topCol = juce::Colour(0xff5a2224);
            botCol = juce::Colour(0xff3b1517);
            border = juce::Colour(0xffef5350);
            iconCol = juce::Colour(0xffff5252);
            textCol = juce::Colours::white;
        }
        else
        {
            iconCol = juce::Colour(0xffe53935);
        }
    }
    else if (icon_ == Icon::Play && isOn)
    {
        topCol = juce::Colour(0xff244830);
        botCol = juce::Colour(0xff193322);
        border = juce::Colour(0xff66bb6a);
        iconCol = juce::Colour(0xff66bb6a);
        textCol = juce::Colours::white;
    }
    else if (icon_ == Icon::Loop && isOn)
    {
        topCol = juce::Colour(0xff524222);
        botCol = juce::Colour(0xff382c16);
        border = juce::Colour(0xffffca28);
        iconCol = juce::Colour(0xffffca28);
        textCol = juce::Colours::white;
    }
    else if (icon_ == Icon::Follow && isOn)
    {
        topCol = juce::Colour(0xff264354);
        botCol = juce::Colour(0xff1a2f3d);
        border = Palette::accent.withAlpha(0.85f);
        iconCol = Palette::accent;
        textCol = juce::Colours::white;
    }

    // Subtle drop shadow underneath button
    g.setColour(juce::Colours::black.withAlpha(0.28f));
    g.fillRoundedRectangle(bounds.translated(0.0f, 1.0f), radius);

    // Brushed metallic vertical bevel fill (matching TransportBar & TramiLookAndFeel)
    juce::ColourGradient grad(topCol, bounds.getX(), bounds.getY(),
                              botCol, bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill(grad);
    g.fillRoundedRectangle(bounds, radius);

    // Delicate inner top specular highlight
    if (!shouldDrawButtonAsDown)
    {
        g.setColour(juce::Colours::white.withAlpha(shouldDrawButtonAsHighlighted ? 0.22f : 0.14f));
        g.drawHorizontalLine(juce::roundToInt(bounds.getY() + 1.0f),
                             bounds.getX() + radius * 0.7f, bounds.getRight() - radius * 0.7f);
    }

    g.setColour(border);
    g.drawRoundedRectangle(bounds, radius, isOn ? 1.3f : 1.0f);

    const float totalW = bounds.getWidth();
    const float totalH = bounds.getHeight();
    juce::Rectangle<float> iconArea;
    juce::Rectangle<float> labelArea;

    // Prefer clean horizontal icon + label layout on toolbar-height buttons
    const bool useHorizontal = !label_.isEmpty() && (totalW >= 54.0f && totalW > totalH * 1.45f);

    if (label_.isEmpty())
    {
        iconArea = bounds.reduced(2.0f);
    }
    else if (useHorizontal)
    {
        const float iconW = std::min(20.0f, totalH - 6.0f);
        auto inner = bounds.reduced(5.0f, 2.0f);
        iconArea = inner.removeFromLeft(iconW);
        inner.removeFromLeft(3.0f);
        labelArea = inner;
    }
    else
    {
        const float iconH = std::round(totalH * 0.52f);
        iconArea = bounds.removeFromTop(iconH);
        labelArea = bounds;
    }

    if (shouldDrawButtonAsDown)
    {
        iconArea.translate(0.0f, 1.0f);
        labelArea.translate(0.0f, 1.0f);
    }

    drawIcon(g, iconArea, iconCol);

    if (!label_.isEmpty())
    {
        g.setColour(textCol);
        float fontSz = useHorizontal ? 11.5f : (totalH >= 38.0f ? 11.0f : 10.0f);
        auto font = juce::Font(Typography::bold(fontSz));

        const float textW = static_cast<float>(font.getStringWidth(label_));
        const float maxTextW = labelArea.getWidth() - 2.0f;
        if (textW > maxTextW && maxTextW > 10.0f)
        {
            fontSz = std::max(8.5f, fontSz * (maxTextW / textW));
            font = juce::Font(Typography::bold(fontSz));
        }
        g.setFont(font);

        g.drawText(label_, labelArea,
                   useHorizontal ? juce::Justification::centredLeft : juce::Justification::centred,
                   false);
    }
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
    case Icon::Loop:
    {
        juce::Path p;
        p.startNewSubPath(cx - 6.5f, cy + 1.5f);
        p.lineTo(cx - 6.5f, cy - 2.5f);
        p.cubicTo(cx - 6.5f, cy - 5.5f, cx + 6.5f, cy - 5.5f, cx + 6.5f, cy - 2.5f);
        p.lineTo(cx + 6.5f, cy - 0.5f);

        p.startNewSubPath(cx + 6.5f, cy - 1.5f);
        p.lineTo(cx + 6.5f, cy + 2.5f);
        p.cubicTo(cx + 6.5f, cy + 5.5f, cx - 6.5f, cy + 5.5f, cx - 6.5f, cy + 2.5f);
        p.lineTo(cx - 6.5f, cy + 0.5f);
        g.strokePath(p, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path a1;
        a1.addTriangle(cx + 6.5f, cy + 1.5f, cx + 3.5f, cy - 2.0f, cx + 9.5f, cy - 2.0f);
        g.fillPath(a1);

        juce::Path a2;
        a2.addTriangle(cx - 6.5f, cy - 1.5f, cx - 3.5f, cy + 2.0f, cx - 9.5f, cy + 2.0f);
        g.fillPath(a2);
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
    case Icon::New:
    {
        juce::Path p;
        p.startNewSubPath(cx - 5.0f, cy - 6.0f);
        p.lineTo(cx + 1.5f, cy - 6.0f);
        p.lineTo(cx + 5.0f, cy - 2.5f);
        p.lineTo(cx + 5.0f, cy + 6.0f);
        p.lineTo(cx - 5.0f, cy + 6.0f);
        p.closeSubPath();
        g.strokePath(p, juce::PathStrokeType(1.4f));

        juce::Path fold;
        fold.startNewSubPath(cx + 1.5f, cy - 6.0f);
        fold.lineTo(cx + 1.5f, cy - 2.5f);
        fold.lineTo(cx + 5.0f, cy - 2.5f);
        g.strokePath(fold, juce::PathStrokeType(1.2f));
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
    case Icon::Follow:
    {
        // Vertical playhead line
        g.setColour(colour);
        g.drawVerticalLine(juce::roundToInt(cx + 3.0f), cy - 6.0f, cy + 6.0f);

        // Playhead top triangle marker
        juce::Path phMarker;
        phMarker.addTriangle(cx + 0.5f, cy - 6.0f, cx + 5.5f, cy - 6.0f, cx + 3.0f, cy - 2.5f);
        g.fillPath(phMarker);

        // Right-pointing tracking arrow
        juce::Path arrow;
        arrow.startNewSubPath(cx - 7.0f, cy + 0.5f);
        arrow.lineTo(cx - 0.5f, cy + 0.5f);
        arrow.lineTo(cx - 3.5f, cy - 2.5f);
        arrow.startNewSubPath(cx - 0.5f, cy + 0.5f);
        arrow.lineTo(cx - 3.5f, cy + 3.5f);
        g.strokePath(arrow, juce::PathStrokeType(1.6f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));
        break;
    }
    }
}

} // namespace trami

