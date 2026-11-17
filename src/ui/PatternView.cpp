#include "ui/PatternView.h"

#include "model/Timing.h"
#include "ui/StudioOneZoomCorner.h"
#include "ui/TramiLookAndFeel.h"

#include <cmath>

namespace trami
{

class PatternView::SyncViewport : public juce::Viewport
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
            vBar.setBounds(vBar.getX(), PatternGrid::kRulerHeight, vBar.getWidth(), std::max(0, h - PatternGrid::kRulerHeight - totalSize));

        if (hBar.isVisible())
            hBar.setBounds(hBar.getX(), hBar.getY(), std::max(0, w - totalSize), hBar.getHeight());

        zoomCorner_.setBounds(w - totalSize, h - totalSize, totalSize, totalSize);
    }

private:
    StudioOneZoomCorner zoomCorner_;
};

class PatternView::HeaderCorner : public juce::Component
{
public:
    HeaderCorner()
    {
        setOpaque(true);
    }

    std::function<void(float deltaY)> onScrollY;

    void paint(juce::Graphics& g) override
    {
        g.setColour(juce::Colour(0xff23272d));
        g.fillRect(getLocalBounds());
        g.setColour(juce::Colour(0xff181a1f));
        g.drawHorizontalLine(getHeight() - 1, 0.0f, static_cast<float>(getWidth()));
        g.drawVerticalLine(getWidth() - 1, 0.0f, static_cast<float>(getHeight()));
    }

    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override
    {
        if (onScrollY && wheel.deltaY != 0.0f)
            onScrollY(wheel.deltaY);
    }
};

class PatternView::PatternRuler : public juce::Component, public juce::TooltipClient
{
public:
    PatternRuler(PatternGrid& grid, juce::Viewport& gridViewport)
        : grid_(grid), gridViewport_(gridViewport)
    {
        setOpaque(true);
    }

    void setScrollX(int scrollX)
    {
        if (scrollX_ != scrollX)
        {
            scrollX_ = scrollX;
            repaint();
        }
    }

    int scrollX() const noexcept { return scrollX_; }

    juce::String getTooltip() override { return grid_.getTooltip(); }

    void paint(juce::Graphics& g) override
    {
        juce::Graphics::ScopedSaveState save(g);
        g.addTransform(juce::AffineTransform::translation(static_cast<float>(-scrollX_), 0.0f));
        const int renderWidth = std::max(grid_.getWidth(), scrollX_ + getWidth());
        grid_.paintRuler(g, renderWidth);
    }

    void mouseMove(const juce::MouseEvent& e) override
    {
        grid_.mouseMove(toGridEvent(e));
        setMouseCursor(grid_.getMouseCursor());
    }

    void mouseExit(const juce::MouseEvent& e) override
    {
        grid_.mouseExit(toGridEvent(e));
        setMouseCursor(juce::MouseCursor::NormalCursor);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        grid_.mouseDown(toGridEvent(e));
        repaint();
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        grid_.mouseDrag(toGridEvent(e));
        setMouseCursor(grid_.getMouseCursor());
        repaint();
    }

    void mouseUp(const juce::MouseEvent& e) override
    {
        grid_.mouseUp(toGridEvent(e));
        setMouseCursor(grid_.getMouseCursor());
        repaint();
    }

    void mouseDoubleClick(const juce::MouseEvent& e) override
    {
        grid_.mouseDoubleClick(toGridEvent(e));
        repaint();
    }

    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override
    {
        if (e.mods.isCommandDown() || e.mods.isCtrlDown())
        {
            grid_.mouseWheelMove(toGridEvent(e), wheel);
            return;
        }
        const float delta = (wheel.deltaX != 0.0f) ? wheel.deltaX : wheel.deltaY;
        if (delta != 0.0f)
        {
            const int currentX = gridViewport_.getViewPositionX();
            int deltaX = juce::roundToInt(-delta * 60.0f);
            if (deltaX == 0 && delta != 0.0f)
                deltaX = (delta > 0.0f ? -1 : 1);
            gridViewport_.setViewPosition(currentX + deltaX, gridViewport_.getViewPositionY());
        }
    }

private:
    juce::MouseEvent toGridEvent(const juce::MouseEvent& e) const
    {
        return e.getEventRelativeTo(&grid_).withNewPosition(juce::Point<int>(e.x + scrollX_, e.y));
    }

