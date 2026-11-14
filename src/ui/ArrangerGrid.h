#pragma once

#include "model/Ids.h"
#include "model/Song.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace trami
{

class ArrangerGrid : public juce::Component, public juce::DragAndDropTarget
{
public:
    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void blockClicked(BlockId blockId) = 0;
        virtual void blockDoubleClicked(PatternId patternId) = 0;
        virtual void blockMoved(BlockId blockId, int newTrack, int newBar) = 0;
        virtual void blockDeleteRequested(BlockId blockId) = 0;
        virtual void gridCellClicked(int track, int bar) = 0;
        virtual void patternDropped(PatternId /*patternId*/, int /*track*/, int /*bar*/) {}
        virtual void loopRegionChanged(int /*startBar*/, int /*endBar*/) {}
        virtual void playheadSeekRequested(double /*bar*/) {}
        virtual void zoomRequested(float /*deltaX*/, float /*deltaY*/, juce::Point<int> /*anchorInGrid*/) {}
        virtual void zoomToLevelRequested(float /*targetZoomX*/, double /*anchorBar*/, int /*anchorViewportX*/) {}
    };

    ArrangerGrid(const Song& song, Listener& listener);

    void setSelectedBlock(BlockId blockId);
    BlockId selectedBlock() const noexcept { return selectedBlock_; }
    void setPlayheadBar(double bar);
    int loopStartBar() const noexcept { return loopStartBar_; }
    int loopEndBar() const noexcept { return loopEndBar_; }
    void setLoopRegion(int startBar, int endBar);

    void setZoom(float zoomX, float zoomY);
    float zoomX() const noexcept { return zoomX_; }
    float zoomY() const noexcept { return zoomY_; }
    int barWidth() const noexcept { return barWidth_; }
    int trackHeight() const noexcept { return trackHeight_; }

    void paint(juce::Graphics& g) override;
    void resized() override;

    void mouseMove(const juce::MouseEvent& e) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    void mouseMagnify(const juce::MouseEvent& e, float scaleFactor) override;
    bool keyPressed(const juce::KeyPress& key) override;

    // DragAndDropTarget
    bool isInterestedInDragSource(const SourceDetails& dragSourceDetails) override;
    void itemDragEnter(const SourceDetails& dragSourceDetails) override;
    void itemDragMove(const SourceDetails& dragSourceDetails) override;
    void itemDragExit(const SourceDetails& dragSourceDetails) override;
    void itemDropped(const SourceDetails& dragSourceDetails) override;

    static constexpr int kRulerHeight = 28;
    static constexpr int kDefaultTrackHeight = 44;
    static constexpr int kDefaultBarWidth = 64;
    static constexpr int kTrackHeight = kDefaultTrackHeight;
    static constexpr int kBarWidth = kDefaultBarWidth;

    std::pair<int, int> pointToTrackAndBar(juce::Point<int> pt) const;

    static juce::Colour patternColour(PatternId id);

private:
    struct DragState
    {
        BlockId blockId;
        int originalTrack = 0;
        int originalBar = 0;
        int currentTrack = 0;
        int currentBar = 0;
        juce::Point<int> startMousePos;
        bool active = false;
    };

    juce::Rectangle<int> blockBounds(int track, int bar, int lengthBars) const;
    const PatternBlock* findBlockAt(juce::Point<int> pt) const;

    const Song& song_;
    Listener& listener_;

    BlockId selectedBlock_;
    double playheadBar_ = -1.0;
    DragState drag_;
    int dragHoverTrack_ = -1;
    int dragHoverBar_ = -1;
    PatternId dragHoverPatternId_;

    float zoomX_ = 1.0f;
    float zoomY_ = 1.0f;
    int barWidth_ = kDefaultBarWidth;
    int trackHeight_ = kDefaultTrackHeight;

    enum class RulerDragMode
    {
        None,
        RulerPending,
        SeekPlayhead,
        ZoomRuler,
        SetRegion,
        MoveRegion,
        ResizeStart,
        ResizeEnd
    };

    int loopStartBar_ = 0;
    int loopEndBar_ = 4;
    RulerDragMode rulerDragMode_ = RulerDragMode::None;
    juce::Point<int> rulerDragStartScreenPos_;
    float rulerDragStartZoomX_ = 1.0f;
    double rulerDragAnchorBar_ = 0.0;
    int rulerDragAnchorViewportX_ = 0;
    double rulerDragClickedBar_ = 0.0;
    int rulerDragRegionAnchorBar_ = 0;
    int rulerDragOriginalStart_ = 0;
    int rulerDragOriginalEnd_ = 4;
};

} // namespace trami
