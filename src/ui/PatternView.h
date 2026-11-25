#pragma once

#include "engine/AudioEngine.h"
#include "engine/SnapshotBuilder.h"
#include "engine/StretchCache.h"
#include "io/SampleImporter.h"
#include "model/Song.h"
#include "ui/BottomDockPanel.h"
#include "ui/IconButton.h"
#include "ui/PatternGrid.h"
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

    void setPattern(PatternId patternId, bool forceReload = false);
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
    void clipVolumeEnvelopeChanged(ClipId clipId, std::vector<EnvelopePoint> points) override;
    void clipPanEnvelopeChanged(ClipId clipId, std::vector<EnvelopePoint> points) override;
    void trackAutomationChanged(int track, TrackEnvelopeType type, std::vector<EnvelopePoint> points) override;
    void clipSplitRequested(const std::vector<ClipId>& clipIds, double splitStep) override;
    void clipsJoinRequested(const std::vector<ClipId>& clipIds) override;
    void clipPainted(int track, double startStep, double lengthSteps) override;
    void markerAddRequested(double step) override;
    void markerMoved(int markerId, double newStep) override;
    void markerRenameRequested(int markerId) override;
    void markerDeleteRequested(int markerId) override;
    void tabLoopToMarkerSection(int direction) override;
    void clipReverseRequested(ClipId clipId) override;
    void clipsReverseRequested(const std::vector<ClipId>& clipIds) override;
    void envelopeLockToggleRequested() override;
    void clipFxRequested(ClipId clipId) override;
    void clipFxBypassRequested(ClipId clipId) override;
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
    void patternLengthExtensionRequested(int minBars) override;

    int loopStartStep() const noexcept { return loopStartStep_; }
    int loopEndStep() const noexcept { return loopEndStep_; }

    bool isFollowPlayhead() const noexcept { return followPlayhead_; }
    void setFollowPlayhead(bool enabled);
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
    void setOnSongRenamed(std::function<void(const juce::String&)> cb) { onSongRenamed_ = std::move(cb); }
    void setOnOpenAudioSettings(std::function<void()> cb) { onOpenAudioSettings_ = std::move(cb); }

    void updateSampleSelector();
    void autoAdjustPatternLength();
    void updateLengthSelector();
    void updateTrackHeaders();
    void syncToolbarTogglesFromSong();
    void openTrackFxDialog(int track);
    void toggleTrackFxBypass(int track);
    void jumpToClip(ClipId clipId);
    void jumpToTrack(int track);
    void openMasterFxDialog();
    void toggleMasterFxBypass();
    void republish();
    void publishSnapshotQuick(const std::vector<DragClipOverride>* overrides = nullptr);
    void repaintGrid();
    void onTimeSignatureChanged();
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
    void fitZoomToProject(int minBarsVisible = 8, int maxBarsVisible = 32);
    void resetPlayhead();
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
        int transposeSemitones = 0;
        std::vector<EnvelopePoint> volumeEnvelope;
        std::vector<EnvelopePoint> panEnvelope;
        std::vector<PluginSlot> fxChain;
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
    class HeaderCorner;
    class PatternRuler;
    class MasterBusLane;
    class ResizeSplitter;
    void timerCallback() override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

    Song& song_;
    AudioEngine& engine_;
    StretchCache& stretchCache_;
    PatternId currentPattern_;
    SampleId activeSample_;
    SampleBank activeBank_;
    std::uint64_t republishGeneration_ = 0;
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
    IconButton browserToggle_{"Dock", IconButton::Icon::Browser, "Dock"};
    juce::Label patternTitle_;

    juce::Label rasterLabel_{"Raster:", "Grid:"};
    juce::ComboBox rasterCombo_;

    IconButton addTrackButton_{"AddTrack", IconButton::Icon::Plus, "+ Track"};
    IconButton followToggle_{"Follow", IconButton::Icon::Follow, "Follow"};

    // Edit Tool & Action buttons
    IconButton selectToolButton_{"Select", IconButton::Icon::SelectTool, "Select"};
    IconButton paintToolButton_{"Paint", IconButton::Icon::PaintTool, "Paint"};
    IconButton eraseToolButton_{"Erase", IconButton::Icon::EraseTool, "Erase"};
    IconButton splitButton_{"Split", IconButton::Icon::SplitTool, "Split"};
    IconButton joinButton_{"Join", IconButton::Icon::JoinClips, "Join"};
    IconButton markerButton_{"Marker", IconButton::Icon::Marker, "Marker"};

    // Edit Mode Toggles
    IconButton autoCrossfadeToggle_{"X-Fade", IconButton::Icon::Crossfade, "X-Fade"};
    IconButton envLockToggle_{"Env Lock", IconButton::Icon::EnvelopeLock, "Env Lock"};
    IconButton rippleEditToggle_{"Ripple", IconButton::Icon::RippleEdit, "Ripple"};

    double recordStartStep_ = 0.0;
    int recordTargetTrack_ = -1;
    void setActiveTool(PatternGrid::EditTool tool);

    bool followPlayhead_ = true;
    bool showBrowser_ = true;
    int trackHeaderWidth_ = TrackHeaderList::kHeaderWidth;
    int browserHeight_ = 250;
    int lastGridViewportW_ = 0;
    bool needsInitialAutoFit_ = true;
    bool isUpdatingZoom_ = false;
    std::function<void(const juce::String&)> onShowToast_;
    std::function<void(const juce::String&)> onSongRenamed_;
    std::function<void()> onOpenAudioSettings_;
    std::unique_ptr<BottomDockPanel> dockPanel_;
    std::unique_ptr<ResizeSplitter> headerSplitter_;
    std::unique_ptr<ResizeSplitter> browserSplitter_;
    std::unique_ptr<HeaderCorner> headerCorner_;
    std::unique_ptr<PatternRuler> ruler_;
    std::unique_ptr<MasterBusLane> masterBusLane_;
    std::unique_ptr<TrackHeaderList> trackHeaders_;
    std::unique_ptr<juce::Viewport> trackHeadersViewport_;
    std::unique_ptr<PatternGrid> grid_;
    std::unique_ptr<SyncViewport> gridViewport_;

    void ensureDockVisible();
    void scrollToPlayheadIfNeeded(double phStep);
    void handleFileDrop(const juce::String& filePath, int targetTrack, int step, bool hasSpecificStep);
    void importAndAssignToTrack(const juce::String& filePath, int targetTrack, int step, bool hasSpecificStep,
                                std::optional<SampleKind> explicitKind = std::nullopt);
};

} // namespace trami
