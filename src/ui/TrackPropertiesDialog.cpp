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
    setSize(560, 400);

    sampleId_ = song_.patternTrackSample(patternId_, trackIndex_);

    generalTabBtn_.onClick = [this]() { switchTab(ActiveTab::General); };
    stretchTabBtn_.onClick = [this]() { switchTab(ActiveTab::Stretch); };
    summaryTabBtn_.onClick = [this]() { switchTab(ActiveTab::Summary); };
    addAndMakeVisible(generalTabBtn_);
    addAndMakeVisible(stretchTabBtn_);
    addAndMakeVisible(summaryTabBtn_);

    // --- General Tab ---
    for (auto* lbl : {&trackTypeLabel_, &beatsLabel_, &rootNoteLabel_, &origBpmLabel_, &fxSendLabel_})
    {
        lbl->setFont(Typography::bold(12.5f));
        lbl->setColour(juce::Label::textColourId, Palette::textDim);
        addAndMakeVisible(lbl);
    }

    trackTypeCombo_.addItem("Loop (Time-Stretched to Tempo)", 1);
    trackTypeCombo_.addItem("One-Shot (Natural Speed / Unstretched)", 2);
    trackTypeCombo_.addItem("Disk-Based (Stream Long Audio)", 3);
    trackTypeCombo_.onChange = [this]() { applyGeneralChanges(); };
    addAndMakeVisible(trackTypeCombo_);

    beatsCombo_.addItem("1 Beat (1/4 Bar)", 1);
    beatsCombo_.addItem("2 Beats (1/2 Bar)", 2);
    beatsCombo_.addItem("4 Beats (1 Bar)", 4);
    beatsCombo_.addItem("8 Beats (2 Bars)", 8);
    beatsCombo_.addItem("16 Beats (4 Bars)", 16);
    beatsCombo_.addItem("32 Beats (8 Bars)", 32);
    beatsCombo_.addItem("64 Beats (16 Bars)", 64);
    beatsCombo_.onChange = [this]() { applyGeneralChanges(); };
    addAndMakeVisible(beatsCombo_);

    rootNoteCombo_.addItem("Don't Transpose (Drums / Unpitched)", 100);
    static constexpr const char* kNotes[12] = {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };
    for (int i = 0; i < 12; ++i)
        rootNoteCombo_.addItem(kNotes[i], i + 1);
    rootNoteCombo_.onChange = [this]() { applyGeneralChanges(); };
    addAndMakeVisible(rootNoteCombo_);

    origBpmEditor_.setColour(juce::TextEditor::backgroundColourId, Palette::background);
    origBpmEditor_.setColour(juce::TextEditor::textColourId, Palette::text);
    origBpmEditor_.setColour(juce::TextEditor::outlineColourId, Palette::gridBeatLine);
    origBpmEditor_.onReturnKey = [this]() { applyGeneralChanges(); };
    origBpmEditor_.onFocusLost = [this]() { applyGeneralChanges(); };
    addAndMakeVisible(origBpmEditor_);

    fxSendSlider_.setRange(0.0, 1.0, 0.01);
    fxSendSlider_.onValueChange = [this]() {
        song_.setTrackFxSend(patternId_, trackIndex_, static_cast<float>(fxSendSlider_.getValue()));
        if (onChanged_)
            onChanged_();
    };
    addAndMakeVisible(fxSendSlider_);

    // --- Stretch Tab ---
    autoDetectBtn_.setTooltip("Automatically detect transient onsets in the audio waveform");
    autoDetectBtn_.onClick = [this]() {
        if (Sample* s = song_.findSample(sampleId_))
        {
            if (auto raw = stretchCache_.getRawAudio(sampleId_))
            {
                s->stretchMarkers = Sample::detectTransients(raw->left(), raw->right(), raw->numFrames(), 44100.0, 32);
                if (onChanged_)
                    onChanged_();
                repaint();
            }
        }
    };
    addAndMakeVisible(autoDetectBtn_);

    beatGridBtn_.setTooltip("Place stretch markers evenly at each 1/8th beat subdivision");
    beatGridBtn_.onClick = [this]() {
        if (Sample* s = song_.findSample(sampleId_))
        {
            s->stretchMarkers.clear();
            const int divs = std::clamp(static_cast<int>( std::round(s->loopCycleBeats() * 2.0) ), 2, 64);
            for (int i = 1; i < divs; ++i)
                s->stretchMarkers.push_back(static_cast<double>(i) / static_cast<double>(divs));
            if (onChanged_)
                onChanged_();
            repaint();
        }
    };
    addAndMakeVisible(beatGridBtn_);

    clearMarkersBtn_.setTooltip("Remove all stretch markers");
    clearMarkersBtn_.onClick = [this]() {
        if (Sample* s = song_.findSample(sampleId_))
        {
            s->stretchMarkers.clear();
            if (onChanged_)
                onChanged_();
            repaint();
        }
    };
    addAndMakeVisible(clearMarkersBtn_);

    // --- Summary Tab ---
    fileInfoLabel_.setFont(Typography::regular(12.0f));
    fileInfoLabel_.setColour(juce::Label::textColourId, Palette::text);
    addAndMakeVisible(fileInfoLabel_);

    for (auto* lbl : {&titleLabel_, &authorLabel_, &copyrightLabel_, &commentsLabel_})
    {
        lbl->setFont(Typography::bold(12.0f));
        lbl->setColour(juce::Label::textColourId, Palette::textDim);
        addAndMakeVisible(lbl);
    }

    for (auto* ed : {&titleEditor_, &authorEditor_, &copyrightEditor_, &commentsEditor_})
    {
        ed->setColour(juce::TextEditor::backgroundColourId, Palette::background);
        ed->setColour(juce::TextEditor::textColourId, Palette::text);
        ed->setColour(juce::TextEditor::outlineColourId, Palette::gridBeatLine);
        ed->onTextChange = [this]() { applySummaryChanges(); };
        addAndMakeVisible(ed);
    }
    commentsEditor_.setMultiLine(true);
    commentsEditor_.setReturnKeyStartsNewLine(true);

    loadFromModel();
    switchTab(ActiveTab::General);
}

