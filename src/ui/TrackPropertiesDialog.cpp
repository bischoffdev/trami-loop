#include "ui/TrackPropertiesDialog.h"

#include <algorithm>
#include <cmath>

namespace trami
{

TrackPropertiesComponent::TrackPropertiesComponent(Song& song, StretchCache& stretchCache,
                                                   PatternId patternId, int trackIndex,
                                                   PropertiesChangedCallback onChanged)
    : song_(song), stretchCache_(stretchCache), patternId_(patternId),
      trackIndex_(trackIndex), onChanged_(std::move(onChanged))
{
    setSize(560, 240);

    // Header: Track Selector + Sub-tabs
    trackSelectLabel_.setFont(Typography::bold(11.5f));
    trackSelectLabel_.setColour(juce::Label::textColourId, Palette::textDim);
    addAndMakeVisible(trackSelectLabel_);

    trackSelectorCombo_.setTextWhenNoChoicesAvailable("No Tracks");
    trackSelectorCombo_.onChange = [this]()
    {
        if (isLoading_)
            return;
        const int idx = trackSelectorCombo_.getSelectedId() - 1;
        if (idx >= 0 && idx != trackIndex_)
            setTrack(patternId_, idx);
    };
    addAndMakeVisible(trackSelectorCombo_);

    generalTabBtn_.onClick = [this]() { switchTab(ActiveTab::General); };
    stretchTabBtn_.onClick = [this]() { switchTab(ActiveTab::Stretch); };
    summaryTabBtn_.onClick = [this]() { switchTab(ActiveTab::Summary); };
    addAndMakeVisible(generalTabBtn_);
    addAndMakeVisible(stretchTabBtn_);
    addAndMakeVisible(summaryTabBtn_);

    // --- General Tab ---
    for (auto* lbl : {&trackTypeLabel_, &beatsLabel_, &rootNoteLabel_, &origBpmLabel_, &fxSendLabel_})
    {
        lbl->setFont(Typography::bold(12.0f));
        lbl->setColour(juce::Label::textColourId, Palette::textDim);
        addAndMakeVisible(lbl);
    }

    trackTypeCombo_.addItem("Loop (Time-Stretched to Tempo)", 1);
    trackTypeCombo_.addItem("One-Shot (Natural Speed / Unstretched)", 2);
    trackTypeCombo_.addItem("Disk-Based (Stream Long Audio)", 3);
    trackTypeCombo_.onChange = [this]() { if (!isLoading_) applyGeneralChanges(); };
    addAndMakeVisible(trackTypeCombo_);

    beatsCombo_.addItem("1 Beat (1/4 Bar)", 1);
    beatsCombo_.addItem("2 Beats (1/2 Bar)", 2);
    beatsCombo_.addItem("4 Beats (1 Bar)", 4);
    beatsCombo_.addItem("8 Beats (2 Bars)", 8);
    beatsCombo_.addItem("16 Beats (4 Bars)", 16);
    beatsCombo_.addItem("32 Beats (8 Bars)", 32);
    beatsCombo_.addItem("64 Beats (16 Bars)", 64);
    beatsCombo_.onChange = [this]() { if (!isLoading_) applyGeneralChanges(); };
    addAndMakeVisible(beatsCombo_);

    rootNoteCombo_.addItem("Don't Transpose (Drums / Unpitched)", 100);
    static constexpr const char* kNotes[12] = {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };
    for (int i = 0; i < 12; ++i)
        rootNoteCombo_.addItem(kNotes[i], i + 1);
    rootNoteCombo_.onChange = [this]() { if (!isLoading_) applyGeneralChanges(); };
    addAndMakeVisible(rootNoteCombo_);

    origBpmEditor_.setColour(juce::TextEditor::backgroundColourId, Palette::background);
    origBpmEditor_.setColour(juce::TextEditor::textColourId, Palette::text);
    origBpmEditor_.setColour(juce::TextEditor::outlineColourId, Palette::gridBeatLine);
    origBpmEditor_.onReturnKey = [this]() { if (!isLoading_) applyGeneralChanges(); };
    origBpmEditor_.onFocusLost = [this]() { if (!isLoading_) applyGeneralChanges(); };
    addAndMakeVisible(origBpmEditor_);

    fxSendSlider_.setRange(0.0, 1.0, 0.01);
    fxSendSlider_.onValueChange = [this]()
    {
        if (isLoading_ || !hasValidTrack())
            return;
        song_.setTrackFxSend(patternId_, trackIndex_, static_cast<float>(fxSendSlider_.getValue()));
        if (onChanged_)
            onChanged_();
    };
    addAndMakeVisible(fxSendSlider_);

    // --- Stretch Tab ---
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

    stretchZoomOutBtn_.setTooltip("Zoom Out Stretch Waveform");
    stretchZoomOutBtn_.onClick = [this]() { setStretchZoom(stretchZoom_ / 1.5); };
    addAndMakeVisible(stretchZoomOutBtn_);

    stretchZoomLabel_.setFont(Typography::bold(11.0f));
    stretchZoomLabel_.setColour(juce::Label::textColourId, Palette::textDim);
    stretchZoomLabel_.setJustificationType(juce::Justification::centred);
    stretchZoomLabel_.setText("1.0x", juce::dontSendNotification);
    addAndMakeVisible(stretchZoomLabel_);

    stretchZoomInBtn_.setTooltip("Zoom In Stretch Waveform (or scroll/pinch on waveform)");
    stretchZoomInBtn_.onClick = [this]() { setStretchZoom(stretchZoom_ * 1.5); };
    addAndMakeVisible(stretchZoomInBtn_);

    stretchZoomResetBtn_.setTooltip("Reset Stretch Waveform Zoom to 1:1");
    stretchZoomResetBtn_.onClick = [this]() { setStretchZoom(1.0); };
    addAndMakeVisible(stretchZoomResetBtn_);

    // --- Summary Tab ---
    fileInfoLabel_.setFont(Typography::regular(11.5f));
    fileInfoLabel_.setColour(juce::Label::textColourId, Palette::text);
    addAndMakeVisible(fileInfoLabel_);

    for (auto* lbl : {&titleLabel_, &authorLabel_, &copyrightLabel_, &commentsLabel_})
    {
        lbl->setFont(Typography::bold(11.5f));
        lbl->setColour(juce::Label::textColourId, Palette::textDim);
        addAndMakeVisible(lbl);
    }

    for (auto* ed : {&titleEditor_, &authorEditor_, &copyrightEditor_, &commentsEditor_})
    {
        ed->setColour(juce::TextEditor::backgroundColourId, Palette::background);
        ed->setColour(juce::TextEditor::textColourId, Palette::text);
        ed->setColour(juce::TextEditor::outlineColourId, Palette::gridBeatLine);
        ed->onTextChange = [this]() { if (!isLoading_) applySummaryChanges(); };
        addAndMakeVisible(ed);
    }
    commentsEditor_.setMultiLine(true);
    commentsEditor_.setReturnKeyStartsNewLine(true);

    refreshFromModel();
    switchTab(ActiveTab::General);
}

bool TrackPropertiesComponent::hasValidTrack() const
{
    const auto* pat = song_.findPattern(patternId_);
    return pat != nullptr && trackIndex_ >= 0 && trackIndex_ < static_cast<int>(pat->tracks.size());
}

void TrackPropertiesComponent::setTrack(PatternId patternId, int trackIndex)
{
    const bool changed = (patternId_ != patternId || trackIndex_ != trackIndex);
    patternId_ = patternId;
    trackIndex_ = trackIndex;
    if (changed)
    {
        stretchZoom_ = 1.0;
        stretchScrollNorm_ = 0.0;
        stretchZoomLabel_.setText("1.0x", juce::dontSendNotification);
    }
    refreshFromModel();
}

void TrackPropertiesComponent::refreshFromModel()
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
        sampleId_ = song_.patternTrackSample(patternId_, trackIndex_);
        if (!sampleId_.isValid() && !pat->tracks[static_cast<std::size_t>(trackIndex_)].clips.empty())
            sampleId_ = pat->tracks[static_cast<std::size_t>(trackIndex_)].clips.front().sampleId;
    }

    rebuildTrackSelector();
    loadFromModel();
    switchTab(activeTab_);
}

