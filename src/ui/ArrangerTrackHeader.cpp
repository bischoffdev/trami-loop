#include "ui/ArrangerTrackHeader.h"

#include "ui/ArrangerGrid.h"
#include "ui/TramiLookAndFeel.h"

namespace trami
{

ArrangerTrackHeaderList::ArrangerTrackHeaderList(Song& song, MixChangedCallback onMixChanged)
    : song_(song), onMixChanged_(std::move(onMixChanged))
{
    for (int t = 0; t < kArrangerTracks; ++t)
    {
        auto& c = controls_[static_cast<std::size_t>(t)];

        c.label = std::make_unique<juce::Label>("lbl", "Track " + juce::String(t + 1));
        c.label->setFont(Typography::bold(14.0f));
        c.label->setColour(juce::Label::textColourId, Palette::text);
        c.label->setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(c.label.get());

        c.muteButton = std::make_unique<juce::TextButton>("M");
        c.muteButton->setClickingTogglesState(true);
        c.muteButton->setColour(juce::TextButton::buttonColourId, Palette::surface.brighter(0.05f));
        c.muteButton->setColour(juce::TextButton::textColourOffId, Palette::textDim);
        c.muteButton->setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffe53935));
        c.muteButton->setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        c.muteButton->setTooltip("Mute Arranger Track " + juce::String(t + 1));
        c.muteButton->onClick = [this, t]() {
            if (auto* track = song_.arrangerTrack(t))
            {
                track->muted = controls_[static_cast<std::size_t>(t)].muteButton->getToggleState();
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
        c.soloButton->setTooltip("Solo Arranger Track " + juce::String(t + 1));
        c.soloButton->onClick = [this, t]() {
            if (auto* track = song_.arrangerTrack(t))
            {
                track->solo = controls_[static_cast<std::size_t>(t)].soloButton->getToggleState();
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
            if (auto* track = song_.arrangerTrack(t))
            {
                track->mix.volume =
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
            if (auto* track = song_.arrangerTrack(t))
            {
                track->mix.pan = static_cast<float>(controls_[static_cast<std::size_t>(t)].panSlider->getValue());
                if (onMixChanged_)
                    onMixChanged_(t);
            }
        };
        addAndMakeVisible(c.panSlider.get());
    }

    updateFromModel();
}

void ArrangerTrackHeaderList::updateFromModel()
{
    const auto& arrangement = song_.arrangement();
    for (int t = 0; t < kArrangerTracks; ++t)
    {
        const auto& track = arrangement[static_cast<std::size_t>(t)];
        auto& c = controls_[static_cast<std::size_t>(t)];
        c.muteButton->setToggleState(track.muted, juce::dontSendNotification);
        c.soloButton->setToggleState(track.solo, juce::dontSendNotification);
        c.volumeSlider->setValue(track.mix.volume, juce::dontSendNotification);
        c.panSlider->setValue(track.mix.pan, juce::dontSendNotification);
    }
}

void ArrangerTrackHeaderList::setTrackHeight(int height)
{
    const int clamped = std::max(22, height);
    if (trackHeight_ != clamped)
    {
        trackHeight_ = clamped;
        resized();
        repaint();
    }
}

void ArrangerTrackHeaderList::paint(juce::Graphics& g)
{
    g.fillAll(Palette::surface);

    // Header top area (matching ruler)
    g.setColour(Palette::background);
    g.fillRect(0, 0, getWidth(), ArrangerGrid::kRulerHeight);
    g.setColour(Palette::textDim);
    g.setFont(Typography::bold(12.0f));
    g.drawText("ARRANGER TRACKS", 10, 0, getWidth() - 20, ArrangerGrid::kRulerHeight, juce::Justification::centredLeft);

    for (int t = 0; t < kArrangerTracks; ++t)
    {
        const int y = ArrangerGrid::kRulerHeight + t * trackHeight_;

        if (t % 2 != 0)
        {
            g.setColour(Palette::background.withAlpha(0.2f));
            g.fillRect(0, y, getWidth(), trackHeight_);
        }

        // Horizontal separator
        g.setColour(Palette::gridLine.withAlpha(0.6f));
        g.drawHorizontalLine(y + trackHeight_, 0.0f, static_cast<float>(getWidth()));
    }
}

void ArrangerTrackHeaderList::resized()
{
    for (int t = 0; t < kArrangerTracks; ++t)
    {
        const int y = ArrangerGrid::kRulerHeight + t * trackHeight_;
        auto row = juce::Rectangle<int>(0, y, getWidth(), trackHeight_);
        auto& c = controls_[static_cast<std::size_t>(t)];

        if (trackHeight_ >= 34)
        {
            // 2-row layout:
            // Top: Label (clear, readable 14px bold)
            auto topArea = row.removeFromTop(20).reduced(6, 1);
            c.label->setBounds(topArea);

            // Bottom: M S Vol Pan
            auto botArea = row.reduced(6, 2);
            c.muteButton->setBounds(botArea.removeFromLeft(22));
            botArea.removeFromLeft(4);
            c.soloButton->setBounds(botArea.removeFromLeft(22));
            botArea.removeFromLeft(8);

            const int sliderWidth = std::max(20, (botArea.getWidth() - 6) / 2);
            c.volumeSlider->setBounds(botArea.removeFromLeft(sliderWidth));
            botArea.removeFromLeft(6);
            c.panSlider->setBounds(botArea);
        }
        else
        {
            // 1-row compact layout
            auto r = row.reduced(6, 2);
            c.label->setBounds(r.removeFromLeft(65));
            r.removeFromLeft(3);
            c.muteButton->setBounds(r.removeFromLeft(18));
            r.removeFromLeft(2);
            c.soloButton->setBounds(r.removeFromLeft(18));
            r.removeFromLeft(4);

            const int sliderWidth = std::max(15, (r.getWidth() - 3) / 2);
            c.volumeSlider->setBounds(r.removeFromLeft(sliderWidth));
            r.removeFromLeft(3);
            c.panSlider->setBounds(r);
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

} // namespace trami