void TrackPropertiesComponent::loadFromModel()
{
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
        fxSendSlider_.setValue(pat->tracks[static_cast<std::size_t>(trackIndex_)].fxSend, juce::dontSendNotification);
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

        // If stretchMarkers is empty and we have audio, populate initial transients automatically
        if (s->stretchMarkers.empty() && raw != nullptr && raw->numFrames() > 0)
        {
            if (Sample* mutS = song_.findSample(sampleId_))
                mutS->stretchMarkers = Sample::detectTransients(raw->left(), raw->right(), raw->numFrames(), 44100.0, 24);
        }
    }
    else
    {
        fileInfoLabel_.setText("No sample assigned to this track.", juce::dontSendNotification);
    }
}

void TrackPropertiesComponent::applyGeneralChanges()
{
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
             &autoDetectBtn_, &beatGridBtn_, &clearMarkersBtn_})
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
    auto area = getLocalBounds().reduced(14);
    auto tabBar = area.removeFromTop(30);
    generalTabBtn_.setBounds(tabBar.removeFromLeft(110));
    tabBar.removeFromLeft(6);
    stretchTabBtn_.setBounds(tabBar.removeFromLeft(110));
    tabBar.removeFromLeft(6);
    summaryTabBtn_.setBounds(tabBar.removeFromLeft(110));

    area.removeFromTop(14);

    if (activeTab_ == ActiveTab::General)
    {
        constexpr int labelW = 180;
        constexpr int rowH = 30;
        constexpr int gap = 12;

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
        origBpmEditor_.setBounds(row4.removeFromLeft(140));
        area.removeFromTop(gap);

        auto row5 = area.removeFromTop(rowH);
        fxSendLabel_.setBounds(row5.removeFromLeft(labelW));
        fxSendSlider_.setBounds(row5);
    }
    else if (activeTab_ == ActiveTab::Stretch)
    {
        auto btnRow = area.removeFromBottom(30);
        autoDetectBtn_.setBounds(btnRow.removeFromLeft(175));
        btnRow.removeFromLeft(8);
        beatGridBtn_.setBounds(btnRow.removeFromLeft(130));
        btnRow.removeFromLeft(8);
        clearMarkersBtn_.setBounds(btnRow.removeFromLeft(120));

        area.removeFromBottom(10);
        stretchWaveformRect_ = area;
    }
    else if (activeTab_ == ActiveTab::Summary)
    {
        fileInfoLabel_.setBounds(area.removeFromTop(24));
        area.removeFromTop(10);

        constexpr int labelW = 150;
        auto r1 = area.removeFromTop(26);
        titleLabel_.setBounds(r1.removeFromLeft(labelW));
        titleEditor_.setBounds(r1);
        area.removeFromTop(8);

        auto r2 = area.removeFromTop(26);
        authorLabel_.setBounds(r2.removeFromLeft(labelW));
        authorEditor_.setBounds(r2);
        area.removeFromTop(8);

        auto r3 = area.removeFromTop(26);
        copyrightLabel_.setBounds(r3.removeFromLeft(labelW));
        copyrightEditor_.setBounds(r3);
        area.removeFromTop(8);

        commentsLabel_.setBounds(area.removeFromLeft(labelW).removeFromTop(26));
        commentsEditor_.setBounds(area);
    }
}

