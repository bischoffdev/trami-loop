#include "ui/SampleBrowserPanel.h"

#include "engine/TimeStretcher.h"
#include "model/Timing.h"
#include "ui/TramiLookAndFeel.h"

#include <algorithm>
#include <cmath>

namespace trami
{

SampleBrowserPanel::SampleBrowserPanel(Song& song, AudioEngine& engine, StretchCache& stretchCache,
                                       SampleImportedCallback onImported)
    : song_(song), engine_(engine), stretchCache_(stretchCache), onImported_(std::move(onImported))
{
    formatThread_.startThread(juce::Thread::Priority::normal);

    const auto root = juce::File::getSpecialLocation(juce::File::userHomeDirectory);
    directoryList_ = std::make_unique<juce::DirectoryContentsList>(&filter_, formatThread_);
    directoryList_->setDirectory(root, true, true);

    fileTree_ = std::make_unique<juce::FileTreeComponent>(*directoryList_);
    fileTree_->addListener(this);
    fileTree_->addMouseListener(this, true);
    fileTree_->setColour(juce::FileTreeComponent::backgroundColourId, Palette::background);
    fileTree_->setColour(juce::DirectoryContentsDisplayComponent::textColourId, Palette::text);
    addAndMakeVisible(fileTree_.get());

    titleLabel_.setFont(Typography::bold(12.5f));
    titleLabel_.setColour(juce::Label::textColourId, Palette::text);
    titleLabel_.setText("EXPLORER", juce::dontSendNotification);
    addAndMakeVisible(titleLabel_);

    refreshDrives();
    driveSelector_.setTooltip("Choose Drive or Folder (e.g. CD/DVD, USB, External Drives)");
    driveSelector_.onChange = [this]() {
        const int selectedId = driveSelector_.getSelectedId();
        if (selectedId == 9999)
        {
            folderChooser_ = std::make_unique<juce::FileChooser>(
                "Choose Directory or External Drive",
                directoryList_ != nullptr ? directoryList_->getDirectory() : juce::File::getSpecialLocation(juce::File::userHomeDirectory),
                "*", true);

            auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories;
            folderChooser_->launchAsync(flags, [this](const juce::FileChooser& fc) {
                const auto chosen = fc.getResult();
                if (chosen.exists())
                    selectDirectory(chosen);
            });
            return;
        }

        if (selectedId > 0 && selectedId <= static_cast<int>(driveList_.size()))
        {
            const auto& dir = driveList_[static_cast<std::size_t>(selectedId - 1)];
            selectDirectory(dir);
        }
    };
    addAndMakeVisible(driveSelector_);

    upFolderButton_.setTooltip("Up one directory level");
    upFolderButton_.onClick = [this]() {
        if (directoryList_ != nullptr)
        {
            const auto current = directoryList_->getDirectory();
            const auto parent = current.getParentDirectory();
            if (parent != current && parent.exists())
                selectDirectory(parent);
        }
    };
    addAndMakeVisible(upFolderButton_);

    browseFolderButton_.setTooltip("Browse custom directory or drive...");
    browseFolderButton_.onClick = [this]() {
        folderChooser_ = std::make_unique<juce::FileChooser>(
            "Choose Directory or External Drive",
            directoryList_ != nullptr ? directoryList_->getDirectory() : juce::File::getSpecialLocation(juce::File::userHomeDirectory),
            "*", true);

        auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories;
        folderChooser_->launchAsync(flags, [this](const juce::FileChooser& fc) {
            const auto chosen = fc.getResult();
            if (chosen.exists())
                selectDirectory(chosen);
        });
    };
    addAndMakeVisible(browseFolderButton_);

    pathLabel_.setFont(Typography::regular(11.5f));
    pathLabel_.setColour(juce::Label::textColourId, Palette::textDim);
    pathLabel_.setText(root.getFullPathName(), juce::dontSendNotification);
    addAndMakeVisible(pathLabel_);

    fileNameLabel_.setFont(Typography::bold(14.0f));
    fileNameLabel_.setColour(juce::Label::textColourId, Palette::text);
    fileNameLabel_.setText("No file selected", juce::dontSendNotification);
    addAndMakeVisible(fileNameLabel_);

    fileDetailsLabel_.setFont(Typography::regular(11.5f));
    fileDetailsLabel_.setColour(juce::Label::textColourId, Palette::textDim);
    fileDetailsLabel_.setText("Drag audio files onto any track above", juce::dontSendNotification);
    addAndMakeVisible(fileDetailsLabel_);

    autoPreviewToggle_.setColour(juce::ToggleButton::textColourId, Palette::textDim);
    autoPreviewToggle_.setTooltip("Automatically play samples on selection");
    addAndMakeVisible(autoPreviewToggle_);

    syncPreviewToggle_.setColour(juce::ToggleButton::textColourId, Palette::accent);
    syncPreviewToggle_.setToggleState(true, juce::dontSendNotification);
    syncPreviewToggle_.setTooltip("ACID In-Sync Auditioning: Time-stretch loops to project BPM, conform to project Key, and beat-sync preview when playing");
    addAndMakeVisible(syncPreviewToggle_);

    previewButton_.setTooltip("Preview selected sample (Space)");
    previewButton_.onClick = [this]() { previewCurrentSelection(); };
    addAndMakeVisible(previewButton_);

    addLoopButton_.setTooltip("Add as Loop to New Track");
    addLoopButton_.onClick = [this]() {
        const auto file = fileTree_->getSelectedFile();
        if (file.existsAsFile() && importer_.isSupportedFormat(file.getFullPathName().toStdString()))
        {
            if (onAddToNewTrack_)
                onAddToNewTrack_(file.getFullPathName(), SampleKind::Loop);
            else
                importCurrentSelection(SampleKind::Loop);
        }
    };
    addAndMakeVisible(addLoopButton_);

    addOneShotButton_.setTooltip("Add as One-Shot to New Track");
    addOneShotButton_.onClick = [this]() {
        const auto file = fileTree_->getSelectedFile();
        if (file.existsAsFile() && importer_.isSupportedFormat(file.getFullPathName().toStdString()))
        {
            if (onAddToNewTrack_)
                onAddToNewTrack_(file.getFullPathName(), SampleKind::OneShot);
            else
                importCurrentSelection(SampleKind::OneShot);
        }
    };
    addAndMakeVisible(addOneShotButton_);

    addDiskButton_.setTooltip("Add as Disk-Based Stream Track (for long audio files)");
    addDiskButton_.onClick = [this]() {
        const auto file = fileTree_->getSelectedFile();
        if (file.existsAsFile() && importer_.isSupportedFormat(file.getFullPathName().toStdString()))
        {
            if (onAddToNewTrack_)
                onAddToNewTrack_(file.getFullPathName(), SampleKind::DiskStream);
            else
                importCurrentSelection(SampleKind::DiskStream);
        }
    };
    addAndMakeVisible(addDiskButton_);

    startTimerHz(20);
}

SampleBrowserPanel::~SampleBrowserPanel()
{
    stopTimer();
    fileTree_->removeMouseListener(this);
    fileTree_->removeListener(this);
    formatThread_.stopThread(1000);
}

void SampleBrowserPanel::selectionChanged()
{
    const auto file = fileTree_->getSelectedFile();
    if (file.exists())
    {
        pathLabel_.setText(file.getFullPathName(), juce::dontSendNotification);

        if (file.existsAsFile() && importer_.isSupportedFormat(file.getFullPathName().toStdString()))
        {
            const auto filePath = file.getFullPathName().toStdString();
            fileNameLabel_.setText(file.getFileName(), juce::dontSendNotification);

            // Load audio data and ACID metadata for live waveform preview
            selectedAcidMeta_ = AcidChunkReader::readFromFile(filePath);
            auto audioOpt = importer_.loadAudioData(filePath, engine_.sampleRate());
            if (audioOpt.has_value())
            {
                selectedAudio_ = std::make_shared<const AudioData>(std::move(*audioOpt));
                previewDuration_ = static_cast<double>(selectedAudio_->numFrames()) / engine_.sampleRate();
                juce::String acidInfo;
                if (selectedAcidMeta_.has_value() && selectedAcidMeta_->isAcidized)
                {
                    if (selectedAcidMeta_->isDiskBased)
                    {
                        acidInfo = "  |  ACID Disk-Based";
                    }
                    else if (selectedAcidMeta_->isOneShot)
                    {
                        acidInfo = "  |  ACID One-Shot";
                    }
                    else
                    {
                        const double trueBpm = (selectedAcidMeta_->numBeats > 0 && previewDuration_ > 0.0)
                            ? ((static_cast<double>(selectedAcidMeta_->numBeats) * 60.0) / previewDuration_)
                            : selectedAcidMeta_->tempo;
                        acidInfo = "  |  ACID Loop (" + juce::String(selectedAcidMeta_->numBeats) + " beats, " +
                                   juce::String(trueBpm, 1) + " BPM";
                        if (selectedAcidMeta_->rootNote >= 0)
                            acidInfo += juce::String(", Key: ") + Sample::pitchClassName(selectedAcidMeta_->rootNote);
                        acidInfo += ")";
                    }
                }
                fileDetailsLabel_.setText(
                    juce::String(previewDuration_, 2) + "s  |  " +
                    juce::String(static_cast<int>(engine_.sampleRate())) + " Hz  |  " +
                    (selectedAudio_->numChannels() == 2 ? "Stereo" : "Mono") +
                    acidInfo,
                    juce::dontSendNotification);
            }
            else
            {
                selectedAudio_.reset();
                previewDuration_ = 0.0;
                fileDetailsLabel_.setText("Could not load audio details", juce::dontSendNotification);
            }

            if (autoPreviewToggle_.getToggleState())
            {
                previewCurrentSelection();
            }
        }
        else
        {
            selectedAudio_.reset();
            previewDuration_ = 0.0;
            fileNameLabel_.setText(file.isDirectory() ? file.getFileName() : "No file selected", juce::dontSendNotification);
            fileDetailsLabel_.setText(file.isDirectory() ? "Directory" : "Drag audio files onto any track above", juce::dontSendNotification);
        }

        if (engine_.sequencer().isPreviewActive() && currentPreviewPath_ == file.getFullPathName().toStdString())
        {
            previewButton_.setIcon(IconButton::Icon::Stop);
            previewButton_.setLabel("Stop");
            previewButton_.setToggleState(true, juce::dontSendNotification);
        }
        else
        {
            previewButton_.setIcon(IconButton::Icon::Play);
            previewButton_.setLabel("Preview");
            previewButton_.setToggleState(false, juce::dontSendNotification);
        }

        previewProgress_ = 0.0;
        repaint();
    }
}

void SampleBrowserPanel::fileClicked(const juce::File& file, const juce::MouseEvent& e)
{
    juce::ignoreUnused(file, e);
    // Do not initiate drag on simple clicks; selectionChanged handles file selection.
}

void SampleBrowserPanel::mouseDown(const juce::MouseEvent& e)
{
    juce::ignoreUnused(e);
}

void SampleBrowserPanel::mouseDrag(const juce::MouseEvent& e)
{
    if (e.getDistanceFromDragStart() > 6)
    {
        const auto file = fileTree_->getSelectedFile();
        if (file.existsAsFile() && importer_.isSupportedFormat(file.getFullPathName().toStdString()))
        {
            startSampleDrag(file);
        }
    }
}

void SampleBrowserPanel::startSampleDrag(const juce::File& file)
{
    auto* ddc = juce::DragAndDropContainer::findParentDragContainerFor(this);
    if (ddc == nullptr || ddc->isDragAndDropActive())
        return;

    constexpr int imgW = 160;
    constexpr int imgH = 26;
    juce::Image dragImg(juce::Image::ARGB, imgW, imgH, true);
    {
        juce::Graphics g(dragImg);
        g.setColour(juce::Colour(0xee161a22));
        g.fillRoundedRectangle(0.5f, 0.5f, imgW - 1.0f, imgH - 1.0f, 4.0f);
        g.setColour(Palette::accent);
        g.drawRoundedRectangle(0.5f, 0.5f, imgW - 1.0f, imgH - 1.0f, 4.0f, 1.5f);

        // Music note glyph
        g.setFont(Typography::bold(12.0f));
        g.drawText(juce::CharPointer_UTF8("\xe2\x99\xab"), 6, 0, 16, imgH, juce::Justification::centred);

        g.setColour(Palette::text);
        g.setFont(Typography::regular(11.5f));
        g.drawText(file.getFileName(), 24, 0, imgW - 30, imgH, juce::Justification::centredLeft, true);
    }

    const juce::Point<int> offset(-15, -13);
    ddc->startDragging(file.getFullPathName(), this, juce::ScaledImage(dragImg, 1.0), true, &offset);
}

void SampleBrowserPanel::fileDoubleClicked(const juce::File& file)
{
    if (file.existsAsFile())
        previewCurrentSelection();
}

void SampleBrowserPanel::browserRootChanged(const juce::File& newRoot)
{
    pathLabel_.setText(newRoot.getFullPathName(), juce::dontSendNotification);
}

void SampleBrowserPanel::refreshDrives()
{
    driveSelector_.clear(juce::dontSendNotification);
    driveList_.clear();

    juce::Array<juce::File> roots;
    juce::File::findFileSystemRoots(roots);

    int id = 1;
    for (const auto& root : roots)
    {
        juce::String name = root.getFullPathName();
        juce::String vol = root.getVolumeLabel();
        juce::String label = name;
        if (vol.isNotEmpty())
            label += " [" + vol + "]";
        if (root.isOnCDRomDrive())
            label += " (CD/DVD)";
        else if (root.isOnRemovableDrive())
            label += " (Removable/USB)";
        else if (root.isOnHardDisk())
            label += " (Drive)";

        driveList_.push_back(root);
        driveSelector_.addItem(label, id++);
    }

    driveSelector_.addSeparator();

    auto home = juce::File::getSpecialLocation(juce::File::userHomeDirectory);
    auto desktop = juce::File::getSpecialLocation(juce::File::userDesktopDirectory);
    auto music = juce::File::getSpecialLocation(juce::File::userMusicDirectory);
    auto docs = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);

