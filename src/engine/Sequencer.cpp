#include "engine/Sequencer.h"

#include "engine/SnapshotBuilder.h"
#include "engine/Synth.h"
#include "model/Timing.h"

#include <algorithm>
#include <cmath>

namespace trami
{

Sequencer::Sequencer(SnapshotExchange& exchange, double sampleRate)
    : exchange_(exchange), click_(synth::click(sampleRate, 1000.0)), accentClick_(synth::click(sampleRate, 1600.0))
{
    resetTrackMix();
}

Sequencer::~Sequencer()
{
    delete pendingPreview_.exchange(nullptr);
    delete currentPreview_;
    currentPreview_ = nullptr;
    collectPreviewGarbage();

    delete current_;
    for (auto* s : retiringSnapshots_)
        delete s;
}

void Sequencer::setTrackMix(int track, float volume, float pan, bool muted, bool solo) noexcept
{
    if (track < 0 || track >= kMaxTracks)
        return;
    trackMix_[static_cast<std::size_t>(track)].volume.store(volume, std::memory_order_relaxed);
    trackMix_[static_cast<std::size_t>(track)].pan.store(pan, std::memory_order_relaxed);
    trackMix_[static_cast<std::size_t>(track)].muted.store(muted, std::memory_order_relaxed);
    trackMix_[static_cast<std::size_t>(track)].solo.store(solo, std::memory_order_relaxed);

    bool any = false;
    for (int t = 0; t < kMaxTracks; ++t)
    {
        if (trackMix_[static_cast<std::size_t>(t)].solo.load(std::memory_order_relaxed))
        {
            any = true;
            break;
        }
    }
    anySolo_.store(any, std::memory_order_relaxed);
    updateAllVoiceGains();
}

void Sequencer::setTrackVolume(int track, float volume) noexcept
{
    if (track < 0 || track >= kMaxTracks)
        return;
    trackMix_[static_cast<std::size_t>(track)].volume.store(volume, std::memory_order_relaxed);
    updateVoiceGainsForTrack(track);
}

void Sequencer::setTrackPan(int track, float pan) noexcept
{
    if (track < 0 || track >= kMaxTracks)
        return;
    trackMix_[static_cast<std::size_t>(track)].pan.store(pan, std::memory_order_relaxed);
    updateVoiceGainsForTrack(track);
}

void Sequencer::setTrackMuted(int track, bool muted) noexcept
{
    if (track < 0 || track >= kMaxTracks)
        return;
    trackMix_[static_cast<std::size_t>(track)].muted.store(muted, std::memory_order_relaxed);
    if (muted && trackMix_[static_cast<std::size_t>(track)].solo.load(std::memory_order_relaxed))
    {
        trackMix_[static_cast<std::size_t>(track)].solo.store(false, std::memory_order_relaxed);
        bool any = false;
        for (int t = 0; t < kMaxTracks; ++t)
        {
            if (trackMix_[static_cast<std::size_t>(t)].solo.load(std::memory_order_relaxed))
            {
                any = true;
                break;
            }
        }
        anySolo_.store(any, std::memory_order_relaxed);
    }
    updateAllVoiceGains();
}

void Sequencer::setTrackSolo(int track, bool solo) noexcept
{
    if (track < 0 || track >= kMaxTracks)
        return;
    trackMix_[static_cast<std::size_t>(track)].solo.store(solo, std::memory_order_relaxed);
    if (solo)
        trackMix_[static_cast<std::size_t>(track)].muted.store(false, std::memory_order_relaxed);

    bool any = false;
    for (int t = 0; t < kMaxTracks; ++t)
    {
        if (trackMix_[static_cast<std::size_t>(t)].solo.load(std::memory_order_relaxed))
        {
            any = true;
            break;
        }
    }
    anySolo_.store(any, std::memory_order_relaxed);
    updateAllVoiceGains();
}

void Sequencer::resetTrackMix() noexcept
{
    for (int t = 0; t < kMaxTracks; ++t)
    {
        trackMix_[static_cast<std::size_t>(t)].volume.store(1.0f, std::memory_order_relaxed);
        trackMix_[static_cast<std::size_t>(t)].pan.store(0.0f, std::memory_order_relaxed);
        trackMix_[static_cast<std::size_t>(t)].muted.store(false, std::memory_order_relaxed);
        trackMix_[static_cast<std::size_t>(t)].solo.store(false, std::memory_order_relaxed);
    }
    anySolo_.store(false, std::memory_order_relaxed);
    updateAllVoiceGains();
}

void Sequencer::syncTrackMixFromSnapshot() noexcept
{
    if (current_ == nullptr || !current_->hasTrackMixes)
        return;

    for (std::size_t t = 0; t < current_->trackMixes.size() && t < static_cast<std::size_t>(kMaxTracks); ++t)
    {
        const auto& m = current_->trackMixes[t];
        trackMix_[t].volume.store(m.volume, std::memory_order_relaxed);
        trackMix_[t].pan.store(m.pan, std::memory_order_relaxed);
        trackMix_[t].muted.store(m.muted, std::memory_order_relaxed);
        trackMix_[t].solo.store(m.solo, std::memory_order_relaxed);
    }

    bool any = false;
    for (int t = 0; t < kMaxTracks; ++t)
    {
        if (trackMix_[static_cast<std::size_t>(t)].solo.load(std::memory_order_relaxed))
        {
            any = true;
            break;
        }
    }
    anySolo_.store(any, std::memory_order_relaxed);
    updateAllVoiceGains();
}

void Sequencer::updateVoiceGainsForTrack(int track) noexcept
{
    if (track < 0 || track >= kMaxTracks)
        return;

    const bool muted = trackMix_[static_cast<std::size_t>(track)].muted.load(std::memory_order_relaxed);
    const bool solo = trackMix_[static_cast<std::size_t>(track)].solo.load(std::memory_order_relaxed);
    const bool anySolo = anySolo_.load(std::memory_order_relaxed);
    const float vol = trackMix_[static_cast<std::size_t>(track)].volume.load(std::memory_order_relaxed);
    const float pan = trackMix_[static_cast<std::size_t>(track)].pan.load(std::memory_order_relaxed);
    const auto [pL, pR] = SnapshotBuilder::panGains(pan);

    for (auto& v : voices_)
    {
        if (v.active && v.track == track)
        {
            if (muted || (anySolo && !solo))
            {
                v.gainLeft = 0.0f;
                v.gainRight = 0.0f;
            }
            else
            {
                v.gainLeft = v.baseGainLeft * vol * pL;
                v.gainRight = v.baseGainRight * vol * pR;
            }
        }
    }
}

void Sequencer::updateAllVoiceGains() noexcept
{
    for (int t = 0; t < kMaxTracks; ++t)
        updateVoiceGainsForTrack(t);
}

void Sequencer::triggerPreview(std::shared_ptr<const AudioData> audio)
{
    collectPreviewGarbage();
    if (audio == nullptr || audio->numFrames() == 0)
        return;

    auto* item = new PreviewItem{std::move(audio)};
    if (PreviewItem* prev = pendingPreview_.exchange(item, std::memory_order_acq_rel))
        delete prev;
}

void Sequencer::collectPreviewGarbage()
{
    while (true)
    {
        const auto read = retiredRead_.load(std::memory_order_relaxed);
        if (read == retiredWrite_.load(std::memory_order_acquire))
            break;

        PreviewItem* item = retiredPreviews_[read];
        retiredRead_.store((read + 1) % kRetiredQueueCapacity, std::memory_order_release);
        delete item;
    }
}

void Sequencer::retirePreview(PreviewItem* item) noexcept
{
    if (item == nullptr)
        return;

    const auto write = retiredWrite_.load(std::memory_order_relaxed);
    const auto nextWrite = (write + 1) % kRetiredQueueCapacity;
    if (nextWrite != retiredRead_.load(std::memory_order_acquire))
    {
        retiredPreviews_[write] = item;
        retiredWrite_.store(nextWrite, std::memory_order_release);
    }
}

void Sequencer::handlePreviewRequests() noexcept
{
    if (stopPreviewRequested_.exchange(false, std::memory_order_acq_rel))
    {
        previewVoice_.active = false;
        if (currentPreview_ != nullptr)
        {
            retirePreview(currentPreview_);
            currentPreview_ = nullptr;
        }
        previewPlaying_.store(false, std::memory_order_release);
    }

    if (PreviewItem* next = pendingPreview_.exchange(nullptr, std::memory_order_acq_rel))
    {
        if (currentPreview_ != nullptr)
            retirePreview(currentPreview_);

        currentPreview_ = next;
        previewVoice_ = Voice{currentPreview_->audio.get(), nullptr, 0, currentPreview_->audio->numFrames(),
                               0, 0.85f, 0.85f, 0.85f, 0.85f, true, -1};
        previewPlaying_.store(true, std::memory_order_release);
    }
}

void Sequencer::renderPreview(float* left, float* right, int numSamples) noexcept
{
    if (!previewVoice_.active || previewVoice_.audio == nullptr)
        return;

    const float* srcL = previewVoice_.audio->left();
    const float* srcR = previewVoice_.audio->right();
    const std::int64_t frames = previewVoice_.audio->numFrames();

    for (int i = 0; i < numSamples; ++i)
    {
        if (previewVoice_.readPosition >= frames)
        {
            previewVoice_.active = false;
            if (currentPreview_ != nullptr)
            {
                retirePreview(currentPreview_);
                currentPreview_ = nullptr;
            }
            previewPlaying_.store(false, std::memory_order_release);
            break;
        }

        left[i] += srcL[previewVoice_.readPosition] * previewVoice_.gainLeft;
        right[i] += srcR[previewVoice_.readPosition] * previewVoice_.gainRight;
        ++previewVoice_.readPosition;
    }
}

void Sequencer::process(float* left, float* right, int numSamples) noexcept
{
    adoptPendingSnapshot();
    handleTransportRequests();
    handlePreviewRequests();

    if (playing_.load() && current_ != nullptr && current_->lengthSamples > 0)
    {
        int offset = 0;
        while (offset < numSamples)
        {
            const auto chunk = std::min<std::int64_t>(numSamples - offset, current_->lengthSamples - position_);
            scheduleRange(position_, position_ + chunk, offset);
            scheduleMetronome(position_, position_ + chunk, offset);
            offset += static_cast<int>(chunk);
            position_ += chunk;

            if (position_ >= current_->lengthSamples)
            {
                position_ = 0;
                if (!current_->looping)
                {
                    playing_.store(false);
                    playRequested_.store(false);
                    break;
                }
            }
        }
    }

    renderVoices(left, right, numSamples);
    renderPreview(left, right, numSamples);
    publishedPosition_.store(position_);
}

void Sequencer::adoptPendingSnapshot() noexcept
{
    for (auto& s : retiringSnapshots_)
    {
        if (s != nullptr && !hasVoicesFrom(s))
        {
            if (exchange_.tryRetire(s))
                s = nullptr;
        }
    }

    bool hasRoom = false;
    for (auto* s : retiringSnapshots_)
    {
        if (s == nullptr)
        {
            hasRoom = true;
            break;
        }
    }

    if (!hasRoom)
        return;

    if (PlaybackSnapshot* next = exchange_.takePending())
    {
        if (current_ != nullptr)
        {
            for (auto& s : retiringSnapshots_)
            {
                if (s == nullptr)
                {
                    s = current_;
                    break;
                }
            }
        }
        current_ = next;
        if (current_->lengthSamples > 0)
            position_ %= current_->lengthSamples;
        syncTrackMixFromSnapshot();
    }
}

void Sequencer::handleTransportRequests() noexcept
{
    if (rewindRequested_.exchange(false))
    {
        position_ = 0;
        stopAllVoices();
    }

    const bool wanted = playRequested_.load();
    if (wanted != playing_.load())
    {
        playing_.store(wanted);
        if (!wanted)
            stopAllVoices();
    }
}

void Sequencer::scheduleRange(std::int64_t from, std::int64_t to, int blockOffset) noexcept
{
    const auto& events = current_->events;
    auto it = std::lower_bound(events.begin(), events.end(), from,
                               [](const PlaybackEvent& e, std::int64_t pos) { return e.startSample < pos; });

    for (; it != events.end() && it->startSample < to; ++it)
        startVoice(it->audio, current_, blockOffset + static_cast<int>(it->startSample - from), it->gainLeft,
                   it->gainRight, it->audioOffset, it->maxFrames, it->track);
}

void Sequencer::scheduleMetronome(std::int64_t from, std::int64_t to, int blockOffset) noexcept
{
    const double perBeat = current_->samplesPerBeat;
    if (!metronomeEnabled_.load() || perBeat <= 0.0)
        return;

    auto beat = static_cast<std::int64_t>(std::ceil(static_cast<double>(from) / perBeat));
    for (;; ++beat)
    {
        const auto at = std::llround(static_cast<double>(beat) * perBeat);
        if (at < from)
            continue;
        if (at >= to)
            break;
        const AudioData* sound = (beat % kBeatsPerBar == 0) ? &accentClick_ : &click_;
        startVoice(sound, nullptr, blockOffset + static_cast<int>(at - from), 0.5f, 0.5f);
    }
}

void Sequencer::startVoice(const AudioData* audio, const PlaybackSnapshot* owner, int delay, float gainL,
                           float gainR, std::int64_t audioOffset, std::int64_t maxFrames, int track) noexcept
{
    if (audio == nullptr || audio->numFrames() == 0)
        return;

    const std::int64_t totalFrames = audio->numFrames();
    const std::int64_t startFrame = std::clamp<std::int64_t>(audioOffset, 0, totalFrames);
    const std::int64_t endFrame = (maxFrames >= 0)
        ? std::clamp<std::int64_t>(startFrame + maxFrames, startFrame, totalFrames)
        : totalFrames;

    Voice* target = &voices_.front();
    for (auto& voice : voices_)
    {
        if (!voice.active)
        {
            target = &voice;
            break;
        }
        if (voice.readPosition > target->readPosition)
            target = &voice;
    }

    float effL = gainL;
    float effR = gainR;

    if (track >= 0 && track < kMaxTracks)
    {
        const bool muted = trackMix_[static_cast<std::size_t>(track)].muted.load(std::memory_order_relaxed);
        const bool solo = trackMix_[static_cast<std::size_t>(track)].solo.load(std::memory_order_relaxed);
        const bool anySolo = anySolo_.load(std::memory_order_relaxed);

        if (muted || (anySolo && !solo))
        {
            effL = 0.0f;
            effR = 0.0f;
        }
        else
        {
            const float vol = trackMix_[static_cast<std::size_t>(track)].volume.load(std::memory_order_relaxed);
            const float pan = trackMix_[static_cast<std::size_t>(track)].pan.load(std::memory_order_relaxed);
            const auto [pL, pR] = SnapshotBuilder::panGains(pan);
            effL = gainL * vol * pL;
            effR = gainR * vol * pR;
        }
    }

    *target = Voice{audio, owner, startFrame, endFrame, delay, gainL, gainR, effL, effR, true, track};
}

void Sequencer::renderVoices(float* left, float* right, int numSamples) noexcept
{
    for (auto& voice : voices_)
    {
        if (!voice.active)
            continue;

        if (voice.gainLeft == 0.0f && voice.gainRight == 0.0f)
        {
            const int advance = std::min<int>(numSamples - voice.startDelay,
                                              static_cast<int>(voice.endPosition - voice.readPosition));
            if (advance > 0)
                voice.readPosition += advance;
            if (voice.readPosition >= voice.endPosition)
            {
                voice.active = false;
                voice.owner = nullptr;
            }
            voice.startDelay = 0;
            continue;
        }

        const float* srcL = voice.audio->left();
        const float* srcR = voice.audio->right();

        for (int i = voice.startDelay; i < numSamples; ++i)
        {
            if (voice.readPosition >= voice.endPosition)
            {
                voice.active = false;
                voice.owner = nullptr;
                break;
            }
            left[i] += srcL[voice.readPosition] * voice.gainLeft;
            right[i] += srcR[voice.readPosition] * voice.gainRight;
            ++voice.readPosition;
        }
        voice.startDelay = 0;
    }
}

void Sequencer::stopAllVoices() noexcept
{
    for (auto& voice : voices_)
    {
        voice.active = false;
        voice.owner = nullptr;
    }
}

bool Sequencer::hasVoicesFrom(const PlaybackSnapshot* snapshot) const noexcept
{
    return std::any_of(voices_.begin(), voices_.end(),
                       [snapshot](const Voice& v) { return v.active && v.owner == snapshot; });
}

} // namespace trami
