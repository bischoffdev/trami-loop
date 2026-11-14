#pragma once

#include "engine/StretchCache.h"
#include "model/Ids.h"
#include "model/Song.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace trami
{

class PatternGrid : public juce::Component,
                    public juce::TooltipClient,
                    public juce::FileDragAndDropTarget,
                    public juce::DragAndDropTarget
{
public:
    juce::String getTooltip() override { return currentTooltip_; }
    struct ClipResizeInfo
    {
        ClipId clipId;
        double newStartStep = 0.0;
        double newLengthSteps = 0.0;
        double newOffsetSteps = 0.0;
    };

    struct ClipMoveInfo
    {
        ClipId clipId;
        int newTrack = 0;
        double newStep = 0.0;
    };

    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void clipClicked(ClipId clipId) = 0;
        virtual void clipMoved(ClipId clipId, int newTrack, double newStep) = 0;
        virtual void clipsMoved(const std::vector<ClipMoveInfo>& moves)
        {
            for (const auto& m : moves)
                clipMoved(m.clipId, m.newTrack, m.newStep);
        }
        virtual void clipResized(ClipId clipId, double newStartStep, double newLengthSteps, double newOffsetSteps) = 0;
        virtual void clipsResized(const std::vector<ClipResizeInfo>& resizes)
        {
            for (const auto& r : resizes)
                clipResized(r.clipId, r.newStartStep, r.newLengthSteps, r.newOffsetSteps);
        }
        virtual void clipEnvelopeChanged(ClipId /*clipId*/, float /*gain*/, float /*fadeInSteps*/, float /*fadeOutSteps*/) {}
        virtual void clipReverseRequested(ClipId /*clipId*/) {}
        virtual void clipsReverseRequested(const std::vector<ClipId>& /*clipIds*/) {}
        virtual void clipDeleteRequested(ClipId clipId) = 0;
        virtual void clipsDeleteRequested(const std::vector<ClipId>& clipIds)
        {
            for (auto cid : clipIds)
                clipDeleteRequested(cid);
        }
        virtual void clipCutRequested(ClipId /*clipId*/) {}
        virtual void clipCopyRequested(ClipId /*clipId*/) {}
        virtual void clipPasteRequested(int /*track*/, int /*step*/) {}
        virtual void clipDuplicateRequested(ClipId /*clipId*/) {}
        virtual void clipDuplicatedTo(ClipId /*clipId*/, int /*track*/, double /*startStep*/) {}
        virtual void gridCellClicked(int track, int step) = 0;
        virtual void gridCellDoubleClicked(int track, int step) = 0;
        virtual void fileDroppedOnGrid(const juce::String& filePath, int track, int step) = 0;
        virtual void loopRegionChanged(int /*startStep*/, int /*endStep*/) {}
        virtual void playheadSeekRequested(int /*step*/) {}
        virtual void zoomRequested(float /*deltaX*/, float /*deltaY*/, juce::Point<int> /*anchorInGrid*/) {}
        virtual void zoomToLevelRequested(float /*targetZoomX*/, double /*anchorStep*/, int /*anchorViewportX*/) {}
        virtual void clipsDragging(const std::vector<DragClipOverride>& /*items*/) {}
        virtual void clipsDragEnded() {}
    };

    PatternGrid(const Song& song, StretchCache& stretchCache, PatternId patternId, Listener& listener);

    void setPattern(PatternId patternId);
    void setSelectedClip(ClipId clipId);
    void setSelectedClips(const std::vector<ClipId>& clipIds);
    bool isClipSelected(ClipId clipId) const noexcept;
    void deselectClip(ClipId clipId);
    void clearSelection();
    void startMarquee(juce::Point<int> pos, juce::ModifierKeys mods);
    void updateMarquee(juce::Point<int> pos);
    void finishMarquee();
    ClipId selectedClip() const noexcept { return selectedClips_.empty() ? ClipId{} : selectedClips_.back(); }
    const std::vector<ClipId>& selectedClips() const noexcept { return selectedClips_; }
    void setCursorPosition(int track, int step);
    void clearCursor();
    int cursorTrack() const noexcept { return cursorTrack_; }
    int cursorStep() const noexcept { return cursorStep_; }
    void setPlayheadStep(double step);
    double playheadStep() const noexcept { return playheadStep_; }
    double clipSpan(const Clip& clip, int trackIndex = -1) const;
    int loopStartStep() const noexcept { return loopStartStep_; }
    int loopEndStep() const noexcept { return loopEndStep_; }
    void setLoopRegion(int startStep, int endStep);

    void setZoom(float zoomX, float zoomY);
    float zoomX() const noexcept { return zoomX_; }
    float zoomY() const noexcept { return zoomY_; }
    int stepWidth() const noexcept { return stepWidth_; }
    int trackHeight() const noexcept { return trackHeight_; }

    int stepToX(double step) const;
    double xToStep(int x) const;

    void setRasterSteps(int steps);
    int rasterSteps() const noexcept { return rasterSteps_; }

    void paint(juce::Graphics& g) override;
    void resized() override;

    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    void mouseMagnify(const juce::MouseEvent& e, float scaleFactor) override;
    bool keyPressed(const juce::KeyPress& key) override;

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
        ResizeRight,
        FadeIn,
        FadeOut,
        Gain
    };

    using DragClipItem = DragClipOverride;

    struct DragState
    {
        std::vector<DragClipItem> items;
        ClipId primaryClipId;
        juce::Point<int> startMousePos;
        DragMode mode = DragMode::None;
        bool active = false;

        const DragClipItem* find(ClipId id) const noexcept
        {
            for (const auto& it : items)
                if (it.clipId == id) return &it;
            return nullptr;
        }

        DragClipItem* find(ClipId id) noexcept
        {
            for (auto& it : items)
                if (it.clipId == id) return &it;
            return nullptr;
        }
    };

    juce::Rectangle<int> clipBounds(int track, double step, double lengthSteps) const;
    std::pair<int, int> pointToTrackAndStep(juce::Point<int> pt) const;
    const Clip* findClipAt(juce::Point<int> pt) const;
    DragMode hitTestResize(juce::Point<int> pt) const;
    juce::Colour sampleColour(SampleId id) const;

    const Song& song_;
    StretchCache& stretchCache_;
    PatternId patternId_;
    Listener& listener_;

    std::vector<ClipId> selectedClips_;
    bool isMarquee_ = false;
    juce::Point<int> marqueeStart_;
    juce::Point<int> marqueeEnd_;
    std::vector<ClipId> preMarqueeSelectedClips_;
    bool primaryClipWasSelectedOnDown_ = false;
    int cursorTrack_ = -1;
    int cursorStep_ = -1;
    double playheadStep_ = -1.0;
    DragState drag_;
    juce::String dragTooltipText_;
    juce::Point<int> dragCurrentMousePos_;
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
        RulerPending,
        SeekPlayhead,
        ZoomRuler,
        SetRegion,
        MoveRegion,
        ResizeStart,
        ResizeEnd
    };

    int loopStartStep_ = 0;
    int loopEndStep_ = 16;
    RulerDragMode rulerDragMode_ = RulerDragMode::None;
    juce::Point<int> rulerDragStartScreenPos_;
    float rulerDragStartZoomX_ = 1.0f;
    double rulerDragAnchorStep_ = 0.0;
    int rulerDragAnchorViewportX_ = 0;
    int rulerDragClickedStep_ = 0;
    int rulerDragRegionAnchorStep_ = 0;
    int rulerDragOriginalStart_ = 0;
    int rulerDragOriginalEnd_ = 16;
    juce::String currentTooltip_;
};

} // namespace trami
