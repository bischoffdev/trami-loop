#include "ui/StudioOneZoomCorner.h"

#include <cmath>

namespace trami
{

StudioOneZoomCorner::StudioOneZoomCorner()
{
    setOpaque(false);
}

void StudioOneZoomCorner::setZoomLevels(float zoomX, float zoomY)
{
    zoomX_ = std::clamp(zoomX, 0.25f, 4.0f);
    zoomY_ = std::clamp(zoomY, 0.5f, 2.5f);
    repaint();
}

StudioOneZoomCorner::TargetArea StudioOneZoomCorner::getTargetAt(juce::Point<int> pt) const
{
    if (getCornerBounds().contains(pt))
        return TargetArea::Corner;
    if (getHSliderTrackBounds().contains(pt))
        return TargetArea::HSlider;
    if (getVSliderTrackBounds().contains(pt))
        return TargetArea::VSlider;
    return TargetArea::None;
}

juce::Rectangle<int> StudioOneZoomCorner::getHSliderTrackBounds() const
{
    return {0, kSliderLength, kSliderLength, kThickness};
}

juce::Rectangle<int> StudioOneZoomCorner::getVSliderTrackBounds() const
{
    return {kSliderLength, 0, kThickness, kSliderLength};
}

juce::Rectangle<int> StudioOneZoomCorner::getCornerBounds() const
{
    return {kSliderLength, kSliderLength, kThickness, kThickness};
}

juce::Rectangle<int> StudioOneZoomCorner::getHSliderThumbBounds() const
{
    constexpr int thumbW = 16;
    constexpr int thumbH = 13;
    const float t = std::clamp((std::log2(std::clamp(zoomX_, 0.25f, 4.0f)) + 2.0f) / 4.0f, 0.0f, 1.0f);
    const int travel = kSliderLength - thumbW;
    const int thumbX = juce::roundToInt(t * static_cast<float>(travel));
    return {thumbX, kSliderLength + 2, thumbW, thumbH};
}

juce::Rectangle<int> StudioOneZoomCorner::getVSliderThumbBounds() const
{
    constexpr int thumbW = 13;
    constexpr int thumbH = 16;
    const float zy = std::clamp(zoomY_, 0.5f, 2.5f);
    float t = 0.5f;
    if (zy <= 1.0f)
        t = zy - 0.5f;
    else
        t = 0.5f + (zy - 1.0f) / 3.0f;
    t = std::clamp(t, 0.0f, 1.0f);
    const int travel = kSliderLength - thumbH;
    const int thumbY = juce::roundToInt((1.0f - t) * static_cast<float>(travel));
    return {kSliderLength + 2, thumbY, thumbW, thumbH};
}

bool StudioOneZoomCorner::hitTest(int x, int y)
{
    return getTargetAt({x, y}) != TargetArea::None;
}

void StudioOneZoomCorner::paint(juce::Graphics& g)
{
    const juce::Colour trackBg = Palette::surface.darker(0.28f);
    const juce::Colour lineCol = Palette::gridLine.withAlpha(0.55f);
    const juce::Colour tickCol = Palette::textDim.withAlpha(0.45f);

    const auto hTrack = getHSliderTrackBounds();
    const auto vTrack = getVSliderTrackBounds();
    const auto corner = getCornerBounds();

    // 1. Tracks background
    g.setColour(trackBg);
    g.fillRect(hTrack);
    g.fillRect(vTrack);
    g.fillRect(corner);

    // Track outer borders
    g.setColour(lineCol);
    g.drawVerticalLine(kSliderLength, 0.0f, static_cast<float>(kSliderLength));
    g.drawHorizontalLine(kSliderLength, 0.0f, static_cast<float>(kSliderLength));

    // Dividers around corner
    g.drawHorizontalLine(kSliderLength, static_cast<float>(kSliderLength), static_cast<float>(kSliderLength + kThickness));
    g.drawVerticalLine(kSliderLength, static_cast<float>(kSliderLength), static_cast<float>(kSliderLength + kThickness));

    // 2. Track Grooves and Center Ticks (at 100% zoom)
    // Horizontal center groove
    g.setColour(lineCol.withAlpha(0.4f));
    const float hMidY = static_cast<float>(kSliderLength) + static_cast<float>(kThickness) * 0.5f;
    g.drawHorizontalLine(juce::roundToInt(hMidY), 4.0f, static_cast<float>(kSliderLength - 4));

    // Horizontal 100% tick at middle (t = 0.5 -> x = kSliderLength / 2)
    g.setColour(tickCol);
    g.drawVerticalLine(kSliderLength / 2, static_cast<float>(kSliderLength + 3), static_cast<float>(kSliderLength + kThickness - 3));

    // Vertical center groove
    g.setColour(lineCol.withAlpha(0.4f));
    const float vMidX = static_cast<float>(kSliderLength) + static_cast<float>(kThickness) * 0.5f;
    g.drawVerticalLine(juce::roundToInt(vMidX), 4.0f, static_cast<float>(kSliderLength - 4));

    // Vertical 100% tick at middle (t = 0.5 -> y = kSliderLength / 2)
    g.setColour(tickCol);
    g.drawHorizontalLine(kSliderLength / 2, static_cast<float>(kSliderLength + 3), static_cast<float>(kSliderLength + kThickness - 3));

    // 3. Corner Reset Button
    const bool cornerHover = (hovered_ == TargetArea::Corner);
    const bool cornerDown = (dragging_ == TargetArea::Corner);
    if (cornerDown)
    {
        g.setColour(Palette::accent.withAlpha(0.3f));
        g.fillRect(corner);
    }
    else if (cornerHover)
    {
        g.setColour(Palette::surface.brighter(0.12f));
        g.fillRect(corner);
    }

    // Outward 4 chevrons icon ↖ ↗ ↘ ↙
    const float cx = static_cast<float>(corner.getCentreX());
    const float cy = static_cast<float>(corner.getCentreY());
    juce::Colour cornerIconCol = (cornerHover || cornerDown) ? juce::Colours::white : Palette::textDim.brighter(0.2f);
    g.setColour(cornerIconCol);

    constexpr float d = 4.2f;
    constexpr float arm = 2.4f;
    juce::Path p;
    p.startNewSubPath(cx - d + arm, cy - d); p.lineTo(cx - d, cy - d); p.lineTo(cx - d, cy - d + arm);
    p.startNewSubPath(cx + d - arm, cy - d); p.lineTo(cx + d, cy - d); p.lineTo(cx + d, cy - d + arm);
    p.startNewSubPath(cx - d + arm, cy + d); p.lineTo(cx - d, cy + d); p.lineTo(cx - d, cy + d - arm);
    p.startNewSubPath(cx + d - arm, cy + d); p.lineTo(cx + d, cy + d); p.lineTo(cx + d, cy + d - arm);
    g.strokePath(p, juce::PathStrokeType(1.4f, juce::PathStrokeType::mitered, juce::PathStrokeType::square));

    // 4. Horizontal Slider Thumb
    const auto hThumb = getHSliderThumbBounds().toFloat();
    const bool hActive = (dragging_ == TargetArea::HSlider);
    const bool hHover = (hovered_ == TargetArea::HSlider);

    juce::Colour hThumbCol = hActive ? Palette::accent.withAlpha(0.45f)
                                     : (hHover ? Palette::surface.brighter(0.25f) : Palette::surface.brighter(0.14f));
    g.setColour(hThumbCol);
    g.fillRoundedRectangle(hThumb, 3.0f);

    g.setColour(hActive ? Palette::accent : (hHover ? Palette::accent.withAlpha(0.7f) : Palette::surface.brighter(0.32f)));
    g.drawRoundedRectangle(hThumb, 3.0f, 1.0f);

    // Grip lines on H Thumb (vertical 2 notches)
    const float hThumbMidX = hThumb.getCentreX();
    g.setColour(Palette::textDim.withAlpha(0.6f));
    g.drawVerticalLine(juce::roundToInt(hThumbMidX - 2.0f), hThumb.getY() + 3.0f, hThumb.getBottom() - 3.0f);
    g.drawVerticalLine(juce::roundToInt(hThumbMidX + 2.0f), hThumb.getY() + 3.0f, hThumb.getBottom() - 3.0f);

    // 5. Vertical Slider Thumb
    const auto vThumb = getVSliderThumbBounds().toFloat();
    const bool vActive = (dragging_ == TargetArea::VSlider);
    const bool vHover = (hovered_ == TargetArea::VSlider);

    juce::Colour vThumbCol = vActive ? Palette::accent.withAlpha(0.45f)
                                     : (vHover ? Palette::surface.brighter(0.25f) : Palette::surface.brighter(0.14f));
    g.setColour(vThumbCol);
    g.fillRoundedRectangle(vThumb, 3.0f);

    g.setColour(vActive ? Palette::accent : (vHover ? Palette::accent.withAlpha(0.7f) : Palette::surface.brighter(0.32f)));
    g.drawRoundedRectangle(vThumb, 3.0f, 1.0f);

    // Grip lines on V Thumb (horizontal 2 notches)
    const float vThumbMidY = vThumb.getCentreY();
    g.setColour(Palette::textDim.withAlpha(0.6f));
    g.drawHorizontalLine(juce::roundToInt(vThumbMidY - 2.0f), vThumb.getX() + 3.0f, vThumb.getRight() - 3.0f);
    g.drawHorizontalLine(juce::roundToInt(vThumbMidY + 2.0f), vThumb.getX() + 3.0f, vThumb.getRight() - 3.0f);
}

void StudioOneZoomCorner::updateZoomFromMouseX(int mouseX)
{
    constexpr int thumbW = 16;
    const int travel = kSliderLength - thumbW;
    const float t = std::clamp(static_cast<float>(mouseX - thumbW / 2) / static_cast<float>(travel), 0.0f, 1.0f);
    const float newZoomX = std::pow(2.0f, 4.0f * t - 2.0f);
    zoomX_ = newZoomX;
    if (onZoomXChanged)
        onZoomXChanged(newZoomX);
    repaint();
}

void StudioOneZoomCorner::updateZoomFromMouseY(int mouseY)
{
    constexpr int thumbH = 16;
    const int travel = kSliderLength - thumbH;
    const float t = std::clamp(1.0f - static_cast<float>(mouseY - thumbH / 2) / static_cast<float>(travel), 0.0f, 1.0f);
    float newZoomY = 1.0f;
    if (t <= 0.5f)
        newZoomY = 0.5f + t;
    else
        newZoomY = 1.0f + (t - 0.5f) * 3.0f;
    zoomY_ = newZoomY;
    if (onZoomYChanged)
        onZoomYChanged(newZoomY);
    repaint();
}

void StudioOneZoomCorner::mouseMove(const juce::MouseEvent& e)
{
    const auto target = getTargetAt(e.getPosition());
    if (target != hovered_)
    {
        hovered_ = target;
        switch (hovered_)
        {
        case TargetArea::HSlider:
            currentTooltip_ = juce::String::formatted("Horizontal Zoom: %d%% (Drag to zoom, Double-click: 100%%)",
                                                      juce::roundToInt(zoomX_ * 100.0f));
            break;
        case TargetArea::VSlider:
            currentTooltip_ = juce::String::formatted("Track Height: %d%% (Drag to zoom, Double-click: 100%%)",
                                                      juce::roundToInt(zoomY_ * 100.0f));
            break;
        case TargetArea::Corner:
            currentTooltip_ = "Reset Zoom to 100% (Cmd 0)";
            break;
        case TargetArea::None:
            currentTooltip_.clear();
            break;
        }
        repaint();
    }
}

void StudioOneZoomCorner::mouseExit(const juce::MouseEvent&)
{
    if (hovered_ != TargetArea::None)
    {
        hovered_ = TargetArea::None;
        repaint();
    }
}

void StudioOneZoomCorner::mouseDown(const juce::MouseEvent& e)
{
    dragging_ = getTargetAt(e.getPosition());
    if (dragging_ == TargetArea::HSlider)
    {
        updateZoomFromMouseX(e.x);
    }
    else if (dragging_ == TargetArea::VSlider)
    {
        updateZoomFromMouseY(e.y);
    }
    else if (dragging_ == TargetArea::Corner)
    {
        if (onResetZoom)
            onResetZoom();
    }
    repaint();
}

void StudioOneZoomCorner::mouseDrag(const juce::MouseEvent& e)
{
    if (dragging_ == TargetArea::HSlider)
    {
        updateZoomFromMouseX(e.x);
    }
    else if (dragging_ == TargetArea::VSlider)
    {
        updateZoomFromMouseY(e.y);
    }
}

void StudioOneZoomCorner::mouseUp(const juce::MouseEvent&)
{
    dragging_ = TargetArea::None;
    repaint();
}

void StudioOneZoomCorner::mouseDoubleClick(const juce::MouseEvent& e)
{
    const auto target = getTargetAt(e.getPosition());
    if (target == TargetArea::HSlider)
    {
        zoomX_ = 1.0f;
        if (onZoomXChanged)
            onZoomXChanged(1.0f);
        repaint();
    }
    else if (target == TargetArea::VSlider)
    {
        zoomY_ = 1.0f;
        if (onZoomYChanged)
            onZoomYChanged(1.0f);
        repaint();
    }
    else if (target == TargetArea::Corner)
    {
        if (onResetZoom)
            onResetZoom();
    }
}

} // namespace trami
