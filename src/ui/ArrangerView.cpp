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
        if (onPushUndo_)
            onPushUndo_();
        const auto name = ("Pattern " + juce::String(song_.patterns().size() + 1)).toStdString();
        const auto newId = song_.addPattern(name, 1);
        activePattern_ = newId;
        updatePatternList();
        republish();
    };
    addAndMakeVisible(addPatternButton_);

    editPatternButton_.onClick = [this]() {
        if (onEditPattern_)
            onEditPattern_(activePattern_);
    };
    addAndMakeVisible(editPatternButton_);

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
    if (activeBank_.empty() && !song_.samples().empty())
        return;
    const auto rawBank = stretchCache_.getRawBank();
    engine_.publish(SnapshotBuilder::buildArrangement(song_, activeBank_, engine_.sampleRate(),
                                                     loopStartBar_, loopEndBar_, &rawBank));
}

void ArrangerView::republish()
{
    stretchCache_.requestUpdateAsync(song_, engine_.sampleRate(), [this](SampleBank newBank) {
        juce::MessageManager::callAsync([this, bank = std::move(newBank)]() mutable {
            activeBank_ = std::move(bank);
            syncAllTrackMixes();
            const auto rawBank = stretchCache_.getRawBank();
            engine_.publish(SnapshotBuilder::buildArrangement(song_, activeBank_, engine_.sampleRate(),
                                                             loopStartBar_, loopEndBar_, &rawBank));
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
        republish();
        grid_->repaint();
    }
}

void ArrangerView::blockDeleteRequested(BlockId blockId)
{
    if (onPushUndo_)
        onPushUndo_();
    if (song_.removeBlock(blockId))
    {
        republish();
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
        republish();
        if (trackHeaders_ != nullptr)
            trackHeaders_->updateFromModel();
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
    const float newZoomY = std::clamp(targetZoomY, 0.5f, 2.5f);

    if (std::abs(newZoomX - zoomX_) < 0.001f && std::abs(newZoomY - zoomY_) < 0.001f)
        return;

    const double ratioX = static_cast<double>(newZoomX) / static_cast<double>(zoomX_);
    const int vpX = gridViewport_->getViewPositionX();
    const int newVpX = juce::roundToInt((vpX + anchorInGrid.x) * ratioX - anchorInGrid.x);

    zoomX_ = newZoomX;
    zoomY_ = newZoomY;

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
    const auto anchor = gridViewport_ != nullptr ? gridViewport_->getViewArea().getCentre() : juce::Point<int>(0, 0);
    applyZoom(1.0f, 1.0f, anchor);
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

    if (samplesPerBar > 0.0)
    {
        const double currentBar = static_cast<double>(seq.position()) / samplesPerBar;
        grid_->setPlayheadBar(seq.isPlaying() ? currentBar : -1.0);
    }
}

void ArrangerView::paint(juce::Graphics& g)
{
    g.fillAll(Palette::background);

    constexpr int kToolbarHeight = 44;
    g.setColour(Palette::surface);
    g.fillRect(0, 0, getWidth(), kToolbarHeight);
    g.setColour(Palette::gridLine);
    g.drawHorizontalLine(kToolbarHeight, 0.0f, static_cast<float>(getWidth()));
}

void ArrangerView::resized()
{
    constexpr int kToolbarHeight = 44;
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

    patternCombo_.setBounds(toolbar.removeFromLeft(150));
    toolbar.removeFromLeft(8);
    addPatternButton_.setBounds(toolbar.removeFromLeft(68));
    toolbar.removeFromLeft(8);
    editPatternButton_.setBounds(toolbar.removeFromLeft(88));

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
