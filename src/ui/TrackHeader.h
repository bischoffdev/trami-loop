#pragma once

#include "model/Ids.h"
#include "model/Song.h"
#include "ui/DAWTrackControls.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <functional>

namespace trami
{

class TrackHeaderList : public juce::Component,
                        public juce::FileDragAndDropTarget,
                        public juce::DragAndDropTarget,
                        private juce::Timer
{
public:
    using MixChangedCallback = std::function<void(int track)>;
    using TrackSelectedCallback = std::function<void(int track, SampleId sampleId)>;
    using FileDroppedCallback = std::function<void(const juce::String& filePath, int track)>;
    using AddTrackCallback = std::function<void()>;
    using TrackTypeChangedCallback = std::function<void(int track, SampleKind newKind)>;
    using DuplicateTrackCallback = std::function<void(int track)>;
    using DeleteTrackCallback = std::function<void(int track)>;
    using MoveTrackCallback = std::function<void(int from, int to)>;
    using TrackRenamedCallback = std::function<void(int track, const std::string& name)>;
    using TrackPropertiesCallback = std::function<void(int track)>;
    using TrackFxCallback = std::function<void(int track)>;
    using TrackPeakProvider = std::function<std::pair<float, float>(int track)>;

    TrackHeaderList(Song& song, PatternId patternId, MixChangedCallback onMixChanged);
    ~TrackHeaderList() override;

    void setPattern(PatternId patternId);
    void updateFromModel();
    void setTrackHeight(int height);
    int trackHeight() const noexcept { return trackHeight_; }

    void setOnTrackSelected(TrackSelectedCallback cb) { onTrackSelected_ = std::move(cb); }
    void setOnFileDropped(FileDroppedCallback cb) { onFileDropped_ = std::move(cb); }
    void setOnAddTrack(AddTrackCallback cb) { onAddTrack_ = std::move(cb); }
    void setOnTrackTypeChanged(TrackTypeChangedCallback cb) { onTrackTypeChanged_ = std::move(cb); }
    void setOnDuplicateTrack(DuplicateTrackCallback cb) { onDuplicateTrack_ = std::move(cb); }
    void setOnDeleteTrack(DeleteTrackCallback cb) { onDeleteTrack_ = std::move(cb); }
    void setOnMoveTrack(MoveTrackCallback cb) { onMoveTrack_ = std::move(cb); }
    void setOnTrackRenamed(TrackRenamedCallback cb) { onTrackRenamed_ = std::move(cb); }
    void setOnTrackProperties(TrackPropertiesCallback cb) { onTrackProperties_ = std::move(cb); }
    void setOnTrackFx(TrackFxCallback cb) { onTrackFx_ = std::move(cb); }
    void setTrackPeakProvider(TrackPeakProvider cb);

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

    // FileDragAndDropTarget
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void fileDragEnter(const juce::StringArray& files, int x, int y) override;
    void fileDragMove(const juce::StringArray& files, int x, int y) override;
    void fileDragExit(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

    // DragAndDropTarget
    bool isInterestedInDragSource(const SourceDetails& dragSourceDetails) override;
    void itemDragEnter(const SourceDetails& dragSourceDetails) override;
    void itemDragMove(const SourceDetails& dragSourceDetails) override;
    void itemDragExit(const SourceDetails& dragSourceDetails) override;
    void itemDropped(const SourceDetails& dragSourceDetails) override;

    static constexpr int kHeaderWidth = 360;
    static constexpr int kMinHeaderWidth = 280;
    static constexpr int kMaxHeaderWidth = 560;
    static constexpr int kMinTrackHeight = 66;

private:
    class TrackSliderLookAndFeel;
    std::unique_ptr<TrackSliderLookAndFeel> sliderLnf_;
    struct TrackControls
    {
        std::unique_ptr<juce::Label> nameLabel;
        std::unique_ptr<juce::TextButton> fxButton;
        std::unique_ptr<juce::TextButton> propsButton;
        std::unique_ptr<juce::TextButton> typeButton;
        std::unique_ptr<juce::TextButton> deleteButton;
        std::unique_ptr<DAWMuteButton> muteButton;
        std::unique_ptr<DAWSoloButton> soloButton;
        std::unique_ptr<DAWArmButton> armButton;
        std::unique_ptr<juce::Label> volLabel;
        std::unique_ptr<juce::Slider> volumeSlider;
        std::unique_ptr<juce::Label> panLabel;
        std::unique_ptr<juce::Slider> panSlider;
    };

    void timerCallback() override;
    int pointToTrack(juce::Point<int> pt) const;

    Song& song_;
    PatternId patternId_;
    MixChangedCallback onMixChanged_;
    TrackSelectedCallback onTrackSelected_;
    FileDroppedCallback onFileDropped_;
    AddTrackCallback onAddTrack_;
    TrackTypeChangedCallback onTrackTypeChanged_;
    DuplicateTrackCallback onDuplicateTrack_;
    DeleteTrackCallback onDeleteTrack_;
    MoveTrackCallback onMoveTrack_;
    TrackRenamedCallback onTrackRenamed_;
    TrackPropertiesCallback onTrackProperties_;
    TrackFxCallback onTrackFx_;
    TrackPeakProvider trackPeakProvider_;
    std::vector<float> peakL_;
    std::vector<float> peakR_;
    int trackHeight_ = kMinTrackHeight;
    int dragHoverTrack_ = -1;
    bool isReorderDrag_ = false;

    std::vector<TrackControls> controls_;
    std::unique_ptr<juce::TextButton> addTrackButton_;
};

} // namespace trami
