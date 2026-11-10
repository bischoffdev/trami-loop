#include "ui/MainContainer.h"

#include "ui/TramiLookAndFeel.h"

#include <juce_audio_utils/juce_audio_utils.h>

namespace trami
{

MainContainer::MainContainer(Song& song, AudioEngine& engine, StretchCache& stretchCache)
    : song_(song), engine_(engine), stretchCache_(stretchCache)
{
    setWantsKeyboardFocus(true);

    patternModeButton_.setActiveColour(Palette::patternAccent);
    patternModeButton_.setTooltip("Switch to Pattern Editor (F1)");
    patternModeButton_.onClick = [this]() { setViewMode(ViewMode::Pattern); };
    addAndMakeVisible(patternModeButton_);

    arrangerModeButton_.setActiveColour(Palette::arrangerAccent);
    arrangerModeButton_.setTooltip("Switch to Arranger Timeline (F2)");
    arrangerModeButton_.onClick = [this]() { setViewMode(ViewMode::Arranger); };
    addAndMakeVisible(arrangerModeButton_);

    undoButton_.setTooltip("Undo last change (Cmd+Z)");
    undoButton_.onClick = [this]() { performUndo(); };
    addAndMakeVisible(undoButton_);

    redoButton_.setTooltip("Redo last undone change (Cmd+Shift+Z)");
    redoButton_.onClick = [this]() { performRedo(); };
    addAndMakeVisible(redoButton_);

    saveButton_.setTooltip("Save Project (Cmd+S)");
    saveButton_.onClick = [this]() { saveProjectDialog(); };
    addAndMakeVisible(saveButton_);

    openButton_.setTooltip("Open Project (Cmd+O)");
    openButton_.onClick = [this]() { openProjectDialog(); };
    addAndMakeVisible(openButton_);

    exportButton_.setTooltip("Export Mix to WAV (Cmd+E)");
    exportButton_.onClick = [this]() { showExportDialog(); };
    addAndMakeVisible(exportButton_);

    settingsButton_.setTooltip("Audio Device & MIDI Settings");
    settingsButton_.onClick = [this]() { showSettingsDialog(); };
    addAndMakeVisible(settingsButton_);

    PatternId initialPattern = song_.patterns().empty() ? PatternId{1} : song_.patterns().front().id;

    // Pattern List Panel on the left (persistent in both views)
    patternListPanel_ = std::make_unique<PatternListPanel>(
        song_, initialPattern,
        [this](PatternId patId) {
            if (patternView_ != nullptr)
                patternView_->setPattern(patId);
        },
        [this]() {
            pushUndoState();
            if (patternView_ != nullptr)
                patternView_->updateTrackHeaders();
            if (arrangerView_ != nullptr)
                arrangerView_->updatePatternList();
        });
    addAndMakeVisible(patternListPanel_.get());

    // Pattern View
    patternView_ = std::make_unique<PatternView>(song_, engine_, stretchCache_);
    patternView_->setOnPushUndo([this]() { pushUndoState(); });
    addAndMakeVisible(patternView_.get());

    // Arranger View
    arrangerView_ = std::make_unique<ArrangerView>(song_, engine_, stretchCache_, [this](PatternId patId) {
        editPattern(patId);
    });
    arrangerView_->setOnPushUndo([this]() { pushUndoState(); });
    addChildComponent(arrangerView_.get());

    // Bottom Transport Bar
    transportBar_ = std::make_unique<TransportBar>(
        song_, engine_,
        [this]() {
            if (currentMode_ == ViewMode::Pattern && patternView_ != nullptr)
                patternView_->republish();
            else if (currentMode_ == ViewMode::Arranger && arrangerView_ != nullptr)
                arrangerView_->republish();
        },
        [this]() {
            if (currentMode_ == ViewMode::Pattern && patternView_ != nullptr)
                patternView_->toggleRecording();
            else if (currentMode_ == ViewMode::Arranger && arrangerView_ != nullptr)
                arrangerView_->toggleRecording();
        });

    patternView_->setOnLoopRegionChanged([this](int start, int end) {
        if (currentMode_ == ViewMode::Pattern && transportBar_ != nullptr)
            transportBar_->setLoopRange(start, end, false);
    });

    arrangerView_->setOnLoopRegionChanged([this](int start, int end) {
        if (currentMode_ == ViewMode::Arranger && transportBar_ != nullptr)
            transportBar_->setLoopRange(start, end, true);
    });

    transportBar_->setOnLoopRangeChanged([this](int start, int end) {
        if (currentMode_ == ViewMode::Pattern && patternView_ != nullptr)
            patternView_->setLoopRegion(start, end);
        else if (currentMode_ == ViewMode::Arranger && arrangerView_ != nullptr)
            arrangerView_->setLoopRegion(start, end);
    });

    patternView_->setOnShowToast([this](const juce::String& msg) { showToast(msg); });
    transportBar_->setOnShowToast([this](const juce::String& msg) { showToast(msg); });

    transportBar_->setOnSwingChanged([this](double /*newSwing*/) {
        if (currentMode_ == ViewMode::Pattern && patternView_ != nullptr)
        {
            patternView_->publishSnapshotQuick();
            patternView_->repaintGrid();
        }
        else if (currentMode_ == ViewMode::Arranger && arrangerView_ != nullptr)
        {
            arrangerView_->publishSnapshotQuick();
        }
    });

    engine_.sequencer().setMasterVolume(song_.masterVolume());

    addAndMakeVisible(transportBar_.get());
    addAndMakeVisible(toastOverlay_);

    if (patternView_ != nullptr)
        patternView_->setZoom(song_.patternZoomX(), song_.patternZoomY());
    if (arrangerView_ != nullptr)
        arrangerView_->setZoom(song_.arrangerZoomX(), song_.arrangerZoomY());

    setViewMode(ViewMode::Pattern);
}

MainContainer::~MainContainer() = default;

void MainContainer::pushUndoState()
{
    history_.pushState(song_);
}

void MainContainer::setViewMode(ViewMode mode)
{
    if (mode != currentMode_)
    {
        if (engine_.recorder().state() != RecordState::Idle)
            engine_.recorder().stop();

        if (engine_.sequencer().isPlaying())
        {
            engine_.sequencer().requestPlaying(false);
            engine_.sequencer().requestRewind();
        }
    }

    currentMode_ = mode;

    patternModeButton_.setToggleState(mode == ViewMode::Pattern, juce::dontSendNotification);
    arrangerModeButton_.setToggleState(mode == ViewMode::Arranger, juce::dontSendNotification);

    if (mode == ViewMode::Pattern)
    {
        arrangerView_->setVisible(false);
        patternView_->setVisible(true);
        patternView_->republish();
        if (patternView_->isShowing())
            patternView_->grabKeyboardFocus();
    }
    else
    {
        patternView_->setVisible(false);
        arrangerView_->setVisible(true);
        arrangerView_->updatePatternList();
        arrangerView_->republish();
        if (arrangerView_->isShowing())
            arrangerView_->grabKeyboardFocus();
    }

    if (patternListPanel_ != nullptr)
        patternListPanel_->updateFromModel();

    if (transportBar_ != nullptr)
    {
        if (mode == ViewMode::Pattern && patternView_ != nullptr)
            transportBar_->setLoopRange(patternView_->loopStartStep(), patternView_->loopEndStep(), false);
        else if (mode == ViewMode::Arranger && arrangerView_ != nullptr)
            transportBar_->setLoopRange(arrangerView_->loopStartBar(), arrangerView_->loopEndBar(), true);
    }

    resized();
}

void MainContainer::editPattern(PatternId patternId)
{
    if (patternListPanel_ != nullptr)
        patternListPanel_->setActivePattern(patternId);
    patternView_->setPattern(patternId);
    setViewMode(ViewMode::Pattern);
}

void MainContainer::saveProjectDialog()
{
    fileChooser_ = std::make_unique<juce::FileChooser>(
        "Save trami-loop project folder", juce::File::getSpecialLocation(juce::File::userHomeDirectory), "*", true);

    fileChooser_->launchAsync(
        juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectDirectories,
        [this](const juce::FileChooser& fc) {
            const auto result = fc.getResult();
            if (result != juce::File{})
            {
                const std::filesystem::path dir(result.getFullPathName().toStdString());
                if (ProjectSerializer::saveProject(song_, stretchCache_, dir))
                {
                    currentProjectDir_ = dir;
                    showToast("Project saved: " + result.getFileName());
                }
            }
        });
}

void MainContainer::openProjectDialog()
{
    fileChooser_ = std::make_unique<juce::FileChooser>(
        "Open trami-loop project folder", juce::File::getSpecialLocation(juce::File::userHomeDirectory), "*", true);

    fileChooser_->launchAsync(
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
        [this](const juce::FileChooser& fc) {
            const auto result = fc.getResult();
            if (result != juce::File{})
            {
                const std::filesystem::path dir(result.getFullPathName().toStdString());
                if (auto loaded = ProjectSerializer::loadProject(dir, stretchCache_, importer_, engine_.sampleRate()))
                {
                    song_ = std::move(*loaded);
                    currentProjectDir_ = dir;
                    history_.clear();
                    engine_.sequencer().setMasterVolume(song_.masterVolume());
                    updateAllViews();
                }
            }
        });
}

void MainContainer::showSettingsDialog()
{
    auto* comp =
        new juce::AudioDeviceSelectorComponent(engine_.deviceManager(), 0, 2, 0, 2, false, false, true, false);
    comp->setSize(500, 380);

    juce::DialogWindow::LaunchOptions opt;
    opt.dialogTitle = "Audio Settings";
    opt.content.setOwned(comp);
    opt.dialogBackgroundColour = Palette::surface;
    opt.escapeKeyTriggersCloseButton = true;
    opt.useNativeTitleBar = true;
    opt.resizable = false;
    opt.launchAsync();
}

void MainContainer::showExportDialog()
{
    fileChooser_ = std::make_unique<juce::FileChooser>(
        "Export Mixdown WAV",
        juce::File::getSpecialLocation(juce::File::userHomeDirectory).getChildFile("mixdown.wav"), "*.wav", true);

    fileChooser_->launchAsync(
        juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
        [this](const juce::FileChooser& fc) {
            const auto result = fc.getResult();
            if (result != juce::File{})
            {
                ExportOptions opt;
                opt.sampleRate = engine_.sampleRate();
                opt.bitsPerSample = 24;

                const std::filesystem::path dest(result.getFullPathName().toStdString());
                if (WavExporter::exportMixdown(song_, stretchCache_, opt, dest))
                {
                    juce::AlertWindow::showMessageBoxAsync(
                        juce::MessageBoxIconType::InfoIcon, "Export Complete",
                        "Successfully exported WAV mixdown to:\n" + result.getFullPathName());
                }
            }
        });
}

void MainContainer::showToast(const juce::String& message, bool isError)
{
    toastOverlay_.show(message, isError);
}

void MainContainer::performUndo()
{
    if (history_.undo(song_))
    {
        engine_.sequencer().setMasterVolume(song_.masterVolume());
        updateAllViews();
        showToast("Undo");
    }
}

void MainContainer::performRedo()
{
    if (history_.redo(song_))
    {
        engine_.sequencer().setMasterVolume(song_.masterVolume());
        updateAllViews();
        showToast("Redo");
    }
}

void MainContainer::updateAllViews()
{
    if (patternListPanel_ != nullptr)
    {
        if (!song_.patterns().empty())
            patternListPanel_->setActivePattern(song_.patterns().front().id);
        patternListPanel_->updateFromModel();
    }

    if (patternView_ != nullptr)
    {
        patternView_->setZoom(song_.patternZoomX(), song_.patternZoomY());
        if (!song_.patterns().empty())
            patternView_->setPattern(song_.patterns().front().id);
        patternView_->updateLengthSelector();
        patternView_->updateSampleSelector();
        patternView_->updateTrackHeaders();
        patternView_->republish();
    }

    if (arrangerView_ != nullptr)
    {
        arrangerView_->setZoom(song_.arrangerZoomX(), song_.arrangerZoomY());
        arrangerView_->updatePatternList();
        arrangerView_->republish();
    }

    if (transportBar_ != nullptr)
    {
        transportBar_->updateDisplay();
    }
}

bool MainContainer::keyPressed(const juce::KeyPress& key)
{
    const auto mods = key.getModifiers();
    const bool isCmdOrCtrl = mods.isCommandDown() || mods.isCtrlDown();

    if (isCmdOrCtrl && key.isKeyCode('Z'))
    {
        if (mods.isShiftDown())
            performRedo();
        else
            performUndo();
        return true;
    }

    if (isCmdOrCtrl && key.isKeyCode('Y'))
    {
        performRedo();
        return true;
    }

    if (isCmdOrCtrl && key.isKeyCode('S'))
    {
        saveProjectDialog();
        return true;
    }

    if (isCmdOrCtrl && key.isKeyCode('O'))
    {
        openProjectDialog();
        return true;
    }

    if (currentMode_ == ViewMode::Pattern && patternView_ != nullptr)
        return patternView_->keyPressed(key);
    if (currentMode_ == ViewMode::Arranger && arrangerView_ != nullptr)
        return arrangerView_->keyPressed(key);

    return false;
}

void MainContainer::paint(juce::Graphics& g)
{
    g.fillAll(Palette::background);

    constexpr int kTopBarHeight = 44;
    g.setColour(Palette::surface.darker(0.3f));
    g.fillRect(0, 0, getWidth(), kTopBarHeight);
    g.setColour(Palette::gridLine);
    g.drawHorizontalLine(kTopBarHeight, 0.0f, static_cast<float>(getWidth()));

    g.setColour(Palette::accent);
    g.setFont(Typography::bold(17.0f));
    g.drawText("trami-loop", 14, 0, 110, kTopBarHeight, juce::Justification::centredLeft);
}

void MainContainer::resized()
{
    constexpr int kTopBarHeight = 44;
    constexpr int kBottomBarHeight = 44;
    constexpr int kPatternListWidth = 210;

    auto topArea = juce::Rectangle<int>(0, 0, getWidth(), kTopBarHeight).reduced(8, 4);
    topArea.removeFromLeft(125);

    patternModeButton_.setBounds(topArea.removeFromLeft(64));
    topArea.removeFromLeft(4);
    arrangerModeButton_.setBounds(topArea.removeFromLeft(64));
    topArea.removeFromLeft(16);

    undoButton_.setBounds(topArea.removeFromLeft(48));
    topArea.removeFromLeft(4);
    redoButton_.setBounds(topArea.removeFromLeft(48));
    topArea.removeFromLeft(16);

    saveButton_.setBounds(topArea.removeFromLeft(48));
    topArea.removeFromLeft(4);
    openButton_.setBounds(topArea.removeFromLeft(48));
    topArea.removeFromLeft(16);

    exportButton_.setBounds(topArea.removeFromLeft(54));
    topArea.removeFromLeft(6);
    settingsButton_.setBounds(topArea.removeFromLeft(48));

    // Bottom transport bar
    if (transportBar_ != nullptr)
        transportBar_->setBounds(0, getHeight() - kBottomBarHeight, getWidth(), kBottomBarHeight);

    // Main workspace area
    const int contentY = kTopBarHeight;
    const int contentH = getHeight() - kTopBarHeight - kBottomBarHeight;

    if (patternListPanel_ != nullptr)
        patternListPanel_->setBounds(0, contentY, kPatternListWidth, contentH);

    const auto viewBounds = juce::Rectangle<int>(kPatternListWidth, contentY, getWidth() - kPatternListWidth, contentH);
    if (patternView_ != nullptr)
        patternView_->setBounds(viewBounds);
    if (arrangerView_ != nullptr)
        arrangerView_->setBounds(viewBounds);

    // Toast notification overlay (floating above transport bar)
    const int toastW = std::clamp(320, 200, getWidth() - 40);
    constexpr int toastH = 34;
    toastOverlay_.setBounds((getWidth() - toastW) / 2, getHeight() - kBottomBarHeight - toastH - 10, toastW, toastH);
    toastOverlay_.toFront(true);
}

} // namespace trami
