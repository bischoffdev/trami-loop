#pragma once

#include "engine/AudioEngine.h"
#include "engine/SnapshotBuilder.h"
#include "engine/StretchCache.h"
#include "model/Song.h"
#include "ui/ArrangerGrid.h"
#include "ui/ArrangerTrackHeader.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace trami
{

class ArrangerView : public juce::Component, public ArrangerGrid::Listener, private juce::Timer
{
public:
    using EditPatternCallback = std::function<void(PatternId patternId)>;

    ArrangerView(Song& song, AudioEngine& engine, StretchCache& stretchCache,
                 EditPatternCallback onEditPattern);
    ~ArrangerView() override;

    void updatePatternList();
    void republish();
    void toggleRecording();
    void onRecordFinished(AudioData recorded);

    void paint(juce::Graphics& g) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;

    // ArrangerGrid::Listener
    void blockClicked(BlockId blockId) override;
    void blockDoubleClicked(PatternId patternId) override;
    void blockMoved(BlockId blockId, int newTrack, int newBar) override;
    void blockDeleteRequested(BlockId blockId) override;
    void gridCellClicked(int track, int bar) override;

private:
    void timerCallback() override;

    Song& song_;
    AudioEngine& engine_;
    StretchCache& stretchCache_;
    EditPatternCallback onEditPattern_;

    PatternId activePattern_;
    SampleBank activeBank_;

    // Toolbar controls
    juce::ComboBox patternCombo_;
    juce::TextButton addPatternButton_{"+ Pattern"};
    juce::TextButton duplicatePatternButton_{"Duplicate"};
    juce::TextButton deletePatternButton_{"Delete"};
    juce::TextButton editPatternButton_{"Edit in Pattern View"};

    juce::TextButton playButton_{"Play Song"};
    juce::TextButton recordButton_{"Rec"};
    juce::TextButton rewindButton_{"|<<"};
    juce::ToggleButton metroToggle_{"Click"};
    juce::Label tempoLabel_;
    juce::TextButton tempoDown_{"-"};
    juce::TextButton tempoUp_{"+"};

    std::unique_ptr<ArrangerTrackHeaderList> trackHeaders_;
    std::unique_ptr<ArrangerGrid> grid_;
    std::unique_ptr<juce::Viewport> gridViewport_;
};

} // namespace trami
