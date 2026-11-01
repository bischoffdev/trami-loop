#pragma once

#include "engine/AudioData.h"
#include "engine/PlaybackSnapshot.h"
#include "engine/SnapshotExchange.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>

namespace trami
{

/// Real-time sequencer. Triggers snapshot events at their exact sample
/// position and mixes the resulting voices, dedicated preview voice, and metronome.
///
/// Threading: `process()` runs on the audio thread and never allocates, locks
/// or frees.
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

    /// Plays an audio buffer once as an audition/preview. Safely replaces any
    /// in-progress preview without locking or allocating on the audio thread.
    void triggerPreview(std::shared_ptr<const AudioData> audio);

    /// Stops any currently playing audition preview.
    void stopPreview() noexcept { stopPreviewRequested_.store(true); }

    /// Returns true if an audition preview is actively playing.
    bool isPreviewPlaying() const noexcept { return previewPlaying_.load(); }

    /// Called on the message thread to safely delete retired preview buffers.
    void collectPreviewGarbage();

    // --- Audio thread --------------------------------------------------------
    /// Adds `numSamples` of output to the (pre-cleared) left/right buffers.
    void process(float* left, float* right, int numSamples) noexcept;

private:
    static constexpr int kMaxVoices = 64;
    static constexpr std::size_t kRetiredQueueCapacity = 32;

    struct Voice
    {
        const AudioData* audio = nullptr;
        const PlaybackSnapshot* owner = nullptr;
        std::int64_t readPosition = 0;
        int startDelay = 0;
        float gainLeft = 1.0f;
        float gainRight = 1.0f;
        bool active = false;
    };

    struct PreviewItem
    {
        std::shared_ptr<const AudioData> audio;
    };

    void adoptPendingSnapshot() noexcept;
    void handleTransportRequests() noexcept;
    void handlePreviewRequests() noexcept;
    void scheduleRange(std::int64_t from, std::int64_t to, int blockOffset) noexcept;
    void scheduleMetronome(std::int64_t from, std::int64_t to, int blockOffset) noexcept;
    void startVoice(const AudioData* audio, const PlaybackSnapshot* owner, int delay, float gainL, float gainR) noexcept;
    void renderVoices(float* left, float* right, int numSamples) noexcept;
    void renderPreview(float* left, float* right, int numSamples) noexcept;
    void stopAllVoices() noexcept;
    bool hasVoicesFrom(const PlaybackSnapshot* snapshot) const noexcept;
    void retirePreview(PreviewItem* item) noexcept;

    SnapshotExchange& exchange_;
    PlaybackSnapshot* current_ = nullptr;
    PlaybackSnapshot* previous_ = nullptr;

    std::array<Voice, kMaxVoices> voices_{};
    std::int64_t position_ = 0;

    AudioData click_;
    AudioData accentClick_;

    // Dedicated preview state
    Voice previewVoice_{};
    PreviewItem* currentPreview_ = nullptr;
    std::atomic<PreviewItem*> pendingPreview_{nullptr};
    std::atomic<bool> stopPreviewRequested_{false};
    std::atomic<bool> previewPlaying_{false};

    // Lock-free SPSC ring buffer for retired preview items (audio thread -> message thread)
    std::array<PreviewItem*, kRetiredQueueCapacity> retiredPreviews_{};
    std::atomic<std::size_t> retiredWrite_{0};
    std::atomic<std::size_t> retiredRead_{0};

    std::atomic<bool> playRequested_{false};
    std::atomic<bool> rewindRequested_{false};
    std::atomic<bool> metronomeEnabled_{false};
    std::atomic<bool> playing_{false};
    std::atomic<std::int64_t> publishedPosition_{0};
};

} // namespace trami
