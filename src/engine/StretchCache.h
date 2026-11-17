#pragma once

#include "engine/AudioData.h"
#include "engine/SnapshotBuilder.h"
#include "model/Ids.h"
#include "model/Song.h"

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <tuple>
#include <unordered_map>

namespace trami
{

/// Manages raw sample buffers and maintains a cache of time-stretched versions
/// for loops matching the current song tempo.
class StretchCache
{
public:
    StretchCache() = default;
    ~StretchCache();

    StretchCache(const StretchCache&) = delete;
    StretchCache& operator=(const StretchCache&) = delete;

    /// Stores raw audio for a sample.
    void setRawAudio(SampleId id, std::shared_ptr<const AudioData> audio);
    std::shared_ptr<const AudioData> getRawAudio(SampleId id) const;
    SampleBank getRawBank() const;
    void removeSample(SampleId id);
    void invalidateSample(SampleId id);
    void clear();

    /// Synchronously retrieves or generates the active SampleBank for the given song.
    SampleBank getOrUpdateSync(const Song& song, double sampleRate);

    /// Asynchronously renders stretched loops on a worker thread, then invokes
    /// `onComplete` with the updated SampleBank. If a new request arrives before
    /// the previous finishes, the previous job is cancelled.
    using CompletionCallback = std::function<void(SampleBank)>;
    void requestUpdateAsync(const Song& song, double sampleRate, CompletionCallback onComplete);

    /// Cancels and joins any active background worker.
    void cancelPending();

    /// Retrieves or renders stretched and/or pitch-shifted audio for a specific sample and semitone transposition.
    std::shared_ptr<const AudioData> getAudio(const Sample& sample, double targetBpm, double sampleRate, int semitones = 0,
                                             std::optional<SampleKind> kindOverride = std::nullopt,
                                             std::optional<int> loopBarsOverride = std::nullopt);

private:
    std::shared_ptr<const AudioData> getOrRender(const Sample& sample, double targetBpm, double sampleRate, int semitones = 0,
                                                 std::optional<SampleKind> kindOverride = std::nullopt,
                                                 std::optional<int> loopBarsOverride = std::nullopt);

    mutable std::mutex mutex_;
    std::unordered_map<SampleId, std::shared_ptr<const AudioData>> rawBank_;

    // Key: (SampleId, semitones, roundedBpm x 10, loopBars, sampleRateInt)
    using CacheKey = std::tuple<std::uint32_t, int, int, int, int>;
    std::map<CacheKey, std::shared_ptr<const AudioData>> cache_;

    std::thread workerThread_;
    std::atomic<bool> cancelRequested_{false};
    std::atomic<std::uint64_t> currentJobId_{0};
};

} // namespace trami