    if (home.exists())
    {
        driveList_.push_back(home);
        driveSelector_.addItem("Home (" + home.getFileName() + ")", id++);
    }
    if (desktop.exists())
    {
        driveList_.push_back(desktop);
        driveSelector_.addItem("Desktop", id++);
    }
    if (music.exists())
    {
        driveList_.push_back(music);
        driveSelector_.addItem("Music", id++);
    }
    if (docs.exists())
    {
        driveList_.push_back(docs);
        driveSelector_.addItem("Documents", id++);
    }

    driveSelector_.addSeparator();
    driveSelector_.addItem("Browse Folder...", 9999);
}

void SampleBrowserPanel::selectDirectory(const juce::File& dir)
{
    if (dir.exists())
    {
        if (directoryList_ != nullptr)
            directoryList_->setDirectory(dir, true, true);
        pathLabel_.setText(dir.getFullPathName(), juce::dontSendNotification);

        for (std::size_t i = 0; i < driveList_.size(); ++i)
        {
            if (driveList_[i] == dir)
            {
                driveSelector_.setSelectedId(static_cast<int>(i + 1), juce::dontSendNotification);
                return;
            }
        }
    }
}

void SampleBrowserPanel::previewCurrentSelection()
{
    const auto file = fileTree_->getSelectedFile();
    if (!file.existsAsFile())
        return;

    const auto filePath = file.getFullPathName().toStdString();
    if (!importer_.isSupportedFormat(filePath))
        return;

    // Toggle stop if already previewing this file
    if (engine_.sequencer().isPreviewActive() && currentPreviewPath_ == filePath)
    {
        engine_.sequencer().stopPreview();
        currentPreviewPath_.clear();
        previewProgress_ = 0.0;
        previewButton_.setIcon(IconButton::Icon::Play);
        previewButton_.setLabel("Preview");
        previewButton_.setToggleState(false, juce::dontSendNotification);
        repaint();
        return;
    }

    if (selectedAudio_ == nullptr || selectedAudio_->numFrames() == 0 || currentPreviewPath_ != filePath)
    {
        selectedAcidMeta_ = AcidChunkReader::readFromFile(filePath);
        auto audioOpt = importer_.loadAudioData(filePath, engine_.sampleRate());
        if (!audioOpt.has_value())
            return;
        selectedAudio_ = std::make_shared<const AudioData>(std::move(*audioOpt));
    }

    currentPreviewPath_ = filePath;
    const double rawDur = static_cast<double>(selectedAudio_->numFrames()) / engine_.sampleRate();
    std::shared_ptr<const AudioData> audioToPlay = selectedAudio_;
    int startDelaySamples = 0;
    bool loopPreview = false;

    const bool hasAcid = (selectedAcidMeta_.has_value() && selectedAcidMeta_->isAcidized);
    const bool isLoopCandidate = hasAcid
        ? (!selectedAcidMeta_->isOneShot && !selectedAcidMeta_->isDiskBased)
        : (rawDur >= 0.35 && rawDur <= 16.0);

    if (syncPreviewToggle_.getToggleState() && isLoopCandidate && rawDur > 0.05 && song_.bpm() > 20.0)
    {
        double loopBeats = 4.0;
        if (hasAcid && selectedAcidMeta_->numBeats > 0)
        {
            loopBeats = static_cast<double>(selectedAcidMeta_->numBeats);
        }
        else
        {
            const double secPerBeat = 60.0 / song_.bpm();
            const double estBeats = rawDur / secPerBeat;
            if (estBeats < 1.5) loopBeats = 1.0;
            else if (estBeats < 3.0) loopBeats = 2.0;
            else if (estBeats < 6.0) loopBeats = 4.0;
            else if (estBeats < 12.0) loopBeats = 8.0;
            else loopBeats = 16.0;
        }

        const double origBpm = (loopBeats * 60.0) / rawDur;
        const double ratio = (origBpm > 20.0 && origBpm < 300.0) ? (origBpm / song_.bpm()) : 1.0;
        const int semitones = hasAcid
            ? Sample::keyConformSemitones(selectedAcidMeta_->rootNote, song_.projectRootNote())
            : 0;
        const double pitchScale = std::pow(2.0, static_cast<double>(semitones) / 12.0);

        if (std::abs(ratio - 1.0) > 0.002 || semitones != 0)
        {
            AudioData stretched = TimeStretcher::stretch(*selectedAudio_, ratio, engine_.sampleRate(), pitchScale);
            audioToPlay = std::make_shared<const AudioData>(std::move(stretched));
        }

        if (engine_.sequencer().isPlaying())
        {
            loopPreview = true;
            const Timing timing(song_.bpm(), engine_.sampleRate(), song_.swing());
            const std::int64_t samplesPerBeat = std::max<std::int64_t>(1, std::llround(timing.samplesPerStep() * kStepsPerBeat));
            const std::int64_t pos = engine_.sequencer().position();
            const std::int64_t rem = pos % samplesPerBeat;
            if (rem > 64)
                startDelaySamples = static_cast<int>(samplesPerBeat - rem);
        }
    }

    previewDuration_ = static_cast<double>(audioToPlay->numFrames()) / engine_.sampleRate();
    previewProgress_ = 0.0;
    previewButton_.setIcon(IconButton::Icon::Stop);
    previewButton_.setLabel("Stop");
    previewButton_.setToggleState(true, juce::dontSendNotification);
    engine_.sequencer().triggerPreview(audioToPlay, startDelaySamples, loopPreview);
    repaint();
}

