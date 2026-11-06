#pragma once

#include "model/Arrangement.h"
#include "model/Pattern.h"
#include "model/Sample.h"

#include <optional>
#include <string>
#include <vector>

namespace trami
{

/// The complete document. All edits go through these methods, which keep the
/// song consistent (valid references, clips inside patterns, no overlapping
/// blocks). Methods return false / nullopt when an edit is rejected.
class Song
{
public:
    // --- Global settings -----------------------------------------------------
    double bpm() const noexcept { return bpm_; }
    double swing() const noexcept { return swing_; }
    float masterVolume() const noexcept { return masterVolume_; }

    void setBpm(double bpm) noexcept;
    void setSwing(double swing) noexcept;
    void setMasterVolume(float volume) noexcept;

    // --- Samples -------------------------------------------------------------
    SampleId addSample(Sample sample);
    bool removeSample(SampleId id); ///< Also removes all clips using it.
    const Sample* findSample(SampleId id) const noexcept;
    Sample* findSample(SampleId id) noexcept;
    const std::vector<Sample>& samples() const noexcept { return samples_; }

    // --- Patterns ------------------------------------------------------------
    PatternId addPattern(std::string name, int lengthBars);
    std::optional<PatternId> duplicatePattern(PatternId id);
    bool renamePattern(PatternId id, std::string name);
    bool setPatternLength(PatternId id, int lengthBars); ///< Drops clips beyond the new end.
    bool removePattern(PatternId id);                    ///< Also removes its blocks.
    const Pattern* findPattern(PatternId id) const noexcept;
    Pattern* findPattern(PatternId id) noexcept;
    const std::vector<Pattern>& patterns() const noexcept { return patterns_; }

    // --- Clips ---------------------------------------------------------------
    std::optional<ClipId> placeClip(PatternId pattern, int track, SampleId sample, int startStep,
                                    int lengthSteps = 0, int offsetSteps = 0);
    bool resizeClip(PatternId pattern, ClipId clip, int newStartStep, int newLengthSteps, int newOffsetSteps);
    bool moveClip(PatternId pattern, ClipId clip, int newTrack, int newStartStep);
    bool removeClip(PatternId pattern, ClipId clip);
    bool setPatternTrackSample(PatternId pattern, int track, SampleId sample);
    bool replaceTrackSample(PatternId pattern, int track, SampleId newSampleId);
    SampleId patternTrackSample(PatternId pattern, int track) const noexcept;

    // --- Arrangement ---------------------------------------------------------
    std::optional<BlockId> placeBlock(int arrangerTrack, PatternId pattern, int startBar);
    bool moveBlock(BlockId block, int newTrack, int newStartBar);
    bool removeBlock(BlockId block);
    const Arrangement& arrangement() const noexcept { return arrangement_; }
    ArrangerTrack* arrangerTrack(int index) noexcept;

    /// Length of the song in bars (end of the last block).
    int lengthBars() const noexcept;

private:
    static constexpr int kMaxBars = 9999;

    bool blockFits(int track, int startBar, int lengthBars, BlockId ignore) const noexcept;
    int patternLength(PatternId id) const noexcept;

    double bpm_ = 120.0;
    double swing_ = 0.0;
    float masterVolume_ = 1.0f;

    std::vector<Sample> samples_;
    std::vector<Pattern> patterns_;
    Arrangement arrangement_{};

    IdGenerator<SampleId> sampleIds_;
    IdGenerator<PatternId> patternIds_;
    IdGenerator<ClipId> clipIds_;
    IdGenerator<BlockId> blockIds_;
};

} // namespace trami
