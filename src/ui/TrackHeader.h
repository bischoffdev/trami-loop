#pragma once

#include "model/Song.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <functional>

namespace trami
{

class TrackHeaderList : public juce::Component
{
public:
    using MixChangedCallback = std::function<void(int track, float volume, float pan)>;

    TrackHeaderList(Song& song, PatternId patternId, MixChangedCallback onMixChanged);

    void setPattern(PatternId patternId);
    void updateFromModel();

    void paint(juce::Graphics& g) override;
    void resized() override;

    static constexpr int kHeaderWidth = 160;

private:
    struct TrackControls
    {
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<juce::Slider> volumeSlider;
        std::unique_ptr<juce::Slider> panSlider;
    };

    Song& song_;
    PatternId patternId_;
    MixChangedCallback onMixChanged_;

    std::array<TrackControls, kTracksPerPattern> controls_;
};

} // namespace trami