void SampleBrowserPanel::timerCallback()
{
    const bool isPlaying = engine_.sequencer().isPreviewActive();
    if (!isPlaying)
    {
        if (!currentPreviewPath_.empty() || previewButton_.getLabel() != "Preview")
        {
            currentPreviewPath_.clear();
            previewProgress_ = 0.0;
            previewButton_.setIcon(IconButton::Icon::Play);
            previewButton_.setLabel("Preview");
            previewButton_.setToggleState(false, juce::dontSendNotification);
            repaint();
        }
    }
    else
    {
        previewProgress_ = std::clamp(engine_.sequencer().previewProgress(), 0.0, 1.0);
        repaint();
    }
}

void SampleBrowserPanel::importCurrentSelection(SampleKind kind, int loopBars)
{
    const auto file = fileTree_->getSelectedFile();
    if (!file.existsAsFile())
        return;

    const auto filePath = file.getFullPathName().toStdString();
    if (!importer_.isSupportedFormat(filePath))
        return;

    auto result = importer_.importFile(filePath, engine_.sampleRate(), kind, loopBars);
    if (!result.has_value())
        return;

    const auto sampleId = song_.addSample(std::move(result->sample));
    stretchCache_.setRawAudio(sampleId, std::make_shared<const AudioData>(std::move(result->audio)));

    if (onImported_)
        onImported_(sampleId);
}