void TrackPropertiesComponent::rebuildTrackSelector()
{
    const bool prevLoading = isLoading_;
    isLoading_ = true;
    trackSelectorCombo_.clear(juce::dontSendNotification);

    const auto* pat = song_.findPattern(patternId_);
    if (pat != nullptr)
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
            trackSelectorCombo_.addItem(label, i + 1);
        }

        if (trackIndex_ >= 0 && trackIndex_ < static_cast<int>(pat->tracks.size()))
            trackSelectorCombo_.setSelectedId(trackIndex_ + 1, juce::dontSendNotification);
    }

    isLoading_ = prevLoading;
}

void TrackPropertiesComponent::loadFromModel()
{
    const bool prevLoading = isLoading_;
    isLoading_ = true;

    const bool valid = hasValidTrack();
    for (juce::Component* c : std::initializer_list<juce::Component*>{
             &trackTypeCombo_, &beatsCombo_, &rootNoteCombo_, &origBpmEditor_, &fxSendSlider_,
             &autoDetectBtn_, &beatGridBtn_, &clearMarkersBtn_,
             &titleEditor_, &authorEditor_, &copyrightEditor_, &commentsEditor_})
    {
        c->setEnabled(valid);
    }

    if (!valid)
    {
        fileInfoLabel_.setText("No track available. Add a track to edit its properties.", juce::dontSendNotification);
        titleEditor_.setText({}, juce::dontSendNotification);
        authorEditor_.setText({}, juce::dontSendNotification);
        copyrightEditor_.setText({}, juce::dontSendNotification);
        commentsEditor_.setText({}, juce::dontSendNotification);
        isLoading_ = prevLoading;
        return;
    }

    const SampleKind kind = song_.patternTrackKind(patternId_, trackIndex_);
    if (kind == SampleKind::Loop)
        trackTypeCombo_.setSelectedId(1, juce::dontSendNotification);
    else if (kind == SampleKind::OneShot)
        trackTypeCombo_.setSelectedId(2, juce::dontSendNotification);
    else
        trackTypeCombo_.setSelectedId(3, juce::dontSendNotification);

    const auto* pat = song_.findPattern(patternId_);
    if (pat != nullptr && trackIndex_ >= 0 && trackIndex_ < static_cast<int>(pat->tracks.size()))
        fxSendSlider_.setValue(pat->tracks[static_cast<std::size_t>(trackIndex_)].fxSend, juce::dontSendNotification);

    const Sample* s = song_.findSample(sampleId_);
    if (s != nullptr)
    {
        const int beats = std::max(1, static_cast<int>(std::round(s->loopCycleBeats())));
        if (beatsCombo_.indexOfItemId(beats) >= 0)
            beatsCombo_.setSelectedId(beats, juce::dontSendNotification);
        else
            beatsCombo_.setSelectedId(4, juce::dontSendNotification);

        if (s->rootNote >= 0)
            rootNoteCombo_.setSelectedId((s->rootNote % 12) + 1, juce::dontSendNotification);
        else
            rootNoteCombo_.setSelectedId(100, juce::dontSendNotification);

        const double bpmVal = (s->acidBpm > 0.0) ? s->acidBpm : s->originalBpm();
        origBpmEditor_.setText(bpmVal > 0.0 ? juce::String(bpmVal, 2) : "120.00", juce::dontSendNotification);

        auto raw = stretchCache_.getRawAudio(sampleId_);
        const int ch = raw != nullptr ? raw->numChannels() : 2;
        const std::int64_t frames = raw != nullptr ? raw->numFrames() : 0;

        juce::String info = "File: " + juce::String(s->fileName.empty() ? s->name : s->fileName) +
                            "   |   Duration: " + juce::String(s->durationSeconds, 2) + " s" +
                            "   |   Frames: " + juce::String(frames) +
                            "   |   Channels: " + juce::String(ch) +
                            (s->missing ? "   [MISSING ON DISK]" : "");
        fileInfoLabel_.setText(info, juce::dontSendNotification);

        const std::string trkName = song_.patternTrackName(patternId_, trackIndex_);
        titleEditor_.setText(juce::String(!trkName.empty() ? trkName : s->name), juce::dontSendNotification);
        authorEditor_.setText(juce::String(s->author), juce::dontSendNotification);
        copyrightEditor_.setText(juce::String(s->copyright), juce::dontSendNotification);
        commentsEditor_.setText(juce::String(s->comments), juce::dontSendNotification);
    }
    else
    {
        const std::string trkName = song_.patternTrackName(patternId_, trackIndex_);
        titleEditor_.setText(juce::String(trkName), juce::dontSendNotification);
        authorEditor_.setText({}, juce::dontSendNotification);
        copyrightEditor_.setText({}, juce::dontSendNotification);
        commentsEditor_.setText({}, juce::dontSendNotification);
        origBpmEditor_.setText("120.00", juce::dontSendNotification);
        beatsCombo_.setSelectedId(4, juce::dontSendNotification);
        rootNoteCombo_.setSelectedId(100, juce::dontSendNotification);
        fileInfoLabel_.setText("No sample assigned to this track.", juce::dontSendNotification);
    }

    isLoading_ = prevLoading;
}

