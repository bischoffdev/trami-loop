#include "ui/MainContainer.h"
#include "BinaryData.h"
#include "io/AcidProjectReader.h"
#include "ui/TramiLookAndFeel.h"

#include <juce_audio_utils/juce_audio_utils.h>

namespace trami
{

MainContainer::MainContainer(Song& song, AudioEngine& engine, StretchCache& stretchCache)
    : song_(song), engine_(engine), stretchCache_(stretchCache)
{
    setWantsKeyboardFocus(true);

    undoButton_.setTooltip("Undo last change (Cmd+Z)");
    undoButton_.onClick = [this]() { performUndo(); };
    addAndMakeVisible(undoButton_);

    redoButton_.setTooltip("Redo last undone change (Cmd+Shift+Z)");
    redoButton_.onClick = [this]() { performRedo(); };
    addAndMakeVisible(redoButton_);

    newButton_.setTooltip("Create New Project (Ctrl+N)");
    newButton_.onClick = [this]() { newProjectDialog(); };
    addAndMakeVisible(newButton_);

    openButton_.setTooltip("Open Project (Ctrl+O)");
    openButton_.onClick = [this]() { openProjectDialog(); };
    addAndMakeVisible(openButton_);

    saveButton_.setTooltip("Save Project (Ctrl+S / Ctrl+Shift+S)");
    saveButton_.onClick = [this]() { saveProjectDialog(false); };
    addAndMakeVisible(saveButton_);

    exportButton_.setTooltip("Export Mix to WAV (Cmd+E)");
    exportButton_.onClick = [this]() { showExportDialog(); };
    addAndMakeVisible(exportButton_);

    settingsButton_.setTooltip("Audio Device & MIDI Settings");
    settingsButton_.onClick = [this]() { showSettingsDialog(); };
    addAndMakeVisible(settingsButton_);

    // Multitrack Song View (the single primary timeline view)
    patternView_ = std::make_unique<PatternView>(song_, engine_, stretchCache_);
    patternView_->setOnPushUndo([this]() { pushUndoState(); });
    patternView_->setOnSongRenamed([this](const juce::String& newName)
    {
        currentProjectName_ = newName;
        updateWindowTitle();
    });
    patternView_->setOnOpenAudioSettings([this]() { showSettingsDialog(); });
    addAndMakeVisible(patternView_.get());

    // Bottom Transport Bar
    transportBar_ = std::make_unique<TransportBar>(
        song_, engine_,
        [this]() {
            if (patternView_ != nullptr)
            {
                patternView_->republish();
                patternView_->repaintGrid();
            }
        },
        [this]() {
            if (patternView_ != nullptr)
                patternView_->toggleRecording();
        });

    patternView_->setOnLoopRegionChanged([this](int start, int end) {
        if (transportBar_ != nullptr)
            transportBar_->setLoopRange(start, end, false);
    });

    transportBar_->setOnLoopRangeChanged([this](int start, int end) {
        if (patternView_ != nullptr)
            patternView_->setLoopRegion(start, end);
    });

    patternView_->setOnShowToast([this](const juce::String& msg) { showToast(msg); });
    transportBar_->setOnShowToast([this](const juce::String& msg) { showToast(msg); });

    transportBar_->setOnSwingChanged([this](double /*newSwing*/) {
        if (patternView_ != nullptr)
        {
            patternView_->publishSnapshotQuick();
            patternView_->repaintGrid();
        }
    });

    transportBar_->setOnTimeSignatureChanged([this]() {
        if (patternView_ != nullptr)
            patternView_->onTimeSignatureChanged();
    });

    engine_.sequencer().setMasterVolume(song_.masterVolume());

    addAndMakeVisible(transportBar_.get());
    addAndMakeVisible(toastOverlay_);

    if (song_.patterns().empty())
        song_.addPattern("Unnamed Song", 1);

    patternView_->setPattern(song_.patterns().front().id);
    patternView_->fitZoomToProject(8, 32);
    transportBar_->setLoopRange(patternView_->loopStartStep(), patternView_->loopEndStep(), false);
    updateWindowTitle();
}

MainContainer::~MainContainer() = default;

void MainContainer::parentHierarchyChanged()
{
    updateWindowTitle();
}

juce::String MainContainer::activeProjectDisplayName() const
{
    if (currentProjectName_.isNotEmpty())
        return currentProjectName_;
    if (currentProjectDir_.has_value())
    {
        const auto dirName = currentProjectDir_->filename().string();
        if (!dirName.empty())
            return juce::String::fromUTF8(dirName.c_str());
    }
    if (!song_.patterns().empty() && !song_.patterns().front().name.empty())
        return juce::String::fromUTF8(song_.patterns().front().name.c_str());
    return "Untitled Project";
}

void MainContainer::updateWindowTitle()
{
    const juce::String projName = activeProjectDisplayName();
    const juce::String fullTitle = juce::String(JUCE_APPLICATION_NAME_STRING) +
                                   " v" + juce::String(JUCE_APPLICATION_VERSION_STRING) +
                                   " - " + projName;
    if (auto* docWin = findParentComponentOfClass<juce::DocumentWindow>())
        docWin->setName(fullTitle);
    repaint(0, 0, getWidth(), 44);
}

void MainContainer::pushUndoState()
{
    history_.pushState(song_);
    undoAudioStack_.push_back(stretchCache_.getRawBank());
    if (undoAudioStack_.size() > SongHistory::kMaxHistory)
        undoAudioStack_.erase(undoAudioStack_.begin());
    redoAudioStack_.clear();
}

void MainContainer::newProjectDialog()
{
    bool hasContent = !song_.samples().empty();
    for (const auto& pat : song_.patterns())
    {
        for (const auto& trk : pat.tracks)
        {
            if (!trk.clips.empty())
            {
                hasContent = true;
                break;
            }
        }
        if (hasContent)
            break;
    }
    for (int t = 0; t < song_.numArrangerTracks(); ++t)
    {
        if (const auto* trk = song_.arrangerTrack(t))
        {
            if (!trk->blocks.empty())
            {
                hasContent = true;
                break;
            }
        }
    }

    if (hasContent)
    {
        juce::AlertWindow::showOkCancelBox(
            juce::MessageBoxIconType::QuestionIcon,
            "New Project",
            "Are you sure you want to create a new project?\nAny unsaved changes will be lost.",
            "Choose Location...",
            "Cancel",
            this,
            juce::ModalCallbackFunction::create([this](int result) {
                if (result == 1)
                    promptNewProjectLocation();
            }));
    }
    else
    {
        promptNewProjectLocation();
    }
}

void MainContainer::promptNewProjectLocation()
{
    juce::File initialLoc;
    if (currentProjectDir_.has_value())
        initialLoc = juce::File(juce::String::fromUTF8(currentProjectDir_->string().c_str())).getParentDirectory();
    else
        initialLoc = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);

