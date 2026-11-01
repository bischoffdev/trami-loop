#include "ui/PatternView.h"

#include "model/Timing.h"
#include "ui/TramiLookAndFeel.h"

#include <cmath>

namespace trami
{

PatternView::PatternView(Song& song, AudioEngine& engine, StretchCache& stretchCache)
    : song_(song), engine_(engine), stretchCache_(stretchCache)
{
    setWantsKeyboardFocus(true);

    if (song_.patterns().empty())
        currentPattern_ = song_.addPattern("Pattern 1", 1);
    else
        currentPattern_ = song_.patterns().front().id;

    if (!song_.samples().empty())
        activeSample_ = song_.samples().front().id;

    // Toolbar setup
    patternTitle_.setFont(juce::FontOptions(14.0f).withStyle("Bold"));
    patternTitle_.setColour(juce::Label::textColourId, Palette::text);
    addAndMakeVisible(patternTitle_);

    lengthCombo_.addItem("1 Bar", 1);
    lengthCombo_.addItem("2 Bars", 2);
    lengthCombo_.addItem("4 Bars", 4);
    lengthCombo_.addItem("8 Bars", 8);
    lengthCombo_.onChange = [this]() {
        const int bars = lengthCombo_.getSelectedId();
        if (bars > 0 && song_.setPatternLength(currentPattern_, bars))
        {
            republish();
            resized();
        }
    };
    addAndMakeVisible(lengthCombo_);

    sampleCombo_.onChange = [this]() {
        const int idx = sampleCombo_.getSelectedId();
        if (idx > 0 && idx <= static_cast<int>(song_.samples().size()))
            activeSample_ = song_.samples()[static_cast<std::size_t>(idx - 1)].id;
    };
    addAndMakeVisible(sampleCombo_);

    playButton_.onClick = [this]() {
        auto& seq = engine_.sequencer();
        seq.requestPlaying(!seq.isPlaying());
    };
    addAndMakeVisible(playButton_);

    rewindButton_.onClick = [this]() { engine_.sequencer().requestRewind(); };
    addAndMakeVisible(rewindButton_);

    metroToggle_.setColour(juce::ToggleButton::textColourId, Palette::textDim);
    metroToggle_.onClick = [this]() { engine_.sequencer().setMetronomeEnabled(metroToggle_.getToggleState()); };
    addAndMakeVisible(metroToggle_);

    swingToggle_.setColour(juce::ToggleButton::textColourId, Palette::textDim);
    swingToggle_.onClick = [this]() {
        song_.setSwing(swingToggle_.getToggleState() ? 0.5 : 0.0);
        republish();
    };
    addAndMakeVisible(swingToggle_);

    tempoLabel_.setFont(juce::FontOptions(13.0f));
    tempoLabel_.setColour(juce::Label::textColourId, Palette::text);
    tempoLabel_.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(tempoLabel_);

    tempoDown_.onClick = [this]() {
        song_.setBpm(song_.bpm() - 1.0);
        republish();
    };
    addAndMakeVisible(tempoDown_);

    tempoUp_.onClick = [this]() {
        song_.setBpm(song_.bpm() + 1.0);
        republish();
    };
    addAndMakeVisible(tempoUp_);

    // Grid & Track headers
    trackHeaders_ = std::make_unique<TrackHeaderList>(song_, currentPattern_, [this](int, float, float) {
        republish();
    });
    addAndMakeVisible(trackHeaders_.get());

    grid_ = std::make_unique<PatternGrid>(song_, currentPattern_, *this);

    gridViewport_ = std::make_unique<juce::Viewport>();
    gridViewport_->setViewedComponent(grid_.get(), false);
    gridViewport_->setScrollBarsShown(true, true);
    addAndMakeVisible(gridViewport_.get());

    updateLengthSelector();
    updateSampleSelector();
    republish();
    startTimerHz(30);
}

PatternView::~PatternView()
{
    stopTimer();
}

void PatternView::setPattern(PatternId patternId)
{
    currentPattern_ = patternId;
    trackHeaders_->setPattern(patternId);
    grid_->setPattern(patternId);
    updateLengthSelector();
    republish();
    resized();
}

void PatternView::updateLengthSelector()
{
    const Pattern* p = song_.findPattern(currentPattern_);
    if (p != nullptr)
    {
        patternTitle_.setText(p->name, juce::dontSendNotification);
        lengthCombo_.setSelectedId(p->lengthBars, juce::dontSendNotification);
    }
}

void PatternView::updateSampleSelector()
{
    sampleCombo_.clear(juce::dontSendNotification);
    int selectedId = 1;
    int idx = 1;

    for (const auto& sample : song_.samples())
    {
        const juce::String tag = (sample.kind == SampleKind::Loop) ? "[L] " : "[1] ";
        sampleCombo_.addItem(tag + sample.name, idx);
        if (sample.id == activeSample_)
            selectedId = idx;
        ++idx;
    }

    if (!song_.samples().empty())
        sampleCombo_.setSelectedId(selectedId, juce::dontSendNotification);
}

void PatternView::republish()
{
    tempoLabel_.setText(juce::String(song_.bpm(), 0) + " BPM", juce::dontSendNotification);
    swingToggle_.setToggleState(song_.swing() > 0.0, juce::dontSendNotification);
    metroToggle_.setToggleState(engine_.sequencer().isMetronomeEnabled(), juce::dontSendNotification);

    stretchCache_.requestUpdateAsync(song_, engine_.sampleRate(), [this](SampleBank newBank) {
        juce::MessageManager::callAsync([this, bank = std::move(newBank)]() mutable {
            activeBank_ = std::move(bank);
            engine_.publish(SnapshotBuilder::buildPattern(song_, currentPattern_, activeBank_, engine_.sampleRate()));
            if (grid_ != nullptr)
                grid_->repaint();
        });
    });
}

void PatternView::clipClicked(ClipId)
{
}

void PatternView::clipMoved(ClipId clipId, int newTrack, int newStep)
{
    if (song_.moveClip(currentPattern_, clipId, newTrack, newStep))
    {
        republish();
        grid_->repaint();
    }
}

void PatternView::clipDeleteRequested(ClipId clipId)
{
    if (song_.removeClip(currentPattern_, clipId))
    {
        republish();
        grid_->repaint();
    }
}

void PatternView::gridCellClicked(int track, int step)
{
    if (!activeSample_.isValid())
    {
        if (!song_.samples().empty())
            activeSample_ = song_.samples().front().id;
        else
            return;
    }

    if (song_.placeClip(currentPattern_, track, activeSample_, step))
    {
        republish();
        grid_->repaint();
    }
}

bool PatternView::keyPressed(const juce::KeyPress& key)
{
    auto& seq = engine_.sequencer();

    if (key == juce::KeyPress::spaceKey)
    {
        seq.requestPlaying(!seq.isPlaying());
        return true;
    }
    if (key == juce::KeyPress::returnKey)
    {
        seq.requestRewind();
        return true;
    }
    return false;
}

void PatternView::timerCallback()
{
    auto& seq = engine_.sequencer();
    playButton_.setButtonText(seq.isPlaying() ? "Stop" : "Play");

    const Pattern* p = song_.findPattern(currentPattern_);
    if (p != nullptr && p->lengthBars > 0)
    {
        const Timing timing(song_.bpm(), engine_.sampleRate());
        const double samplesPerStep = timing.samplesPerStep();
        const std::int64_t totalSamples = static_cast<std::int64_t>(timing.samplesPerBar() * p->lengthBars);

        if (totalSamples > 0 && samplesPerStep > 0.0)
        {
            const std::int64_t pos = seq.position() % totalSamples;
            const double currentStep = static_cast<double>(pos) / samplesPerStep;
            grid_->setPlayheadStep(seq.isPlaying() ? currentStep : -1.0);
        }
    }
}

void PatternView::paint(juce::Graphics& g)
{
    g.fillAll(Palette::background);

    // Toolbar background
    g.setColour(Palette::surface);
    g.fillRect(0, 0, getWidth(), 40);
    g.setColour(Palette::gridLine);
    g.drawHorizontalLine(40, 0.0f, static_cast<float>(getWidth()));
}

void PatternView::resized()
{
    constexpr int kToolbarHeight = 40;
    auto toolbar = juce::Rectangle<int>(0, 0, getWidth(), kToolbarHeight).reduced(8, 6);

    patternTitle_.setBounds(toolbar.removeFromLeft(100));
    toolbar.removeFromLeft(8);
    lengthCombo_.setBounds(toolbar.removeFromLeft(80));
    toolbar.removeFromLeft(12);

    sampleCombo_.setBounds(toolbar.removeFromLeft(160));
    toolbar.removeFromLeft(16);

    rewindButton_.setBounds(toolbar.removeFromLeft(36));
    toolbar.removeFromLeft(4);
    playButton_.setBounds(toolbar.removeFromLeft(54));
    toolbar.removeFromLeft(12);

    metroToggle_.setBounds(toolbar.removeFromLeft(60));
    toolbar.removeFromLeft(8);
    swingToggle_.setBounds(toolbar.removeFromLeft(64));
    toolbar.removeFromLeft(16);

    tempoDown_.setBounds(toolbar.removeFromLeft(28));
    tempoLabel_.setBounds(toolbar.removeFromLeft(64));
    tempoUp_.setBounds(toolbar.removeFromLeft(28));

    // Content area below toolbar
    const int contentY = kToolbarHeight;
    const int contentH = getHeight() - contentY;

    trackHeaders_->setBounds(0, contentY, TrackHeaderList::kHeaderWidth, contentH);

    const int gridX = TrackHeaderList::kHeaderWidth;
    const int gridW = getWidth() - gridX;
    gridViewport_->setBounds(gridX, contentY, gridW, contentH);

    const Pattern* p = song_.findPattern(currentPattern_);
    const int steps = p != nullptr ? p->lengthSteps() : 16;
    const int neededGridWidth = steps * PatternGrid::kStepWidth;
    const int neededGridHeight = PatternGrid::kRulerHeight + kTracksPerPattern * PatternGrid::kTrackHeight;

    grid_->setSize(std::max(gridW, neededGridWidth), std::max(contentH, neededGridHeight));
}

} // namespace trami
