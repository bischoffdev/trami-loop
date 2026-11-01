#include "engine/Sequencer.h"

#include "engine/Synth.h"
#include "model/Timing.h"

#include <algorithm>
#include <cmath>

namespace trami
{

Sequencer::Sequencer(SnapshotExchange& exchange, double sampleRate)
    : exchange_(exchange), click_(synth::click(sampleRate, 1000.0)), accentClick_(synth::click(sampleRate, 1600.0))
{
}

Sequencer::~Sequencer()
{
    delete pendingPreview_.exchange(nullptr);
    delete currentPreview_;
    currentPreview_ = nullptr;
    collectPreviewGarbage();

    if (previous_ != current_)
        delete previous_;
    delete current_;
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
        previewVoice_ = Voice{currentPreview_->audio.get(), nullptr, 0, 0, 0.85f, 0.85f, true};
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
    if (previous_ != nullptr && !hasVoicesFrom(previous_) && exchange_.tryRetire(previous_))
        previous_ = nullptr;

    if (previous_ != nullptr)
        return;

    if (PlaybackSnapshot* next = exchange_.takePending())
    {
        previous_ = current_;
        current_ = next;
        if (current_->lengthSamples > 0)
            position_ %= current_->lengthSamples;
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
                   it->gainRight);
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
                           float gainR) noexcept
{
    if (audio == nullptr || audio->numFrames() == 0)
        return;

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

    *target = Voice{audio, owner, 0, delay, gainL, gainR, true};
}

void Sequencer::renderVoices(float* left, float* right, int numSamples) noexcept
{
    for (auto& voice : voices_)
    {
        if (!voice.active)
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
        voice.active = false;
}

bool Sequencer::hasVoicesFrom(const PlaybackSnapshot* snapshot) const noexcept
{
    return std::any_of(voices_.begin(), voices_.end(),
                       [snapshot](const Voice& v) { return v.active && v.owner == snapshot; });
}

} // namespace trami
