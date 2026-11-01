#pragma once

#include "engine/AudioEngine.h"
#include "engine/SnapshotBuilder.h"
#include "engine/StretchCache.h"
#include "model/Song.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace trami
{

/// Temporary developer screen for milestone 2 & 3: plays a built-in demo beat
/// with drums and a time-stretched bass loop to verify the engine and time-stretcher.
class DevTransportComponent : public juce::Component, private juce::Timer
{
public:
    explicit DevTransportComponent(AudioEngine& engine);

    void paint(juce::Graphics& g) override;
    bool keyPressed(const juce::KeyPress& key) override;

private:
    void buildDemoSong();
    void updateBankAndSnapshot();
    void timerCallback() override { repaint(); }

    AudioEngine& engine_;
    Song song_;
    StretchCache stretchCache_;
    SampleBank activeBank_;
    PatternId pattern_;
    bool isStretching_ = false;
};

} // namespace trami
