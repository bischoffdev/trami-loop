#pragma once

#include "engine/AudioEngine.h"
#include "engine/StretchCache.h"
#include "io/SampleImporter.h"
#include "model/Song.h"
#include "ui/IconButton.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace trami
{

class SampleBrowserPanel : public juce::Component,
                           public juce::FileBrowserListener,
                           private juce::Timer
{
public:
    using SampleImportedCallback = std::function<void(SampleId newSampleId)>;
    using AddToNewTrackCallback = std::function<void(const juce::String& filePath, SampleKind kind)>;

    SampleBrowserPanel(Song& song, AudioEngine& engine, StretchCache& stretchCache,
                       SampleImportedCallback onImported);
    ~SampleBrowserPanel() override;

    void setOnAddToNewTrack(AddToNewTrackCallback cb) { onAddToNewTrack_ = std::move(cb); }

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;

    // FileBrowserListener
    void selectionChanged() override;
    void fileClicked(const juce::File& file, const juce::MouseEvent& e) override;
    void fileDoubleClicked(const juce::File& file) override;
    void browserRootChanged(const juce::File& newRoot) override;

    void previewCurrentSelection();
    void importCurrentSelection(SampleKind kind, int loopBars = 1);

private:
    Song& song_;
    AudioEngine& engine_;
    StretchCache& stretchCache_;
    SampleImportedCallback onImported_;
    AddToNewTrackCallback onAddToNewTrack_;

    SampleImporter importer_;
    juce::TimeSliceThread formatThread_{"AudioFormatThread"};
    juce::WildcardFileFilter filter_{"*.wav;*.mp3;*.WAV;*.MP3", "*", "Audio Files"};
    std::unique_ptr<juce::DirectoryContentsList> directoryList_;
    std::unique_ptr<juce::FileTreeComponent> fileTree_;

    IconButton previewButton_{"Preview", IconButton::Icon::Play, "Preview"};
    juce::ToggleButton autoPreviewToggle_{"Auto Preview"};
    IconButton addLoopButton_{"+ Loop", IconButton::Icon::Duplicate, "Add as Loop to New Track"};
    IconButton addOneShotButton_{"+ 1-Shot", IconButton::Icon::Duplicate, "Add as One-Shot to New Track"};

    juce::Label titleLabel_;
    juce::Label pathLabel_;
    juce::Label fileNameLabel_;
    juce::Label fileDetailsLabel_;

    std::shared_ptr<const AudioData> selectedAudio_;
    std::string currentPreviewPath_;
    double previewDuration_ = 0.0;
    double previewProgress_ = 0.0;
    juce::Rectangle<int> waveformRect_;

    void startSampleDrag(const juce::File& file);
    void timerCallback() override;
};

} // namespace trami
