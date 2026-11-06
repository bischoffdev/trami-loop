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
        virtual void zoomRequested(float /*deltaX*/, float /*deltaY*/, juce::Point<int> /*anchorInGrid*/) {}
    };

    PatternGrid(const Song& song, StretchCache& stretchCache, PatternId patternId, Listener& listener);

    void setPattern(PatternId patternId);
    void setSelectedClip(ClipId clipId);
    void setPlayheadStep(double step);

    void setZoom(float zoomX, float zoomY);
    float zoomX() const noexcept { return zoomX_; }
    float zoomY() const noexcept { return zoomY_; }
    int stepWidth() const noexcept { return stepWidth_; }
    int trackHeight() const noexcept { return trackHeight_; }

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
    void filesDropped(const juce::StringArray& files, int x, int y) override;

    // DragAndDropTarget
    bool isInterestedInDragSource(const SourceDetails& dragSourceDetails) override;
    void itemDropped(const SourceDetails& dragSourceDetails) override;

    static constexpr int kRulerHeight = 28;
    static constexpr int kDefaultTrackHeight = 40;
    static constexpr int kDefaultStepWidth = 28;
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
    int clipSpan(const Clip& clip) const;
    juce::Colour sampleColour(SampleId id) const;

    const Song& song_;
    StretchCache& stretchCache_;
    PatternId patternId_;
    Listener& listener_;

    ClipId selectedClip_;
    double playheadStep_ = -1.0;
    DragState drag_;

    float zoomX_ = 1.0f;
    float zoomY_ = 1.0f;
    int stepWidth_ = kDefaultStepWidth;
    int trackHeight_ = kDefaultTrackHeight;
};

} // namespace trami
