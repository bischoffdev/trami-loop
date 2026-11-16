#pragma once

#include "engine/AudioEngine.h"
#include "engine/SnapshotBuilder.h"
#include "engine/StretchCache.h"
#include "io/SampleImporter.h"
#include "model/Song.h"
#include "ui/IconButton.h"
#include "ui/PatternGrid.h"
#include "ui/SampleBrowserPanel.h"
#include "ui/TrackHeader.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <unordered_set>

namespace trami
{

class PatternView : public juce::Component,
                    public PatternGrid::Listener,
                    public juce::FileDragAndDropTarget,
                    public juce::DragAndDropTarget,
                    private juce::Timer
{
public:
    PatternView(Song& song, AudioEngine& engine, StretchCache& stretchCache);
    ~PatternView() override;

    void setPattern(PatternId patternId);
    void paint(juce::Graphics& g) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;

    // PatternGrid::Listener
    void clipClicked(ClipId clipId) override;
    void clipMoved(ClipId clipId, int newTrack, double newStep) override;
    void clipsMoved(const std::vector<PatternGrid::ClipMoveInfo>& moves) override;
    void clipResized(ClipId clipId, double newStartStep, double newLengthSteps, double newOffsetSteps) override;
    void clipsResized(const std::vector<PatternGrid::ClipResizeInfo>& resizes) override;
    void clipEnvelopeChanged(ClipId clipId, float gain, float fadeInSteps, float fadeOutSteps) override;
    void clipReverseRequested(ClipId clipId) override;
    void clipsReverseRequested(const std::vector<ClipId>& clipIds) override;
    void clipsTransposeDelta(const std::vector<ClipId>& clipIds, int deltaSemitones) override;
    void clipsTransposeSet(const std::vector<ClipId>& clipIds, int semitones) override;
    void clipDeleteRequested(ClipId clipId) override;
    void clipsDeleteRequested(const std::vector<ClipId>& clipIds) override;
    void clipCutRequested(ClipId clipId) override;
    void clipCopyRequested(ClipId clipId) override;
    void clipPasteRequested(int track, int step) override;
    void clipDuplicateRequested(ClipId clipId) override;
    void clipDuplicatedTo(ClipId clipId, int track, double startStep) override;
    void gridCellClicked(int track, int step) override;
    void gridCellDoubleClicked(int track, int step) override;
    void fileDroppedOnGrid(const juce::String& filePath, int track, int step) override;
    void fileDroppedOnTrack(const juce::String& filePath, int track);
    void loopRegionChanged(int startStep, int endStep) override;
    void playheadSeekRequested(int step) override;
    void zoomRequested(float deltaX, float deltaY, juce::Point<int> anchorInGrid) override;
    void zoomToLevelRequested(float targetZoomX, double anchorStep, int anchorViewportX) override;
    void clipsDragging(const std::vector<DragClipOverride>& items) override;
    void clipsDragEnded() override;

    int loopStartStep() const noexcept { return loopStartStep_; }
    int loopEndStep() const noexcept { return loopEndStep_; }
    void setLoopRegion(int startStep, int endStep);
    void setOnLoopRegionChanged(std::function<void(int, int)> cb) { onLoopRegionChanged_ = std::move(cb); }

    // FileDragAndDropTarget
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

    // DragAndDropTarget
    bool isInterestedInDragSource(const SourceDetails& dragSourceDetails) override;
    void itemDropped(const SourceDetails& dragSourceDetails) override;

    using PushUndoCallback = std::function<void()>;
    void setOnPushUndo(PushUndoCallback cb) { onPushUndo_ = std::move(cb); }
    void setOnShowToast(std::function<void(const juce::String&)> cb) { onShowToast_ = std::move(cb); }

    void updateSampleSelector();
    void updateLengthSelector();
    void updateTrackHeaders();
    void republish();
    void publishSnapshotQuick(const std::vector<DragClipOverride>* overrides = nullptr);
    void repaintGrid();
    void syncAllTrackMixes();
    void toggleRecording();
    void onRecordFinished(AudioData recorded);
    void addTrack();
    void duplicateTrack(int track);
    void deleteTrack(int track);
    void moveTrack(int from, int to);
    void duplicateCurrentPattern();
    void deleteCurrentPattern();
    void setTrackKind(int trackIndex, SampleKind newKind);
    void toggleCurrentTrackType();

    void setZoom(float x, float y);
    void applyZoom(float targetZoomX, float targetZoomY, juce::Point<int> anchorInGrid);
    void applyZoomAtStep(float targetZoomX, double anchorStep, int anchorViewportX);
    void zoomIn();
    void zoomOut();
    void resetZoom();
    void trackHeightIn();
    void trackHeightOut();

    struct ClipboardClipItem
    {
        SampleId sampleId;
        int sourceTrackIndex = 0;
        std::string sourceTrackName;
        SampleId sourceTrackSampleId;
        std::optional<SampleKind> sourceTrackKind;
        double lengthSteps = 0.0;
        double offsetSteps = 0.0;
        float gain = 1.0f;
        float fadeInSteps = 0.0f;
        float fadeOutSteps = 0.0f;
        bool reversed = false;
        double originalStartStep = 0.0;
        double span = 0.0;
    };

    struct ClipClipboard
    {
        PatternId sourcePatternId;
        int primaryTrackIndex = 0;
        double minStartStep = 0.0;
        double maxEndStep = 0.0;
        std::vector<ClipboardClipItem> items;
    };

    void copySelectedClip();
    void cutSelectedClip();
    void pasteClip(int targetTrack = -1, int targetStep = -1);
    void duplicateSelectedClip();
    bool hasClipInClipboard() const noexcept { return clipClipboard_.has_value() && !clipClipboard_->items.empty(); }

    int findOrCreateRespectiveTrack(PatternId destPatternId,
                                    int sourceTrackIndex,
                                    const std::string& sourceTrackName,
                                    SampleId sourceTrackSampleId,
                                    SampleId clipSampleId,
                                    std::optional<SampleKind> sourceTrackKind,
                                    std::unordered_set<int>& claimedDestTracks);

private:
    class SyncViewport;
    void timerCallback() override;

    Song& song_;
    AudioEngine& engine_;
    StretchCache& stretchCache_;
    PatternId currentPattern_;
    SampleId activeSample_;
    SampleBank activeBank_;
    SampleImporter importer_;
    PushUndoCallback onPushUndo_;
    std::function<void(int, int)> onLoopRegionChanged_;

    std::optional<ClipClipboard> clipClipboard_;
    int pasteTargetTrack_ = -1;
    int pasteTargetStep_ = 0;

    float zoomX_ = 1.0f;
    float zoomY_ = 1.0f;
    int loopStartStep_ = 0;
    int loopEndStep_ = 0;

    // Toolbar controls
    IconButton browserToggle_{"Browser", IconButton::Icon::Browser, "Browser"};
    juce::Label patternTitle_;
    juce::ComboBox lengthCombo_;

    juce::Label rasterLabel_{"Raster:", "Grid:"};
    juce::ComboBox rasterCombo_;

    IconButton addTrackButton_{"AddTrack", IconButton::Icon::Plus, "+ Track"};

    bool showBrowser_ = true;
    std::function<void(const juce::String&)> onShowToast_;
    std::unique_ptr<SampleBrowserPanel> browserPanel_;
    std::unique_ptr<TrackHeaderList> trackHeaders_;
    std::unique_ptr<juce::Viewport> trackHeadersViewport_;
    std::unique_ptr<PatternGrid> grid_;
    std::unique_ptr<SyncViewport> gridViewport_;

    void handleFileDrop(const juce::String& filePath, int targetTrack, int step, bool hasSpecificStep);
    void importAndAssignToTrack(const juce::String& filePath, int targetTrack, int step, bool hasSpecificStep,
                                std::optional<SampleKind> explicitKind = std::nullopt);
};

} // namespace trami
