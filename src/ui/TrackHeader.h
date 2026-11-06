#pragma once

#include "model/Ids.h"
#include "model/Song.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <functional>

namespace trami
{

class TrackHeaderList : public juce::Component,
                        public juce::FileDragAndDropTarget,
                        public juce::DragAndDropTarget
{
public:
    using MixChangedCallback = std::function<void(int track)>;
    using TrackSelectedCallback = std::function<void(int track, SampleId sampleId)>;
    using FileDroppedCallback = std::function<void(const juce::String& filePath, int track)>;

    TrackHeaderList(Song& song, PatternId patternId, MixChangedCallback onMixChanged);

    void setPattern(PatternId patternId);
    void updateFromModel();
    void setTrackHeight(int height);
    int trackHeight() const noexcept { return trackHeight_; }

    void setOnTrackSelected(TrackSelectedCallback cb) { onTrackSelected_ = std::move(cb); }
    void setOnFileDropped(FileDroppedCallback cb) { onFileDropped_ = std::move(cb); }

    std::function<void(const juce::MouseEvent&, const juce::MouseWheelDetails&)> onMouseWheel;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    void mouseDown(const juce::MouseEvent& e) override;

    // FileDragAndDropTarget
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

    // DragAndDropTarget
    bool isInterestedInDragSource(const SourceDetails& dragSourceDetails) override;
    void itemDropped(const SourceDetails& dragSourceDetails) override;

    static constexpr int kHeaderWidth = 260;

private:
    struct TrackControls
    {
        std::unique_ptr<juce::Label> nameLabel;
        std::unique_ptr<juce::TextButton> muteButton;
        std::unique_ptr<juce::TextButton> soloButton;
        std::unique_ptr<juce::Slider> volumeSlider;
        std::unique_ptr<juce::Slider> panSlider;
    };

    int pointToTrack(juce::Point<int> pt) const;

    Song& song_;
    PatternId patternId_;
    MixChangedCallback onMixChanged_;
    TrackSelectedCallback onTrackSelected_;
    FileDroppedCallback onFileDropped_;
    int trackHeight_ = 36;

    std::array<TrackControls, kTracksPerPattern> controls_;
};

} // namespace trami
