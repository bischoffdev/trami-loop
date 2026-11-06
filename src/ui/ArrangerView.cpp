#include "ui/ArrangerView.h"

#include "model/Timing.h"
#include "ui/TramiLookAndFeel.h"

#include <algorithm>

namespace trami
{

class ArrangerView::SyncViewport : public juce::Viewport
{
public:
    std::function<void(const juce::Rectangle<int>&)> onAreaChanged;
    void visibleAreaChanged(const juce::Rectangle<int>& newVisibleArea) override
    {
        if (onAreaChanged)
            onAreaChanged(newVisibleArea);
    }
};

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

    recordButton_.setActiveColour(juce::Colour(0xffe53935));
    recordButton_.onClick = [this]() { toggleRecording(); };
    addAndMakeVisible(recordButton_);

    rewindButton_.onClick = [this]() { engine_.sequencer().requestRewind(); };
    addAndMakeVisible(rewindButton_);

    metroToggle_.setClickingTogglesState(true);
    metroToggle_.onClick = [this]() { engine_.sequencer().setMetronomeEnabled(metroToggle_.getToggleState()); };
    addAndMakeVisible(metroToggle_);

    tempoLabel_.setFont(Typography::bold(13.5f));
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

    // Zoom controls
    zoomOutButton_.setTooltip("Zoom Out Time (Cmd -)");
    zoomOutButton_.onClick = [this]() { zoomOut(); };
    addAndMakeVisible(zoomOutButton_);

    zoomResetButton_.setTooltip("Reset Zoom to 100% (Cmd 0)");
    zoomResetButton_.onClick = [this]() { resetZoom(); };
    addAndMakeVisible(zoomResetButton_);

    zoomInButton_.setTooltip("Zoom In Time (Cmd +)");
    zoomInButton_.onClick = [this]() { zoomIn(); };
    addAndMakeVisible(zoomInButton_);

    vZoomOutButton_.setTooltip("Decrease Track Height (Cmd Shift -)");
    vZoomOutButton_.onClick = [this]() { trackHeightOut(); };
    addAndMakeVisible(vZoomOutButton_);

    vZoomInButton_.setTooltip("Increase Track Height (Cmd Shift +)");
    vZoomInButton_.onClick = [this]() { trackHeightIn(); };
    addAndMakeVisible(vZoomInButton_);

    // Track headers & Grid
    trackHeaders_ = std::make_unique<ArrangerTrackHeaderList>(song_, [this](int track) {
        if (const auto* trk = song_.arrangerTrack(track))
        {
            engine_.sequencer().setTrackMix(track, trk->mix.volume, trk->mix.pan, trk->muted, trk->solo);
        }
        publishSnapshotQuick();
    });
    trackHeaders_->onMouseWheel = [this](const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) {
        if (gridViewport_ != nullptr)
            gridViewport_->mouseWheelMove(e, wheel);
    };

    trackHeadersViewport_ = std::make_unique<juce::Viewport>();
    trackHeadersViewport_->setViewedComponent(trackHeaders_.get(), false);
    trackHeadersViewport_->setScrollBarsShown(false, false);
    addAndMakeVisible(trackHeadersViewport_.get());

    grid_ = std::make_unique<ArrangerGrid>(song_, *this);

