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
    browserToggle_.onClick = [this]() {
        showBrowser_ = !showBrowser_;
        browserPanel_->setVisible(showBrowser_);
        resized();
    };
    addAndMakeVisible(browserToggle_);

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

    // Browser Panel
    browserPanel_ = std::make_unique<SampleBrowserPanel>(song_, engine_, stretchCache_, [this](SampleId id) {
        activeSample_ = id;
        updateSampleSelector();
        republish();
    });
    addAndMakeVisible(browserPanel_.get());

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

void PatternView::fileDroppedOnGrid(const juce::String& filePath, int track, int step)
{
    const std::filesystem::path p(filePath.toStdString());
    if (!importer_.isSupportedFormat(p))
        return;

    const juce::String lowerName = filePath.toLowerCase();
    SampleKind kind = lowerName.contains("loop") ? SampleKind::Loop : SampleKind::OneShot;
    int bars = 1;
    if (lowerName.contains("2bar") || lowerName.contains("2_bar") || lowerName.contains("2 bar"))
        bars = 2;
    else if (lowerName.contains("4bar") || lowerName.contains("4_bar") || lowerName.contains("4 bar"))
        bars = 4;

    auto res = importer_.importFile(p, engine_.sampleRate(), kind, bars);
    if (!res.has_value())
        return;

    const auto sampleId = song_.addSample(std::move(res->sample));
    stretchCache_.setRawAudio(sampleId, std::make_shared<const AudioData>(std::move(res->audio)));
    activeSample_ = sampleId;

    song_.placeClip(currentPattern_, track, sampleId, step);
    updateSampleSelector();
    republish();
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

    g.setColour(Palette::surface);
    g.fillRect(0, 0, getWidth(), 40);
    g.setColour(Palette::gridLine);
    g.drawHorizontalLine(40, 0.0f, static_cast<float>(getWidth()));
}

void PatternView::resized()
{
    constexpr int kToolbarHeight = 40;
    auto toolbar = juce::Rectangle<int>(0, 0, getWidth(), kToolbarHeight).reduced(8, 6);

    browserToggle_.setBounds(toolbar.removeFromLeft(70));
    toolbar.removeFromLeft(8);

    patternTitle_.setBounds(toolbar.removeFromLeft(90));
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

    int currentX = 0;
    if (showBrowser_ && browserPanel_ != nullptr)
    {
        constexpr int kBrowserWidth = 240;
        browserPanel_->setVisible(true);
        browserPanel_->setBounds(0, contentY, kBrowserWidth, contentH);
        currentX += kBrowserWidth;
    }
    else if (browserPanel_ != nullptr)
    {
        browserPanel_->setVisible(false);
    }

    trackHeaders_->setBounds(currentX, contentY, TrackHeaderList::kHeaderWidth, contentH);
    currentX += TrackHeaderList::kHeaderWidth;

    const int gridW = getWidth() - currentX;
    gridViewport_->setBounds(currentX, contentY, gridW, contentH);

    const Pattern* p = song_.findPattern(currentPattern_);
    const int steps = p != nullptr ? p->lengthSteps() : 16;
    const int neededGridWidth = steps * PatternGrid::kStepWidth;
    const int neededGridHeight = PatternGrid::kRulerHeight + kTracksPerPattern * PatternGrid::kTrackHeight;

    grid_->setSize(std::max(gridW, neededGridWidth), std::max(contentH, neededGridHeight));
}

} // namespace trami
