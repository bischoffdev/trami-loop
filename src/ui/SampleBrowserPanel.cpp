#include "ui/SampleBrowserPanel.h"

#include "ui/TramiLookAndFeel.h"

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

    pathLabel_.setFont(juce::FontOptions(11.0f));
    pathLabel_.setColour(juce::Label::textColourId, Palette::textDim);
    pathLabel_.setText(root.getFullPathName(), juce::dontSendNotification);
    addAndMakeVisible(pathLabel_);

    previewButton_.onClick = [this]() { previewCurrentSelection(); };
    addAndMakeVisible(previewButton_);

    importOneShotButton_.onClick = [this]() { importCurrentSelection(SampleKind::OneShot, 1); };
    addAndMakeVisible(importOneShotButton_);

    importLoop1Button_.onClick = [this]() { importCurrentSelection(SampleKind::Loop, 1); };
    addAndMakeVisible(importLoop1Button_);

    importLoop2Button_.onClick = [this]() { importCurrentSelection(SampleKind::Loop, 2); };
    addAndMakeVisible(importLoop2Button_);

    importLoop4Button_.onClick = [this]() { importCurrentSelection(SampleKind::Loop, 4); };
    addAndMakeVisible(importLoop4Button_);

    startTimerHz(15);
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
        pathLabel_.setText(file.getFullPathName(), juce::dontSendNotification);
}

void SampleBrowserPanel::fileClicked(const juce::File& file, const juce::MouseEvent& e)
{
    if (file.existsAsFile() && importer_.isSupportedFormat(file.getFullPathName().toStdString()))
    {
        if (e.mods.isLeftButtonDown())
        {
            // Start drag-and-drop if user drags
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
    if (engine_.sequencer().isPreviewPlaying() && currentPreviewPath_ == filePath)
    {
        engine_.sequencer().stopPreview();
        currentPreviewPath_.clear();
        previewButton_.setButtonText("Preview");
        return;
    }

    auto audioOpt = importer_.loadAudioData(filePath, engine_.sampleRate());
    if (audioOpt.has_value())
    {
        currentPreviewPath_ = filePath;
        previewButton_.setButtonText("Stop");
        engine_.sequencer().triggerPreview(std::make_shared<const AudioData>(std::move(*audioOpt)));
    }
}

void SampleBrowserPanel::timerCallback()
{
    if (!engine_.sequencer().isPreviewPlaying())
    {
        if (!currentPreviewPath_.empty() || previewButton_.getButtonText() != "Preview")
        {
            currentPreviewPath_.clear();
            previewButton_.setButtonText("Preview");
        }
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

    g.setColour(Palette::gridLine);
    g.drawVerticalLine(getWidth() - 1, 0.0f, static_cast<float>(getHeight()));

    // Title
    g.setColour(Palette::background);
    g.fillRect(0, 0, getWidth(), 32);
    g.setColour(Palette::text);
    g.setFont(juce::FontOptions(13.0f).withStyle("Bold"));
    g.drawText("SAMPLE BROWSER", 8, 0, getWidth() - 16, 32, juce::Justification::centredLeft);
}

void SampleBrowserPanel::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop(32); // Header

    // Bottom import & preview buttons
    auto bottomArea = area.removeFromBottom(140).reduced(6);
    previewButton_.setBounds(bottomArea.removeFromTop(24));
    bottomArea.removeFromTop(4);
    importOneShotButton_.setBounds(bottomArea.removeFromTop(24));
    bottomArea.removeFromTop(4);

    auto loopRow1 = bottomArea.removeFromTop(24);
    importLoop1Button_.setBounds(loopRow1.removeFromLeft(loopRow1.getWidth() / 2 - 2));
    loopRow1.removeFromLeft(4);
    importLoop2Button_.setBounds(loopRow1);
    bottomArea.removeFromTop(4);

    importLoop4Button_.setBounds(bottomArea.removeFromTop(24));

    pathLabel_.setBounds(area.removeFromBottom(20).reduced(4, 0));
    fileTree_->setBounds(area);
}

} // namespace trami