    gridViewport_ = std::make_unique<SyncViewport>();
    gridViewport_->setViewedComponent(grid_.get(), false);
    gridViewport_->setScrollBarsShown(true, true);
    gridViewport_->onAreaChanged = [this](const juce::Rectangle<int>& area) {
        if (trackHeadersViewport_ != nullptr)
            trackHeadersViewport_->setViewPosition(0, area.getY());
    };
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

void ArrangerView::syncAllTrackMixes()
{
    for (int t = 0; t < kArrangerTracks; ++t)
    {
        if (const auto* trk = song_.arrangerTrack(t))
            engine_.sequencer().setTrackMix(t, trk->mix.volume, trk->mix.pan, trk->muted, trk->solo);
    }
}

void ArrangerView::publishSnapshotQuick()
{
    if (activeBank_.empty() && !song_.samples().empty())
        return;
    engine_.publish(SnapshotBuilder::buildArrangement(song_, activeBank_, engine_.sampleRate()));
}

void ArrangerView::republish()
{
    tempoLabel_.setText(juce::String(song_.bpm(), 0) + " BPM", juce::dontSendNotification);
    metroToggle_.setToggleState(engine_.sequencer().isMetronomeEnabled(), juce::dontSendNotification);

    stretchCache_.requestUpdateAsync(song_, engine_.sampleRate(), [this](SampleBank newBank) {
        juce::MessageManager::callAsync([this, bank = std::move(newBank)]() mutable {
            activeBank_ = std::move(bank);
            syncAllTrackMixes();
            engine_.publish(SnapshotBuilder::buildArrangement(song_, activeBank_, engine_.sampleRate()));
            if (trackHeaders_ != nullptr)
                trackHeaders_->updateFromModel();
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
        recordButton_.setLabel("Stop");
        recordButton_.setToggleState(true, juce::dontSendNotification);

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
        recordButton_.setLabel("Rec");
        recordButton_.setToggleState(false, juce::dontSendNotification);
    }
}

void ArrangerView::onRecordFinished(AudioData recorded)
{
    recordButton_.setLabel("Rec");
    recordButton_.setToggleState(false, juce::dontSendNotification);

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

void ArrangerView::zoomRequested(float deltaX, float deltaY, juce::Point<int> anchorInGrid)
{
    applyZoom(zoomX_ * deltaX, zoomY_ * deltaY, anchorInGrid);
}

void ArrangerView::applyZoom(float targetZoomX, float targetZoomY, juce::Point<int> anchorInGrid)
{
    const float newZoomX = std::clamp(targetZoomX, 0.2f, 4.0f);
    const float newZoomY = std::clamp(targetZoomY, 0.6f, 2.5f);

    if (std::abs(newZoomX - zoomX_) < 0.001f && std::abs(newZoomY - zoomY_) < 0.001f)
        return;

    const float oldZoomX = zoomX_;
    const float oldZoomY = zoomY_;

    const int screenAnchorX = anchorInGrid.x - gridViewport_->getViewPositionX();
    const int screenAnchorY = anchorInGrid.y - gridViewport_->getViewPositionY();

    zoomX_ = newZoomX;
    zoomY_ = newZoomY;

    grid_->setZoom(zoomX_, zoomY_);
    trackHeaders_->setTrackHeight(grid_->trackHeight());

    zoomResetButton_.setButtonText(juce::String(juce::roundToInt(zoomX_ * 100.0f)) + "%");

    resized();

    const int newGridX = juce::roundToInt(static_cast<float>(anchorInGrid.x) * (newZoomX / oldZoomX));
    const int newGridY = juce::roundToInt(static_cast<float>(anchorInGrid.y) * (newZoomY / oldZoomY));

    const int newViewX = std::max(0, newGridX - screenAnchorX);
    const int newViewY = std::max(0, newGridY - screenAnchorY);

    gridViewport_->setViewPosition(newViewX, newViewY);
    if (trackHeadersViewport_ != nullptr)
        trackHeadersViewport_->setViewPosition(0, newViewY);
}

void ArrangerView::zoomIn()
{
    const int centerX = gridViewport_->getViewPositionX() + gridViewport_->getViewWidth() / 2;
    const int centerY = gridViewport_->getViewPositionY() + gridViewport_->getViewHeight() / 2;
    applyZoom(zoomX_ * 1.25f, zoomY_, {centerX, centerY});
}

void ArrangerView::zoomOut()
{
    const int centerX = gridViewport_->getViewPositionX() + gridViewport_->getViewWidth() / 2;
    const int centerY = gridViewport_->getViewPositionY() + gridViewport_->getViewHeight() / 2;
    applyZoom(zoomX_ / 1.25f, zoomY_, {centerX, centerY});
}

void ArrangerView::resetZoom()
{
    const int centerX = gridViewport_->getViewPositionX() + gridViewport_->getViewWidth() / 2;
    const int centerY = gridViewport_->getViewPositionY() + gridViewport_->getViewHeight() / 2;
    applyZoom(1.0f, 1.0f, {centerX, centerY});
}

void ArrangerView::trackHeightIn()
{
    const int centerX = gridViewport_->getViewPositionX() + gridViewport_->getViewWidth() / 2;
    const int centerY = gridViewport_->getViewPositionY() + gridViewport_->getViewHeight() / 2;
    applyZoom(zoomX_, zoomY_ * 1.2f, {centerX, centerY});
}

void ArrangerView::trackHeightOut()
{
    const int centerX = gridViewport_->getViewPositionX() + gridViewport_->getViewWidth() / 2;
    const int centerY = gridViewport_->getViewPositionY() + gridViewport_->getViewHeight() / 2;
    applyZoom(zoomX_, zoomY_ / 1.2f, {centerX, centerY});
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

    const auto mods = key.getModifiers();
    const bool isCmdOrCtrl = mods.isCommandDown() || mods.isCtrlDown();
    if (isCmdOrCtrl)
    {
        const auto ch = key.getTextCharacter();
        const auto code = key.getKeyCode();

        if (ch == '+' || ch == '=' || code == '+' || code == '=' || key.isKeyCode(juce::KeyPress::numberPadAdd))
        {
            if (mods.isShiftDown())
                trackHeightIn();
            else
                zoomIn();
            return true;
        }
        if (ch == '-' || code == '-' || key.isKeyCode(juce::KeyPress::numberPadSubtract))
        {
            if (mods.isShiftDown())
                trackHeightOut();
            else
                zoomOut();
            return true;
        }
        if (ch == '0' || code == '0' || key.isKeyCode(juce::KeyPress::numberPad0))
        {
            resetZoom();
            return true;
        }
    }

    return false;
}

void ArrangerView::timerCallback()
{
    auto& seq = engine_.sequencer();
    const bool isPlaying = seq.isPlaying();
    playButton_.setIcon(isPlaying ? IconButton::Icon::Stop : IconButton::Icon::Play);
    playButton_.setLabel(isPlaying ? "Stop" : "Play Song");
    playButton_.setToggleState(isPlaying, juce::dontSendNotification);

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

    constexpr int kToolbarHeight = 52;
    g.setColour(Palette::surface);
    g.fillRect(0, 0, getWidth(), kToolbarHeight);
    g.setColour(Palette::gridLine);
    g.drawHorizontalLine(kToolbarHeight, 0.0f, static_cast<float>(getWidth()));
}

void ArrangerView::resized()
{
    constexpr int kToolbarHeight = 52;
    auto toolbar = juce::Rectangle<int>(0, 0, getWidth(), kToolbarHeight).reduced(8, 4);

    // Zoom controls on the right
    auto rightCluster = toolbar.removeFromRight(236);
    vZoomInButton_.setBounds(rightCluster.removeFromRight(46));
    rightCluster.removeFromRight(3);
    vZoomOutButton_.setBounds(rightCluster.removeFromRight(46));
    rightCluster.removeFromRight(6);

    zoomInButton_.setBounds(rightCluster.removeFromRight(44));
    rightCluster.removeFromRight(2);
    zoomResetButton_.setBounds(rightCluster.removeFromRight(48));
    rightCluster.removeFromRight(2);
    zoomOutButton_.setBounds(rightCluster.removeFromRight(44));

    toolbar.removeFromRight(12);

    patternCombo_.setBounds(toolbar.removeFromLeft(140));
    toolbar.removeFromLeft(6);
    addPatternButton_.setBounds(toolbar.removeFromLeft(48));
    toolbar.removeFromLeft(3);
    duplicatePatternButton_.setBounds(toolbar.removeFromLeft(66));
    toolbar.removeFromLeft(3);
    deletePatternButton_.setBounds(toolbar.removeFromLeft(48));
    toolbar.removeFromLeft(6);

    editPatternButton_.setBounds(toolbar.removeFromLeft(82));
    toolbar.removeFromLeft(12);

    rewindButton_.setBounds(toolbar.removeFromLeft(48));
    toolbar.removeFromLeft(3);
    playButton_.setBounds(toolbar.removeFromLeft(66));
    toolbar.removeFromLeft(3);
    recordButton_.setBounds(toolbar.removeFromLeft(48));
    toolbar.removeFromLeft(10);

    metroToggle_.setBounds(toolbar.removeFromLeft(48));
    toolbar.removeFromLeft(12);

    tempoDown_.setBounds(toolbar.removeFromLeft(28));
    tempoLabel_.setBounds(toolbar.removeFromLeft(68));
    tempoUp_.setBounds(toolbar.removeFromLeft(28));

    // Content area below toolbar
    const int contentY = kToolbarHeight;
    const int contentH = getHeight() - contentY;

    const int headerW = ArrangerTrackHeaderList::kHeaderWidth;
    trackHeadersViewport_->setBounds(0, contentY, headerW, contentH);

    const int gridX = headerW;
    const int gridW = getWidth() - gridX;
    gridViewport_->setBounds(gridX, contentY, gridW, contentH);

    const int totalBars = std::max(16, song_.lengthBars() + 6);
    const int neededGridWidth = totalBars * grid_->barWidth();
    const int neededGridHeight = ArrangerGrid::kRulerHeight + kArrangerTracks * grid_->trackHeight();

    grid_->setSize(std::max(gridW, neededGridWidth), std::max(contentH, neededGridHeight));
    trackHeaders_->setSize(headerW, std::max(contentH, neededGridHeight));
}

} // namespace trami
