#include "ui/WaveEditorPanel.h"

#include "ui/TramiLookAndFeel.h"

#include <algorithm>
#include <cmath>

namespace trami
{

class WaveEditorPanel::IconButton : public juce::Button
{
public:
    enum class Kind
    {
        PlayStop,
        Loop
    };

    IconButton(const juce::String& name, Kind kind) : juce::Button(name), kind_(kind)
    {
        setWantsKeyboardFocus(false);
    }

    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced(0.5f);
        const bool on = getToggleState();
        const bool enabled = isEnabled();

        juce::Colour bg = Palette::surface.brighter(0.08f);
        if (kind_ == Kind::PlayStop)
            bg = on ? juce::Colour(0xffb91c1c).withAlpha(0.78f) : juce::Colour(0xff15803d).withAlpha(0.55f);
        else if (on)
            bg = Palette::accent.withAlpha(0.85f);

        if (shouldDrawButtonAsDown)
            bg = bg.darker(0.18f);
        else if (shouldDrawButtonAsHighlighted)
            bg = bg.brighter(0.14f);

        if (!enabled)
            bg = bg.withMultipliedAlpha(0.45f);

        g.setColour(bg);
        g.fillRoundedRectangle(bounds, 4.0f);
        g.setColour((on && kind_ == Kind::Loop ? Palette::accent.brighter(0.25f) : Palette::gridLine)
                        .withMultipliedAlpha(enabled ? 1.0f : 0.45f));
        g.drawRoundedRectangle(bounds, 4.0f, 1.0f);

        const float cx = bounds.getCentreX();
        const float cy = bounds.getCentreY();
        const juce::Colour iconCol =
            (kind_ == Kind::PlayStop)
                ? (on ? juce::Colours::white : juce::Colour(0xffdcfce7))
                : (on ? juce::Colour(0xff0b1120) : Palette::text.withAlpha(0.85f));
        g.setColour(iconCol.withMultipliedAlpha(enabled ? 1.0f : 0.4f));

