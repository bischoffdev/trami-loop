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

/// Pure-C++ hook for hosting external VST3 / AU plugin chains (no built-in effects).
class AudioPluginProcessor
{
public:
    virtual ~AudioPluginProcessor() = default;
    virtual void processClip(std::uint32_t clipId, float* left, float* right, int numSamples) noexcept = 0;
    virtual void processTrack(int trackIndex, float* left, float* right, int numSamples) noexcept = 0;
    virtual void processMaster(float* left, float* right, int numSamples) noexcept = 0;

    /// Latency reported by hosted plugin chains (in samples) for Plugin Delay Compensation (PDC).
    virtual int clipLatencySamples(std::uint32_t /*clipId*/) const noexcept { return 0; }
    virtual int trackLatencySamples(int /*trackIndex*/) const noexcept { return 0; }
    virtual int masterLatencySamples() const noexcept { return 0; }
};

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
    void requestPlaying(bool shouldPlay) noexcept
    {
        playRequested_.store(shouldPlay);
        if (!shouldPlay)
        {
            stopVoicesRequested_.store(true);
            stopPreviewRequested_.store(true);
            playing_.store(false);
        }
    }
    void requestRewind() noexcept
    {
        rewindRequested_.store(true);
        publishedPosition_.store(0);
    }
    void requestSeek(std::int64_t samplePosition) noexcept;
    void requestStop() noexcept
    {
        requestPlaying(false);
    }
    void setMetronomeEnabled(bool enabled) noexcept { metronomeEnabled_.store(enabled); }
    void setLoopingEnabled(bool enabled) noexcept { loopingEnabled_.store(enabled); }

    bool isPlaying() const noexcept { return playing_.load(); }
    bool isMetronomeEnabled() const noexcept { return metronomeEnabled_.load(); }
    bool isLoopingEnabled() const noexcept { return loopingEnabled_.load(); }
    std::int64_t position() const noexcept { return publishedPosition_.load(); }
    int activeVoiceCount() const noexcept { return activeVoiceCount_.load(std::memory_order_relaxed); }

    /// Plays an audio buffer as an audition/preview. Safely replaces any
    /// in-progress preview without locking or allocating on the audio thread.
    void triggerPreview(std::shared_ptr<const AudioData> audio, int startDelaySamples = 0, bool loopPreview = false);

    /// Stops any currently playing audition preview.
    void stopPreview() noexcept { stopPreviewRequested_.store(true); }

    /// Returns true if an audition preview is currently queued or actively playing.
    bool isPreviewActive() const noexcept
    {
        return pendingPreview_.load(std::memory_order_relaxed) != nullptr ||
               previewPlaying_.load(std::memory_order_relaxed);
    }

    /// Normalized progress [0.0 .. 1.0] of the currently playing preview.
    double previewProgress() const noexcept { return previewProgress_.load(std::memory_order_relaxed); }

    /// Real-time track mixer controls (effective immediately on active and upcoming voices).
    static constexpr int kMaxTracks = 64;
    void setTrackMix(int track, float volume, float pan, bool muted, bool solo) noexcept;
    void setTrackVolume(int track, float volume) noexcept;
    void setTrackPan(int track, float pan) noexcept;
    void setTrackMuted(int track, bool muted) noexcept;
    void setTrackSolo(int track, bool solo) noexcept;
    void setTrackLatencySamples(int track, int latencySamples) noexcept;
    int trackLatencySamples(int track) const noexcept;
    int trackCompensationSamples(int track) const noexcept;
    int maxTrackLatencySamples() const noexcept;
    void setPdcEnabled(bool enabled) noexcept { pdcEnabled_.store(enabled, std::memory_order_relaxed); }
    bool isPdcEnabled() const noexcept { return pdcEnabled_.load(std::memory_order_relaxed); }
    void setProjectLatencySamples(int samples) noexcept { projectLatencySamples_.store(std::max(0, samples), std::memory_order_relaxed); }
    int projectLatencySamples() const noexcept { return projectLatencySamples_.load(std::memory_order_relaxed); }
    int totalLatencySamples() const noexcept;
    void syncTrackMixFromSnapshot() noexcept;
    void resetTrackMix() noexcept;

    /// Master volume (0.0 to 2.0). Smoothly applied to final output.
    void setMasterVolume(float volume) noexcept;
    float masterVolume() const noexcept { return masterVolume_.load(std::memory_order_relaxed); }

    struct OutputLevels
    {
        float peakL = 0.0f;
        float peakR = 0.0f;
        float rmsL = 0.0f;
        float rmsR = 0.0f;
        bool clipped = false;
    };
    OutputLevels outputLevels() const noexcept;
    void resetClipped() noexcept { clipOccurred_.store(false, std::memory_order_relaxed); }

    /// Live per-track stereo peak levels [0.0 .. 2.0+] for track header meters.
    std::pair<float, float> trackPeakLevels(int track) const noexcept;

    /// Attaches an external VST3 / AU plugin host processor (nullptr to disable).
    void setPluginProcessor(AudioPluginProcessor* processor) noexcept
    {
        pluginProcessor_.store(processor, std::memory_order_release);
    }

    /// Called on the message thread to safely delete retired preview buffers.
    void collectPreviewGarbage();

    // --- Audio thread --------------------------------------------------------
    /// Adds `numSamples` of output to the (pre-cleared) left/right buffers.
    void process(float* left, float* right, int numSamples) noexcept;

