#pragma once

#include "model/Arrangement.h"
#include "model/Pattern.h"
#include "model/Sample.h"
#include "model/Timing.h"

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
    GroovePreset groovePreset() const noexcept { return groovePreset_; }
    float masterVolume() const noexcept { return masterVolume_; }
    int projectRootNote() const noexcept { return projectRootNote_; }
    int timeSigNumerator() const noexcept { return timeSigNumerator_; }
    int timeSigDenominator() const noexcept { return timeSigDenominator_; }
    int stepsPerBeat() const noexcept { return std::max(1, 32 / timeSigDenominator_); }
    int stepsPerBar() const noexcept { return timeSigNumerator_ * stepsPerBeat(); }
    bool autoCrossfadeEnabled() const noexcept { return autoCrossfadeEnabled_; }
    bool envelopeLockEnabled() const noexcept { return envelopeLockEnabled_; }
    bool rippleEditEnabled() const noexcept { return rippleEditEnabled_; }

    void setBpm(double bpm) noexcept;
    void setSwing(double swing) noexcept;
    void setGroovePreset(GroovePreset preset) noexcept;
    void setMasterVolume(float volume) noexcept;
    void setProjectRootNote(int rootNote) noexcept;
    void setTimeSignature(int numerator, int denominator) noexcept;
    void setAutoCrossfadeEnabled(bool enabled) noexcept { autoCrossfadeEnabled_ = enabled; }
    void setEnvelopeLockEnabled(bool enabled) noexcept { envelopeLockEnabled_ = enabled; }
    void setRippleEditEnabled(bool enabled) noexcept { rippleEditEnabled_ = enabled; }

    /// Returns total transpose in semitones combining master project key conforming (for Loops with a rootNote)
    /// and the clip's individual transposeSemitones.
    int effectiveClipTranspose(const Sample* sample, int clipTransposeSemitones, SampleKind trackKind) const noexcept;
    double defaultClipSpanSteps(const Clip& clip, int trackIndex, PatternId patternId) const noexcept;

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
    std::vector<Pattern>& patterns() noexcept { return patterns_; }

    // --- Clips ---------------------------------------------------------------
    std::optional<ClipId> placeClip(PatternId pattern, int track, SampleId sample, double startStep,
                                    double lengthSteps = 0.0, double offsetSteps = 0.0);
    bool resizeClip(PatternId pattern, ClipId clip, double newStartStep, double newLengthSteps, double newOffsetSteps);
    bool setClipEnvelope(PatternId pattern, ClipId clip, float gain, float fadeInSteps, float fadeOutSteps);
    bool setClipVolumeEnvelope(PatternId pattern, ClipId clip, std::vector<EnvelopePoint> points);
    bool setClipPanEnvelope(PatternId pattern, ClipId clip, std::vector<EnvelopePoint> points);
    bool setClipReversed(PatternId pattern, ClipId clip, bool reversed);
    bool isClipReversed(PatternId pattern, ClipId clip) const noexcept;
    bool setClipTranspose(PatternId pattern, ClipId clip, int semitones);
    int clipTranspose(PatternId pattern, ClipId clip) const noexcept;
    const Clip* findClip(PatternId pattern, ClipId clip) const noexcept;
    Clip* findClip(PatternId pattern, ClipId clip) noexcept;
    bool moveClip(PatternId pattern, ClipId clip, int newTrack, double newStartStep);
    bool moveClipBetweenPatterns(PatternId fromPattern, PatternId toPattern, ClipId clip, double newStartStep);
    bool removeClip(PatternId pattern, ClipId clip);
    std::optional<std::pair<ClipId, ClipId>> splitClip(PatternId pattern, ClipId clip, double splitStep);
    std::optional<ClipId> joinClips(PatternId pattern, const std::vector<ClipId>& clipIds);
    bool setTrackAutomation(PatternId pattern, int trackIndex, TrackEnvelopeType type, std::vector<EnvelopePoint> points);
    bool setTrackFxSend(PatternId pattern, int trackIndex, float fxSend);
    bool setMasterAutomation(PatternId pattern, std::vector<EnvelopePoint> points);
    bool setClipFxChain(PatternId pattern, ClipId clip, std::vector<PluginSlot> chain);
    bool setTrackFxChain(PatternId pattern, int trackIndex, std::vector<PluginSlot> chain);
    bool setPatternTrackColor(PatternId pattern, int trackIndex, std::uint32_t colorArgb);
    bool setMasterFxChain(PatternId pattern, std::vector<PluginSlot> chain);
    int addMarker(PatternId pattern, double step, std::string name);
    bool moveMarker(PatternId pattern, int markerId, double newStep);
    bool renameMarker(PatternId pattern, int markerId, std::string newName);
    bool removeMarker(PatternId pattern, int markerId);
    bool setPatternTrackSample(PatternId pattern, int track, SampleId sample);
    bool replaceTrackSample(PatternId pattern, int track, SampleId newSampleId);
    SampleId patternTrackSample(PatternId pattern, int track) const noexcept;
    int addTrackToPattern(PatternId pattern, SampleId sample = {});
    bool removeTrackFromPattern(PatternId pattern, int trackIndex);
    int duplicateTrackInPattern(PatternId pattern, int trackIndex);
    bool movePatternTrack(PatternId pattern, int fromIndex, int toIndex);
    bool renamePatternTrack(PatternId pattern, int trackIndex, std::string name);
    std::string patternTrackName(PatternId pattern, int trackIndex) const;
    bool setPatternTrackKind(PatternId pattern, int trackIndex, SampleKind kind);
    SampleKind patternTrackKind(PatternId pattern, int trackIndex) const;

    // --- Arrangement ---------------------------------------------------------
    std::optional<BlockId> placeBlock(int arrangerTrack, PatternId pattern, int startBar);
    bool moveBlock(BlockId block, int newTrack, int newStartBar);
    bool removeBlock(BlockId block);
    bool moveArrangerTrack(int fromIndex, int toIndex);
    bool deleteArrangerTrack(int trackIndex);
    int addArrangerTrack(PatternId patternId = {});
    int numArrangerTracks() const noexcept;
    bool renameArrangerTrack(int trackIndex, std::string name);
    std::string arrangerTrackName(int trackIndex) const;
    PatternId arrangerTrackPattern(int trackIndex) const;
    bool setArrangerTrackPattern(int trackIndex, PatternId patternId);
    int findTrackForPattern(PatternId patternId) const;
    const Arrangement& arrangement() const noexcept { return arrangement_; }
    ArrangerTrack* arrangerTrack(int index) noexcept;

    // --- Zoom State ----------------------------------------------------------
    float patternZoomX() const noexcept { return patternZoomX_; }
    float patternZoomY() const noexcept { return patternZoomY_; }
    void setPatternZoom(float x, float y) noexcept { patternZoomX_ = x; patternZoomY_ = y; }

    float arrangerZoomX() const noexcept { return arrangerZoomX_; }
    float arrangerZoomY() const noexcept { return arrangerZoomY_; }
    void setArrangerZoom(float x, float y) noexcept { arrangerZoomX_ = x; arrangerZoomY_ = y; }

    /// Length of the song in bars (end of the last block).
    int lengthBars() const noexcept;

private:
    static constexpr int kMaxBars = 9999;

    bool blockFits(int track, int startBar, int lengthBars, BlockId ignore) const noexcept;
    int patternLength(PatternId id) const noexcept;

    double bpm_ = 120.0;
    double swing_ = 0.0;
    GroovePreset groovePreset_ = GroovePreset::Straight;
    float masterVolume_ = 1.0f;
    int projectRootNote_ = -1; ///< -1 = None / No master key, 0..127 = MIDI note / pitch class.
    int timeSigNumerator_ = 4;
    int timeSigDenominator_ = 4;
    bool autoCrossfadeEnabled_ = true;
    bool envelopeLockEnabled_ = true;
    bool rippleEditEnabled_ = false;
    float patternZoomX_ = 1.0f;
    float patternZoomY_ = 1.0f;
    float arrangerZoomX_ = 1.0f;
    float arrangerZoomY_ = 1.0f;

    std::vector<Sample> samples_;
    std::vector<Pattern> patterns_;
    Arrangement arrangement_{};

    IdGenerator<SampleId> sampleIds_;
    IdGenerator<PatternId> patternIds_;
    IdGenerator<ClipId> clipIds_;
    IdGenerator<BlockId> blockIds_;
};

} // namespace trami
