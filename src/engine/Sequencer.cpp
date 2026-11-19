#include "engine/Sequencer.h"

#include "engine/SnapshotBuilder.h"
#include "engine/Synth.h"
#include "model/Timing.h"

#include <algorithm>
#include <cmath>

namespace trami
{

Sequencer::Sequencer(SnapshotExchange& exchange, double sampleRate)
    : exchange_(exchange)
    , click_(synth::click(sampleRate, 1000.0))
    , accentClick_(synth::click(sampleRate, 1600.0))
    , scratch_(std::make_unique<ScratchBuffers>())
{
    for (int t = 0; t < kMaxTracks; ++t)
    {
        trackPeakL_[static_cast<std::size_t>(t)].store(0.0f, std::memory_order_relaxed);
        trackPeakR_[static_cast<std::size_t>(t)].store(0.0f, std::memory_order_relaxed);
    }
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

void Sequencer::setMasterVolume(float volume) noexcept
{
    masterVolume_.store(std::clamp(volume, 0.0f, 2.0f), std::memory_order_relaxed);
}

Sequencer::OutputLevels Sequencer::outputLevels() const noexcept
{
    return {
        outPeakL_.load(std::memory_order_relaxed),
        outPeakR_.load(std::memory_order_relaxed),
        outRmsL_.load(std::memory_order_relaxed),
        outRmsR_.load(std::memory_order_relaxed),
        clipOccurred_.load(std::memory_order_relaxed)
    };
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

void Sequencer::triggerPreview(std::shared_ptr<const AudioData> audio, int startDelaySamples, bool loopPreview)
{
    collectPreviewGarbage();
    if (audio == nullptr || audio->numFrames() == 0)
        return;

    auto* item = new PreviewItem{std::move(audio), std::max(0, startDelaySamples), loopPreview};
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
        previewLooping_ = false;
        if (currentPreview_ != nullptr)
        {
            retirePreview(currentPreview_);
            currentPreview_ = nullptr;
        }
        previewPlaying_.store(false, std::memory_order_release);
        previewProgress_.store(0.0, std::memory_order_relaxed);
    }

    if (PreviewItem* next = pendingPreview_.exchange(nullptr, std::memory_order_acq_rel))
    {
        if (currentPreview_ != nullptr)
            retirePreview(currentPreview_);

        currentPreview_ = next;
        previewLooping_ = currentPreview_->loopPreview;
        previewVoice_ = Voice{currentPreview_->audio.get(), nullptr, 0, 0, currentPreview_->audio->numFrames(),
                              currentPreview_->startDelaySamples, 0.85f, 0.85f, 0.85f, 0.85f, true, -1, 0, 0, 0};
        previewPlaying_.store(true, std::memory_order_release);
        previewProgress_.store(0.0, std::memory_order_relaxed);
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
        if (previewVoice_.startDelay > 0)
        {
            --previewVoice_.startDelay;
            continue;
        }

        if (previewVoice_.readPosition >= frames)
        {
            if (previewLooping_ && frames > 0)
            {
                previewVoice_.readPosition = 0;
            }
            else
            {
                previewVoice_.active = false;
                if (currentPreview_ != nullptr)
                {
                    retirePreview(currentPreview_);
                    currentPreview_ = nullptr;
                }
                previewPlaying_.store(false, std::memory_order_release);
                previewProgress_.store(0.0, std::memory_order_relaxed);
                break;
            }
        }

        left[i] += srcL[previewVoice_.readPosition] * previewVoice_.gainLeft;
        right[i] += srcR[previewVoice_.readPosition] * previewVoice_.gainRight;
        ++previewVoice_.readPosition;
    }

    if (previewVoice_.active && frames > 0)
    {
        previewProgress_.store(static_cast<double>(previewVoice_.readPosition) / static_cast<double>(frames),
                               std::memory_order_relaxed);
    }
}

void Sequencer::process(float* left, float* right, int numSamples) noexcept
{
    adoptPendingSnapshot();
    handleTransportRequests();
    handlePreviewRequests();

    if (playing_.load() && current_ != nullptr && current_->lengthSamples > 0)
    {
        const bool isLooping = loopingEnabled_.load();
        const std::int64_t loopStart = (current_->loopEndSample > current_->loopStartSample && current_->loopStartSample >= 0)
                                          ? current_->loopStartSample : 0;
        const std::int64_t loopEnd = (current_->loopEndSample > current_->loopStartSample && current_->loopEndSample <= current_->lengthSamples)
                                        ? current_->loopEndSample : current_->lengthSamples;

        int offset = 0;
        while (offset < numSamples)
        {
            const std::int64_t segEnd = (isLooping && loopEnd > loopStart) ? loopEnd : current_->lengthSamples;
            if (position_ >= segEnd)
            {
                if (isLooping && loopEnd > loopStart)
                {
                    stopTrackVoicesAt(offset);
                    position_ = loopStart;
                }
                else
                {
                    position_ = 0;
                    playing_.store(false);
                    playRequested_.store(false);
                    break;
                }
            }

            const auto chunk = std::min<std::int64_t>(numSamples - offset, segEnd - position_);
            if (chunk <= 0)
            {
                if (isLooping && loopEnd > loopStart)
                {
                    stopTrackVoicesAt(offset);
                    position_ = loopStart;
                    continue;
                }
                else
                {
                    position_ = 0;
                    playing_.store(false);
                    playRequested_.store(false);
                    break;
                }
            }

            scheduleRange(position_, position_ + chunk, offset);
            scheduleMetronome(position_, position_ + chunk, offset);
            offset += static_cast<int>(chunk);
            position_ += chunk;

            if (position_ >= segEnd)
            {
                if (isLooping && loopEnd > loopStart)
                {
                    stopTrackVoicesAt(offset);
                    position_ = loopStart;
                }
                else
                {
                    position_ = 0;
                    if (!isLooping)
                    {
                        playing_.store(false);
                        playRequested_.store(false);
                        break;
                    }
                }
            }
        }
    }

    renderVoices(left, right, numSamples);
    renderPreview(left, right, numSamples);

    // Apply master volume ramp
    const float targetVol = masterVolume_.load(std::memory_order_relaxed);
    if (std::abs(currentMasterVol_ - targetVol) < 0.0001f)
    {
        currentMasterVol_ = targetVol;
        if (currentMasterVol_ != 1.0f)
        {
            for (int i = 0; i < numSamples; ++i)
            {
                left[i] *= currentMasterVol_;
                right[i] *= currentMasterVol_;
            }
        }
    }
    else
    {
        const float step = (targetVol - currentMasterVol_) / static_cast<float>(numSamples);
        for (int i = 0; i < numSamples; ++i)
        {
            currentMasterVol_ += step;
            left[i] *= currentMasterVol_;
            right[i] *= currentMasterVol_;
        }
        currentMasterVol_ = targetVol;
    }

    // Measure overall loudness (peak & RMS)
    float peakL = 0.0f;
    float peakR = 0.0f;
    float sumSqL = 0.0f;
    float sumSqR = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        const float aL = std::abs(left[i]);
        const float aR = std::abs(right[i]);
        if (aL > peakL) peakL = aL;
        if (aR > peakR) peakR = aR;
        sumSqL += left[i] * left[i];
        sumSqR += right[i] * right[i];
    }

    outPeakL_.store(peakL, std::memory_order_relaxed);
    outPeakR_.store(peakR, std::memory_order_relaxed);
    const float denom = static_cast<float>(std::max(1, numSamples));
    outRmsL_.store(std::sqrt(sumSqL / denom), std::memory_order_relaxed);
    outRmsR_.store(std::sqrt(sumSqR / denom), std::memory_order_relaxed);

    if (peakL >= 1.0f || peakR >= 1.0f)
        clipOccurred_.store(true, std::memory_order_relaxed);

    renderMetronome(left, right, numSamples);

    publishedPosition_.store(position_);
}

std::pair<float, float> Sequencer::trackPeakLevels(int track) const noexcept
{
    if (track < 0 || track >= kMaxTracks)
        return {0.0f, 0.0f};
    return {
        trackPeakL_[static_cast<std::size_t>(track)].load(std::memory_order_relaxed),
        trackPeakR_[static_cast<std::size_t>(track)].load(std::memory_order_relaxed)
    };
}

void Sequencer::adoptPendingSnapshot() noexcept
{
    if (!playing_.load(std::memory_order_relaxed) || stopVoicesRequested_.load(std::memory_order_relaxed))
        stopAllVoices();

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

        // Real-time synchronization of active voices with the newly adopted snapshot:
        if (playing_.load(std::memory_order_relaxed))
        {
            for (auto& voice : voices_)
            {
                if (!voice.active || voice.clipId == 0)
                    continue;

                // Find the PlaybackEvent for this clipId that actually spans current playback position_
                // (looped clips emit multiple events sharing the same clipId).
                const PlaybackEvent* matchingEv = nullptr;
                for (const auto& ev : current_->events)
                {
                    if (ev.clipId != 0 && ev.clipId == voice.clipId)
                    {
                        const std::int64_t evEnd = ev.startSample +
                            (ev.maxFrames >= 0 ? ev.maxFrames : (ev.audio ? ev.audio->numFrames() : 0));
                        if (position_ >= ev.startSample && position_ < evEnd)
                        {
                            matchingEv = &ev;
                            break;
                        }
                    }
                }

                if (matchingEv == nullptr)
                {
                    // Clip was removed or moved away from active playback: silence voice immediately
                    voice.active = false;
                    voice.owner = nullptr;
                }
                else
                {
                    // Clip edited/reversed/transposed/moved while still spanning current playback position: resync voice
                    voice.track = matchingEv->track;
                    voice.owner = current_;
                    voice.audio = matchingEv->audio;
                    voice.reversed = matchingEv->reversed;
                    voice.startPosition = matchingEv->audioOffset;
                    voice.endPosition = (matchingEv->maxFrames >= 0 && matchingEv->audio != nullptr)
                        ? std::min<std::int64_t>(matchingEv->audioOffset + matchingEv->maxFrames, matchingEv->audio->numFrames())
                        : (matchingEv->audio ? static_cast<std::int64_t>(matchingEv->audio->numFrames()) : 0);
                    voice.fadeInSamples = matchingEv->fadeInSamples;
                    voice.fadeOutSamples = matchingEv->fadeOutSamples;
                    voice.baseGainLeft = matchingEv->gainLeft;
                    voice.baseGainRight = matchingEv->gainRight;
                    voice.hasClipFx = matchingEv->hasClipFx;
                    voice.fxSend = matchingEv->fxSend;
                    voice.volEnv = matchingEv->volEnv;
                    voice.volEnvCount = matchingEv->volEnvCount;
                    voice.panEnv = matchingEv->panEnv;
                    voice.panEnvCount = matchingEv->panEnvCount;
                    const std::int64_t expectedOffset = (position_ - matchingEv->startSample) + matchingEv->audioOffset;
                    if (matchingEv->audio != nullptr)
                        voice.readPosition = std::clamp<std::int64_t>(expectedOffset, 0, matchingEv->audio->numFrames());
                }
            }

            // Recompute effective voice gains (track volume/pan/mute/solo * updated baseGain)
            updateAllVoiceGains();

            // If a clip was placed, pasted, or dragged directly onto the current playback position, start it
            for (const auto& ev : current_->events)
            {
                if (ev.clipId == 0 || ev.audio == nullptr)
                    continue;

                const std::int64_t totalFrames = ev.audio->numFrames();
                const std::int64_t evEnd = ev.startSample +
                    (ev.maxFrames >= 0 ? ev.maxFrames : totalFrames);

                if (position_ >= ev.startSample && position_ < evEnd)
                {
                    bool voicePlaying = false;
                    for (const auto& v : voices_)
                    {
                        if (v.active && v.clipId == ev.clipId)
                        {
                            voicePlaying = true;
                            break;
                        }
                    }

                    if (!voicePlaying)
                    {
                        const std::int64_t offsetInClip = (position_ - ev.startSample) + ev.audioOffset;
                        const std::int64_t remainingFrames = evEnd - position_;
                        if (offsetInClip < totalFrames && remainingFrames > 0)
                        {
                            const std::int64_t fullEventFrames = (ev.maxFrames >= 0) ? ev.maxFrames : (totalFrames - ev.audioOffset);
                            startVoice(ev.audio, current_, 0, ev.gainLeft, ev.gainRight,
                                       ev.audioOffset, fullEventFrames, ev.track,
                                       ev.fadeInSamples, ev.fadeOutSamples, ev.clipId, ev.reversed, &ev);
                            // Advance readPosition to the exact seek offset while preserving startPosition for envelope timing
                            for (auto& v : voices_)
                            {
                                if (v.active && v.clipId == ev.clipId && v.audio == ev.audio)
                                {
                                    v.readPosition = std::clamp<std::int64_t>(offsetInClip, 0, totalFrames);
                                    break;
                                }
                            }
                        }
                    }
                }
            }

            updateAllVoiceGains();
        }
    }
}

