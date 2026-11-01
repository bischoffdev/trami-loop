#include "ui/TrackHeader.h"

#include "ui/PatternGrid.h"
#include "ui/TramiLookAndFeel.h"

namespace trami
{

TrackHeaderList::TrackHeaderList(Song& song, PatternId patternId, MixChangedCallback onMixChanged)
    : song_(song), patternId_(patternId), onMixChanged_(std::move(onMixChanged))
{
    for (int t = 0; t < kTracksPerPattern; ++t)
    {
        auto& c = controls_[static_cast<std::size_t>(t)];

        c.label = std::make_unique<juce::Label>("lbl", "T" + juce::String(t + 1));
        c.label->setFont(juce::FontOptions(11.0f));
        c.label->setColour(juce::Label::textColourId, Palette::textDim);
        c.label->setJustificationType(juce::Justification::centred);
        addAndMakeVisible(c.label.get());

        c.volumeSlider = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal, juce::Slider::NoTextBox);
        c.volumeSlider->setRange(0.0, 2.0, 0.01);
        c.volumeSlider->setValue(1.0, juce::dontSendNotification);
        c.volumeSlider->setTooltip("Volume");
        c.volumeSlider->onValueChange = [this, t]() {
            if (Pattern* p = song_.findPattern(patternId_))
            {
                p->tracks[static_cast<std::size_t>(t)].mix.volume =
                    static_cast<float>(controls_[static_cast<std::size_t>(t)].volumeSlider->getValue());
                if (onMixChanged_)
                    onMixChanged_(t, p->tracks[static_cast<std::size_t>(t)].mix.volume,
                                  p->tracks[static_cast<std::size_t>(t)].mix.pan);
            }
        };
        addAndMakeVisible(c.volumeSlider.get());

        c.panSlider = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal, juce::Slider::NoTextBox);
        c.panSlider->setRange(-1.0, 1.0, 0.01);
        c.panSlider->setValue(0.0, juce::dontSendNotification);
        c.panSlider->setTooltip("Pan");
        c.panSlider->onValueChange = [this, t]() {
            if (Pattern* p = song_.findPattern(patternId_))
            {
                p->tracks[static_cast<std::size_t>(t)].mix.pan =
                    static_cast<float>(controls_[static_cast<std::size_t>(t)].panSlider->getValue());
                if (onMixChanged_)
                    onMixChanged_(t, p->tracks[static_cast<std::size_t>(t)].mix.volume,
                                  p->tracks[static_cast<std::size_t>(t)].mix.pan);
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

void TrackHeaderList::updateFromModel()
{
    const Pattern* p = song_.findPattern(patternId_);
    if (p == nullptr)
        return;

    for (int t = 0; t < kTracksPerPattern; ++t)
    {
        const auto& mix = p->tracks[static_cast<std::size_t>(t)].mix;
        auto& c = controls_[static_cast<std::size_t>(t)];
        c.volumeSlider->setValue(mix.volume, juce::dontSendNotification);
        c.panSlider->setValue(mix.pan, juce::dontSendNotification);
    }
}

void TrackHeaderList::paint(juce::Graphics& g)
{
    g.fillAll(Palette::surface);

    // Header top area (matching ruler)
    g.setColour(Palette::background);
    g.fillRect(0, 0, getWidth(), PatternGrid::kRulerHeight);
    g.setColour(Palette::textDim);
    g.setFont(juce::FontOptions(11.0f));
    g.drawText("TRACKS", 8, 0, getWidth() - 16, PatternGrid::kRulerHeight, juce::Justification::centredLeft);

    // Horizontal separators
    g.setColour(Palette::gridLine.withAlpha(0.6f));
    for (int t = 0; t <= kTracksPerPattern; ++t)
    {
        const int y = PatternGrid::kRulerHeight + t * PatternGrid::kTrackHeight;
        g.drawHorizontalLine(y, 0.0f, static_cast<float>(getWidth()));
    }
}

void TrackHeaderList::resized()
{
    for (int t = 0; t < kTracksPerPattern; ++t)
    {
        const int y = PatternGrid::kRulerHeight + t * PatternGrid::kTrackHeight;
        auto row = juce::Rectangle<int>(0, y, getWidth(), PatternGrid::kTrackHeight).reduced(4, 4);

        auto& c = controls_[static_cast<std::size_t>(t)];
        c.label->setBounds(row.removeFromLeft(30));
        row.removeFromLeft(4);
        c.volumeSlider->setBounds(row.removeFromLeft(60));
        row.removeFromLeft(4);
        c.panSlider->setBounds(row);
    }
}

} // namespace trami
