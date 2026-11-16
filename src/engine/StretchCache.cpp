#include "engine/StretchCache.h"

#include "engine/TimeStretcher.h"
#include "model/Timing.h"

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
    std::unordered_map<std::uint32_t, SampleKind> effectiveKinds;
    std::unordered_map<std::uint32_t, int> effectiveBars;
    for (const auto& sample : song.samples())
    {
        effectiveKinds[sample.id.value] = sample.kind;
        effectiveBars[sample.id.value] = sample.loopBars;
    }
    for (const auto& pat : song.patterns())
    {
        for (std::size_t t = 0; t < pat.tracks.size(); ++t)
        {
            const auto kind = song.patternTrackKind(pat.id, static_cast<int>(t));
            const auto sid = song.patternTrackSample(pat.id, static_cast<int>(t));
            if (sid.isValid() && kind == SampleKind::Loop)
            {
                effectiveKinds[sid.value] = SampleKind::Loop;
                if (const auto* s = song.findSample(sid))
                {
                    if (s->loopBars > 0)
                        effectiveBars[sid.value] = s->loopBars;
                    else
                    {
                        const double secPerBar = (song.bpm() > 0.0) ? (60.0 / song.bpm()) * 4.0 : 2.0;
                        effectiveBars[sid.value] = std::max(1, static_cast<int>(std::round(s->durationSeconds / secPerBar)));
                    }
                }
            }
        }
    }

    SampleBank bank;
    for (const auto& sample : song.samples())
    {
        auto audio = getOrRender(sample, song.bpm(), sampleRate, 0,
                                 effectiveKinds[sample.id.value], effectiveBars[sample.id.value]);
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

std::shared_ptr<const AudioData> StretchCache::getAudio(const Sample& sample, double targetBpm, double sampleRate, int semitones,
                                                        std::optional<SampleKind> kindOverride, std::optional<int> loopBarsOverride)
{
    return getOrRender(sample, targetBpm, sampleRate, semitones, kindOverride, loopBarsOverride);
}

void StretchCache::requestUpdateAsync(const Song& song, double sampleRate, CompletionCallback onComplete)
{
    cancelPending();
    const std::uint64_t jobId = ++currentJobId_;

    workerThread_ = std::thread([this, song, sampleRate, onComplete = std::move(onComplete), jobId]() {
        std::unordered_map<std::uint32_t, SampleKind> effectiveKinds;
        std::unordered_map<std::uint32_t, int> effectiveBars;
        for (const auto& sample : song.samples())
        {
            effectiveKinds[sample.id.value] = sample.kind;
            effectiveBars[sample.id.value] = sample.loopBars;
        }
        for (const auto& pat : song.patterns())
        {
            for (std::size_t t = 0; t < pat.tracks.size(); ++t)
            {
                const auto kind = song.patternTrackKind(pat.id, static_cast<int>(t));
                const auto sid = song.patternTrackSample(pat.id, static_cast<int>(t));
                if (sid.isValid() && kind == SampleKind::Loop)
                {
                    effectiveKinds[sid.value] = SampleKind::Loop;
                    if (const auto* s = song.findSample(sid))
                    {
                        if (s->loopBars > 0)
                            effectiveBars[sid.value] = s->loopBars;
                        else
                        {
                            const double secPerBar = (song.bpm() > 0.0) ? (60.0 / song.bpm()) * 4.0 : 2.0;
                            effectiveBars[sid.value] = std::max(1, static_cast<int>(std::round(s->durationSeconds / secPerBar)));
                        }
                    }
                }
            }
        }

        SampleBank bank;
        for (const auto& sample : song.samples())
        {
            if (cancelRequested_.load() || jobId != currentJobId_.load())
                return;

            auto audio = getOrRender(sample, song.bpm(), sampleRate, 0,
                                     effectiveKinds[sample.id.value], effectiveBars[sample.id.value]);
            if (audio != nullptr)
                bank[sample.id] = std::move(audio);
        }

        // Also pre-cache any transposed clips in the song
        for (const auto& pat : song.patterns())
        {
            for (std::size_t t = 0; t < pat.tracks.size(); ++t)
            {
                const auto kind = song.patternTrackKind(pat.id, static_cast<int>(t));
                const auto& trk = pat.tracks[t];
                for (const auto& c : trk.clips)
                {
                    if (c.transposeSemitones != 0)
                    {
                        if (const auto* s = song.findSample(c.sampleId))
                        {
                            if (cancelRequested_.load() || jobId != currentJobId_.load())
                                return;
                            getOrRender(*s, song.bpm(), sampleRate, c.transposeSemitones, kind, effectiveBars[s->id.value]);
                        }
                    }
                }
            }
        }

        if (cancelRequested_.load() || jobId != currentJobId_.load())
            return;

        if (onComplete)
            onComplete(std::move(bank));
    });
}

std::shared_ptr<const AudioData> StretchCache::getOrRender(const Sample& sample, double targetBpm, double sampleRate, int semitones,
                                                           std::optional<SampleKind> kindOverride, std::optional<int> loopBarsOverride)
{
    const SampleKind effectiveKind = kindOverride.value_or(sample.kind);
    int effectiveBars = loopBarsOverride.value_or(sample.loopBars);
    if (effectiveKind == SampleKind::Loop && effectiveBars <= 0)
    {
        const double secPerBar = (targetBpm > 0.0) ? (60.0 / targetBpm) * 4.0 : 2.0;
        effectiveBars = (secPerBar > 0.0 && sample.durationSeconds > 0.0)
            ? std::max(1, static_cast<int>(std::round(sample.durationSeconds / secPerBar)))
            : 1;
    }

    std::shared_ptr<const AudioData> raw;
    const int bpmKey = (effectiveKind == SampleKind::OneShot) ? 0 : static_cast<int>(std::round(targetBpm * 10.0));
    CacheKey key{sample.id.value, semitones, bpmKey,
                 (effectiveKind == SampleKind::Loop ? effectiveBars : 0),
                 static_cast<int>(std::round(sampleRate))};

    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto rawIt = rawBank_.find(sample.id);
        if (rawIt == rawBank_.end() || rawIt->second == nullptr)
            return nullptr;
        raw = rawIt->second;

        // One-shots with 0 transpose don't need stretching or pitching
        if (effectiveKind == SampleKind::OneShot && semitones == 0)
            return raw;

        // Loops at original tempo with 0 transpose don't need stretching or pitching
        double origBpm = sample.acidBpm;
        if (origBpm <= 0.0 && effectiveBars > 0 && sample.durationSeconds > 0.0)
            origBpm = Timing::loopBpm(sample.durationSeconds, effectiveBars);

        const bool tempoMatches = (origBpm <= 0.0 || std::abs(origBpm - targetBpm) < 0.001);
        if (effectiveKind == SampleKind::Loop && semitones == 0 && tempoMatches)
            return raw;

        // Check if already in cache
        auto cacheIt = cache_.find(key);
        if (cacheIt != cache_.end())
            return cacheIt->second;
    }

    // Render stretch & pitch outside the lock
    double ratio = 1.0;
    if (effectiveKind == SampleKind::Loop)
    {
        double origBpm = sample.acidBpm;
        if (origBpm <= 0.0 && effectiveBars > 0 && sample.durationSeconds > 0.0)
            origBpm = Timing::loopBpm(sample.durationSeconds, effectiveBars);

        if (origBpm > 0.0 && targetBpm > 0.0)
            ratio = origBpm / targetBpm;
    }
    const double pitchScale = std::pow(2.0, static_cast<double>(semitones) / 12.0);
    AudioData resultAudio = (effectiveKind == SampleKind::OneShot)
        ? TimeStretcher::resample(*raw, pitchScale)
        : TimeStretcher::stretch(*raw, ratio, sampleRate, pitchScale);
    auto rendered = std::make_shared<const AudioData>(std::move(resultAudio));

    {
        std::lock_guard<std::mutex> lock(mutex_);
        cache_[key] = rendered;
    }

    return rendered;
}

} // namespace trami
