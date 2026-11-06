#include "ui/MainContainer.h"

#include "ui/TramiLookAndFeel.h"

#include <juce_audio_utils/juce_audio_utils.h>

namespace trami
{

MainContainer::MainContainer(Song& song, AudioEngine& engine, StretchCache& stretchCache)
    : song_(song), engine_(engine), stretchCache_(stretchCache)
{
    setWantsKeyboardFocus(true);

    patternModeButton_.onClick = [this]() { setViewMode(ViewMode::Pattern); };
    addAndMakeVisible(patternModeButton_);

    arrangerModeButton_.onClick = [this]() { setViewMode(ViewMode::Arranger); };
    addAndMakeVisible(arrangerModeButton_);

    undoButton_.onClick = [this]() { performUndo(); };
    addAndMakeVisible(undoButton_);

    redoButton_.onClick = [this]() { performRedo(); };
    addAndMakeVisible(redoButton_);

    saveButton_.onClick = [this]() { saveProjectDialog(); };
    addAndMakeVisible(saveButton_);

    openButton_.onClick = [this]() { openProjectDialog(); };
    addAndMakeVisible(openButton_);

    exportButton_.onClick = [this]() { showExportDialog(); };
    addAndMakeVisible(exportButton_);

    settingsButton_.onClick = [this]() { showSettingsDialog(); };
    addAndMakeVisible(settingsButton_);

    patternView_ = std::make_unique<PatternView>(song_, engine_, stretchCache_);
    addAndMakeVisible(patternView_.get());

    arrangerView_ = std::make_unique<ArrangerView>(song_, engine_, stretchCache_, [this](PatternId patId) {
        editPattern(patId);
    });
    addChildComponent(arrangerView_.get());

    setViewMode(ViewMode::Pattern);
}

MainContainer::~MainContainer() = default;

void MainContainer::setViewMode(ViewMode mode)
{
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

    resized();
}

void MainContainer::editPattern(PatternId patternId)
{
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

void MainContainer::performUndo()
{
    if (history_.undo(song_))
        updateAllViews();
}

void MainContainer::performRedo()
{
    if (history_.redo(song_))
        updateAllViews();
}

void MainContainer::updateAllViews()
{
    if (patternView_ != nullptr)
    {
        if (!song_.patterns().empty())
            patternView_->setPattern(song_.patterns().front().id);
        patternView_->updateLengthSelector();
        patternView_->updateSampleSelector();
        patternView_->republish();
    }

    if (arrangerView_ != nullptr)
    {
        arrangerView_->updatePatternList();
        arrangerView_->republish();
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

    constexpr int kTopBarHeight = 50;
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
    constexpr int kTopBarHeight = 50;
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

    const auto contentBounds = juce::Rectangle<int>(0, kTopBarHeight, getWidth(), getHeight() - kTopBarHeight);
    if (patternView_ != nullptr)
        patternView_->setBounds(contentBounds);
    if (arrangerView_ != nullptr)
        arrangerView_->setBounds(contentBounds);
}

} // namespace trami
