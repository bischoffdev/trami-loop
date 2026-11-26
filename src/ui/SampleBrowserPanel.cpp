#include "ui/SampleBrowserPanel.h"

#include "engine/TimeStretcher.h"
#include "model/Timing.h"
#include "ui/TramiLookAndFeel.h"

#include <algorithm>
#include <cmath>

namespace trami
{

// ============================================================================
// Resizable vertical splitter between Folder Tree and Audio File List
// ============================================================================
class SampleBrowserPanel::FolderTreeSplitter final : public juce::Component
{
public:
    FolderTreeSplitter()
    {
        setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
        setRepaintsOnMouseActivity(true);
    }

    std::function<void()> onDragStart;
    std::function<void(int dx)> onDragDelta;

    void paint(juce::Graphics& g) override
    {
        const bool active = isMouseOverOrDragging();
        g.fillAll(active ? Palette::accent.withAlpha(0.22f) : Palette::gridLine.withAlpha(0.65f));
        const int cx = getWidth() / 2;
        g.setColour(active ? Palette::accent : Palette::textDim.withAlpha(0.35f));
        g.drawVerticalLine(cx, 6.0f, static_cast<float>(getHeight() - 6));
    }

    void mouseDown(const juce::MouseEvent&) override
    {
        if (onDragStart)
            onDragStart();
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (onDragDelta)
            onDragDelta(e.getDistanceFromDragStartX());
    }
};

// ============================================================================
// Sonic Foundry ACID-style multi-column audio file list (flows top-to-bottom,
// then left-to-right across columns with horizontal scrolling)
// ============================================================================
class SampleBrowserPanel::AudioFileListView final : public juce::Component
{
public:
    AudioFileListView()
    {
        setWantsKeyboardFocus(true);
    }

    std::function<void(const juce::File&)> onFileSelected;
    std::function<void(const juce::File&)> onFileDoubleClicked;
    std::function<void(const juce::File&)> onFileDragStart;

    void setFiles(std::vector<juce::File> files)
    {
        files_ = std::move(files);
        selectedIndex_ = -1;
        hoveredIndex_ = -1;
        updateLayoutSize();
        repaint();
    }

    void setViewportSize(int viewW, int viewH)
    {
        if (viewW_ != viewW || viewH_ != viewH)
        {
            viewW_ = std::max(1, viewW);
            viewH_ = std::max(1, viewH);
            updateLayoutSize();
            repaint();
        }
    }

    juce::File selectedFile() const
    {
        if (selectedIndex_ >= 0 && selectedIndex_ < static_cast<int>(files_.size()))
            return files_[static_cast<std::size_t>(selectedIndex_)];
        return {};
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(Palette::background);

        if (files_.empty())
        {
            g.setColour(Palette::textDim.withAlpha(0.65f));
            g.setFont(Typography::regular(12.0f));
            g.drawText("No audio files in this folder (.wav, .mp3, .aiff, .flac, .ogg)",
                       getLocalBounds().withWidth(std::max(viewW_, getWidth())).reduced(12),
                       juce::Justification::centred, false);
            return;
        }

        const int rowsPerCol = std::max(1, rowsPerColumn());
        const auto clip = g.getClipBounds();

        for (int i = 0; i < static_cast<int>(files_.size()); ++i)
        {
            const int col = i / rowsPerCol;
            const int row = i % rowsPerCol;
            const juce::Rectangle<int> cell(kPadX + col * kColWidth, kPadY + row * kRowHeight,
                                            kColWidth - 6, kRowHeight);
            if (!cell.intersects(clip))
                continue;

            const bool isSelected = (i == selectedIndex_);
            const bool isHovered = (i == hoveredIndex_);

            if (isSelected)
            {
                g.setColour(Palette::accent.withAlpha(0.28f));
                g.fillRoundedRectangle(cell.toFloat(), 3.0f);
                g.setColour(Palette::accent.withAlpha(0.85f));
                g.drawRoundedRectangle(cell.toFloat().reduced(0.5f), 3.0f, 1.0f);
            }
            else if (isHovered)
            {
                g.setColour(juce::Colours::white.withAlpha(0.07f));
                g.fillRoundedRectangle(cell.toFloat(), 3.0f);
            }

            const auto& entry = files_[static_cast<std::size_t>(i)];
            const bool isDir = entry.isDirectory();

            // Draw folder icon for subfolders or classic ACID-inspired audio-file speaker icon
            if (isDir)
                drawFolderIcon(g, cell.getX() + 4, cell.getCentreY(), isSelected);
            else
                drawAudioFileIcon(g, cell.getX() + 4, cell.getCentreY(), isSelected);

            g.setColour(isSelected ? juce::Colours::white : (isDir ? juce::Colour(0xfffde68a) : Palette::text));
            g.setFont((isSelected || isDir) ? Typography::bold(14.0f) : Typography::regular(14.0f));
            g.drawText(entry.getFileName(),
                       cell.withTrimmedLeft(22).withTrimmedRight(4),
                       juce::Justification::centredLeft, true);
        }
    }

