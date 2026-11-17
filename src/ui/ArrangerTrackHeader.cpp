#include "ui/ArrangerTrackHeader.h"

#include "ui/ArrangerGrid.h"
#include "ui/TramiLookAndFeel.h"

namespace trami
{

class TrackNameLabel : public juce::Label
{
  public:
    TrackNameLabel(const juce::String& name, const juce::String& text) : juce::Label(name, text) {}

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

ArrangerTrackHeaderList::ArrangerTrackHeaderList(Song& song, MixChangedCallback onMixChanged)
    : song_(song), onMixChanged_(std::move(onMixChanged))
{
    for (int t = 0; t < kArrangerTracks; ++t)
    {
        auto& c = controls_[static_cast<std::size_t>(t)];

        c.label = std::make_unique<TrackNameLabel>("lbl", "Track " + juce::String(t + 1));
        c.label->setFont(Typography::bold(14.0f));
        c.label->setColour(juce::Label::textColourId, Palette::text);
        c.label->setJustificationType(juce::Justification::centredLeft);
        c.label->setEditable(false, true, false);
        c.label->onTextChange = [this, t, lbl = c.label.get()]
        {
            if (lbl != nullptr)
            {
                juce::String txt = lbl->getText().trim();
                song_.renameArrangerTrack(t, txt.toStdString());
                if (onTrackRenamed_)
                    onTrackRenamed_(t, txt.toStdString());
                updateFromModel();
            }
        };
        addAndMakeVisible(c.label.get());

        c.deleteButton = std::make_unique<juce::TextButton>(juce::CharPointer_UTF8("\xc3\x97"));
        c.deleteButton->setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        c.deleteButton->setColour(juce::TextButton::textColourOffId, Palette::textDim);
        c.deleteButton->setColour(juce::TextButton::buttonOnColourId, Palette::danger.withAlpha(0.3f));
        c.deleteButton->setTooltip("Delete Arranger Track " + juce::String(t + 1));
        c.deleteButton->onClick = [this, t]()
        {
            if (onDeleteTrack_)
                onDeleteTrack_(t);
        };
        addAndMakeVisible(c.deleteButton.get());

        c.muteButton = std::make_unique<DAWMuteButton>();
        c.muteButton->setTooltip("Mute Arranger Track " + juce::String(t + 1));
        c.muteButton->onClick = [this, t]()
        {
            if (auto* track = song_.arrangerTrack(t))
            {
                track->muted = controls_[static_cast<std::size_t>(t)].muteButton->getToggleState();
                if (onMixChanged_)
                    onMixChanged_(t);
            }
        };
        addAndMakeVisible(c.muteButton.get());

        c.soloButton = std::make_unique<DAWSoloButton>();
        c.soloButton->setTooltip("Solo Arranger Track " + juce::String(t + 1));
        c.soloButton->onClick = [this, t]()
        {
            if (auto* track = song_.arrangerTrack(t))
            {
                track->solo = controls_[static_cast<std::size_t>(t)].soloButton->getToggleState();
                if (onMixChanged_)
                    onMixChanged_(t);
            }
        };
        addAndMakeVisible(c.soloButton.get());

        c.armButton = std::make_unique<DAWArmButton>();
        c.armButton->setTooltip("Arm Arranger Track " + juce::String(t + 1) + " for Recording");
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
        c.volumeSlider->onValueChange = [this, t]()
        {
            if (auto* track = song_.arrangerTrack(t))
            {
                track->mix.volume = static_cast<float>(controls_[static_cast<std::size_t>(t)].volumeSlider->getValue());
                if (onMixChanged_)
                    onMixChanged_(t);
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
        c.panSlider->onValueChange = [this, t]()
        {
            if (auto* track = song_.arrangerTrack(t))
            {
                track->mix.pan = static_cast<float>(controls_[static_cast<std::size_t>(t)].panSlider->getValue());
                if (onMixChanged_)
                    onMixChanged_(t);
            }
        };
        addAndMakeVisible(c.panSlider.get());
    }

    addTrackButton_ = std::make_unique<juce::TextButton>("+ Add Track");
    addTrackButton_->setColour(juce::TextButton::buttonColourId, Palette::surface.brighter(0.04f));
    addTrackButton_->setColour(juce::TextButton::textColourOffId, Palette::arrangerAccent);
    addTrackButton_->setTooltip("Add a new arranger track");
    addTrackButton_->onClick = [this]()
    {
        if (onAddTrack_)
            onAddTrack_();
    };
    addAndMakeVisible(addTrackButton_.get());

    updateFromModel();
}

void ArrangerTrackHeaderList::updateFromModel()
{
    const int numTracks = song_.numArrangerTracks();
    const auto& arrangement = song_.arrangement();

    for (int t = 0; t < kArrangerTracks; ++t)
    {
        auto& c = controls_[static_cast<std::size_t>(t)];
        const bool isVisibleTrack = (t < numTracks);

        c.label->setVisible(isVisibleTrack);
        if (c.deleteButton != nullptr)
            c.deleteButton->setVisible(isVisibleTrack);
        c.muteButton->setVisible(isVisibleTrack);
        c.soloButton->setVisible(isVisibleTrack);
        if (c.armButton != nullptr)
            c.armButton->setVisible(isVisibleTrack);
        c.volLabel->setVisible(isVisibleTrack);
        c.volumeSlider->setVisible(isVisibleTrack);
        c.panLabel->setVisible(isVisibleTrack);
        c.panSlider->setVisible(isVisibleTrack);

        if (!isVisibleTrack)
            continue;

        const auto& track = arrangement[static_cast<std::size_t>(t)];
        const std::string name = song_.arrangerTrackName(t);
        if (!name.empty())
        {
            c.label->setText(juce::String(name), juce::dontSendNotification);
        }
        else if (track.patternId.isValid())
        {
            if (const auto* p = song_.findPattern(track.patternId))
                c.label->setText(juce::String(t + 1) + ". " + juce::String(p->name), juce::dontSendNotification);
            else
                c.label->setText("Track " + juce::String(t + 1), juce::dontSendNotification);
        }
        else
        {
            c.label->setText("Track " + juce::String(t + 1) + " (Empty)", juce::dontSendNotification);
        }

        c.muteButton->setToggleState(track.muted, juce::dontSendNotification);
        c.soloButton->setToggleState(track.solo, juce::dontSendNotification);
        c.volumeSlider->setValue(track.mix.volume, juce::dontSendNotification);
        c.panSlider->setValue(track.mix.pan, juce::dontSendNotification);
    }

    if (addTrackButton_ != nullptr)
        addTrackButton_->setVisible(numTracks < kArrangerTracks);

    resized();
    repaint();
}

void ArrangerTrackHeaderList::setTrackHeight(int height)
{
    const int clamped = std::max(kMinTrackHeight, height);
    if (trackHeight_ != clamped)
    {
        trackHeight_ = clamped;
        resized();
        repaint();
    }
}

int ArrangerTrackHeaderList::pointToTrack(juce::Point<int> pt) const
{
    const int t = (pt.y - ArrangerGrid::kRulerHeight) / trackHeight_;
    return (t >= 0 && t < song_.numArrangerTracks()) ? t : -1;
}

void ArrangerTrackHeaderList::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff22252b));