void Sequencer::requestSeek(std::int64_t samplePosition) noexcept
{
    const auto pos = std::max<std::int64_t>(0, samplePosition);
    seekRequestedPosition_.store(pos);
    seekRequested_.store(true);
    if (!playing_.load())
    {
        publishedPosition_.store(pos);
    }
}

void Sequencer::handleTransportRequests() noexcept
{
    const bool stopRequested = stopVoicesRequested_.exchange(false);

    if (rewindRequested_.exchange(false))
    {
        position_ = 0;
        publishedPosition_.store(0);
        stopAllVoices();
        if (playing_.load())
            chaseActiveEvents(position_);
    }

    if (seekRequested_.exchange(false))
    {
        const auto target = seekRequestedPosition_.load();
        position_ = std::max<std::int64_t>(0, target);
        if (current_ != nullptr && current_->lengthSamples > 0)
            position_ = std::min(position_, current_->lengthSamples);
        publishedPosition_.store(position_);
        stopAllVoices();
        if (playing_.load())
            chaseActiveEvents(position_);
    }

    const bool wanted = playRequested_.load();
    const bool wasPlaying = playing_.load();

    if (stopRequested || !wanted)
    {
        stopAllVoices();
        playing_.store(false);
    }
    else if (wanted && !wasPlaying)
    {
        playing_.store(true);
        chaseActiveEvents(position_);
    }
}