    void mouseMove(const juce::MouseEvent& e) override
    {
        const int idx = hitTestIndex(e.getPosition());
        if (idx != hoveredIndex_)
        {
            hoveredIndex_ = idx;
            repaint();
        }
    }

    void mouseExit(const juce::MouseEvent&) override
    {
        if (hoveredIndex_ != -1)
        {
            hoveredIndex_ = -1;
            repaint();
        }
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        grabKeyboardFocus();
        dragStarted_ = false;
        const int idx = hitTestIndex(e.getPosition());
        if (idx >= 0 && idx < static_cast<int>(files_.size()))
        {
            selectIndex(idx);
        }
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (!dragStarted_ && e.getDistanceFromDragStart() > 6)
        {
            const auto file = selectedFile();
            if (file.existsAsFile() && onFileDragStart)
            {
                dragStarted_ = true;
                onFileDragStart(file);
            }
        }
    }

    void mouseDoubleClick(const juce::MouseEvent& e) override
    {
        const int idx = hitTestIndex(e.getPosition());
        if (idx >= 0 && idx < static_cast<int>(files_.size()))
        {
            const juce::File chosen = files_[static_cast<std::size_t>(idx)];
            selectIndex(idx);
            if (onFileDoubleClicked)
                onFileDoubleClicked(chosen);
        }
    }

    bool keyPressed(const juce::KeyPress& key) override
    {
        if (files_.empty())
            return false;

        const int rowsPerCol = std::max(1, rowsPerColumn());
        const int count = static_cast<int>(files_.size());
        int next = selectedIndex_;

        if (key == juce::KeyPress::downKey)
            next = (selectedIndex_ < 0) ? 0 : std::min(count - 1, selectedIndex_ + 1);
        else if (key == juce::KeyPress::upKey)
            next = (selectedIndex_ < 0) ? 0 : std::max(0, selectedIndex_ - 1);
        else if (key == juce::KeyPress::rightKey)
            next = (selectedIndex_ < 0) ? 0 : std::min(count - 1, selectedIndex_ + rowsPerCol);
        else if (key == juce::KeyPress::leftKey)
            next = (selectedIndex_ < 0) ? 0 : std::max(0, selectedIndex_ - rowsPerCol);
        else if (key == juce::KeyPress::returnKey || key == juce::KeyPress::spaceKey)
        {
            if (selectedIndex_ >= 0 && selectedIndex_ < count && onFileDoubleClicked)
            {
                const juce::File chosen = files_[static_cast<std::size_t>(selectedIndex_)];
                onFileDoubleClicked(chosen);
            }
            return true;
        }
        else
        {
            return false;
        }

        if (next != selectedIndex_)
        {
            selectIndex(next);
            scrollSelectionIntoView();
        }
        return true;
    }

private:
    static constexpr int kRowHeight = 24;
    static constexpr int kColWidth = 240;
    static constexpr int kPadX = 6;
    static constexpr int kPadY = 4;

    int rowsPerColumn() const
    {
        return std::max(1, (viewH_ - kPadY * 2) / kRowHeight);
    }

