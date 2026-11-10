#pragma once

#include "model/Song.h"
#include "ui/DAWTrackControls.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <functional>

namespace trami
{

class ArrangerTrackHeaderList : public juce::Component,
                                public juce::DragAndDropTarget
{
public:
    using MixChangedCallback = std::function<void(int track)>;
    using TrackRenamedCallback = std::function<void(int track, const std::string& name)>;
    using DeleteTrackCallback = std::function<void(int track)>;
    using MoveTrackCallback = std::function<void(int from, int to)>;
    using AddTrackCallback = std::function<void()>;

    ArrangerTrackHeaderList(Song& song, MixChangedCallback onMixChanged);

    void updateFromModel();
    void setTrackHeight(int height);
    int trackHeight() const noexcept { return trackHeight_; }
    void setOnTrackRenamed(TrackRenamedCallback cb) { onTrackRenamed_ = std::move(cb); }
    void setOnDeleteTrack(DeleteTrackCallback cb) { onDeleteTrack_ = std::move(cb); }
    void setOnMoveTrack(MoveTrackCallback cb) { onMoveTrack_ = std::move(cb); }
    void setOnAddTrack(AddTrackCallback cb) { onAddTrack_ = std::move(cb); }

    std::function<void(const juce::MouseEvent&, const juce::MouseWheelDetails&)> onMouseWheel;
    std::function<void(const juce::MouseEvent&)> onMarqueeStart;
    std::function<void(const juce::MouseEvent&)> onMarqueeDrag;
    std::function<void(const juce::MouseEvent&)> onMarqueeEnd;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;

    // DragAndDropTarget
    bool isInterestedInDragSource(const SourceDetails& dragSourceDetails) override;
    void itemDragEnter(const SourceDetails& dragSourceDetails) override;
    void itemDragMove(const SourceDetails& dragSourceDetails) override;
    void itemDragExit(const SourceDetails& dragSourceDetails) override;
    void itemDropped(const SourceDetails& dragSourceDetails) override;

    static constexpr int kHeaderWidth = 260;

private:
    struct TrackControls
    {
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<juce::TextButton> deleteButton;
        std::unique_ptr<DAWMuteButton> muteButton;
        std::unique_ptr<DAWSoloButton> soloButton;
        std::unique_ptr<DAWArmButton> armButton;
        std::unique_ptr<juce::Label> volLabel;
        std::unique_ptr<juce::Slider> volumeSlider;
        std::unique_ptr<juce::Label> panLabel;
        std::unique_ptr<juce::Slider> panSlider;
    };

    int pointToTrack(juce::Point<int> pt) const;

    Song& song_;
    MixChangedCallback onMixChanged_;
    TrackRenamedCallback onTrackRenamed_;
    DeleteTrackCallback onDeleteTrack_;
    MoveTrackCallback onMoveTrack_;
    AddTrackCallback onAddTrack_;
    int trackHeight_ = 44;
    int dragHoverTrack_ = -1;

    std::unique_ptr<juce::TextButton> addTrackButton_;
    std::array<TrackControls, kArrangerTracks> controls_;
};

} // namespace trami