    initialLoc = initialLoc.getChildFile("NewProject.trami");

    fileChooser_ = std::make_unique<juce::FileChooser>(
        "Choose Location for New Project", initialLoc, "*.trami", true);

    fileChooser_->launchAsync(
        juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
        [this](const juce::FileChooser& fc) {
            const auto result = fc.getResult();
            if (result != juce::File{})
            {
                const std::filesystem::path chosen(result.getFullPathName().toStdString());
                std::filesystem::path dir;
                if (std::filesystem::is_directory(chosen))
                {
                    dir = chosen;
                }
                else if (chosen.filename() == "project.trami")
                {
                    dir = chosen.parent_path();
                }
                else
                {
                    const auto stem = chosen.stem().string();
                    dir = chosen.parent_path() / stem;
                }

                createNewProjectAt(dir);
            }
        });
}

void MainContainer::createNewProjectAt(const std::filesystem::path& dir)
{
    if (engine_.recorder().state() != RecordState::Idle)
        engine_.recorder().stop();

    engine_.sequencer().requestPlaying(false);
    engine_.sequencer().requestRewind();
    engine_.pluginHost().clearAllChains();

    song_ = Song{};
    const auto songPattern = song_.addPattern(dir.stem().string().empty() ? "Unnamed Song" : dir.stem().string(), 1);

    stretchCache_.clear();
    currentProjectDir_ = dir;
    currentProjectName_ = juce::String::fromUTF8(dir.filename().string().c_str());
    currentProjectIsAcidImport_ = false;
    history_.clear();
    undoAudioStack_.clear();
    redoAudioStack_.clear();

    engine_.sequencer().setMasterVolume(song_.masterVolume());

    ProjectSerializer::saveProject(song_, stretchCache_, dir);

    patternView_->setPattern(songPattern, true);
    updateAllViews();
    updateWindowTitle();
    patternView_->fitZoomToProject(8, 32);
    showToast("New Project Created: " + juce::String(dir.filename().string()));
}

