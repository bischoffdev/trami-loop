#include "ui/TrackPropertiesDialog.h"

#include "ui/DAWTrackControls.h"

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
    setSize(760, 260);

    // Header: Track Selector + File info summary
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

    fileInfoLabel_.setFont(Typography::regular(11.5f));
    fileInfoLabel_.setColour(juce::Label::textColourId, Palette::textDim);
    fileInfoLabel_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(fileInfoLabel_);

    // --- General Controls ---
    for (auto* lbl : {&trackTypeLabel_, &beatsLabel_, &rootNoteLabel_, &origBpmLabel_, &fxSendLabel_, &recordInputLabel_})
    {
        lbl->setFont(Typography::bold(11.5f));
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

    {
        const juce::StringArray noNames;
        for (int ch = 0; ch < 16; ++ch)
            recordInputCombo_.addItem(DAWInputSources::sourceLabel(ch, false, noNames), DAWInputSources::kMonoBase + ch);
        for (int ch = 0; ch + 1 < 16; ch += 2)
            recordInputCombo_.addItem(DAWInputSources::sourceLabel(ch, true, noNames), DAWInputSources::kStereoBase + ch);
    }
    recordInputCombo_.setTooltip("Select the recording source for this track (each mono input and each stereo pair is a separate source). Changing it does not arm the track.");
    recordInputCombo_.onChange = [this]()
    {
        if (isLoading_ || !hasValidTrack())
            return;
        int ch = 0;
        bool stereo = true;
        if (!DAWInputSources::decode(recordInputCombo_.getSelectedId(), ch, stereo))
            return;
        if (onPushUndo)
            onPushUndo();
        song_.setPatternTrackInput(patternId_, trackIndex_, ch, stereo);
        if (onChanged_)
            onChanged_();
    };
    addAndMakeVisible(recordInputCombo_);

    // --- Summary Controls ---
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
}

bool TrackPropertiesComponent::hasValidTrack() const
{
    const auto* pat = song_.findPattern(patternId_);
    return pat != nullptr && trackIndex_ >= 0 && trackIndex_ < static_cast<int>(pat->tracks.size());
}

