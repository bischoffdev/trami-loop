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
        browserPanel_->setVisible(showBrowser_);
        resized();
    };
    addAndMakeVisible(browserToggle_);

    patternTitle_.setFont(Typography::bold(15.0f));
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

    recordButton_.setActiveColour(juce::Colour(0xffe53935));
    recordButton_.onClick = [this]() { toggleRecording(); };
    addAndMakeVisible(recordButton_);

    rewindButton_.onClick = [this]() { engine_.sequencer().requestRewind(); };
    addAndMakeVisible(rewindButton_);

    metroToggle_.setClickingTogglesState(true);
    metroToggle_.onClick = [this]() { engine_.sequencer().setMetronomeEnabled(metroToggle_.getToggleState()); };
    addAndMakeVisible(metroToggle_);

    swingToggle_.setClickingTogglesState(true);
    swingToggle_.onClick = [this]() {
        song_.setSwing(swingToggle_.getToggleState() ? 0.5 : 0.0);
        republish();
    };
    addAndMakeVisible(swingToggle_);

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

    // Browser Panel
    browserPanel_ = std::make_unique<SampleBrowserPanel>(song_, engine_, stretchCache_, [this](SampleId id) {
        activeSample_ = id;
        updateSampleSelector();
        republish();
    });
    addAndMakeVisible(browserPanel_.get());

    // Grid & Track headers
    trackHeaders_ = std::make_unique<TrackHeaderList>(song_, currentPattern_, [this](int track) {
        if (const Pattern* p = song_.findPattern(currentPattern_))
        {
            if (track >= 0 && track < kTracksPerPattern)
            {
                const auto& trk = p->tracks[static_cast<std::size_t>(track)];
                engine_.sequencer().setTrackMix(track, trk.mix.volume, trk.mix.pan, trk.muted, trk.solo);
            }
        }
        publishSnapshotQuick();
    });
    trackHeaders_->setOnTrackSelected([this](int track, SampleId sampleId) {
        if (sampleId.isValid())
        {
            activeSample_ = sampleId;
            updateSampleSelector();
        }
        else if (activeSample_.isValid())
        {
            song_.setPatternTrackSample(currentPattern_, track, activeSample_);
            trackHeaders_->updateFromModel();
            republish();
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
    trackHeaders_->setPattern(patternId);
    grid_->setPattern(patternId);
    updateLengthSelector();
    trackHeaders_->updateFromModel();
    syncAllTrackMixes();
    republish();
    resized();
}

void PatternView::syncAllTrackMixes()
{
    if (const Pattern* p = song_.findPattern(currentPattern_))
    {
        for (int t = 0; t < kTracksPerPattern; ++t)
        {
            const auto& trk = p->tracks[static_cast<std::size_t>(t)];
            engine_.sequencer().setTrackMix(t, trk.mix.volume, trk.mix.pan, trk.muted, trk.solo);
        }
    }
}

void PatternView::publishSnapshotQuick()
{
    if (activeBank_.empty() && !song_.samples().empty())
        return;
    engine_.publish(SnapshotBuilder::buildPattern(song_, currentPattern_, activeBank_, engine_.sampleRate()));
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
            syncAllTrackMixes();
            engine_.publish(SnapshotBuilder::buildPattern(song_, currentPattern_, activeBank_, engine_.sampleRate()));
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

void PatternView::onRecordFinished(AudioData recorded)
{
    recordButton_.setLabel("Rec");
    recordButton_.setToggleState(false, juce::dontSendNotification);

    Sample s;
    s.name = ("Take " + juce::String(song_.samples().size() + 1)).toStdString();
    s.durationSeconds = static_cast<double>(recorded.numFrames()) / engine_.sampleRate();
    s.kind = SampleKind::OneShot;

    const auto id = song_.addSample(s);
    stretchCache_.setRawAudio(id, std::make_shared<const AudioData>(std::move(recorded)));
    activeSample_ = id;

    song_.placeClip(currentPattern_, 0, id, 0);
    updateSampleSelector();
    republish();
}

void PatternView::clipClicked(ClipId)
{
}

void PatternView::clipMoved(ClipId clipId, int newTrack, int newStep)
{
    if (song_.moveClip(currentPattern_, clipId, newTrack, newStep))
    {
        if (trackHeaders_ != nullptr)
            trackHeaders_->updateFromModel();
        republish();
        grid_->repaint();
    }
}

void PatternView::clipResized(ClipId clipId, int newStartStep, int newLengthSteps, int newOffsetSteps)
{
    if (song_.resizeClip(currentPattern_, clipId, newStartStep, newLengthSteps, newOffsetSteps))
    {
        republish();
        grid_->repaint();
    }
}

void PatternView::clipDeleteRequested(ClipId clipId)
{
    if (song_.removeClip(currentPattern_, clipId))
    {
        if (trackHeaders_ != nullptr)
            trackHeaders_->updateFromModel();
        republish();
        grid_->repaint();
    }
}

void PatternView::gridCellClicked(int track, int step)
{
    const Pattern* p = song_.findPattern(currentPattern_);
    SampleId sampleToUse = activeSample_;

    if (p != nullptr && track >= 0 && track < kTracksPerPattern)
    {
        const auto& patTrack = p->tracks[static_cast<std::size_t>(track)];
        if (patTrack.sampleId.isValid() && song_.findSample(patTrack.sampleId) != nullptr)
            sampleToUse = patTrack.sampleId;
        else if (!patTrack.clips.empty() && song_.findSample(patTrack.clips.front().sampleId) != nullptr)
            sampleToUse = patTrack.clips.front().sampleId;
    }

    if (!sampleToUse.isValid())
    {
        if (!song_.samples().empty())
            sampleToUse = song_.samples().front().id;
        else
            return;
    }

    if (song_.placeClip(currentPattern_, track, sampleToUse, step))
    {
        activeSample_ = sampleToUse;
        updateSampleSelector();
        if (trackHeaders_ != nullptr)
            trackHeaders_->updateFromModel();
        republish();
        grid_->repaint();
    }
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
    const std::filesystem::path p(filePath.toStdString());
    if (!importer_.isSupportedFormat(p))
        return;

    const Pattern* pat = song_.findPattern(currentPattern_);
    if (pat == nullptr || targetTrack < 0 || targetTrack >= kTracksPerPattern)
        return;

    const auto& trackObj = pat->tracks[static_cast<std::size_t>(targetTrack)];
    const bool isOccupied = trackObj.sampleId.isValid() || !trackObj.clips.empty();

    if (!isOccupied)
    {
        importAndAssignToTrack(filePath, targetTrack, step, hasSpecificStep);
        return;
    }

    // Target track is occupied: find next empty track (if any)
    int nextEmptyTrack = -1;
    for (int i = 1; i < kTracksPerPattern; ++i)
    {
        const int candidate = (targetTrack + i) % kTracksPerPattern;
        const auto& candTrack = pat->tracks[static_cast<std::size_t>(candidate)];
        if (!candTrack.sampleId.isValid() && candTrack.clips.empty())
        {
            nextEmptyTrack = candidate;
            break;
        }
    }

    auto* alert = new juce::AlertWindow(
        "Track Occupied",
        "Track " + juce::String(targetTrack + 1) + " is already occupied.\nWhat would you like to do with " +
            juce::File(filePath).getFileName() + "?",
        juce::AlertWindow::QuestionIcon);

    if (nextEmptyTrack >= 0)
        alert->addButton("Add to Track " + juce::String(nextEmptyTrack + 1) + " (Next Empty)", 1);
    else
        alert->addButton("No Empty Tracks Available", 0);

    alert->addButton("Replace Sound on Track " + juce::String(targetTrack + 1), 2);
    alert->addButton("Cancel", 3);

    alert->enterModalState(
        true,
        juce::ModalCallbackFunction::create([this, alert, filePath, targetTrack, nextEmptyTrack, step,
                                             hasSpecificStep](int result) {
            std::unique_ptr<juce::AlertWindow> keeper(alert);
            if (result == 1 && nextEmptyTrack >= 0)
            {
                importAndAssignToTrack(filePath, nextEmptyTrack, hasSpecificStep ? step : 0, hasSpecificStep);
            }
            else if (result == 2)
            {
                replaceSoundOnTrack(filePath, targetTrack, step, hasSpecificStep);
            }
        }),
        false);
}

void PatternView::importAndAssignToTrack(const juce::String& filePath, int targetTrack, int step, bool hasSpecificStep)
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

    song_.setPatternTrackSample(currentPattern_, targetTrack, sampleId);
    song_.placeClip(currentPattern_, targetTrack, sampleId, hasSpecificStep ? step : 0);
    updateSampleSelector();
    if (trackHeaders_ != nullptr)
        trackHeaders_->updateFromModel();
    republish();
    grid_->repaint();
}

void PatternView::replaceSoundOnTrack(const juce::String& filePath, int targetTrack, int step, bool hasSpecificStep)
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

    song_.replaceTrackSample(currentPattern_, targetTrack, sampleId);

    const Pattern* pat = song_.findPattern(currentPattern_);
    if (pat != nullptr && pat->tracks[static_cast<std::size_t>(targetTrack)].clips.empty())
    {
        song_.placeClip(currentPattern_, targetTrack, sampleId, hasSpecificStep ? step : 0);
    }

    updateSampleSelector();
    if (trackHeaders_ != nullptr)
        trackHeaders_->updateFromModel();
    republish();
    grid_->repaint();
}

void PatternView::zoomRequested(float deltaX, float deltaY, juce::Point<int> anchorInGrid)
{
    applyZoom(zoomX_ * deltaX, zoomY_ * deltaY, anchorInGrid);
}

void PatternView::applyZoom(float targetZoomX, float targetZoomY, juce::Point<int> anchorInGrid)
{
    const float newZoomX = std::clamp(targetZoomX, 0.25f, 4.0f);
    const float newZoomY = std::clamp(targetZoomY, 0.65f, 2.5f);

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

void PatternView::zoomIn()
{
    const int centerX = gridViewport_->getViewPositionX() + gridViewport_->getViewWidth() / 2;
    const int centerY = gridViewport_->getViewPositionY() + gridViewport_->getViewHeight() / 2;
    applyZoom(zoomX_ * 1.25f, zoomY_, {centerX, centerY});
}

void PatternView::zoomOut()
{
    const int centerX = gridViewport_->getViewPositionX() + gridViewport_->getViewWidth() / 2;
    const int centerY = gridViewport_->getViewPositionY() + gridViewport_->getViewHeight() / 2;
    applyZoom(zoomX_ / 1.25f, zoomY_, {centerX, centerY});
}

void PatternView::resetZoom()
{
    const int centerX = gridViewport_->getViewPositionX() + gridViewport_->getViewWidth() / 2;
    const int centerY = gridViewport_->getViewPositionY() + gridViewport_->getViewHeight() / 2;
    applyZoom(1.0f, 1.0f, {centerX, centerY});
}

void PatternView::trackHeightIn()
{
    const int centerX = gridViewport_->getViewPositionX() + gridViewport_->getViewWidth() / 2;
    const int centerY = gridViewport_->getViewPositionY() + gridViewport_->getViewHeight() / 2;
    applyZoom(zoomX_, zoomY_ * 1.2f, {centerX, centerY});
}

void PatternView::trackHeightOut()
{
    const int centerX = gridViewport_->getViewPositionX() + gridViewport_->getViewWidth() / 2;
    const int centerY = gridViewport_->getViewPositionY() + gridViewport_->getViewHeight() / 2;
    applyZoom(zoomX_, zoomY_ / 1.2f, {centerX, centerY});
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

void PatternView::timerCallback()
{
    auto& seq = engine_.sequencer();
    const bool isPlaying = seq.isPlaying();
    playButton_.setIcon(isPlaying ? IconButton::Icon::Stop : IconButton::Icon::Play);
    playButton_.setLabel(isPlaying ? "Stop" : "Play");
    playButton_.setToggleState(isPlaying, juce::dontSendNotification);

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

    constexpr int kToolbarHeight = 52;
    g.setColour(Palette::surface);
    g.fillRect(0, 0, getWidth(), kToolbarHeight);
    g.setColour(Palette::gridLine);
    g.drawHorizontalLine(kToolbarHeight, 0.0f, static_cast<float>(getWidth()));
}

void PatternView::resized()
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

    browserToggle_.setBounds(toolbar.removeFromLeft(54));
    toolbar.removeFromLeft(6);

    patternTitle_.setBounds(toolbar.removeFromLeft(85));
    toolbar.removeFromLeft(6);
    lengthCombo_.setBounds(toolbar.removeFromLeft(85));
    toolbar.removeFromLeft(10);

    sampleCombo_.setBounds(toolbar.removeFromLeft(150));
    toolbar.removeFromLeft(12);

    rewindButton_.setBounds(toolbar.removeFromLeft(48));
    toolbar.removeFromLeft(3);
    playButton_.setBounds(toolbar.removeFromLeft(48));
    toolbar.removeFromLeft(3);
    recordButton_.setBounds(toolbar.removeFromLeft(48));
    toolbar.removeFromLeft(10);

    metroToggle_.setBounds(toolbar.removeFromLeft(48));
    toolbar.removeFromLeft(3);
    swingToggle_.setBounds(toolbar.removeFromLeft(48));
    toolbar.removeFromLeft(12);

    tempoDown_.setBounds(toolbar.removeFromLeft(28));
    tempoLabel_.setBounds(toolbar.removeFromLeft(68));
    tempoUp_.setBounds(toolbar.removeFromLeft(28));

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

    const Pattern* p = song_.findPattern(currentPattern_);
    const int steps = p != nullptr ? p->lengthSteps() : 16;
    const int neededGridWidth = steps * grid_->stepWidth();
    const int neededGridHeight = PatternGrid::kRulerHeight + kTracksPerPattern * grid_->trackHeight();

    grid_->setSize(std::max(gridW, neededGridWidth), std::max(timelineH, neededGridHeight));
    trackHeaders_->setSize(headerW, std::max(timelineH, neededGridHeight));
}

} // namespace trami