    // Header top area (matching ruler)
    g.setColour(Palette::arrangerHeaderBg);
    g.fillRect(0, 0, getWidth(), ArrangerGrid::kRulerHeight);
    g.setColour(Palette::arrangerAccent);
    g.drawHorizontalLine(ArrangerGrid::kRulerHeight - 1, 0.0f, static_cast<float>(getWidth()));

    g.setColour(Palette::arrangerAccent);
    g.setFont(Typography::bold(12.0f));
    g.drawText("ARRANGER TRACKS", 12, 0, getWidth() - 24, ArrangerGrid::kRulerHeight, juce::Justification::centredLeft);

    constexpr int kStripWidth = 14;
    const int numTracks = song_.numArrangerTracks();

    for (int t = 0; t < numTracks; ++t)
    {
        const int y = ArrangerGrid::kRulerHeight + t * trackHeight_;

        // Track surface background
        g.setColour(t % 2 == 0 ? juce::Colour(0xff24282f) : juce::Colour(0xff282c34));
        g.fillRect(0, y, getWidth(), trackHeight_);

        // Studio One left color strip
        const auto& track = song_.arrangement()[static_cast<std::size_t>(t)];
        const juce::Colour trackCol = track.patternId.isValid() ? ArrangerGrid::patternColour(track.patternId)
                                                                : Palette::arrangerAccent.withAlpha(0.6f);

        g.setColour(trackCol);
        g.fillRect(0, y, kStripWidth, trackHeight_);

        // Dark dividing line on right edge of color strip
        g.setColour(juce::Colours::black.withAlpha(0.45f));
        g.drawVerticalLine(kStripWidth - 1, static_cast<float>(y), static_cast<float>(y + trackHeight_));

        // Chevron fold icon at the top of the color strip
        DAWTrackIcons::drawTrackFoldChevron(g, kStripWidth * 0.5f, static_cast<float>(y + 8), 7.0f);

        // Waveform icon before track name (if row height allows)
        if (trackHeight_ >= 30)
        {
            DAWTrackIcons::drawAudioWaveformIcon(
                g, juce::Rectangle<float>(static_cast<float>(kStripWidth + 4), static_cast<float>(y + 2), 16.0f, 18.0f),
                juce::Colour(0xff9ca3af));
        }

        // Horizontal separator between tracks
        g.setColour(juce::Colour(0xff181a1f));
        g.drawHorizontalLine(y + trackHeight_ - 1, 0.0f, static_cast<float>(getWidth()));

        // Drag hover highlight
        if (dragHoverTrack_ == t)
        {
            g.setColour(Palette::arrangerAccent);
            g.fillRect(0, y, getWidth(), 3);
        }
    }

