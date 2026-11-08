#pragma once

#include "engine/AudioEngine.h"
#include "engine/SnapshotBuilder.h"
#include "engine/StretchCache.h"
#include "model/Song.h"
#include "ui/ArrangerGrid.h"
#include "ui/ArrangerTrackHeader.h"
#include "ui/IconButton.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace trami
{

class ArrangerView : public juce::Component,
                     public ArrangerGrid::Listener,
                     public juce::DragAndDropTarget,
                     private juce::Timer
{
public:
    using EditPatternCallback = std::function<void(PatternId patternId)>;
    using PushUndoCallback = std::function<void()>;

    ArrangerView(Song& song, AudioEngine& engine, StretchCache& stretchCache,
                 EditPatternCallback onEditPattern);
    ~ArrangerView() override;

    void setOnPushUndo(PushUndoCallback cb) { onPushUndo_ = std::move(cb); }
    void updatePatternList();
    void republish();
    void publishSnapshotQuick();
    void syncAllTrackMixes();
    void toggleRecording();
    void onRecordFinished(AudioData recorded);

    void paint(juce::Graphics& g) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;

    // DragAndDropTarget
    bool isInterestedInDragSource(const SourceDetails& dragSourceDetails) override;
    void itemDropped(const SourceDetails& dragSourceDetails) override;

    // ArrangerGrid::Listener
    void blockClicked(BlockId blockId) override;
    void blockDoubleClicked(PatternId patternId) override;
    void blockMoved(BlockId blockId, int newTrack, int newBar) override;
    void blockDeleteRequested(BlockId blockId) override;
    void gridCellClicked(int track, int bar) override;
    void patternDropped(PatternId patternId, int track, int bar) override;
    void loopRegionChanged(int startBar, int endBar) override;
    void zoomRequested(float deltaX, float deltaY, juce::Point<int> anchorInGrid) override;

    void applyZoom(float targetZoomX, float targetZoomY, juce::Point<int> anchorInGrid);
    void zoomIn();
    void zoomOut();
    void resetZoom();
    void trackHeightIn();
    void trackHeightOut();

private:
    class SyncViewport;
    void timerCallback() override;

    Song& song_;
    AudioEngine& engine_;
    StretchCache& stretchCache_;
    EditPatternCallback onEditPattern_;
    PushUndoCallback onPushUndo_;

    PatternId activePattern_;
    SampleBank activeBank_;

    float zoomX_ = 1.0f;
    float zoomY_ = 1.0f;
    int loopStartBar_ = 0;
    int loopEndBar_ = 0;

    // Toolbar controls
    juce::ComboBox patternCombo_;
    IconButton addPatternButton_{"NewPattern", IconButton::Icon::Plus, "+ Pattern"};
    IconButton editPatternButton_{"Edit", IconButton::Icon::Edit, "Edit Pattern"};

    // Zoom controls
    IconButton zoomOutButton_{"ZoomOut", IconButton::Icon::ZoomOut, "Out"};
    IconButton zoomResetButton_{"ZoomReset", IconButton::Icon::ZoomReset, "100%"};
    IconButton zoomInButton_{"ZoomIn", IconButton::Icon::ZoomIn, "In"};
    IconButton vZoomOutButton_{"VZoomOut", IconButton::Icon::ZoomOut, "Tracks-"};
    IconButton vZoomInButton_{"VZoomIn", IconButton::Icon::ZoomIn, "Tracks+"};

    std::unique_ptr<ArrangerTrackHeaderList> trackHeaders_;
    std::unique_ptr<juce::Viewport> trackHeadersViewport_;
    std::unique_ptr<ArrangerGrid> grid_;
    std::unique_ptr<SyncViewport> gridViewport_;
};

} // namespace trami