    PatternGrid& grid_;
    juce::Viewport& gridViewport_;
    int scrollX_ = 0;
};

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
    browserToggle_.setClickingTogglesState(true);
    browserToggle_.setToggleState(showBrowser_, juce::dontSendNotification);
    browserToggle_.setTooltip("Toggle Sample Browser Panel (B)");
    browserToggle_.onClick = [this]() {
        showBrowser_ = browserToggle_.getToggleState();
        if (browserPanel_ != nullptr)
            browserPanel_->setVisible(showBrowser_);
        resized();
    };
    addAndMakeVisible(browserToggle_);

    patternTitle_.setFont(Typography::bold(15.0f));
    patternTitle_.setColour(juce::Label::textColourId, Palette::text);
    patternTitle_.setEditable(false, true, false);
    patternTitle_.setTooltip("Double-click to rename pattern");
    patternTitle_.onTextChange = [this]() {
        const juce::String newName = patternTitle_.getText().trim();
        if (newName.isNotEmpty())
        {
            if (onPushUndo_)
                onPushUndo_();
            song_.renamePattern(currentPattern_, newName.toStdString());
        }
    };
    addAndMakeVisible(patternTitle_);

    // Raster Size Selector
    rasterLabel_.setFont(Typography::bold(12.0f));
    rasterLabel_.setColour(juce::Label::textColourId, Palette::textDim);
    rasterLabel_.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(rasterLabel_);

    rasterCombo_.addItem("1 Bar", 32);
    rasterCombo_.addItem("1/2", 16);
    rasterCombo_.addItem("1/4", 8);
    rasterCombo_.addItem("1/8", 4);
    rasterCombo_.addItem("1/16", 2);
    rasterCombo_.addItem("1/32", 1);
    rasterCombo_.setSelectedId(2, juce::dontSendNotification); // Default 1/16
    rasterCombo_.onChange = [this]() {
        const int steps = rasterCombo_.getSelectedId();
        if (steps > 0 && grid_ != nullptr)
            grid_->setRasterSteps(steps);
    };
    rasterCombo_.setTooltip("Grid Snap Resolution (1 to 1/32)");
    addAndMakeVisible(rasterCombo_);

    // Add Track Button
    addTrackButton_.setTooltip("Add a new sample track to this pattern");
    addTrackButton_.onClick = [this]() { addTrack(); };
    addAndMakeVisible(addTrackButton_);

    // Follow Playhead Toggle
    followToggle_.setClickingTogglesState(true);
    followToggle_.setToggleState(followPlayhead_, juce::dontSendNotification);
    followToggle_.setTooltip("Toggle Playhead Follow (Auto-Scroll) (F)");
    followToggle_.onClick = [this]() {
        setFollowPlayhead(followToggle_.getToggleState());
        if (onShowToast_)
            onShowToast_(followPlayhead_ ? "Playhead Follow: ON" : "Playhead Follow: OFF");
    };
    addAndMakeVisible(followToggle_);

    // Browser Panel
    browserPanel_ = std::make_unique<SampleBrowserPanel>(song_, engine_, stretchCache_, [this](SampleId id) {
        activeSample_ = id;
        updateSampleSelector();
        republish();
    });
    browserPanel_->setOnAddToNewTrack([this](const juce::String& filePath, SampleKind kind) {
        importAndAssignToTrack(filePath, -1, 0, false, kind);
    });
    addAndMakeVisible(browserPanel_.get());

    // Grid & Track headers
    trackHeaders_ = std::make_unique<TrackHeaderList>(song_, currentPattern_, [this](int track) {
        if (const auto* pat = song_.findPattern(currentPattern_))
        {
            if (track >= 0 && track < static_cast<int>(pat->tracks.size()))
            {
                const auto& trk = pat->tracks[static_cast<std::size_t>(track)];
                engine_.sequencer().setTrackMix(track, trk.mix.volume, trk.mix.pan, trk.muted, trk.solo);
            }
        }
        publishSnapshotQuick();
    });
    trackHeaders_->setOnAddTrack([this]() { addTrack(); });
    trackHeaders_->setOnTrackTypeChanged([this](int track, SampleKind newKind) {
        setTrackKind(track, newKind);
    });
    trackHeaders_->setOnTrackRenamed([this](int, const std::string&) {
        if (onPushUndo_)
            onPushUndo_();
    });
    trackHeaders_->setOnDuplicateTrack([this](int track) {
        duplicateTrack(track);
    });
    trackHeaders_->setOnDeleteTrack([this](int track) {
        deleteTrack(track);
    });
    trackHeaders_->setOnMoveTrack([this](int from, int to) {
        moveTrack(from, to);
    });
    trackHeaders_->setOnTrackSelected([this](int track, SampleId sampleId) {
        if (sampleId.isValid())
        {
            activeSample_ = sampleId;
            updateSampleSelector();
        }
        else if (activeSample_.isValid())
        {
            const auto* pat = song_.findPattern(currentPattern_);
            if (pat != nullptr && track >= 0 && track < static_cast<int>(pat->tracks.size()))
            {
                if (onPushUndo_)
                    onPushUndo_();
                song_.setPatternTrackSample(currentPattern_, track, activeSample_);
                trackHeaders_->updateFromModel();
                republish();
            }
        }
    });
    trackHeaders_->setOnFileDropped([this](const juce::String& filePath, int track) {
        fileDroppedOnTrack(filePath, track);
    });
    trackHeaders_->onMouseWheel = [this](const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) {
        if (gridViewport_ != nullptr)
            gridViewport_->mouseWheelMove(e, wheel);
    };
    trackHeaders_->onMarqueeStart = [this](const juce::MouseEvent& e) {
        if (grid_ != nullptr)
        {
            const auto gridPt = grid_->getLocalPoint(trackHeaders_.get(), e.getPosition());
            grid_->startMarquee(gridPt, e.mods);
        }
    };
    trackHeaders_->onMarqueeDrag = [this](const juce::MouseEvent& e) {
        if (grid_ != nullptr)
        {
            const auto gridPt = grid_->getLocalPoint(trackHeaders_.get(), e.getPosition());
            grid_->updateMarquee(gridPt);
        }
    };
    trackHeaders_->onMarqueeEnd = [this](const juce::MouseEvent&) {
        if (grid_ != nullptr)
            grid_->finishMarquee();
    };

    trackHeadersViewport_ = std::make_unique<juce::Viewport>();
    trackHeadersViewport_->setViewedComponent(trackHeaders_.get(), false);
    trackHeadersViewport_->setScrollBarsShown(false, false);
    trackHeadersViewport_->addMouseListener(this, true);
    addAndMakeVisible(trackHeadersViewport_.get());

    grid_ = std::make_unique<PatternGrid>(song_, stretchCache_, currentPattern_, *this);

    gridViewport_ = std::make_unique<SyncViewport>();
    gridViewport_->setViewedComponent(grid_.get(), false);
    gridViewport_->setScrollBarsShown(true, true);
    gridViewport_->onAreaChanged = [this](const juce::Rectangle<int>& area) {
        if (trackHeadersViewport_ != nullptr)
            trackHeadersViewport_->setViewPosition(0, area.getY());
        if (ruler_ != nullptr)
            ruler_->setScrollX(area.getX());
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

    headerCorner_ = std::make_unique<HeaderCorner>();
    headerCorner_->onScrollY = [this](float deltaY) {
        if (gridViewport_ != nullptr && deltaY != 0.0f)
        {
            const int currentY = gridViewport_->getViewPositionY();
            int dy = juce::roundToInt(-deltaY * 60.0f);
            if (dy == 0 && deltaY != 0.0f)
                dy = (deltaY > 0.0f ? -1 : 1);
            gridViewport_->setViewPosition(gridViewport_->getViewPositionX(), currentY + dy);
        }
    };
    addAndMakeVisible(headerCorner_.get());

    ruler_ = std::make_unique<PatternRuler>(*grid_, *gridViewport_);
    addAndMakeVisible(ruler_.get());

    const auto* initialPat = song_.findPattern(currentPattern_);
    const int initialTotalSteps = initialPat != nullptr ? initialPat->lengthSteps() : 32;
    loopStartStep_ = 0;
    loopEndStep_ = initialTotalSteps;
    if (grid_ != nullptr)
        grid_->setLoopRegion(loopStartStep_, loopEndStep_);

    updateLengthSelector();
    updateSampleSelector();
    publishSnapshotQuick();
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
    pasteTargetTrack_ = -1;
    pasteTargetStep_ = -1;
    const auto* pat = song_.findPattern(patternId);
    loopStartStep_ = 0;
    loopEndStep_ = pat != nullptr ? pat->lengthSteps() : 32;
    if (grid_ != nullptr)
        grid_->setLoopRegion(loopStartStep_, loopEndStep_);
    if (onLoopRegionChanged_)
        onLoopRegionChanged_(loopStartStep_, loopEndStep_);
    trackHeaders_->setPattern(patternId);
    grid_->setPattern(patternId);
    if (ruler_ != nullptr)
        ruler_->repaint();
    resetPlayhead();
    updateLengthSelector();
    trackHeaders_->updateFromModel();
    syncAllTrackMixes();
    republish();
    resized();
}

void PatternView::loopRegionChanged(int startStep, int endStep)
{
    loopStartStep_ = startStep;
    loopEndStep_ = endStep;
    republish();
    if (ruler_ != nullptr)
        ruler_->repaint();
    if (onLoopRegionChanged_)
        onLoopRegionChanged_(loopStartStep_, loopEndStep_);
}

void PatternView::setLoopRegion(int startStep, int endStep)
{
    const auto* pat = song_.findPattern(currentPattern_);
    const int totalSteps = pat != nullptr ? pat->lengthSteps() : kStepsPerBar;
    loopStartStep_ = std::clamp(startStep, 0, totalSteps);
    loopEndStep_ = (endStep <= 0) ? totalSteps : std::clamp(endStep, loopStartStep_ + 1, totalSteps);
    if (grid_ != nullptr)
        grid_->setLoopRegion(loopStartStep_, loopEndStep_);
    republish();
    if (onLoopRegionChanged_)
        onLoopRegionChanged_(loopStartStep_, loopEndStep_);
}

void PatternView::playheadSeekRequested(int step)
{
    const Timing timing(song_.bpm(), engine_.sampleRate(), song_.swing());
    const std::int64_t targetSample = timing.stepToSample(step);
    engine_.sequencer().requestSeek(targetSample);
    if (grid_ != nullptr)
        grid_->setPlayheadStep(static_cast<double>(step));
    if (ruler_ != nullptr)
        ruler_->repaint();
    if (followPlayhead_)
        scrollToPlayheadIfNeeded(static_cast<double>(step));
}

void PatternView::setFollowPlayhead(bool enabled)
{
    followPlayhead_ = enabled;
    followToggle_.setToggleState(followPlayhead_, juce::dontSendNotification);
    if (followPlayhead_ && grid_ != nullptr)
    {
        scrollToPlayheadIfNeeded(grid_->playheadStep());
    }
}

void PatternView::scrollToPlayheadIfNeeded(double phStep)
{
    if (gridViewport_ == nullptr || grid_ == nullptr || phStep < 0.0)
        return;

    const int phX = grid_->stepToX(phStep);
    const int viewX = gridViewport_->getViewPositionX();
    const int viewW = gridViewport_->getViewWidth();

    if (viewW <= 0)
        return;

    if (grid_->isMouseButtonDown()
        || (ruler_ != nullptr && ruler_->isMouseButtonDown())
        || gridViewport_->getHorizontalScrollBar().isMouseButtonDown()
        || gridViewport_->getVerticalScrollBar().isMouseButtonDown())
    {
        return;
    }

    // Page-scroll: when the playhead approaches the right edge, page forward;
    // when it loops or jumps back to the left, page backward.
    if (phX >= viewX + viewW - 10 || phX < viewX)
    {
        const int targetX = std::max(0, phX - 40);
        gridViewport_->setViewPosition(targetX, gridViewport_->getViewPositionY());
    }
}

void PatternView::updateTrackHeaders()
{
    if (trackHeaders_ != nullptr)
    {
        trackHeaders_->setPattern(currentPattern_);
        trackHeaders_->updateFromModel();
    }
    if (grid_ != nullptr)
    {
        grid_->setPattern(currentPattern_);
        grid_->repaint();
    }
}

void PatternView::addTrack()
{
    if (onPushUndo_)
        onPushUndo_();
    song_.addTrackToPattern(currentPattern_, activeSample_);
    trackHeaders_->updateFromModel();
    republish();
    resized();
}

