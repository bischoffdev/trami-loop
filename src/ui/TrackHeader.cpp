#include "ui/TrackHeader.h"

#include "ui/PatternGrid.h"
#include "ui/TramiLookAndFeel.h"

#include <algorithm>

namespace trami
{

class TrackNameLabel : public juce::Label
{
public:
    TrackNameLabel(const juce::String& name, const juce::String& text)
        : juce::Label(name, text) {}

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (auto* parent = getParentComponent())
        {
            if (e.mods.isPopupMenu())
            {
                parent->mouseDown(e.getEventRelativeTo(parent));
                return;
            }
            parent->mouseDown(e.getEventRelativeTo(parent));
        }
        juce::Label::mouseDown(e);
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (auto* parent = getParentComponent())
            parent->mouseDrag(e.getEventRelativeTo(parent));
        juce::Label::mouseDrag(e);
    }
};

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

            c.nameLabel = std::make_unique<TrackNameLabel>("lbl", juce::String(t + 1) + ": (empty)");
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

            c.propsButton = std::make_unique<juce::TextButton>("P");
            c.propsButton->setColour(juce::TextButton::buttonColourId, Palette::surface.brighter(0.10f));
            c.propsButton->setColour(juce::TextButton::textColourOffId, Palette::accent);
            c.propsButton->setTooltip("Track " + juce::String(t + 1) + " Properties (Stretch Markers / Key / Summary)");
            c.propsButton->onClick = [this, t]() {
                if (onTrackProperties_)
                    onTrackProperties_(t);
            };
            addAndMakeVisible(c.propsButton.get());

            c.typeButton = std::make_unique<juce::TextButton>("type");
            addAndMakeVisible(c.typeButton.get());

            c.deleteButton = std::make_unique<juce::TextButton>(juce::CharPointer_UTF8("\xc3\x97"));
            c.deleteButton->setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
            c.deleteButton->setColour(juce::TextButton::textColourOffId, Palette::textDim.withAlpha(0.5f));
            c.deleteButton->setColour(juce::TextButton::buttonOnColourId, Palette::danger.withAlpha(0.2f));
            c.deleteButton->setTooltip("Delete Track " + juce::String(t + 1));
            c.deleteButton->onClick = [this, t]() {
                if (onDeleteTrack_)
                    onDeleteTrack_(t);
            };
            addAndMakeVisible(c.deleteButton.get());

            c.muteButton = std::make_unique<DAWMuteButton>();
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

            c.soloButton = std::make_unique<DAWSoloButton>();
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

            c.armButton = std::make_unique<DAWArmButton>();
            c.armButton->setTooltip("Arm Track " + juce::String(t + 1) + " for Recording");
            c.armButton->onClick = [this, t]() {
                if (auto* p = song_.findPattern(patternId_))
                {
                    if (t < static_cast<int>(p->tracks.size()))
                    {
                        p->tracks[static_cast<std::size_t>(t)].armed =
                            controls_[static_cast<std::size_t>(t)].armButton->getToggleState();
                        if (onMixChanged_)
                            onMixChanged_(t);
                    }
                }
            };
            addAndMakeVisible(c.armButton.get());

            c.volLabel = std::make_unique<juce::Label>("vol", "Vol");
            c.volLabel->setFont(Typography::bold(10.0f));
            c.volLabel->setBorderSize(juce::BorderSize<int>(0));
            c.volLabel->setColour(juce::Label::textColourId, Palette::textDim);
            c.volLabel->setJustificationType(juce::Justification::centred);
            c.volLabel->setInterceptsMouseClicks(false, false);
            addAndMakeVisible(c.volLabel.get());

            c.volumeSlider = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal, juce::Slider::NoTextBox);
            c.volumeSlider->setScrollWheelEnabled(false);
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
            c.panLabel->setBorderSize(juce::BorderSize<int>(0));
            c.panLabel->setColour(juce::Label::textColourId, Palette::textDim);
            c.panLabel->setJustificationType(juce::Justification::centred);
            c.panLabel->setInterceptsMouseClicks(false, false);
            addAndMakeVisible(c.panLabel.get());

            c.panSlider = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal, juce::Slider::NoTextBox);
            c.panSlider->setScrollWheelEnabled(false);
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
            c.armButton->setToggleState(track.armed, juce::dontSendNotification);

            const SampleKind kind = song_.patternTrackKind(patternId_, t);
            if (kind == SampleKind::Loop)
            {
                c.typeButton->setButtonText("LOOP");
                c.typeButton->setColour(juce::TextButton::buttonColourId, juce::Colour(0xfff59e0b).withAlpha(0.28f));
                c.typeButton->setColour(juce::TextButton::textColourOffId, juce::Colour(0xffffca28));
                c.typeButton->setTooltip("Track " + juce::String(t + 1) + ": Loop (Time-Stretched to tempo). Click to cycle to 1-Shot / Disk.");
            }
            else if (kind == SampleKind::OneShot)
            {
                c.typeButton->setButtonText("1-SHOT");
                c.typeButton->setColour(juce::TextButton::buttonColourId, Palette::surface.brighter(0.08f));
                c.typeButton->setColour(juce::TextButton::textColourOffId, Palette::textDim.brighter(0.2f));
                c.typeButton->setTooltip("Track " + juce::String(t + 1) + ": One-Shot (Natural pitch/duration). Click to cycle to Disk / Loop.");
            }
            else
            {
                c.typeButton->setButtonText("DISK");
                c.typeButton->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff10b981).withAlpha(0.28f));
                c.typeButton->setColour(juce::TextButton::textColourOffId, juce::Colour(0xff34d399));
                c.typeButton->setTooltip("Track " + juce::String(t + 1) + ": Disk-Based (Streams long audio files). Click to cycle to Loop.");
            }
            c.typeButton->onClick = [this, t, kind]() {
                SampleKind newKind = SampleKind::Loop;
                if (kind == SampleKind::Loop)
                    newKind = SampleKind::OneShot;
                else if (kind == SampleKind::OneShot)
                    newKind = SampleKind::DiskStream;
                else
                    newKind = SampleKind::Loop;
                if (onTrackTypeChanged_)
                    onTrackTypeChanged_(t, newKind);
            };

            SampleId sid = track.sampleId;
            if (!sid.isValid() && !track.clips.empty())
                sid = track.clips.front().sampleId;

            const Sample* samplePtr = song_.findSample(sid);
            const bool isMissing = (samplePtr != nullptr && samplePtr->missing);

            const std::string customName = song_.patternTrackName(patternId_, t);
            if (!customName.empty())
            {
                const juce::String suffix = isMissing ? " [MISSING]" : "";
                c.nameLabel->setText(juce::String(t + 1) + ". " + customName + suffix, juce::dontSendNotification);
                c.nameLabel->setColour(juce::Label::textColourId, isMissing ? Palette::danger : Palette::text);
                c.nameLabel->setTooltip("Track " + juce::String(t + 1) + ": " + customName +
                                        (isMissing ? " (Missing file: " + juce::String(samplePtr->fileName) + ")"
                                                   : " (Double-click to rename)"));
            }
            else if (samplePtr != nullptr)
            {
                const juce::String fn = samplePtr->fileName.empty() ? juce::String(samplePtr->name) : juce::String(samplePtr->fileName);
                const juce::String suffix = isMissing ? " [MISSING]" : "";
                c.nameLabel->setText(juce::String(t + 1) + ". " + fn + suffix, juce::dontSendNotification);
                c.nameLabel->setColour(juce::Label::textColourId, isMissing ? Palette::danger : Palette::text);
                c.nameLabel->setTooltip("Track " + juce::String(t + 1) + ": " + fn +
                                        (isMissing ? " (Missing audio file)" : " (Double-click to rename)"));
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
    const int clamped = std::max(kMinTrackHeight, height);
    if (trackHeight_ != clamped)
    {
        trackHeight_ = clamped;
        resized();
        repaint();
    }
}

