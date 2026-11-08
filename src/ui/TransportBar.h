#pragma once

#include "engine/AudioEngine.h"
#include "model/Song.h"
#include "ui/IconButton.h"
#include "ui/TramiLookAndFeel.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace trami
{

class TransportBar : public juce::Component, private juce::Timer
{
public:
    using ActionCallback = std::function<void()>;
    using ValueCallback = std::function<void()>;

    TransportBar(Song& song, AudioEngine& engine,
                 ActionCallback onModelModified,
                 ActionCallback onRecordToggle);
    ~TransportBar() override;

    void setLooping(bool looping);
    bool isLooping() const noexcept { return isLooping_; }

    void paint(juce::Graphics& g) override;
    void resized() override;

    void updateDisplay();

    static constexpr int kHeight = 58;

private:
    void timerCallback() override;

    Song& song_;
    AudioEngine& engine_;
    ActionCallback onModelModified_;
    ActionCallback onRecordToggle_;

    bool isLooping_ = true;

    IconButton rewindButton_{"Rewind", IconButton::Icon::Rewind, "Rewind"};
    IconButton playButton_{"Play", IconButton::Icon::Play, "Play"};
    IconButton recordButton_{"Rec", IconButton::Icon::Record, "Rec"};
    IconButton loopButton_{"Loop", IconButton::Icon::Loop, "Loop"};
    IconButton metroButton_{"Click", IconButton::Icon::Metronome, "Click"};

    juce::Label timeLabel_;

    juce::TextButton tempoDown_{"-"};
    juce::Label tempoLabel_;
    juce::TextButton tempoUp_{"+"};

    juce::Label swingLabel_{"Swing", "Swing:"};
    juce::Slider swingSlider_;
};

} // namespace trami

