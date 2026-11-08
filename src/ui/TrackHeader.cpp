#include "ui/TrackHeader.h"

#include "ui/PatternGrid.h"
#include "ui/TramiLookAndFeel.h"

#include <algorithm>

namespace trami
{

TrackHeaderList::TrackHeaderList(Song& song, PatternId patternId, MixChangedCallback onMixChanged)
    : song_(song), patternId_(patternId), onMixChanged_(std::move(onMixChanged))
{
    addTrackButton_ = std::make_unique<juce::TextButton>("+ Add Track");
    addTrackButton_->setColour(juce::TextButton::buttonColourId, Palette::surface.brighter(0.08f));
    addTrackButton_->setColour(juce::TextButton::textColourOffId, Palette::accent);
    addTrackButton_->onClick = [this]() {
        if (onAddTrack_)
            onAddTrack_();
    };
    addAndMakeVisible(addTrackButton_.get());

    updateFromModel();
}

void TrackHeaderList::setPattern(PatternId patternId)
{
    patternId_ = patternId;
    updateFromModel();
}

int TrackHeaderList::pointToTrack(juce::Point<int> pt) const
{
    if (pt.y < PatternGrid::kRulerHeight)
        return -1;
    const int t = (pt.y - PatternGrid::kRulerHeight) / trackHeight_;
    const auto* pat = song_.findPattern(patternId_);
    if (!pat)
        return -1;
    if (t < 0 || t >= static_cast<int>(pat->tracks.size()))
        return -1;
    return t;
}

void TrackHeaderList::updateFromModel()
{
    const auto* pat = song_.findPattern(patternId_);
    const int numTracks = pat != nullptr ? static_cast<int>(pat->tracks.size()) : 0;

    if (static_cast<int>(controls_.size()) != numTracks)
    {
        controls_.clear();
        controls_.resize(static_cast<std::size_t>(numTracks));

        for (int t = 0; t < numTracks; ++t)
        {
            auto& c = controls_[static_cast<std::size_t>(t)];

            c.nameLabel = std::make_unique<juce::Label>("lbl", juce::String(t + 1) + ": (empty)");
            c.nameLabel->setFont(Typography::bold(13.0f));
            c.nameLabel->setColour(juce::Label::textColourId, Palette::textDim.withAlpha(0.7f));
            c.nameLabel->setJustificationType(juce::Justification::centredLeft);
            c.nameLabel->setEditable(false, true, false);
            c.nameLabel->onTextChange = [this, t, lbl = c.nameLabel.get()] {
                if (lbl != nullptr)
                {
                    juce::String txt = lbl->getText().trim();
                    if (txt.startsWith(juce::String(t + 1) + ". "))
                        txt = txt.substring((juce::String(t + 1) + ". ").length()).trim();
                    song_.renamePatternTrack(patternId_, t, txt.toStdString());
                    if (onTrackRenamed_)
                        onTrackRenamed_(t, txt.toStdString());
                    updateFromModel();
                }
            };
            addAndMakeVisible(c.nameLabel.get());

            c.typeButton = std::make_unique<juce::TextButton>("type");
            addAndMakeVisible(c.typeButton.get());

            c.muteButton = std::make_unique<juce::TextButton>("M");
            c.muteButton->setClickingTogglesState(true);
            c.muteButton->setColour(juce::TextButton::buttonColourId, Palette::surface.brighter(0.05f));
            c.muteButton->setColour(juce::TextButton::textColourOffId, Palette::textDim);
            c.muteButton->setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffe53935));
            c.muteButton->setColour(juce::TextButton::textColourOnId, juce::Colours::white);
            c.muteButton->setTooltip("Mute Track " + juce::String(t + 1));
            c.muteButton->onClick = [this, t]() {
                if (auto* p = song_.findPattern(patternId_))
                {
                    if (t < static_cast<int>(p->tracks.size()))
                    {
                        p->tracks[static_cast<std::size_t>(t)].muted =
                            controls_[static_cast<std::size_t>(t)].muteButton->getToggleState();
                        if (onMixChanged_)
                            onMixChanged_(t);
                    }
                }
            };
            addAndMakeVisible(c.muteButton.get());

            c.soloButton = std::make_unique<juce::TextButton>("S");
            c.soloButton->setClickingTogglesState(true);
            c.soloButton->setColour(juce::TextButton::buttonColourId, Palette::surface.brighter(0.05f));
            c.soloButton->setColour(juce::TextButton::textColourOffId, Palette::textDim);
            c.soloButton->setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xfffdd835));
            c.soloButton->setColour(juce::TextButton::textColourOnId, juce::Colours::black);
            c.soloButton->setTooltip("Solo Track " + juce::String(t + 1));
            c.soloButton->onClick = [this, t]() {
                if (auto* p = song_.findPattern(patternId_))
                {
                    if (t < static_cast<int>(p->tracks.size()))
                    {
                        p->tracks[static_cast<std::size_t>(t)].solo =
                            controls_[static_cast<std::size_t>(t)].soloButton->getToggleState();
                        if (onMixChanged_)
                            onMixChanged_(t);
                    }
                }
            };
            addAndMakeVisible(c.soloButton.get());

            c.volLabel = std::make_unique<juce::Label>("vol", "Vol");
            c.volLabel->setFont(Typography::bold(10.0f));
            c.volLabel->setColour(juce::Label::textColourId, Palette::textDim);
            c.volLabel->setJustificationType(juce::Justification::centredRight);
            c.volLabel->setInterceptsMouseClicks(false, false);
            addAndMakeVisible(c.volLabel.get());

            c.volumeSlider = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal, juce::Slider::NoTextBox);
            c.volumeSlider->setRange(0.0, 2.0, 0.01);
            c.volumeSlider->setValue(1.0, juce::dontSendNotification);
            c.volumeSlider->setTooltip("Track " + juce::String(t + 1) + " Volume");
            c.volumeSlider->onValueChange = [this, t]() {
                if (auto* p = song_.findPattern(patternId_))
                {
                    if (t < static_cast<int>(p->tracks.size()))
                    {
                        p->tracks[static_cast<std::size_t>(t)].mix.volume =
                            static_cast<float>(controls_[static_cast<std::size_t>(t)].volumeSlider->getValue());
                        if (onMixChanged_)
                            onMixChanged_(t);
                    }
                }
            };
            addAndMakeVisible(c.volumeSlider.get());

            c.panLabel = std::make_unique<juce::Label>("pan", "Pan");
            c.panLabel->setFont(Typography::bold(10.0f));
            c.panLabel->setColour(juce::Label::textColourId, Palette::textDim);
            c.panLabel->setJustificationType(juce::Justification::centredRight);
            c.panLabel->setInterceptsMouseClicks(false, false);
            addAndMakeVisible(c.panLabel.get());

            c.panSlider = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal, juce::Slider::NoTextBox);
            c.panSlider->setRange(-1.0, 1.0, 0.01);
            c.panSlider->setValue(0.0, juce::dontSendNotification);
            c.panSlider->setTooltip("Track " + juce::String(t + 1) + " Pan");
            c.panSlider->onValueChange = [this, t]() {
                if (auto* p = song_.findPattern(patternId_))
                {
                    if (t < static_cast<int>(p->tracks.size()))
                    {
                        p->tracks[static_cast<std::size_t>(t)].mix.pan =
                            static_cast<float>(controls_[static_cast<std::size_t>(t)].panSlider->getValue());
                        if (onMixChanged_)
                            onMixChanged_(t);
                    }
                }
            };
            addAndMakeVisible(c.panSlider.get());
        }
        resized();
    }

    if (pat != nullptr)
    {
        for (int t = 0; t < numTracks; ++t)
        {
            const auto& track = pat->tracks[static_cast<std::size_t>(t)];
            auto& c = controls_[static_cast<std::size_t>(t)];

            c.volumeSlider->setValue(track.mix.volume, juce::dontSendNotification);
            c.panSlider->setValue(track.mix.pan, juce::dontSendNotification);
            c.muteButton->setToggleState(track.muted, juce::dontSendNotification);
            c.soloButton->setToggleState(track.solo, juce::dontSendNotification);

            const SampleKind kind = song_.patternTrackKind(patternId_, t);
            if (kind == SampleKind::Loop)
            {
                c.typeButton->setButtonText("LOOP");
                c.typeButton->setColour(juce::TextButton::buttonColourId, juce::Colour(0xfff59e0b).withAlpha(0.28f));
                c.typeButton->setColour(juce::TextButton::textColourOffId, juce::Colour(0xffffca28));
                c.typeButton->setTooltip("Track " + juce::String(t + 1) + ": Loop (Time-Stretched to tempo). Click to toggle to 1-Shot.");
            }
            else
            {
                c.typeButton->setButtonText("1-SHOT");
                c.typeButton->setColour(juce::TextButton::buttonColourId, Palette::surface.brighter(0.08f));
                c.typeButton->setColour(juce::TextButton::textColourOffId, Palette::textDim.brighter(0.2f));
                c.typeButton->setTooltip("Track " + juce::String(t + 1) + ": One-Shot (Natural pitch/duration). Click to toggle to Loop.");
            }
            c.typeButton->onClick = [this, t, kind]() {
                const SampleKind newKind = (kind == SampleKind::Loop) ? SampleKind::OneShot : SampleKind::Loop;
                if (onTrackTypeChanged_)
                    onTrackTypeChanged_(t, newKind);
            };

            SampleId sid = track.sampleId;
            if (!sid.isValid() && !track.clips.empty())
                sid = track.clips.front().sampleId;

            const std::string customName = song_.patternTrackName(patternId_, t);
            if (!customName.empty())
            {
                c.nameLabel->setText(juce::String(t + 1) + ". " + customName, juce::dontSendNotification);
                c.nameLabel->setColour(juce::Label::textColourId, Palette::text);
                c.nameLabel->setTooltip("Track " + juce::String(t + 1) + ": " + customName + " (Double-click to rename)");
            }
            else if (const Sample* s = song_.findSample(sid))
            {
                const juce::String fn = s->fileName.empty() ? juce::String(s->name) : juce::String(s->fileName);
                c.nameLabel->setText(juce::String(t + 1) + ". " + fn, juce::dontSendNotification);
                c.nameLabel->setColour(juce::Label::textColourId, Palette::text);
                c.nameLabel->setTooltip("Track " + juce::String(t + 1) + ": " + fn + " (Double-click to rename)");
            }
            else
            {
                c.nameLabel->setText(juce::String(t + 1) + ". (empty)", juce::dontSendNotification);
                c.nameLabel->setColour(juce::Label::textColourId, Palette::textDim.withAlpha(0.6f));
                c.nameLabel->setTooltip("Track " + juce::String(t + 1) + " (Double-click to rename)");
            }
        }
    }
    repaint();
}

