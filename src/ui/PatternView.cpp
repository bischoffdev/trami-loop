#include "ui/PatternView.h"

#include "model/Timing.h"
#include "ui/TramiLookAndFeel.h"

#include <cmath>

namespace trami
{

class PatternView::SyncViewport : public juce::Viewport
{
public:
    std::function<void(const juce::Rectangle<int>&)> onAreaChanged;
    void visibleAreaChanged(const juce::Rectangle<int>& newVisibleArea) override
    {
        if (onAreaChanged)
            onAreaChanged(newVisibleArea);
    }
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

    lengthCombo_.addItem("1 Bar", 1);
    lengthCombo_.addItem("2 Bars", 2);
    lengthCombo_.addItem("4 Bars", 4);
    lengthCombo_.addItem("8 Bars", 8);
    lengthCombo_.onChange = [this]() {
        const int bars = lengthCombo_.getSelectedId();
        if (bars > 0)
        {
            if (onPushUndo_)
                onPushUndo_();
            if (song_.setPatternLength(currentPattern_, bars))
            {
                republish();
                resized();
            }
        }
    };
    addAndMakeVisible(lengthCombo_);

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
    addTrackButton_.onClick = [this]() { addTrack(); };
    addAndMakeVisible(addTrackButton_);

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

    trackHeadersViewport_ = std::make_unique<juce::Viewport>();
    trackHeadersViewport_->setViewedComponent(trackHeaders_.get(), false);
    trackHeadersViewport_->setScrollBarsShown(false, false);
    addAndMakeVisible(trackHeadersViewport_.get());

    grid_ = std::make_unique<PatternGrid>(song_, stretchCache_, currentPattern_, *this);

    gridViewport_ = std::make_unique<SyncViewport>();
    gridViewport_->setViewedComponent(grid_.get(), false);
    gridViewport_->setScrollBarsShown(true, true);
    gridViewport_->onAreaChanged = [this](const juce::Rectangle<int>& area) {
        if (trackHeadersViewport_ != nullptr)
            trackHeadersViewport_->setViewPosition(0, area.getY());
    };
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
    const auto* pat = song_.findPattern(patternId);
    loopStartStep_ = 0;
    loopEndStep_ = pat != nullptr ? pat->lengthSteps() : 16;
    trackHeaders_->setPattern(patternId);
    grid_->setPattern(patternId);
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

void PatternView::publishSnapshotQuick()
{
    if (activeBank_.empty() && !song_.samples().empty())
        return;
    const auto rawBank = stretchCache_.getRawBank();
    engine_.publish(SnapshotBuilder::buildPattern(song_, currentPattern_, activeBank_, engine_.sampleRate(),
                                                 loopStartStep_, loopEndStep_, &rawBank));
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
                                                         loopStartStep_, loopEndStep_, &rawBank));
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
    republish();
    resized();
}

void PatternView::clipClicked(ClipId)
{
}

void PatternView::clipMoved(ClipId clipId, int newTrack, int newStep)
{
    const auto* pat = song_.findPattern(currentPattern_);
    if (!pat || newTrack < 0 || newTrack >= static_cast<int>(pat->tracks.size()))
        return;

    if (onPushUndo_)
        onPushUndo_();

    song_.moveClip(currentPattern_, clipId, newTrack, newStep);
    republish();
    grid_->repaint();
}

void PatternView::clipResized(ClipId clipId, int newStartStep, int newLengthSteps, int newOffsetSteps)
{
    if (onPushUndo_)
        onPushUndo_();
    song_.resizeClip(currentPattern_, clipId, newStartStep, newLengthSteps, newOffsetSteps);
    republish();
    grid_->repaint();
}

void PatternView::clipDeleteRequested(ClipId clipId)
{
    if (onPushUndo_)
        onPushUndo_();
    song_.removeClip(currentPattern_, clipId);
    republish();
    grid_->repaint();
}