void PatternView::duplicateTrack(int track)
{
    if (onPushUndo_)
        onPushUndo_();
    if (song_.duplicateTrackInPattern(currentPattern_, track) >= 0)
    {
        trackHeaders_->updateFromModel();
        republish();
        resized();
        grid_->repaint();
    }
}

void PatternView::deleteTrack(int track)
{
    if (onPushUndo_)
        onPushUndo_();
    if (song_.removeTrackFromPattern(currentPattern_, track))
    {
        autoAdjustPatternLength();
        trackHeaders_->updateFromModel();
        republish();
        resized();
        grid_->repaint();
    }
}

void PatternView::moveTrack(int from, int to)
{
    if (onPushUndo_)
        onPushUndo_();
    if (song_.movePatternTrack(currentPattern_, from, to))
    {
        trackHeaders_->updateFromModel();
        republish();
        resized();
        grid_->repaint();
    }
}

void PatternView::duplicateCurrentPattern()
{
    if (onPushUndo_)
        onPushUndo_();
    if (auto newId = song_.duplicatePattern(currentPattern_))
    {
        setPattern(*newId);
    }
}

void PatternView::deleteCurrentPattern()
{
    if (song_.patterns().size() <= 1)
        return;

    if (onPushUndo_)
        onPushUndo_();

    const auto oldId = currentPattern_;
    song_.removePattern(oldId);
    if (!song_.patterns().empty())
        setPattern(song_.patterns().front().id);
}

void PatternView::setTrackKind(int trackIndex, SampleKind newKind)
{
    if (onPushUndo_)
        onPushUndo_();

    const auto sid = song_.patternTrackSample(currentPattern_, trackIndex);
    if (sid.isValid())
        stretchCache_.invalidateSample(sid);

    song_.setPatternTrackKind(currentPattern_, trackIndex, newKind);

    updateSampleSelector();
    trackHeaders_->updateFromModel();
    republish();
    grid_->repaint();
}

void PatternView::toggleCurrentTrackType()
{
}

void PatternView::syncAllTrackMixes()
{
    const auto* pat = song_.findPattern(currentPattern_);
    if (pat == nullptr)
        return;

    for (std::size_t t = 0; t < pat->tracks.size() && t < PlaybackSnapshot::kMaxTrackMixes; ++t)
    {
        const auto& trk = pat->tracks[t];
        engine_.sequencer().setTrackMix(static_cast<int>(t), trk.mix.volume, trk.mix.pan, trk.muted, trk.solo);
    }
}

void PatternView::publishSnapshotQuick(const std::vector<DragClipOverride>* overrides)
{
    const auto rawBank = stretchCache_.getRawBank();
    const auto& bankToUse = activeBank_.empty() ? rawBank : activeBank_;
    if (bankToUse.empty() && !song_.samples().empty())
        return;
    engine_.publish(SnapshotBuilder::buildPattern(song_, currentPattern_, bankToUse, engine_.sampleRate(),
                                                 loopStartStep_, loopEndStep_, &rawBank, overrides, &stretchCache_));
}

void PatternView::repaintGrid()
{
    if (grid_ != nullptr)
        grid_->repaint();
}

void PatternView::clipsDragging(const std::vector<DragClipOverride>& items)
{
    publishSnapshotQuick(&items);
}

void PatternView::clipsDragEnded()
{
    publishSnapshotQuick(nullptr);
}

void PatternView::updateLengthSelector()
{
    const Pattern* p = song_.findPattern(currentPattern_);
    if (p != nullptr)
        patternTitle_.setText(p->name, juce::dontSendNotification);
}

void PatternView::updateSampleSelector()
{
    if (activeSample_.isValid() && song_.findSample(activeSample_) == nullptr)
        activeSample_ = {};
    if (!activeSample_.isValid() && !song_.samples().empty())
        activeSample_ = song_.samples().front().id;
}

void PatternView::republish()
{
    stretchCache_.requestUpdateAsync(song_, engine_.sampleRate(), [this](SampleBank newBank) {
        juce::MessageManager::callAsync([this, bank = std::move(newBank)]() mutable {
            activeBank_ = std::move(bank);
            syncAllTrackMixes();
            const auto rawBank = stretchCache_.getRawBank();
            engine_.publish(SnapshotBuilder::buildPattern(song_, currentPattern_, activeBank_, engine_.sampleRate(),
                                                         loopStartStep_, loopEndStep_, &rawBank, nullptr, &stretchCache_));
            if (trackHeaders_ != nullptr)
                trackHeaders_->updateFromModel();
            if (grid_ != nullptr)
                grid_->repaint();
        });
    });
}

void PatternView::toggleRecording()
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

void PatternView::onRecordFinished(AudioData recorded)
{
    if (onPushUndo_)
        onPushUndo_();

    Sample s;
    s.name = ("Take " + juce::String(song_.samples().size() + 1)).toStdString();
    s.durationSeconds = static_cast<double>(recorded.numFrames()) / engine_.sampleRate();
    s.kind = SampleKind::OneShot;

    const auto id = song_.addSample(s);
    stretchCache_.setRawAudio(id, std::make_shared<const AudioData>(std::move(recorded)));
    activeSample_ = id;

    const int trackIdx = song_.addTrackToPattern(currentPattern_, id);
    song_.placeClip(currentPattern_, trackIdx, id, 0);

    updateSampleSelector();
    publishSnapshotQuick();
    republish();
    resized();
}

void PatternView::clipClicked(ClipId)
{
}

void PatternView::clipMoved(ClipId clipId, int newTrack, double newStep)
{
    clipsMoved({{clipId, newTrack, newStep}});
}

void PatternView::clipsMoved(const std::vector<PatternGrid::ClipMoveInfo>& moves)
{
    if (moves.empty())
        return;

    const auto* p = song_.findPattern(currentPattern_);
    if (p != nullptr)
    {
        double maxEndStep = 0.0;
        for (const auto& m : moves)
        {
            if (const auto* c = song_.findClip(currentPattern_, m.clipId))
                maxEndStep = std::max(maxEndStep, m.newStep + c->lengthSteps);
        }
        if (maxEndStep > static_cast<double>(p->lengthSteps()))
        {
            const int neededBars = std::max(p->lengthBars + 1,
                                            static_cast<int>(std::ceil(maxEndStep / static_cast<double>(kStepsPerBar))));
            patternLengthExtensionRequested(neededBars);
        }
    }

    if (onPushUndo_)
        onPushUndo_();

    for (const auto& m : moves)
        song_.moveClip(currentPattern_, m.clipId, m.newTrack, m.newStep);

    autoAdjustPatternLength();

    publishSnapshotQuick();
    republish();
    if (grid_ != nullptr)
        grid_->repaint();
}

void PatternView::clipResized(ClipId clipId, double newStartStep, double newLengthSteps, double newOffsetSteps)
{
    clipsResized({{clipId, newStartStep, newLengthSteps, newOffsetSteps}});
}

void PatternView::clipsResized(const std::vector<PatternGrid::ClipResizeInfo>& resizes)
{
    if (resizes.empty())
        return;

    const auto* p = song_.findPattern(currentPattern_);
    if (p != nullptr)
    {
        double maxEndStep = 0.0;
        for (const auto& r : resizes)
            maxEndStep = std::max(maxEndStep, r.newStartStep + r.newLengthSteps);
        if (maxEndStep > static_cast<double>(p->lengthSteps()))
        {
            const int neededBars = std::max(p->lengthBars + 1,
                                            static_cast<int>(std::ceil(maxEndStep / static_cast<double>(kStepsPerBar))));
            patternLengthExtensionRequested(neededBars);
        }
    }

    if (onPushUndo_)
        onPushUndo_();

    for (const auto& r : resizes)
        song_.resizeClip(currentPattern_, r.clipId, r.newStartStep, r.newLengthSteps, r.newOffsetSteps);

    autoAdjustPatternLength();

    publishSnapshotQuick();
    republish();
    if (grid_ != nullptr)
        grid_->repaint();
}

void PatternView::patternLengthExtensionRequested(int minBars)
{
    const auto* p = song_.findPattern(currentPattern_);
    if (p == nullptr || minBars <= p->lengthBars)
        return;

    const int targetBars = std::clamp(minBars, 1, 9999);
    if (song_.setPatternLength(currentPattern_, targetBars))
    {
        updateLengthSelector();
        republish();
        resized();
    }
}

