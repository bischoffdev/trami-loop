#pragma once

#include "engine/AudioData.h"
#include "engine/PlaybackSnapshot.h"
#include "engine/SnapshotExchange.h"

#include <array>
#include <atomic>
#include <cstdint>

namespace trami
{

/// Real-time sequencer. Triggers snapshot events at their exact sample
/// position and mixes the resulting voices plus an optional metronome.
///
/// Threading: `process()` runs on the audio thread and never allocates, locks
/// or frees. The request*/set* methods may be called from any thread.
class Sequencer
{
public:
    Sequencer(SnapshotExchange& exchange, double sampleRate);
    ~Sequencer();

    Sequencer(const Sequencer&) = delete;
    Sequencer& operator=(const Sequencer&) = delete;

    // --- Any thread ----------------------------------------------------------
    void requestPlaying(bool shouldPlay) noexcept { playRequested_.store(shouldPlay); }
    void requestRewind() noexcept { rewindRequested_.store(true); }
    void setMetronomeEnabled(bool enabled) noexcept { metronomeEnabled_.store(enabled); }

    bool isPlaying() const noexcept { return playing_.load(); }
    bool isMetronomeEnabled() const noexcept { return metronomeEnabled_.load(); }
    std::int64_t position() const noexcept { return publishedPosition_.load(); }

    // --- Audio thread --------------------------------------------------------
    /// Adds `numSamples` of output to the (pre-cleared) left/right buffers.
    void process(float* left, float* right, int numSamples) noexcept;

private:
    static constexpr int kMaxVoices = 64;

    struct Voice
    {
        const AudioData* audio = nullptr;
        const PlaybackSnapshot* owner = nullptr;
        std::int64_t readPosition = 0;
        int startDelay = 0; ///< Samples to wait within the current block.
        float gainLeft = 1.0f;
        float gainRight = 1.0f;
        bool active = false;
    };

    void adoptPendingSnapshot() noexcept;
    void handleTransportRequests() noexcept;
    void scheduleRange(std::int64_t from, std::int64_t to, int blockOffset) noexcept;
    void scheduleMetronome(std::int64_t from, std::int64_t to, int blockOffset) noexcept;
    void startVoice(const AudioData* audio, const PlaybackSnapshot* owner, int delay, float gainL, float gainR) noexcept;
    void renderVoices(float* left, float* right, int numSamples) noexcept;
    void stopAllVoices() noexcept;
    bool hasVoicesFrom(const PlaybackSnapshot* snapshot) const noexcept;

    SnapshotExchange& exchange_;
    PlaybackSnapshot* current_ = nullptr;
    PlaybackSnapshot* previous_ = nullptr; ///< Kept alive until its voices finish.

    std::array<Voice, kMaxVoices> voices_{};
    std::int64_t position_ = 0;

    AudioData click_;
    AudioData accentClick_;

    std::atomic<bool> playRequested_{false};
    std::atomic<bool> rewindRequested_{false};
    std::atomic<bool> metronomeEnabled_{false};
    std::atomic<bool> playing_{false};
    std::atomic<std::int64_t> publishedPosition_{0};
};

} // namespace trami
