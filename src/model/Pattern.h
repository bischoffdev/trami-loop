#pragma once

#include "model/Ids.h"
#include "model/Sample.h"

#include <array>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace trami
{

inline constexpr int kTracksPerPattern = 16;

/// A sample placed on a pattern track at a grid step (supports fractional steps for unconstrained resizing/moving).
struct Clip
{
    ClipId id;
    SampleId sampleId;
    double startStep = 0.0;
    double lengthSteps = 0.0; ///< Custom length in steps. 0 = default/natural duration.
    double offsetSteps = 0.0; ///< Start offset in steps into the audio.
    float gain = 1.0f;           ///< Linear gain multiplier, 0..2 (default 1.0 = 0 dB).
    float fadeInSteps = 0.0f;    ///< Fade-in duration in steps (0 = no fade).
    float fadeOutSteps = 0.0f;   ///< Fade-out duration in steps (0 = no fade).
    bool reversed = false;       ///< When true, audio plays in reverse.
};

/// Temporary clip override state during real-time dragging, resizing, or envelope adjustments.
struct DragClipOverride
{
    ClipId clipId;
    int originalTrack = 0;
    double originalStartStep = 0.0;
    double originalLengthSteps = 0.0;
    double originalOffsetSteps = 0.0;
    float originalGain = 1.0f;
    float originalFadeIn = 0.0f;
    float originalFadeOut = 0.0f;

    int currentTrack = 0;
    double currentStartStep = 0.0;
    double currentLengthSteps = 0.0;
    double currentOffsetSteps = 0.0;
    float currentGain = 1.0f;
    float currentFadeIn = 0.0f;
    float currentFadeOut = 0.0f;
};

/// Effective fade-in and fade-out durations (in steps) resulting from explicit envelope settings
/// combined with automatic crossfades from overlapping clips.
struct ClipCrossfade
{
    float fadeInSteps = 0.0f;
    float fadeOutSteps = 0.0f;
};

struct TrackClipSpan
{
    ClipId id;
    double startStep = 0.0;
    double spanSteps = 0.0;
    float explicitFadeIn = 0.0f;
    float explicitFadeOut = 0.0f;
};

/// Computes effective crossfades for overlapping clips on a track.
/// Any overlapping clips receive an automatic crossfade across the overlap duration
/// (unless their explicit manual fade is already larger).
std::unordered_map<std::uint32_t, ClipCrossfade> computeTrackCrossfades(std::vector<TrackClipSpan> clips);

/// Mixer settings shared by pattern and arranger tracks.
struct TrackMix
{
    float volume = 1.0f; ///< Linear gain, 0..2.
    float pan = 0.0f;    ///< -1 (left) .. +1 (right).
};

struct PatternTrack
{
    std::string name;
    TrackMix mix;
    bool muted = false;
    bool solo = false;
    SampleId sampleId;
    std::optional<SampleKind> kindOverride;
    std::vector<Clip> clips;
};

/// A pattern representing a track with its mixer settings and clips.
struct Pattern
{
    PatternId id;
    std::string name;
    int lengthBars = 1;
    std::vector<PatternTrack> tracks{1};

    int lengthSteps() const noexcept;
    PatternTrack& track() noexcept { return tracks.front(); }
    const PatternTrack& track() const noexcept { return tracks.front(); }
};

} // namespace trami
