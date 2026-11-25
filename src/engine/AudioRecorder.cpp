#include "engine/AudioRecorder.h"

#include <algorithm>
#include <cmath>

namespace trami
{

AudioRecorder::AudioRecorder()
{
    clearInputPeaks();
}

void AudioRecorder::start(int countInSamples, int latencySamples, RecordFinishedCallback onFinished,
                          int inputChannel, bool stereo)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        recordedChannels_.clear();
        onFinished_ = std::move(onFinished);
    }

    inputChannel_.store(std::max(0, inputChannel), std::memory_order_relaxed);
    inputStereo_.store(stereo, std::memory_order_relaxed);
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
    if (inputs == nullptr || numChannels <= 0 || numSamples <= 0)
    {
        activeInputChannels_.store(0, std::memory_order_relaxed);
        return;
    }

    const int clampedChannels = std::min(numChannels, kMaxInputChannels);
    activeInputChannels_.store(clampedChannels, std::memory_order_relaxed);

    // Measure live per-channel input peaks for real-time track input metering (even when not actively recording)
    for (int ch = 0; ch < clampedChannels; ++ch)
    {
        float blockPeak = 0.0f;
        const float* src = inputs[ch];
        if (src != nullptr)
        {
            for (int i = 0; i < numSamples; ++i)
            {
                const float a = std::abs(src[i]);
                if (a > blockPeak)
                    blockPeak = a;
            }
        }
        const float prev = inputPeaks_[static_cast<std::size_t>(ch)].load(std::memory_order_relaxed);
        const float decayed = prev * 0.90f;
        const float next = (blockPeak >= decayed) ? blockPeak : (decayed < 0.0005f ? 0.0f : decayed);
        inputPeaks_[static_cast<std::size_t>(ch)].store(next, std::memory_order_relaxed);
    }
    for (int ch = clampedChannels; ch < kMaxInputChannels; ++ch)
        inputPeaks_[static_cast<std::size_t>(ch)].store(0.0f, std::memory_order_relaxed);

    if (state_.load() != RecordState::Recording)
        return;

    // Use try_lock to avoid blocking the audio thread if UI is calling stop()
    std::unique_lock<std::mutex> lock(mutex_, std::try_to_lock);
    if (!lock.owns_lock())
        return;

    const int baseCh = std::clamp(inputChannel_.load(std::memory_order_relaxed), 0, numChannels - 1);
    const bool stereo = inputStereo_.load(std::memory_order_relaxed);
    const std::size_t targetChannels = stereo ? 2u : 1u;

    if (recordedChannels_.size() != targetChannels)
        recordedChannels_.resize(targetChannels);

    if (!stereo)
    {
        auto& chData = recordedChannels_[0];
        if (inputs[baseCh] != nullptr)
            chData.insert(chData.end(), inputs[baseCh], inputs[baseCh] + numSamples);
        else
            chData.insert(chData.end(), static_cast<std::size_t>(numSamples), 0.0f);
    }
    else
    {
        const int rightCh = (baseCh + 1 < numChannels) ? (baseCh + 1) : baseCh;
        const int srcCh[2] = {baseCh, rightCh};
        for (std::size_t ch = 0; ch < 2u; ++ch)
        {
            auto& chData = recordedChannels_[ch];
            const float* src = inputs[srcCh[ch]];
            if (src != nullptr)
                chData.insert(chData.end(), src, src + numSamples);
            else
                chData.insert(chData.end(), static_cast<std::size_t>(numSamples), 0.0f);
        }
    }
}

float AudioRecorder::channelPeakLevel(int channel) const noexcept
{
    if (channel < 0 || channel >= kMaxInputChannels)
        return 0.0f;
    return inputPeaks_[static_cast<std::size_t>(channel)].load(std::memory_order_relaxed);
}

std::pair<float, float> AudioRecorder::inputPeakLevels(int inputChannel, bool stereo) const noexcept
{
    const int numIn = activeInputChannels_.load(std::memory_order_relaxed);
    if (numIn <= 0)
        return {0.0f, 0.0f};

    const int chL = std::clamp(inputChannel, 0, std::min(kMaxInputChannels - 1, numIn - 1));
    const float pL = inputPeaks_[static_cast<std::size_t>(chL)].load(std::memory_order_relaxed);
    if (!stereo)
        return {pL, pL};

    const int chR = (chL + 1 < numIn && chL + 1 < kMaxInputChannels) ? (chL + 1) : chL;
    const float pR = inputPeaks_[static_cast<std::size_t>(chR)].load(std::memory_order_relaxed);
    return {pL, pR};
}

void AudioRecorder::clearInputPeaks() noexcept
{
    activeInputChannels_.store(0, std::memory_order_relaxed);
    for (auto& p : inputPeaks_)
        p.store(0.0f, std::memory_order_relaxed);
}

} // namespace trami