void MainContainer::createNewProject()
{
    promptNewProjectLocation();
}

void MainContainer::saveProjectDialog(bool promptForNewLocation)
{
    if (!song_.patterns().empty())
    {
        if (auto* pat = song_.findPattern(song_.patterns().front().id))
            engine_.pluginHost().captureStatesIntoPattern(*pat);
    }

    if (!promptForNewLocation && !currentProjectIsAcidImport_ && currentProjectDir_.has_value())
    {
        if (ProjectSerializer::saveProject(song_, stretchCache_, *currentProjectDir_))
        {
            showToast("Project saved");
        }
        else
        {
            showToast("Failed to save project", true);
        }
        return;
    }

    juce::File initialLoc;
    if (currentProjectDir_.has_value())
    {
        const juce::File baseDir(juce::String::fromUTF8(currentProjectDir_->string().c_str()));
        if (currentProjectIsAcidImport_)
        {
            juce::String defaultProjectName = currentProjectName_.isNotEmpty()
                ? currentProjectName_
                : (song_.patterns().empty() ? "MyProject" : juce::String(song_.patterns().front().name));
            defaultProjectName = juce::File::createLegalFileName(defaultProjectName);
            if (defaultProjectName.isEmpty())
                defaultProjectName = "MyProject";
            initialLoc = baseDir.getChildFile(defaultProjectName + ".trami");
        }
        else
        {
            initialLoc = baseDir;
        }
    }
    else
    {
        juce::File defaultDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
        juce::String defaultProjectName = song_.patterns().empty() ? "MyProject" : juce::String(song_.patterns().front().name);
        defaultProjectName = juce::File::createLegalFileName(defaultProjectName);
        if (defaultProjectName.isEmpty())
            defaultProjectName = "MyProject";
        initialLoc = defaultDir.getChildFile(defaultProjectName + ".trami");
    }

    fileChooser_ = std::make_unique<juce::FileChooser>(
        "Save TramiLoop project", initialLoc, "*.trami", true);

    fileChooser_->launchAsync(
        juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
        [this](const juce::FileChooser& fc) {
            const auto result = fc.getResult();
            if (result != juce::File{})
            {
                const std::filesystem::path chosen(result.getFullPathName().toStdString());
                std::filesystem::path dir;
                if (std::filesystem::is_directory(chosen))
                {
                    dir = chosen;
                }
                else if (chosen.filename() == "project.trami")
                {
                    dir = chosen.parent_path();
                }
                else
                {
                    const auto stem = chosen.stem().string();
                    dir = chosen.parent_path() / stem;
                }

                if (ProjectSerializer::saveProject(song_, stretchCache_, dir))
                {
                    currentProjectDir_ = dir;
                    currentProjectName_ = juce::String::fromUTF8(dir.filename().string().c_str());
                    currentProjectIsAcidImport_ = false;
                    updateWindowTitle();
                    showToast("Project saved: " + juce::String(dir.filename().string()));
                }
                else
                {
                    showToast("Failed to save project", true);
                }
            }
        });
}

