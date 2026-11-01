#include "engine/AudioRecorder.h"

#include <algorithm>

namespace trami
{

AudioRecorder::AudioRecorder() = default;

void AudioRecorder::start(int countInSamples, int latencySamples, RecordFinishedCallback onFinished)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        recordedChannels_.clear();
        onFinished_ = std::move(onFinished);
    }

    latencySamples_.store(std::max(0, latencySamples));
    if (countInSamples > 0)
    {
        countInRemaining_.store(countInSamples);
        state_.store(RecordState::CountingIn);
    }
    else
    {
        countInRemaining_.store(0);
        state_.store(RecordState::Recording);
    }
}

void AudioRecorder::stop()
{
    RecordFinishedCallback callback;
    AudioData result;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        state_.store(RecordState::Idle);

        if (!recordedChannels_.empty())
        {
            const int latency = latencySamples_.load();
            result.channels.resize(recordedChannels_.size());

            for (std::size_t ch = 0; ch < recordedChannels_.size(); ++ch)
            {
                const auto& raw = recordedChannels_[ch];
                if (static_cast<int>(raw.size()) > latency)
                {
                    result.channels[ch].assign(raw.begin() + latency, raw.end());
                }
            }
        }

        recordedChannels_.clear();
        callback = std::move(onFinished_);
    }

    if (callback && result.numFrames() > 0)
        callback(std::move(result));
}

void AudioRecorder::advanceSamples(int numSamples) noexcept
{
    if (state_.load() == RecordState::CountingIn)
    {
        const int remaining = countInRemaining_.fetch_sub(numSamples) - numSamples;
        if (remaining <= 0)
            state_.store(RecordState::Recording);
    }
}

void AudioRecorder::processInput(const float* const* inputs, int numChannels, int numSamples) noexcept
{
    if (state_.load() != RecordState::Recording || inputs == nullptr || numChannels <= 0 || numSamples <= 0)
        return;

    // Use try_lock to avoid blocking the audio thread if UI is calling stop()
    std::unique_lock<std::mutex> lock(mutex_, std::try_to_lock);
    if (!lock.owns_lock())
        return;

    if (recordedChannels_.size() < static_cast<std::size_t>(numChannels))
        recordedChannels_.resize(static_cast<std::size_t>(numChannels));

    for (int ch = 0; ch < numChannels; ++ch)
    {
        if (inputs[ch] != nullptr)
        {
            auto& chData = recordedChannels_[static_cast<std::size_t>(ch)];
            chData.insert(chData.end(), inputs[ch], inputs[ch] + numSamples);
        }
    }
}

} // namespace trami