void SampleBrowserPanel::paint(juce::Graphics& g)
{
    g.fillAll(Palette::surface);

    // Top horizontal border of panel
    g.setColour(Palette::gridLine);
    g.drawHorizontalLine(0, 0.0f, static_cast<float>(getWidth()));

    // Title area background
    g.setColour(Palette::background.withAlpha(0.6f));
    g.fillRect(0, 0, getWidth(), 28);
    g.setColour(Palette::gridLine);
    g.drawHorizontalLine(28, 0.0f, static_cast<float>(getWidth()));

    // Vertical divider between fileTree and preview
    if (fileTree_ != nullptr)
    {
        g.setColour(Palette::gridLine);
        g.drawVerticalLine(fileTree_->getRight(), 28.0f, static_cast<float>(getHeight()));
    }

    // Waveform box
    if (!waveformRect_.isEmpty())
    {
        g.setColour(Palette::background);
        g.fillRoundedRectangle(waveformRect_.toFloat(), 4.0f);
        g.setColour(Palette::gridLine.withAlpha(0.8f));
        g.drawRoundedRectangle(waveformRect_.toFloat(), 4.0f, 1.0f);

        if (selectedAudio_ != nullptr && selectedAudio_->numFrames() > 0)
        {
            const float cy = static_cast<float>(waveformRect_.getCentreY());
            const float maxAmp = static_cast<float>(waveformRect_.getHeight() - 8) * 0.5f;
            const int w = waveformRect_.getWidth() - 4;
            const int bx = waveformRect_.getX() + 2;
            const std::int64_t totalFrames = selectedAudio_->numFrames();
            const float* audioL = selectedAudio_->left();
            const float* audioR = selectedAudio_->right();

            // Center horizontal zero line
            g.setColour(Palette::accent.withAlpha(0.25f));
            g.drawHorizontalLine(static_cast<int>(cy), static_cast<float>(bx), static_cast<float>(bx + w));

            g.saveState();
            g.reduceClipRegion(waveformRect_.reduced(2, 2));
            g.setColour(Palette::accent.brighter(0.25f));

            for (int px = 0; px < w; ++px)
            {
                const std::int64_t f0 = (static_cast<std::int64_t>(px) * totalFrames) / w;
                const std::int64_t f1 =
                    std::min<std::int64_t>(totalFrames, (static_cast<std::int64_t>(px + 1) * totalFrames) / w);

                float minVal = 0.0f;
                float maxVal = 0.0f;

                if (f1 > f0)
                {
                    const std::int64_t stepSize = std::max<std::int64_t>(1, (f1 - f0) / 32);
                    for (std::int64_t f = f0; f < f1; f += stepSize)
                    {
                        const float s = 0.5f * (audioL[f] + audioR[f]);
                        minVal = std::min(minVal, s);
                        maxVal = std::max(maxVal, s);
                    }
                }

                const float y1 = cy - maxVal * maxAmp;
                const float y2 = cy - minVal * maxAmp;
                g.drawVerticalLine(bx + px, std::min(y1, y2), std::max(y1, y2) + 1.0f);
            }

            // Animated playhead during preview
            if (engine_.sequencer().isPreviewActive() && previewDuration_ > 0.0)
            {
                const float playheadX =
                    static_cast<float>(bx) + static_cast<float>(w) * static_cast<float>(previewProgress_);
                g.setColour(juce::Colours::white);
                g.drawVerticalLine(static_cast<int>(playheadX), static_cast<float>(waveformRect_.getY()),
                                   static_cast<float>(waveformRect_.getBottom()));
            }

            g.restoreState();
        }
        else
        {
            g.setColour(Palette::textDim.withAlpha(0.6f));
            g.setFont(Typography::regular(12.0f));
            g.drawText("Select an audio file (.wav, .mp3) to preview", waveformRect_, juce::Justification::centred,
                       false);
        }
    }
}