void TrackPropertiesComponent::applyGeneralChanges()
{
    if (!hasValidTrack())
        return;

    const int typeId = trackTypeCombo_.getSelectedId();
    SampleKind newKind = SampleKind::Loop;
    if (typeId == 2)
        newKind = SampleKind::OneShot;
    else if (typeId == 3)
        newKind = SampleKind::DiskStream;

    if (song_.patternTrackKind(patternId_, trackIndex_) != newKind)
        song_.setPatternTrackKind(patternId_, trackIndex_, newKind);

    if (Sample* s = song_.findSample(sampleId_))
    {
        s->kind = newKind;
        const int beatsId = beatsCombo_.getSelectedId();
        if (beatsId > 0)
        {
            s->loopBeats = static_cast<double>(beatsId);
            s->loopBars = std::max(1, (beatsId + 3) / 4);
            if (s->durationSeconds > 0.0)
                s->acidBpm = (s->loopBeats * 60.0) / s->durationSeconds;
        }

        const int rootId = rootNoteCombo_.getSelectedId();
        if (rootId == 100)
            s->rootNote = -1;
        else if (rootId >= 1 && rootId <= 12)
            s->rootNote = 60 + (rootId - 1);

        const double editedBpm = origBpmEditor_.getText().getDoubleValue();
        if (editedBpm >= 20.0 && editedBpm <= 300.0)
            s->acidBpm = editedBpm;

        stretchCache_.invalidateSample(sampleId_);
    }

    if (onChanged_)
        onChanged_();
    repaint();
}

