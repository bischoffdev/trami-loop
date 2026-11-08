#pragma once

#include "engine/StretchCache.h"
#include "model/Ids.h"
#include "model/Song.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace trami
{

class PatternGrid : public juce::Component,
                    public juce::FileDragAndDropTarget,
                    public juce::DragAndDropTarget
{
public:
    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void clipClicked(ClipId clipId) = 0;
        virtual void clipMoved(ClipId clipId, int newTrack, int newStep) = 0;
        virtual void clipResized(ClipId clipId, int newStartStep, int newLengthSteps, int newOffsetSteps) = 0;
        virtual void clipDeleteRequested(ClipId clipId) = 0;
        virtual void gridCellClicked(int track, int step) = 0;
        virtual void fileDroppedOnGrid(const juce::String& filePath, int track, int step) = 0;
        virtual void loopRegionChanged(int /*startStep*/, int /*endStep*/) {}
        virtual void zoomRequested(float /*deltaX*/, float /*deltaY*/, juce::Point<int> /*anchorInGrid*/) {}
    };

    PatternGrid(const Song& song, StretchCache& stretchCache, PatternId patternId, Listener& listener);

    void setPattern(PatternId patternId);
    void setSelectedClip(ClipId clipId);
    void setPlayheadStep(double step);
    int loopStartStep() const noexcept { return loopStartStep_; }
    int loopEndStep() const noexcept { return loopEndStep_; }
    void setLoopRegion(int startStep, int endStep);

    void setZoom(float zoomX, float zoomY);
    float zoomX() const noexcept { return zoomX_; }
    float zoomY() const noexcept { return zoomY_; }
    int stepWidth() const noexcept { return stepWidth_; }
    int trackHeight() const noexcept { return trackHeight_; }

    void setRasterSteps(int steps);
    int rasterSteps() const noexcept { return rasterSteps_; }

    void paint(juce::Graphics& g) override;
    void resized() override;

    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    void mouseMagnify(const juce::MouseEvent& e, float scaleFactor) override;

    // FileDragAndDropTarget
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void fileDragEnter(const juce::StringArray& files, int x, int y) override;
    void fileDragMove(const juce::StringArray& files, int x, int y) override;
    void fileDragExit(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

    // DragAndDropTarget
    bool isInterestedInDragSource(const SourceDetails& dragSourceDetails) override;
    void itemDragEnter(const SourceDetails& dragSourceDetails) override;
    void itemDragMove(const SourceDetails& dragSourceDetails) override;
    void itemDragExit(const SourceDetails& dragSourceDetails) override;
    void itemDropped(const SourceDetails& dragSourceDetails) override;

    static constexpr int kRulerHeight = 28;
    static constexpr int kDefaultTrackHeight = 40;
    static constexpr int kDefaultStepWidth = 16;
    static constexpr int kTrackHeight = kDefaultTrackHeight;
    static constexpr int kStepWidth = kDefaultStepWidth;

private:
    enum class DragMode
    {
        None,
        Move,
        ResizeLeft,
        ResizeRight
    };

    struct DragState
    {
        ClipId clipId;
        int originalTrack = 0;
        int originalStartStep = 0;
        int originalLengthSteps = 0;
        int originalOffsetSteps = 0;
        int currentTrack = 0;
        int currentStartStep = 0;
        int currentLengthSteps = 0;
        int currentOffsetSteps = 0;
        DragMode mode = DragMode::None;
        bool active = false;
    };

    juce::Rectangle<int> clipBounds(int track, int step, int lengthSteps) const;
    std::pair<int, int> pointToTrackAndStep(juce::Point<int> pt) const;
    const Clip* findClipAt(juce::Point<int> pt) const;
    DragMode hitTestResize(juce::Point<int> pt) const;
    int clipSpan(const Clip& clip, int trackIndex = -1) const;
    juce::Colour sampleColour(SampleId id) const;

    const Song& song_;
    StretchCache& stretchCache_;
    PatternId patternId_;
    Listener& listener_;

    ClipId selectedClip_;
    double playheadStep_ = -1.0;
    DragState drag_;
    int dragHoverTrack_ = -1;
    int dragHoverStep_ = -1;

    float zoomX_ = 1.0f;
    float zoomY_ = 1.0f;
    int stepWidth_ = kDefaultStepWidth;
    int trackHeight_ = kDefaultTrackHeight;
    int rasterSteps_ = 2; // Default 1/16 (2 steps in 1/32 grid)

    enum class RulerDragMode
    {
        None,
        SetRegion,
        MoveRegion,
        ResizeStart,
        ResizeEnd
    };

    int loopStartStep_ = 0;
    int loopEndStep_ = 16;
    RulerDragMode rulerDragMode_ = RulerDragMode::None;
    int rulerDragAnchorStep_ = 0;
    int rulerDragOriginalStart_ = 0;
    int rulerDragOriginalEnd_ = 16;
};

} // namespace trami