    if (dragHoverTrack_ >= numTracks && addTrackButton_ != nullptr)
    {
        g.setColour(Palette::arrangerAccent.withAlpha(0.25f));
        g.fillRoundedRectangle(addTrackButton_->getBounds().toFloat().expanded(2.0f), 4.0f);
        g.setColour(Palette::arrangerAccent);
        g.drawRoundedRectangle(addTrackButton_->getBounds().toFloat().expanded(2.0f), 4.0f, 2.0f);
    }

    // Right vertical border dividing from ArrangerGrid
    g.setColour(juce::Colour(0xff181a1f));
    g.drawVerticalLine(getWidth() - 2, 0.0f, static_cast<float>(getHeight()));
    g.drawVerticalLine(getWidth() - 1, 0.0f, static_cast<float>(getHeight()));
}

void ArrangerTrackHeaderList::resized()
{
    const int numTracks = song_.numArrangerTracks();
    constexpr int kStripWidth = 14;

    for (int t = 0; t < numTracks; ++t)
    {
        const int y = ArrangerGrid::kRulerHeight + t * trackHeight_;
        auto& c = controls_[static_cast<std::size_t>(t)];

        // 2-row layout:
        // Top: Waveform icon (painted) + Name Label + subtle delete button
        const int nameX = kStripWidth + 24;
        const int nameW = std::max(40, getWidth() - nameX - 26);
        c.label->setBounds(nameX, y + 2, nameW, 20);
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
        if (numTracks < kArrangerTracks)
        {
            const int y = ArrangerGrid::kRulerHeight + numTracks * trackHeight_ + 4;
            addTrackButton_->setBounds(kStripWidth + 4, y, getWidth() - kStripWidth - 12, 24);
            addTrackButton_->setVisible(true);
        }
        else
        {
            addTrackButton_->setVisible(false);
        }
    }
}

void ArrangerTrackHeaderList::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (onMouseWheel)
        onMouseWheel(e, wheel);
    else
        juce::Component::mouseWheelMove(e, wheel);
}

void ArrangerTrackHeaderList::mouseDrag(const juce::MouseEvent& e)
{
    if (e.mouseWasDraggedSinceMouseDown() && !e.mods.isPopupMenu())
    {
        const int t = pointToTrack(e.getMouseDownPosition());
        if (t < 0 || t >= kArrangerTracks)
        {
            if (onMarqueeDrag)
                onMarqueeDrag(e);
            return;
        }
        if (t >= 0 && t < kArrangerTracks)
        {
            if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor(this))
            {
                if (!container->isDragAndDropActive())
                {
                    const int trackY = ArrangerGrid::kRulerHeight + t * trackHeight_;
                    const auto trackBounds = juce::Rectangle<int>(0, trackY, getWidth(), trackHeight_);
                    auto dragImg = createComponentSnapshot(trackBounds, true, 1.0f);
                    {
                        juce::Graphics g(dragImg);
                        g.setColour(Palette::accent);
                        g.drawRect(dragImg.getBounds().toFloat(), 1.5f);
                    }
                    const auto offset = trackBounds.getPosition() - e.getPosition();
                    container->startDragging(juce::var("arranger_track_drag:" + juce::String(t)),
                                             this,
                                             juce::ScaledImage(dragImg, 1.0),
                                             true,
                                             &offset);
                }
            }
        }
    }
}