void TrackPropertiesComponent::applySummaryChanges()
{
    if (!hasValidTrack())
        return;

    const std::string newTitle = titleEditor_.getText().trim().toStdString();
    if (!newTitle.empty())
        song_.renamePatternTrack(patternId_, trackIndex_, newTitle);

    if (Sample* s = song_.findSample(sampleId_))
    {
        if (!newTitle.empty())
            s->name = newTitle;
        s->author = authorEditor_.getText().toStdString();
        s->copyright = copyrightEditor_.getText().toStdString();
        s->comments = commentsEditor_.getText().toStdString();
    }
    rebuildTrackSelector();
    if (onChanged_)
        onChanged_();
}

void TrackPropertiesComponent::switchTab(ActiveTab tab)
{
    activeTab_ = tab;

    const bool isGen = (tab == ActiveTab::General);
    const bool isStr = (tab == ActiveTab::Stretch);
    const bool isSum = (tab == ActiveTab::Summary);

    generalTabBtn_.setColour(juce::TextButton::buttonColourId, isGen ? Palette::accent.withAlpha(0.28f) : Palette::surface);
    stretchTabBtn_.setColour(juce::TextButton::buttonColourId, isStr ? Palette::accent.withAlpha(0.28f) : Palette::surface);
    summaryTabBtn_.setColour(juce::TextButton::buttonColourId, isSum ? Palette::accent.withAlpha(0.28f) : Palette::surface);

    for (juce::Component* c : std::initializer_list<juce::Component*>{
             &trackTypeLabel_, &trackTypeCombo_, &beatsLabel_, &beatsCombo_,
             &rootNoteLabel_, &rootNoteCombo_, &origBpmLabel_, &origBpmEditor_,
             &fxSendLabel_, &fxSendSlider_})
        c->setVisible(isGen);

    for (juce::Component* c : std::initializer_list<juce::Component*>{
             &autoDetectBtn_, &beatGridBtn_, &clearMarkersBtn_,
             &stretchZoomOutBtn_, &stretchZoomLabel_, &stretchZoomInBtn_, &stretchZoomResetBtn_})
        c->setVisible(isStr);

    for (juce::Component* c : std::initializer_list<juce::Component*>{
             &fileInfoLabel_, &titleLabel_, &titleEditor_, &authorLabel_, &authorEditor_,
             &copyrightLabel_, &copyrightEditor_, &commentsLabel_, &commentsEditor_})
        c->setVisible(isSum);

    resized();
    repaint();
}

