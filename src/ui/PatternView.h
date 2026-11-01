#pragma once

#include "engine/AudioEngine.h"
#include "engine/SnapshotBuilder.h"
#include "engine/StretchCache.h"
#include "model/Song.h"
#include "ui/PatternGrid.h"
#include "ui/TrackHeader.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace trami
{

class PatternView : public juce::Component, public PatternGrid::Listener, private juce::Timer
{
public:
    PatternView(Song& song, AudioEngine& engine, StretchCache& stretchCache);
    ~PatternView() override;

    void setPattern(PatternId patternId);
    void paint(juce::Graphics& g) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;

private:
    // PatternGrid::Listener
    void clipClicked(ClipId clipId) override;
    void clipMoved(ClipId clipId, int newTrack, int newStep) override;
    void clipDeleteRequested(ClipId clipId) override;
    void gridCellClicked(int track, int step) override;

    void updateSampleSelector();
    void updateLengthSelector();
    void republish();
    void timerCallback() override;

    Song& song_;
    AudioEngine& engine_;
    StretchCache& stretchCache_;
    PatternId currentPattern_;
    SampleId activeSample_;
    SampleBank activeBank_;

    // Toolbar controls
    juce::Label patternTitle_;
    juce::ComboBox lengthCombo_;
    juce::ComboBox sampleCombo_;
    juce::TextButton playButton_{"Play"};
    juce::TextButton rewindButton_{"|<<"};
    juce::ToggleButton metroToggle_{"Click"};
    juce::ToggleButton swingToggle_{"Swing"};
    juce::Label tempoLabel_;
    juce::TextButton tempoDown_{"-"};
    juce::TextButton tempoUp_{"+"};

    std::unique_ptr<TrackHeaderList> trackHeaders_;
    std::unique_ptr<PatternGrid> grid_;
    std::unique_ptr<juce::Viewport> gridViewport_;
};

} // namespace trami
