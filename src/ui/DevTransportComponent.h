#pragma once

#include "engine/AudioEngine.h"
#include "engine/SnapshotBuilder.h"
#include "model/Song.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace trami
{

/// Temporary developer screen for milestone 2: plays a built-in demo beat so
/// the engine can be heard. Replaced by the pattern view in milestone 4.
class DevTransportComponent : public juce::Component, private juce::Timer
{
public:
    explicit DevTransportComponent(AudioEngine& engine);

    void paint(juce::Graphics& g) override;
    bool keyPressed(const juce::KeyPress& key) override;

private:
    void buildDemoSong();
    void republish();
    void timerCallback() override { repaint(); }

    AudioEngine& engine_;
    Song song_;
    SampleBank bank_;
    PatternId pattern_;
};

} // namespace trami