void MainContainer::openProjectDialog()
{
    juce::File initialLoc;
    if (currentProjectDir_.has_value())
        initialLoc = juce::File(juce::String::fromUTF8(currentProjectDir_->string().c_str())).getParentDirectory();
    else
        initialLoc = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);

    fileChooser_ = std::make_unique<juce::FileChooser>(
        "Open project (*.trami, *.acd, *.acd-zip, *.acp, *.acp-zip)", initialLoc,
        "*.trami;*.acd;*.acd-zip;*.acp;*.acp-zip", true);

    fileChooser_->launchAsync(
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& fc) {
            const auto result = fc.getResult();
            if (result != juce::File{})
            {
                const std::filesystem::path chosen(result.getFullPathName().toStdString());

                // Check if chosen file is an ACID project (.acd or .acd-zip)
                if (AcidProjectReader::isAcidProject(chosen))
                {
                    if (engine_.recorder().state() != RecordState::Idle)
                        engine_.recorder().stop();
                    engine_.sequencer().requestPlaying(false);
                    engine_.sequencer().requestRewind();
                    engine_.sequencer().stopPreview();

                    std::filesystem::path extractedDir;
                    if (auto loaded = AcidProjectReader::loadProject(chosen, stretchCache_, importer_, engine_.sampleRate(), &extractedDir))
                    {
                        song_ = std::move(*loaded);
                        currentProjectDir_ = chosen.parent_path();
                        currentProjectName_ = juce::String::fromUTF8(chosen.stem().string().c_str());
                        currentProjectIsAcidImport_ = true;
                        history_.clear();
    undoAudioStack_.clear();
    redoAudioStack_.clear();
                        engine_.pluginHost().clearAllChains();
                        engine_.sequencer().resetTrackMix();
                        engine_.sequencer().setMasterVolume(song_.masterVolume());
                        if (patternView_ != nullptr && !song_.patterns().empty())
                            patternView_->setPattern(song_.patterns().front().id, true);
                        updateAllViews();
                        updateWindowTitle();
                        if (patternView_ != nullptr)
                            patternView_->fitZoomToProject(8, 32);
                        int missingAudio = 0;
                        if (!song_.patterns().empty())
                        {
                            const auto& pat = song_.patterns().front();
                            for (const auto& trk : pat.tracks)
                            {
                                if (const auto* s = song_.findSample(trk.sampleId))
                                {
                                    if (s->missing || s->durationSeconds <= 0.0)
                                        missingAudio++;
                                }
                            }
                        }
                        if (missingAudio > 0)
                            showToast("Opened project: " + juce::String(chosen.stem().string()) + " (" + juce::String(missingAudio) + " missing samples)");
                        else
                            showToast("Opened project: " + juce::String(chosen.stem().string()));
                        return;
                    }
                    else
                    {
                        showToast("Failed to open project", true);
                        return;
                    }
                }

                std::filesystem::path projectPath;
                if (std::filesystem::is_directory(chosen))
                {
                    projectPath = chosen;
                }
                else if (chosen.filename() == "project.trami")
                {
                    projectPath = chosen.parent_path();
                }
                else
                {
                    projectPath = chosen;
                }

                if (engine_.recorder().state() != RecordState::Idle)
                    engine_.recorder().stop();
                engine_.sequencer().requestPlaying(false);
                engine_.sequencer().requestRewind();
                engine_.sequencer().stopPreview();

                if (auto loaded = ProjectSerializer::loadProject(projectPath, stretchCache_, importer_, engine_.sampleRate()))
                {
                    song_ = std::move(*loaded);
                    currentProjectDir_ = std::filesystem::is_directory(projectPath) ? projectPath : projectPath.parent_path();
                    const auto projStem = (chosen.filename() == "project.trami")
                                              ? chosen.parent_path().filename().string()
                                              : chosen.stem().string();
                    currentProjectName_ = juce::String::fromUTF8(projStem.c_str());
                    currentProjectIsAcidImport_ = false;
                    history_.clear();
    undoAudioStack_.clear();
    redoAudioStack_.clear();
                    engine_.pluginHost().clearAllChains();
                    engine_.sequencer().resetTrackMix();
                    engine_.sequencer().setMasterVolume(song_.masterVolume());
                    if (patternView_ != nullptr && !song_.patterns().empty())
                        patternView_->setPattern(song_.patterns().front().id, true);
                    updateAllViews();
                    updateWindowTitle();
                    if (patternView_ != nullptr)
                        patternView_->fitZoomToProject(8, 32);
                    showToast("Project loaded: " + juce::String(chosen.stem().string()));
                }
                else
                {
                    showToast("Failed to open project", true);
                }
            }
        });
}