void TrackPropertiesComponent::resized()
{
    auto area = getLocalBounds().reduced(10, 8);
    auto topBar = area.removeFromTop(26);

    trackSelectLabel_.setBounds(topBar.removeFromLeft(44));
    trackSelectorCombo_.setBounds(topBar.removeFromLeft(std::min(220, std::max(120, topBar.getWidth() / 3))));
    topBar.removeFromLeft(14);

    generalTabBtn_.setBounds(topBar.removeFromLeft(88));
    topBar.removeFromLeft(4);
    stretchTabBtn_.setBounds(topBar.removeFromLeft(88));
    topBar.removeFromLeft(4);
    summaryTabBtn_.setBounds(topBar.removeFromLeft(88));

    area.removeFromTop(10);

    if (activeTab_ == ActiveTab::General)
    {
        const bool twoCol = (area.getWidth() >= 680);
        constexpr int rowH = 26;
        constexpr int gap = 8;

        if (twoCol)
        {
            const int colGap = 24;
            const int colW = (area.getWidth() - colGap) / 2;
            auto leftCol = area.removeFromLeft(colW);
            area.removeFromLeft(colGap);
            auto rightCol = area;

            constexpr int labelW = 148;

            auto r1 = leftCol.removeFromTop(rowH);
            trackTypeLabel_.setBounds(r1.removeFromLeft(labelW));
            trackTypeCombo_.setBounds(r1);
            leftCol.removeFromTop(gap);

            auto r2 = leftCol.removeFromTop(rowH);
            beatsLabel_.setBounds(r2.removeFromLeft(labelW));
            beatsCombo_.setBounds(r2);
            leftCol.removeFromTop(gap);

            auto r3 = leftCol.removeFromTop(rowH);
            rootNoteLabel_.setBounds(r3.removeFromLeft(labelW));
            rootNoteCombo_.setBounds(r3);

            auto r4 = rightCol.removeFromTop(rowH);
            origBpmLabel_.setBounds(r4.removeFromLeft(labelW));
            origBpmEditor_.setBounds(r4.removeFromLeft(120));
            rightCol.removeFromTop(gap);

            auto r5 = rightCol.removeFromTop(rowH);
            fxSendLabel_.setBounds(r5.removeFromLeft(labelW));
            fxSendSlider_.setBounds(r5);
        }
        else
        {
            constexpr int labelW = 160;
            auto row1 = area.removeFromTop(rowH);
            trackTypeLabel_.setBounds(row1.removeFromLeft(labelW));
            trackTypeCombo_.setBounds(row1);
            area.removeFromTop(gap);

            auto row2 = area.removeFromTop(rowH);
            beatsLabel_.setBounds(row2.removeFromLeft(labelW));
            beatsCombo_.setBounds(row2);
            area.removeFromTop(gap);

            auto row3 = area.removeFromTop(rowH);
            rootNoteLabel_.setBounds(row3.removeFromLeft(labelW));
            rootNoteCombo_.setBounds(row3);
            area.removeFromTop(gap);

            auto row4 = area.removeFromTop(rowH);
            origBpmLabel_.setBounds(row4.removeFromLeft(labelW));
            origBpmEditor_.setBounds(row4.removeFromLeft(130));
            area.removeFromTop(gap);

            auto row5 = area.removeFromTop(rowH);
            fxSendLabel_.setBounds(row5.removeFromLeft(labelW));
            fxSendSlider_.setBounds(row5);
        }
    }
    else if (activeTab_ == ActiveTab::Stretch)
    {
        auto btnRow = area.removeFromBottom(26);
        autoDetectBtn_.setBounds(btnRow.removeFromLeft(165));
        btnRow.removeFromLeft(6);
        beatGridBtn_.setBounds(btnRow.removeFromLeft(124));
        btnRow.removeFromLeft(6);
        clearMarkersBtn_.setBounds(btnRow.removeFromLeft(114));

        stretchZoomResetBtn_.setBounds(btnRow.removeFromRight(36));
        btnRow.removeFromRight(4);
        stretchZoomInBtn_.setBounds(btnRow.removeFromRight(26));
        btnRow.removeFromRight(2);
        stretchZoomLabel_.setBounds(btnRow.removeFromRight(42));
        btnRow.removeFromRight(2);
        stretchZoomOutBtn_.setBounds(btnRow.removeFromRight(26));

        area.removeFromBottom(4);
        stretchScrollRect_ = area.removeFromBottom(10);
        area.removeFromBottom(4);
        stretchWaveformRect_ = area;
    }
    else if (activeTab_ == ActiveTab::Summary)
    {
        fileInfoLabel_.setBounds(area.removeFromTop(20));
        area.removeFromTop(6);

        const bool twoCol = (area.getWidth() >= 680);
        constexpr int labelW = 136;
        constexpr int rowH = 24;

        if (twoCol)
        {
            const int colGap = 20;
            const int colW = (area.getWidth() - colGap) / 2;
            auto leftCol = area.removeFromLeft(colW);
            area.removeFromLeft(colGap);
            auto rightCol = area;

            auto r1 = leftCol.removeFromTop(rowH);
            titleLabel_.setBounds(r1.removeFromLeft(labelW));
            titleEditor_.setBounds(r1);
            leftCol.removeFromTop(6);

            auto r2 = leftCol.removeFromTop(rowH);
            authorLabel_.setBounds(r2.removeFromLeft(labelW));
            authorEditor_.setBounds(r2);
            leftCol.removeFromTop(6);

            auto r3 = leftCol.removeFromTop(rowH);
            copyrightLabel_.setBounds(r3.removeFromLeft(labelW));
            copyrightEditor_.setBounds(r3);

            commentsLabel_.setBounds(rightCol.removeFromTop(18));
            commentsEditor_.setBounds(rightCol);
        }
        else
        {
            auto r1 = area.removeFromTop(rowH);
            titleLabel_.setBounds(r1.removeFromLeft(labelW));
            titleEditor_.setBounds(r1);
            area.removeFromTop(6);

            auto r2 = area.removeFromTop(rowH);
            authorLabel_.setBounds(r2.removeFromLeft(labelW));
            authorEditor_.setBounds(r2);
            area.removeFromTop(6);

            auto r3 = area.removeFromTop(rowH);
            copyrightLabel_.setBounds(r3.removeFromLeft(labelW));
            copyrightEditor_.setBounds(r3);
            area.removeFromTop(6);

            commentsLabel_.setBounds(area.removeFromLeft(labelW).removeFromTop(rowH));
            commentsEditor_.setBounds(area);
        }
    }
}

void TrackPropertiesComponent::setStretchZoom(double newZoom, double anchorNorm)
{
    const double clampedZoom = std::clamp(newZoom, 1.0, 16.0);
    const double oldVis = 1.0 / stretchZoom_;
    const double newVis = 1.0 / clampedZoom;

    if (anchorNorm < 0.0)
        anchorNorm = stretchScrollNorm_ + oldVis * 0.5;

    const double fracInView = (oldVis > 1e-6) ? std::clamp((anchorNorm - stretchScrollNorm_) / oldVis, 0.0, 1.0) : 0.5;
    stretchZoom_ = clampedZoom;
    stretchScrollNorm_ = std::clamp(anchorNorm - fracInView * newVis, 0.0, std::max(0.0, 1.0 - newVis));
    stretchZoomLabel_.setText(juce::String(stretchZoom_, 1) + "x", juce::dontSendNotification);
    repaint();
}