void TrackHeaderList::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff22252b));

    // Header top area (matching ruler)
    g.setColour(Palette::patternHeaderBg);
    g.fillRect(0, 0, getWidth(), PatternGrid::kRulerHeight);
    g.setColour(Palette::patternAccent);
    g.drawHorizontalLine(PatternGrid::kRulerHeight - 1, 0.0f, static_cast<float>(getWidth()));

    const auto* pat = song_.findPattern(patternId_);
    const int numTracks = pat != nullptr ? static_cast<int>(pat->tracks.size()) : 0;
    constexpr int kStripWidth = 14;

    for (int t = 0; t < numTracks; ++t)
    {
        const int y = PatternGrid::kRulerHeight + t * trackHeight_;

        // Track surface background
        g.setColour(t % 2 == 0 ? juce::Colour(0xff24282f) : juce::Colour(0xff282c34));
        g.fillRect(0, y, getWidth(), trackHeight_);

        // Studio One left color strip
        juce::Colour badgeCol = Palette::patternAccent.withAlpha(0.6f);
        const auto& track = pat->tracks[static_cast<std::size_t>(t)];
        SampleId sid = track.sampleId;
        if (!sid.isValid() && !track.clips.empty())
            sid = track.clips.front().sampleId;

        if (const Sample* s = song_.findSample(sid))
            badgeCol = s->missing ? Palette::danger.withAlpha(0.8f) : juce::Colour::fromHSV(s->hue(), 0.8f, 0.88f, 1.0f);

        g.setColour(badgeCol);
        g.fillRect(0, y, kStripWidth, trackHeight_);

        // Dark divider on right edge of color strip
        g.setColour(juce::Colours::black.withAlpha(0.45f));
        g.drawVerticalLine(kStripWidth - 1, static_cast<float>(y), static_cast<float>(y + trackHeight_));

        // Chevron fold icon at top of color strip
        DAWTrackIcons::drawTrackFoldChevron(g, kStripWidth * 0.5f, static_cast<float>(y + 8), 7.0f);

        // Waveform icon before track name (if row height allows)
        if (trackHeight_ >= 30)
        {
            DAWTrackIcons::drawAudioWaveformIcon(g, juce::Rectangle<float>(static_cast<float>(kStripWidth + 4),
                                                                           static_cast<float>(y + 2), 16.0f, 18.0f),
                                                 juce::Colour(0xff9ca3af));
        }

        // Horizontal separator between tracks
        g.setColour(juce::Colour(0xff181a1f));
        g.drawHorizontalLine(y + trackHeight_ - 1, 0.0f, static_cast<float>(getWidth()));

        // Drag hover highlight
        if (dragHoverTrack_ == t)
        {
            if (isReorderDrag_)
            {
                g.setColour(Palette::patternAccent);
                g.fillRect(0, y, getWidth(), 3);
            }
            else
            {
                g.setColour(Palette::accent.withAlpha(0.22f));
                g.fillRect(0, y, getWidth(), trackHeight_);
                g.setColour(Palette::accent);
                g.drawRect(1.0f, static_cast<float>(y + 1), static_cast<float>(getWidth() - 2), static_cast<float>(trackHeight_ - 2), 2.0f);
            }
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
    g.setColour(juce::Colour(0xff181a1f));
    g.drawVerticalLine(getWidth() - 2, 0.0f, static_cast<float>(getHeight()));
    g.drawVerticalLine(getWidth() - 1, 0.0f, static_cast<float>(getHeight()));
}

void TrackHeaderList::resized()
{
    const int numTracks = static_cast<int>(controls_.size());
    constexpr int kStripWidth = 14;

    for (int t = 0; t < numTracks; ++t)
    {
        const int y = PatternGrid::kRulerHeight + t * trackHeight_;
        auto& c = controls_[static_cast<std::size_t>(t)];

        // 2-row layout:
        // Top: Waveform icon + Name Label + Props Button + Type Button + Delete Button
        const int nameX = kStripWidth + 24;
        const int nameW = std::max(40, getWidth() - nameX - 104);
        c.nameLabel->setBounds(nameX, y + 2, nameW, 20);
        if (c.propsButton != nullptr)
            c.propsButton->setBounds(getWidth() - 102, y + 2, 22, 20);
        if (c.typeButton != nullptr)
            c.typeButton->setBounds(getWidth() - 78, y + 2, 50, 20);
        if (c.deleteButton != nullptr)
            c.deleteButton->setBounds(getWidth() - 24, y + 2, 20, 20);

        // Bottom: [ M | S ]  ( • )  Vol [===] Pan [===]
        const int btnY = y + 24;
        const int btnH = std::clamp(trackHeight_ - 26, 18, 22);

        c.muteButton->setBounds(kStripWidth + 4, btnY, 22, btnH);
        c.soloButton->setBounds(kStripWidth + 28, btnY, 22, btnH);
        c.armButton->setBounds(kStripWidth + 54, btnY, 18, btnH);

        const int remainX = kStripWidth + 76;
        const int remainW = std::max(20, getWidth() - remainX - 6);
        constexpr int lblW = 26;
        const int sliderW = std::max(20, (remainW - (lblW * 2 + 8)) / 2);

        c.volLabel->setVisible(true);
        c.volumeSlider->setVisible(true);
        c.panLabel->setVisible(true);
        c.panSlider->setVisible(true);

        int curX = remainX;
        c.volLabel->setBounds(curX, btnY, lblW, btnH);
        curX += lblW + 2;
        c.volumeSlider->setBounds(curX, btnY, sliderW, btnH);
        curX += sliderW + 6;
        c.panLabel->setBounds(curX, btnY, lblW, btnH);
        curX += lblW + 2;
        c.panSlider->setBounds(curX, btnY, sliderW, btnH);
    }

    if (addTrackButton_ != nullptr)
    {
        const int y = PatternGrid::kRulerHeight + numTracks * trackHeight_ + 6;
        addTrackButton_->setBounds(kStripWidth + 4, y, getWidth() - kStripWidth - 12, 24);
    }
}

void TrackHeaderList::mouseDown(const juce::MouseEvent& e)
{
    const int t = pointToTrack(e.getPosition());
    const auto* pat = song_.findPattern(patternId_);
    if (!pat || t < 0 || t >= static_cast<int>(pat->tracks.size()))
    {
        if (onMarqueeStart)
            onMarqueeStart(e);
        return;
    }

    const auto& track = pat->tracks[static_cast<std::size_t>(t)];
    SampleId sid = track.sampleId;
    if (!sid.isValid() && !track.clips.empty())
        sid = track.clips.front().sampleId;

    const SampleKind kind = song_.patternTrackKind(patternId_, t);

    if (e.mods.isPopupMenu())
    {
        const TrackEnvelopeType visEnv = track.visibleEnvelope;
        juce::PopupMenu m;
        m.addItem(20, "Track Properties (Stretch / Key / Summary)...");
        m.addItem(1, "Rename Track...");
        m.addSeparator();
        m.addItem(2, "Track Type: Loop (Time-Stretched)", true, kind == SampleKind::Loop);
        m.addItem(3, "Track Type: One-Shot (Natural Speed)", true, kind == SampleKind::OneShot);
        m.addItem(6, "Track Type: Disk-Based (Stream Long Audio)", true, kind == SampleKind::DiskStream);
        m.addSeparator();
        juce::PopupMenu envMenu;
        envMenu.addItem(30, "Off (Hide Track Automation)", true, visEnv == TrackEnvelopeType::None);
        envMenu.addItem(31, "Volume Envelope", true, visEnv == TrackEnvelopeType::Volume);
        envMenu.addItem(32, "Pan Envelope", true, visEnv == TrackEnvelopeType::Pan);
        envMenu.addItem(33, "Aux FX Send Envelope", true, visEnv == TrackEnvelopeType::FxSend);
        m.addSubMenu("Track Envelope / Automation", envMenu);
        m.addSeparator();
        m.addItem(4, "Duplicate Track");
        m.addItem(10, "Move Track Up", t > 0);
        m.addItem(11, "Move Track Down", t < static_cast<int>(pat->tracks.size()) - 1);
        m.addSeparator();
        m.addItem(5, "Delete Track", pat->tracks.size() > 1);

        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withTargetScreenArea(
                            juce::Rectangle<int>(e.getScreenX(), e.getScreenY(), 1, 1)),
                        [this, t](int result) {
                            if (result == 20 && onTrackProperties_)
                            {
                                onTrackProperties_(t);
                            }
                            else if (result == 1 && t < static_cast<int>(controls_.size()))
                            {
                                if (controls_[static_cast<std::size_t>(t)].nameLabel != nullptr)
                                    controls_[static_cast<std::size_t>(t)].nameLabel->showEditor();
                            }
                            else if (result == 2 && onTrackTypeChanged_)
                                onTrackTypeChanged_(t, SampleKind::Loop);
                            else if (result == 3 && onTrackTypeChanged_)
                                onTrackTypeChanged_(t, SampleKind::OneShot);
                            else if (result == 6 && onTrackTypeChanged_)
                                onTrackTypeChanged_(t, SampleKind::DiskStream);
                            else if (result >= 30 && result <= 33)
                            {
                                if (auto* p = song_.findPattern(patternId_))
                                {
                                    if (t < static_cast<int>(p->tracks.size()))
                                    {
                                        auto& trk = p->tracks[static_cast<std::size_t>(t)];
                                        if (result == 30) trk.visibleEnvelope = TrackEnvelopeType::None;
                                        else if (result == 31)
                                        {
                                            trk.visibleEnvelope = TrackEnvelopeType::Volume;
                                            if (trk.volumeAutomation.empty())
                                                trk.volumeAutomation = {{0.0, trk.mix.volume, EnvelopeCurve::Linear},
                                                                        {static_cast<double>(p->lengthSteps()), trk.mix.volume, EnvelopeCurve::Linear}};
                                        }
                                        else if (result == 32)
                                        {
                                            trk.visibleEnvelope = TrackEnvelopeType::Pan;
                                            if (trk.panAutomation.empty())
                                                trk.panAutomation = {{0.0, trk.mix.pan, EnvelopeCurve::Linear},
                                                                     {static_cast<double>(p->lengthSteps()), trk.mix.pan, EnvelopeCurve::Linear}};
                                        }
                                        else if (result == 33)
                                        {
                                            trk.visibleEnvelope = TrackEnvelopeType::FxSend;
                                            if (trk.fxAutomation.empty())
                                                trk.fxAutomation = {{0.0, trk.fxSend, EnvelopeCurve::Linear},
                                                                    {static_cast<double>(p->lengthSteps()), trk.fxSend, EnvelopeCurve::Linear}};
                                        }
                                        if (onMixChanged_)
                                            onMixChanged_(t);
                                    }
                                }
                            }
                            else if (result == 4 && onDuplicateTrack_)
                                onDuplicateTrack_(t);
                            else if (result == 10 && onMoveTrack_)
                                onMoveTrack_(t, t - 1);
                            else if (result == 11 && onMoveTrack_)
                                onMoveTrack_(t, t + 1);
                            else if (result == 5 && onDeleteTrack_)
                                onDeleteTrack_(t);
                        });
        return;
    }

    const int y = PatternGrid::kRulerHeight + t * trackHeight_;
    const auto badgeRect = juce::Rectangle<int>(0, y, 36, trackHeight_);
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

void TrackHeaderList::mouseDrag(const juce::MouseEvent& e)
{
    const int startT = pointToTrack(e.getMouseDownPosition());
    const auto* pat = song_.findPattern(patternId_);
    if (!pat || startT < 0 || startT >= static_cast<int>(pat->tracks.size()))
    {
        if (onMarqueeDrag)
            onMarqueeDrag(e);
        return;
    }

    if (e.mouseWasDraggedSinceMouseDown() && !e.mods.isPopupMenu())
    {
        const int t = startT;
        if (pat != nullptr && t >= 0 && t < static_cast<int>(pat->tracks.size()))
        {
            if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor(this))
            {
                if (!container->isDragAndDropActive())
                {
                    const int trackY = PatternGrid::kRulerHeight + t * trackHeight_;
                    const auto trackBounds = juce::Rectangle<int>(0, trackY, getWidth(), trackHeight_);
                    auto dragImg = createComponentSnapshot(trackBounds, true, 1.0f);
                    {
                        juce::Graphics g(dragImg);
                        g.setColour(Palette::accent);
                        g.drawRect(dragImg.getBounds().toFloat(), 1.5f);
                    }
                    const auto offset = trackBounds.getPosition() - e.getPosition();
                    container->startDragging(juce::var("pattern_track_drag:" + juce::String(t)),
                                             this,
                                             juce::ScaledImage(dragImg, 1.0),
                                             true,
                                             &offset);
                }
            }
        }
    }
}

