#include "ui/TrackHeader.h"

#include "ui/PatternGrid.h"
#include "ui/TramiLookAndFeel.h"

#include <algorithm>

namespace trami
{

TrackHeaderList::TrackHeaderList(Song& song, PatternId patternId, MixChangedCallback onMixChanged)
    : song_(song), patternId_(patternId), onMixChanged_(std::move(onMixChanged))
{
    for (int t = 0; t < kTracksPerPattern; ++t)
    {
        auto& c = controls_[static_cast<std::size_t>(t)];

        c.nameLabel = std::make_unique<juce::Label>("lbl", juce::String(t + 1) + ": (empty)");
        c.nameLabel->setFont(Typography::bold(14.0f));
        c.nameLabel->setColour(juce::Label::textColourId, Palette::textDim.withAlpha(0.7f));
        c.nameLabel->setJustificationType(juce::Justification::centredLeft);
        c.nameLabel->setInterceptsMouseClicks(false, false);
        addAndMakeVisible(c.nameLabel.get());

        c.muteButton = std::make_unique<juce::TextButton>("M");
        c.muteButton->setClickingTogglesState(true);
        c.muteButton->setColour(juce::TextButton::buttonColourId, Palette::surface.brighter(0.05f));
        c.muteButton->setColour(juce::TextButton::textColourOffId, Palette::textDim);
        c.muteButton->setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffe53935));
        c.muteButton->setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        c.muteButton->setTooltip("Mute Track " + juce::String(t + 1));
        c.muteButton->onClick = [this, t]() {
            if (Pattern* p = song_.findPattern(patternId_))
            {
                p->tracks[static_cast<std::size_t>(t)].muted = controls_[static_cast<std::size_t>(t)].muteButton->getToggleState();
                if (onMixChanged_)
                    onMixChanged_(t);
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
            if (Pattern* p = song_.findPattern(patternId_))
            {
                p->tracks[static_cast<std::size_t>(t)].solo = controls_[static_cast<std::size_t>(t)].soloButton->getToggleState();
                if (onMixChanged_)
                    onMixChanged_(t);
            }
        };
        addAndMakeVisible(c.soloButton.get());

        c.volumeSlider = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal, juce::Slider::NoTextBox);
        c.volumeSlider->setRange(0.0, 2.0, 0.01);
        c.volumeSlider->setValue(1.0, juce::dontSendNotification);
        c.volumeSlider->setTooltip("Track " + juce::String(t + 1) + " Volume");
        c.volumeSlider->onValueChange = [this, t]() {
            if (Pattern* p = song_.findPattern(patternId_))
            {
                p->tracks[static_cast<std::size_t>(t)].mix.volume =
                    static_cast<float>(controls_[static_cast<std::size_t>(t)].volumeSlider->getValue());
                if (onMixChanged_)
                    onMixChanged_(t);
            }
        };
        addAndMakeVisible(c.volumeSlider.get());

        c.panSlider = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal, juce::Slider::NoTextBox);
        c.panSlider->setRange(-1.0, 1.0, 0.01);
        c.panSlider->setValue(0.0, juce::dontSendNotification);
        c.panSlider->setTooltip("Track " + juce::String(t + 1) + " Pan");
        c.panSlider->onValueChange = [this, t]() {
            if (Pattern* p = song_.findPattern(patternId_))
            {
                p->tracks[static_cast<std::size_t>(t)].mix.pan =
                    static_cast<float>(controls_[static_cast<std::size_t>(t)].panSlider->getValue());
                if (onMixChanged_)
                    onMixChanged_(t);
            }
        };
        addAndMakeVisible(c.panSlider.get());
    }

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
    if (t < 0 || t >= kTracksPerPattern)
        return -1;
    return t;
}

void TrackHeaderList::mouseDown(const juce::MouseEvent& e)
{
    const int t = pointToTrack(e.getPosition());
    if (t < 0)
        return;

    const Pattern* p = song_.findPattern(patternId_);
    SampleId sid;
    if (p != nullptr)
    {
        const auto& trk = p->tracks[static_cast<std::size_t>(t)];
        if (trk.sampleId.isValid())
            sid = trk.sampleId;
        else if (!trk.clips.empty())
            sid = trk.clips.front().sampleId;
    }

    if (onTrackSelected_)
        onTrackSelected_(t, sid);
}

