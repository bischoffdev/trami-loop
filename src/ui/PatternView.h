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

namespace trami
{

class PatternView : public juce::Component, public PatternGrid::Listener, private juce::Timer
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
    void clipMoved(ClipId clipId, int newTrack, int newStep) override;
    void clipResized(ClipId clipId, int newStartStep, int newLengthSteps, int newOffsetSteps) override;
    void clipDeleteRequested(ClipId clipId) override;
    void gridCellClicked(int track, int step) override;
    void fileDroppedOnGrid(const juce::String& filePath, int track, int step) override;
    void fileDroppedOnTrack(const juce::String& filePath, int track);
    void zoomRequested(float deltaX, float deltaY, juce::Point<int> anchorInGrid) override;

    void updateSampleSelector();
    void updateLengthSelector();
    void republish();
    void publishSnapshotQuick();
    void syncAllTrackMixes();
    void toggleRecording();
    void onRecordFinished(AudioData recorded);

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
    PatternId currentPattern_;
    SampleId activeSample_;
    SampleBank activeBank_;
    SampleImporter importer_;

    float zoomX_ = 1.0f;
    float zoomY_ = 1.0f;

    // Toolbar controls
    IconButton browserToggle_{"Browser", IconButton::Icon::Browser, "Browser"};
    juce::Label patternTitle_;
    juce::ComboBox lengthCombo_;
    juce::ComboBox sampleCombo_;
    IconButton rewindButton_{"Rewind", IconButton::Icon::Rewind, "Rewind"};
    IconButton playButton_{"Play", IconButton::Icon::Play, "Play"};
    IconButton recordButton_{"Rec", IconButton::Icon::Record, "Rec"};
    IconButton metroToggle_{"Click", IconButton::Icon::Metronome, "Click"};
    IconButton swingToggle_{"Swing", IconButton::Icon::Swing, "Swing"};
    juce::Label tempoLabel_;
    juce::TextButton tempoDown_{"-"};
    juce::TextButton tempoUp_{"+"};

    // Zoom controls
    IconButton zoomOutButton_{"ZoomOut", IconButton::Icon::ZoomOut, "Out"};
    IconButton zoomResetButton_{"ZoomReset", IconButton::Icon::ZoomReset, "100%"};
    IconButton zoomInButton_{"ZoomIn", IconButton::Icon::ZoomIn, "In"};
    IconButton vZoomOutButton_{"VZoomOut", IconButton::Icon::ZoomOut, "Tracks-"};
    IconButton vZoomInButton_{"VZoomIn", IconButton::Icon::ZoomIn, "Tracks+"};

    bool showBrowser_ = true;
    std::unique_ptr<SampleBrowserPanel> browserPanel_;
    std::unique_ptr<TrackHeaderList> trackHeaders_;
    std::unique_ptr<juce::Viewport> trackHeadersViewport_;
    std::unique_ptr<PatternGrid> grid_;
    std::unique_ptr<SyncViewport> gridViewport_;

    void handleFileDrop(const juce::String& filePath, int targetTrack, int step, bool hasSpecificStep);
    void importAndAssignToTrack(const juce::String& filePath, int targetTrack, int step, bool hasSpecificStep);
    void replaceSoundOnTrack(const juce::String& filePath, int targetTrack, int step, bool hasSpecificStep);
};

} // namespace trami