void TrackHeaderList::mouseUp(const juce::MouseEvent& e)
{
    if (onMarqueeEnd)
        onMarqueeEnd(e);
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
        if (s.startsWith("pattern_track_drag:"))
            return true;
        return s.endsWithIgnoreCase(".wav") || s.endsWithIgnoreCase(".mp3");
    }
    return false;
}

void TrackHeaderList::itemDragEnter(const SourceDetails& dragSourceDetails)
{
    isReorderDrag_ = dragSourceDetails.description.toString().startsWith("pattern_track_drag:");
    dragHoverTrack_ = pointToTrack(dragSourceDetails.localPosition);
    repaint();
}

void TrackHeaderList::itemDragMove(const SourceDetails& dragSourceDetails)
{
    isReorderDrag_ = dragSourceDetails.description.toString().startsWith("pattern_track_drag:");
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
    isReorderDrag_ = false;
    repaint();
}

void TrackHeaderList::itemDropped(const SourceDetails& dragSourceDetails)
{
    dragHoverTrack_ = -1;
    isReorderDrag_ = false;
    repaint();
    if (!dragSourceDetails.description.isString())
        return;

    const juce::String desc = dragSourceDetails.description.toString();
    if (desc.startsWith("pattern_track_drag:"))
    {
        const int fromTrack = desc.fromFirstOccurrenceOf("pattern_track_drag:", false, false).getIntValue();
        const int toTrack = pointToTrack(dragSourceDetails.localPosition);
        const auto* pat = song_.findPattern(patternId_);
        if (pat != nullptr && fromTrack != toTrack && toTrack >= 0 && toTrack < static_cast<int>(pat->tracks.size()))
        {
            if (onMoveTrack_)
                onMoveTrack_(fromTrack, toTrack);
        }
        return;
    }

    const juce::String filePath = desc;
    const int t = pointToTrack(dragSourceDetails.localPosition);
    if (onFileDropped_)
        onFileDropped_(filePath, t);
}

} // namespace trami