void PatternView::autoAdjustPatternLength()
{
    const auto* p = song_.findPattern(currentPattern_);
    if (p == nullptr)
        return;

    double maxClipEndStep = 0.0;
    for (const auto& track : p->tracks)
    {
        for (const auto& clip : track.clips)
        {
            maxClipEndStep = std::max(maxClipEndStep, clip.startStep + clip.lengthSteps);
        }
    }

    // A song always starts with and has at least 1 bar.
    // If clips exist, length in bars is exactly enough to contain all clips.
    const int neededBars = std::max(1, static_cast<int>(std::ceil((maxClipEndStep - 1e-4) / static_cast<double>(kStepsPerBar))));

    if (neededBars != p->lengthBars)
    {
        if (song_.setPatternLength(currentPattern_, neededBars))
        {
            if (loopEndStep_ > neededBars * kStepsPerBar || loopEndStep_ == p->lengthBars * kStepsPerBar)
            {
                loopEndStep_ = neededBars * kStepsPerBar;
                if (grid_ != nullptr)
                    grid_->setLoopRegion(loopStartStep_, loopEndStep_);
                if (onLoopRegionChanged_)
                    onLoopRegionChanged_(loopStartStep_, loopEndStep_);
            }
            republish();
            resized();
            if (grid_ != nullptr)
                grid_->repaint();
        }
    }
}

void PatternView::clipEnvelopeChanged(ClipId clipId, float gain, float fadeInSteps, float fadeOutSteps)
{
    if (onPushUndo_)
        onPushUndo_();
    song_.setClipEnvelope(currentPattern_, clipId, gain, fadeInSteps, fadeOutSteps);
    publishSnapshotQuick();
    republish();
    grid_->repaint();
}

void PatternView::clipReverseRequested(ClipId clipId)
{
    clipsReverseRequested({clipId});
}

void PatternView::clipsReverseRequested(const std::vector<ClipId>& clipIds)
{
    if (clipIds.empty())
        return;

    if (onPushUndo_)
        onPushUndo_();

    bool anyReversed = false;
    for (auto cid : clipIds)
    {
        const bool current = song_.isClipReversed(currentPattern_, cid);
        song_.setClipReversed(currentPattern_, cid, !current);
        if (!current)
            anyReversed = true;
    }

    if (onShowToast_)
    {
        const int count = static_cast<int>(clipIds.size());
        if (count == 1)
            onShowToast_(anyReversed ? "Clip reversed" : "Clip restored to normal direction");
        else
            onShowToast_(juce::String::formatted("%d clips direction toggled", count));
    }

    publishSnapshotQuick();
    republish();
    if (grid_ != nullptr)
        grid_->repaint();
}

void PatternView::clipsTransposeDelta(const std::vector<ClipId>& clipIds, int deltaSemitones)
{
    if (clipIds.empty())
        return;

    if (onPushUndo_)
        onPushUndo_();

    int lastTranspose = 0;
    for (auto cid : clipIds)
    {
        const int cur = song_.clipTranspose(currentPattern_, cid);
        const int next = std::clamp(cur + deltaSemitones, -48, 48);
        song_.setClipTranspose(currentPattern_, cid, next);
        lastTranspose = next;
    }

    if (onShowToast_)
    {
        const int count = static_cast<int>(clipIds.size());
        if (count == 1)
            onShowToast_(juce::String::formatted("Transpose: %+d st", lastTranspose));
        else
            onShowToast_(juce::String::formatted("Transposed %d clips (%+d st)", count, deltaSemitones));
    }

    publishSnapshotQuick();
    republish();
    if (grid_ != nullptr)
        grid_->repaint();
}

void PatternView::clipsTransposeSet(const std::vector<ClipId>& clipIds, int semitones)
{
    if (clipIds.empty())
        return;

    if (onPushUndo_)
        onPushUndo_();

    const int target = std::clamp(semitones, -48, 48);
    for (auto cid : clipIds)
        song_.setClipTranspose(currentPattern_, cid, target);

    if (onShowToast_)
    {
        const int count = static_cast<int>(clipIds.size());
        if (count == 1)
            onShowToast_(juce::String::formatted("Transpose: %+d st", target));
        else
            onShowToast_(juce::String::formatted("Set %d clips transpose: %+d st", count, target));
    }

    publishSnapshotQuick();
    republish();
    if (grid_ != nullptr)
        grid_->repaint();
}

void PatternView::clipDeleteRequested(ClipId clipId)
{
    clipsDeleteRequested({clipId});
}

void PatternView::clipsDeleteRequested(const std::vector<ClipId>& clipIds)
{
    if (clipIds.empty())
        return;

    const auto* pat = song_.findPattern(currentPattern_);
    ClipId nextClipId;

    if (pat != nullptr)
    {
        const auto lastDeletedId = clipIds.back();
        for (const auto& track : pat->tracks)
        {
            const auto it = std::find_if(track.clips.begin(), track.clips.end(),
                                         [lastDeletedId](const Clip& c) { return c.id == lastDeletedId; });
            if (it != track.clips.end())
            {
                const int curStep = it->startStep;
                const Clip* nextClip = nullptr;
                const Clip* prevClip = nullptr;

                for (const auto& c : track.clips)
                {
                    if (std::find(clipIds.begin(), clipIds.end(), c.id) != clipIds.end())
                        continue;

                    if (c.startStep >= curStep)
                    {
                        if (nextClip == nullptr || c.startStep < nextClip->startStep)
                            nextClip = &c;
                    }
                    else
                    {
                        if (prevClip == nullptr || c.startStep > prevClip->startStep)
                            prevClip = &c;
                    }
                }

                if (nextClip != nullptr)
                    nextClipId = nextClip->id;
                else if (prevClip != nullptr)
                    nextClipId = prevClip->id;

                break;
            }
        }
    }

    if (onPushUndo_)
        onPushUndo_();

    for (const auto cid : clipIds)
        song_.removeClip(currentPattern_, cid);

    autoAdjustPatternLength();

    if (onShowToast_)
    {
        const int count = static_cast<int>(clipIds.size());
        onShowToast_(juce::String::formatted("Deleted %d clip%s", count, count > 1 ? "s" : ""));
    }

    if (grid_ != nullptr)
        grid_->setSelectedClip(nextClipId);

    publishSnapshotQuick();
    republish();
    if (grid_ != nullptr)
        grid_->repaint();
}

void PatternView::clipCutRequested(ClipId clipId)
{
    if (grid_ != nullptr)
        grid_->setSelectedClip(clipId);
    cutSelectedClip();
}

void PatternView::clipCopyRequested(ClipId clipId)
{
    if (grid_ != nullptr)
        grid_->setSelectedClip(clipId);
    copySelectedClip();
}

void PatternView::clipPasteRequested(int track, int step)
{
    pasteClip(track, step);
}

void PatternView::clipDuplicateRequested(ClipId clipId)
{
    if (grid_ != nullptr)
        grid_->setSelectedClip(clipId);
    duplicateSelectedClip();
}

void PatternView::copySelectedClip()
{
    if (grid_ == nullptr)
        return;

    const auto* pat = song_.findPattern(currentPattern_);
    if (!pat)
        return;

    const auto& selected = grid_->selectedClips();
    std::vector<ClipId> targetIds = selected;
    if (targetIds.empty() && grid_->selectedClip().isValid())
        targetIds.push_back(grid_->selectedClip());

    if (targetIds.empty())
        return;

    ClipClipboard cb;
    cb.sourcePatternId = currentPattern_;
    cb.minStartStep = std::numeric_limits<int>::max();
    cb.maxEndStep = 0;

    for (int t = 0; t < static_cast<int>(pat->tracks.size()); ++t)
    {
        const auto& track = pat->tracks[static_cast<std::size_t>(t)];
        for (const auto& c : track.clips)
        {
            if (std::find(targetIds.begin(), targetIds.end(), c.id) != targetIds.end())
            {
                const int span = grid_->clipSpan(c, t);
                ClipboardClipItem item;
                item.sampleId = c.sampleId;
                item.sourceTrackIndex = t;
                item.sourceTrackName = track.name;
                item.sourceTrackSampleId = track.sampleId;
                item.sourceTrackKind = track.kindOverride;
                item.lengthSteps = c.lengthSteps;
                item.offsetSteps = c.offsetSteps;
                item.gain = c.gain;
                item.fadeInSteps = c.fadeInSteps;
                item.fadeOutSteps = c.fadeOutSteps;
                item.reversed = c.reversed;
                item.originalStartStep = c.startStep;
                item.span = span;

                cb.minStartStep = std::min(cb.minStartStep, c.startStep);
                const double endStep = c.startStep + (span > 0.0 ? span : (c.lengthSteps > 0.0 ? c.lengthSteps : 4.0));
                cb.maxEndStep = std::max(cb.maxEndStep, endStep);
                cb.items.push_back(std::move(item));
            }
        }
    }

    if (cb.items.empty())
        return;

    cb.primaryTrackIndex = cb.items.front().sourceTrackIndex;
    pasteTargetTrack_ = -1;
    pasteTargetStep_ = -1;
    clipClipboard_ = std::move(cb);

    if (onShowToast_)
    {
        if (clipClipboard_->items.size() == 1)
            onShowToast_("Clip copied to clipboard");
        else
            onShowToast_(juce::String::formatted("%d clips copied to clipboard", static_cast<int>(clipClipboard_->items.size())));
    }
}