void SampleBrowserPanel::resized()
{
    // Top bar: [EXPLORER] [Drive Selector] [Up] [...] [Path Label]
    titleLabel_.setBounds(10, 2, 70, 24);
    driveSelector_.setBounds(85, 2, 175, 24);
    upFolderButton_.setBounds(264, 2, 26, 24);
    browseFolderButton_.setBounds(294, 2, 26, 24);
    pathLabel_.setBounds(326, 2, std::max(50, getWidth() - 336), 24);

    const int contentY = 28;
    const int contentH = getHeight() - contentY;
    constexpr int kTreeWidth = 280;

    fileTree_->setBounds(0, contentY, kTreeWidth, contentH);

    // Right pane
    auto rightArea = juce::Rectangle<int>(kTreeWidth + 10, contentY + 6, getWidth() - kTreeWidth - 20, contentH - 12);

    fileNameLabel_.setBounds(rightArea.removeFromTop(20));
    fileDetailsLabel_.setBounds(rightArea.removeFromTop(18));
    rightArea.removeFromTop(4);

    auto controlBar = rightArea.removeFromBottom(46);
    previewButton_.setBounds(controlBar.removeFromLeft(74).reduced(0, 4));
    controlBar.removeFromLeft(6);
    autoPreviewToggle_.setBounds(controlBar.removeFromLeft(102).reduced(0, 8));
    controlBar.removeFromLeft(6);
    syncPreviewToggle_.setBounds(controlBar.removeFromLeft(136).reduced(0, 8));
    controlBar.removeFromLeft(10);
    addLoopButton_.setBounds(controlBar.removeFromLeft(112).reduced(0, 4));
    controlBar.removeFromLeft(6);
    addOneShotButton_.setBounds(controlBar.removeFromLeft(126).reduced(0, 4));
    controlBar.removeFromLeft(6);
    addDiskButton_.setBounds(controlBar.removeFromLeft(112).reduced(0, 4));

    waveformRect_ = rightArea.reduced(0, 2);
}

} // namespace trami