double TrackPropertiesComponent::stretchNormToX(double norm) const
{
    const double x0 = static_cast<double>(stretchWaveformRect_.getX() + 4);
    const double w = static_cast<double>(std::max(1, stretchWaveformRect_.getWidth() - 8));
    const double vis = 1.0 / std::max(1.0, stretchZoom_);
    return x0 + ((norm - stretchScrollNorm_) / vis) * w;
}

double TrackPropertiesComponent::stretchXToNorm(int x) const
{
    const double x0 = static_cast<double>(stretchWaveformRect_.getX() + 4);
    const double w = static_cast<double>(std::max(1, stretchWaveformRect_.getWidth() - 8));
    const double vis = 1.0 / std::max(1.0, stretchZoom_);
    const double frac = (static_cast<double>(x) - x0) / w;
    return std::clamp(stretchScrollNorm_ + frac * vis, 0.001, 0.999);
}

juce::Rectangle<int> TrackPropertiesComponent::stretchScrollThumbRect() const
{
    if (stretchScrollRect_.isEmpty())
        return {};
    const double vis = 1.0 / std::max(1.0, stretchZoom_);
    const int w = stretchScrollRect_.getWidth();
    const int thumbW = std::max(24, juce::roundToInt(vis * static_cast<double>(w)));
    const int maxX = std::max(0, w - thumbW);
    const double maxScroll = std::max(1e-6, 1.0 - vis);
    const int thumbX = (stretchZoom_ <= 1.001)
                           ? 0
                           : std::clamp(juce::roundToInt((stretchScrollNorm_ / maxScroll) * static_cast<double>(maxX)), 0, maxX);
    return {stretchScrollRect_.getX() + thumbX, stretchScrollRect_.getY() + 1, thumbW, std::max(4, stretchScrollRect_.getHeight() - 2)};
}

