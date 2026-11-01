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

        c.label = std::make_unique<juce::Label>("lbl", "A" + juce::String(t + 1));
        c.label->setFont(juce::FontOptions(11.0f).withStyle("Bold"));
        c.label->setColour(juce::Label::textColourId, Palette::textDim);
        c.label->setJustificationType(juce::Justification::centred);
        addAndMakeVisible(c.label.get());

        c.muteToggle = std::make_unique<juce::ToggleButton>("M");
        c.muteToggle->setColour(juce::ToggleButton::textColourId, Palette::textDim);
        c.muteToggle->onClick = [this, t]() {
            if (auto* track = song_.arrangerTrack(t))
            {
                track->muted = controls_[static_cast<std::size_t>(t)].muteToggle->getToggleState();
                if (onMixChanged_)
                    onMixChanged_();
            }
        };
        addAndMakeVisible(c.muteToggle.get());

        c.volumeSlider = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal, juce::Slider::NoTextBox);
        c.volumeSlider->setRange(0.0, 2.0, 0.01);
        c.volumeSlider->setValue(1.0, juce::dontSendNotification);
        c.volumeSlider->setTooltip("Arranger Track Volume");
        c.volumeSlider->onValueChange = [this, t]() {
            if (auto* track = song_.arrangerTrack(t))
            {
                track->mix.volume =
                    static_cast<float>(controls_[static_cast<std::size_t>(t)].volumeSlider->getValue());
                if (onMixChanged_)
                    onMixChanged_();
            }
        };
        addAndMakeVisible(c.volumeSlider.get());

        c.panSlider = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal, juce::Slider::NoTextBox);
        c.panSlider->setRange(-1.0, 1.0, 0.01);
        c.panSlider->setValue(0.0, juce::dontSendNotification);
        c.panSlider->setTooltip("Arranger Track Pan");
        c.panSlider->onValueChange = [this, t]() {
            if (auto* track = song_.arrangerTrack(t))
            {
                track->mix.pan = static_cast<float>(controls_[static_cast<std::size_t>(t)].panSlider->getValue());
                if (onMixChanged_)
                    onMixChanged_();
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
        c.muteToggle->setToggleState(track.muted, juce::dontSendNotification);
        c.volumeSlider->setValue(track.mix.volume, juce::dontSendNotification);
        c.panSlider->setValue(track.mix.pan, juce::dontSendNotification);
    }
}

void ArrangerTrackHeaderList::paint(juce::Graphics& g)
{
    g.fillAll(Palette::surface);

    // Header top area (matching ruler)
    g.setColour(Palette::background);
    g.fillRect(0, 0, getWidth(), ArrangerGrid::kRulerHeight);
    g.setColour(Palette::textDim);
    g.setFont(juce::FontOptions(11.0f));
    g.drawText("ARRANGER TRACKS", 8, 0, getWidth() - 16, ArrangerGrid::kRulerHeight, juce::Justification::centredLeft);

    // Horizontal separators
    g.setColour(Palette::gridLine.withAlpha(0.6f));
    for (int t = 0; t <= kArrangerTracks; ++t)
    {
        const int y = ArrangerGrid::kRulerHeight + t * ArrangerGrid::kTrackHeight;
        g.drawHorizontalLine(y, 0.0f, static_cast<float>(getWidth()));
    }
}

void ArrangerTrackHeaderList::resized()
{
    for (int t = 0; t < kArrangerTracks; ++t)
    {
        const int y = ArrangerGrid::kRulerHeight + t * ArrangerGrid::kTrackHeight;
        auto row = juce::Rectangle<int>(0, y, getWidth(), ArrangerGrid::kTrackHeight).reduced(4, 6);

        auto& c = controls_[static_cast<std::size_t>(t)];
        c.label->setBounds(row.removeFromLeft(24));
        row.removeFromLeft(2);
        c.muteToggle->setBounds(row.removeFromLeft(32));
        row.removeFromLeft(4);
        c.volumeSlider->setBounds(row.removeFromLeft(50));
        row.removeFromLeft(4);
        c.panSlider->setBounds(row);
    }
}

} // namespace trami