void PatternView::cutSelectedClip()
{
    if (grid_ == nullptr)
        return;

    const auto& selected = grid_->selectedClips();
    std::vector<ClipId> toDelete = selected;
    if (toDelete.empty() && grid_->selectedClip().isValid())
        toDelete.push_back(grid_->selectedClip());

    if (toDelete.empty())
        return;

    copySelectedClip();
    clipsDeleteRequested(toDelete);
    if (onShowToast_)
    {
        if (toDelete.size() == 1)
            onShowToast_("Clip cut");
        else
            onShowToast_(juce::String::formatted("%d clips cut", static_cast<int>(toDelete.size())));
    }
}

int PatternView::findOrCreateRespectiveTrack(PatternId destPatternId,
                                             int sourceTrackIndex,
                                             const std::string& sourceTrackName,
                                             SampleId sourceTrackSampleId,
                                             SampleId clipSampleId,
                                             std::optional<SampleKind> sourceTrackKind,
                                             std::unordered_set<int>& claimedDestTracks)
{
    auto* destPat = song_.findPattern(destPatternId);
    if (destPat == nullptr)
        return -1;

    const SampleId sampleToMatch = sourceTrackSampleId.isValid() ? sourceTrackSampleId : clipSampleId;
    const juce::String trimmedSourceName = juce::String(sourceTrackName).trim();

    // 1. Check if track at sourceTrackIndex exists and matches sample or is empty
    if (sourceTrackIndex >= 0 && sourceTrackIndex < static_cast<int>(destPat->tracks.size()))
    {
        if (claimedDestTracks.find(sourceTrackIndex) == claimedDestTracks.end())
        {
            auto& candidateTrack = destPat->tracks[static_cast<std::size_t>(sourceTrackIndex)];
            if (sampleToMatch.isValid() && candidateTrack.sampleId == sampleToMatch)
            {
                claimedDestTracks.insert(sourceTrackIndex);
                return sourceTrackIndex;
            }
            if (!candidateTrack.sampleId.isValid() && candidateTrack.clips.empty())
            {
                if (sampleToMatch.isValid())
                    song_.setPatternTrackSample(destPatternId, sourceTrackIndex, sampleToMatch);
                if (trimmedSourceName.isNotEmpty())
                    song_.renamePatternTrack(destPatternId, sourceTrackIndex, trimmedSourceName.toStdString());
                if (sourceTrackKind.has_value())
                    song_.setPatternTrackKind(destPatternId, sourceTrackIndex, *sourceTrackKind);
                claimedDestTracks.insert(sourceTrackIndex);
                return sourceTrackIndex;
            }
        }
    }

    // 2. Exact SampleId match on any other unclaimed track in destination pattern
    if (sampleToMatch.isValid())
    {
        for (int i = 0; i < static_cast<int>(destPat->tracks.size()); ++i)
        {
            if (claimedDestTracks.find(i) == claimedDestTracks.end() &&
                destPat->tracks[static_cast<std::size_t>(i)].sampleId == sampleToMatch)
            {
                claimedDestTracks.insert(i);
                return i;
            }
        }
    }

    // 3. Track Name match on any other unclaimed track
    if (trimmedSourceName.isNotEmpty())
    {
        for (int i = 0; i < static_cast<int>(destPat->tracks.size()); ++i)
        {
            if (claimedDestTracks.find(i) == claimedDestTracks.end() &&
                trimmedSourceName.equalsIgnoreCase(juce::String(destPat->tracks[static_cast<std::size_t>(i)].name).trim()))
            {
                if (sampleToMatch.isValid() && !destPat->tracks[static_cast<std::size_t>(i)].sampleId.isValid())
                    song_.setPatternTrackSample(destPatternId, i, sampleToMatch);
                claimedDestTracks.insert(i);
                return i;
            }
        }
    }

    // 4. Any other unclaimed empty track in destination pattern
    for (int i = 0; i < static_cast<int>(destPat->tracks.size()); ++i)
    {
        if (claimedDestTracks.find(i) == claimedDestTracks.end())
        {
            auto& candidateTrack = destPat->tracks[static_cast<std::size_t>(i)];
            if (!candidateTrack.sampleId.isValid() && candidateTrack.clips.empty())
            {
                if (sampleToMatch.isValid())
                    song_.setPatternTrackSample(destPatternId, i, sampleToMatch);
                if (trimmedSourceName.isNotEmpty())
                    song_.renamePatternTrack(destPatternId, i, trimmedSourceName.toStdString());
                if (sourceTrackKind.has_value())
                    song_.setPatternTrackKind(destPatternId, i, *sourceTrackKind);
                claimedDestTracks.insert(i);
                return i;
            }
        }
    }

    // 5. Track DOES NOT EXIST for this clip -> CREATE IT!
    if (static_cast<int>(destPat->tracks.size()) < kTracksPerPattern)
    {
        const int newTrackIdx = song_.addTrackToPattern(destPatternId, sampleToMatch);
        if (newTrackIdx >= 0)
        {
            if (trimmedSourceName.isNotEmpty())
                song_.renamePatternTrack(destPatternId, newTrackIdx, trimmedSourceName.toStdString());
            if (sourceTrackKind.has_value())
                song_.setPatternTrackKind(destPatternId, newTrackIdx, *sourceTrackKind);
            claimedDestTracks.insert(newTrackIdx);
            return newTrackIdx;
        }
    }

    // Fallback: clamped sourceTrackIndex
    return std::clamp(sourceTrackIndex, 0, static_cast<int>(destPat->tracks.size()) - 1);
}

