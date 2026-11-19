#pragma once

#include "engine/AudioEngine.h"
#include "engine/StretchCache.h"
#include "io/ProjectSerializer.h"
#include "io/SampleImporter.h"
#include "io/WavExporter.h"
#include "model/Song.h"
#include "model/UndoManager.h"
#include "ui/IconButton.h"
#include "ui/PatternView.h"
#include "ui/ToastOverlay.h"
#include "ui/TransportBar.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <filesystem>
#include <optional>

namespace trami
{

class MainContainer : public juce::Component, public juce::DragAndDropContainer
{
public:
    MainContainer(Song& song, AudioEngine& engine, StretchCache& stretchCache);
    ~MainContainer() override;

    void newProjectDialog();
    void createNewProject();
    void createNewProjectAt(const std::filesystem::path& dir);
    void promptNewProjectLocation();
    void saveProjectDialog(bool promptForNewLocation = false);
    void openProjectDialog();
    void showExportDialog();
    void showSettingsDialog();

    void showToast(const juce::String& message, bool isError = false);

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

    IconButton undoButton_{"Undo", IconButton::Icon::Undo, "Undo"};
    IconButton redoButton_{"Redo", IconButton::Icon::Redo, "Redo"};
    IconButton newButton_{"New", IconButton::Icon::New, "New"};
    IconButton openButton_{"Open", IconButton::Icon::Open, "Open"};
    IconButton saveButton_{"Save", IconButton::Icon::Save, "Save"};
    IconButton exportButton_{"Export", IconButton::Icon::Export, "Export"};
    IconButton settingsButton_{"Audio", IconButton::Icon::Settings, "Settings"};

    std::unique_ptr<PatternView> patternView_;
    std::unique_ptr<TransportBar> transportBar_;

    std::unique_ptr<juce::FileChooser> fileChooser_;
    juce::TooltipWindow tooltipWindow_{this, 400};
    ToastOverlay toastOverlay_;
};

} // namespace trami
