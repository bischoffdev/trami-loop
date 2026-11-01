#include "ui/ArrangerView.h"

#include "model/Timing.h"
#include "ui/TramiLookAndFeel.h"

#include <algorithm>

namespace trami
{

ArrangerView::ArrangerView(Song& song, AudioEngine& engine, StretchCache& stretchCache,
                           EditPatternCallback onEditPattern)
    : song_(song), engine_(engine), stretchCache_(stretchCache), onEditPattern_(std::move(onEditPattern))
{
    setWantsKeyboardFocus(true);

    if (!song_.patterns().empty())
        activePattern_ = song_.patterns().front().id;

    // Pattern combo
    patternCombo_.onChange = [this]() {
        const int idx = patternCombo_.getSelectedId();
        if (idx > 0 && idx <= static_cast<int>(song_.patterns().size()))
            activePattern_ = song_.patterns()[static_cast<std::size_t>(idx - 1)].id;
    };
    addAndMakeVisible(patternCombo_);

    addPatternButton_.onClick = [this]() {
        const auto name = ("Pattern " + juce::String(song_.patterns().size() + 1)).toStdString();
        const auto newId = song_.addPattern(name, 1);
        activePattern_ = newId;
        updatePatternList();
        republish();
    };
    addAndMakeVisible(addPatternButton_);

    duplicatePatternButton_.onClick = [this]() {
        if (auto dup = song_.duplicatePattern(activePattern_))
        {
            activePattern_ = *dup;
            updatePatternList();
            republish();
        }
    };
    addAndMakeVisible(duplicatePatternButton_);

    deletePatternButton_.onClick = [this]() {
        if (song_.patterns().size() > 1)
        {
            song_.removePattern(activePattern_);
            activePattern_ = song_.patterns().front().id;
            updatePatternList();
            republish();
        }
    };
    addAndMakeVisible(deletePatternButton_);

    editPatternButton_.onClick = [this]() {
        if (onEditPattern_)
            onEditPattern_(activePattern_);
    };
    addAndMakeVisible(editPatternButton_);

    // Transport
    playButton_.onClick = [this]() {
        auto& seq = engine_.sequencer();
        seq.requestPlaying(!seq.isPlaying());
    };
    addAndMakeVisible(playButton_);

    recordButton_.setColour(juce::TextButton::textColourOffId, juce::Colours::red.brighter(0.5f));
    recordButton_.onClick = [this]() { toggleRecording(); };
    addAndMakeVisible(recordButton_);

    rewindButton_.onClick = [this]() { engine_.sequencer().requestRewind(); };
    addAndMakeVisible(rewindButton_);

    metroToggle_.setColour(juce::ToggleButton::textColourId, Palette::textDim);
    metroToggle_.onClick = [this]() { engine_.sequencer().setMetronomeEnabled(metroToggle_.getToggleState()); };
    addAndMakeVisible(metroToggle_);

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

    // Track headers & Grid
    trackHeaders_ = std::make_unique<ArrangerTrackHeaderList>(song_, [this]() { republish(); });
    addAndMakeVisible(trackHeaders_.get());

    grid_ = std::make_unique<ArrangerGrid>(song_, *this);

    gridViewport_ = std::make_unique<juce::Viewport>();
    gridViewport_->setViewedComponent(grid_.get(), false);
    gridViewport_->setScrollBarsShown(true, true);
    addAndMakeVisible(gridViewport_.get());

    updatePatternList();
    republish();
    startTimerHz(30);
}

ArrangerView::~ArrangerView()
{
    stopTimer();
}

void ArrangerView::updatePatternList()
{
    patternCombo_.clear(juce::dontSendNotification);
    int selectedId = 1;
    int idx = 1;

    for (const auto& pattern : song_.patterns())
    {
        patternCombo_.addItem(juce::String(pattern.name) + " (" + juce::String(pattern.lengthBars) + "b)", idx);
        if (pattern.id == activePattern_)
            selectedId = idx;
        ++idx;
    }

    if (!song_.patterns().empty())
        patternCombo_.setSelectedId(selectedId, juce::dontSendNotification);
}

void ArrangerView::republish()
{
    tempoLabel_.setText(juce::String(song_.bpm(), 0) + " BPM", juce::dontSendNotification);
    metroToggle_.setToggleState(engine_.sequencer().isMetronomeEnabled(), juce::dontSendNotification);

    stretchCache_.requestUpdateAsync(song_, engine_.sampleRate(), [this](SampleBank newBank) {
        juce::MessageManager::callAsync([this, bank = std::move(newBank)]() mutable {
            activeBank_ = std::move(bank);
            engine_.publish(SnapshotBuilder::buildArrangement(song_, activeBank_, engine_.sampleRate()));
            if (grid_ != nullptr)
                grid_->repaint();
            resized();
        });
    });
}

void ArrangerView::toggleRecording()
{
    auto& rec = engine_.recorder();
    auto& seq = engine_.sequencer();

    if (rec.state() == RecordState::Idle)
    {
        const Timing timing(song_.bpm(), engine_.sampleRate());
        const int countInSamples = static_cast<int>(timing.samplesPerBar());

        seq.setMetronomeEnabled(true);
        recordButton_.setButtonText("Stop");
        recordButton_.setColour(juce::TextButton::buttonColourId, juce::Colours::red.withAlpha(0.6f));

        rec.start(countInSamples, engine_.inputLatencySamples(), [this](AudioData recorded) {
            juce::MessageManager::callAsync([this, audio = std::move(recorded)]() mutable {
                onRecordFinished(std::move(audio));
            });
        });
        seq.requestPlaying(true);
    }
    else
    {
        rec.stop();
        seq.requestPlaying(false);
        recordButton_.setButtonText("Rec");
        recordButton_.setColour(juce::TextButton::buttonColourId, Palette::surface);
    }
}

void ArrangerView::onRecordFinished(AudioData recorded)
{
    recordButton_.setButtonText("Rec");
    recordButton_.setColour(juce::TextButton::buttonColourId, Palette::surface);

    const double rate = engine_.sampleRate();
    const double durationSec = static_cast<double>(recorded.numFrames()) / rate;
    const double secondsPerBar = (60.0 / song_.bpm()) * kBeatsPerBar;
    const int recordedBars = std::max(1, static_cast<int>(std::round(durationSec / secondsPerBar)));

    Sample s;
    s.name = ("Take " + juce::String(song_.samples().size() + 1)).toStdString();
    s.durationSeconds = durationSec;
    s.kind = SampleKind::OneShot;

    const auto sampleId = song_.addSample(s);
    stretchCache_.setRawAudio(sampleId, std::make_shared<const AudioData>(std::move(recorded)));

    const auto patId = song_.addPattern(s.name, recordedBars);
    song_.placeClip(patId, 0, sampleId, 0);

    song_.placeBlock(0, patId, song_.lengthBars());
    activePattern_ = patId;

    updatePatternList();
    republish();
}

void ArrangerView::blockClicked(BlockId)
{
}

void ArrangerView::blockDoubleClicked(PatternId patternId)
{
    if (onEditPattern_)
        onEditPattern_(patternId);
}

void ArrangerView::blockMoved(BlockId blockId, int newTrack, int newBar)
{
    if (song_.moveBlock(blockId, newTrack, newBar))
    {
        republish();
        grid_->repaint();
    }
}

void ArrangerView::blockDeleteRequested(BlockId blockId)
{
    if (song_.removeBlock(blockId))
    {
        republish();
        grid_->repaint();
    }
}

void ArrangerView::gridCellClicked(int track, int bar)
{
    if (!activePattern_.isValid())
    {
        if (!song_.patterns().empty())
            activePattern_ = song_.patterns().front().id;
        else
            return;
    }

    if (song_.placeBlock(track, activePattern_, bar))
    {
        republish();
        grid_->repaint();
    }
}

bool ArrangerView::keyPressed(const juce::KeyPress& key)
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

void ArrangerView::timerCallback()
{
    auto& seq = engine_.sequencer();
    playButton_.setButtonText(seq.isPlaying() ? "Stop" : "Play Song");

    const Timing timing(song_.bpm(), engine_.sampleRate());
    const double samplesPerBar = timing.samplesPerBar();

    if (samplesPerBar > 0.0)
    {
        const double currentBar = static_cast<double>(seq.position()) / samplesPerBar;
        grid_->setPlayheadBar(seq.isPlaying() ? currentBar : -1.0);
    }
}

void ArrangerView::paint(juce::Graphics& g)
{
    g.fillAll(Palette::background);

    g.setColour(Palette::surface);
    g.fillRect(0, 0, getWidth(), 40);
    g.setColour(Palette::gridLine);
    g.drawHorizontalLine(40, 0.0f, static_cast<float>(getWidth()));
}

void ArrangerView::resized()
{
    constexpr int kToolbarHeight = 40;
    auto toolbar = juce::Rectangle<int>(0, 0, getWidth(), kToolbarHeight).reduced(8, 6);

    patternCombo_.setBounds(toolbar.removeFromLeft(150));
    toolbar.removeFromLeft(6);
    addPatternButton_.setBounds(toolbar.removeFromLeft(76));
    toolbar.removeFromLeft(4);
    duplicatePatternButton_.setBounds(toolbar.removeFromLeft(76));
    toolbar.removeFromLeft(4);
    deletePatternButton_.setBounds(toolbar.removeFromLeft(60));
    toolbar.removeFromLeft(10);

    editPatternButton_.setBounds(toolbar.removeFromLeft(140));
    toolbar.removeFromLeft(16);

    rewindButton_.setBounds(toolbar.removeFromLeft(36));
    toolbar.removeFromLeft(4);
    playButton_.setBounds(toolbar.removeFromLeft(80));
    toolbar.removeFromLeft(4);
    recordButton_.setBounds(toolbar.removeFromLeft(50));
    toolbar.removeFromLeft(12);

    metroToggle_.setBounds(toolbar.removeFromLeft(60));
    toolbar.removeFromLeft(16);

    tempoDown_.setBounds(toolbar.removeFromLeft(28));
    tempoLabel_.setBounds(toolbar.removeFromLeft(64));
    tempoUp_.setBounds(toolbar.removeFromLeft(28));

    // Content area below toolbar
    const int contentY = kToolbarHeight;
    const int contentH = getHeight() - contentY;

    trackHeaders_->setBounds(0, contentY, ArrangerTrackHeaderList::kHeaderWidth, contentH);

    const int gridX = ArrangerTrackHeaderList::kHeaderWidth;
    const int gridW = getWidth() - gridX;
    gridViewport_->setBounds(gridX, contentY, gridW, contentH);

    const int totalBars = std::max(16, song_.lengthBars() + 6);
    const int neededGridWidth = totalBars * ArrangerGrid::kBarWidth;
    const int neededGridHeight = ArrangerGrid::kRulerHeight + kArrangerTracks * ArrangerGrid::kTrackHeight;

    grid_->setSize(std::max(gridW, neededGridWidth), std::max(contentH, neededGridHeight));
}

} // namespace trami
