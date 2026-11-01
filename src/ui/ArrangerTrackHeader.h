#pragma once

#include "model/Song.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <functional>

namespace trami
{

class ArrangerTrackHeaderList : public juce::Component
{
public:
    using MixChangedCallback = std::function<void()>;

    ArrangerTrackHeaderList(Song& song, MixChangedCallback onMixChanged);

    void updateFromModel();

    void paint(juce::Graphics& g) override;
    void resized() override;

    static constexpr int kHeaderWidth = 170;

private:
    struct TrackControls
    {
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<juce::ToggleButton> muteToggle;
        std::unique_ptr<juce::Slider> volumeSlider;
        std::unique_ptr<juce::Slider> panSlider;
    };

    Song& song_;
    MixChangedCallback onMixChanged_;

    std::array<TrackControls, kArrangerTracks> controls_;
};

} // namespace trami