private:
    static constexpr int kMaxVoices = 64;
    static constexpr std::size_t kRetiredQueueCapacity = 32;
    static constexpr std::size_t kMaxRetiring = 8;

    struct Voice
    {
        const AudioData* audio = nullptr;
        const PlaybackSnapshot* owner = nullptr;
        std::int64_t readPosition = 0;
        std::int64_t startPosition = 0;
        std::int64_t endPosition = 0;
        int startDelay = 0;
        float baseGainLeft = 1.0f;
        float baseGainRight = 1.0f;
        float gainLeft = 1.0f;
        float gainRight = 1.0f;
        bool active = false;
        int track = -1;
        std::int64_t fadeInSamples = 0;
        std::int64_t fadeOutSamples = 0;
        std::uint32_t clipId = 0;
        bool reversed = false;
        bool hasClipFx = false;
        int clipLatencySamples = 0;
        float fxSend = 0.0f;
        std::array<PlaybackEnvelopeNode, kMaxPlaybackEnvNodes> volEnv{};
        int volEnvCount = 0;
        std::array<PlaybackEnvelopeNode, kMaxPlaybackEnvNodes> panEnv{};
        int panEnvCount = 0;
    };

    struct TrackMixState
    {
        std::atomic<float> volume{1.0f};
        std::atomic<float> pan{0.0f};
        std::atomic<bool> muted{false};
        std::atomic<bool> solo{false};
        std::atomic<int> manualLatencySamples{0};
        std::atomic<int> fxLatencySamples{0};
        std::atomic<int> maxClipLatencySamples{0};
    };

    struct PreviewItem
    {
        std::shared_ptr<const AudioData> audio;
        int startDelaySamples = 0;
        bool loopPreview = false;
    };

    void adoptPendingSnapshot() noexcept;
    void handleTransportRequests() noexcept;
    void handlePreviewRequests() noexcept;
    void chaseActiveEvents(std::int64_t pos, int blockOffset = 0) noexcept;
    void scheduleRange(std::int64_t from, std::int64_t to, int blockOffset) noexcept;
    void scheduleMetronome(std::int64_t from, std::int64_t to, int blockOffset) noexcept;
    void startMetronomeVoice(const AudioData* audio, int delay, float gainL, float gainR) noexcept;
    void startVoice(const AudioData* audio, const PlaybackSnapshot* owner, int delay, float gainL, float gainR,
                    std::int64_t audioOffset = 0, std::int64_t maxFrames = -1, int track = -1,
                    std::int64_t fadeInSamples = 0, std::int64_t fadeOutSamples = 0,
                    std::uint32_t clipId = 0, bool reversed = false,
                    const PlaybackEvent* srcEvent = nullptr) noexcept;
    void renderVoices(float* left, float* right, int numSamples) noexcept;
    void renderPreview(float* left, float* right, int numSamples) noexcept;
    void renderMetronome(float* left, float* right, int numSamples) noexcept;
    void stopAllVoices() noexcept;
    void stopTrackVoicesAt(int offsetInBlock) noexcept;
    bool hasVoicesFrom(const PlaybackSnapshot* snapshot) const noexcept;
    void retirePreview(PreviewItem* item) noexcept;
    void updateVoiceGainsForTrack(int track) noexcept;
    void updateAllVoiceGains() noexcept;

    SnapshotExchange& exchange_;
    PlaybackSnapshot* current_ = nullptr;
    std::array<PlaybackSnapshot*, kMaxRetiring> retiringSnapshots_{};

    std::array<Voice, kMaxVoices> voices_{};
    std::array<TrackMixState, kMaxTracks> trackMix_{};
    std::atomic<bool> anySolo_{false};
    std::int64_t position_ = 0;

    AudioData click_;
    AudioData accentClick_;
    std::array<Voice, 8> metronomeVoices_{};

    // Dedicated preview state
    Voice previewVoice_{};
    bool previewLooping_ = false;
    PreviewItem* currentPreview_ = nullptr;
    std::atomic<PreviewItem*> pendingPreview_{nullptr};
    std::atomic<bool> stopPreviewRequested_{false};
    std::atomic<bool> previewPlaying_{false};
    std::atomic<double> previewProgress_{0.0};

    // Pre-allocated audio-thread scratch buffers for per-clip & per-track plugin processing + metering.
    // Flat heap-allocated vectors (sizeof(ScratchBuffers) ~= 96 bytes) so MSVC never materializes
    // a 2 MB aggregate temporary on the 1 MB Windows stack inside make_unique<ScratchBuffers>().
    static constexpr int kMaxScratchSamples = 4096;
    static constexpr int kPdcBufferSize = 65536; ///< Power-of-2 circular delay buffer per track (~1.36s at 48 kHz)
    static constexpr int kPdcBufferMask = kPdcBufferSize - 1;
    struct ScratchBuffers
    {
        std::vector<float> trackBufL;
        std::vector<float> trackBufR;
        std::vector<float> clipBufL;
        std::vector<float> clipBufR;
        std::vector<float> pdcRingL;
        std::vector<float> pdcRingR;
        std::array<int, kMaxTracks> pdcWritePos{};
        std::array<int, kMaxTracks> pdcActiveDelay{};
        std::array<int, kMaxTracks> trackTailRemaining{};
        std::array<int, kMaxTracks> lastClipLat{};

        ScratchBuffers()
            : trackBufL(static_cast<std::size_t>(kMaxTracks) * kMaxScratchSamples, 0.0f)
            , trackBufR(static_cast<std::size_t>(kMaxTracks) * kMaxScratchSamples, 0.0f)
            , clipBufL(static_cast<std::size_t>(kMaxScratchSamples), 0.0f)
            , clipBufR(static_cast<std::size_t>(kMaxScratchSamples), 0.0f)
            , pdcRingL(static_cast<std::size_t>(kMaxTracks) * kPdcBufferSize, 0.0f)
            , pdcRingR(static_cast<std::size_t>(kMaxTracks) * kPdcBufferSize, 0.0f)
        {
        }

        float* trackL(int track) noexcept
        {
            return trackBufL.data() + static_cast<std::size_t>(track) * kMaxScratchSamples;
        }
        float* trackR(int track) noexcept
        {
            return trackBufR.data() + static_cast<std::size_t>(track) * kMaxScratchSamples;
        }
        float* ringL(int track) noexcept
        {
            return pdcRingL.data() + static_cast<std::size_t>(track) * kPdcBufferSize;
        }
        float* ringR(int track) noexcept
        {
            return pdcRingR.data() + static_cast<std::size_t>(track) * kPdcBufferSize;
        }

        void clearTrackPdc(int track) noexcept
        {
            if (track < 0 || track >= kMaxTracks)
                return;
            std::fill_n(ringL(track), kPdcBufferSize, 0.0f);
            std::fill_n(ringR(track), kPdcBufferSize, 0.0f);
            pdcWritePos[static_cast<std::size_t>(track)] = 0;
            pdcActiveDelay[static_cast<std::size_t>(track)] = 0;
            trackTailRemaining[static_cast<std::size_t>(track)] = 0;
            lastClipLat[static_cast<std::size_t>(track)] = 0;
        }

        void clearAllPdc() noexcept
        {
            for (int t = 0; t < kMaxTracks; ++t)
            {
                if (pdcActiveDelay[static_cast<std::size_t>(t)] > 0 ||
                    pdcWritePos[static_cast<std::size_t>(t)] != 0 ||
                    trackTailRemaining[static_cast<std::size_t>(t)] > 0)
                {
                    clearTrackPdc(t);
                }
            }
        }
    };
    std::unique_ptr<ScratchBuffers> scratch_;
    std::array<std::atomic<float>, kMaxTracks> trackPeakL_{};
    std::array<std::atomic<float>, kMaxTracks> trackPeakR_{};
    std::atomic<AudioPluginProcessor*> pluginProcessor_{nullptr};
    std::atomic<bool> pdcEnabled_{true};
    std::atomic<int> projectLatencySamples_{0};
    std::atomic<int> modelMasterLatencySamples_{0};

    // Lock-free SPSC ring buffer for retired preview items (audio thread -> message thread)
    std::array<PreviewItem*, kRetiredQueueCapacity> retiredPreviews_{};
    std::atomic<std::size_t> retiredWrite_{0};
    std::atomic<std::size_t> retiredRead_{0};

    std::atomic<bool> playRequested_{false};
    std::atomic<bool> stopVoicesRequested_{false};
    std::atomic<bool> rewindRequested_{false};
    std::atomic<bool> seekRequested_{false};
    std::atomic<std::int64_t> seekRequestedPosition_{0};
    std::atomic<bool> metronomeEnabled_{false};
    std::atomic<bool> loopingEnabled_{true};
    std::atomic<bool> playing_{false};
    std::atomic<std::int64_t> publishedPosition_{0};
    std::atomic<int> activeVoiceCount_{0};

    // Master volume & loudness metering
    std::atomic<float> masterVolume_{1.0f};
    float currentMasterVol_ = 1.0f;
    std::atomic<float> outPeakL_{0.0f};
    std::atomic<float> outPeakR_{0.0f};
    std::atomic<float> outRmsL_{0.0f};
    std::atomic<float> outRmsR_{0.0f};
    std::atomic<bool> clipOccurred_{false};
};

} // namespace trami