void PatternView::pasteClip(int targetTrack, int targetStep)
{
    if (!clipClipboard_.has_value() || clipClipboard_->items.empty())
        return;

    const auto* pat = song_.findPattern(currentPattern_);
    if (!pat || pat->tracks.empty())
        return;

    const auto& cb = *clipClipboard_;
    const bool isCrossPattern = (cb.sourcePatternId != currentPattern_);
    const int patLength = pat->lengthSteps();
    const int initialTrackCount = static_cast<int>(pat->tracks.size());

    // Map source track index -> destination track index for this paste operation
    std::unordered_map<int, int> sourceToDestTrackMap;
    std::unordered_set<int> claimedDestTracks;

    auto getDestTrack = [&](const ClipboardClipItem& item) -> int {
        if (isCrossPattern)
        {
            auto it = sourceToDestTrackMap.find(item.sourceTrackIndex);
            if (it != sourceToDestTrackMap.end())
                return it->second;

            int destT = findOrCreateRespectiveTrack(currentPattern_,
                                                    item.sourceTrackIndex,
                                                    item.sourceTrackName,
                                                    item.sourceTrackSampleId,
                                                    item.sampleId,
                                                    item.sourceTrackKind,
                                                    claimedDestTracks);
            sourceToDestTrackMap[item.sourceTrackIndex] = destT;
            return destT;
        }
        else
        {
            // Within same pattern
            int baseTrack = (targetTrack >= 0) ? targetTrack : ((pasteTargetTrack_ >= 0) ? pasteTargetTrack_ : item.sourceTrackIndex);
            if (cb.items.size() == 1)
                return std::clamp(baseTrack, 0, static_cast<int>(song_.findPattern(currentPattern_)->tracks.size()) - 1);

            const int offset = baseTrack - cb.primaryTrackIndex;
            return std::clamp(item.sourceTrackIndex + offset, 0, static_cast<int>(song_.findPattern(currentPattern_)->tracks.size()) - 1);
        }
    };

    int baseStep = targetStep;
    if (baseStep < 0)
    {
        if (isCrossPattern)
        {
            // For cross-pattern paste without explicit step target, keep exact clip start steps
            baseStep = -1;
        }
        else
        {
            baseStep = (pasteTargetStep_ >= 0) ? pasteTargetStep_ : -1;
            if (grid_ != nullptr)
            {
                const double ph = grid_->playheadStep();
                if (ph >= 0.0 && ph < static_cast<double>(patLength) && std::abs(ph - cb.minStartStep) > 0.001)
                {
                    const int r = grid_->rasterSteps();
                    baseStep = (static_cast<int>(ph) / r) * r;
                }
            }
        }
    }

    if (onPushUndo_)
        onPushUndo_();

    std::vector<ClipId> newClipIds;
    int maxPastedEndStep = 0;
    int lastPastedTrack = 0;

    for (const auto& item : cb.items)
    {
        const int destTrack = getDestTrack(item);
        if (destTrack < 0)
            continue;

        double destStep = 0.0;
        if (baseStep >= 0)
        {
            destStep = static_cast<double>(baseStep) + (item.originalStartStep - cb.minStartStep);
        }
        else
        {
            if (isCrossPattern)
            {
                destStep = item.originalStartStep;
            }
            else
            {
                const double span = item.span > 0.0 ? item.span : (item.lengthSteps > 0.0 ? item.lengthSteps : 4.0);
                destStep = item.originalStartStep + span;
            }
        }

        if (destStep >= static_cast<double>(patLength))
            destStep = std::fmod(destStep, static_cast<double>(patLength));

        auto newClipId = song_.placeClip(currentPattern_, destTrack, item.sampleId, destStep, item.lengthSteps, item.offsetSteps);
        if (newClipId.has_value())
        {
            song_.setClipEnvelope(currentPattern_, *newClipId, item.gain, item.fadeInSteps, item.fadeOutSteps);
            if (item.reversed)
                song_.setClipReversed(currentPattern_, *newClipId, true);
            newClipIds.push_back(*newClipId);
            lastPastedTrack = destTrack;
            const double span = item.span > 0.0 ? item.span : (item.lengthSteps > 0.0 ? item.lengthSteps : 4.0);
            maxPastedEndStep = std::max(maxPastedEndStep, juce::roundToInt(destStep + span));
        }
    }

    if (!newClipIds.empty())
    {
        pasteTargetTrack_ = lastPastedTrack;
        pasteTargetStep_ = maxPastedEndStep < patLength ? maxPastedEndStep : 0;

        if (grid_ != nullptr)
        {
            grid_->setSelectedClips(newClipIds);
            grid_->setCursorPosition(lastPastedTrack, pasteTargetStep_);
        }

        trackHeaders_->updateFromModel();
        syncAllTrackMixes();
        autoAdjustPatternLength();
        publishSnapshotQuick();
        republish();
        resized();
        if (grid_ != nullptr)
            grid_->repaint();

        const int currentTrackCount = song_.findPattern(currentPattern_) != nullptr ? static_cast<int>(song_.findPattern(currentPattern_)->tracks.size()) : initialTrackCount;
        const int createdCount = currentTrackCount - initialTrackCount;

        if (onShowToast_)
        {
            if (isCrossPattern)
            {
                if (createdCount > 0)
                {
                    onShowToast_(juce::String::formatted("Pasted %d clip%s and created %d track%s",
                                                          static_cast<int>(newClipIds.size()),
                                                          newClipIds.size() > 1 ? "s" : "",
                                                          createdCount,
                                                          createdCount > 1 ? "s" : ""));
                }
                else if (newClipIds.size() == 1)
                    onShowToast_("Clip pasted to respective track");
                else
                    onShowToast_(juce::String::formatted("%d clips pasted to respective tracks", static_cast<int>(newClipIds.size())));
            }
            else
            {
                if (newClipIds.size() == 1)
                    onShowToast_("Clip pasted");
                else
                    onShowToast_(juce::String::formatted("%d clips pasted", static_cast<int>(newClipIds.size())));
            }
        }
    }
}

void PatternView::duplicateSelectedClip()
{
    if (grid_ == nullptr)
        return;

    const auto& selected = grid_->selectedClips();
    std::vector<ClipId> targetIds = selected;
    if (targetIds.empty() && grid_->selectedClip().isValid())
        targetIds.push_back(grid_->selectedClip());

    if (targetIds.empty())
        return;

    copySelectedClip();
    if (!clipClipboard_.has_value() || clipClipboard_->items.empty())
        return;

    const auto* pat = song_.findPattern(currentPattern_);
    if (!pat)
        return;

    const auto& cb = *clipClipboard_;
    const double spanTotal = cb.maxEndStep - cb.minStartStep;
    const double stepOffset = spanTotal > 0.0 ? spanTotal : 4.0;

    double maxNeededEnd = 0.0;
    for (const auto& item : cb.items)
        maxNeededEnd = std::max(maxNeededEnd, item.originalStartStep + stepOffset + (item.lengthSteps > 0.0 ? item.lengthSteps : 4.0));
    const int neededBars = std::max(1, static_cast<int>(std::ceil((maxNeededEnd - 1e-4) / static_cast<double>(kStepsPerBar))));
    if (neededBars > pat->lengthBars)
    {
        song_.setPatternLength(currentPattern_, neededBars);
        pat = song_.findPattern(currentPattern_);
    }

    if (onPushUndo_)
        onPushUndo_();

    std::vector<ClipId> newClipIds;
    for (const auto& item : cb.items)
    {
        const double destStep = item.originalStartStep + stepOffset;
        if (destStep >= pat->lengthSteps())
            continue;

        auto newClipId = song_.placeClip(currentPattern_, item.sourceTrackIndex, item.sampleId, destStep, item.lengthSteps, item.offsetSteps);
        if (newClipId.has_value())
        {
            song_.setClipEnvelope(currentPattern_, *newClipId, item.gain, item.fadeInSteps, item.fadeOutSteps);
            if (item.reversed)
                song_.setClipReversed(currentPattern_, *newClipId, true);
            newClipIds.push_back(*newClipId);
        }
    }

    if (!newClipIds.empty())
    {
        autoAdjustPatternLength();
        grid_->setSelectedClips(newClipIds);
        grid_->setCursorPosition(-1, -1);
        if (onShowToast_)
        {
            if (newClipIds.size() == 1)
                onShowToast_("Clip duplicated");
            else
                onShowToast_(juce::String::formatted("%d clips duplicated", static_cast<int>(newClipIds.size())));
        }
        publishSnapshotQuick();
        republish();
        grid_->repaint();
    }
}

void PatternView::clipDuplicatedTo(ClipId clipId, int track, double startStep)
{
    const auto* pat = song_.findPattern(currentPattern_);
    if (!pat || track < 0 || track >= static_cast<int>(pat->tracks.size()))
        return;

    const Clip* clip = nullptr;
    for (const auto& t : pat->tracks)
    {
        for (const auto& c : t.clips)
        {
            if (c.id == clipId)
            {
                clip = &c;
                break;
            }
        }
    }
    if (clip == nullptr)
        return;

    const int neededBars = std::max(1, static_cast<int>(std::ceil(((startStep + clip->lengthSteps) - 1e-4) / static_cast<double>(kStepsPerBar))));
    if (neededBars > pat->lengthBars)
    {
        song_.setPatternLength(currentPattern_, neededBars);
        pat = song_.findPattern(currentPattern_);
    }

    if (onPushUndo_)
        onPushUndo_();

    auto newClipId = song_.placeClip(currentPattern_, track, clip->sampleId, startStep, clip->lengthSteps, clip->offsetSteps);
    if (newClipId.has_value())
    {
        song_.setClipEnvelope(currentPattern_, *newClipId, clip->gain, clip->fadeInSteps, clip->fadeOutSteps);
        pasteTargetTrack_ = track;
        pasteTargetStep_ = juce::roundToInt(startStep);
        autoAdjustPatternLength();
        if (grid_ != nullptr)
            grid_->setSelectedClip(*newClipId);
        publishSnapshotQuick();
        republish();
        if (grid_ != nullptr)
            grid_->repaint();
    }
}

void PatternView::gridCellClicked(int track, int step)
{
    pasteTargetTrack_ = track;
    pasteTargetStep_ = step;
    if (grid_ != nullptr)
    {
        grid_->setSelectedClip({});
        grid_->setCursorPosition(track, step);
    }
}