    void updateLayoutSize()
    {
        const int rowsPerCol = rowsPerColumn();
        const int numCols = files_.empty() ? 1 : (static_cast<int>(files_.size()) + rowsPerCol - 1) / rowsPerCol;
        const int contentW = std::max(viewW_, kPadX * 2 + numCols * kColWidth);
        const int contentH = std::max(viewH_, kPadY * 2 + rowsPerCol * kRowHeight);
        setSize(contentW, contentH);
    }

    int hitTestIndex(juce::Point<int> pt) const
    {
        if (files_.empty() || pt.x < kPadX || pt.y < kPadY)
            return -1;

        const int rowsPerCol = rowsPerColumn();
        const int col = (pt.x - kPadX) / kColWidth;
        const int row = (pt.y - kPadY) / kRowHeight;
        if (row < 0 || row >= rowsPerCol || col < 0)
            return -1;

        const int idx = col * rowsPerCol + row;
        return (idx >= 0 && idx < static_cast<int>(files_.size())) ? idx : -1;
    }

    void selectIndex(int idx)
    {
        selectedIndex_ = idx;
        repaint();
        if (idx >= 0 && idx < static_cast<int>(files_.size()) && onFileSelected)
        {
            const juce::File chosen = files_[static_cast<std::size_t>(idx)];
            onFileSelected(chosen);
        }
    }

    void scrollSelectionIntoView()
    {
        if (selectedIndex_ < 0)
            return;
        auto* vp = findParentComponentOfClass<juce::Viewport>();
        if (vp == nullptr)
            return;

        const int rowsPerCol = rowsPerColumn();
        const int col = selectedIndex_ / rowsPerCol;
        const int xL = kPadX + col * kColWidth;
        const int xR = xL + kColWidth;
        const int curX = vp->getViewPositionX();
        const int visW = vp->getViewWidth();

        if (xL < curX)
            vp->setViewPosition(xL, vp->getViewPositionY());
        else if (xR > curX + visW)
            vp->setViewPosition(std::max(0, xR - visW), vp->getViewPositionY());
    }

    static void drawFolderIcon(juce::Graphics& g, int x, int cy, bool selected)
    {
        const float fx = static_cast<float>(x);
        const float fy = static_cast<float>(cy - 5);

        // Folder tab
        g.setColour(selected ? Palette::accent : juce::Colour(0xffd97706));
        g.fillRoundedRectangle(fx, fy - 1.0f, 6.0f, 3.0f, 1.0f);

        // Folder body
        g.setColour(selected ? Palette::accent.brighter(0.25f) : juce::Colour(0xfff59e0b));
        g.fillRoundedRectangle(fx, fy + 1.0f, 13.0f, 9.0f, 1.5f);
        g.setColour(juce::Colours::black.withAlpha(0.3f));
        g.drawRoundedRectangle(fx, fy + 1.0f, 13.0f, 9.0f, 1.5f, 0.8f);
    }

    static void drawAudioFileIcon(juce::Graphics& g, int x, int cy, bool selected)
    {
        // Mini document + speaker cone icon (reminiscent of ACID Explorer)
        const float fx = static_cast<float>(x);
        const float fy = static_cast<float>(cy - 6);

        // Document page outline
        g.setColour(selected ? juce::Colour(0xffcbd5e1) : juce::Colour(0xff64748b));
        g.drawRect(fx + 3.0f, fy, 10.0f, 12.0f, 1.0f);

        // Speaker triangle on left overlapping page
        juce::Path speaker;
        speaker.startNewSubPath(fx, static_cast<float>(cy - 2));
        speaker.lineTo(fx + 3.0f, static_cast<float>(cy - 2));
        speaker.lineTo(fx + 6.5f, static_cast<float>(cy - 5));
        speaker.lineTo(fx + 6.5f, static_cast<float>(cy + 5));
        speaker.lineTo(fx + 3.0f, static_cast<float>(cy + 2));
        speaker.lineTo(fx, static_cast<float>(cy + 2));
        speaker.closeSubPath();

        g.setColour(selected ? Palette::accent : juce::Colour(0xffeab308));
        g.fillPath(speaker);

        // Tiny sound wave bars inside document
        g.setColour(selected ? juce::Colours::white.withAlpha(0.85f) : Palette::accent.withAlpha(0.75f));
        g.drawVerticalLine(x + 9, static_cast<float>(cy - 2), static_cast<float>(cy + 2));
        g.drawVerticalLine(x + 11, static_cast<float>(cy - 4), static_cast<float>(cy + 4));
    }

