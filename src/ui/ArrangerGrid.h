#pragma once

#include "model/Ids.h"
#include "model/Song.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace trami
{

class ArrangerGrid : public juce::Component
{
public:
    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void blockClicked(BlockId blockId) = 0;
        virtual void blockDoubleClicked(PatternId patternId) = 0;
        virtual void blockMoved(BlockId blockId, int newTrack, int newBar) = 0;
        virtual void blockDeleteRequested(BlockId blockId) = 0;
        virtual void gridCellClicked(int track, int bar) = 0;
    };

    ArrangerGrid(const Song& song, Listener& listener);

    void setSelectedBlock(BlockId blockId);
    void setPlayheadBar(double bar);

    void paint(juce::Graphics& g) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;

    static constexpr int kRulerHeight = 28;
    static constexpr int kTrackHeight = 44;
    static constexpr int kBarWidth = 64;

private:
    struct DragState
    {
        BlockId blockId;
        int originalTrack = 0;
        int originalBar = 0;
        int currentTrack = 0;
        int currentBar = 0;
        bool active = false;
    };

    juce::Rectangle<int> blockBounds(int track, int bar, int lengthBars) const;
    std::pair<int, int> pointToTrackAndBar(juce::Point<int> pt) const;
    const PatternBlock* findBlockAt(juce::Point<int> pt) const;
    juce::Colour patternColour(PatternId id) const;

    const Song& song_;
    Listener& listener_;

    BlockId selectedBlock_;
    double playheadBar_ = -1.0;
    DragState drag_;
};

} // namespace trami
