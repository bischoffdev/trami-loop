#pragma once

#include "engine/AudioData.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <vector>

namespace trami
{

enum class RecordState
{
    Idle,
    CountingIn,
    Recording
};

class AudioRecorder
{
public:
    using RecordFinishedCallback = std::function<void(AudioData recordedAudio)>;

    AudioRecorder();

    RecordState state() const noexcept { return state_.load(); }
    bool isRecording() const noexcept { return state_.load() == RecordState::Recording; }
    bool isCountingIn() const noexcept { return state_.load() == RecordState::CountingIn; }

    /// Starts a recording session with countInBars before capture begins.
    void start(int countInSamples, int latencySamples, RecordFinishedCallback onFinished);

    /// Stops recording, applies latency compensation, and triggers completion callback.
    void stop();

    /// Called on the audio thread with incoming microphone / input samples.
    void processInput(const float* const* inputs, int numChannels, int numSamples) noexcept;

    /// Called on the audio thread with current playback sample count to advance count-in.
    void advanceSamples(int numSamples) noexcept;

private:
    std::atomic<RecordState> state_{RecordState::Idle};
    std::atomic<int> countInRemaining_{0};
    std::atomic<int> latencySamples_{0};

    mutable std::mutex mutex_;
    std::vector<std::vector<float>> recordedChannels_;
    RecordFinishedCallback onFinished_;
};

} // namespace trami