void TrackHeaderList::updateFromModel()
{
    const Pattern* p = song_.findPattern(patternId_);
    if (p == nullptr)
        return;

    for (int t = 0; t < kTracksPerPattern; ++t)
    {
        const auto& track = p->tracks[static_cast<std::size_t>(t)];
        auto& c = controls_[static_cast<std::size_t>(t)];

        c.volumeSlider->setValue(track.mix.volume, juce::dontSendNotification);
        c.panSlider->setValue(track.mix.pan, juce::dontSendNotification);
        c.muteButton->setToggleState(track.muted, juce::dontSendNotification);
        c.soloButton->setToggleState(track.solo, juce::dontSendNotification);

        SampleId sid = track.sampleId;
        if (!sid.isValid() && !track.clips.empty())
            sid = track.clips.front().sampleId;

        if (const Sample* s = song_.findSample(sid))
        {
            const juce::String fn = s->fileName.empty() ? juce::String(s->name) : juce::String(s->fileName);
            c.nameLabel->setText(juce::String(t + 1) + ". " + fn, juce::dontSendNotification);
            c.nameLabel->setColour(juce::Label::textColourId, Palette::text);
            c.nameLabel->setTooltip("Track " + juce::String(t + 1) + ": " + fn +
                                    (s->kind == SampleKind::Loop ? " [Loop]" : " [One-shot]"));
        }
        else
        {
            c.nameLabel->setText(juce::String(t + 1) + ". (empty)", juce::dontSendNotification);
            c.nameLabel->setColour(juce::Label::textColourId, Palette::textDim.withAlpha(0.6f));
            c.nameLabel->setTooltip("Track " + juce::String(t + 1) + " (empty)");
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
    g.fillAll(Palette::surface);

    // Header top area (matching ruler)
    g.setColour(Palette::background);
    g.fillRect(0, 0, getWidth(), PatternGrid::kRulerHeight);
    g.setColour(Palette::textDim);
    g.setFont(Typography::bold(12.0f));
    g.drawText("TRACKS / FILES", 10, 0, getWidth() - 20, PatternGrid::kRulerHeight, juce::Justification::centredLeft);

    const Pattern* pattern = song_.findPattern(patternId_);

    for (int t = 0; t < kTracksPerPattern; ++t)
    {
        const int y = PatternGrid::kRulerHeight + t * trackHeight_;

        if (t % 2 != 0)
        {
            g.setColour(Palette::background.withAlpha(0.25f));
            g.fillRect(0, y, getWidth(), trackHeight_);
        }

        // ACID-style track badge on left
        const auto badgeRect = juce::Rectangle<float>(6.0f, static_cast<float>(y + 3), 32.0f,
                                                      static_cast<float>(std::max(16, trackHeight_ - 6)));
        juce::Colour badgeCol = Palette::surface.brighter(0.08f);
        SampleKind kind = SampleKind::OneShot;
        bool hasSample = false;

        if (pattern != nullptr)
        {
            const auto& track = pattern->tracks[static_cast<std::size_t>(t)];
            SampleId sid = track.sampleId;
            if (!sid.isValid() && !track.clips.empty())
                sid = track.clips.front().sampleId;

            if (const Sample* s = song_.findSample(sid))
            {
                hasSample = true;
                kind = s->kind;
                badgeCol = juce::Colour::fromHSV(s->hue(), 0.75f, 0.85f, 1.0f);
            }
        }

        g.setColour(badgeCol);
        g.fillRoundedRectangle(badgeRect, 4.0f);
        g.setColour(badgeCol.brighter(0.25f));
        g.drawRoundedRectangle(badgeRect, 4.0f, 1.0f);

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
    }
}

void TrackHeaderList::resized()
{
    for (int t = 0; t < kTracksPerPattern; ++t)
    {
        const int y = PatternGrid::kRulerHeight + t * trackHeight_;
        auto& c = controls_[static_cast<std::size_t>(t)];

        if (trackHeight_ >= 34)
        {
            // 2-row layout:
            // Top: Name Label
            c.nameLabel->setBounds(44, y + 2, getWidth() - 48, 18);

            // Bottom: M S Vol Pan
            c.muteButton->setBounds(44, y + 21, 22, 16);
            c.soloButton->setBounds(70, y + 21, 22, 16);

            const int availableSliderW = getWidth() - 100 - 8;
            const int sliderW = std::max(20, availableSliderW / 2);
            c.volumeSlider->setBounds(98, y + 21, sliderW, 16);
            c.panSlider->setBounds(98 + sliderW + 4, y + 21, sliderW, 16);
        }
        else
        {
            // 1-row compact layout
            c.nameLabel->setBounds(42, y + 2, 75, trackHeight_ - 4);
            c.muteButton->setBounds(120, y + 2, 18, trackHeight_ - 4);
            c.soloButton->setBounds(140, y + 2, 18, trackHeight_ - 4);

            const int sliderW = std::max(15, (getWidth() - 166) / 2);
            c.volumeSlider->setBounds(162, y + 2, sliderW, trackHeight_ - 4);
            c.panSlider->setBounds(162 + sliderW + 2, y + 2, sliderW, trackHeight_ - 4);
        }
    }
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

void TrackHeaderList::filesDropped(const juce::StringArray& files, int /*x*/, int y)
{
    if (files.isEmpty())
        return;

    const int t = pointToTrack({10, y});
    if (t >= 0 && onFileDropped_)
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

void TrackHeaderList::itemDropped(const SourceDetails& dragSourceDetails)
{
    if (!dragSourceDetails.description.isString())
        return;

    const juce::String filePath = dragSourceDetails.description.toString();
    const int t = pointToTrack(dragSourceDetails.localPosition);
    if (t >= 0 && onFileDropped_)
        onFileDropped_(filePath, t);
}

} // namespace trami
