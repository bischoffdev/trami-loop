#pragma once

#include "ui/TramiLookAndFeel.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace trami
{

class StudioOneZoomCorner : public juce::Component, public juce::TooltipClient
{
public:
    static constexpr int kThickness = 17;
    static constexpr int kSliderLength = 72;

    StudioOneZoomCorner();
    ~StudioOneZoomCorner() override = default;

    juce::String getTooltip() override { return currentTooltip_; }

    void setZoomLevels(float zoomX, float zoomY);
    float zoomX() const noexcept { return zoomX_; }
    float zoomY() const noexcept { return zoomY_; }

    std::function<void(float newZoomX)> onZoomXChanged;
    std::function<void(float newZoomY)> onZoomYChanged;
    std::function<void()> onResetZoom;

    // Backward compatibility
    std::function<void()> onZoomIn;
    std::function<void()> onZoomOut;
    std::function<void()> onTrackHeightIn;
    std::function<void()> onTrackHeightOut;
    std::function<void(float deltaX, float deltaY)> onZoomDelta;

    bool hitTest(int x, int y) override;
    void paint(juce::Graphics& g) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;

private:
    enum class TargetArea
    {
        None,
        HSlider,
        VSlider,
        Corner
    };

    TargetArea getTargetAt(juce::Point<int> pt) const;
    juce::Rectangle<int> getHSliderTrackBounds() const;
    juce::Rectangle<int> getVSliderTrackBounds() const;
    juce::Rectangle<int> getHSliderThumbBounds() const;
    juce::Rectangle<int> getVSliderThumbBounds() const;
    juce::Rectangle<int> getCornerBounds() const;

    float zoomX_ = 1.0f;
    float zoomY_ = 1.0f;

    TargetArea hovered_ = TargetArea::None;
    TargetArea dragging_ = TargetArea::None;
    juce::String currentTooltip_;

    void updateZoomFromMouseX(int mouseX);
    void updateZoomFromMouseY(int mouseY);
};

} // namespace trami
