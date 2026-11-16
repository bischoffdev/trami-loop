#include "ui/ArrangerView.h"

#include "model/Timing.h"
#include "ui/StudioOneZoomCorner.h"
#include "ui/TramiLookAndFeel.h"

#include <algorithm>

namespace trami
{

class ArrangerView::SyncViewport : public juce::Viewport
{
public:
    SyncViewport()
    {
        setScrollBarThickness(StudioOneZoomCorner::kThickness);
        addAndMakeVisible(zoomCorner_);
    }

    StudioOneZoomCorner& zoomCorner() noexcept { return zoomCorner_; }

    std::function<void(const juce::Rectangle<int>&)> onAreaChanged;
    void visibleAreaChanged(const juce::Rectangle<int>& newVisibleArea) override
    {
        if (onAreaChanged)
            onAreaChanged(newVisibleArea);
    }

    void resized() override
    {
        juce::Viewport::resized();

        constexpr int T = StudioOneZoomCorner::kThickness;
        constexpr int L = StudioOneZoomCorner::kSliderLength;
        constexpr int totalSize = L + T;
        const int w = getWidth();
        const int h = getHeight();

        auto& vBar = getVerticalScrollBar();
        auto& hBar = getHorizontalScrollBar();

        if (vBar.isVisible())
            vBar.setBounds(vBar.getX(), vBar.getY(), vBar.getWidth(), std::max(0, h - totalSize));

        if (hBar.isVisible())
            hBar.setBounds(hBar.getX(), hBar.getY(), std::max(0, w - totalSize), hBar.getHeight());

        zoomCorner_.setBounds(w - totalSize, h - totalSize, totalSize, totalSize);
    }

private:
    StudioOneZoomCorner zoomCorner_;
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

    addPatternButton_.setTooltip("Create and append a new pattern");
    addPatternButton_.onClick = [this]() {
        if (onPushUndo_)
            onPushUndo_();
        const auto name = ("Pattern " + juce::String(song_.patterns().size() + 1)).toStdString();
        const auto newId = song_.addPattern(name, 1);
        activePattern_ = newId;
        updatePatternList();
        republish();
    };
    addAndMakeVisible(addPatternButton_);

    editPatternButton_.setTooltip("Switch to Pattern Editor for selected pattern");
    editPatternButton_.onClick = [this]() {
        if (onEditPattern_)
            onEditPattern_(activePattern_);
    };
    addAndMakeVisible(editPatternButton_);

    addTrackButton_.onClick = [this]() { addTrack(); };
    addAndMakeVisible(addTrackButton_);

    zoomX_ = song_.arrangerZoomX();
    zoomY_ = song_.arrangerZoomY();

