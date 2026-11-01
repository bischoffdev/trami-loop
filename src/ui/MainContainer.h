#pragma once

#include "engine/AudioEngine.h"
#include "engine/StretchCache.h"
#include "io/ProjectSerializer.h"
#include "io/SampleImporter.h"
#include "io/WavExporter.h"
#include "model/Song.h"
#include "model/UndoManager.h"
#include "ui/ArrangerView.h"
#include "ui/PatternView.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <filesystem>
#include <optional>

namespace trami
{

enum class ViewMode
{
    Pattern,
    Arranger
};

class MainContainer : public juce::Component
{
public:
    MainContainer(Song& song, AudioEngine& engine, StretchCache& stretchCache);
    ~MainContainer() override;

    void setViewMode(ViewMode mode);
    void editPattern(PatternId patternId);

    void saveProjectDialog();
    void openProjectDialog();
    void showExportDialog();
    void showSettingsDialog();

    void performUndo();
    void performRedo();

    void paint(juce::Graphics& g) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;

private:
    void updateAllViews();

    Song& song_;
    AudioEngine& engine_;
    StretchCache& stretchCache_;
    SongHistory history_;
    SampleImporter importer_;

    std::optional<std::filesystem::path> currentProjectDir_;
    ViewMode currentMode_ = ViewMode::Pattern;

    juce::TextButton patternModeButton_{"Pattern Editor"};
    juce::TextButton arrangerModeButton_{"Arranger / Song"};

    juce::TextButton undoButton_{"Undo"};
    juce::TextButton redoButton_{"Redo"};
    juce::TextButton saveButton_{"Save"};
    juce::TextButton openButton_{"Open"};
    juce::TextButton exportButton_{"Export WAV"};
    juce::TextButton settingsButton_{"Audio"};

    std::unique_ptr<PatternView> patternView_;
    std::unique_ptr<ArrangerView> arrangerView_;

    std::unique_ptr<juce::FileChooser> fileChooser_;
};

} // namespace trami