bool ArrangerTrackHeaderList::isInterestedInDragSource(const SourceDetails& details)
{
    return details.description.toString().startsWith("arranger_track_drag:");
}

void ArrangerTrackHeaderList::itemDragEnter(const SourceDetails& details)
{
    dragHoverTrack_ = pointToTrack(details.localPosition);
    repaint();
}

void ArrangerTrackHeaderList::itemDragMove(const SourceDetails& details)
{
    const int t = pointToTrack(details.localPosition);
    if (t != dragHoverTrack_)
    {
        dragHoverTrack_ = t;
        repaint();
    }
}

void ArrangerTrackHeaderList::itemDragExit(const SourceDetails&)
{
    dragHoverTrack_ = -1;
    repaint();
}

void ArrangerTrackHeaderList::itemDropped(const SourceDetails& details)
{
    dragHoverTrack_ = -1;
    repaint();
    const juce::String desc = details.description.toString();
    if (desc.startsWith("arranger_track_drag:"))
    {
        const int fromTrack = desc.fromFirstOccurrenceOf("arranger_track_drag:", false, false).getIntValue();
        const int toTrack = pointToTrack(details.localPosition);
        if (fromTrack != toTrack && toTrack >= 0 && toTrack < kArrangerTracks)
        {
            if (onMoveTrack_)
                onMoveTrack_(fromTrack, toTrack);
        }
    }
}

void ArrangerTrackHeaderList::mouseDown(const juce::MouseEvent& e)
{
    const int t = pointToTrack(e.getPosition());
    if (t < 0 || t >= kArrangerTracks)
    {
        if (onMarqueeStart)
            onMarqueeStart(e);
        return;
    }

    if (e.mods.isPopupMenu())
    {
        juce::PopupMenu m;
        m.addItem(1, "Rename Track...");

        juce::PopupMenu assignMenu;
        assignMenu.addItem(100, "(Unassigned)", true, !song_.arrangerTrackPattern(t).isValid());
        int menuIdx = 101;
        std::vector<PatternId> patIds;
        for (const auto& pat : song_.patterns())
        {
            const bool isCurrent = (song_.arrangerTrackPattern(t) == pat.id);
            assignMenu.addItem(menuIdx++, juce::String(pat.name), true, isCurrent);
            patIds.push_back(pat.id);
        }
        m.addSubMenu("Assign Pattern", assignMenu);
        m.addItem(2, "Clear Track Blocks");
        m.addSeparator();
        m.addItem(10, "Move Track Up", t > 0);
        m.addItem(11, "Move Track Down", t < song_.numArrangerTracks() - 1);
        m.addSeparator();
        m.addItem(20, "Delete Track", song_.numArrangerTracks() > 1);

        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withTargetScreenArea(
                            juce::Rectangle<int>(e.getScreenX(), e.getScreenY(), 1, 1)),
                        [this, t, patIds](int result)
                        {
                            if (result == 1 && t < static_cast<int>(controls_.size()))
                            {
                                if (controls_[static_cast<std::size_t>(t)].label != nullptr)
                                    controls_[static_cast<std::size_t>(t)].label->showEditor();
                            }
                            else if (result == 2)
                            {
                                if (auto* trk = song_.arrangerTrack(t))
                                {
                                    trk->blocks.clear();
                                    if (onMixChanged_)
                                        onMixChanged_(t);
                                }
                            }
                            else if (result == 10 && onMoveTrack_)
                            {
                                onMoveTrack_(t, t - 1);
                            }
                            else if (result == 11 && onMoveTrack_)
                            {
                                onMoveTrack_(t, t + 1);
                            }
                            else if (result == 20 && onDeleteTrack_)
                            {
                                onDeleteTrack_(t);
                            }
                            else if (result == 100)
                            {
                                song_.setArrangerTrackPattern(t, PatternId{});
                                updateFromModel();
                                if (onMixChanged_)
                                    onMixChanged_(t);
                            }
                            else if (result >= 101 && result < 101 + static_cast<int>(patIds.size()))
                            {
                                song_.setArrangerTrackPattern(t, patIds[static_cast<std::size_t>(result - 101)]);
                                updateFromModel();
                                if (onMixChanged_)
                                    onMixChanged_(t);
                            }
                        });
    }
}

void ArrangerTrackHeaderList::mouseUp(const juce::MouseEvent& e)
{
    if (onMarqueeEnd)
        onMarqueeEnd(e);
}

} // namespace trami