    // Headers & Grid
    trackHeaders_ = std::make_unique<ArrangerTrackHeaderList>(song_, [this](int track) {
        if (const auto* trk = song_.arrangerTrack(track))
            engine_.sequencer().setTrackMix(track, trk->mix.volume, trk->mix.pan, trk->muted, trk->solo);
        publishSnapshotQuick();
    });
    trackHeaders_->setOnTrackRenamed([this](int, const std::string&) {
        if (onPushUndo_)
            onPushUndo_();
    });
    trackHeaders_->setOnDeleteTrack([this](int track) {
        deleteTrack(track);
    });
    trackHeaders_->setOnMoveTrack([this](int from, int to) {
        moveTrack(from, to);
    });
    trackHeaders_->setOnAddTrack([this]() {
        addTrack();
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
    loopStartBar_ = grid_->loopStartBar();
    loopEndBar_ = std::max(grid_->loopEndBar(), song_.lengthBars());
    grid_->setLoopRegion(loopStartBar_, loopEndBar_);

    gridViewport_ = std::make_unique<SyncViewport>();
    gridViewport_->setViewedComponent(grid_.get(), false);
    gridViewport_->setScrollBarsShown(true, true);
    gridViewport_->onAreaChanged = [this](const juce::Rectangle<int>& area) {
        if (trackHeadersViewport_ != nullptr)
            trackHeadersViewport_->setViewPosition(0, area.getY());
    };
    gridViewport_->zoomCorner().setZoomLevels(zoomX_, zoomY_);
    gridViewport_->zoomCorner().onZoomXChanged = [this](float newZoomX) {
        const auto anchor = gridViewport_ != nullptr ? gridViewport_->getViewArea().getCentre() : juce::Point<int>(0, 0);
        applyZoom(newZoomX, zoomY_, anchor);
    };
    gridViewport_->zoomCorner().onZoomYChanged = [this](float newZoomY) {
        const auto anchor = gridViewport_ != nullptr ? gridViewport_->getViewArea().getCentre() : juce::Point<int>(0, 0);
        applyZoom(zoomX_, newZoomY, anchor);
    };
    gridViewport_->zoomCorner().onZoomIn = [this]() { zoomIn(); };
    gridViewport_->zoomCorner().onZoomOut = [this]() { zoomOut(); };
    gridViewport_->zoomCorner().onTrackHeightIn = [this]() { trackHeightIn(); };
    gridViewport_->zoomCorner().onTrackHeightOut = [this]() { trackHeightOut(); };
    gridViewport_->zoomCorner().onResetZoom = [this]() { resetZoom(); };
    gridViewport_->zoomCorner().onZoomDelta = [this](float dx, float dy) {
        applyZoom(zoomX_ * dx, zoomY_ * dy, {getWidth() / 2, 0});
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
    if (!song_.findPattern(activePattern_) && !song_.patterns().empty())
        activePattern_ = song_.patterns().front().id;

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

    if (trackHeaders_ != nullptr)
        trackHeaders_->updateFromModel();
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
    const auto rawBank = stretchCache_.getRawBank();
    const auto& bankToUse = activeBank_.empty() ? rawBank : activeBank_;
    if (bankToUse.empty() && !song_.samples().empty())
        return;
    engine_.publish(SnapshotBuilder::buildArrangement(song_, bankToUse, engine_.sampleRate(),
                                                     loopStartBar_, loopEndBar_, &rawBank, &stretchCache_));
}

void ArrangerView::republish()
{
    stretchCache_.requestUpdateAsync(song_, engine_.sampleRate(), [this](SampleBank newBank) {
        juce::MessageManager::callAsync([this, bank = std::move(newBank)]() mutable {
            activeBank_ = std::move(bank);
            syncAllTrackMixes();
            const auto rawBank = stretchCache_.getRawBank();
            engine_.publish(SnapshotBuilder::buildArrangement(song_, activeBank_, engine_.sampleRate(),
                                                             loopStartBar_, loopEndBar_, &rawBank, &stretchCache_));
            if (trackHeaders_ != nullptr)
                trackHeaders_->updateFromModel();
            if (grid_ != nullptr)
                grid_->repaint();
            resized();
        });
    });
}

void ArrangerView::loopRegionChanged(int startBar, int endBar)
{
    loopStartBar_ = startBar;
    loopEndBar_ = endBar;
    republish();
    if (onLoopRegionChanged_)
        onLoopRegionChanged_(loopStartBar_, loopEndBar_);
}

void ArrangerView::setLoopRegion(int startBar, int endBar)
{
    const int maxBars = std::max(2, song_.lengthBars());
    loopStartBar_ = std::clamp(startBar, 0, maxBars);
    loopEndBar_ = std::clamp(endBar, loopStartBar_ + 1, std::max(maxBars, endBar));
    if (grid_ != nullptr)
        grid_->setLoopRegion(loopStartBar_, loopEndBar_);
    republish();
    if (onLoopRegionChanged_)
        onLoopRegionChanged_(loopStartBar_, loopEndBar_);
}

void ArrangerView::playheadSeekRequested(double bar)
{
    const Timing timing(song_.bpm(), engine_.sampleRate());
    const double samplesPerBar = timing.samplesPerBar();
    const std::int64_t targetSample = static_cast<std::int64_t>(std::round(bar * samplesPerBar));
    engine_.sequencer().requestSeek(targetSample);
    if (grid_ != nullptr)
        grid_->setPlayheadBar(bar);
}

void ArrangerView::addTrack()
{
    if (onPushUndo_)
        onPushUndo_();
    if (song_.addArrangerTrack(activePattern_) >= 0)
    {
        if (trackHeaders_ != nullptr)
            trackHeaders_->updateFromModel();
        republish();
        if (grid_ != nullptr)
            grid_->repaint();
        resized();
    }
}

void ArrangerView::deleteTrack(int track)
{
    if (onPushUndo_)
        onPushUndo_();
    if (song_.deleteArrangerTrack(track))
    {
        trackHeaders_->updateFromModel();
        syncAllTrackMixes();
        republish();
        if (grid_ != nullptr)
            grid_->repaint();
        resized();
    }
}

void ArrangerView::moveTrack(int from, int to)
{
    if (onPushUndo_)
        onPushUndo_();
    if (song_.moveArrangerTrack(from, to))
    {
        trackHeaders_->updateFromModel();
        syncAllTrackMixes();
        republish();
        if (grid_ != nullptr)
            grid_->repaint();
    }
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
    }
}

void ArrangerView::onRecordFinished(AudioData recorded)
{
    if (onPushUndo_)
        onPushUndo_();

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

bool ArrangerView::isInterestedInDragSource(const SourceDetails& dragSourceDetails)
{
    if (dragSourceDetails.description.isString())
    {
        return dragSourceDetails.description.toString().startsWith("pattern:");
    }
    return false;
}

void ArrangerView::itemDropped(const SourceDetails& dragSourceDetails)
{
    if (!dragSourceDetails.description.isString())
        return;

    const juce::String desc = dragSourceDetails.description.toString();
    if (!desc.startsWith("pattern:"))
        return;

    const int patVal = desc.fromFirstOccurrenceOf("pattern:", false, false).getIntValue();
    if (patVal <= 0)
        return;

    constexpr int kToolbarHeight = 44;
    const int headerW = ArrangerTrackHeaderList::kHeaderWidth;
    const auto ptInArranger = dragSourceDetails.localPosition;

    const auto ptInViewport = ptInArranger - juce::Point<int>(headerW, kToolbarHeight);
    const auto ptInGrid = ptInViewport + juce::Point<int>(gridViewport_->getViewPositionX(), gridViewport_->getViewPositionY());

    const auto [track, bar] = grid_->pointToTrackAndBar(ptInGrid);
    if (track >= 0 && track < kArrangerTracks && bar >= 0)
    {
        patternDropped(PatternId{static_cast<std::uint32_t>(patVal)}, track, bar);
    }
}

void ArrangerView::patternDropped(PatternId patternId, int track, int bar)
{
    if (track >= 0 && track < kArrangerTracks && bar >= 0 && patternId.isValid())
    {
        if (onPushUndo_)
            onPushUndo_();

        int targetTrack = song_.findTrackForPattern(patternId);
        if (targetTrack < 0)
        {
            if (!song_.arrangerTrackPattern(track).isValid())
            {
                targetTrack = track;
            }
            else
            {
                for (int t = 0; t < kArrangerTracks; ++t)
                {
                    if (!song_.arrangerTrackPattern(t).isValid())
                    {
                        targetTrack = t;
                        break;
                    }
                }
                if (targetTrack < 0)
                    targetTrack = track;
            }
            song_.setArrangerTrackPattern(targetTrack, patternId);
        }

        if (song_.placeBlock(targetTrack, patternId, bar))
        {
            publishSnapshotQuick();
            republish();
            updatePatternList();
            if (trackHeaders_ != nullptr)
                trackHeaders_->updateFromModel();
            grid_->repaint();
        }
    }
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
    if (onPushUndo_)
        onPushUndo_();
    if (song_.moveBlock(blockId, newTrack, newBar))
    {
        publishSnapshotQuick();
        republish();
        grid_->repaint();
    }
}

void ArrangerView::blockDeleteRequested(BlockId blockId)
{
    BlockId nextBlockId;
    for (int t = 0; t < song_.numArrangerTracks(); ++t)
    {
        if (const auto* track = song_.arrangerTrack(t))
        {
            const auto it = std::find_if(track->blocks.begin(), track->blocks.end(),
                                         [blockId](const PatternBlock& b) { return b.id == blockId; });
            if (it != track->blocks.end())
            {
                const int curBar = it->startBar;
                const PatternBlock* nextBlock = nullptr;
                const PatternBlock* prevBlock = nullptr;

                for (const auto& b : track->blocks)
                {
                    if (b.id == blockId)
                        continue;

                    if (b.startBar >= curBar)
                    {
                        if (nextBlock == nullptr || b.startBar < nextBlock->startBar)
                            nextBlock = &b;
                    }
                    else
                    {
                        if (prevBlock == nullptr || b.startBar > prevBlock->startBar)
                            prevBlock = &b;
                    }
                }

                if (nextBlock != nullptr)
                    nextBlockId = nextBlock->id;
                else if (prevBlock != nullptr)
                    nextBlockId = prevBlock->id;

                break;
            }
        }
    }

    if (onPushUndo_)
        onPushUndo_();
    if (song_.removeBlock(blockId))
    {
        if (grid_ != nullptr)
            grid_->setSelectedBlock(nextBlockId);
        publishSnapshotQuick();
        republish();
        if (grid_ != nullptr)
            grid_->repaint();
    }
}

void ArrangerView::gridCellClicked(int track, int bar)
{
    PatternId patId = song_.arrangerTrackPattern(track);
    if (!patId.isValid())
    {
        if (activePattern_.isValid())
        {
            patId = activePattern_;
            song_.setArrangerTrackPattern(track, patId);
        }
        else if (!song_.patterns().empty())
        {
            patId = song_.patterns().front().id;
            song_.setArrangerTrackPattern(track, patId);
        }
        else
        {
            return;
        }
    }

    if (onPushUndo_)
        onPushUndo_();

    if (song_.placeBlock(track, patId, bar))
    {
        publishSnapshotQuick();
        republish();
        if (trackHeaders_ != nullptr)
            trackHeaders_->updateFromModel();
        grid_->repaint();
    }
}

void ArrangerView::setZoom(float x, float y)
{
    applyZoom(x, y, {0, 0});
}

void ArrangerView::zoomRequested(float deltaX, float deltaY, juce::Point<int> anchorInGrid)
{
    applyZoom(zoomX_ * deltaX, zoomY_ * deltaY, anchorInGrid);
}

void ArrangerView::zoomToLevelRequested(float targetZoomX, double anchorBar, int anchorViewportX)
{
    applyZoomAtBar(targetZoomX, anchorBar, anchorViewportX);
}

void ArrangerView::applyZoomAtBar(float targetZoomX, double anchorBar, int anchorViewportX)
{
    const float newZoomX = std::clamp(targetZoomX, 0.2f, 4.0f);
    song_.setArrangerZoom(newZoomX, zoomY_);

    if (std::abs(newZoomX - zoomX_) < 0.0005f)
        return;

    zoomX_ = newZoomX;

    if (gridViewport_ != nullptr)
        gridViewport_->zoomCorner().setZoomLevels(zoomX_, zoomY_);

    if (grid_ != nullptr)
        grid_->setZoom(zoomX_, zoomY_);
    if (trackHeaders_ != nullptr)
        trackHeaders_->setTrackHeight(grid_->trackHeight());

    resized();

    if (gridViewport_ != nullptr && grid_ != nullptr)
    {
        const int newBarW = grid_->barWidth();
        const int newVpX = juce::roundToInt(anchorBar * static_cast<double>(newBarW) - static_cast<double>(anchorViewportX));
        gridViewport_->setViewPosition(std::max(0, newVpX), gridViewport_->getViewPositionY());
    }
}

void ArrangerView::applyZoom(float targetZoomX, float targetZoomY, juce::Point<int> anchorInGrid)
{
    const float newZoomX = std::clamp(targetZoomX, 0.2f, 4.0f);
    const float newZoomY = std::clamp(targetZoomY, 0.5f, 2.5f);

    song_.setArrangerZoom(newZoomX, newZoomY);

    if (std::abs(newZoomX - zoomX_) < 0.001f && std::abs(newZoomY - zoomY_) < 0.001f)
        return;

    const double ratioX = static_cast<double>(newZoomX) / static_cast<double>(zoomX_);
    const int vpX = gridViewport_->getViewPositionX();
    const int newVpX = (anchorInGrid.x == 0 && anchorInGrid.y == 0)
                           ? juce::roundToInt(static_cast<double>(vpX) * ratioX)
                           : juce::roundToInt(static_cast<double>(vpX) + static_cast<double>(anchorInGrid.x) * (ratioX - 1.0));

    zoomX_ = newZoomX;
    zoomY_ = newZoomY;

    if (gridViewport_ != nullptr)
        gridViewport_->zoomCorner().setZoomLevels(zoomX_, zoomY_);

    if (grid_ != nullptr)
        grid_->setZoom(zoomX_, zoomY_);
    if (trackHeaders_ != nullptr)
        trackHeaders_->setTrackHeight(grid_->trackHeight());

    resized();

    if (gridViewport_ != nullptr)
        gridViewport_->setViewPosition(std::max(0, newVpX), gridViewport_->getViewPositionY());
}

void ArrangerView::zoomIn()
{
    const auto anchor = gridViewport_ != nullptr ? gridViewport_->getViewArea().getCentre() : juce::Point<int>(0, 0);
    applyZoom(zoomX_ * 1.25f, zoomY_, anchor);
}

void ArrangerView::zoomOut()
{
    const auto anchor = gridViewport_ != nullptr ? gridViewport_->getViewArea().getCentre() : juce::Point<int>(0, 0);
    applyZoom(zoomX_ / 1.25f, zoomY_, anchor);
}

void ArrangerView::resetZoom()
{
    const int totalBars = std::max(4, song_.lengthBars());
    const int numTracks = std::max(1, song_.numArrangerTracks());

    const int viewW = gridViewport_ != nullptr
                          ? std::max(100, gridViewport_->getWidth() - StudioOneZoomCorner::kThickness - 4)
                          : getWidth();
    const int viewH = gridViewport_ != nullptr
                          ? std::max(100, gridViewport_->getHeight() - ArrangerGrid::kRulerHeight - StudioOneZoomCorner::kThickness - 4)
                          : getHeight();

    const float fitX = static_cast<float>(viewW) / static_cast<float>(totalBars * ArrangerGrid::kDefaultBarWidth);
    const float fitY = static_cast<float>(viewH) / static_cast<float>(numTracks * ArrangerGrid::kDefaultTrackHeight);

    applyZoom(fitX, std::clamp(fitY, 0.6f, 2.0f), {0, 0});
    if (gridViewport_ != nullptr)
        gridViewport_->setViewPosition(0, 0);
}

void ArrangerView::trackHeightIn()
{
    const auto anchor = gridViewport_ != nullptr ? gridViewport_->getViewArea().getCentre() : juce::Point<int>(0, 0);
    applyZoom(zoomX_, zoomY_ * 1.2f, anchor);
}

void ArrangerView::trackHeightOut()
{
    const auto anchor = gridViewport_ != nullptr ? gridViewport_->getViewArea().getCentre() : juce::Point<int>(0, 0);
    applyZoom(zoomX_, zoomY_ / 1.2f, anchor);
}

bool ArrangerView::keyPressed(const juce::KeyPress& key)
{
    if (key.getKeyCode() == juce::KeyPress::spaceKey)
    {
        auto& seq = engine_.sequencer();
        seq.requestPlaying(!seq.isPlaying());
        return true;
    }

    if (key.getKeyCode() == juce::KeyPress::returnKey)
    {
        engine_.sequencer().requestRewind();
        return true;
    }

    if (key.getKeyCode() == juce::KeyPress::deleteKey || key.getKeyCode() == juce::KeyPress::backspaceKey)
    {
        if (grid_ != nullptr && grid_->selectedBlock().isValid())
        {
            blockDeleteRequested(grid_->selectedBlock());
            return true;
        }
    }

    if (key.getModifiers().isCommandDown())
    {
        if (key.getKeyCode() == '=' || key.getKeyCode() == '+')
        {
            if (key.getModifiers().isShiftDown())
                trackHeightIn();
            else
                zoomIn();
            return true;
        }
        if (key.getKeyCode() == '-')
        {
            if (key.getModifiers().isShiftDown())
                trackHeightOut();
            else
                zoomOut();
            return true;
        }
        if (key.getKeyCode() == '0')
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
    const Timing timing(song_.bpm(), engine_.sampleRate());
    const double samplesPerBar = timing.samplesPerBar();

    if (samplesPerBar > 0.0 && grid_ != nullptr)
    {
        const double currentBar = static_cast<double>(seq.position()) / samplesPerBar;
        grid_->setPlayheadBar(currentBar);
    }
}

void ArrangerView::paint(juce::Graphics& g)
{
    g.fillAll(Palette::background);

    constexpr int kToolbarHeight = 44;
    g.setColour(Palette::arrangerHeaderBg);
    g.fillRect(0, 0, getWidth(), kToolbarHeight);
    g.setColour(Palette::arrangerAccent.withAlpha(0.7f));
    g.drawHorizontalLine(kToolbarHeight, 0.0f, static_cast<float>(getWidth()));
    g.fillRect(0, 0, 4, kToolbarHeight);
}

void ArrangerView::resized()
{
    constexpr int kToolbarHeight = 44;
    auto toolbar = juce::Rectangle<int>(0, 0, getWidth(), kToolbarHeight).reduced(8, 4);

    patternCombo_.setBounds(toolbar.removeFromLeft(150));
    toolbar.removeFromLeft(8);
    addPatternButton_.setBounds(toolbar.removeFromLeft(68));
    toolbar.removeFromLeft(8);
    editPatternButton_.setBounds(toolbar.removeFromLeft(88));
    toolbar.removeFromLeft(8);
    addTrackButton_.setBounds(toolbar.removeFromLeft(70));

    // Content area below toolbar
    const int contentY = kToolbarHeight;
    const int contentH = getHeight() - contentY;

    const int headerW = ArrangerTrackHeaderList::kHeaderWidth;
    trackHeadersViewport_->setBounds(0, contentY, headerW, contentH);

    const int gridX = headerW;
    const int gridW = getWidth() - gridX;
    gridViewport_->setBounds(gridX, contentY, gridW, contentH);

    const int numTracks = song_.numArrangerTracks();
    const int totalBars = std::max(16, song_.lengthBars() + 6);
    const int neededGridWidth = totalBars * grid_->barWidth();
    const int neededGridHeight = ArrangerGrid::kRulerHeight + (numTracks + 1) * grid_->trackHeight();

    grid_->setSize(std::max(gridW, neededGridWidth), std::max(contentH, neededGridHeight));
    trackHeaders_->setSize(headerW, std::max(contentH, neededGridHeight));
}

} // namespace trami