    std::vector<juce::File> files_;
    int selectedIndex_ = -1;
    int hoveredIndex_ = -1;
    int viewW_ = 400;
    int viewH_ = 140;
    bool dragStarted_ = false;
};

// ============================================================================
// SampleBrowserPanel
// ============================================================================
SampleBrowserPanel::SampleBrowserPanel(Song& song, AudioEngine& engine, StretchCache& stretchCache,
                                       SampleImportedCallback onImported)
    : song_(song), engine_(engine), stretchCache_(stretchCache), onImported_(std::move(onImported))
{
    formatThread_.startThread(juce::Thread::Priority::normal);

    auto root = juce::File::getSpecialLocation(juce::File::userHomeDirectory);
    const juce::File demoRoot("/tmp/TramiLoop Samples");
    if (demoRoot.isDirectory())
        root = demoRoot;
    currentDirectory_ = root;

    // Left pane: Folders-only hierarchical tree
    directoryList_ = std::make_unique<juce::DirectoryContentsList>(&dirFilter_, formatThread_);
    directoryList_->setDirectory(root, true, false);

    folderTree_ = std::make_unique<juce::FileTreeComponent>(*directoryList_);
    folderTree_->addListener(this);
    folderTree_->setColour(juce::FileTreeComponent::backgroundColourId, Palette::background);
    folderTree_->setColour(juce::DirectoryContentsDisplayComponent::textColourId, Palette::text);
    folderTree_->setIndentSize(14);
    addAndMakeVisible(folderTree_.get());

    // Splitter between folder tree and file list
    paneSplitter_ = std::make_unique<FolderTreeSplitter>();
    auto startW = std::make_shared<int>(treeWidth_);
    paneSplitter_->onDragStart = [this, startW]() { *startW = treeWidth_; };
    paneSplitter_->onDragDelta = [this, startW](int dx)
    {
        const int maxW = std::max(140, getWidth() - 240);
        treeWidth_ = std::clamp(*startW + dx, 130, maxW);
        resized();
    };
    addAndMakeVisible(paneSplitter_.get());

    // Right pane: Multi-column audio file list
    fileListView_ = std::make_unique<AudioFileListView>();
    fileListView_->onFileSelected = [this](const juce::File& file) { onAudioFileSelected(file); };
    fileListView_->onFileDoubleClicked = [this](const juce::File& file) { onAudioFileDoubleClicked(file); };
    fileListView_->onFileDragStart = [this](const juce::File& file) { startSampleDrag(file); };

    fileListViewport_ = std::make_unique<juce::Viewport>();
    fileListViewport_->setViewedComponent(fileListView_.get(), false);
    fileListViewport_->setScrollBarsShown(false, true, false, true);
    addAndMakeVisible(fileListViewport_.get());

    // Top bar controls
    refreshDrives();
    driveSelector_.setTooltip("Choose Drive or Folder (e.g. CD/DVD, USB, External Drives)");
    driveSelector_.onChange = [this]()
    {
        const int selectedId = driveSelector_.getSelectedId();
        if (selectedId == 9999)
        {
            folderChooser_ = std::make_unique<juce::FileChooser>(
                "Choose Directory or External Drive",
                currentDirectory_.exists() ? currentDirectory_ : juce::File::getSpecialLocation(juce::File::userHomeDirectory),
                "*", true);

            auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories;
            folderChooser_->launchAsync(flags, [this](const juce::FileChooser& fc)
            {
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
    upFolderButton_.onClick = [this]()
    {
        const auto current = currentDirectory_.exists() ? currentDirectory_ : directoryList_->getDirectory();
        const auto parent = current.getParentDirectory();
        if (parent != current && parent.exists())
            selectDirectory(parent);
    };
    addAndMakeVisible(upFolderButton_);

    browseFolderButton_.setTooltip("Browse custom directory or drive...");
    browseFolderButton_.onClick = [this]()
    {
        folderChooser_ = std::make_unique<juce::FileChooser>(
            "Choose Directory or External Drive",
            currentDirectory_.exists() ? currentDirectory_ : juce::File::getSpecialLocation(juce::File::userHomeDirectory),
            "*", true);

        auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories;
        folderChooser_->launchAsync(flags, [this](const juce::FileChooser& fc)
        {
            const auto chosen = fc.getResult();
            if (chosen.exists())
                selectDirectory(chosen);
        });
    };
    addAndMakeVisible(browseFolderButton_);

    pathLabel_.setFont(Typography::regular(11.0f));
    pathLabel_.setColour(juce::Label::textColourId, Palette::textDim);
    pathLabel_.setText(root.getFullPathName(), juce::dontSendNotification);
    addAndMakeVisible(pathLabel_);

    // Bottom audition strip controls
    fileNameLabel_.setFont(Typography::bold(12.0f));
    fileNameLabel_.setColour(juce::Label::textColourId, Palette::text);
    fileNameLabel_.setText("No file selected", juce::dontSendNotification);
    addAndMakeVisible(fileNameLabel_);

    fileDetailsLabel_.setFont(Typography::regular(10.5f));
    fileDetailsLabel_.setColour(juce::Label::textColourId, Palette::textDim);
    fileDetailsLabel_.setText("Drag audio files onto any track above", juce::dontSendNotification);
    addAndMakeVisible(fileDetailsLabel_);

    autoPreviewToggle_.setColour(juce::ToggleButton::textColourId, Palette::textDim);
    autoPreviewToggle_.setTooltip("Automatically play samples on selection");
    addAndMakeVisible(autoPreviewToggle_);

    syncPreviewToggle_.setColour(juce::ToggleButton::textColourId, Palette::accent);
    syncPreviewToggle_.setToggleState(true, juce::dontSendNotification);
    syncPreviewToggle_.setTooltip("In-Sync Auditioning: Time-stretch loops to project BPM, conform to project Key, and beat-sync preview when playing");
    addAndMakeVisible(syncPreviewToggle_);

    previewButton_.setTooltip("Preview selected sample (Space)");
    previewButton_.onClick = [this]() { previewCurrentSelection(); };
    addAndMakeVisible(previewButton_);

    addLoopButton_.setTooltip("Add as Loop to New Track");
    addLoopButton_.onClick = [this]()
    {
        if (selectedFile_.existsAsFile() && importer_.isSupportedFormat(selectedFile_.getFullPathName().toStdString()))
        {
            if (onAddToNewTrack_)
                onAddToNewTrack_(selectedFile_.getFullPathName(), SampleKind::Loop);
            else
                importCurrentSelection(SampleKind::Loop);
        }
    };
    addAndMakeVisible(addLoopButton_);

    addOneShotButton_.setTooltip("Add as One-Shot to New Track");
    addOneShotButton_.onClick = [this]()
    {
        if (selectedFile_.existsAsFile() && importer_.isSupportedFormat(selectedFile_.getFullPathName().toStdString()))
        {
            if (onAddToNewTrack_)
                onAddToNewTrack_(selectedFile_.getFullPathName(), SampleKind::OneShot);
            else
                importCurrentSelection(SampleKind::OneShot);
        }
    };
    addAndMakeVisible(addOneShotButton_);

    addDiskButton_.setTooltip("Add as Disk-Based Stream Track (for long audio files)");
    addDiskButton_.onClick = [this]()
    {
        if (selectedFile_.existsAsFile() && importer_.isSupportedFormat(selectedFile_.getFullPathName().toStdString()))
        {
            if (onAddToNewTrack_)
                onAddToNewTrack_(selectedFile_.getFullPathName(), SampleKind::DiskStream);
            else
                importCurrentSelection(SampleKind::DiskStream);
        }
    };
    addAndMakeVisible(addDiskButton_);

    populateFilesForDirectory(root);
    startTimerHz(20);
}

SampleBrowserPanel::~SampleBrowserPanel()
{
    stopTimer();
    if (folderTree_ != nullptr)
        folderTree_->removeListener(this);
    formatThread_.stopThread(1000);
}

void SampleBrowserPanel::populateFilesForDirectory(juce::File dir)
{
    if (!dir.isDirectory())
        return;

    currentDirectory_ = dir;
    pathLabel_.setText(dir.getFullPathName(), juce::dontSendNotification);

    std::vector<juce::File> subDirs;
    for (const auto& entry : juce::RangedDirectoryIterator(dir, false, "*", juce::File::findDirectories))
    {
        const auto d = entry.getFile();
        if (!d.isHidden() && !d.getFileName().startsWithChar('.'))
            subDirs.push_back(d);
    }
    std::sort(subDirs.begin(), subDirs.end(),
              [](const juce::File& a, const juce::File& b)
              {
                  return a.getFileName().compareNatural(b.getFileName()) < 0;
              });

    std::vector<juce::File> audioFiles;
    for (const auto& entry : juce::RangedDirectoryIterator(dir, false, "*", juce::File::findFiles))
    {
        const auto f = entry.getFile();
        if (!f.isHidden() && importer_.isSupportedFormat(f.getFullPathName().toStdString()))
            audioFiles.push_back(f);
    }

    std::sort(audioFiles.begin(), audioFiles.end(),
              [](const juce::File& a, const juce::File& b)
              {
                  return a.getFileName().compareNatural(b.getFileName()) < 0;
              });

    std::vector<juce::File> combined;
    combined.reserve(subDirs.size() + audioFiles.size());
    combined.insert(combined.end(), subDirs.begin(), subDirs.end());
    combined.insert(combined.end(), audioFiles.begin(), audioFiles.end());

    fileListView_->setFiles(std::move(combined));
    fileListViewport_->setViewPosition(0, 0);

    if (dir.getFullPathName().startsWith("/tmp/TramiLoop Samples") && !audioFiles.empty())
    {
        onAudioFileSelected(audioFiles.front());
        return;
    }

    selectedFile_ = juce::File{};
    selectedAudio_.reset();
    selectedAcidMeta_.reset();
    previewDuration_ = 0.0;
    previewProgress_ = 0.0;
    fileNameLabel_.setText(dir.getFileName().isNotEmpty() ? dir.getFileName() : dir.getFullPathName(),
                           juce::dontSendNotification);
    fileDetailsLabel_.setText("Select an audio file to preview or drag onto a track", juce::dontSendNotification);
    repaint();
}

void SampleBrowserPanel::selectionChanged()
{
    const auto file = folderTree_->getSelectedFile();
    if (file.isDirectory())
        populateFilesForDirectory(file);
}

void SampleBrowserPanel::fileClicked(const juce::File& file, const juce::MouseEvent&)
{
    if (file.isDirectory())
        populateFilesForDirectory(file);
}

void SampleBrowserPanel::fileDoubleClicked(const juce::File& file)
{
    if (file.isDirectory())
        selectDirectory(file);
}

void SampleBrowserPanel::browserRootChanged(const juce::File& newRoot)
{
    if (newRoot.isDirectory())
        populateFilesForDirectory(newRoot);
}

void SampleBrowserPanel::onAudioFileSelected(juce::File file)
{
    selectedFile_ = file;
    if (file.isDirectory())
    {
        selectedAudio_.reset();
        selectedAcidMeta_.reset();
        previewDuration_ = 0.0;
        previewProgress_ = 0.0;
        pathLabel_.setText(file.getFullPathName(), juce::dontSendNotification);
        fileNameLabel_.setText(file.getFileName(), juce::dontSendNotification);
        fileDetailsLabel_.setText("Folder - double-click or press Enter to open", juce::dontSendNotification);
        repaint();
        return;
    }
    if (file.existsAsFile() && importer_.isSupportedFormat(file.getFullPathName().toStdString()))
    {
        const auto filePath = file.getFullPathName().toStdString();
        pathLabel_.setText(file.getFullPathName(), juce::dontSendNotification);
        fileNameLabel_.setText(file.getFileName(), juce::dontSendNotification);

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
                    acidInfo = " | Disk";
                }
                else if (selectedAcidMeta_->isOneShot)
                {
                    acidInfo = " | One-Shot";
                }
                else
                {
                    const double trueBpm = (selectedAcidMeta_->numBeats > 0 && previewDuration_ > 0.0)
                        ? ((static_cast<double>(selectedAcidMeta_->numBeats) * 60.0) / previewDuration_)
                        : selectedAcidMeta_->tempo;
                    acidInfo = " | Loop (" + juce::String(selectedAcidMeta_->numBeats) + "b, " +
                               juce::String(trueBpm, 1) + " BPM";
                    if (selectedAcidMeta_->rootNote >= 0)
                        acidInfo += juce::String(", ") + Sample::pitchClassName(selectedAcidMeta_->rootNote);
                    acidInfo += ")";
                }
            }
            fileDetailsLabel_.setText(
                juce::String(previewDuration_, 2) + "s | " +
                juce::String(static_cast<int>(engine_.sampleRate())) + " Hz | " +
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
            previewCurrentSelection();
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

void SampleBrowserPanel::onAudioFileDoubleClicked(juce::File file)
{
    if (file.isDirectory())
    {
        populateFilesForDirectory(file);
        return;
    }
    if (file.existsAsFile())
    {
        selectedFile_ = file;
        previewCurrentSelection();
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

        g.setFont(Typography::bold(12.0f));
        g.drawText(juce::CharPointer_UTF8("\xe2\x99\xab"), 6, 0, 16, imgH, juce::Justification::centred);

        g.setColour(Palette::text);
        g.setFont(Typography::regular(11.5f));
        g.drawText(file.getFileName(), 24, 0, imgW - 30, imgH, juce::Justification::centredLeft, true);
    }

    const juce::Point<int> offset(-15, -13);
    ddc->startDragging(file.getFullPathName(), this, juce::ScaledImage(dragImg, 1.0), true, &offset);
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

void SampleBrowserPanel::selectDirectory(juce::File dir)
{
    if (dir.exists() && dir.isDirectory())
    {
        if (directoryList_ != nullptr)
            directoryList_->setDirectory(dir, true, false);
        populateFilesForDirectory(dir);

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
    const auto file = selectedFile_;
    if (!file.existsAsFile())
        return;

    const auto filePath = file.getFullPathName().toStdString();
    if (!importer_.isSupportedFormat(filePath))
        return;

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
            repaint(previewStripRect_);
        }
    }
    else
    {
        previewProgress_ = std::clamp(engine_.sequencer().previewProgress(), 0.0, 1.0);
        repaint(waveformRect_);
    }
}

void SampleBrowserPanel::importCurrentSelection(SampleKind kind, int loopBars)
{
    const auto file = selectedFile_;
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

    // Top navigation bar background
    g.setColour(Palette::panelBackground);
    g.fillRect(0, 0, getWidth(), 28);
    g.setColour(Palette::gridLine);
    g.drawHorizontalLine(27, 0.0f, static_cast<float>(getWidth()));

    // Bottom audition strip background
    if (!previewStripRect_.isEmpty())
    {
        g.setColour(Palette::panelHeader);
        g.fillRect(previewStripRect_);
        g.setColour(Palette::gridLine);
        g.drawHorizontalLine(previewStripRect_.getY(), 0.0f, static_cast<float>(getWidth()));
    }

    // Inline compact waveform preview
    if (!waveformRect_.isEmpty())
    {
        g.setColour(Palette::background);
        g.fillRoundedRectangle(waveformRect_.toFloat(), 3.0f);
        g.setColour(Palette::gridLine.withAlpha(0.8f));
        g.drawRoundedRectangle(waveformRect_.toFloat(), 3.0f, 1.0f);

        if (selectedAudio_ != nullptr && selectedAudio_->numFrames() > 0)
        {
            const float cy = static_cast<float>(waveformRect_.getCentreY());
            const float maxAmp = static_cast<float>(waveformRect_.getHeight() - 6) * 0.5f;
            const int w = std::max(1, waveformRect_.getWidth() - 4);
            const int bx = waveformRect_.getX() + 2;
            const std::int64_t totalFrames = selectedAudio_->numFrames();
            const float* audioL = selectedAudio_->left();
            const float* audioR = selectedAudio_->right();

            g.setColour(Palette::accent.withAlpha(0.22f));
            g.drawHorizontalLine(static_cast<int>(cy), static_cast<float>(bx), static_cast<float>(bx + w));

            g.saveState();
            g.reduceClipRegion(waveformRect_.reduced(2, 2));
            g.setColour(Palette::accent.brighter(0.2f));

            for (int px = 0; px < w; ++px)
            {
                const std::int64_t f0 = (static_cast<std::int64_t>(px) * totalFrames) / w;
                const std::int64_t f1 =
                    std::min<std::int64_t>(totalFrames, (static_cast<std::int64_t>(px + 1) * totalFrames) / w);

                float minVal = 0.0f;
                float maxVal = 0.0f;
                if (f1 > f0)
                {
                    const std::int64_t stepSize = std::max<std::int64_t>(1, (f1 - f0) / 24);
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
            g.setColour(Palette::textDim.withAlpha(0.55f));
            g.setFont(Typography::regular(10.5f));
            g.drawText("Waveform Preview", waveformRect_, juce::Justification::centred, false);
        }
    }
}

void SampleBrowserPanel::resized()
{
    constexpr int kTopBarH = 28;
    constexpr int kStripH = 40;
    constexpr int kSplitterW = 5;

    // Top bar: [Drive Selector] [Up] [Browse] [Path]  ...  [+ Loop] [+ One-Shot] [+ Disk]
    auto topBar = juce::Rectangle<int>(0, 0, getWidth(), kTopBarH).reduced(6, 2);
    driveSelector_.setBounds(topBar.removeFromLeft(165));
    topBar.removeFromLeft(4);
    upFolderButton_.setBounds(topBar.removeFromLeft(24));
    topBar.removeFromLeft(4);
    browseFolderButton_.setBounds(topBar.removeFromLeft(24));
    topBar.removeFromLeft(8);

    addDiskButton_.setBounds(topBar.removeFromRight(68));
    topBar.removeFromRight(4);
    addOneShotButton_.setBounds(topBar.removeFromRight(88));
    topBar.removeFromRight(4);
    addLoopButton_.setBounds(topBar.removeFromRight(68));
    topBar.removeFromRight(8);

    pathLabel_.setBounds(topBar);

    // Bottom compact audition strip
    const int contentH = std::max(40, getHeight() - kTopBarH - kStripH);
    previewStripRect_ = juce::Rectangle<int>(0, kTopBarH + contentH, getWidth(), kStripH);

    auto strip = previewStripRect_.reduced(8, 4);
    previewButton_.setBounds(strip.removeFromLeft(74));
    strip.removeFromLeft(6);
    autoPreviewToggle_.setBounds(strip.removeFromLeft(56));
    strip.removeFromLeft(4);
    syncPreviewToggle_.setBounds(strip.removeFromLeft(56));
    strip.removeFromLeft(8);

    auto infoCol = strip.removeFromLeft(std::min(280, std::max(140, strip.getWidth() / 2)));
    fileNameLabel_.setBounds(infoCol.removeFromTop(17));
    fileDetailsLabel_.setBounds(infoCol);
    strip.removeFromLeft(8);

    waveformRect_ = strip;

    // Middle ACID Explorer dual-pane: Folder Tree (left) | Splitter | Multi-Column File List (right)
    const int maxTreeW = std::max(130, getWidth() - 220);
    const int clampedTreeW = std::clamp(treeWidth_, 130, maxTreeW);

    folderTree_->setBounds(0, kTopBarH, clampedTreeW, contentH);
    paneSplitter_->setBounds(clampedTreeW, kTopBarH, kSplitterW, contentH);

    const int listX = clampedTreeW + kSplitterW;
    const int listW = std::max(0, getWidth() - listX);
    fileListViewport_->setBounds(listX, kTopBarH, listW, contentH);
    fileListView_->setViewportSize(std::max(1, fileListViewport_->getMaximumVisibleWidth()),
                                   std::max(1, fileListViewport_->getMaximumVisibleHeight()));
}

} // namespace trami