void PatternView::gridCellDoubleClicked(int track, int step)
{
    const auto* pat = song_.findPattern(currentPattern_);
    if (!pat || track < 0 || track >= static_cast<int>(pat->tracks.size()))
        return;

    SampleId sid = pat->tracks[static_cast<std::size_t>(track)].sampleId;
    if (!sid.isValid())
        sid = activeSample_;
    if (!sid.isValid())
        return;

    if (onPushUndo_)
        onPushUndo_();

    auto newClipId = song_.placeClip(currentPattern_, track, sid, step);
    if (newClipId.has_value() && grid_ != nullptr)
    {
        grid_->setSelectedClip(*newClipId);
        grid_->clearCursor();
    }
    autoAdjustPatternLength();
    publishSnapshotQuick();
    republish();
    if (grid_ != nullptr)
        grid_->repaint();
}

void PatternView::fileDroppedOnGrid(const juce::String& filePath, int track, int step)
{
    handleFileDrop(filePath, track, step, true);
}

void PatternView::fileDroppedOnTrack(const juce::String& filePath, int track)
{
    handleFileDrop(filePath, track, 0, false);
}

void PatternView::handleFileDrop(const juce::String& filePath, int targetTrack, int step, bool hasSpecificStep)
{
    importAndAssignToTrack(filePath, targetTrack, step, hasSpecificStep);
}

void PatternView::importAndAssignToTrack(const juce::String& filePath, int targetTrack, int step, bool hasSpecificStep,
                                        std::optional<SampleKind> explicitKind)
{
    const std::filesystem::path p(filePath.toStdString());
    if (!importer_.isSupportedFormat(p))
        return;

    const juce::String lowerName = filePath.toLowerCase();
    SampleKind kind = explicitKind.has_value()
                          ? *explicitKind
                          : (lowerName.contains("loop") ? SampleKind::Loop : SampleKind::OneShot);
    int bars = 1;
    if (lowerName.contains("2bar") || lowerName.contains("2_bar") || lowerName.contains("2 bar"))
        bars = 2;
    else if (lowerName.contains("4bar") || lowerName.contains("4_bar") || lowerName.contains("4 bar"))
        bars = 4;

    auto res = importer_.importFile(p, engine_.sampleRate(), kind, bars);
    if (!res.has_value())
        return;

    if (onPushUndo_)
        onPushUndo_();

    const auto sampleId = song_.addSample(std::move(res->sample));
    stretchCache_.setRawAudio(sampleId, std::make_shared<const AudioData>(std::move(res->audio)));
    activeSample_ = sampleId;

    const auto* pat = song_.findPattern(currentPattern_);
    const int sampleSteps = bars * kStepsPerBar;
    const int targetStep = hasSpecificStep ? step : 0;
    const int neededEndStep = targetStep + sampleSteps;
    const int neededBars = std::max(1, static_cast<int>(std::ceil((neededEndStep - 1e-4) / static_cast<double>(kStepsPerBar))));
    if (pat != nullptr && neededBars > pat->lengthBars)
    {
        song_.setPatternLength(currentPattern_, neededBars);
        pat = song_.findPattern(currentPattern_);
    }

    if (pat != nullptr && targetTrack >= 0 && targetTrack < static_cast<int>(pat->tracks.size()))
    {
        song_.replaceTrackSample(currentPattern_, targetTrack, sampleId);
        if (hasSpecificStep)
            song_.placeClip(currentPattern_, targetTrack, sampleId, step);
    }
    else
    {
        const int newTrack = song_.addTrackToPattern(currentPattern_, sampleId);
        song_.placeClip(currentPattern_, newTrack, sampleId, targetStep);
    }

    autoAdjustPatternLength();
    updateSampleSelector();
    updateLengthSelector();
    trackHeaders_->updateFromModel();
    publishSnapshotQuick();
    republish();
    resized();
}

bool PatternView::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (const auto& f : files)
        if (f.endsWithIgnoreCase(".wav") || f.endsWithIgnoreCase(".mp3"))
            return true;
    return false;
}

void PatternView::filesDropped(const juce::StringArray& files, int /*x*/, int /*y*/)
{
    for (const auto& f : files)
        handleFileDrop(f, -1, 0, false);
}

bool PatternView::isInterestedInDragSource(const SourceDetails& dragSourceDetails)
{
    if (dragSourceDetails.description.isString())
    {
        const juce::String s = dragSourceDetails.description.toString();
        return s.endsWithIgnoreCase(".wav") || s.endsWithIgnoreCase(".mp3");
    }
    return false;
}

void PatternView::itemDropped(const SourceDetails& dragSourceDetails)
{
    if (dragSourceDetails.description.isString())
        handleFileDrop(dragSourceDetails.description.toString(), -1, 0, false);
}

void PatternView::setZoom(float x, float y)
{
    applyZoom(x, y, {0, 0});
}

void PatternView::zoomRequested(float deltaX, float deltaY, juce::Point<int> anchorInGrid)
{
    applyZoom(zoomX_ * deltaX, zoomY_ * deltaY, anchorInGrid);
}

void PatternView::zoomToLevelRequested(float targetZoomX, double anchorStep, int anchorViewportX)
{
    applyZoomAtStep(targetZoomX, anchorStep, anchorViewportX);
}

void PatternView::applyZoomAtStep(float targetZoomX, double anchorStep, int anchorViewportX)
{
    const float newZoomX = std::clamp(targetZoomX, StudioOneZoomCorner::kMinZoomX, StudioOneZoomCorner::kMaxZoomX);
    song_.setPatternZoom(newZoomX, zoomY_);

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
        const int newStepW = grid_->stepWidth();
        const int newVpX = juce::roundToInt(anchorStep * static_cast<double>(newStepW) - static_cast<double>(anchorViewportX));
        gridViewport_->setViewPosition(std::max(0, newVpX), gridViewport_->getViewPositionY());
    }
}