void MainContainer::showSettingsDialog()
{
    auto* comp =
        new juce::AudioDeviceSelectorComponent(engine_.deviceManager(), 0, 32, 0, 2, false, false, true, false);
    comp->setSize(500, 420);

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
    const float curPatZoomX = song_.patternZoomX();
    const float curPatZoomY = song_.patternZoomY();
    const float curArrZoomX = song_.arrangerZoomX();
    const float curArrZoomY = song_.arrangerZoomY();

    if (history_.undo(song_))
    {
        if (!undoAudioStack_.empty())
        {
            redoAudioStack_.push_back(stretchCache_.getRawBank());
            stretchCache_.restoreRawBank(std::move(undoAudioStack_.back()));
            undoAudioStack_.pop_back();
        }
        // Keep the user's current zoom level and tool mode intact across undo
        song_.setPatternZoom(curPatZoomX, curPatZoomY);
        song_.setArrangerZoom(curArrZoomX, curArrZoomY);
        engine_.sequencer().setMasterVolume(song_.masterVolume());
        updateAllViews();
        showToast("Undo");
    }
}

void MainContainer::performRedo()
{
    const float curPatZoomX = song_.patternZoomX();
    const float curPatZoomY = song_.patternZoomY();
    const float curArrZoomX = song_.arrangerZoomX();
    const float curArrZoomY = song_.arrangerZoomY();

    if (history_.redo(song_))
    {
        if (!redoAudioStack_.empty())
        {
            undoAudioStack_.push_back(stretchCache_.getRawBank());
            stretchCache_.restoreRawBank(std::move(redoAudioStack_.back()));
            redoAudioStack_.pop_back();
        }
        // Keep the user's current zoom level and tool mode intact across redo
        song_.setPatternZoom(curPatZoomX, curPatZoomY);
        song_.setArrangerZoom(curArrZoomX, curArrZoomY);
        engine_.sequencer().setMasterVolume(song_.masterVolume());
        updateAllViews();
        showToast("Redo");
    }
}

void MainContainer::updateAllViews()
{
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

    if (transportBar_ != nullptr)
    {
        if (patternView_ != nullptr)
            transportBar_->setLoopRange(patternView_->loopStartStep(), patternView_->loopEndStep(), false);
        transportBar_->updateDisplay();
    }
    updateWindowTitle();
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

    if (isCmdOrCtrl && key.isKeyCode('N'))
    {
        newProjectDialog();
        return true;
    }

    if (isCmdOrCtrl && key.isKeyCode('S'))
    {
        saveProjectDialog(mods.isShiftDown());
        return true;
    }

    if (isCmdOrCtrl && key.isKeyCode('O'))
    {
        openProjectDialog();
        return true;
    }

    if (patternView_ != nullptr)
        return patternView_->keyPressed(key);

    return false;
}