void Sequencer::chaseActiveEvents(std::int64_t pos) noexcept
{
    if (current_ == nullptr || current_->events.empty())
        return;

    for (const auto& ev : current_->events)
    {
        if (ev.audio == nullptr || ev.audio->numFrames() == 0)
            continue;

        const std::int64_t totalFrames = ev.audio->numFrames();
        const std::int64_t evEnd = ev.startSample +
            (ev.maxFrames >= 0 ? ev.maxFrames : totalFrames);

        if (pos > ev.startSample && pos < evEnd)
        {
            const std::int64_t offsetInClip = (pos - ev.startSample) + ev.audioOffset;
            const std::int64_t remainingFrames = evEnd - pos;
            if (offsetInClip < totalFrames && remainingFrames > 0)
            {
                startVoice(ev.audio, current_, 0, ev.gainLeft, ev.gainRight,
                           offsetInClip, remainingFrames, ev.track,
                           ev.fadeInSamples, ev.fadeOutSamples, ev.clipId, ev.reversed, &ev);
            }
        }
    }
}

void Sequencer::scheduleRange(std::int64_t from, std::int64_t to, int blockOffset) noexcept
{
    const auto& events = current_->events;
    auto it = std::lower_bound(events.begin(), events.end(), from,
                               [](const PlaybackEvent& e, std::int64_t pos) { return e.startSample < pos; });

    for (; it != events.end() && it->startSample < to; ++it)
        startVoice(it->audio, current_, blockOffset + static_cast<int>(it->startSample - from), it->gainLeft,
                   it->gainRight, it->audioOffset, it->maxFrames, it->track, it->fadeInSamples, it->fadeOutSamples,
                   it->clipId, it->reversed, &(*it));
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
        startMetronomeVoice(sound, blockOffset + static_cast<int>(at - from), 0.5f, 0.5f);
    }
}

