#pragma once

#include "engine/Sequencer.h"
#include "engine/SnapshotExchange.h"

#include <juce_audio_devices/juce_audio_devices.h>

#include <memory>

namespace trami
{

/// Owns the audio device and connects it to the Sequencer.
/// Created and used on the message thread; only the device callback runs on
/// the audio thread.
class AudioEngine : private juce::AudioIODeviceCallback, private juce::Timer
{
public:
    AudioEngine();
    ~AudioEngine() override;

    /// Opens the default output device. Returns an error message, or empty on success.
    juce::String start();

    double sampleRate() const noexcept { return sampleRate_; }
    Sequencer& sequencer() noexcept { return *sequencer_; }
    void publish(std::unique_ptr<PlaybackSnapshot> snapshot) { exchange_.publish(std::move(snapshot)); }

private:
    void audioDeviceIOCallbackWithContext(const float* const* inputs, int numInputs, float* const* outputs,
                                          int numOutputs, int numSamples,
                                          const juce::AudioIODeviceCallbackContext& context) override;
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override {}
    void timerCallback() override { exchange_.collectGarbage(); }

    juce::AudioDeviceManager deviceManager_;
    SnapshotExchange exchange_;
    std::unique_ptr<Sequencer> sequencer_;
    double sampleRate_ = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};

} // namespace trami