int TrackPropertiesComponent::hitTestMarker(juce::Point<int> pt) const
{
    if (activeTab_ != ActiveTab::Stretch || !stretchWaveformRect_.contains(pt))
        return -1;
    const Sample* s = song_.findSample(sampleId_);
    if (s == nullptr)
        return -1;

    int bestIdx = -1;
    int bestDist = 8;
    for (std::size_t i = 0; i < s->stretchMarkers.size(); ++i)
    {
        const int mx = juce::roundToInt(stretchNormToX(s->stretchMarkers[i]));
        if (mx < stretchWaveformRect_.getX() || mx > stretchWaveformRect_.getRight())
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

void TrackPropertiesComponent::mouseMove(const juce::MouseEvent& e)
{
    if (activeTab_ != ActiveTab::Stretch)
        return;
    const int idx = hitTestMarker(e.getPosition());
    if (idx != hoveredMarkerIdx_)
    {
        hoveredMarkerIdx_ = idx;
        setMouseCursor(idx >= 0 ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void TrackPropertiesComponent::mouseDown(const juce::MouseEvent& e)
{
    if (activeTab_ != ActiveTab::Stretch)
        return;

    if (stretchScrollRect_.expanded(0, 2).contains(e.getPosition()) && stretchZoom_ > 1.001)
    {
        draggingStretchScroll_ = true;
        scrollDragStartX_ = e.x;
        scrollDragStartNorm_ = stretchScrollNorm_;
        return;
    }

    if (!stretchWaveformRect_.contains(e.getPosition()))
        return;

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
        return;
    }

    draggingMarkerIdx_ = idx;
    pushedUndoForMarkerDrag_ = false;
}

void TrackPropertiesComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (activeTab_ != ActiveTab::Stretch)
        return;

    if (draggingStretchScroll_)
    {
        const double vis = 1.0 / std::max(1.0, stretchZoom_);
        const int w = std::max(1, stretchScrollRect_.getWidth());
        const double deltaNorm = static_cast<double>(e.x - scrollDragStartX_) / static_cast<double>(w);
        stretchScrollNorm_ = std::clamp(scrollDragStartNorm_ + deltaNorm, 0.0, std::max(0.0, 1.0 - vis));
        repaint();
        return;
    }

    if (draggingMarkerIdx_ < 0)
        return;

    Sample* s = song_.findSample(sampleId_);
    if (s == nullptr || draggingMarkerIdx_ >= static_cast<int>(s->stretchMarkers.size()))
        return;

    if (!pushedUndoForMarkerDrag_)
    {
        if (onPushUndo)
            onPushUndo();
        pushedUndoForMarkerDrag_ = true;
    }

    const double norm = std::clamp(stretchXToNorm(e.getPosition().x), 0.005, 0.995);
    s->stretchMarkers[static_cast<std::size_t>(draggingMarkerIdx_)] = norm;
    repaint();
}

void TrackPropertiesComponent::mouseUp(const juce::MouseEvent&)
{
    draggingStretchScroll_ = false;
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

void TrackPropertiesComponent::mouseDoubleClick(const juce::MouseEvent& e)
{
    if (activeTab_ != ActiveTab::Stretch || !stretchWaveformRect_.contains(e.getPosition()))
        return;

    Sample* s = song_.findSample(sampleId_);
    if (s == nullptr)
        return;

    if (onPushUndo)
        onPushUndo();
    const double norm = std::clamp(stretchXToNorm(e.getPosition().x), 0.005, 0.995);
    s->stretchMarkers.push_back(norm);
    std::sort(s->stretchMarkers.begin(), s->stretchMarkers.end());
    if (onChanged_)
        onChanged_();
    repaint();
}

void TrackPropertiesComponent::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (activeTab_ != ActiveTab::Stretch || !stretchWaveformRect_.contains(e.getPosition()))
    {
        juce::Component::mouseWheelMove(e, wheel);
        return;
    }

    if (e.mods.isShiftDown() || (std::abs(wheel.deltaX) > std::abs(wheel.deltaY) && stretchZoom_ > 1.001))
    {
        const double vis = 1.0 / std::max(1.0, stretchZoom_);
        const double delta = (std::abs(wheel.deltaX) > 1e-4f ? -wheel.deltaX : -wheel.deltaY) * vis * 0.45;
        stretchScrollNorm_ = std::clamp(stretchScrollNorm_ + delta, 0.0, std::max(0.0, 1.0 - vis));
        repaint();
        return;
    }

    if (std::abs(wheel.deltaY) > 1e-4f)
    {
        const double factor = std::pow(1.28, static_cast<double>(wheel.deltaY) * 3.0);
        setStretchZoom(stretchZoom_ * factor, stretchXToNorm(e.getPosition().x));
    }
}

void TrackPropertiesComponent::mouseMagnify(const juce::MouseEvent& e, float scaleFactor)
{
    if (activeTab_ == ActiveTab::Stretch && stretchWaveformRect_.contains(e.getPosition()) && scaleFactor > 0.01f)
        setStretchZoom(stretchZoom_ * static_cast<double>(scaleFactor), stretchXToNorm(e.getPosition().x));
}

void TrackPropertiesComponent::paint(juce::Graphics& g)
{
    g.fillAll(Palette::surface);

    // Subtle divider below header row
    g.setColour(Palette::gridLine);
    g.drawHorizontalLine(39, 0.0f, static_cast<float>(getWidth()));

    if (activeTab_ != ActiveTab::Stretch || stretchWaveformRect_.isEmpty())
        return;

    g.setColour(Palette::background);
    g.fillRoundedRectangle(stretchWaveformRect_.toFloat(), 4.0f);
    g.setColour(Palette::gridBeatLine);
    g.drawRoundedRectangle(stretchWaveformRect_.toFloat(), 4.0f, 1.0f);

    const Sample* s = song_.findSample(sampleId_);
    auto raw = stretchCache_.getRawAudio(sampleId_);
    const int x0 = stretchWaveformRect_.getX() + 4;
    const int w = std::max(1, stretchWaveformRect_.getWidth() - 8);
    const int y0 = stretchWaveformRect_.getY() + 18;
    const int h = std::max(10, stretchWaveformRect_.getHeight() - 24);
    const float cy = static_cast<float>(y0) + static_cast<float>(h) * 0.5f;

    const double viewStart = stretchScrollNorm_;
    const double viewSpan = 1.0 / std::max(1.0, stretchZoom_);
    const double viewEnd = std::min(1.0, viewStart + viewSpan);

    g.setColour(juce::Colour(0xff1c2128));
    g.fillRect(stretchWaveformRect_.getX() + 1, stretchWaveformRect_.getY() + 1, stretchWaveformRect_.getWidth() - 2, 16);

    g.saveState();
    g.reduceClipRegion(stretchWaveformRect_.reduced(2, 1));

    const int beats = (s != nullptr) ? std::max(1, static_cast<int>(std::round(s->loopCycleBeats()))) : 4;
    const int subdivsPerBeat = (stretchZoom_ >= 4.0) ? 4 : 2;
    const int totalSubdivs = beats * subdivsPerBeat;
    for (int b = 0; b <= totalSubdivs; ++b)
    {
        const double norm = static_cast<double>(b) / static_cast<double>(totalSubdivs);
        if (norm < viewStart - 0.02 || norm > viewEnd + 0.02)
            continue;
        const int bx = juce::roundToInt(stretchNormToX(norm));
        const bool isBeat = (b % subdivsPerBeat == 0);
        g.setColour(isBeat ? Palette::gridBeatLine.withAlpha(0.75f) : Palette::gridLine.withAlpha(0.5f));
        g.drawVerticalLine(bx, static_cast<float>(y0), static_cast<float>(stretchWaveformRect_.getBottom() - 2));
        if (isBeat && b < totalSubdivs)
        {
            g.setColour(Palette::textDim);
            g.setFont(Typography::bold(9.5f));
            g.drawText(juce::String(b / subdivsPerBeat + 1) + ".1", bx + 3, stretchWaveformRect_.getY() + 1, 30, 14,
                       juce::Justification::centredLeft);
        }
    }

    if (raw != nullptr && raw->numFrames() > 0)
    {
        const float* audioL = raw->left();
        const float* audioR = raw->right();
        const std::int64_t totalFrames = raw->numFrames();
        const float maxAmp = static_cast<float>(h) * 0.46f;

        g.setColour(Palette::accent.withAlpha(0.25f));
        g.drawHorizontalLine(static_cast<int>(cy), static_cast<float>(x0), static_cast<float>(x0 + w));

        g.setColour(Palette::accent.brighter(0.2f));
        for (int px = 0; px < w; ++px)
        {
            const double n0 = viewStart + (static_cast<double>(px) / static_cast<double>(w)) * viewSpan;
            const double n1 = viewStart + (static_cast<double>(px + 1) / static_cast<double>(w)) * viewSpan;
            const std::int64_t f0 = std::clamp<std::int64_t>(static_cast<std::int64_t>(n0 * static_cast<double>(totalFrames)), 0, totalFrames);
            const std::int64_t f1 = std::clamp<std::int64_t>(static_cast<std::int64_t>(n1 * static_cast<double>(totalFrames)), f0, totalFrames);

            float minV = 0.0f;
            float maxV = 0.0f;
            if (f1 > f0)
            {
                const std::int64_t step = std::max<std::int64_t>(1, (f1 - f0) / 16);
                for (std::int64_t f = f0; f < f1; f += step)
                {
                    const float v = 0.5f * (audioL[f] + audioR[f]);
                    minV = std::min(minV, v);
                    maxV = std::max(maxV, v);
                }
            }
            const float y1 = cy - maxV * maxAmp;
            const float y2 = cy - minV * maxAmp;
            g.drawVerticalLine(x0 + px, std::min(y1, y2), std::max(y1, y2) + 1.0f);
        }
    }
    else
    {
        g.setColour(Palette::textDim);
        g.setFont(Typography::regular(12.0f));
        g.drawText("No audio waveform available (double-click to add markers, right-click to delete)",
                   stretchWaveformRect_, juce::Justification::centred);
    }

    if (s != nullptr)
    {
        for (std::size_t i = 0; i < s->stretchMarkers.size(); ++i)
        {
            const double norm = s->stretchMarkers[i];
            if (norm < viewStart - 0.02 || norm > viewEnd + 0.02)
                continue;
            const int mx = juce::roundToInt(stretchNormToX(norm));
            const bool isHovered = (static_cast<int>(i) == hoveredMarkerIdx_ || static_cast<int>(i) == draggingMarkerIdx_);
            const juce::Colour mCol = isHovered ? juce::Colours::white : juce::Colour(0xfff59e0b);

            g.setColour(mCol.withAlpha(0.88f));
            g.drawVerticalLine(mx, static_cast<float>(stretchWaveformRect_.getY() + 14),
                               static_cast<float>(stretchWaveformRect_.getBottom() - 2));

            juce::Path flag;
            flag.startNewSubPath(static_cast<float>(mx - 5), static_cast<float>(stretchWaveformRect_.getY() + 14));
            flag.lineTo(static_cast<float>(mx + 5), static_cast<float>(stretchWaveformRect_.getY() + 14));
            flag.lineTo(static_cast<float>(mx), static_cast<float>(stretchWaveformRect_.getY() + 22));
            flag.closeSubPath();
            g.fillPath(flag);
        }
    }
    g.restoreState();

    // Horizontal zoom scrollbar below waveform
    if (!stretchScrollRect_.isEmpty())
    {
        g.setColour(Palette::background);
        g.fillRoundedRectangle(stretchScrollRect_.toFloat(), 3.0f);
        const auto thumb = stretchScrollThumbRect();
        g.setColour(stretchZoom_ > 1.001 ? Palette::accent.withAlpha(0.55f) : Palette::gridBeatLine);
        g.fillRoundedRectangle(thumb.toFloat(), 3.0f);
    }
}

void TrackPropertiesComponent::showDialog(juce::Component* parent, Song& song, StretchCache& stretchCache,
                                          PatternId patternId, int trackIndex, PropertiesChangedCallback onChanged)
{
    juce::ignoreUnused(parent);
    auto* comp = new TrackPropertiesComponent(song, stretchCache, patternId, trackIndex, std::move(onChanged));

    juce::DialogWindow::LaunchOptions opt;
    const std::string tName = song.patternTrackName(patternId, trackIndex);
    opt.dialogTitle = "Track " + juce::String(trackIndex + 1) + " Properties" +
                      (!tName.empty() ? (" - " + juce::String(tName)) : "");
    opt.content.setOwned(comp);
    opt.dialogBackgroundColour = Palette::surface;
    opt.escapeKeyTriggersCloseButton = true;
    opt.useNativeTitleBar = true;
    opt.resizable = true;
    opt.launchAsync();
}

} // namespace trami
