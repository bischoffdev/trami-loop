#include "engine/StretchCache.h"

#include "engine/TimeStretcher.h"

#include <cmath>

namespace trami
{

StretchCache::~StretchCache()
{
    cancelPending();
}

void StretchCache::setRawAudio(SampleId id, std::shared_ptr<const AudioData> audio)
{
    std::lock_guard<std::mutex> lock(mutex_);
    rawBank_[id] = std::move(audio);
}

std::shared_ptr<const AudioData> StretchCache::getRawAudio(SampleId id) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = rawBank_.find(id);
    return it != rawBank_.end() ? it->second : nullptr;
}

SampleBank StretchCache::getRawBank() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return rawBank_;
}

void StretchCache::removeSample(SampleId id)
{
    std::lock_guard<std::mutex> lock(mutex_);
    rawBank_.erase(id);
    for (auto it = cache_.begin(); it != cache_.end();)
    {
        if (std::get<0>(it->first) == id.value)
            it = cache_.erase(it);
        else
            ++it;
    }
}

void StretchCache::clear()
{
    std::lock_guard<std::mutex> lock(mutex_);
    rawBank_.clear();
    cache_.clear();
}

SampleBank StretchCache::getOrUpdateSync(const Song& song, double sampleRate)
{
    SampleBank bank;
    for (const auto& sample : song.samples())
    {
        auto audio = getOrRender(sample, song.bpm(), sampleRate);
        if (audio != nullptr)
            bank[sample.id] = std::move(audio);
    }
    return bank;
}

void StretchCache::cancelPending()
{
    cancelRequested_.store(true);
    if (workerThread_.joinable())
        workerThread_.join();
    cancelRequested_.store(false);
}

void StretchCache::requestUpdateAsync(const Song& song, double sampleRate, CompletionCallback onComplete)
{
    cancelPending();
    const std::uint64_t jobId = ++currentJobId_;

    workerThread_ = std::thread([this, song, sampleRate, onComplete = std::move(onComplete), jobId]() {
        SampleBank bank;
        for (const auto& sample : song.samples())
        {
            if (cancelRequested_.load() || jobId != currentJobId_.load())
                return;

            auto audio = getOrRender(sample, song.bpm(), sampleRate);
            if (audio != nullptr)
                bank[sample.id] = std::move(audio);
        }

        if (cancelRequested_.load() || jobId != currentJobId_.load())
            return;

        if (onComplete)
            onComplete(std::move(bank));
    });
}

std::shared_ptr<const AudioData> StretchCache::getOrRender(const Sample& sample, double targetBpm, double sampleRate)
{
    std::shared_ptr<const AudioData> raw;
    CacheKey key{sample.id.value, static_cast<int>(std::round(targetBpm * 10.0)), sample.loopBars,
                 static_cast<int>(std::round(sampleRate))};

    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto rawIt = rawBank_.find(sample.id);
        if (rawIt == rawBank_.end() || rawIt->second == nullptr)
            return nullptr;
        raw = rawIt->second;

        // One-shots are never stretched
        if (sample.kind == SampleKind::OneShot)
            return raw;

        // Loops at original tempo don't need stretching
        const double origBpm = sample.originalBpm();
        if (origBpm <= 0.0 || std::abs(origBpm - targetBpm) < 0.001)
            return raw;

        // Check if already in cache
        auto cacheIt = cache_.find(key);
        if (cacheIt != cache_.end())
            return cacheIt->second;
    }

    // Render stretch outside the lock
    const double origBpm = sample.originalBpm();
    const double ratio = origBpm / targetBpm;
    AudioData stretched = TimeStretcher::stretch(*raw, ratio, sampleRate);
    auto rendered = std::make_shared<const AudioData>(std::move(stretched));

    {
        std::lock_guard<std::mutex> lock(mutex_);
        cache_[key] = rendered;
    }

    return rendered;
}

} // namespace trami