void Sequencer::startMetronomeVoice(const AudioData* audio, int delay, float gainL, float gainR) noexcept
{
    if (audio == nullptr || audio->numFrames() == 0)
        return;

    Voice* target = &metronomeVoices_.front();
    for (auto& voice : metronomeVoices_)
    {
        if (!voice.active)
        {
            target = &voice;
            break;
        }
        if (voice.readPosition > target->readPosition)
            target = &voice;
    }

    *target = Voice{audio, nullptr, 0, 0, audio->numFrames(), delay, gainL, gainR, gainL, gainR, true, -1, 0, 0, 0, false};
}

void Sequencer::renderMetronome(float* left, float* right, int numSamples) noexcept
{
    if (!playing_.load(std::memory_order_relaxed) && !metronomeEnabled_.load(std::memory_order_relaxed))
    {
        for (auto& voice : metronomeVoices_)
            voice.active = false;
        return;
    }

    for (auto& voice : metronomeVoices_)
    {
        if (!voice.active || voice.audio == nullptr)
            continue;

        const float* srcL = voice.audio->left();
        const float* srcR = voice.audio->right();
        const std::int64_t frames = voice.audio->numFrames();

        for (int i = voice.startDelay; i < numSamples; ++i)
        {
            if (voice.readPosition >= frames)
            {
                voice.active = false;
                break;
            }

            left[i] += srcL[voice.readPosition] * voice.gainLeft * currentMasterVol_;
            right[i] += srcR[voice.readPosition] * voice.gainRight * currentMasterVol_;
            ++voice.readPosition;
        }
        voice.startDelay = 0;
    }
}