int TrackPropertiesComponent::hitTestMarker(juce::Point<int> pt) const
{
    if (activeTab_ != ActiveTab::Stretch || !stretchWaveformRect_.contains(pt))
        return -1;
    const Sample* s = song_.findSample(sampleId_);
    if (s == nullptr)
        return -1;

    const int x0 = stretchWaveformRect_.getX() + 4;
    const int w = std::max(1, stretchWaveformRect_.getWidth() - 8);
    int bestIdx = -1;
    int bestDist = 8;
    for (std::size_t i = 0; i < s->stretchMarkers.size(); ++i)
    {
        const int mx = x0 + juce::roundToInt(s->stretchMarkers[i] * static_cast<double>(w));
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
    if (activeTab_ != ActiveTab::Stretch || !stretchWaveformRect_.contains(e.getPosition()))
        return;

    const int idx = hitTestMarker(e.getPosition());
    if (e.mods.isRightButtonDown())
    {
        if (idx >= 0)
        {
            if (Sample* s = song_.findSample(sampleId_))
            {
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
}

void TrackPropertiesComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (activeTab_ != ActiveTab::Stretch || draggingMarkerIdx_ < 0)
        return;

    Sample* s = song_.findSample(sampleId_);
    if (s == nullptr || draggingMarkerIdx_ >= static_cast<int>(s->stretchMarkers.size()))
        return;

    const int x0 = stretchWaveformRect_.getX() + 4;
    const int w = std::max(1, stretchWaveformRect_.getWidth() - 8);
    const double norm = std::clamp(static_cast<double>(e.getPosition().x - x0) / static_cast<double>(w), 0.005, 0.995);
    s->stretchMarkers[static_cast<std::size_t>(draggingMarkerIdx_)] = norm;
    repaint();
}

void TrackPropertiesComponent::mouseUp(const juce::MouseEvent&)
{
    if (draggingMarkerIdx_ >= 0)
    {
        if (Sample* s = song_.findSample(sampleId_))
            std::sort(s->stretchMarkers.begin(), s->stretchMarkers.end());
        draggingMarkerIdx_ = -1;
        if (onChanged_)
            onChanged_();
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

    const int x0 = stretchWaveformRect_.getX() + 4;
    const int w = std::max(1, stretchWaveformRect_.getWidth() - 8);
    const double norm = std::clamp(static_cast<double>(e.getPosition().x - x0) / static_cast<double>(w), 0.005, 0.995);
    s->stretchMarkers.push_back(norm);
    std::sort(s->stretchMarkers.begin(), s->stretchMarkers.end());
    if (onChanged_)
        onChanged_();
    repaint();
}

void TrackPropertiesComponent::paint(juce::Graphics& g)
{
    g.fillAll(Palette::surface);

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

    // Ruler header inside waveform box
    g.setColour(juce::Colour(0xff1c2128));
    g.fillRect(stretchWaveformRect_.getX() + 1, stretchWaveformRect_.getY() + 1, stretchWaveformRect_.getWidth() - 2, 16);

    // Beat grid subdivisions
    const int beats = (s != nullptr) ? std::max(1, static_cast<int>(std::round(s->loopCycleBeats()))) : 4;
    for (int b = 0; b <= beats * 2; ++b)
    {
        const int bx = x0 + (b * w) / (beats * 2);
        const bool isBeat = (b % 2 == 0);
        g.setColour(isBeat ? Palette::gridBeatLine.withAlpha(0.75f) : Palette::gridLine.withAlpha(0.5f));
        g.drawVerticalLine(bx, static_cast<float>(y0), static_cast<float>(stretchWaveformRect_.getBottom() - 2));
        if (isBeat && b < beats * 2)
        {
            g.setColour(Palette::textDim);
            g.setFont(Typography::bold(9.5f));
            g.drawText(juce::String(b / 2 + 1) + ".1", bx + 3, stretchWaveformRect_.getY() + 1, 30, 14,
                       juce::Justification::centredLeft);
        }
    }

    // Waveform
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
            const std::int64_t f0 = (static_cast<std::int64_t>(px) * totalFrames) / w;
            const std::int64_t f1 = std::min<std::int64_t>(totalFrames, (static_cast<std::int64_t>(px + 1) * totalFrames) / w);
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

    // Interactive transient / stretch markers
    if (s != nullptr)
    {
        for (std::size_t i = 0; i < s->stretchMarkers.size(); ++i)
        {
            const int mx = x0 + juce::roundToInt(s->stretchMarkers[i] * static_cast<double>(w));
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