void PatternView::applyZoom(float targetZoomX, float targetZoomY, juce::Point<int> anchorInGrid)
{
    const float newZoomX = std::clamp(targetZoomX, StudioOneZoomCorner::kMinZoomX, StudioOneZoomCorner::kMaxZoomX);
    const float newZoomY = std::clamp(targetZoomY, 1.0f, 2.5f);

    song_.setPatternZoom(newZoomX, newZoomY);

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

void PatternView::zoomIn()
{
    const auto anchor = gridViewport_ != nullptr ? gridViewport_->getViewArea().getCentre() : juce::Point<int>(0, 0);
    applyZoom(zoomX_ * 1.25f, zoomY_, anchor);
}

void PatternView::zoomOut()
{
    const auto anchor = gridViewport_ != nullptr ? gridViewport_->getViewArea().getCentre() : juce::Point<int>(0, 0);
    applyZoom(zoomX_ / 1.25f, zoomY_, anchor);
}

void PatternView::resetZoom()
{
    const auto* pat = song_.findPattern(currentPattern_);
    const int totalSteps = pat != nullptr ? std::max(1, pat->lengthSteps()) : kStepsPerBar;
    const int numTracks = pat != nullptr ? std::max(1, static_cast<int>(pat->tracks.size())) : 1;

    const int viewW = gridViewport_ != nullptr
                          ? std::max(100, gridViewport_->getWidth() - StudioOneZoomCorner::kThickness - 4)
                          : getWidth();
    const int viewH = gridViewport_ != nullptr
                          ? std::max(100, gridViewport_->getHeight() - PatternGrid::kRulerHeight - StudioOneZoomCorner::kThickness - 4)
                          : getHeight();

    const float fitX = static_cast<float>(viewW) / static_cast<float>(totalSteps * PatternGrid::kDefaultStepWidth);
    const float fitY = static_cast<float>(viewH) / static_cast<float>(numTracks * PatternGrid::kDefaultTrackHeight);

    applyZoom(fitX, std::clamp(fitY, 1.0f, 2.0f), {0, 0});
    if (gridViewport_ != nullptr)
        gridViewport_->setViewPosition(0, 0);
}

void PatternView::resetPlayhead()
{
    if (grid_ != nullptr)
        grid_->setPlayheadStep(0.0);
    if (gridViewport_ != nullptr)
        gridViewport_->setViewPosition(0, gridViewport_->getViewPositionY());
}

void PatternView::trackHeightIn()
{
    const auto anchor = gridViewport_ != nullptr ? gridViewport_->getViewArea().getCentre() : juce::Point<int>(0, 0);
    applyZoom(zoomX_, zoomY_ * 1.2f, anchor);
}

void PatternView::trackHeightOut()
{
    const auto anchor = gridViewport_ != nullptr ? gridViewport_->getViewArea().getCentre() : juce::Point<int>(0, 0);
    applyZoom(zoomX_, zoomY_ / 1.2f, anchor);
}

void PatternView::timerCallback()
{
    if (grid_ != nullptr)
    {
        const Timing timing(song_.bpm(), engine_.sampleRate(), song_.swing());
        const double samplesPerStep = timing.samplesPerStep();
        if (samplesPerStep > 0.0)
        {
            const auto* pat = song_.findPattern(currentPattern_);
            const int totalSteps = pat != nullptr ? pat->lengthSteps() : kStepsPerBar;

            const std::int64_t pos = engine_.sequencer().position();
            const double currentStep = static_cast<double>(pos) / samplesPerStep;
            const double phStep = std::fmod(currentStep, static_cast<double>(totalSteps));
            grid_->setPlayheadStep(phStep);
            if (ruler_ != nullptr)
                ruler_->repaint();

            if (followPlayhead_ && engine_.sequencer().isPlaying())
                scrollToPlayheadIfNeeded(phStep);
        }
    }
}

bool PatternView::keyPressed(const juce::KeyPress& key)
{
    if (key.getKeyCode() == juce::KeyPress::spaceKey)
    {
        auto& seq = engine_.sequencer();
        seq.requestPlaying(!seq.isPlaying());
        return true;
    }

    if (key.getKeyCode() == '0' || key.getKeyCode() == juce::KeyPress::numberPad0)
    {
        if (engine_.recorder().state() != RecordState::Idle)
            engine_.recorder().stop();
        engine_.sequencer().requestPlaying(false);
        return true;
    }

    if (key.getKeyCode() == juce::KeyPress::returnKey)
    {
        engine_.sequencer().requestRewind();
        return true;
    }

    if (key.getKeyCode() == juce::KeyPress::deleteKey || key.getKeyCode() == juce::KeyPress::backspaceKey)
    {
        if (grid_ != nullptr && !grid_->selectedClips().empty())
        {
            clipsDeleteRequested(grid_->selectedClips());
            return true;
        }
    }

    const auto mods = key.getModifiers();
    const bool isCmdOrCtrl = mods.isCommandDown() || mods.isCtrlDown();

    if (!key.getModifiers().isCommandDown() && (key.isKeyCode('R') || key.getKeyCode() == 'r' || key.getKeyCode() == 'R'))
    {
        if (grid_ != nullptr)
        {
            const auto& selected = grid_->selectedClips();
            if (!selected.empty())
            {
                clipsReverseRequested(selected);
                return true;
            }
            if (grid_->selectedClip().isValid())
            {
                clipReverseRequested(grid_->selectedClip());
                return true;
            }
        }
    }

    if (isCmdOrCtrl)
    {
        if (key.isKeyCode('C'))
        {
            copySelectedClip();
            return true;
        }
        if (key.isKeyCode('V'))
        {
            pasteClip();
            return true;
        }
        if (key.isKeyCode('X'))
        {
            cutSelectedClip();
            return true;
        }
        if (key.isKeyCode('D'))
        {
            duplicateSelectedClip();
            return true;
        }
        if (key.getKeyCode() == '=' || key.getKeyCode() == '+')
        {
            if (mods.isShiftDown())
                trackHeightIn();
            else
                zoomIn();
            return true;
        }
        if (key.getKeyCode() == '-')
        {
            if (mods.isShiftDown())
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

    const bool isPlus = !isCmdOrCtrl && (key.getTextCharacter() == '+' ||
                                         key.getKeyCode() == juce::KeyPress::numberPadAdd ||
                                         key.isKeyCode('+') ||
                                         (key.getKeyCode() == '=' && !mods.isAltDown()));
    const bool isMinus = !isCmdOrCtrl && (key.getTextCharacter() == '-' ||
                                          key.getKeyCode() == juce::KeyPress::numberPadSubtract ||
                                          key.isKeyCode('-') ||
                                          key.getKeyCode() == '-');

    if (!isCmdOrCtrl && (key.getKeyCode() == 'F' || key.getKeyCode() == 'f'))
    {
        setFollowPlayhead(!followPlayhead_);
        if (onShowToast_)
            onShowToast_(followPlayhead_ ? "Playhead Follow: ON" : "Playhead Follow: OFF");
        return true;
    }

    if (grid_ != nullptr && (isPlus || isMinus))
    {
        const auto& selected = grid_->selectedClips();
        if (!selected.empty())
        {
            clipsTransposeDelta(selected, isPlus ? 1 : -1);
            return true;
        }
        if (grid_->selectedClip().isValid())
        {
            clipsTransposeDelta({grid_->selectedClip()}, isPlus ? 1 : -1);
            return true;
        }
    }

    if (grid_ != nullptr && grid_->keyPressed(key))
        return true;

    return false;
}

void PatternView::paint(juce::Graphics& g)
{
    g.fillAll(Palette::background);

    constexpr int kToolbarHeight = 44;
    g.setColour(Palette::patternHeaderBg);
    g.fillRect(0, 0, getWidth(), kToolbarHeight);
    g.setColour(Palette::patternAccent.withAlpha(0.7f));
    g.drawHorizontalLine(kToolbarHeight, 0.0f, static_cast<float>(getWidth()));
    g.fillRect(0, 0, 4, kToolbarHeight);
}

void PatternView::resized()
{
    constexpr int kToolbarHeight = 44;
    auto toolbar = juce::Rectangle<int>(0, 0, getWidth(), kToolbarHeight).reduced(8, 4);

    browserToggle_.setBounds(toolbar.removeFromLeft(64));
    toolbar.removeFromLeft(8);

    patternTitle_.setBounds(toolbar.removeFromLeft(80));
    toolbar.removeFromLeft(12);

    // Grid / Raster
    rasterLabel_.setBounds(toolbar.removeFromLeft(36));
    toolbar.removeFromLeft(2);
    rasterCombo_.setBounds(toolbar.removeFromLeft(65));
    toolbar.removeFromLeft(10);

    addTrackButton_.setBounds(toolbar.removeFromLeft(72));
    toolbar.removeFromLeft(8);
    followToggle_.setBounds(toolbar.removeFromLeft(64));

    // Content area below toolbar
    const int contentY = kToolbarHeight;
    const int totalH = getHeight() - contentY;

    int timelineH = totalH;
    if (showBrowser_ && browserPanel_ != nullptr)
    {
        const int browserH = std::clamp(totalH * 38 / 100, 180, 250);
        timelineH = totalH - browserH;
        browserPanel_->setVisible(true);
        browserPanel_->setBounds(0, contentY + timelineH, getWidth(), browserH);
    }
    else if (browserPanel_ != nullptr)
    {
        browserPanel_->setVisible(false);
    }

    const int headerW = TrackHeaderList::kHeaderWidth;
    trackHeadersViewport_->setBounds(0, contentY, headerW, timelineH);

    const int gridW = getWidth() - headerW;
    gridViewport_->setBounds(headerW, contentY, gridW, timelineH);

    if (headerCorner_ != nullptr)
        headerCorner_->setBounds(0, contentY, headerW, PatternGrid::kRulerHeight);

    if (ruler_ != nullptr)
        ruler_->setBounds(headerW, contentY, gridW, PatternGrid::kRulerHeight);

    const auto* pat = song_.findPattern(currentPattern_);
    const int numTracks = pat != nullptr ? static_cast<int>(pat->tracks.size()) : 0;
    const int totalSteps = pat != nullptr ? pat->lengthSteps() : kStepsPerBar;

    const int neededGridWidth = grid_->stepToX(totalSteps);
    const int neededGridHeight = PatternGrid::kRulerHeight + (numTracks + 1) * grid_->trackHeight();

    grid_->setSize(std::max(gridW, neededGridWidth), std::max(timelineH, neededGridHeight));
    trackHeaders_->setSize(headerW, std::max(timelineH, neededGridHeight));
}

void PatternView::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (trackHeadersViewport_ != nullptr && (e.eventComponent == trackHeadersViewport_.get() || trackHeadersViewport_->isParentOf(e.eventComponent)))
    {
        if (gridViewport_ != nullptr && wheel.deltaY != 0.0f)
        {
            const int currentY = gridViewport_->getViewPositionY();
            int deltaY = juce::roundToInt(-wheel.deltaY * 60.0f);
            if (deltaY == 0 && wheel.deltaY != 0.0f)
                deltaY = (wheel.deltaY > 0.0f ? -1 : 1);
            gridViewport_->setViewPosition(gridViewport_->getViewPositionX(), currentY + deltaY);
            return;
        }
    }
    juce::Component::mouseWheelMove(e, wheel);
}

} // namespace trami
