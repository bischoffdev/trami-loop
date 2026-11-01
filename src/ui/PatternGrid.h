#pragma once

#include "model/Ids.h"
#include "model/Song.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace trami
{

class PatternGrid : public juce::Component, public juce::FileDragAndDropTarget
{
public:
    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void clipClicked(ClipId clipId) = 0;
        virtual void clipMoved(ClipId clipId, int newTrack, int newStep) = 0;
        virtual void clipDeleteRequested(ClipId clipId) = 0;
        virtual void gridCellClicked(int track, int step) = 0;
        virtual void fileDroppedOnGrid(const juce::String& filePath, int track, int step) = 0;
    };

    PatternGrid(const Song& song, PatternId patternId, Listener& listener);

    void setPattern(PatternId patternId);
    void setSelectedClip(ClipId clipId);
    void setPlayheadStep(double step);

    void paint(juce::Graphics& g) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;

    // FileDragAndDropTarget
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

    static constexpr int kRulerHeight = 28;
    static constexpr int kTrackHeight = 36;
    static constexpr int kStepWidth = 28;

private:
    struct DragState
    {
        ClipId clipId;
        int originalTrack = 0;
        int originalStep = 0;
        int currentTrack = 0;
        int currentStep = 0;
        bool active = false;
    };

    juce::Rectangle<int> clipBounds(int track, int step, int lengthSteps) const;
    std::pair<int, int> pointToTrackAndStep(juce::Point<int> pt) const;
    const Clip* findClipAt(juce::Point<int> pt) const;
    juce::Colour sampleColour(SampleId id) const;

    const Song& song_;
    PatternId patternId_;
    Listener& listener_;

    ClipId selectedClip_;
    double playheadStep_ = -1.0;
    DragState drag_;
};

} // namespace trami