void PatternView::gridCellClicked(int track, int step)
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

    song_.placeClip(currentPattern_, track, sid, step);
    republish();
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
    if (pat != nullptr && targetTrack >= 0 && targetTrack < static_cast<int>(pat->tracks.size()))
    {
        song_.replaceTrackSample(currentPattern_, targetTrack, sampleId);
        if (hasSpecificStep)
            song_.placeClip(currentPattern_, targetTrack, sampleId, step);
    }
    else
    {
        const int newTrack = song_.addTrackToPattern(currentPattern_, sampleId);
        song_.placeClip(currentPattern_, newTrack, sampleId, hasSpecificStep ? step : 0);
    }

    updateSampleSelector();
    updateLengthSelector();
    trackHeaders_->updateFromModel();
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

void PatternView::zoomRequested(float deltaX, float deltaY, juce::Point<int> anchorInGrid)
{
    applyZoom(zoomX_ * deltaX, zoomY_ * deltaY, anchorInGrid);
}

void PatternView::applyZoom(float targetZoomX, float targetZoomY, juce::Point<int> anchorInGrid)
{
    const float newZoomX = std::clamp(targetZoomX, 0.25f, 4.0f);
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
    const auto anchor = gridViewport_ != nullptr ? gridViewport_->getViewArea().getCentre() : juce::Point<int>(0, 0);
    applyZoom(1.0f, 1.0f, anchor);
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
    if (engine_.sequencer().isPlaying() && grid_ != nullptr)
    {
        const Timing timing(song_.bpm(), engine_.sampleRate(), song_.swing());
        const double samplesPerStep = timing.samplesPerStep();
        if (samplesPerStep > 0.0)
        {
            const auto* pat = song_.findPattern(currentPattern_);
            const int totalSteps = pat != nullptr ? pat->lengthSteps() : kStepsPerBar;

            const std::int64_t pos = engine_.sequencer().position();
            const double currentStep = static_cast<double>(pos) / samplesPerStep;
            grid_->setPlayheadStep(std::fmod(currentStep, static_cast<double>(totalSteps)));
        }
    }
    else if (grid_ != nullptr)
    {
        grid_->setPlayheadStep(-1.0);
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

void PatternView::paint(juce::Graphics& g)
{
    g.fillAll(Palette::background);

    constexpr int kToolbarHeight = 44;
    g.setColour(Palette::surface);
    g.fillRect(0, 0, getWidth(), kToolbarHeight);
    g.setColour(Palette::gridLine);
    g.drawHorizontalLine(kToolbarHeight, 0.0f, static_cast<float>(getWidth()));
}

void PatternView::resized()
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

    browserToggle_.setBounds(toolbar.removeFromLeft(54));
    toolbar.removeFromLeft(8);

    patternTitle_.setBounds(toolbar.removeFromLeft(80));
    toolbar.removeFromLeft(4);
    lengthCombo_.setBounds(toolbar.removeFromLeft(75));
    toolbar.removeFromLeft(12);

    // Grid / Raster
    rasterLabel_.setBounds(toolbar.removeFromLeft(36));
    toolbar.removeFromLeft(2);
    rasterCombo_.setBounds(toolbar.removeFromLeft(65));
    toolbar.removeFromLeft(10);

    addTrackButton_.setBounds(toolbar.removeFromLeft(70));

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

    const auto* pat = song_.findPattern(currentPattern_);
    const int numTracks = pat != nullptr ? static_cast<int>(pat->tracks.size()) : 0;
    const int totalSteps = pat != nullptr ? pat->lengthSteps() : kStepsPerBar;

    const int neededGridWidth = totalSteps * grid_->stepWidth();
    const int neededGridHeight = PatternGrid::kRulerHeight + (numTracks + 1) * grid_->trackHeight();

    grid_->setSize(std::max(gridW, neededGridWidth), std::max(timelineH, neededGridHeight));
    trackHeaders_->setSize(headerW, std::max(timelineH, neededGridHeight));
}

} // namespace trami