void TrackPropertiesComponent::setTrack(PatternId patternId, int trackIndex)
{
    patternId_ = patternId;
    trackIndex_ = trackIndex;
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
    repaint();
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
             &recordInputCombo_,
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
    {
        const auto& trk = pat->tracks[static_cast<std::size_t>(trackIndex_)];
        fxSendSlider_.setValue(trk.fxSend, juce::dontSendNotification);
        recordInputCombo_.setSelectedId((trk.inputStereo ? DAWInputSources::kStereoBase : DAWInputSources::kMonoBase) +
                                            std::clamp(trk.inputChannel, 0, 15),
                                        juce::dontSendNotification);
    }

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

        const int chLat = song_.channelLatencySamples(patternId_, trackIndex_);
        const int chComp = song_.channelCompensationSamples(patternId_, trackIndex_);
        const int projLat = song_.totalProjectLatencySamples(patternId_);
        juce::String info = "File: " + juce::String(s->fileName.empty() ? s->name : s->fileName) +
                            "  |  " + juce::String(s->durationSeconds, 2) + " s" +
                            "  |  " + juce::String(frames) + " f" +
                            "  |  " + juce::String(ch) + " ch" +
                            "  |  PDC: " + juce::String(chLat) + " smp (+" + juce::String(chComp) +
                            ", Proj " + juce::String(projLat) + ")" +
                            (s->missing ? "  [MISSING]" : "");
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

void TrackPropertiesComponent::paint(juce::Graphics& g)
{
    g.fillAll(Palette::surface);
    g.setColour(Palette::gridLine);
    g.drawHorizontalLine(36, 0.0f, static_cast<float>(getWidth()));
}

void TrackPropertiesComponent::resized()
{
    auto area = getLocalBounds().reduced(10, 6);
    auto topBar = area.removeFromTop(24);

    trackSelectLabel_.setBounds(topBar.removeFromLeft(44));
    trackSelectorCombo_.setBounds(topBar.removeFromLeft(std::clamp(topBar.getWidth() / 4, 130, 220)));
    topBar.removeFromLeft(14);
    fileInfoLabel_.setBounds(topBar);

    area.removeFromTop(10);

    const bool twoCol = (area.getWidth() >= 660);
    constexpr int rowH = 24;
    constexpr int gap = 5;

    if (twoCol)
    {
        const int colGap = 24;
        const int colW = (area.getWidth() - colGap) / 2;
        auto leftCol = area.removeFromLeft(colW);
        area.removeFromLeft(colGap);
        auto rightCol = area;

        constexpr int leftLabelW = 148;
        auto r1 = leftCol.removeFromTop(rowH);
        trackTypeLabel_.setBounds(r1.removeFromLeft(leftLabelW));
        trackTypeCombo_.setBounds(r1);
        leftCol.removeFromTop(gap);

        auto r2 = leftCol.removeFromTop(rowH);
        beatsLabel_.setBounds(r2.removeFromLeft(leftLabelW));
        beatsCombo_.setBounds(r2);
        leftCol.removeFromTop(gap);

        auto r3 = leftCol.removeFromTop(rowH);
        rootNoteLabel_.setBounds(r3.removeFromLeft(leftLabelW));
        rootNoteCombo_.setBounds(r3);
        leftCol.removeFromTop(gap);

        auto r4 = leftCol.removeFromTop(rowH);
        origBpmLabel_.setBounds(r4.removeFromLeft(leftLabelW));
        origBpmEditor_.setBounds(r4.removeFromLeft(110));
        leftCol.removeFromTop(gap);

        auto r5 = leftCol.removeFromTop(rowH);
        fxSendLabel_.setBounds(r5.removeFromLeft(leftLabelW));
        fxSendSlider_.setBounds(r5);
        leftCol.removeFromTop(gap);

        auto r6 = leftCol.removeFromTop(rowH);
        recordInputLabel_.setBounds(r6.removeFromLeft(leftLabelW));
        recordInputCombo_.setBounds(r6);

        constexpr int rightLabelW = 136;
        auto s1 = rightCol.removeFromTop(rowH);
        titleLabel_.setBounds(s1.removeFromLeft(rightLabelW));
        titleEditor_.setBounds(s1);
        rightCol.removeFromTop(gap);

        auto s2 = rightCol.removeFromTop(rowH);
        authorLabel_.setBounds(s2.removeFromLeft(rightLabelW));
        authorEditor_.setBounds(s2);
        rightCol.removeFromTop(gap);

        auto s3 = rightCol.removeFromTop(rowH);
        copyrightLabel_.setBounds(s3.removeFromLeft(rightLabelW));
        copyrightEditor_.setBounds(s3);
        rightCol.removeFromTop(gap);

        commentsLabel_.setBounds(rightCol.removeFromTop(18));
        commentsEditor_.setBounds(rightCol);
    }
    else
    {
        constexpr int labelW = 150;
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
        origBpmEditor_.setBounds(row4.removeFromLeft(120));
        area.removeFromTop(gap);

        auto row5 = area.removeFromTop(rowH);
        fxSendLabel_.setBounds(row5.removeFromLeft(labelW));
        fxSendSlider_.setBounds(row5);
        area.removeFromTop(gap);

        auto row6 = area.removeFromTop(rowH);
        recordInputLabel_.setBounds(row6.removeFromLeft(labelW));
        recordInputCombo_.setBounds(row6);
        area.removeFromTop(gap);

        auto s1 = area.removeFromTop(rowH);
        titleLabel_.setBounds(s1.removeFromLeft(labelW));
        titleEditor_.setBounds(s1);
        area.removeFromTop(gap);

        auto s2 = area.removeFromTop(rowH);
        authorLabel_.setBounds(s2.removeFromLeft(labelW));
        authorEditor_.setBounds(s2);
        area.removeFromTop(gap);

        auto s3 = area.removeFromTop(rowH);
        copyrightLabel_.setBounds(s3.removeFromLeft(labelW));
        copyrightEditor_.setBounds(s3);
        area.removeFromTop(gap);

        commentsLabel_.setBounds(area.removeFromLeft(labelW).removeFromTop(rowH));
        commentsEditor_.setBounds(area);
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
