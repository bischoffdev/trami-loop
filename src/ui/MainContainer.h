#pragma once

#include "engine/AudioEngine.h"
#include "engine/StretchCache.h"
#include "io/ProjectSerializer.h"
#include "io/SampleImporter.h"
#include "io/WavExporter.h"
#include "model/Song.h"
#include "model/UndoManager.h"
#include "ui/ArrangerView.h"
#include "ui/IconButton.h"
#include "ui/PatternListPanel.h"
#include "ui/PatternView.h"
#include "ui/TransportBar.h"

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

class MainContainer : public juce::Component, public juce::DragAndDropContainer
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
    void pushUndoState();

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

    IconButton patternModeButton_{"Pattern", IconButton::Icon::PatternMode, "Pattern"};
    IconButton arrangerModeButton_{"Arranger", IconButton::Icon::ArrangerMode, "Arranger"};

    IconButton undoButton_{"Undo", IconButton::Icon::Undo, "Undo"};
    IconButton redoButton_{"Redo", IconButton::Icon::Redo, "Redo"};
    IconButton saveButton_{"Save", IconButton::Icon::Save, "Save"};
    IconButton openButton_{"Open", IconButton::Icon::Open, "Open"};
    IconButton exportButton_{"Export", IconButton::Icon::Export, "Export"};
    IconButton settingsButton_{"Audio", IconButton::Icon::Settings, "Audio"};

    std::unique_ptr<PatternListPanel> patternListPanel_;
    std::unique_ptr<PatternView> patternView_;
    std::unique_ptr<ArrangerView> arrangerView_;
    std::unique_ptr<TransportBar> transportBar_;

    std::unique_ptr<juce::FileChooser> fileChooser_;
};

} // namespace trami
