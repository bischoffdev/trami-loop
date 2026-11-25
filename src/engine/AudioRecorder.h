#pragma once

#include "engine/AudioData.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <utility>
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
    static constexpr int kMaxInputChannels = 32;
    static constexpr int kLivePeakBucketSamples = 256;
    using RecordFinishedCallback = std::function<void(AudioData recordedAudio)>;

    struct AdvanceResult
    {
        bool countInFinished = false;
        int finishedOffset = 0;
        bool triggerClick = false;
        bool clickAccent = false;
        int clickOffset = 0;
    };

    AudioRecorder();

    RecordState state() const noexcept { return state_.load(); }
    bool isRecording() const noexcept { return state_.load() == RecordState::Recording; }
    bool isCountingIn() const noexcept { return state_.load() == RecordState::CountingIn; }

    void setPreCountEnabled(bool enabled) noexcept { preCountEnabled_.store(enabled, std::memory_order_relaxed); }
    bool isPreCountEnabled() const noexcept { return preCountEnabled_.load(std::memory_order_relaxed); }

    /// Starts a recording session with `countInSamples` before capture begins.
    /// `countInBeatSamples` specifies samples per beat for audible pre-count clicks (0 = no beat clicks).
    /// `inputChannel` selects the 0-based hardware input channel; `stereo` records 2 channels (ch, ch+1) when true or 1 mono channel (ch) when false.
    void start(int countInSamples, int latencySamples, RecordFinishedCallback onFinished,
               int inputChannel = 0, bool stereo = true, int countInBeatSamples = 0);

    int inputChannel() const noexcept { return inputChannel_.load(std::memory_order_relaxed); }
    bool isStereo() const noexcept { return inputStereo_.load(std::memory_order_relaxed); }

    /// 1-based pre-count beat currently playing (0 when not counting in).
    int currentCountInBeat() const noexcept;
    int countInTotalBeats() const noexcept { return countInTotalBeats_.load(std::memory_order_relaxed); }

    /// Live captured frame count and downsampled waveform peaks for rendering a growing recording clip.
    std::int64_t recordedFrames() const noexcept { return recordedFrames_.load(std::memory_order_relaxed); }
    std::vector<float> copyLiveWaveformPeaks() const;

    /// Stops recording, applies latency compensation, and triggers completion callback.
    void stop();

    /// Called on the audio thread with incoming microphone / input samples.
    /// Always updates live per-channel input peak meters (even while Idle/CountingIn),
    /// and captures audio into the recording buffer when state() == RecordState::Recording.
    void processInput(const float* const* inputs, int numChannels, int numSamples) noexcept;

    /// Called on the audio thread with current block sample count to advance pre-count and report click/start transitions.
    AdvanceResult advanceSamples(int numSamples) noexcept;

    /// Returns the live (left, right) input peak levels for the given 0-based `inputChannel`
    /// and `stereo` mode (when `stereo == false`, both left and right equal the mono channel peak).
    std::pair<float, float> inputPeakLevels(int inputChannel, bool stereo) const noexcept;

    /// Returns the live peak level [0..1+] of a single 0-based hardware input channel.
    float channelPeakLevel(int channel) const noexcept;

    /// Number of hardware input channels seen in the most recent audio callback.
    int activeInputChannels() const noexcept { return activeInputChannels_.load(std::memory_order_relaxed); }

    /// Resets all live input peak meters to zero.
    void clearInputPeaks() noexcept;

private:
    std::atomic<RecordState> state_{RecordState::Idle};
    std::atomic<bool> preCountEnabled_{false};
    std::atomic<int> countInTotalSamples_{0};
    std::atomic<int> countInElapsed_{0};
    std::atomic<int> countInBeatSamples_{0};
    std::atomic<int> countInTotalBeats_{0};
    std::atomic<int> latencySamples_{0};
    std::atomic<int> inputChannel_{0};
    std::atomic<bool> inputStereo_{true};
    std::atomic<int> activeInputChannels_{0};
    std::atomic<std::int64_t> recordedFrames_{0};
    std::array<std::atomic<float>, kMaxInputChannels> inputPeaks_{};

    mutable std::mutex mutex_;
    std::vector<std::vector<float>> recordedChannels_;
    std::vector<float> liveWaveformPeaks_;
    float currentBucketPeak_ = 0.0f;
    int currentBucketCount_ = 0;
    RecordFinishedCallback onFinished_;
};

} // namespace trami