void TrackHeaderList::setTrackHeight(int height)
{
    const int clamped = std::max(20, height);
    if (trackHeight_ != clamped)
    {
        trackHeight_ = clamped;
        resized();
        repaint();
    }
}

void TrackHeaderList::paint(juce::Graphics& g)
{
    g.fillAll(Palette::trackHeaderSurface);

    // Header top area (matching ruler)
    g.setColour(Palette::panelHeader);
    g.fillRect(0, 0, getWidth(), PatternGrid::kRulerHeight);
    g.setColour(Palette::gridLine);
    g.drawHorizontalLine(PatternGrid::kRulerHeight - 1, 0.0f, static_cast<float>(getWidth()));

    g.setColour(Palette::textDim);
    g.setFont(Typography::bold(12.0f));
    g.drawText("TRACKS", 10, 0, getWidth() - 20, PatternGrid::kRulerHeight, juce::Justification::centredLeft);

    const auto* pat = song_.findPattern(patternId_);
    const int numTracks = pat != nullptr ? static_cast<int>(pat->tracks.size()) : 0;

    for (int t = 0; t < numTracks; ++t)
    {
        const int y = PatternGrid::kRulerHeight + t * trackHeight_;

        if (t % 2 != 0)
        {
            g.setColour(Palette::trackHeaderSurface.brighter(0.04f));
            g.fillRect(0, y, getWidth(), trackHeight_);
        }

        // ACID-style track badge on left
        const auto badgeRect = juce::Rectangle<float>(6.0f, static_cast<float>(y + 3), 32.0f,
                                                      static_cast<float>(std::max(16, trackHeight_ - 6)));
        juce::Colour badgeCol = Palette::surface.brighter(0.08f);
        const SampleKind kind = song_.patternTrackKind(patternId_, t);
        bool hasSample = false;

        const auto& track = pat->tracks[static_cast<std::size_t>(t)];
        SampleId sid = track.sampleId;
        if (!sid.isValid() && !track.clips.empty())
            sid = track.clips.front().sampleId;

        if (const Sample* s = song_.findSample(sid))
        {
            hasSample = true;
            badgeCol = juce::Colour::fromHSV(s->hue(), 0.75f, 0.85f, 1.0f);
        }

        g.setColour(badgeCol);
        g.fillRoundedRectangle(badgeRect, 4.0f);
        g.setColour(kind == SampleKind::Loop ? juce::Colour(0xffffb300) : badgeCol.brighter(0.25f));
        g.drawRoundedRectangle(badgeRect, 4.0f, kind == SampleKind::Loop ? 1.5f : 1.0f);

        const float bcx = badgeRect.getCentreX();
        const float bcy = badgeRect.getCentreY();

        if (!hasSample)
        {
            g.setColour(Palette::textDim);
            g.setFont(Typography::bold(12.0f));
            g.drawText(juce::String(t + 1), badgeRect.toNearestInt(), juce::Justification::centred, false);
        }
        else if (kind == SampleKind::Loop)
        {
            // ACID loop swirl vector icon
            g.setColour(juce::Colours::white);
            juce::Path swirl;
            swirl.startNewSubPath(bcx - 6.0f, bcy + 3.0f);
            swirl.cubicTo(bcx - 7.0f, bcy - 5.0f, bcx - 1.0f, bcy - 6.0f, bcx + 2.0f, bcy - 3.5f);
            swirl.cubicTo(bcx + 5.5f, bcy - 1.0f, bcx + 5.5f, bcy + 4.5f, bcx + 1.0f, bcy + 5.5f);
            swirl.cubicTo(bcx - 3.5f, bcy + 6.0f, bcx - 4.5f, bcy + 1.0f, bcx - 1.0f, bcy - 0.2f);
            swirl.cubicTo(bcx + 1.5f, bcy - 1.0f, bcx + 2.5f, bcy + 1.8f, bcx + 0.8f, bcy + 2.2f);
            g.strokePath(swirl, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        else
        {
            // One-shot arrow icon
            g.setColour(juce::Colours::white);
            juce::Path arrow;
            arrow.startNewSubPath(bcx - 5.0f, bcy);
            arrow.lineTo(bcx + 4.0f, bcy);
            arrow.startNewSubPath(bcx + 1.0f, bcy - 4.0f);
            arrow.lineTo(bcx + 5.0f, bcy);
            arrow.lineTo(bcx + 1.0f, bcy + 4.0f);
            g.strokePath(arrow, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Horizontal separator
        g.setColour(Palette::gridLine.withAlpha(0.6f));
        g.drawHorizontalLine(y + trackHeight_, 0.0f, static_cast<float>(getWidth()));

        // Drag hover highlight
        if (dragHoverTrack_ == t)
        {
            g.setColour(Palette::accent.withAlpha(0.22f));
            g.fillRect(0, y, getWidth(), trackHeight_);
            g.setColour(Palette::accent);
            g.drawRect(1.0f, static_cast<float>(y + 1), static_cast<float>(getWidth() - 2), static_cast<float>(trackHeight_ - 2), 2.0f);
        }
    }

    if (dragHoverTrack_ >= numTracks && addTrackButton_ != nullptr)
    {
        g.setColour(Palette::accent.withAlpha(0.25f));
        g.fillRoundedRectangle(addTrackButton_->getBounds().toFloat().expanded(2.0f), 4.0f);
        g.setColour(Palette::accent);
        g.drawRoundedRectangle(addTrackButton_->getBounds().toFloat().expanded(2.0f), 4.0f, 2.0f);
    }

    // Right border dividing from PatternGrid
    g.setColour(Palette::gridLine);
    g.drawVerticalLine(getWidth() - 2, 0.0f, static_cast<float>(getHeight()));
    g.drawVerticalLine(getWidth() - 1, 0.0f, static_cast<float>(getHeight()));
}

void TrackHeaderList::resized()
{
    const int numTracks = static_cast<int>(controls_.size());

    for (int t = 0; t < numTracks; ++t)
    {
        const int y = PatternGrid::kRulerHeight + t * trackHeight_;
        auto& c = controls_[static_cast<std::size_t>(t)];

        if (trackHeight_ >= 34)
        {
            // 2-row layout:
            // Top: Name Label & Type Button
            c.nameLabel->setBounds(44, y + 2, getWidth() - 110, 18);
            if (c.typeButton != nullptr)
                c.typeButton->setBounds(getWidth() - 62, y + 2, 56, 18);

            // Bottom: M S Vol [slider] Pan [slider]
            c.muteButton->setBounds(44, y + 21, 20, 16);
            c.soloButton->setBounds(66, y + 21, 20, 16);

            const int remainW = getWidth() - 92;
            const int sliderW = std::max(20, (remainW - 54) / 2);
            c.volLabel->setBounds(88, y + 21, 22, 16);
            c.volumeSlider->setBounds(112, y + 21, sliderW, 16);
            c.panLabel->setBounds(114 + sliderW, y + 21, 24, 16);
            c.panSlider->setBounds(140 + sliderW, y + 21, sliderW, 16);
        }
        else
        {
            // 1-row compact layout
            c.nameLabel->setBounds(42, y + 2, getWidth() - 250, trackHeight_ - 4);
            if (c.typeButton != nullptr)
                c.typeButton->setBounds(getWidth() - 200, y + 2, 48, trackHeight_ - 4);
            c.muteButton->setBounds(getWidth() - 148, y + 2, 18, trackHeight_ - 4);
            c.soloButton->setBounds(getWidth() - 128, y + 2, 18, trackHeight_ - 4);
            c.volLabel->setBounds(getWidth() - 108, y + 2, 22, trackHeight_ - 4);
            c.volumeSlider->setBounds(getWidth() - 84, y + 2, 36, trackHeight_ - 4);
            c.panLabel->setBounds(getWidth() - 46, y + 2, 22, trackHeight_ - 4);
            c.panSlider->setBounds(getWidth() - 22, y + 2, 18, trackHeight_ - 4);
        }
    }

    if (addTrackButton_ != nullptr)
    {
        const int y = PatternGrid::kRulerHeight + numTracks * trackHeight_ + 6;
        addTrackButton_->setBounds(10, y, getWidth() - 20, 26);
    }
}

void TrackHeaderList::mouseDown(const juce::MouseEvent& e)
{
    const int t = pointToTrack(e.getPosition());
    const auto* pat = song_.findPattern(patternId_);
    if (!pat || t < 0 || t >= static_cast<int>(pat->tracks.size()))
        return;

    const auto& track = pat->tracks[static_cast<std::size_t>(t)];
    SampleId sid = track.sampleId;
    if (!sid.isValid() && !track.clips.empty())
        sid = track.clips.front().sampleId;

    const SampleKind kind = song_.patternTrackKind(patternId_, t);

    if (e.mods.isPopupMenu())
    {
        juce::PopupMenu m;
        m.addItem(1, "Rename Track...");
        m.addItem(2, "Track Type: Loop (time-stretched)", true, kind == SampleKind::Loop);
        m.addItem(3, "Track Type: One-Shot (natural pitch)", true, kind == SampleKind::OneShot);
        m.addSeparator();
        m.addItem(4, "Duplicate Track");
        m.addItem(5, "Delete Track", pat->tracks.size() > 1);

        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withTargetScreenArea(
                            juce::Rectangle<int>(e.getScreenX(), e.getScreenY(), 1, 1)),
                        [this, t](int result) {
                            if (result == 1 && t < static_cast<int>(controls_.size()))
                            {
                                if (controls_[static_cast<std::size_t>(t)].nameLabel != nullptr)
                                    controls_[static_cast<std::size_t>(t)].nameLabel->showEditor();
                            }
                            else if (result == 2 && onTrackTypeChanged_)
                                onTrackTypeChanged_(t, SampleKind::Loop);
                            else if (result == 3 && onTrackTypeChanged_)
                                onTrackTypeChanged_(t, SampleKind::OneShot);
                            else if (result == 4 && onDuplicateTrack_)
                                onDuplicateTrack_(t);
                            else if (result == 5 && onDeleteTrack_)
                                onDeleteTrack_(t);
                        });
        return;
    }

    const int y = PatternGrid::kRulerHeight + t * trackHeight_;
    const auto badgeRect = juce::Rectangle<int>(6, y + 3, 32, std::max(16, trackHeight_ - 6));
    if (badgeRect.contains(e.getPosition()))
    {
        const SampleKind newKind = (kind == SampleKind::Loop) ? SampleKind::OneShot : SampleKind::Loop;
        if (onTrackTypeChanged_)
            onTrackTypeChanged_(t, newKind);
        return;
    }

    if (onTrackSelected_)
        onTrackSelected_(t, sid);
}

void TrackHeaderList::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (onMouseWheel)
        onMouseWheel(e, wheel);
    else
        juce::Component::mouseWheelMove(e, wheel);
}

bool TrackHeaderList::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (const auto& f : files)
    {
        if (f.endsWithIgnoreCase(".wav") || f.endsWithIgnoreCase(".mp3"))
            return true;
    }
    return false;
}

void TrackHeaderList::fileDragEnter(const juce::StringArray& files, int /*x*/, int y)
{
    juce::ignoreUnused(files);
    dragHoverTrack_ = pointToTrack({10, y});
    repaint();
}

void TrackHeaderList::fileDragMove(const juce::StringArray& files, int /*x*/, int y)
{
    juce::ignoreUnused(files);
    const int t = pointToTrack({10, y});
    if (t != dragHoverTrack_)
    {
        dragHoverTrack_ = t;
        repaint();
    }
}

void TrackHeaderList::fileDragExit(const juce::StringArray& /*files*/)
{
    dragHoverTrack_ = -1;
    repaint();
}

void TrackHeaderList::filesDropped(const juce::StringArray& files, int /*x*/, int y)
{
    dragHoverTrack_ = -1;
    repaint();
    if (files.isEmpty())
        return;

    const int t = pointToTrack({10, y});
    if (onFileDropped_)
        onFileDropped_(files[0], t);
}

bool TrackHeaderList::isInterestedInDragSource(const SourceDetails& dragSourceDetails)
{
    if (dragSourceDetails.description.isString())
    {
        const juce::String s = dragSourceDetails.description.toString();
        return s.endsWithIgnoreCase(".wav") || s.endsWithIgnoreCase(".mp3");
    }
    return false;
}

void TrackHeaderList::itemDragEnter(const SourceDetails& dragSourceDetails)
{
    dragHoverTrack_ = pointToTrack(dragSourceDetails.localPosition);
    repaint();
}

void TrackHeaderList::itemDragMove(const SourceDetails& dragSourceDetails)
{
    const int t = pointToTrack(dragSourceDetails.localPosition);
    if (t != dragHoverTrack_)
    {
        dragHoverTrack_ = t;
        repaint();
    }
}

void TrackHeaderList::itemDragExit(const SourceDetails& /*dragSourceDetails*/)
{
    dragHoverTrack_ = -1;
    repaint();
}

void TrackHeaderList::itemDropped(const SourceDetails& dragSourceDetails)
{
    dragHoverTrack_ = -1;
    repaint();
    if (!dragSourceDetails.description.isString())
        return;

    const juce::String filePath = dragSourceDetails.description.toString();
    const int t = pointToTrack(dragSourceDetails.localPosition);
    if (onFileDropped_)
        onFileDropped_(filePath, t);
}

} // namespace trami
