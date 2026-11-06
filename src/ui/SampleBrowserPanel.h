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
                           public juce::DragAndDropContainer,
                           private juce::Timer
{
public:
    using SampleImportedCallback = std::function<void(SampleId newSampleId)>;

    SampleBrowserPanel(Song& song, AudioEngine& engine, StretchCache& stretchCache,
                       SampleImportedCallback onImported);
    ~SampleBrowserPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

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

    SampleImporter importer_;
    juce::TimeSliceThread formatThread_{"AudioFormatThread"};
    juce::WildcardFileFilter filter_{"*.wav;*.mp3;*.WAV;*.MP3", "*", "Audio Files"};
    std::unique_ptr<juce::DirectoryContentsList> directoryList_;
    std::unique_ptr<juce::FileTreeComponent> fileTree_;

    IconButton previewButton_{"Preview", IconButton::Icon::Play, "Preview"};
    juce::ToggleButton autoPreviewToggle_{"Auto Preview"};

    juce::Label titleLabel_;
    juce::Label pathLabel_;
    juce::Label fileNameLabel_;
    juce::Label fileDetailsLabel_;

    std::shared_ptr<const AudioData> selectedAudio_;
    std::string currentPreviewPath_;
    double previewDuration_ = 0.0;
    double previewProgress_ = 0.0;
    juce::Rectangle<int> waveformRect_;

    void timerCallback() override;
};

} // namespace trami
