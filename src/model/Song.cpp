#include "model/Song.h"

#include <algorithm>

namespace trami
{
namespace
{

constexpr double kMinBpm = 20.0;
constexpr double kMaxBpm = 300.0;
constexpr int kMaxPatternBars = 9999;

bool isValidTrack(int index, int count) noexcept
{
    return index >= 0 && index < count;
}

template <typename T, typename IdType>
T* findById(std::vector<T>& items, IdType id) noexcept
{
    auto it = std::find_if(items.begin(), items.end(), [id](const T& item) { return item.id == id; });
    return it != items.end() ? &*it : nullptr;
}

} // namespace

// --- Global settings ---------------------------------------------------------

void Song::setBpm(double bpm) noexcept
{
    bpm_ = std::clamp(bpm, kMinBpm, kMaxBpm);
}

void Song::setSwing(double swing) noexcept
{
    swing_ = std::clamp(swing, 0.0, 1.0);
}

void Song::setMasterVolume(float volume) noexcept
{
    masterVolume_ = std::clamp(volume, 0.0f, 2.0f);
}

void Song::setProjectRootNote(int rootNote) noexcept
{
    projectRootNote_ = (rootNote < 0) ? -1 : std::clamp(rootNote, 0, 127);
}

int Song::effectiveClipTranspose(const Sample* sample, int clipTransposeSemitones, SampleKind trackKind) const noexcept
{
    int total = clipTransposeSemitones;
    if (trackKind == SampleKind::Loop && projectRootNote_ >= 0 && sample != nullptr && sample->rootNote >= 0)
    {
        total += Sample::keyConformSemitones(sample->rootNote, projectRootNote_);
    }
    return std::clamp(total, -48, 48);
}

double Song::defaultClipSpanSteps(const Clip& clip, int trackIndex, PatternId patternId) const noexcept
{
    if (clip.lengthSteps > 0.0)
        return clip.lengthSteps;

    const Sample* sample = findSample(clip.sampleId);
    if (sample == nullptr)
        return static_cast<double>(kStepsPerBeat);

    const SampleKind kind = (trackIndex >= 0) ? patternTrackKind(patternId, trackIndex) : sample->kind;
    if (kind == SampleKind::Loop)
    {
        if (sample->loopCycleSteps() > 0.0)
            return sample->loopCycleSteps();
        return static_cast<double>(std::max(1, sample->loopBars) * kStepsPerBar);
    }

    const double stepSec = 60.0 / std::max(20.0, bpm_) / static_cast<double>(kStepsPerBeat);
    return std::max(1.0, sample->durationSeconds / stepSec);
}

// --- Samples -----------------------------------------------------------------

SampleId Song::addSample(Sample sample)
{
    sample.id = sampleIds_.next();
    sample.loopBars = std::max(1, sample.loopBars);
    if (sample.loopBeats <= 0.0)
        sample.loopBeats = static_cast<double>(sample.loopBars * kBeatsPerBar);
    samples_.push_back(std::move(sample));
    return samples_.back().id;
}

bool Song::removeSample(SampleId id)
{
    const auto removed = std::erase_if(samples_, [id](const Sample& s) { return s.id == id; });
    if (removed == 0)
        return false;

    for (auto& pattern : patterns_)
    {
        for (auto& track : pattern.tracks)
        {
            if (track.sampleId == id)
                track.sampleId = {};
            std::erase_if(track.clips, [id](const Clip& c) { return c.sampleId == id; });
        }
    }
    return true;
}

const Sample* Song::findSample(SampleId id) const noexcept
{
    return const_cast<Song*>(this)->findSample(id);
}

Sample* Song::findSample(SampleId id) noexcept
{
    return findById(samples_, id);
}

// --- Patterns ----------------------------------------------------------------

PatternId Song::addPattern(std::string name, int lengthBars)
{
    Pattern pattern;
    pattern.id = patternIds_.next();
    pattern.name = std::move(name);
    pattern.lengthBars = std::clamp(lengthBars, 1, kMaxPatternBars);
    patterns_.push_back(std::move(pattern));
    return patterns_.back().id;
}

std::optional<PatternId> Song::duplicatePattern(PatternId id)
{
    const Pattern* source = findPattern(id);
    if (source == nullptr)
        return std::nullopt;

    Pattern copy = *source;
    copy.id = patternIds_.next();
    copy.name += " copy";
    for (auto& track : copy.tracks)
        for (auto& clip : track.clips)
            clip.id = clipIds_.next();

    patterns_.push_back(std::move(copy));
    return patterns_.back().id;
}

bool Song::renamePattern(PatternId id, std::string name)
{
    Pattern* pattern = findPattern(id);
    if (pattern == nullptr)
        return false;
    pattern->name = std::move(name);
    return true;
}

bool Song::setPatternLength(PatternId id, int lengthBars)
{
    Pattern* pattern = findPattern(id);
    if (pattern == nullptr || lengthBars < 1 || lengthBars > kMaxPatternBars)
        return false;

    // Growing a pattern must not make blocks overlap in the arranger.
    for (int t = 0; t < kArrangerTracks; ++t)
        for (const auto& block : arrangement_[t].blocks)
            if (block.patternId == id && !blockFits(t, block.startBar, lengthBars, block.id))
                return false;

    pattern->lengthBars = lengthBars;
    const int endStep = pattern->lengthSteps();
    for (auto& track : pattern->tracks)
        std::erase_if(track.clips, [endStep](const Clip& c) { return c.startStep >= endStep; });
    return true;
}

bool Song::removePattern(PatternId id)
{
    const auto removed = std::erase_if(patterns_, [id](const Pattern& p) { return p.id == id; });
    if (removed == 0)
        return false;

    for (auto& track : arrangement_)
    {
        std::erase_if(track.blocks, [id](const PatternBlock& b) { return b.patternId == id; });
        if (track.patternId == id)
            track.patternId = {};
    }
    return true;
}

const Pattern* Song::findPattern(PatternId id) const noexcept
{
    return const_cast<Song*>(this)->findPattern(id);
}

Pattern* Song::findPattern(PatternId id) noexcept
{
    return findById(patterns_, id);
}

// --- Clips -------------------------------------------------------------------

std::optional<ClipId> Song::placeClip(PatternId patternId, int track, SampleId sample, double startStep,
                                      double lengthSteps, double offsetSteps)
{
    Pattern* pattern = findPattern(patternId);
    if (pattern == nullptr || findSample(sample) == nullptr || !isValidTrack(track, kTracksPerPattern) ||
        startStep < 0.0 || startStep >= pattern->lengthSteps() || lengthSteps < 0.0 || offsetSteps < 0.0)
        return std::nullopt;

    if (track >= static_cast<int>(pattern->tracks.size()))
        pattern->tracks.resize(static_cast<std::size_t>(track + 1));

    const Clip clip{clipIds_.next(), sample, startStep, lengthSteps, offsetSteps};
    auto& trackObj = pattern->tracks[static_cast<std::size_t>(track)];
    trackObj.clips.push_back(clip);
    trackObj.sampleId = sample;
    return clip.id;
}

bool Song::resizeClip(PatternId patternId, ClipId clipId, double newStartStep, double newLengthSteps, double newOffsetSteps)
{
    Pattern* pattern = findPattern(patternId);
    if (pattern == nullptr || newStartStep < 0.0 || newStartStep >= pattern->lengthSteps() ||
        newLengthSteps <= 0.0 || newOffsetSteps < 0.0)
        return false;

    const double maxAllowedLen = static_cast<double>(pattern->lengthSteps()) - newStartStep;
    newLengthSteps = std::min(newLengthSteps, maxAllowedLen);
    if (newLengthSteps <= 0.0)
        return false;

    for (auto& track : pattern->tracks)
    {
        for (auto& clip : track.clips)
        {
            if (clip.id == clipId)
            {
                clip.startStep = newStartStep;
                clip.lengthSteps = newLengthSteps;
                clip.offsetSteps = newOffsetSteps;
                return true;
            }
        }
    }
    return false;
}

bool Song::setClipEnvelope(PatternId patternId, ClipId clipId, float gain, float fadeInSteps, float fadeOutSteps)
{
    Clip* clip = findClip(patternId, clipId);
    if (clip == nullptr)
        return false;

    clip->gain = std::clamp(gain, 0.0f, 2.0f);
    clip->fadeInSteps = std::max(0.0f, fadeInSteps);
    clip->fadeOutSteps = std::max(0.0f, fadeOutSteps);
    return true;
}

namespace
{
void sortAndClampEnvelope(std::vector<EnvelopePoint>& pts, double minPos, double maxPos, float minVal, float maxVal)
{
    for (auto& p : pts)
    {
        p.position = std::clamp(p.position, minPos, maxPos);
        p.value = std::clamp(p.value, minVal, maxVal);
    }
    std::stable_sort(pts.begin(), pts.end(), [](const EnvelopePoint& a, const EnvelopePoint& b) {
        return a.position < b.position;
    });
}

std::vector<EnvelopePoint> sliceNormalizedEnvelope(const std::vector<EnvelopePoint>& src, double r0, double r1, float defaultVal)
{
    if (src.empty() || r1 <= r0 + 1e-6)
        return {};
    std::vector<EnvelopePoint> out;
    out.push_back({0.0, evaluateEnvelope(src, r0, defaultVal), EnvelopeCurve::Linear});
    for (const auto& pt : src)
    {
        if (pt.position > r0 + 1e-5 && pt.position < r1 - 1e-5)
        {
            out.push_back({(pt.position - r0) / (r1 - r0), pt.value, pt.curve});
        }
    }
    out.push_back({1.0, evaluateEnvelope(src, r1, defaultVal), EnvelopeCurve::Linear});
    return out;
}
} // namespace

bool Song::setClipVolumeEnvelope(PatternId patternId, ClipId clipId, std::vector<EnvelopePoint> points)
{
    Clip* clip = findClip(patternId, clipId);
    if (clip == nullptr)
        return false;
    sortAndClampEnvelope(points, 0.0, 1.0, 0.0f, 2.0f);
    clip->volumeEnvelope = std::move(points);
    return true;
}

bool Song::setClipPanEnvelope(PatternId patternId, ClipId clipId, std::vector<EnvelopePoint> points)
{
    Clip* clip = findClip(patternId, clipId);
    if (clip == nullptr)
        return false;
    sortAndClampEnvelope(points, 0.0, 1.0, -1.0f, 1.0f);
    clip->panEnvelope = std::move(points);
    return true;
}

bool Song::setClipReversed(PatternId patternId, ClipId clipId, bool reversed)
{
    Clip* clip = findClip(patternId, clipId);
    if (clip == nullptr)
        return false;

    clip->reversed = reversed;
    return true;
}

bool Song::isClipReversed(PatternId patternId, ClipId clipId) const noexcept
{
    const Clip* clip = findClip(patternId, clipId);
    return clip != nullptr && clip->reversed;
}

bool Song::setClipTranspose(PatternId patternId, ClipId clipId, int semitones)
{
    Clip* clip = findClip(patternId, clipId);
    if (clip == nullptr)
        return false;

    clip->transposeSemitones = std::clamp(semitones, -48, 48);
    return true;
}

int Song::clipTranspose(PatternId patternId, ClipId clipId) const noexcept
{
    const Clip* clip = findClip(patternId, clipId);
    return clip != nullptr ? clip->transposeSemitones : 0;
}

const Clip* Song::findClip(PatternId patternId, ClipId clipId) const noexcept
{
    const Pattern* pattern = findPattern(patternId);
    if (pattern == nullptr)
        return nullptr;

    for (const auto& track : pattern->tracks)
    {
        for (const auto& clip : track.clips)
        {
            if (clip.id == clipId)
                return &clip;
        }
    }
    return nullptr;
}

Clip* Song::findClip(PatternId patternId, ClipId clipId) noexcept
{
    Pattern* pattern = findPattern(patternId);
    if (pattern == nullptr)
        return nullptr;

    for (auto& track : pattern->tracks)
    {
        for (auto& clip : track.clips)
        {
            if (clip.id == clipId)
                return &clip;
        }
    }
    return nullptr;
}

bool Song::moveClip(PatternId patternId, ClipId clipId, int newTrack, double newStartStep)
{
    Pattern* pattern = findPattern(patternId);
    if (pattern == nullptr || !isValidTrack(newTrack, kTracksPerPattern) || newStartStep < 0.0 ||
        newStartStep >= pattern->lengthSteps())
        return false;

    if (newTrack >= static_cast<int>(pattern->tracks.size()))
        pattern->tracks.resize(static_cast<std::size_t>(newTrack + 1));

    for (auto& track : pattern->tracks)
    {
        auto it = std::find_if(track.clips.begin(), track.clips.end(), [clipId](const Clip& c) { return c.id == clipId; });
        if (it == track.clips.end())
            continue;

        Clip clip = *it;
        clip.startStep = newStartStep;
        track.clips.erase(it);
        pattern->tracks[static_cast<std::size_t>(newTrack)].clips.push_back(clip);
        return true;
    }
    return false;
}

bool Song::moveClipBetweenPatterns(PatternId fromPattern, PatternId toPattern, ClipId clipId, double newStartStep)
{
    Pattern* fromP = findPattern(fromPattern);
    Pattern* toP = findPattern(toPattern);
    if (fromP == nullptr || toP == nullptr || newStartStep < 0.0 || newStartStep >= toP->lengthSteps())
        return false;

    if (toP->tracks.empty())
        toP->tracks.resize(1);

    for (auto& track : fromP->tracks)
    {
        auto it = std::find_if(track.clips.begin(), track.clips.end(), [clipId](const Clip& c) { return c.id == clipId; });
        if (it == track.clips.end())
            continue;

        Clip clip = *it;
        clip.startStep = newStartStep;
        track.clips.erase(it);
        toP->tracks[0].clips.push_back(clip);
        return true;
    }
    return false;
}

bool Song::removeClip(PatternId patternId, ClipId clipId)
{
    Pattern* pattern = findPattern(patternId);
    if (pattern == nullptr)
        return false;

    for (auto& track : pattern->tracks)
        if (std::erase_if(track.clips, [clipId](const Clip& c) { return c.id == clipId; }) > 0)
            return true;
    return false;
}

std::optional<std::pair<ClipId, ClipId>> Song::splitClip(PatternId patternId, ClipId clipId, double splitStep)
{
    Pattern* pattern = findPattern(patternId);
    if (pattern == nullptr)
        return std::nullopt;

    for (std::size_t t = 0; t < pattern->tracks.size(); ++t)
    {
        auto& track = pattern->tracks[t];
        for (std::size_t i = 0; i < track.clips.size(); ++i)
        {
            if (track.clips[i].id != clipId)
                continue;

            Clip orig = track.clips[i];
            const double span = defaultClipSpanSteps(orig, static_cast<int>(t), patternId);
            const double endStep = orig.startStep + span;
            if (splitStep <= orig.startStep + 0.05 || splitStep >= endStep - 0.05)
                return std::nullopt;

            const double leftLen = splitStep - orig.startStep;
            const double rightLen = endStep - splitStep;
            const double ratio = leftLen / span;

            const SampleKind kind = patternTrackKind(patternId, static_cast<int>(t));
            const Sample* sample = findSample(orig.sampleId);

            Clip leftClip = orig;
            leftClip.lengthSteps = leftLen;
            leftClip.fadeInSteps = std::min(orig.fadeInSteps, static_cast<float>(leftLen));
            leftClip.fadeOutSteps = 0.0f;
            if (!orig.volumeEnvelope.empty())
                leftClip.volumeEnvelope = sliceNormalizedEnvelope(orig.volumeEnvelope, 0.0, ratio, orig.gain);
            if (!orig.panEnvelope.empty())
                leftClip.panEnvelope = sliceNormalizedEnvelope(orig.panEnvelope, 0.0, ratio, 0.0f);

            Clip rightClip = orig;
            rightClip.id = clipIds_.next();
            rightClip.startStep = splitStep;
            rightClip.lengthSteps = rightLen;
            if (kind == SampleKind::Loop && sample != nullptr && sample->loopCycleSteps() > 0.0)
                rightClip.offsetSteps = std::fmod(orig.offsetSteps + leftLen, sample->loopCycleSteps());
            else
                rightClip.offsetSteps = orig.offsetSteps + leftLen;
            rightClip.fadeInSteps = 0.0f;
            rightClip.fadeOutSteps = std::min(orig.fadeOutSteps, static_cast<float>(rightLen));
            if (!orig.volumeEnvelope.empty())
                rightClip.volumeEnvelope = sliceNormalizedEnvelope(orig.volumeEnvelope, ratio, 1.0, orig.gain);
            if (!orig.panEnvelope.empty())
                rightClip.panEnvelope = sliceNormalizedEnvelope(orig.panEnvelope, ratio, 1.0, 0.0f);

            track.clips[i] = std::move(leftClip);
            const ClipId rightId = rightClip.id;
            track.clips.insert(track.clips.begin() + static_cast<std::ptrdiff_t>(i + 1), std::move(rightClip));
            return std::make_pair(clipId, rightId);
        }
    }
    return std::nullopt;
}

std::optional<ClipId> Song::joinClips(PatternId patternId, const std::vector<ClipId>& clipIds)
{
    Pattern* pattern = findPattern(patternId);
    if (pattern == nullptr || clipIds.size() < 2)
        return std::nullopt;

    std::optional<ClipId> firstJoinedId;

    for (std::size_t t = 0; t < pattern->tracks.size(); ++t)
    {
        auto& track = pattern->tracks[t];
        std::vector<Clip> matched;
        for (const auto& c : track.clips)
        {
            if (std::find(clipIds.begin(), clipIds.end(), c.id) != clipIds.end())
                matched.push_back(c);
        }

        if (matched.size() < 2)
            continue;

        std::sort(matched.begin(), matched.end(), [](const Clip& a, const Clip& b) {
            return a.startStep < b.startStep;
        });

        const double minStart = matched.front().startStep;
        double maxEnd = minStart;
        for (const auto& c : matched)
        {
            const double span = defaultClipSpanSteps(c, static_cast<int>(t), patternId);
            maxEnd = std::max(maxEnd, c.startStep + span);
        }

        const ClipId keepId = matched.front().id;
        const float endFadeOut = matched.back().fadeOutSteps;

        for (auto it = track.clips.begin(); it != track.clips.end();)
        {
            if (it->id == keepId)
            {
                it->startStep = minStart;
                it->lengthSteps = maxEnd - minStart;
                it->fadeOutSteps = endFadeOut;
                ++it;
            }
            else if (std::find_if(matched.begin(), matched.end(), [&](const Clip& m) { return m.id == it->id; }) != matched.end())
            {
                it = track.clips.erase(it);
            }
            else
            {
                ++it;
            }
        }

        if (!firstJoinedId.has_value())
            firstJoinedId = keepId;
    }

    return firstJoinedId;
}

bool Song::setTrackAutomation(PatternId patternId, int trackIndex, TrackEnvelopeType type, std::vector<EnvelopePoint> points)
{
    Pattern* pattern = findPattern(patternId);
    if (pattern == nullptr || !isValidTrack(trackIndex, kTracksPerPattern) ||
        trackIndex >= static_cast<int>(pattern->tracks.size()))
        return false;

    auto& track = pattern->tracks[static_cast<std::size_t>(trackIndex)];
    const double maxSteps = static_cast<double>(pattern->lengthSteps());
    if (type == TrackEnvelopeType::Volume)
    {
        sortAndClampEnvelope(points, 0.0, maxSteps, 0.0f, 2.0f);
        track.volumeAutomation = std::move(points);
        return true;
    }
    if (type == TrackEnvelopeType::Pan)
    {
        sortAndClampEnvelope(points, 0.0, maxSteps, -1.0f, 1.0f);
        track.panAutomation = std::move(points);
        return true;
    }
    if (type == TrackEnvelopeType::FxSend)
    {
        sortAndClampEnvelope(points, 0.0, maxSteps, 0.0f, 1.0f);
        track.fxAutomation = std::move(points);
        return true;
    }
    return false;
}

bool Song::setTrackFxSend(PatternId patternId, int trackIndex, float fxSend)
{
    Pattern* pattern = findPattern(patternId);
    if (pattern == nullptr || !isValidTrack(trackIndex, kTracksPerPattern) ||
        trackIndex >= static_cast<int>(pattern->tracks.size()))
        return false;
    pattern->tracks[static_cast<std::size_t>(trackIndex)].fxSend = std::clamp(fxSend, 0.0f, 1.0f);
    return true;
}

int Song::addMarker(PatternId patternId, double step, std::string name)
{
    Pattern* pattern = findPattern(patternId);
    if (pattern == nullptr)
        return -1;
    int nextId = 1;
    for (const auto& m : pattern->markers)
        nextId = std::max(nextId, m.id + 1);
    const double clampedStep = std::clamp(step, 0.0, static_cast<double>(pattern->lengthSteps()));
    if (name.empty())
        name = "Marker " + std::to_string(nextId);
    pattern->markers.push_back({nextId, clampedStep, std::move(name)});
    std::stable_sort(pattern->markers.begin(), pattern->markers.end(),
                     [](const TimelineMarker& a, const TimelineMarker& b) { return a.step < b.step; });
    return nextId;
}

bool Song::moveMarker(PatternId patternId, int markerId, double newStep)
{
    Pattern* pattern = findPattern(patternId);
    if (pattern == nullptr)
        return false;
    for (auto& m : pattern->markers)
    {
        if (m.id == markerId)
        {
            m.step = std::clamp(newStep, 0.0, static_cast<double>(pattern->lengthSteps()));
            std::stable_sort(pattern->markers.begin(), pattern->markers.end(),
                             [](const TimelineMarker& a, const TimelineMarker& b) { return a.step < b.step; });
            return true;
        }
    }
    return false;
}

bool Song::renameMarker(PatternId patternId, int markerId, std::string newName)
{
    Pattern* pattern = findPattern(patternId);
    if (pattern == nullptr)
        return false;
    for (auto& m : pattern->markers)
    {
        if (m.id == markerId)
        {
            m.name = std::move(newName);
            return true;
        }
    }
    return false;
}

bool Song::removeMarker(PatternId patternId, int markerId)
{
    Pattern* pattern = findPattern(patternId);
    if (pattern == nullptr)
        return false;
    return std::erase_if(pattern->markers, [markerId](const TimelineMarker& m) { return m.id == markerId; }) > 0;
}

bool Song::setPatternTrackSample(PatternId patternId, int track, SampleId sample)
{
    Pattern* pattern = findPattern(patternId);
    if (pattern == nullptr || !isValidTrack(track, kTracksPerPattern))
        return false;

    if (track >= static_cast<int>(pattern->tracks.size()))
        pattern->tracks.resize(static_cast<std::size_t>(track + 1));

    pattern->tracks[static_cast<std::size_t>(track)].sampleId = sample;
    return true;
}

bool Song::replaceTrackSample(PatternId patternId, int track, SampleId newSampleId)
{
    Pattern* pattern = findPattern(patternId);
    const Sample* newSample = findSample(newSampleId);
    if (pattern == nullptr || !isValidTrack(track, kTracksPerPattern) || newSample == nullptr)
        return false;

    if (track >= static_cast<int>(pattern->tracks.size()))
        pattern->tracks.resize(static_cast<std::size_t>(track + 1));

    auto& t = pattern->tracks[static_cast<std::size_t>(track)];
    t.sampleId = newSampleId;
    t.kindOverride = std::nullopt;
    for (auto& clip : t.clips)
        clip.sampleId = newSampleId;
    return true;
}

SampleId Song::patternTrackSample(PatternId patternId, int track) const noexcept
{
    const Pattern* pattern = findPattern(patternId);
    if (pattern == nullptr || !isValidTrack(track, kTracksPerPattern))
        return {};
    if (track >= static_cast<int>(pattern->tracks.size()))
        return {};
    const auto& t = pattern->tracks[static_cast<std::size_t>(track)];
    if (t.sampleId.isValid())
        return t.sampleId;
    if (!t.clips.empty())
        return t.clips.front().sampleId;
    return {};
}

int Song::addTrackToPattern(PatternId patternId, SampleId sample)
{
    Pattern* pattern = findPattern(patternId);
    if (pattern == nullptr || pattern->tracks.size() >= kTracksPerPattern)
        return -1;

    PatternTrack newTrack;
    newTrack.sampleId = sample;
    pattern->tracks.push_back(std::move(newTrack));
    return static_cast<int>(pattern->tracks.size() - 1);
}

bool Song::removeTrackFromPattern(PatternId patternId, int trackIndex)
{
    Pattern* pattern = findPattern(patternId);
    if (pattern == nullptr || trackIndex < 0 || trackIndex >= static_cast<int>(pattern->tracks.size()))
        return false;

    // Always keep at least 1 track in the pattern
    if (pattern->tracks.size() <= 1)
    {
        pattern->tracks.front().clips.clear();
        pattern->tracks.front().sampleId = {};
        pattern->tracks.front().mix = {};
        pattern->tracks.front().muted = false;
        pattern->tracks.front().solo = false;
        return true;
    }

    pattern->tracks.erase(pattern->tracks.begin() + trackIndex);
    return true;
}

int Song::duplicateTrackInPattern(PatternId patternId, int trackIndex)
{
    Pattern* pattern = findPattern(patternId);
    if (pattern == nullptr || trackIndex < 0 || trackIndex >= static_cast<int>(pattern->tracks.size()) ||
        pattern->tracks.size() >= kTracksPerPattern)
        return -1;

    PatternTrack newTrack = pattern->tracks[static_cast<std::size_t>(trackIndex)];
    for (auto& clip : newTrack.clips)
        clip.id = clipIds_.next();

    pattern->tracks.push_back(std::move(newTrack));
    return static_cast<int>(pattern->tracks.size() - 1);
}

bool Song::movePatternTrack(PatternId patternId, int fromIndex, int toIndex)
{
    Pattern* pattern = findPattern(patternId);
    if (pattern == nullptr)
        return false;
    const int count = static_cast<int>(pattern->tracks.size());
    if (fromIndex < 0 || fromIndex >= count || toIndex < 0 || toIndex >= count || fromIndex == toIndex)
        return false;

    auto track = std::move(pattern->tracks[static_cast<std::size_t>(fromIndex)]);
    pattern->tracks.erase(pattern->tracks.begin() + fromIndex);
    pattern->tracks.insert(pattern->tracks.begin() + toIndex, std::move(track));
    return true;
}

bool Song::renamePatternTrack(PatternId patternId, int trackIndex, std::string name)
{
    Pattern* pattern = findPattern(patternId);
    if (pattern == nullptr || !isValidTrack(trackIndex, kTracksPerPattern))
        return false;
    if (trackIndex >= static_cast<int>(pattern->tracks.size()))
        pattern->tracks.resize(static_cast<std::size_t>(trackIndex + 1));
    pattern->tracks[static_cast<std::size_t>(trackIndex)].name = std::move(name);
    return true;
}

std::string Song::patternTrackName(PatternId patternId, int trackIndex) const
{
    const Pattern* pattern = findPattern(patternId);
    if (pattern == nullptr || !isValidTrack(trackIndex, kTracksPerPattern) ||
        trackIndex >= static_cast<int>(pattern->tracks.size()))
        return {};
    return pattern->tracks[static_cast<std::size_t>(trackIndex)].name;
}

bool Song::setPatternTrackKind(PatternId patternId, int trackIndex, SampleKind kind)
{
    Pattern* pattern = findPattern(patternId);
    if (pattern == nullptr || !isValidTrack(trackIndex, kTracksPerPattern))
        return false;
    if (trackIndex >= static_cast<int>(pattern->tracks.size()))
        pattern->tracks.resize(static_cast<std::size_t>(trackIndex + 1));
    auto& track = pattern->tracks[static_cast<std::size_t>(trackIndex)];
    track.kindOverride = kind;

    // Reset explicit length on clips so they immediately adopt the new kind's visual representation & span
    for (auto& clip : track.clips)
    {
        clip.lengthSteps = 0;
        clip.offsetSteps = 0;
    }

    if (kind == SampleKind::Loop)
    {
        SampleId sid = track.sampleId;
        if (!sid.isValid() && !track.clips.empty())
            sid = track.clips.front().sampleId;
        if (Sample* s = findSample(sid))
        {
            s->kind = SampleKind::Loop;
            if (s->durationSeconds > 0.0 && s->acidBpm <= 0.0)
            {
                const double refBpm = (bpm_ > 0.0) ? bpm_ : 120.0;
                const double secPerBar = (60.0 / refBpm) * 4.0;
                const int estBars = std::max(1, static_cast<int>(std::round(s->durationSeconds / secPerBar)));
                s->loopBars = estBars;
                s->loopBeats = static_cast<double>(estBars * kBeatsPerBar);
            }
        }
    }
    return true;
}

SampleKind Song::patternTrackKind(PatternId patternId, int trackIndex) const
{
    const Pattern* pattern = findPattern(patternId);
    if (pattern == nullptr || !isValidTrack(trackIndex, kTracksPerPattern) ||
        trackIndex >= static_cast<int>(pattern->tracks.size()))
        return SampleKind::OneShot;

    const auto& t = pattern->tracks[static_cast<std::size_t>(trackIndex)];
    if (t.kindOverride.has_value())
        return *t.kindOverride;

    SampleId sid = t.sampleId;
    if (!sid.isValid() && !t.clips.empty())
        sid = t.clips.front().sampleId;
    if (const Sample* s = findSample(sid))
        return s->kind;

    return SampleKind::OneShot;
}

// --- Arrangement -------------------------------------------------------------

std::optional<BlockId> Song::placeBlock(int arrangerTrack, PatternId patternId, int startBar)
{
    const int length = patternLength(patternId);
    if (length == 0 || !isValidTrack(arrangerTrack, kArrangerTracks) || startBar < 0 ||
        startBar + length > kMaxBars || !blockFits(arrangerTrack, startBar, length, BlockId{}))
        return std::nullopt;

    auto& track = arrangement_[static_cast<std::size_t>(arrangerTrack)];
    if (!track.patternId.isValid())
    {
        track.patternId = patternId;
    }
    else if (track.patternId != patternId)
    {
        // Enforce: each arranger track is dedicated to one pattern
        return std::nullopt;
    }

    const PatternBlock block{blockIds_.next(), patternId, startBar};
    track.blocks.push_back(block);
    return block.id;
}

bool Song::moveBlock(BlockId blockId, int newTrack, int newStartBar)
{
    if (!isValidTrack(newTrack, kArrangerTracks) || newStartBar < 0)
        return false;

    for (std::size_t t = 0; t < arrangement_.size(); ++t)
    {
        auto& track = arrangement_[t];
        auto it = std::find_if(track.blocks.begin(), track.blocks.end(),
                               [blockId](const PatternBlock& b) { return b.id == blockId; });
        if (it == track.blocks.end())
            continue;

        if (static_cast<int>(t) != newTrack)
        {
            auto& destTrack = arrangement_[static_cast<std::size_t>(newTrack)];
            if (destTrack.patternId.isValid() && destTrack.patternId != it->patternId)
                return false;
            if (!destTrack.patternId.isValid())
                destTrack.patternId = it->patternId;
        }

        const int length = patternLength(it->patternId);
        if (newStartBar + length > kMaxBars || !blockFits(newTrack, newStartBar, length, blockId))
            return false;

        PatternBlock block = *it;
        block.startBar = newStartBar;
        track.blocks.erase(it);
        arrangement_[static_cast<std::size_t>(newTrack)].blocks.push_back(block);
        return true;
    }
    return false;
}

bool Song::removeBlock(BlockId blockId)
{
    for (auto& track : arrangement_)
        if (std::erase_if(track.blocks, [blockId](const PatternBlock& b) { return b.id == blockId; }) > 0)
            return true;
    return false;
}

bool Song::moveArrangerTrack(int fromIndex, int toIndex)
{
    if (!isValidTrack(fromIndex, kArrangerTracks) || !isValidTrack(toIndex, kArrangerTracks) || fromIndex == toIndex)
        return false;

    ArrangerTrack trk = std::move(arrangement_[static_cast<std::size_t>(fromIndex)]);
    if (fromIndex < toIndex)
    {
        for (int i = fromIndex; i < toIndex; ++i)
            arrangement_[static_cast<std::size_t>(i)] = std::move(arrangement_[static_cast<std::size_t>(i + 1)]);
    }
    else
    {
        for (int i = fromIndex; i > toIndex; --i)
            arrangement_[static_cast<std::size_t>(i)] = std::move(arrangement_[static_cast<std::size_t>(i - 1)]);
    }
    arrangement_[static_cast<std::size_t>(toIndex)] = std::move(trk);
    return true;
}

bool Song::deleteArrangerTrack(int trackIndex)
{
    if (!isValidTrack(trackIndex, kArrangerTracks))
        return false;

    for (int i = trackIndex; i < kArrangerTracks - 1; ++i)
        arrangement_[static_cast<std::size_t>(i)] = std::move(arrangement_[static_cast<std::size_t>(i + 1)]);

    arrangement_[kArrangerTracks - 1] = ArrangerTrack{};
    return true;
}

int Song::numArrangerTracks() const noexcept
{
    int count = 0;
    for (int t = 0; t < kArrangerTracks; ++t)
    {
        const auto& trk = arrangement_[static_cast<std::size_t>(t)];
        if (trk.patternId.isValid() || !trk.blocks.empty() || !trk.name.empty())
            count = t + 1;
    }
    return std::max(1, count);
}

int Song::addArrangerTrack(PatternId patternId)
{
    int target = -1;
    for (int t = 0; t < kArrangerTracks; ++t)
    {
        const auto& trk = arrangement_[static_cast<std::size_t>(t)];
        if (!trk.patternId.isValid() && trk.blocks.empty() && trk.name.empty())
        {
            target = t;
            break;
        }
    }
    if (target < 0)
        return -1;

    if (patternId.isValid())
        setArrangerTrackPattern(target, patternId);
    else
        renameArrangerTrack(target, "Track " + std::to_string(target + 1));
    return target;
}

ArrangerTrack* Song::arrangerTrack(int index) noexcept
{
    return isValidTrack(index, kArrangerTracks) ? &arrangement_[static_cast<std::size_t>(index)] : nullptr;
}

bool Song::renameArrangerTrack(int trackIndex, std::string name)
{
    if (!isValidTrack(trackIndex, kArrangerTracks))
        return false;
    arrangement_[static_cast<std::size_t>(trackIndex)].name = std::move(name);
    return true;
}

std::string Song::arrangerTrackName(int trackIndex) const
{
    if (!isValidTrack(trackIndex, kArrangerTracks))
        return {};
    return arrangement_[static_cast<std::size_t>(trackIndex)].name;
}

PatternId Song::arrangerTrackPattern(int trackIndex) const
{
    if (!isValidTrack(trackIndex, kArrangerTracks))
        return {};
    return arrangement_[static_cast<std::size_t>(trackIndex)].patternId;
}

bool Song::setArrangerTrackPattern(int trackIndex, PatternId patternId)
{
    if (!isValidTrack(trackIndex, kArrangerTracks))
        return false;
    auto& track = arrangement_[static_cast<std::size_t>(trackIndex)];
    if (track.patternId == patternId)
        return true;

    if (track.patternId.isValid() && patternId.isValid() && track.patternId != patternId)
    {
        std::erase_if(track.blocks, [patternId](const PatternBlock& b) { return b.patternId != patternId; });
    }
    track.patternId = patternId;
    return true;
}

int Song::findTrackForPattern(PatternId patternId) const
{
    if (!patternId.isValid())
        return -1;
    for (int t = 0; t < kArrangerTracks; ++t)
    {
        if (arrangement_[static_cast<std::size_t>(t)].patternId == patternId)
            return t;
    }
    return -1;
}

int Song::lengthBars() const noexcept
{
    int end = 0;
    for (const auto& track : arrangement_)
        for (const auto& block : track.blocks)
            end = std::max(end, block.startBar + patternLength(block.patternId));
    if (end > 0)
        return end;
    if (!patterns_.empty())
        return patterns_.front().lengthBars;
    return 0;
}

// --- Helpers -----------------------------------------------------------------

bool Song::blockFits(int track, int startBar, int lengthBars, BlockId ignore) const noexcept
{
    const int endBar = startBar + lengthBars;
    for (const auto& other : arrangement_[static_cast<std::size_t>(track)].blocks)
    {
        if (other.id == ignore)
            continue;
        const int otherStart = other.startBar;
        const int otherEnd = otherStart + patternLength(other.patternId);
        if (startBar < otherEnd && otherStart < endBar)
            return false;
    }
    return true;
}

int Song::patternLength(PatternId id) const noexcept
{
    const Pattern* pattern = findPattern(id);
    return pattern != nullptr ? pattern->lengthBars : 0;
}

} // namespace trami
