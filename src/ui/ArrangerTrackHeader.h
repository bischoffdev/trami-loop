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
    using MixChangedCallback = std::function<void(int track)>;
    using TrackRenamedCallback = std::function<void(int track, const std::string& name)>;

    ArrangerTrackHeaderList(Song& song, MixChangedCallback onMixChanged);

    void updateFromModel();
    void setTrackHeight(int height);
    int trackHeight() const noexcept { return trackHeight_; }
    void setOnTrackRenamed(TrackRenamedCallback cb) { onTrackRenamed_ = std::move(cb); }

    std::function<void(const juce::MouseEvent&, const juce::MouseWheelDetails&)> onMouseWheel;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    void mouseDown(const juce::MouseEvent& e) override;

    static constexpr int kHeaderWidth = 240;

private:
    struct TrackControls
    {
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<juce::TextButton> muteButton;
        std::unique_ptr<juce::TextButton> soloButton;
        std::unique_ptr<juce::Label> volLabel;
        std::unique_ptr<juce::Slider> volumeSlider;
        std::unique_ptr<juce::Label> panLabel;
        std::unique_ptr<juce::Slider> panSlider;
    };

    Song& song_;
    MixChangedCallback onMixChanged_;
    TrackRenamedCallback onTrackRenamed_;
    int trackHeight_ = 44;

    std::array<TrackControls, kArrangerTracks> controls_;
};

} // namespace trami
