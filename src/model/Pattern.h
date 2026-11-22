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

inline constexpr int kTracksPerPattern = 32;

/// Segment interpolation curve for multi-point ACID-style breakpoint envelopes.
enum class EnvelopeCurve
{
    Linear = 0,
    Fast = 1, ///< Logarithmic / fast attack
    Slow = 2  ///< Exponential / slow attack
};

/// A single breakpoint node on a clip or track envelope.
struct EnvelopePoint
{
    double position = 0.0; ///< Normalized [0.0 .. 1.0] on a Clip, or step position [0 .. lengthSteps] on a Track.
    float value = 1.0f;    ///< Gain [0.0 .. 2.0], Pan [-1.0 .. +1.0], or FX Send [0.0 .. 1.0].
    EnvelopeCurve curve = EnvelopeCurve::Linear;
};

enum class TrackEnvelopeType
{
    None = 0,
    Volume = 1,
    Pan = 2,
    FxSend = 3
};

/// Evaluates a sorted breakpoint envelope at `pos`, returning `defaultVal` if `points` is empty.
float evaluateEnvelope(const std::vector<EnvelopePoint>& points, double pos, float defaultVal) noexcept;

/// Serializable descriptor for an external VST3 / AU audio effect plugin slot.
struct PluginSlot
{
    std::string name;             ///< Display name (e.g. "ValhallaSupermassive")
    std::string format;           ///< "VST3" or "AudioUnit"
    std::string fileOrIdentifier; ///< JUCE PluginDescription::fileOrIdentifier
    std::string identifierString; ///< JUCE PluginDescription::createIdentifierString()
    bool bypassed = false;
    std::string stateBase64;      ///< Persisted plugin chunk state (Base64)
};

/// Named timeline marker / section anchor on a song ruler.
struct TimelineMarker
{
    int id = 0;
    double step = 0.0;
    std::string name;
};

/// A sample placed on a song track at a grid step (supports fractional steps for unconstrained resizing/moving).
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
    int transposeSemitones = 0;  ///< Transposition in semitones (-48 .. +48).
    std::vector<EnvelopePoint> volumeEnvelope; ///< Optional multi-point volume envelope across [0..1] of clip.
    std::vector<EnvelopePoint> panEnvelope;    ///< Optional multi-point pan envelope [-1..+1] across [0..1] of clip.
    std::vector<PluginSlot> fxChain;           ///< Optional per-clip / per-event VST3/AU plugin chain.
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
    bool armed = false;
    float fxSend = 0.0f; ///< Aux FX send level [0.0 .. 1.0].
    TrackEnvelopeType visibleEnvelope = TrackEnvelopeType::None;
    std::vector<EnvelopePoint> volumeAutomation; ///< Position in steps.
    std::vector<EnvelopePoint> panAutomation;    ///< Position in steps.
    std::vector<EnvelopePoint> fxAutomation;     ///< Position in steps.
    SampleId sampleId;
    std::optional<SampleKind> kindOverride;
    std::uint32_t colorArgb = 0;     ///< 0 = automatic color from sample; otherwise 0xFFRRGGBB custom track color.
    std::vector<PluginSlot> fxChain; ///< Track-level VST3/AU plugin chain.
    std::vector<Clip> clips;
};

/// A song representing tracks with mixer settings, clips, markers, and master bus automation/FX.
struct Pattern
{
    PatternId id;
    std::string name;
    int lengthBars = 1;
    int stepsPerBar = kStepsPerBar;
    std::vector<PatternTrack> tracks{1};
    std::vector<TimelineMarker> markers;
    std::vector<EnvelopePoint> masterVolumeAutomation; ///< Master bus volume automation (position in steps).
    std::vector<PluginSlot> masterFxChain;             ///< Master bus VST3/AU plugin chain.

    int lengthSteps() const noexcept;
    PatternTrack& track() noexcept { return tracks.front(); }
    const PatternTrack& track() const noexcept { return tracks.front(); }
};

} // namespace trami