void MainContainer::paint(juce::Graphics& g)
{
    g.fillAll(Palette::background);

    constexpr int kTopBarHeight = 44;
    const int mainW = getWidth();

    // Brushed dark-metallic top toolbar gradient (matching Screenshot 2)
    juce::ColourGradient topGrad(juce::Colour(0xff46464d), 0.0f, 0.0f,
                                 juce::Colour(0xff26262b), 0.0f, static_cast<float>(kTopBarHeight), false);
    topGrad.addColour(0.50, juce::Colour(0xff333339));
    g.setGradientFill(topGrad);
    g.fillRect(0, 0, mainW, kTopBarHeight);

    // Specular top highlight & crisp dark bottom border
    g.setColour(juce::Colours::white.withAlpha(0.14f));
    g.drawHorizontalLine(0, 0.0f, static_cast<float>(mainW));
    g.setColour(juce::Colour(0xff141417));
    g.drawHorizontalLine(kTopBarHeight - 1, 0.0f, static_cast<float>(mainW));

    // App icon badge + clean modern metallic title
    const auto appIcon = juce::ImageCache::getFromMemory(BinaryData::icon128_png, BinaryData::icon128_pngSize);
    if (appIcon.isValid())
    {
        g.drawImageWithin(appIcon, 10, 8, 28, 28, juce::RectanglePlacement::centred);
    }
    g.setColour(juce::Colours::black.withAlpha(0.45f));
    g.setFont(Typography::bold(15.5f));
    g.drawText("TramiLoop", 43, 1, 90, kTopBarHeight, juce::Justification::centredLeft);
    g.setColour(juce::Colour(0xfff0f0f6));
    g.drawText("TramiLoop", 43, 0, 90, kTopBarHeight, juce::Justification::centredLeft);

    // Subtle version badge on the right of the top bar
    g.setColour(Palette::textDim.withAlpha(0.75f));
    g.setFont(Typography::regular(11.5f));
    g.drawText("v" + juce::String(JUCE_APPLICATION_VERSION_STRING),
               mainW - 92, 0, 80, kTopBarHeight, juce::Justification::centredRight);
}

void MainContainer::resized()
{
    constexpr int kTopBarHeight = 44;
    constexpr int kBottomBarHeight = 48;
    const int mainW = getWidth();

    auto topArea = juce::Rectangle<int>(0, 0, mainW, kTopBarHeight).reduced(10, 8);
    topArea.removeFromLeft(130);

    undoButton_.setBounds(topArea.removeFromLeft(66));
    topArea.removeFromLeft(4);
    redoButton_.setBounds(topArea.removeFromLeft(66));
    topArea.removeFromLeft(14);

    newButton_.setBounds(topArea.removeFromLeft(62));
    topArea.removeFromLeft(4);
    openButton_.setBounds(topArea.removeFromLeft(66));
    topArea.removeFromLeft(4);
    saveButton_.setBounds(topArea.removeFromLeft(64));
    topArea.removeFromLeft(14);

    exportButton_.setBounds(topArea.removeFromLeft(74));
    topArea.removeFromLeft(4);
    settingsButton_.setBounds(topArea.removeFromLeft(82));

    // Bottom transport bar
    if (transportBar_ != nullptr)
        transportBar_->setBounds(0, getHeight() - kBottomBarHeight, mainW, kBottomBarHeight);

    // Main workspace area
    const int contentY = kTopBarHeight;
    const int contentH = getHeight() - kTopBarHeight - kBottomBarHeight;

    if (patternView_ != nullptr)
        patternView_->setBounds(0, contentY, mainW, contentH);

    // Toast notification overlay (floating above transport bar)
    const int toastW = std::clamp(320, 200, std::max(200, mainW - 40));
    constexpr int toastH = 34;
    toastOverlay_.setBounds((mainW - toastW) / 2, getHeight() - kBottomBarHeight - toastH - 10, toastW, toastH);
    toastOverlay_.toFront(true);
}

} // namespace trami