void Sequencer::startVoice(const AudioData* audio, const PlaybackSnapshot* owner, int delay, float gainL,
                           float gainR, std::int64_t audioOffset, std::int64_t maxFrames, int track,
                           std::int64_t fadeInSamples, std::int64_t fadeOutSamples, std::uint32_t clipId,
                           bool reversed, const PlaybackEvent* srcEvent) noexcept
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

    *target = Voice{audio, owner, startFrame, startFrame, endFrame, delay, gainL, gainR, effL, effR, true, track,
                    std::max<std::int64_t>(0, fadeInSamples), std::max<std::int64_t>(0, fadeOutSamples), clipId, reversed};
    if (srcEvent != nullptr)
    {
        target->hasClipFx = srcEvent->hasClipFx;
        target->fxSend = srcEvent->fxSend;
        target->volEnv = srcEvent->volEnv;
        target->volEnvCount = srcEvent->volEnvCount;
        target->panEnv = srcEvent->panEnv;
        target->panEnvCount = srcEvent->panEnvCount;
    }
}

void Sequencer::renderVoices(float* left, float* right, int numSamples) noexcept
{
    if (!playing_.load(std::memory_order_relaxed))
    {
        for (int t = 0; t < kMaxTracks; ++t)
        {
            trackPeakL_[static_cast<std::size_t>(t)].store(0.0f, std::memory_order_relaxed);
            trackPeakR_[static_cast<std::size_t>(t)].store(0.0f, std::memory_order_relaxed);
        }
        return;
    }

    const int n = std::min(numSamples, kMaxScratchSamples);
    AudioPluginProcessor* pluginProc = pluginProcessor_.load(std::memory_order_acquire);

    std::array<bool, kMaxTracks> trackUsed{};

    for (auto& voice : voices_)
    {
        if (!voice.active)
            continue;

        if (voice.gainLeft == 0.0f && voice.gainRight == 0.0f)
        {
            const int advance = std::min<int>(n - voice.startDelay,
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
        const float invSpan = (voice.endPosition > voice.startPosition)
            ? (1.0f / static_cast<float>(voice.endPosition - voice.startPosition))
            : 0.0f;

        const bool validTrack = (voice.track >= 0 && voice.track < kMaxTracks);
        if (validTrack && !trackUsed[static_cast<std::size_t>(voice.track)])
        {
            std::fill_n(scratch_->trackBufL[static_cast<std::size_t>(voice.track)].data(), n, 0.0f);
            std::fill_n(scratch_->trackBufR[static_cast<std::size_t>(voice.track)].data(), n, 0.0f);
            trackUsed[static_cast<std::size_t>(voice.track)] = true;
        }

        const bool useClipFx = (pluginProc != nullptr && voice.hasClipFx && voice.clipId != 0);
        if (useClipFx)
        {
            std::fill_n(scratch_->clipBufL.data(), n, 0.0f);
            std::fill_n(scratch_->clipBufR.data(), n, 0.0f);
        }

        float* destL = useClipFx
            ? scratch_->clipBufL.data()
            : (validTrack ? scratch_->trackBufL[static_cast<std::size_t>(voice.track)].data() : left);
        float* destR = useClipFx
            ? scratch_->clipBufR.data()
            : (validTrack ? scratch_->trackBufR[static_cast<std::size_t>(voice.track)].data() : right);

        for (int i = voice.startDelay; i < n; ++i)
        {
            if (voice.readPosition >= voice.endPosition)
            {
                voice.active = false;
                voice.owner = nullptr;
                break;
            }

            float env = 1.0f;
            const std::int64_t offsetFromStart = voice.readPosition - voice.startPosition;
            if (voice.fadeInSamples > 0 && offsetFromStart < voice.fadeInSamples)
                env *= static_cast<float>(offsetFromStart) / static_cast<float>(voice.fadeInSamples);
            if (voice.fadeOutSamples > 0)
            {
                const std::int64_t remaining = voice.endPosition - voice.readPosition;
                if (remaining < voice.fadeOutSamples)
                    env *= static_cast<float>(remaining) / static_cast<float>(voice.fadeOutSamples);
            }

            const float normPos = static_cast<float>(offsetFromStart) * invSpan;
            if (voice.volEnvCount > 0)
                env *= evaluatePlaybackEnvelope(voice.volEnv, voice.volEnvCount, normPos, 1.0f);

            float panScaleL = 1.0f;
            float panScaleR = 1.0f;
            if (voice.panEnvCount > 0)
            {
                const float panOffset = evaluatePlaybackEnvelope(voice.panEnv, voice.panEnvCount, normPos, 0.0f);
                const auto [pL, pR] = SnapshotBuilder::panGains(panOffset);
                panScaleL = pL;
                panScaleR = pR;
            }

            const std::int64_t frameIdx = voice.reversed
                ? (voice.endPosition - 1 - offsetFromStart)
                : voice.readPosition;

            destL[i] += srcL[frameIdx] * voice.gainLeft * env * panScaleL;
            destR[i] += srcR[frameIdx] * voice.gainRight * env * panScaleR;

            ++voice.readPosition;
        }
        voice.startDelay = 0;

        if (useClipFx)
        {
            pluginProc->processClip(voice.clipId, scratch_->clipBufL.data(), scratch_->clipBufR.data(), n);
            float* trkL = validTrack ? scratch_->trackBufL[static_cast<std::size_t>(voice.track)].data() : left;
            float* trkR = validTrack ? scratch_->trackBufR[static_cast<std::size_t>(voice.track)].data() : right;
            for (int i = 0; i < n; ++i)
            {
                trkL[i] += scratch_->clipBufL[static_cast<std::size_t>(i)];
                trkR[i] += scratch_->clipBufR[static_cast<std::size_t>(i)];
            }
        }
    }

    // Process per-track VST3/AU FX, measure per-track live peak meters, and sum into master bus
    for (int t = 0; t < kMaxTracks; ++t)
    {
        const auto idx = static_cast<std::size_t>(t);
        if (!trackUsed[idx])
        {
            trackPeakL_[idx].store(0.0f, std::memory_order_relaxed);
            trackPeakR_[idx].store(0.0f, std::memory_order_relaxed);
            continue;
        }

        float* tL = scratch_->trackBufL[idx].data();
        float* tR = scratch_->trackBufR[idx].data();
        if (pluginProc != nullptr)
            pluginProc->processTrack(t, tL, tR, n);

        float pL = 0.0f;
        float pR = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const float aL = std::abs(tL[i]);
            const float aR = std::abs(tR[i]);
            if (aL > pL) pL = aL;
            if (aR > pR) pR = aR;
            left[i] += tL[i];
            right[i] += tR[i];
        }
        trackPeakL_[idx].store(pL, std::memory_order_relaxed);
        trackPeakR_[idx].store(pR, std::memory_order_relaxed);
    }

    // Apply Master Bus Volume Automation across the song timeline
    if (current_ != nullptr && current_->masterVolEnvCount > 0 && current_->lengthSamples > 0)
    {
        const double invLen = 1.0 / static_cast<double>(current_->lengthSamples);
        const std::int64_t blockStartPos = std::max<std::int64_t>(0, position_ - n);
        for (int i = 0; i < n; ++i)
        {
            const float normPos = static_cast<float>(std::clamp(static_cast<double>(blockStartPos + i) * invLen, 0.0, 1.0));
            const float mGain = evaluatePlaybackEnvelope(current_->masterVolEnv, current_->masterVolEnvCount, normPos, 1.0f);
            left[i] *= mGain;
            right[i] *= mGain;
        }
    }

    // Process Master Bus VST3/AU FX chain
    if (pluginProc != nullptr)
        pluginProc->processMaster(left, right, n);
}

void Sequencer::stopAllVoices() noexcept
{
    for (auto& voice : voices_)
    {
        voice.active = false;
        voice.owner = nullptr;
    }
    for (auto& voice : metronomeVoices_)
    {
        voice.active = false;
        voice.owner = nullptr;
    }
}

void Sequencer::stopTrackVoicesAt(int offsetInBlock) noexcept
{
    for (auto& voice : voices_)
    {
        if (voice.active && voice.owner != nullptr)
        {
            if (voice.startDelay <= offsetInBlock)
            {
                const std::int64_t framesUntilWrap = static_cast<std::int64_t>(offsetInBlock - voice.startDelay);
                voice.endPosition = std::min(voice.endPosition, voice.readPosition + framesUntilWrap);
                if (voice.endPosition <= voice.readPosition)
                {
                    voice.active = false;
                    voice.owner = nullptr;
                }
            }
        }
    }
}

bool Sequencer::hasVoicesFrom(const PlaybackSnapshot* snapshot) const noexcept
{
    return std::any_of(voices_.begin(), voices_.end(),
                       [snapshot](const Voice& v) { return v.active && v.owner == snapshot; });
}

} // namespace trami
