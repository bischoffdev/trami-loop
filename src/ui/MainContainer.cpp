#include "ui/MainContainer.h"

#include "ui/TramiLookAndFeel.h"

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

    patternView_ = std::make_unique<PatternView>(song_, engine_, stretchCache_);
    addAndMakeVisible(patternView_.get());

    arrangerView_ = std::make_unique<ArrangerView>(song_, engine_, stretchCache_, [this](PatternId patId) {
        editPattern(patId);
    });
    addChildComponent(arrangerView_.get());

    setViewMode(ViewMode::Pattern);
}

void MainContainer::setViewMode(ViewMode mode)
{
    currentMode_ = mode;

    patternModeButton_.setColour(juce::TextButton::buttonColourId,
                                 mode == ViewMode::Pattern ? Palette::accent.withAlpha(0.6f) : Palette::surface);
    arrangerModeButton_.setColour(juce::TextButton::buttonColourId,
                                  mode == ViewMode::Arranger ? Palette::accent.withAlpha(0.6f) : Palette::surface);

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

bool MainContainer::keyPressed(const juce::KeyPress& key)
{
    if (currentMode_ == ViewMode::Pattern && patternView_ != nullptr)
        return patternView_->keyPressed(key);
    if (currentMode_ == ViewMode::Arranger && arrangerView_ != nullptr)
        return arrangerView_->keyPressed(key);
    return false;
}

void MainContainer::paint(juce::Graphics& g)
{
    g.fillAll(Palette::background);

    // Top switcher bar
    g.setColour(Palette::surface.darker(0.3f));
    g.fillRect(0, 0, getWidth(), 36);
    g.setColour(Palette::gridLine);
    g.drawHorizontalLine(36, 0.0f, static_cast<float>(getWidth()));

    // Brand label
    g.setColour(Palette::accent);
    g.setFont(juce::FontOptions(15.0f).withStyle("Bold"));
    g.drawText("trami-loop", 12, 0, 100, 36, juce::Justification::centredLeft);
}

void MainContainer::resized()
{
    constexpr int kTopBarHeight = 36;
    auto topArea = juce::Rectangle<int>(0, 0, getWidth(), kTopBarHeight).reduced(8, 4);
    topArea.removeFromLeft(120); // space for brand

    patternModeButton_.setBounds(topArea.removeFromLeft(130));
    topArea.removeFromLeft(6);
    arrangerModeButton_.setBounds(topArea.removeFromLeft(130));

    const auto contentBounds = juce::Rectangle<int>(0, kTopBarHeight, getWidth(), getHeight() - kTopBarHeight);
    if (patternView_ != nullptr)
        patternView_->setBounds(contentBounds);
    if (arrangerView_ != nullptr)
        arrangerView_->setBounds(contentBounds);
}

} // namespace trami