        if (kind_ == Kind::PlayStop)
        {
            if (on)
            {
                // Stop square ■
                constexpr float s = 8.5f;
                g.fillRoundedRectangle(cx - s * 0.5f, cy - s * 0.5f, s, s, 1.5f);
            }
            else
            {
                // Play triangle ▶
                juce::Path p;
                p.startNewSubPath(cx - 4.0f, cy - 5.5f);
                p.lineTo(cx + 5.5f, cy);
                p.lineTo(cx - 4.0f, cy + 5.5f);
                p.closeSubPath();
                g.fillPath(p);
            }
        }
        else
        {
            // Loop cycle symbol (rounded rectangle track with directional arrowheads)
            const float w = 11.5f;
            const float h = 7.5f;
            const float x0 = cx - w * 0.5f;
            const float y0 = cy - h * 0.5f;
            juce::Path topArc;
            topArc.startNewSubPath(x0, y0 + h * 0.45f);
            topArc.lineTo(x0, y0 + 2.0f);
            topArc.quadraticTo(x0, y0, x0 + 2.2f, y0);
            topArc.lineTo(x0 + w - 2.5f, y0);
            g.strokePath(topArc, juce::PathStrokeType(1.7f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // Top-right arrowhead
            juce::Path arrow1;
            arrow1.startNewSubPath(x0 + w - 4.2f, y0 - 2.6f);
            arrow1.lineTo(x0 + w - 0.5f, y0);
            arrow1.lineTo(x0 + w - 4.2f, y0 + 2.6f);
            arrow1.closeSubPath();
            g.fillPath(arrow1);

            juce::Path botArc;
            botArc.startNewSubPath(x0 + w, y0 + h * 0.55f);
            botArc.lineTo(x0 + w, y0 + h - 2.0f);
            botArc.quadraticTo(x0 + w, y0 + h, x0 + w - 2.2f, y0 + h);
            botArc.lineTo(x0 + 2.5f, y0 + h);
            g.strokePath(botArc, juce::PathStrokeType(1.7f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // Bottom-left arrowhead
            juce::Path arrow2;
            arrow2.startNewSubPath(x0 + 4.2f, y0 + h - 2.6f);
            arrow2.lineTo(x0 + 0.5f, y0 + h);
            arrow2.lineTo(x0 + 4.2f, y0 + h + 2.6f);
            arrow2.closeSubPath();
            g.fillPath(arrow2);
        }
    }

private:
    Kind kind_;
};

WaveEditorPanel::WaveEditorPanel(Song& song, AudioEngine& engine, StretchCache& stretchCache, PatternId patternId,
                                 std::function<void()> onChanged)
    : song_(song), engine_(engine), stretchCache_(stretchCache), patternId_(patternId), onChanged_(std::move(onChanged))
{
    setWantsKeyboardFocus(true);

    playBtn_ = std::make_unique<IconButton>("WavePlay", IconButton::Kind::PlayStop);
    playBtn_->setTooltip("Play / Stop waveform from the wave playhead (Space when Wave Editor is focused)");
    playBtn_->onClick = [this]()
    {
        grabKeyboardFocus();
        togglePreview();
    };
    addAndMakeVisible(*playBtn_);

    loopBtn_ = std::make_unique<IconButton>("WaveLoop", IconButton::Kind::Loop);
    loopBtn_->setClickingTogglesState(true);
    loopBtn_->setTooltip("Loop waveform playback (loops marked selection if active, otherwise full wave)");
    loopBtn_->onClick = [this]()
    {
        grabKeyboardFocus();
        loopEnabled_ = loopBtn_->getToggleState();
        if (isPreviewing_)
            startPreviewFrom(previewPlayheadNorm_);
        repaint();
    };
    addAndMakeVisible(*loopBtn_);

    trackScopeBtn_.setTooltip("Edit the Track's underlying sample audio (applies to all clips using this sample)");
    trackScopeBtn_.onClick = [this]() { setScope(Scope::TrackSample); };
    addAndMakeVisible(trackScopeBtn_);

    clipScopeBtn_.setTooltip("Edit only the Selected Clip non-destructively (clip trim, clip gain, reverse, or make unique)");
    clipScopeBtn_.onClick = [this]() { setScope(Scope::SelectedClip); };
    addAndMakeVisible(clipScopeBtn_);

    trackSelector_.setTextWhenNoChoicesAvailable("No Tracks");
    trackSelector_.onChange = [this]()
    {
        if (isUpdating_)
            return;
        const int idx = trackSelector_.getSelectedId() - 1;
        if (idx >= 0 && idx != trackIndex_)
            setTrack(patternId_, idx);
    };
    addAndMakeVisible(trackSelector_);

    // Track Sample edit buttons
    trimBtn_.setTooltip("Trim sample audio to keep only the selected waveform region (drag on waveform to select)");
    trimBtn_.onClick = [this]() { applySampleTrim(); };
    addAndMakeVisible(trimBtn_);

    cutBtn_.setColour(juce::TextButton::textColourOffId, juce::Colour(0xfffca5a5));
    cutBtn_.setTooltip("Delete / remove the marked waveform region (Delete / Backspace)");
    cutBtn_.onClick = [this]() { applySampleDelete(); };
    addAndMakeVisible(cutBtn_);

    silenceBtn_.setTooltip("Silence the selected waveform region");
    silenceBtn_.onClick = [this]() { applySampleSilence(); };
    addAndMakeVisible(silenceBtn_);

    normalizeBtn_.setTooltip("Normalize selected region (or entire waveform if no selection) to -0.3 dBFS");
    normalizeBtn_.onClick = [this]() { applySampleNormalize(); };
    addAndMakeVisible(normalizeBtn_);

    fadeInBtn_.setTooltip("Apply linear Fade In to selection (or first 10% of waveform)");
    fadeInBtn_.onClick = [this]() { applySampleFade(true); };
    addAndMakeVisible(fadeInBtn_);

    fadeOutBtn_.setTooltip("Apply linear Fade Out to selection (or last 10% of waveform)");
    fadeOutBtn_.onClick = [this]() { applySampleFade(false); };
    addAndMakeVisible(fadeOutBtn_);

    gainLabel_.setFont(Typography::bold(11.0f));
    gainLabel_.setColour(juce::Label::textColourId, Palette::textDim);
    gainLabel_.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(gainLabel_);

    gainSlider_.setRange(-24.0, 24.0, 0.5);
    gainSlider_.setValue(0.0, juce::dontSendNotification);
    gainSlider_.setDoubleClickReturnValue(true, 0.0);
    gainSlider_.setTextValueSuffix(" dB");
    gainSlider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 52, 20);
    gainSlider_.setTooltip("Gain adjustment in dB for selection (or full sample). Click Apply to commit.");
    addAndMakeVisible(gainSlider_);

    applyGainBtn_.setTooltip("Apply gain adjustment to the selected region (or entire sample if nothing selected)");
    applyGainBtn_.onClick = [this]() { applySampleGain(); };
    addAndMakeVisible(applyGainBtn_);

    // Selected Clip edit controls
    clipTrimBtn_.setTooltip("Non-destructively trim only the selected clip to the highlighted waveform range");
    clipTrimBtn_.onClick = [this]() { applyClipTrim(); };
    addAndMakeVisible(clipTrimBtn_);

    clipReverseBtn_.setClickingTogglesState(true);
    clipReverseBtn_.setTooltip("Reverse playback direction of the selected clip");
    clipReverseBtn_.onClick = [this]()
    {
        if (isUpdating_ || !selectedClipId_.isValid())
            return;
        if (onPushUndo)
            onPushUndo();
        song_.setClipReversed(patternId_, selectedClipId_, clipReverseBtn_.getToggleState());
        if (onChanged_)
            onChanged_();
        repaint();
    };
    addAndMakeVisible(clipReverseBtn_);

    makeUniqueBtn_.setTooltip("Create an independent audio copy for this clip so sample edits only affect this clip");
    makeUniqueBtn_.onClick = [this]() { makeSelectedClipUnique(); };
    addAndMakeVisible(makeUniqueBtn_);

    clipGainLabel_.setFont(Typography::bold(11.0f));
    clipGainLabel_.setColour(juce::Label::textColourId, Palette::textDim);
    clipGainLabel_.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(clipGainLabel_);

    clipGainSlider_.setRange(-24.0, 6.0, 0.25);
    clipGainSlider_.setValue(0.0, juce::dontSendNotification);
    clipGainSlider_.setDoubleClickReturnValue(true, 0.0);
    clipGainSlider_.setTextValueSuffix(" dB");
    clipGainSlider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 54, 20);
    clipGainSlider_.setTooltip("Adjust volume gain of only the selected clip (double-click to reset to 0 dB)");
    clipGainSlider_.onDragStart = [this]()
    {
        if (onPushUndo)
            onPushUndo();
    };
    clipGainSlider_.onValueChange = [this]()
    {
        if (isUpdating_ || !selectedClipId_.isValid())
            return;
        if (const Clip* c = findSelectedClip())
        {
            const double db = clipGainSlider_.getValue();
            const float linear = (db <= -23.9) ? 0.0f : std::clamp(static_cast<float>(std::pow(10.0, db / 20.0)), 0.0f, 2.0f);
            song_.setClipEnvelope(patternId_, selectedClipId_, linear, c->fadeInSteps, c->fadeOutSteps);
            if (onChanged_)
                onChanged_();
            repaint();
        }
    };
    addAndMakeVisible(clipGainSlider_);

    // Stretch marker controls (grouped clearly in bottom bar)
    stretchGroupLabel_.setFont(Typography::bold(11.0f));
    stretchGroupLabel_.setColour(juce::Label::textColourId, Palette::textDim);
    stretchGroupLabel_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(stretchGroupLabel_);

    autoDetectBtn_.setTooltip("Automatically detect transient onsets in the audio waveform");
    autoDetectBtn_.onClick = [this]()
    {
        if (Sample* s = song_.findSample(sampleId_))
        {
            if (auto raw = stretchCache_.getRawAudio(sampleId_))
            {
                if (onPushUndo)
                    onPushUndo();
                s->stretchMarkers = Sample::detectTransients(raw->left(), raw->right(), raw->numFrames(), 44100.0, 32);
                if (onChanged_)
                    onChanged_();
                repaint();
            }
        }
    };
    addAndMakeVisible(autoDetectBtn_);

    beatGridBtn_.setTooltip("Place stretch markers evenly at each 1/8th beat subdivision");
    beatGridBtn_.onClick = [this]()
    {
        if (Sample* s = song_.findSample(sampleId_))
        {
            if (onPushUndo)
                onPushUndo();
            s->stretchMarkers.clear();
            const int divs = std::clamp(static_cast<int>(std::round(s->loopCycleBeats() * 2.0)), 2, 64);
            for (int i = 1; i < divs; ++i)
                s->stretchMarkers.push_back(static_cast<double>(i) / static_cast<double>(divs));
            if (onChanged_)
                onChanged_();
            repaint();
        }
    };
    addAndMakeVisible(beatGridBtn_);

    clearMarkersBtn_.setTooltip("Remove all stretch markers");
    clearMarkersBtn_.onClick = [this]()
    {
        if (Sample* s = song_.findSample(sampleId_))
        {
            if (!s->stretchMarkers.empty())
            {
                if (onPushUndo)
                    onPushUndo();
                s->stretchMarkers.clear();
                if (onChanged_)
                    onChanged_();
                repaint();
            }
        }
    };
    addAndMakeVisible(clearMarkersBtn_);

    toNewTrackBtn_.setColour(juce::TextButton::buttonColourId, Palette::accent.withAlpha(0.32f));
    toNewTrackBtn_.setTooltip("Create a new track containing only the marked waveform region (e.g. a single drum hit)");
    toNewTrackBtn_.onClick = [this]() { extractSelectionToNewTrack(); };
    addAndMakeVisible(toNewTrackBtn_);

    zoomOutBtn_.setTooltip("Zoom Out Waveform");
    zoomOutBtn_.onClick = [this]() { setZoom(zoom_ / 1.5); };
    addAndMakeVisible(zoomOutBtn_);

    zoomLabel_.setFont(Typography::bold(11.0f));
    zoomLabel_.setColour(juce::Label::textColourId, Palette::textDim);
    zoomLabel_.setJustificationType(juce::Justification::centred);
    zoomLabel_.setText("1.0x", juce::dontSendNotification);
    addAndMakeVisible(zoomLabel_);

    zoomInBtn_.setTooltip("Zoom In Waveform (or scroll/pinch on waveform)");
    zoomInBtn_.onClick = [this]() { setZoom(zoom_ * 1.5); };
    addAndMakeVisible(zoomInBtn_);

    zoomResetBtn_.setTooltip("Reset Waveform Zoom to 1:1");
    zoomResetBtn_.onClick = [this]() { setZoom(1.0); };
    addAndMakeVisible(zoomResetBtn_);

    setScope(Scope::TrackSample);
    for (int i = 0; i < getNumChildComponents(); ++i)
    {
        if (auto* btn = dynamic_cast<juce::Button*>(getChildComponent(i)))
            btn->setWantsKeyboardFocus(false);
    }
    refreshFromModel();
}

WaveEditorPanel::~WaveEditorPanel()
{
    stopPreview();
}

void WaveEditorPanel::stopPreview()
{
    stopTimer();
    if (isPreviewing_)
    {
        engine_.sequencer().stopPreview();
        isPreviewing_ = false;
        if (playBtn_ != nullptr)
            playBtn_->setToggleState(false, juce::dontSendNotification);
        repaint();
    }
}

void WaveEditorPanel::togglePreview()
{
    if (isPreviewing_)
    {
        stopPreview();
        return;
    }

    startPreviewFrom(playheadNorm_);
}

void WaveEditorPanel::startPreviewFrom(double startPosNorm)
{
    auto raw = stretchCache_.getRawAudio(sampleId_);
    if (raw == nullptr || raw->numFrames() <= 0)
        return;

    const std::int64_t total = raw->numFrames();
    double startNorm = 0.0;
    double endNorm = 1.0;
    float gain = 1.0f;
    bool reversed = false;

    const double clampedStart = std::clamp(startPosNorm, 0.0, 1.0);
    const bool playheadInsideSelection = hasSelection() &&
                                         clampedStart >= selStartNorm_ - 0.002 &&
                                         clampedStart <= selEndNorm_ + 0.002;

    if (hasSelection() && (loopEnabled_ || playheadInsideSelection))
    {
        startNorm = selStartNorm_;
        endNorm = selEndNorm_;
    }
    else if (scope_ == Scope::SelectedClip)
    {
        if (const Clip* c = findSelectedClip())
        {
            gain = c->gain;
            reversed = c->reversed;
            if (const Sample* s = song_.findSample(sampleId_))
            {
                double fullSteps = static_cast<double>(song_.stepsPerBar());
                const SampleKind kind = song_.patternTrackKind(patternId_, trackIndex_);
                if (kind == SampleKind::Loop)
                    fullSteps = std::max(1.0, s->loopCycleBeats() * static_cast<double>(song_.stepsPerBeat()));
                else if (s->durationSeconds > 0.0)
                {
                    const double stepSec = (60.0 / std::max(20.0, song_.bpm())) / static_cast<double>(song_.stepsPerBeat());
                    if (stepSec > 0.0)
                        fullSteps = std::max(0.25, s->durationSeconds / stepSec);
                }
                const double span = (c->lengthSteps > 0.0) ? c->lengthSteps : fullSteps;
                startNorm = std::clamp(c->offsetSteps / fullSteps, 0.0, 1.0);
                endNorm = std::clamp((c->offsetSteps + span) / fullSteps, startNorm, 1.0);
                if (endNorm <= startNorm + 0.001)
                {
                    startNorm = 0.0;
                    endNorm = 1.0;
                }
            }
        }
    }

    const std::int64_t f0 = std::clamp<std::int64_t>(static_cast<std::int64_t>(std::llround(startNorm * static_cast<double>(total))), 0, total - 1);
    const std::int64_t f1 = std::clamp<std::int64_t>(static_cast<std::int64_t>(std::llround(endNorm * static_cast<double>(total))), f0 + 1, total);

    std::shared_ptr<const AudioData> audioToPlay;
    if (f0 == 0 && f1 == total && std::abs(gain - 1.0f) < 0.001f && !reversed)
    {
        audioToPlay = raw;
    }
    else
    {
        AudioData sliced;
        sliced.channels.resize(raw->channels.size());
        for (std::size_t ch = 0; ch < raw->channels.size(); ++ch)
        {
            sliced.channels[ch].assign(raw->channels[ch].begin() + f0, raw->channels[ch].begin() + f1);
            if (reversed)
                std::reverse(sliced.channels[ch].begin(), sliced.channels[ch].end());
            if (std::abs(gain - 1.0f) >= 0.001f)
            {
                for (float& v : sliced.channels[ch])
                    v = std::clamp(v * gain, -1.0f, 1.0f);
            }
        }
        audioToPlay = std::make_shared<const AudioData>(std::move(sliced));
    }

    previewStartNorm_ = reversed ? endNorm : startNorm;
    previewEndNorm_ = reversed ? startNorm : endNorm;

    // Start from the parked playhead if it lies inside the active playback range
    std::int64_t initialFrame = 0;
    const double minNorm = std::min(previewStartNorm_, previewEndNorm_);
    const double maxNorm = std::max(previewStartNorm_, previewEndNorm_);
    const double spanNorm = std::max(1e-6, maxNorm - minNorm);
    if (clampedStart > minNorm + 0.0005 && clampedStart < maxNorm - 0.0005)
    {
        const double frac = reversed ? ((maxNorm - clampedStart) / spanNorm)
                                     : ((clampedStart - minNorm) / spanNorm);
        initialFrame = std::clamp<std::int64_t>(
            static_cast<std::int64_t>(std::llround(frac * static_cast<double>(audioToPlay->numFrames()))),
            0, std::max<std::int64_t>(0, audioToPlay->numFrames() - 1));
        previewPlayheadNorm_ = clampedStart;
    }
    else
    {
        previewPlayheadNorm_ = previewStartNorm_;
    }

    isPreviewing_ = true;
    if (playBtn_ != nullptr)
        playBtn_->setToggleState(true, juce::dontSendNotification);
    engine_.sequencer().triggerPreview(std::move(audioToPlay), 0, loopEnabled_, initialFrame);
    startTimerHz(30);
    repaint();
}

void WaveEditorPanel::timerCallback()
{
    if (!isPreviewing_)
    {
        stopTimer();
        return;
    }

    if (!engine_.sequencer().isPreviewActive())
    {
        isPreviewing_ = false;
        stopTimer();
        if (playBtn_ != nullptr)
            playBtn_->setToggleState(false, juce::dontSendNotification);
        repaint();
        return;
    }

    const double prog = std::clamp(engine_.sequencer().previewProgress(), 0.0, 1.0);
    previewPlayheadNorm_ = previewStartNorm_ + prog * (previewEndNorm_ - previewStartNorm_);
    repaint(waveformRect_);
}

void WaveEditorPanel::focusGained(FocusChangeType)
{
    repaint();
}

void WaveEditorPanel::focusLost(FocusChangeType)
{
    repaint();
}

bool WaveEditorPanel::keyPressed(const juce::KeyPress& key)
{
    if (scope_ == Scope::TrackSample && hasSelection() &&
        (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey))
    {
        applySampleDelete();
        return true;
    }
    if (key == juce::KeyPress::spaceKey)
    {
        togglePreview();
        return true;
    }
    return false;
}

void WaveEditorPanel::setPattern(PatternId patternId)
{
    stopPreview();
    patternId_ = patternId;
    selectedClipId_ = {};
    trackIndex_ = 0;
    playheadNorm_ = 0.0;
    selStartNorm_ = 0.0;
    selEndNorm_ = 0.0;
    setZoom(1.0);
    refreshFromModel();
}

void WaveEditorPanel::setTrack(PatternId patternId, int trackIndex)
{
    const bool changed = (patternId_ != patternId || trackIndex_ != trackIndex);
    patternId_ = patternId;
    trackIndex_ = trackIndex;
    if (changed)
    {
        stopPreview();
        playheadNorm_ = 0.0;
        selStartNorm_ = 0.0;
        selEndNorm_ = 0.0;
        hoveredSelEdge_ = -1;
        hoveredInsideSel_ = false;
        zoom_ = 1.0;
        scrollNorm_ = 0.0;
        zoomLabel_.setText("1.0x", juce::dontSendNotification);
    }
    refreshFromModel();
}

void WaveEditorPanel::setSelectedClip(ClipId clipId, int trackIndex)
{
    const bool clipChanged = (selectedClipId_ != clipId);
    const bool trackChanged = (trackIndex >= 0 && trackIndex_ != trackIndex);
    selectedClipId_ = clipId;
    if (trackIndex >= 0)
        trackIndex_ = trackIndex;
    if (clipChanged || trackChanged)
    {
        selStartNorm_ = 0.0;
        selEndNorm_ = 0.0;
        hoveredSelEdge_ = -1;
        hoveredInsideSel_ = false;
    }
    if (trackChanged)
    {
        zoom_ = 1.0;
        scrollNorm_ = 0.0;
        zoomLabel_.setText("1.0x", juce::dontSendNotification);
    }
    refreshFromModel();
}

bool WaveEditorPanel::hasValidTrack() const
{
    const auto* pat = song_.findPattern(patternId_);
    return pat != nullptr && trackIndex_ >= 0 && trackIndex_ < static_cast<int>(pat->tracks.size());
}

const Clip* WaveEditorPanel::findSelectedClip() const
{
    if (!selectedClipId_.isValid())
        return nullptr;
    return song_.findClip(patternId_, selectedClipId_);
}

juce::String WaveEditorPanel::currentSummaryText() const
{
    if (!hasValidTrack())
        return "No track selected";
    const std::string name = song_.patternTrackName(patternId_, trackIndex_);
    juce::String base = "Track " + juce::String(trackIndex_ + 1) + (name.empty() ? "" : (" - " + juce::String(name)));
    if (scope_ == Scope::SelectedClip)
        return base + (findSelectedClip() != nullptr ? " (Clip Scope)" : " (No Clip Selected)");
    return base + " (Track Sample)";
}

void WaveEditorPanel::setScope(Scope scope)
{
    scope_ = scope;
    const bool isTrack = (scope_ == Scope::TrackSample);
    trackScopeBtn_.setColour(juce::TextButton::buttonColourId, isTrack ? Palette::accent.withAlpha(0.32f) : Palette::surface);
    clipScopeBtn_.setColour(juce::TextButton::buttonColourId, !isTrack ? juce::Colour(0xff22d3ee).withAlpha(0.32f) : Palette::surface);

    for (juce::Component* c : std::initializer_list<juce::Component*>{
             &trimBtn_, &cutBtn_, &silenceBtn_, &normalizeBtn_, &fadeInBtn_, &fadeOutBtn_,
             &gainLabel_, &gainSlider_, &applyGainBtn_})
        c->setVisible(isTrack);

    for (juce::Component* c : std::initializer_list<juce::Component*>{
             &clipTrimBtn_, &clipReverseBtn_, &makeUniqueBtn_, &clipGainLabel_, &clipGainSlider_})
        c->setVisible(!isTrack);

    syncControlsFromModel();
    resized();
    repaint();
}

void WaveEditorPanel::refreshFromModel()
{
    const auto* pat = song_.findPattern(patternId_);
    if (pat == nullptr || pat->tracks.empty())
    {
        trackIndex_ = -1;
        sampleId_ = {};
    }
    else
    {
        trackIndex_ = std::clamp(trackIndex_, 0, static_cast<int>(pat->tracks.size()) - 1);
        if (const Clip* c = findSelectedClip(); scope_ == Scope::SelectedClip && c != nullptr)
        {
            sampleId_ = c->sampleId;
        }
        else
        {
            sampleId_ = song_.patternTrackSample(patternId_, trackIndex_);
            if (!sampleId_.isValid() && !pat->tracks[static_cast<std::size_t>(trackIndex_)].clips.empty())
                sampleId_ = pat->tracks[static_cast<std::size_t>(trackIndex_)].clips.front().sampleId;
        }
    }

    rebuildTrackSelector();
    syncControlsFromModel();
    repaint();
}

void WaveEditorPanel::rebuildTrackSelector()
{
    isUpdating_ = true;
    trackSelector_.clear(juce::dontSendNotification);
    if (const auto* pat = song_.findPattern(patternId_))
    {
        for (int i = 0; i < static_cast<int>(pat->tracks.size()); ++i)
        {
            const auto& trk = pat->tracks[static_cast<std::size_t>(i)];
            juce::String name = trk.name.empty() ? juce::String() : juce::String::fromUTF8(trk.name.c_str());
            if (name.isEmpty())
            {
                SampleId sid = trk.sampleId;
                if (!sid.isValid() && !trk.clips.empty())
                    sid = trk.clips.front().sampleId;
                if (const Sample* s = song_.findSample(sid))
                    name = juce::String::fromUTF8(s->name.c_str());
            }
            juce::String label = "Track " + juce::String(i + 1);
            if (name.isNotEmpty())
                label += " - " + name;
            trackSelector_.addItem(label, i + 1);
        }
        if (trackIndex_ >= 0 && trackIndex_ < static_cast<int>(pat->tracks.size()))
            trackSelector_.setSelectedId(trackIndex_ + 1, juce::dontSendNotification);
    }
    isUpdating_ = false;
}

void WaveEditorPanel::syncControlsFromModel()
{
    isUpdating_ = true;
    const bool hasAudio = sampleId_.isValid() && stretchCache_.getRawAudio(sampleId_) != nullptr;
    const bool hasSel = hasSelection();

    if (playBtn_ != nullptr)
        playBtn_->setEnabled(hasAudio);
    if (loopBtn_ != nullptr)
        loopBtn_->setEnabled(hasAudio);
    trimBtn_.setEnabled(hasAudio && hasSel);
    cutBtn_.setEnabled(hasAudio && hasSel);
    silenceBtn_.setEnabled(hasAudio && hasSel);
    normalizeBtn_.setEnabled(hasAudio);
    fadeInBtn_.setEnabled(hasAudio);
    fadeOutBtn_.setEnabled(hasAudio);
    gainSlider_.setEnabled(hasAudio);
    applyGainBtn_.setEnabled(hasAudio);

    autoDetectBtn_.setEnabled(hasAudio);
    beatGridBtn_.setEnabled(hasAudio);
    clearMarkersBtn_.setEnabled(hasAudio);
    toNewTrackBtn_.setEnabled(hasAudio && hasSel);

    const Clip* c = findSelectedClip();
    const bool hasClip = (c != nullptr);
    clipTrimBtn_.setEnabled(hasClip && hasAudio && hasSel);
    clipReverseBtn_.setEnabled(hasClip);
    makeUniqueBtn_.setEnabled(hasClip && hasAudio);
    clipGainSlider_.setEnabled(hasClip);
    if (hasClip)
    {
        clipReverseBtn_.setToggleState(c->reversed, juce::dontSendNotification);
        const double db = (c->gain <= 0.001f) ? -24.0 : std::clamp(20.0 * std::log10(static_cast<double>(c->gain)), -24.0, 6.0);
        clipGainSlider_.setValue(db, juce::dontSendNotification);
    }
    isUpdating_ = false;
}

void WaveEditorPanel::applySampleTrim()
{
    if (!hasSelection() || !sampleId_.isValid())
        return;
    auto raw = stretchCache_.getRawAudio(sampleId_);
    Sample* s = song_.findSample(sampleId_);
    if (raw == nullptr || s == nullptr || raw->numFrames() <= 0)
        return;

    const std::int64_t total = raw->numFrames();
    const std::int64_t f0 = std::clamp<std::int64_t>(static_cast<std::int64_t>(std::llround(selStartNorm_ * static_cast<double>(total))), 0, total - 1);
    const std::int64_t f1 = std::clamp<std::int64_t>(static_cast<std::int64_t>(std::llround(selEndNorm_ * static_cast<double>(total))), f0 + 1, total);
    const std::int64_t newLen = f1 - f0;
    if (newLen <= 64 || newLen >= total)
        return;

    if (onPushUndo)
        onPushUndo();

    AudioData trimmed;
    trimmed.channels.resize(raw->channels.size());
    for (std::size_t ch = 0; ch < raw->channels.size(); ++ch)
        trimmed.channels[ch].assign(raw->channels[ch].begin() + f0, raw->channels[ch].begin() + f1);

    const double ratio = static_cast<double>(newLen) / static_cast<double>(total);
    s->durationSeconds *= ratio;

    // Remap stretch markers that fall inside the trimmed range
    std::vector<double> nextMarkers;
    const double span = std::max(1e-6, selEndNorm_ - selStartNorm_);
    for (double m : s->stretchMarkers)
    {
        if (m > selStartNorm_ + 1e-4 && m < selEndNorm_ - 1e-4)
            nextMarkers.push_back((m - selStartNorm_) / span);
    }
    s->stretchMarkers = std::move(nextMarkers);

    stretchCache_.setRawAudio(sampleId_, std::make_shared<const AudioData>(std::move(trimmed)));
    stretchCache_.invalidateSample(sampleId_);
    selStartNorm_ = 0.0;
    selEndNorm_ = 0.0;
    setZoom(1.0);
    syncControlsFromModel();
    if (onChanged_)
        onChanged_();
    if (onShowToast)
        onShowToast("Trimmed sample audio");
    repaint();
}

void WaveEditorPanel::applySampleDelete()
{
    if (!hasSelection() || !sampleId_.isValid())
        return;
    auto raw = stretchCache_.getRawAudio(sampleId_);
    Sample* s = song_.findSample(sampleId_);
    if (raw == nullptr || s == nullptr || raw->numFrames() <= 0)
        return;

    const std::int64_t total = raw->numFrames();
    const std::int64_t f0 = std::clamp<std::int64_t>(static_cast<std::int64_t>(std::llround(selStartNorm_ * static_cast<double>(total))), 0, total - 1);
    const std::int64_t f1 = std::clamp<std::int64_t>(static_cast<std::int64_t>(std::llround(selEndNorm_ * static_cast<double>(total))), f0 + 1, total);
    const std::int64_t removedLen = f1 - f0;
    const std::int64_t remainingLen = total - removedLen;
    if (removedLen <= 0 || remainingLen <= 64)
        return;

    stopPreview();
    if (onPushUndo)
        onPushUndo();

    AudioData edited;
    edited.channels.resize(raw->channels.size());
    for (std::size_t ch = 0; ch < raw->channels.size(); ++ch)
    {
        auto& dst = edited.channels[ch];
        dst.reserve(static_cast<std::size_t>(remainingLen));
        dst.insert(dst.end(), raw->channels[ch].begin(), raw->channels[ch].begin() + f0);
        dst.insert(dst.end(), raw->channels[ch].begin() + f1, raw->channels[ch].end());
    }

    const double keepRatio = static_cast<double>(remainingLen) / static_cast<double>(total);
    s->durationSeconds *= keepRatio;

    // Remap stretch markers outside the deleted range
    std::vector<double> nextMarkers;
    const double delSpan = selEndNorm_ - selStartNorm_;
    const double keepSpan = std::max(1e-6, 1.0 - delSpan);
    for (double m : s->stretchMarkers)
    {
        if (m < selStartNorm_ - 1e-4)
            nextMarkers.push_back(m / keepSpan);
        else if (m > selEndNorm_ + 1e-4)
            nextMarkers.push_back((m - delSpan) / keepSpan);
    }
    s->stretchMarkers = std::move(nextMarkers);

    stretchCache_.setRawAudio(sampleId_, std::make_shared<const AudioData>(std::move(edited)));
    stretchCache_.invalidateSample(sampleId_);
    selStartNorm_ = 0.0;
    selEndNorm_ = 0.0;
    syncControlsFromModel();
    if (onChanged_)
        onChanged_();
    if (onShowToast)
        onShowToast("Removed selected region");
    repaint();
}

void WaveEditorPanel::applySampleSilence()
{
    if (!hasSelection() || !sampleId_.isValid())
        return;
    auto raw = stretchCache_.getRawAudio(sampleId_);
    if (raw == nullptr || raw->numFrames() <= 0)
        return;

    if (onPushUndo)
        onPushUndo();

    const std::int64_t total = raw->numFrames();
    const std::int64_t f0 = std::clamp<std::int64_t>(static_cast<std::int64_t>(std::llround(selStartNorm_ * static_cast<double>(total))), 0, total - 1);
    const std::int64_t f1 = std::clamp<std::int64_t>(static_cast<std::int64_t>(std::llround(selEndNorm_ * static_cast<double>(total))), f0 + 1, total);

    AudioData edited = *raw;
    for (auto& ch : edited.channels)
        std::fill(ch.begin() + f0, ch.begin() + f1, 0.0f);

    stretchCache_.setRawAudio(sampleId_, std::make_shared<const AudioData>(std::move(edited)));
    stretchCache_.invalidateSample(sampleId_);
    if (onChanged_)
        onChanged_();
    if (onShowToast)
        onShowToast("Silenced selected region");
    repaint();
}

void WaveEditorPanel::applySampleNormalize()
{
    if (!sampleId_.isValid())
        return;
    auto raw = stretchCache_.getRawAudio(sampleId_);
    if (raw == nullptr || raw->numFrames() <= 0)
        return;

    const std::int64_t total = raw->numFrames();
    const std::int64_t f0 = hasSelection()
        ? std::clamp<std::int64_t>(static_cast<std::int64_t>(std::llround(selStartNorm_ * static_cast<double>(total))), 0, total - 1)
        : 0;
    const std::int64_t f1 = hasSelection()
        ? std::clamp<std::int64_t>(static_cast<std::int64_t>(std::llround(selEndNorm_ * static_cast<double>(total))), f0 + 1, total)
        : total;

    float peak = 0.0f;
    for (const auto& ch : raw->channels)
        for (std::int64_t i = f0; i < f1; ++i)
            peak = std::max(peak, std::abs(ch[static_cast<std::size_t>(i)]));

    if (peak <= 1e-6f)
        return;

    if (onPushUndo)
        onPushUndo();

    const float targetPeak = 0.966f; // -0.3 dBFS
    const float gain = targetPeak / peak;
    AudioData edited = *raw;
    for (auto& ch : edited.channels)
        for (std::int64_t i = f0; i < f1; ++i)
            ch[static_cast<std::size_t>(i)] = std::clamp(ch[static_cast<std::size_t>(i)] * gain, -1.0f, 1.0f);

    stretchCache_.setRawAudio(sampleId_, std::make_shared<const AudioData>(std::move(edited)));
    stretchCache_.invalidateSample(sampleId_);
    if (onChanged_)
        onChanged_();
    if (onShowToast)
        onShowToast("Normalized waveform (-0.3 dBFS)");
    repaint();
}

void WaveEditorPanel::applySampleGain()
{
    const double db = gainSlider_.getValue();
    if (std::abs(db) < 0.01)
        return;

    applySampleGainDb(db);
    gainSlider_.setValue(0.0, juce::dontSendNotification);
}

void WaveEditorPanel::applySampleGainDb(double db)
{
    if (!sampleId_.isValid())
        return;
    auto raw = stretchCache_.getRawAudio(sampleId_);
    if (raw == nullptr || raw->numFrames() <= 0)
        return;

    const bool isMute = (db <= -47.9);
    if (!isMute && std::abs(db) < 0.01)
        return;

    if (onPushUndo)
        onPushUndo();

    const float gain = isMute ? 0.0f : static_cast<float>(std::pow(10.0, db / 20.0));
    const std::int64_t total = raw->numFrames();
    const std::int64_t f0 = hasSelection()
        ? std::clamp<std::int64_t>(static_cast<std::int64_t>(std::llround(selStartNorm_ * static_cast<double>(total))), 0, total - 1)
        : 0;
    const std::int64_t f1 = hasSelection()
        ? std::clamp<std::int64_t>(static_cast<std::int64_t>(std::llround(selEndNorm_ * static_cast<double>(total))), f0 + 1, total)
        : total;

    AudioData edited = *raw;
    for (auto& ch : edited.channels)
        for (std::int64_t i = f0; i < f1; ++i)
            ch[static_cast<std::size_t>(i)] = std::clamp(ch[static_cast<std::size_t>(i)] * gain, -1.0f, 1.0f);

    stretchCache_.setRawAudio(sampleId_, std::make_shared<const AudioData>(std::move(edited)));
    stretchCache_.invalidateSample(sampleId_);
    if (onChanged_)
        onChanged_();
    if (onShowToast)
        onShowToast(isMute ? juce::String("Muted selected region (-inf dB)")
                           : juce::String::formatted("Applied %+.1f dB gain", db));
    repaint();
}

void WaveEditorPanel::applySampleFade(bool fadeIn)
{
    if (!sampleId_.isValid())
        return;
    auto raw = stretchCache_.getRawAudio(sampleId_);
    if (raw == nullptr || raw->numFrames() <= 0)
        return;

    const std::int64_t total = raw->numFrames();
    std::int64_t f0 = 0;
    std::int64_t f1 = total;
    if (hasSelection())
    {
        f0 = std::clamp<std::int64_t>(static_cast<std::int64_t>(std::llround(selStartNorm_ * static_cast<double>(total))), 0, total - 1);
        f1 = std::clamp<std::int64_t>(static_cast<std::int64_t>(std::llround(selEndNorm_ * static_cast<double>(total))), f0 + 1, total);
    }
    else
    {
        const std::int64_t edgeLen = std::max<std::int64_t>(1, total / 10);
        if (fadeIn)
            f1 = edgeLen;
        else
            f0 = total - edgeLen;
    }

    const std::int64_t len = std::max<std::int64_t>(1, f1 - f0);
    if (onPushUndo)
        onPushUndo();
    AudioData edited = *raw;
    for (auto& ch : edited.channels)
    {
        for (std::int64_t i = f0; i < f1; ++i)
        {
            const float t = static_cast<float>(i - f0) / static_cast<float>(len);
            const float env = fadeIn ? t : (1.0f - t);
            ch[static_cast<std::size_t>(i)] *= env;
        }
    }

    stretchCache_.setRawAudio(sampleId_, std::make_shared<const AudioData>(std::move(edited)));
    stretchCache_.invalidateSample(sampleId_);
    if (onChanged_)
        onChanged_();
    if (onShowToast)
        onShowToast(fadeIn ? "Applied Fade In" : "Applied Fade Out");
    repaint();
}

void WaveEditorPanel::extractSelectionToNewTrack()
{
    if (!hasSelection() || !sampleId_.isValid())
        return;

    auto raw = stretchCache_.getRawAudio(sampleId_);
    const Sample* srcSample = song_.findSample(sampleId_);
    if (raw == nullptr || srcSample == nullptr || raw->numFrames() <= 0)
        return;

    const std::int64_t total = raw->numFrames();
    const std::int64_t f0 = std::clamp<std::int64_t>(
        static_cast<std::int64_t>(std::llround(selStartNorm_ * static_cast<double>(total))), 0, total - 1);
    const std::int64_t f1 = std::clamp<std::int64_t>(
        static_cast<std::int64_t>(std::llround(selEndNorm_ * static_cast<double>(total))), f0 + 1, total);
    const std::int64_t newLen = f1 - f0;
    if (newLen <= 16)
        return;

    if (onPushUndo)
        onPushUndo();

    AudioData sliced;
    sliced.channels.resize(raw->channels.size());
    for (std::size_t ch = 0; ch < raw->channels.size(); ++ch)
        sliced.channels[ch].assign(raw->channels[ch].begin() + f0, raw->channels[ch].begin() + f1);

    // Build a unique slice name based on the source sample name
    std::string baseName = srcSample->name.empty() ? "Wave" : srcSample->name;
    std::string candidateName = baseName + " (slice)";
    int suffix = 2;
    auto nameExists = [&](const std::string& candidate)
    {
        for (const auto& existing : song_.samples())
            if (existing.name == candidate)
                return true;
        return false;
    };
    while (nameExists(candidateName))
        candidateName = baseName + " (slice " + std::to_string(suffix++) + ")";

    Sample newSample;
    newSample.name = candidateName;
    newSample.fileName = candidateName + ".wav";
    newSample.kind = SampleKind::OneShot;
    newSample.loopBars = 1;
    newSample.loopBeats = 0.0;
    newSample.acidBpm = 0.0;
    newSample.rootNote = srcSample->rootNote;
    if (srcSample->durationSeconds > 0.0)
        newSample.durationSeconds = srcSample->durationSeconds * (static_cast<double>(newLen) / static_cast<double>(total));
    else
        newSample.durationSeconds = static_cast<double>(newLen) / std::max(8000.0, engine_.sampleRate());

    const double span = std::max(1e-6, selEndNorm_ - selStartNorm_);
    for (double m : srcSample->stretchMarkers)
    {
        if (m > selStartNorm_ + 1e-4 && m < selEndNorm_ - 1e-4)
            newSample.stretchMarkers.push_back((m - selStartNorm_) / span);
    }

    const SampleId newSampleId = song_.addSample(newSample);
    stretchCache_.setRawAudio(newSampleId, std::make_shared<const AudioData>(std::move(sliced)));

    const int newTrackIdx = song_.addTrackToPattern(patternId_, newSampleId);
    if (newTrackIdx >= 0)
    {
        song_.renamePatternTrack(patternId_, newTrackIdx, newSample.name);
        song_.setPatternTrackKind(patternId_, newTrackIdx, SampleKind::OneShot);
    }

    rebuildTrackSelector();
    syncControlsFromModel();

    if (onTrackCreated && newTrackIdx >= 0)
        onTrackCreated(newTrackIdx);
    else if (onChanged_)
        onChanged_();

    if (onShowToast && newTrackIdx >= 0)
        onShowToast("Created Track " + juce::String(newTrackIdx + 1) + " (" + juce::String::fromUTF8(newSample.name.c_str()) + ")");
    repaint();
}

void WaveEditorPanel::applyClipTrim()
{
    const Clip* c = findSelectedClip();
    const Sample* s = song_.findSample(sampleId_);
    if (c == nullptr || s == nullptr || !hasSelection())
        return;

    double fullSampleSteps = static_cast<double>(song_.stepsPerBar());
    const SampleKind kind = song_.patternTrackKind(patternId_, trackIndex_);
    if (kind == SampleKind::Loop)
    {
        fullSampleSteps = std::max(1.0, s->loopCycleBeats() * static_cast<double>(song_.stepsPerBeat()));
    }
    else if (s->durationSeconds > 0.0)
    {
        const double stepSec = (60.0 / std::max(20.0, song_.bpm())) / static_cast<double>(song_.stepsPerBeat());
        if (stepSec > 0.0)
            fullSampleSteps = std::max(0.25, s->durationSeconds / stepSec);
    }

    const double newOffset = selStartNorm_ * fullSampleSteps;
    const double newLen = std::max(0.25, (selEndNorm_ - selStartNorm_) * fullSampleSteps);

    if (onPushUndo)
        onPushUndo();
    song_.resizeClip(patternId_, selectedClipId_, c->startStep, newLen, newOffset);
    selStartNorm_ = 0.0;
    selEndNorm_ = 0.0;
    syncControlsFromModel();
    if (onChanged_)
        onChanged_();
    if (onShowToast)
        onShowToast("Trimmed selected clip non-destructively");
    repaint();
}

void WaveEditorPanel::makeSelectedClipUnique()
{
    const Clip* c = findSelectedClip();
    const Sample* origSample = song_.findSample(sampleId_);
    auto raw = stretchCache_.getRawAudio(sampleId_);
    if (c == nullptr || origSample == nullptr || raw == nullptr)
        return;

    if (onPushUndo)
        onPushUndo();

    Sample copy = *origSample;
    copy.id = {};
    copy.name = origSample->name + " (Clip Copy)";
    const auto dotPos = origSample->fileName.rfind('.');
    if (dotPos != std::string::npos)
        copy.fileName = origSample->fileName.substr(0, dotPos) + "_copy_" + std::to_string(c->id.value) + origSample->fileName.substr(dotPos);
    else
        copy.fileName = origSample->fileName + "_copy_" + std::to_string(c->id.value) + ".wav";

    const SampleId newSid = song_.addSample(copy);
    stretchCache_.setRawAudio(newSid, std::make_shared<const AudioData>(*raw));

    if (auto* pat = song_.findPattern(patternId_))
    {
        for (auto& trk : pat->tracks)
        {
            for (auto& cl : trk.clips)
            {
                if (cl.id == selectedClipId_)
                {
                    cl.sampleId = newSid;
                    break;
                }
            }
        }
    }

    sampleId_ = newSid;
    setScope(Scope::TrackSample);
    if (onChanged_)
        onChanged_();
    if (onShowToast)
        onShowToast("Created unique sample copy for this clip");
}

void WaveEditorPanel::setZoom(double newZoom, double anchorNorm)
{
    const double clampedZoom = std::clamp(newZoom, 1.0, 16.0);
    const double oldVis = 1.0 / zoom_;
    const double newVis = 1.0 / clampedZoom;

    if (anchorNorm < 0.0)
        anchorNorm = scrollNorm_ + oldVis * 0.5;

    const double fracInView = (oldVis > 1e-6) ? std::clamp((anchorNorm - scrollNorm_) / oldVis, 0.0, 1.0) : 0.5;
    zoom_ = clampedZoom;
    scrollNorm_ = std::clamp(anchorNorm - fracInView * newVis, 0.0, std::max(0.0, 1.0 - newVis));
    zoomLabel_.setText(juce::String(zoom_, 1) + "x", juce::dontSendNotification);
    repaint();
}

double WaveEditorPanel::normToX(double norm) const
{
    const double x0 = static_cast<double>(waveformRect_.getX() + 4);
    const double w = static_cast<double>(std::max(1, waveformRect_.getWidth() - 8));
    const double vis = 1.0 / std::max(1.0, zoom_);
    return x0 + ((norm - scrollNorm_) / vis) * w;
}

double WaveEditorPanel::xToNorm(int x) const
{
    const double x0 = static_cast<double>(waveformRect_.getX() + 4);
    const double w = static_cast<double>(std::max(1, waveformRect_.getWidth() - 8));
    const double vis = 1.0 / std::max(1.0, zoom_);
    const double frac = (static_cast<double>(x) - x0) / w;
    return std::clamp(scrollNorm_ + frac * vis, 0.0, 1.0);
}

juce::Rectangle<int> WaveEditorPanel::scrollThumbRect() const
{
    if (scrollRect_.isEmpty())
        return {};
    const double vis = 1.0 / std::max(1.0, zoom_);
    const int w = scrollRect_.getWidth();
    const int thumbW = std::max(24, juce::roundToInt(vis * static_cast<double>(w)));
    const int maxX = std::max(0, w - thumbW);
    const double maxScroll = std::max(1e-6, 1.0 - vis);
    const int thumbX = (zoom_ <= 1.001)
        ? 0
        : std::clamp(juce::roundToInt((scrollNorm_ / maxScroll) * static_cast<double>(maxX)), 0, maxX);
    return {scrollRect_.getX() + thumbX, scrollRect_.getY() + 1, thumbW, std::max(4, scrollRect_.getHeight() - 2)};
}

int WaveEditorPanel::hitTestMarker(juce::Point<int> pt) const
{
    if (!waveformRect_.contains(pt))
        return -1;
    const Sample* s = song_.findSample(sampleId_);
    if (s == nullptr)
        return -1;

    int bestIdx = -1;
    int bestDist = 8;
    for (std::size_t i = 0; i < s->stretchMarkers.size(); ++i)
    {
        const int mx = juce::roundToInt(normToX(s->stretchMarkers[i]));
        if (mx < waveformRect_.getX() || mx > waveformRect_.getRight())
            continue;
        const int d = std::abs(pt.x - mx);
        if (d <= bestDist)
        {
            bestDist = d;
            bestIdx = static_cast<int>(i);
        }
    }
    return bestIdx;
}

int WaveEditorPanel::hitTestSelectionEdge(juce::Point<int> pt) const
{
    if (!hasSelection() || !waveformRect_.contains(pt))
        return -1;
    const int sx0 = juce::roundToInt(normToX(selStartNorm_));
    const int sx1 = juce::roundToInt(normToX(selEndNorm_));
    const int d0 = std::abs(pt.x - sx0);
    const int d1 = std::abs(pt.x - sx1);
    constexpr int kEdgeHitPx = 7;
    if (d0 <= kEdgeHitPx && d1 <= kEdgeHitPx)
        return (d0 <= d1) ? 0 : 1;
    if (d0 <= kEdgeHitPx)
        return 0;
    if (d1 <= kEdgeHitPx)
        return 1;
    return -1;
}

bool WaveEditorPanel::isInsideSelection(juce::Point<int> pt) const
{
    if (!hasSelection() || !waveformRect_.contains(pt))
        return false;
    const int sx0 = juce::roundToInt(normToX(selStartNorm_));
    const int sx1 = juce::roundToInt(normToX(selEndNorm_));
    return pt.x > sx0 + 7 && pt.x < sx1 - 7;
}

void WaveEditorPanel::mouseMove(const juce::MouseEvent& e)
{
    const auto pt = e.getPosition();
    const int selEdge = hitTestSelectionEdge(pt);
    const int markerIdx = (selEdge < 0) ? hitTestMarker(pt) : -1;
    const bool insideSel = (selEdge < 0 && markerIdx < 0) && isInsideSelection(pt);

    const bool visualChanged = (selEdge != hoveredSelEdge_) ||
                               (markerIdx != hoveredMarkerIdx_) ||
                               (insideSel != hoveredInsideSel_);
    hoveredSelEdge_ = selEdge;
    hoveredMarkerIdx_ = markerIdx;
    hoveredInsideSel_ = insideSel;

    if (selEdge >= 0 || markerIdx >= 0)
        setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
    else if (insideSel)
        setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
    else if (waveformRect_.contains(pt))
        setMouseCursor(juce::MouseCursor::IBeamCursor);
    else
        setMouseCursor(juce::MouseCursor::NormalCursor);

    if (visualChanged)
        repaint();
}

void WaveEditorPanel::mouseDown(const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    if (scrollRect_.expanded(0, 2).contains(e.getPosition()) && zoom_ > 1.001)
    {
        draggingScroll_ = true;
        scrollDragStartX_ = e.x;
        scrollDragStartNorm_ = scrollNorm_;
        return;
    }

    if (!waveformRect_.contains(e.getPosition()))
        return;

    const int selEdge = hitTestSelectionEdge(e.getPosition());
    const int idx = hitTestMarker(e.getPosition());
    if (e.mods.isRightButtonDown())
    {
        if (idx >= 0)
        {
            if (Sample* s = song_.findSample(sampleId_))
            {
                if (onPushUndo)
                    onPushUndo();
                s->stretchMarkers.erase(s->stretchMarkers.begin() + idx);
                hoveredMarkerIdx_ = -1;
                if (onChanged_)
                    onChanged_();
                repaint();
            }
        }
        else if (hasSelection())
        {
            selStartNorm_ = 0.0;
            selEndNorm_ = 0.0;
            hoveredSelEdge_ = -1;
            hoveredInsideSel_ = false;
            syncControlsFromModel();
            repaint();
        }
        return;
    }

    // 0. Click in the top ruler strip (top 15 px of waveform) sets & scrubs the wave playhead
    if (e.y <= waveformRect_.getY() + 15)
    {
        draggingPlayhead_ = true;
        playheadNorm_ = xToNorm(e.x);
        if (isPreviewing_)
            startPreviewFrom(playheadNorm_);
        else
            repaint(waveformRect_);
        return;
    }

    // 1. Drag left or right border of marked selection region
    if (selEdge >= 0)
    {
        draggingSelEdge_ = selEdge;
        selAnchorNorm_ = (selEdge == 0) ? selEndNorm_ : selStartNorm_;
        return;
    }

    // 2. Drag stretch marker
    if (idx >= 0)
    {
        draggingMarkerIdx_ = idx;
        pushedUndoForMarkerDrag_ = false;
        return;
    }

    // 3. Drag up/down inside marked selection region to adjust volume
    if (isInsideSelection(e.getPosition()))
    {
        draggingSelGain_ = true;
        selGainDragStartY_ = e.y;
        selGainDragDb_ = 0.0;
        repaint();
        return;
    }

    // 4. Begin new waveform region selection
    draggingSelection_ = true;
    selAnchorNorm_ = xToNorm(e.x);
    selStartNorm_ = selAnchorNorm_;
    selEndNorm_ = selAnchorNorm_;
    syncControlsFromModel();
    repaint();
}

void WaveEditorPanel::mouseDrag(const juce::MouseEvent& e)
{
    if (draggingScroll_)
    {
        const double vis = 1.0 / std::max(1.0, zoom_);
        const int w = std::max(1, scrollRect_.getWidth());
        const double deltaNorm = static_cast<double>(e.x - scrollDragStartX_) / static_cast<double>(w);
        scrollNorm_ = std::clamp(scrollDragStartNorm_ + deltaNorm, 0.0, std::max(0.0, 1.0 - vis));
        repaint();
        return;
    }

    if (draggingPlayhead_)
    {
        playheadNorm_ = xToNorm(e.x);
        if (isPreviewing_)
            startPreviewFrom(playheadNorm_);
        else
            repaint(waveformRect_);
        return;
    }

    if (draggingSelEdge_ >= 0)
    {
        const double cur = xToNorm(e.x);
        selStartNorm_ = std::min(selAnchorNorm_, cur);
        selEndNorm_ = std::max(selAnchorNorm_, cur);
        syncControlsFromModel();
        repaint();
        return;
    }

    if (draggingSelGain_)
    {
        const double dy = static_cast<double>(selGainDragStartY_ - e.y); // up = louder, down = quieter
        const double dbPerPx = e.mods.isShiftDown() ? 0.05 : 0.20;
        double db = std::clamp(dy * dbPerPx, -48.0, 24.0);
        if (std::abs(db) < 0.2)
            db = 0.0;
        selGainDragDb_ = db;
        repaint();
        return;
    }

    if (draggingMarkerIdx_ >= 0)
    {
        Sample* s = song_.findSample(sampleId_);
        if (s == nullptr || draggingMarkerIdx_ >= static_cast<int>(s->stretchMarkers.size()))
            return;

        if (!pushedUndoForMarkerDrag_)
        {
            if (onPushUndo)
                onPushUndo();
            pushedUndoForMarkerDrag_ = true;
        }

        const double norm = std::clamp(xToNorm(e.x), 0.005, 0.995);
        s->stretchMarkers[static_cast<std::size_t>(draggingMarkerIdx_)] = norm;
        repaint();
        return;
    }

    if (draggingSelection_)
    {
        const double cur = xToNorm(e.x);
        selStartNorm_ = std::min(selAnchorNorm_, cur);
        selEndNorm_ = std::max(selAnchorNorm_, cur);
        syncControlsFromModel();
        repaint();
    }
}

void WaveEditorPanel::mouseUp(const juce::MouseEvent&)
{
    draggingScroll_ = false;
    draggingPlayhead_ = false;
    if (draggingSelEdge_ >= 0)
    {
        draggingSelEdge_ = -1;
        if (!hasSelection())
        {
            selStartNorm_ = 0.0;
            selEndNorm_ = 0.0;
        }
        else
        {
            if (playheadNorm_ < selStartNorm_ || playheadNorm_ > selEndNorm_)
                playheadNorm_ = selStartNorm_;
            if (isPreviewing_)
                startPreviewFrom(playheadNorm_);
        }
        syncControlsFromModel();
        repaint();
    }
    if (draggingSelGain_)
    {
        const double dbToApply = selGainDragDb_;
        draggingSelGain_ = false;
        selGainDragDb_ = 0.0;
        if (dbToApply <= -47.9 || std::abs(dbToApply) >= 0.05)
            applySampleGainDb(dbToApply);
        else
            repaint();
    }
    if (draggingSelection_)
    {
        draggingSelection_ = false;
        if (!hasSelection())
        {
            selStartNorm_ = 0.0;
            selEndNorm_ = 0.0;
            // A simple click on the waveform sets the wave playhead
            playheadNorm_ = std::clamp(selAnchorNorm_, 0.0, 1.0);
            if (isPreviewing_)
                startPreviewFrom(playheadNorm_);
        }
        else
        {
            playheadNorm_ = selStartNorm_;
            if (isPreviewing_)
                startPreviewFrom(playheadNorm_);
        }
        syncControlsFromModel();
        repaint();
    }
    if (draggingMarkerIdx_ >= 0)
    {
        if (pushedUndoForMarkerDrag_)
        {
            if (Sample* s = song_.findSample(sampleId_))
                std::sort(s->stretchMarkers.begin(), s->stretchMarkers.end());
            if (onChanged_)
                onChanged_();
        }
        draggingMarkerIdx_ = -1;
        pushedUndoForMarkerDrag_ = false;
        repaint();
    }
}

void WaveEditorPanel::mouseDoubleClick(const juce::MouseEvent& e)
{
    if (!waveformRect_.contains(e.getPosition()))
        return;

    Sample* s = song_.findSample(sampleId_);
    if (s == nullptr)
        return;

    if (onPushUndo)
        onPushUndo();
    const double norm = std::clamp(xToNorm(e.x), 0.005, 0.995);
    s->stretchMarkers.push_back(norm);
    std::sort(s->stretchMarkers.begin(), s->stretchMarkers.end());
    selStartNorm_ = 0.0;
    selEndNorm_ = 0.0;
    syncControlsFromModel();
    if (onChanged_)
        onChanged_();
    repaint();
}

void WaveEditorPanel::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (!waveformRect_.contains(e.getPosition()))
    {
        juce::Component::mouseWheelMove(e, wheel);
        return;
    }

