#include "ui/SampleBrowserPanel.h"

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
    fileTree_->setColour(juce::FileTreeComponent::backgroundColourId, Palette::background);
    fileTree_->setColour(juce::DirectoryContentsDisplayComponent::textColourId, Palette::text);
    addAndMakeVisible(fileTree_.get());

    titleLabel_.setFont(Typography::bold(12.5f));
    titleLabel_.setColour(juce::Label::textColourId, Palette::text);
    titleLabel_.setText("EXPLORER", juce::dontSendNotification);
    addAndMakeVisible(titleLabel_);

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
    addAndMakeVisible(autoPreviewToggle_);

    previewButton_.onClick = [this]() { previewCurrentSelection(); };
    addAndMakeVisible(previewButton_);

    startTimerHz(20);
}

SampleBrowserPanel::~SampleBrowserPanel()
{
    stopTimer();
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

            // Load audio data for live waveform preview
            auto audioOpt = importer_.loadAudioData(filePath, engine_.sampleRate());
            if (audioOpt.has_value())
            {
                selectedAudio_ = std::make_shared<const AudioData>(std::move(*audioOpt));
                previewDuration_ = static_cast<double>(selectedAudio_->numFrames()) / engine_.sampleRate();
                fileDetailsLabel_.setText(
                    juce::String(previewDuration_, 2) + "s  |  " +
                    juce::String(static_cast<int>(engine_.sampleRate())) + " Hz  |  " +
                    (selectedAudio_->numChannels() == 2 ? "Stereo" : "Mono") +
                    "  |  Drag onto any track above",
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
    if (file.existsAsFile() && importer_.isSupportedFormat(file.getFullPathName().toStdString()))
    {
        if (e.mods.isLeftButtonDown())
        {
            startDragging(file.getFullPathName(), this);
        }
    }
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

    if (selectedAudio_ != nullptr && selectedAudio_->numFrames() > 0 && currentPreviewPath_ == filePath)
    {
        previewProgress_ = 0.0;
        previewButton_.setIcon(IconButton::Icon::Stop);
        previewButton_.setLabel("Stop");
        previewButton_.setToggleState(true, juce::dontSendNotification);
        engine_.sequencer().triggerPreview(selectedAudio_);
        return;
    }

    auto audioOpt = importer_.loadAudioData(filePath, engine_.sampleRate());
    if (audioOpt.has_value())
    {
        currentPreviewPath_ = filePath;
        selectedAudio_ = std::make_shared<const AudioData>(std::move(*audioOpt));
        previewDuration_ = static_cast<double>(selectedAudio_->numFrames()) / engine_.sampleRate();
        previewProgress_ = 0.0;
        previewButton_.setIcon(IconButton::Icon::Stop);
        previewButton_.setLabel("Stop");
        previewButton_.setToggleState(true, juce::dontSendNotification);
        engine_.sequencer().triggerPreview(selectedAudio_);
        repaint();
    }
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
    else if (previewDuration_ > 0.0)
    {
        previewProgress_ += 1.0 / (20.0 * previewDuration_);
        if (previewProgress_ > 1.0)
            previewProgress_ = 1.0;
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
    // Top bar
    titleLabel_.setBounds(10, 2, 90, 24);
    pathLabel_.setBounds(105, 2, getWidth() - 115, 24);

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
    previewButton_.setBounds(controlBar.removeFromLeft(80).reduced(0, 2));
    controlBar.removeFromLeft(12);
    autoPreviewToggle_.setBounds(controlBar.removeFromLeft(120).reduced(0, 8));

    waveformRect_ = rightArea.reduced(0, 2);
}

} // namespace trami
