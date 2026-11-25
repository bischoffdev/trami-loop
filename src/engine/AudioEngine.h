#pragma once

#include "engine/AudioRecorder.h"
#include "engine/PluginHost.h"
#include "engine/Sequencer.h"
#include "engine/SnapshotExchange.h"

#include <juce_audio_devices/juce_audio_devices.h>

#include <memory>

namespace trami
{

/// Owns the audio device and connects it to the Sequencer and AudioRecorder.
class AudioEngine : private juce::AudioIODeviceCallback, private juce::Timer
{
public:
    AudioEngine();
    ~AudioEngine() override;

    juce::String start();

    double sampleRate() const noexcept { return sampleRate_; }
    int inputLatencySamples() const noexcept;
    int outputLatencySamples() const noexcept;
    int recordingLatencySamples() const noexcept;

    /// Returns names of active hardware input channels on the current audio input device (empty if none).
    juce::StringArray inputChannelNames() const;
    /// Returns the name of the currently active audio input device (empty if none).
    juce::String inputDeviceName() const;
    int numInputChannels() const noexcept;

    /// Returns live (L, R) input peak levels for the specified 0-based `inputChannel` and `stereo` mode.
    std::pair<float, float> inputPeakLevels(int inputChannel, bool stereo) const noexcept;

    /// Returns combined (L, R) meter levels for `trackIndex`, reflecting live input level when the track is armed.
    std::pair<float, float> trackMeterLevels(const PatternTrack& track, int trackIndex) const noexcept;

    struct PerformanceMetrics
    {
        float cpuLoad = 0.0f;       ///< Real-time audio callback CPU load [0.0 .. 1.0]
        float secondaryLoad = 0.0f; ///< Voice / Disk / Peak DSP load [0.0 .. 1.0]
        int activeVoices = 0;       ///< Number of currently playing voices
        int pdcLatencySamples = 0;  ///< Total active Plugin Delay Compensation latency in samples
    };
    PerformanceMetrics performanceMetrics() const noexcept;

    Sequencer& sequencer() noexcept { return *sequencer_; }
    AudioRecorder& recorder() noexcept { return recorder_; }
    PluginHost& pluginHost() noexcept { return pluginHost_; }
    juce::AudioDeviceManager& deviceManager() noexcept { return deviceManager_; }

    void publish(std::unique_ptr<PlaybackSnapshot> snapshot) { exchange_.publish(std::move(snapshot)); }

private:
    void audioDeviceIOCallbackWithContext(const float* const* inputs, int numInputs, float* const* outputs,
                                          int numOutputs, int numSamples,
                                          const juce::AudioIODeviceCallbackContext& context) override;
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;
    void timerCallback() override
    {
        exchange_.collectGarbage();
        if (sequencer_ != nullptr)
            sequencer_->collectPreviewGarbage();
    }

    juce::AudioDeviceManager deviceManager_;
    SnapshotExchange exchange_;
    PluginHost pluginHost_;
    double sampleRate_ = 0.0;
    std::unique_ptr<Sequencer> sequencer_;
    AudioRecorder recorder_;
    std::atomic<float> dspLoad_{0.0f};
    std::atomic<float> peakDspLoad_{0.0f};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};

} // namespace trami