    if (e.mods.isShiftDown() || (std::abs(wheel.deltaX) > std::abs(wheel.deltaY) && zoom_ > 1.001))
    {
        const double vis = 1.0 / std::max(1.0, zoom_);
        const double delta = (std::abs(wheel.deltaX) > 1e-4f ? -wheel.deltaX : -wheel.deltaY) * vis * 0.45;
        scrollNorm_ = std::clamp(scrollNorm_ + delta, 0.0, std::max(0.0, 1.0 - vis));
        repaint();
        return;
    }

    if (std::abs(wheel.deltaY) > 1e-4f)
    {
        const double factor = std::pow(1.28, static_cast<double>(wheel.deltaY) * 3.0);
        setZoom(zoom_ * factor, xToNorm(e.x));
    }
}

void WaveEditorPanel::mouseMagnify(const juce::MouseEvent& e, float scaleFactor)
{
    if (waveformRect_.contains(e.getPosition()) && scaleFactor > 0.01f)
        setZoom(zoom_ * static_cast<double>(scaleFactor), xToNorm(e.x));
}

void WaveEditorPanel::paint(juce::Graphics& g)
{
    g.fillAll(Palette::surface);
    g.setColour(Palette::gridLine);
    g.drawHorizontalLine(36, 0.0f, static_cast<float>(getWidth()));

    if (waveformRect_.isEmpty())
        return;

    g.setColour(Palette::background);
    g.fillRoundedRectangle(waveformRect_.toFloat(), 4.0f);
    g.setColour(Palette::gridBeatLine);
    g.drawRoundedRectangle(waveformRect_.toFloat(), 4.0f, 1.0f);

    const Sample* s = song_.findSample(sampleId_);
    auto raw = stretchCache_.getRawAudio(sampleId_);
    const int x0 = waveformRect_.getX() + 4;
    const int w = std::max(1, waveformRect_.getWidth() - 8);
    const int y0 = waveformRect_.getY() + 18;
    const int h = std::max(10, waveformRect_.getHeight() - 24);
    const float cy = static_cast<float>(y0) + static_cast<float>(h) * 0.5f;

    const double viewStart = scrollNorm_;
    const double viewSpan = 1.0 / std::max(1.0, zoom_);
    const double viewEnd = std::min(1.0, viewStart + viewSpan);

    // Ruler strip at top of waveform
    g.setColour(juce::Colour(0xff1c2128));
    g.fillRect(waveformRect_.getX() + 1, waveformRect_.getY() + 1, waveformRect_.getWidth() - 2, 16);

    g.saveState();
    g.reduceClipRegion(waveformRect_.reduced(2, 1));

    // Beat grid lines
    const int beats = (s != nullptr) ? std::max(1, static_cast<int>(std::round(s->loopCycleBeats()))) : 4;
    const int subdivsPerBeat = (zoom_ >= 4.0) ? 4 : 2;
    const int totalSubdivs = beats * subdivsPerBeat;
    for (int b = 0; b <= totalSubdivs; ++b)
    {
        const double norm = static_cast<double>(b) / static_cast<double>(totalSubdivs);
        if (norm < viewStart - 0.02 || norm > viewEnd + 0.02)
            continue;
        const int bx = juce::roundToInt(normToX(norm));
        const bool isBeat = (b % subdivsPerBeat == 0);
        g.setColour(isBeat ? Palette::gridBeatLine.withAlpha(0.75f) : Palette::gridLine.withAlpha(0.5f));
        g.drawVerticalLine(bx, static_cast<float>(y0), static_cast<float>(waveformRect_.getBottom() - 2));
        if (isBeat && b < totalSubdivs)
        {
            g.setColour(Palette::textDim);
            g.setFont(Typography::bold(9.5f));
            g.drawText(juce::String(b / subdivsPerBeat + 1) + ".1", bx + 3, waveformRect_.getY() + 1, 30, 14,
                       juce::Justification::centredLeft);
        }
    }

    // In Selected Clip scope, shade inactive region outside the clip's active slice
    if (scope_ == Scope::SelectedClip && s != nullptr)
    {
        if (const Clip* c = findSelectedClip())
        {
            double fullSteps = static_cast<double>(song_.stepsPerBar());
            const SampleKind kind = song_.patternTrackKind(patternId_, trackIndex_);
            if (kind == SampleKind::Loop)
                fullSteps = std::max(1.0, s->loopCycleBeats() * static_cast<double>(song_.stepsPerBeat()));
            else if (s->durationSeconds > 0.0)
            {
                const double stepSec = (60.0 / std::max(20.0, song_.bpm())) / static_cast<double>(song_.stepsPerBeat());
                if (stepSec > 0.0)
                    fullSteps = std::max(0.25, s->durationSeconds / stepSec);
            }
            const double span = (c->lengthSteps > 0.0) ? c->lengthSteps : fullSteps;
            const double cStartNorm = std::clamp(c->offsetSteps / fullSteps, 0.0, 1.0);
            const double cEndNorm = std::clamp((c->offsetSteps + span) / fullSteps, cStartNorm, 1.0);
            const int cx0 = juce::roundToInt(normToX(cStartNorm));
            const int cx1 = juce::roundToInt(normToX(cEndNorm));

            g.setColour(juce::Colours::black.withAlpha(0.45f));
            if (cx0 > x0)
                g.fillRect(x0, y0, cx0 - x0, h);
            if (cx1 < x0 + w)
                g.fillRect(cx1, y0, (x0 + w) - cx1, h);

            g.setColour(juce::Colour(0xff22d3ee).withAlpha(0.8f));
            g.drawVerticalLine(cx0, static_cast<float>(y0), static_cast<float>(y0 + h));
            g.drawVerticalLine(cx1, static_cast<float>(y0), static_cast<float>(y0 + h));
        }
    }

    // Dragged selection highlight
    if (hasSelection())
    {
        const int sx0 = juce::roundToInt(normToX(selStartNorm_));
        const int sx1 = juce::roundToInt(normToX(selEndNorm_));
        if (sx1 > sx0)
        {
            g.setColour(Palette::accent.withAlpha(draggingSelGain_ ? 0.28f : 0.22f));
            g.fillRect(sx0, y0, sx1 - sx0, h);

            const bool leftHot = (hoveredSelEdge_ == 0 || draggingSelEdge_ == 0);
            const bool rightHot = (hoveredSelEdge_ == 1 || draggingSelEdge_ == 1);

            g.setColour(leftHot ? juce::Colours::white : Palette::accent.withAlpha(0.92f));
            g.fillRect(sx0 - (leftHot ? 1 : 0), y0, leftHot ? 3 : 2, h);
            if (leftHot)
                g.fillRoundedRectangle(static_cast<float>(sx0 - 3), static_cast<float>(y0 + h / 2 - 12), 7.0f, 24.0f, 2.0f);

            g.setColour(rightHot ? juce::Colours::white : Palette::accent.withAlpha(0.92f));
            g.fillRect(sx1 - (rightHot ? 1 : 0), y0, rightHot ? 3 : 2, h);
            if (rightHot)
                g.fillRoundedRectangle(static_cast<float>(sx1 - 3), static_cast<float>(y0 + h / 2 - 12), 7.0f, 24.0f, 2.0f);
        }
    }

    // Waveform
    if (raw != nullptr && raw->numFrames() > 0)
    {
        const float* audioL = raw->left();
        const float* audioR = raw->right();
        const std::int64_t totalFrames = raw->numFrames();
        const float maxAmp = static_cast<float>(h) * 0.46f;
        const float clipGainVis = (scope_ == Scope::SelectedClip && findSelectedClip() != nullptr)
            ? findSelectedClip()->gain
            : 1.0f;
        const float liveSelGainVis = draggingSelGain_
            ? ((selGainDragDb_ <= -47.9) ? 0.0f : static_cast<float>(std::pow(10.0, selGainDragDb_ / 20.0)))
            : 1.0f;

        g.setColour(Palette::accent.withAlpha(0.25f));
        g.drawHorizontalLine(static_cast<int>(cy), static_cast<float>(x0), static_cast<float>(x0 + w));

        g.setColour(scope_ == Scope::SelectedClip ? juce::Colour(0xff38bdf8) : Palette::accent.brighter(0.2f));
        for (int px = 0; px < w; ++px)
        {
            const double n0 = viewStart + (static_cast<double>(px) / static_cast<double>(w)) * viewSpan;
            const double n1 = viewStart + (static_cast<double>(px + 1) / static_cast<double>(w)) * viewSpan;
            const std::int64_t f0 = std::clamp<std::int64_t>(static_cast<std::int64_t>(n0 * static_cast<double>(totalFrames)), 0, totalFrames);
            const std::int64_t f1 = std::clamp<std::int64_t>(static_cast<std::int64_t>(n1 * static_cast<double>(totalFrames)), f0, totalFrames);

            const bool inSel = draggingSelGain_ && hasSelection() && n1 > selStartNorm_ && n0 < selEndNorm_;
            const float totalGainVis = clipGainVis * (inSel ? liveSelGainVis : 1.0f);

            float minV = 0.0f;
            float maxV = 0.0f;
            if (f1 > f0)
            {
                const std::int64_t step = std::max<std::int64_t>(1, (f1 - f0) / 16);
                for (std::int64_t f = f0; f < f1; f += step)
                {
                    const float v = std::clamp(0.5f * (audioL[f] + audioR[f]) * totalGainVis, -1.0f, 1.0f);
                    minV = std::min(minV, v);
                    maxV = std::max(maxV, v);
                }
            }
            const float y1 = cy - maxV * maxAmp;
            const float y2 = cy - minV * maxAmp;
            g.drawVerticalLine(x0 + px, std::min(y1, y2), std::max(y1, y2) + 1.0f);
        }

        // Live gain badge while dragging volume up/down on a marked region
        if (draggingSelGain_ && hasSelection())
        {
            const int sx0 = std::max(x0, juce::roundToInt(normToX(selStartNorm_)));
            const int sx1 = std::min(x0 + w, juce::roundToInt(normToX(selEndNorm_)));
            const juce::String dbText = (selGainDragDb_ <= -47.9)
                ? juce::String("-inf dB")
                : juce::String::formatted("%+.1f dB", selGainDragDb_);
            const int badgeW = 72;
            const int badgeH = 22;
            const int bx = std::clamp((sx0 + sx1) / 2 - badgeW / 2, x0 + 4, std::max(x0 + 4, x0 + w - badgeW - 4));
            const int by = y0 + 18;
            const juce::Rectangle<int> badge(bx, by, badgeW, badgeH);
            g.setColour(juce::Colours::black.withAlpha(0.82f));
            g.fillRoundedRectangle(badge.toFloat(), 4.0f);
            g.setColour(Palette::accent);
            g.drawRoundedRectangle(badge.toFloat(), 4.0f, 1.0f);
            g.setColour(juce::Colours::white);
            g.setFont(Typography::bold(11.5f));
            g.drawText(dbText, badge, juce::Justification::centred);
        }
    }
    else
    {
        g.setColour(Palette::textDim);
        g.setFont(Typography::regular(12.5f));
        g.drawText(scope_ == Scope::SelectedClip
                       ? "Select a clip on the timeline to edit it here (or switch to Track Sample)."
                       : "No audio waveform on this track. Drag to select a range, double-click to add stretch markers.",
                   waveformRect_, juce::Justification::centred);
    }

    // Stretch markers overlay
    if (s != nullptr)
    {
        for (std::size_t i = 0; i < s->stretchMarkers.size(); ++i)
        {
            const double norm = s->stretchMarkers[i];
            if (norm < viewStart - 0.02 || norm > viewEnd + 0.02)
                continue;
            const int mx = juce::roundToInt(normToX(norm));
            const bool isHovered = (static_cast<int>(i) == hoveredMarkerIdx_ || static_cast<int>(i) == draggingMarkerIdx_);
            const juce::Colour mCol = isHovered ? juce::Colours::white : juce::Colour(0xfff59e0b);

            g.setColour(mCol.withAlpha(0.88f));
            g.drawVerticalLine(mx, static_cast<float>(waveformRect_.getY() + 14),
                               static_cast<float>(waveformRect_.getBottom() - 2));

            juce::Path flag;
            flag.startNewSubPath(static_cast<float>(mx - 5), static_cast<float>(waveformRect_.getY() + 14));
            flag.lineTo(static_cast<float>(mx + 5), static_cast<float>(waveformRect_.getY() + 14));
            flag.lineTo(static_cast<float>(mx), static_cast<float>(waveformRect_.getY() + 22));
            flag.closeSubPath();
            g.fillPath(flag);
        }
    }
    // Individual wave playhead line (always visible when audio is loaded)
    if (raw != nullptr && raw->numFrames() > 0)
    {
        const double activePh = isPreviewing_ ? previewPlayheadNorm_ : playheadNorm_;
        if (activePh >= viewStart - 0.01 && activePh <= viewEnd + 0.01)
        {
            const int px = juce::roundToInt(normToX(activePh));
            g.setColour(juce::Colour(0xff22d3ee).withAlpha(isPreviewing_ ? 1.0f : 0.72f));
            g.drawVerticalLine(px, static_cast<float>(waveformRect_.getY() + 1),
                               static_cast<float>(waveformRect_.getBottom() - 2));
            g.setColour(isPreviewing_ ? juce::Colours::white : juce::Colour(0xff22d3ee));
            juce::Path head;
            head.startNewSubPath(static_cast<float>(px - 5), static_cast<float>(waveformRect_.getY() + 1));
            head.lineTo(static_cast<float>(px + 5), static_cast<float>(waveformRect_.getY() + 1));
            head.lineTo(static_cast<float>(px), static_cast<float>(waveformRect_.getY() + 9));
            head.closeSubPath();
            g.fillPath(head);
        }
    }
    g.restoreState();

    // Subtle focus highlight border when Wave Editor has keyboard focus
    if (hasKeyboardFocus(true))
    {
        g.setColour(Palette::accent.withAlpha(0.65f));
        g.drawRoundedRectangle(waveformRect_.toFloat().reduced(0.5f), 4.0f, 1.5f);
    }

    // Horizontal zoom scrollbar below waveform
    if (!scrollRect_.isEmpty())
    {
        g.setColour(Palette::background);
        g.fillRoundedRectangle(scrollRect_.toFloat(), 3.0f);
        const auto thumb = scrollThumbRect();
        g.setColour(zoom_ > 1.001 ? Palette::accent.withAlpha(0.55f) : Palette::gridBeatLine);
        g.fillRoundedRectangle(thumb.toFloat(), 3.0f);
    }

    // Visual group container around Stretch Marker controls
    if (!stretchGroupRect_.isEmpty())
    {
        g.setColour(Palette::background.brighter(0.05f));
        g.fillRoundedRectangle(stretchGroupRect_.toFloat(), 5.0f);
        g.setColour(Palette::gridLine.brighter(0.1f));
        g.drawRoundedRectangle(stretchGroupRect_.toFloat().reduced(0.5f), 5.0f, 1.0f);
    }
}

void WaveEditorPanel::resized()
{
    auto area = getLocalBounds().reduced(10, 6);
    auto topBar = area.removeFromTop(24);

    if (playBtn_ != nullptr)
        playBtn_->setBounds(topBar.removeFromLeft(30));
    topBar.removeFromLeft(3);
    if (loopBtn_ != nullptr)
        loopBtn_->setBounds(topBar.removeFromLeft(30));
    topBar.removeFromLeft(8);
    trackScopeBtn_.setBounds(topBar.removeFromLeft(90));
    topBar.removeFromLeft(3);
    clipScopeBtn_.setBounds(topBar.removeFromLeft(90));
    topBar.removeFromLeft(6);
    trackSelector_.setBounds(topBar.removeFromLeft(std::clamp(topBar.getWidth() / 5, 110, 185)));
    topBar.removeFromLeft(8);

    if (scope_ == Scope::TrackSample)
    {
        trimBtn_.setBounds(topBar.removeFromLeft(46));
        topBar.removeFromLeft(3);
        cutBtn_.setBounds(topBar.removeFromLeft(54));
        topBar.removeFromLeft(3);
        silenceBtn_.setBounds(topBar.removeFromLeft(56));
        topBar.removeFromLeft(3);
        normalizeBtn_.setBounds(topBar.removeFromLeft(70));
        topBar.removeFromLeft(3);
        fadeInBtn_.setBounds(topBar.removeFromLeft(56));
        topBar.removeFromLeft(3);
        fadeOutBtn_.setBounds(topBar.removeFromLeft(62));
        topBar.removeFromLeft(6);

        applyGainBtn_.setBounds(topBar.removeFromRight(48));
        topBar.removeFromRight(3);
        gainLabel_.setBounds(topBar.removeFromLeft(36));
        gainSlider_.setBounds(topBar);
    }
    else
    {
        clipTrimBtn_.setBounds(topBar.removeFromLeft(74));
        topBar.removeFromLeft(4);
        clipReverseBtn_.setBounds(topBar.removeFromLeft(66));
        topBar.removeFromLeft(4);
        makeUniqueBtn_.setBounds(topBar.removeFromLeft(92));
        topBar.removeFromLeft(10);
        clipGainLabel_.setBounds(topBar.removeFromLeft(66));
        clipGainSlider_.setBounds(topBar.withWidth(std::min(220, topBar.getWidth())));
    }

    area.removeFromTop(8);

    // Bottom stretch group + extract + zoom bar
    auto btnRow = area.removeFromBottom(26);
    const int stretchGroupW = 8 + 54 + 86 + 3 + 74 + 3 + 50 + 6;
    stretchGroupRect_ = btnRow.removeFromLeft(stretchGroupW);
    auto sg = stretchGroupRect_.reduced(6, 2);
    stretchGroupLabel_.setBounds(sg.removeFromLeft(54));
    autoDetectBtn_.setBounds(sg.removeFromLeft(86));
    sg.removeFromLeft(3);
    beatGridBtn_.setBounds(sg.removeFromLeft(74));
    sg.removeFromLeft(3);
    clearMarkersBtn_.setBounds(sg.removeFromLeft(50));

    btnRow.removeFromLeft(14);
    toNewTrackBtn_.setBounds(btnRow.removeFromLeft(176).reduced(0, 1));

    zoomResetBtn_.setBounds(btnRow.removeFromRight(36));
    btnRow.removeFromRight(4);
    zoomInBtn_.setBounds(btnRow.removeFromRight(26));
    btnRow.removeFromRight(2);
    zoomLabel_.setBounds(btnRow.removeFromRight(42));
    btnRow.removeFromRight(2);
    zoomOutBtn_.setBounds(btnRow.removeFromRight(26));

    area.removeFromBottom(4);
    scrollRect_ = area.removeFromBottom(10);
    area.removeFromBottom(4);
    waveformRect_ = area;
}

} // namespace trami
