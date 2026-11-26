#pragma once

#include "engine/AudioEngine.h"
#include "engine/StretchCache.h"
#include "model/Ids.h"
#include "model/Song.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace trami
{

/// Docked Wave Editor tab supporting both Track Sample editing (affects all clips using that sample)
/// and non-destructive Selected Clip editing, audition playback with its own playhead, and Stretch markers.
class WaveEditorPanel : public juce::Component, private juce::Timer
{
public:
    enum class Scope
    {
        TrackSample = 0,
        SelectedClip = 1
    };

    WaveEditorPanel(Song& song, AudioEngine& engine, StretchCache& stretchCache, PatternId patternId,
                    std::function<void()> onChanged);
    ~WaveEditorPanel() override;

    void setPattern(PatternId patternId);
    void setTrack(PatternId patternId, int trackIndex);
    void setSelectedClip(ClipId clipId, int trackIndex);
    void setScope(Scope scope);
    void refreshFromModel();
    void togglePreview();
    void stopPreview();

    int currentTrackIndex() const noexcept { return trackIndex_; }
    Scope scope() const noexcept { return scope_; }
    juce::String currentSummaryText() const;

    std::function<void()> onPushUndo;
    std::function<void(const juce::String&)> onShowToast;
    std::function<void(int)> onTrackCreated;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void focusGained(FocusChangeType cause) override;
    void focusLost(FocusChangeType cause) override;
    bool keyPressed(const juce::KeyPress& key) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    void mouseMagnify(const juce::MouseEvent& e, float scaleFactor) override;

private:
    class IconButton;
    void timerCallback() override;
    void startPreviewFrom(double startPosNorm);
    bool hasValidTrack() const;
    const Clip* findSelectedClip() const;
    void rebuildTrackSelector();
    void syncControlsFromModel();

    void setZoom(double newZoom, double anchorNorm = -1.0);
    double normToX(double norm) const;
    double xToNorm(int x) const;
    juce::Rectangle<int> scrollThumbRect() const;
    int hitTestMarker(juce::Point<int> pt) const;
    int hitTestSelectionEdge(juce::Point<int> pt) const;
    bool isInsideSelection(juce::Point<int> pt) const;
    bool hasSelection() const noexcept { return selEndNorm_ > selStartNorm_ + 0.002; }

    // Track Sample editing actions (applies to sample audio across all clips on track)
    void applySampleTrim();
    void applySampleDelete();
    void applySampleSilence();
    void applySampleNormalize();
    void applySampleGain();
    void applySampleGainDb(double db);
    void applySampleFade(bool fadeIn);
    void extractSelectionToNewTrack();

    // Selected Clip non-destructive editing actions
    void applyClipTrim();
    void makeSelectedClipUnique();

    Song& song_;
    AudioEngine& engine_;
    StretchCache& stretchCache_;
    PatternId patternId_;
    int trackIndex_ = 0;
    SampleId sampleId_;
    ClipId selectedClipId_;
    Scope scope_ = Scope::TrackSample;
    std::function<void()> onChanged_;
    bool isUpdating_ = false;

    // Top bar: Play + Loop (symbol buttons) + Scope + Track selector + Edit tools
    std::unique_ptr<IconButton> playBtn_;
    std::unique_ptr<IconButton> loopBtn_;
    juce::TextButton trackScopeBtn_{"Track Sample"};
    juce::TextButton clipScopeBtn_{"Selected Clip"};
    juce::ComboBox trackSelector_;

    // Track-sample edit controls
    juce::TextButton trimBtn_{"Trim"};
    juce::TextButton cutBtn_{"Delete"};
    juce::TextButton silenceBtn_{"Silence"};
    juce::TextButton normalizeBtn_{"Normalize"};
    juce::TextButton fadeInBtn_{"Fade In"};
    juce::TextButton fadeOutBtn_{"Fade Out"};
    juce::Label gainLabel_{"GainLbl", "Gain:"};
    juce::Slider gainSlider_{juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight};
    juce::TextButton applyGainBtn_{"Apply"};

    // Clip-specific edit controls
    juce::TextButton clipTrimBtn_{"Trim Clip"};
    juce::TextButton clipReverseBtn_{"Reverse"};
    juce::TextButton makeUniqueBtn_{"Make Unique"};
    juce::Label clipGainLabel_{"ClipGainLbl", "Clip Gain:"};
    juce::Slider clipGainSlider_{juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight};

    // Bottom bar: Stretch markers group + Extract + Zoom controls
    juce::Label stretchGroupLabel_{"StretchLbl", "Stretch:"};
    juce::TextButton autoDetectBtn_{"Auto-Detect"};
    juce::TextButton beatGridBtn_{"Beat Grid"};
    juce::TextButton clearMarkersBtn_{"Clear"};
    juce::TextButton toNewTrackBtn_{"Create track from selection"};
    juce::Rectangle<int> stretchGroupRect_;
    juce::TextButton zoomOutBtn_{"-"};
    juce::Label zoomLabel_;
    juce::TextButton zoomInBtn_{"+"};
    juce::TextButton zoomResetBtn_{"1:1"};

    // Interactive waveform geometry & state
    juce::Rectangle<int> waveformRect_;
    juce::Rectangle<int> scrollRect_;
    double zoom_ = 1.0;
    double scrollNorm_ = 0.0;

    // Selection range [0..1]
    double selStartNorm_ = 0.0;
    double selEndNorm_ = 0.0;
    bool draggingSelection_ = false;
    double selAnchorNorm_ = 0.0;
    int draggingSelEdge_ = -1; // -1 = none, 0 = left border, 1 = right border
    int hoveredSelEdge_ = -1;
    bool hoveredInsideSel_ = false;
    bool draggingSelGain_ = false;
    int selGainDragStartY_ = 0;
    double selGainDragDb_ = 0.0;

    // Individual preview playhead state
    bool isPreviewing_ = false;
    bool loopEnabled_ = false;
    bool draggingPlayhead_ = false;
    double playheadNorm_ = 0.0;
    double previewStartNorm_ = 0.0;
    double previewEndNorm_ = 1.0;
    double previewPlayheadNorm_ = 0.0;

    // Stretch marker interaction
    int draggingMarkerIdx_ = -1;
    bool pushedUndoForMarkerDrag_ = false;
    int hoveredMarkerIdx_ = -1;

    // Scrollbar drag
    bool draggingScroll_ = false;
    int scrollDragStartX_ = 0;
    double scrollDragStartNorm_ = 0.0;
};

} // namespace trami
