#pragma once

#include "engine/AudioEngine.h"
#include "engine/StretchCache.h"
#include "io/SampleImporter.h"
#include "model/Song.h"

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

    juce::TextButton previewButton_{"Preview"};
    juce::TextButton importOneShotButton_{"+ One-Shot"};
    juce::TextButton importLoop1Button_{"+ Loop 1 Bar"};
    juce::TextButton importLoop2Button_{"+ Loop 2 Bars"};
    juce::TextButton importLoop4Button_{"+ Loop 4 Bars"};

    juce::Label pathLabel_;
    std::string currentPreviewPath_;

    void timerCallback() override;
};

} // namespace trami
