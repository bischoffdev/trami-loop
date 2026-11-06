#include "model/Song.h"

#include <algorithm>

namespace trami
{
namespace
{

constexpr double kMinBpm = 20.0;
constexpr double kMaxBpm = 300.0;
constexpr int kMaxPatternBars = 64;

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

// --- Samples -----------------------------------------------------------------

SampleId Song::addSample(Sample sample)
{
    sample.id = sampleIds_.next();
    sample.loopBars = std::max(1, sample.loopBars);
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
        std::erase_if(track.blocks, [id](const PatternBlock& b) { return b.patternId == id; });
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

std::optional<ClipId> Song::placeClip(PatternId patternId, int track, SampleId sample, int startStep,
                                      int lengthSteps, int offsetSteps)
{
    Pattern* pattern = findPattern(patternId);
    if (pattern == nullptr || findSample(sample) == nullptr || !isValidTrack(track, kTracksPerPattern) ||
        startStep < 0 || startStep >= pattern->lengthSteps() || lengthSteps < 0 || offsetSteps < 0)
        return std::nullopt;

    const Clip clip{clipIds_.next(), sample, startStep, lengthSteps, offsetSteps};
    auto& trackObj = pattern->tracks[static_cast<std::size_t>(track)];
    trackObj.clips.push_back(clip);
    trackObj.sampleId = sample;
    return clip.id;
}

bool Song::resizeClip(PatternId patternId, ClipId clipId, int newStartStep, int newLengthSteps, int newOffsetSteps)
{
    Pattern* pattern = findPattern(patternId);
    if (pattern == nullptr || newStartStep < 0 || newStartStep >= pattern->lengthSteps() ||
        newLengthSteps <= 0 || newOffsetSteps < 0)
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

bool Song::moveClip(PatternId patternId, ClipId clipId, int newTrack, int newStartStep)
{
    Pattern* pattern = findPattern(patternId);
    if (pattern == nullptr || !isValidTrack(newTrack, kTracksPerPattern) || newStartStep < 0 ||
        newStartStep >= pattern->lengthSteps())
        return false;

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

bool Song::setPatternTrackSample(PatternId patternId, int track, SampleId sample)
{
    Pattern* pattern = findPattern(patternId);
    if (pattern == nullptr || !isValidTrack(track, kTracksPerPattern))
        return false;
    pattern->tracks[static_cast<std::size_t>(track)].sampleId = sample;
    return true;
}

bool Song::replaceTrackSample(PatternId patternId, int track, SampleId newSampleId)
{
    Pattern* pattern = findPattern(patternId);
    if (pattern == nullptr || !isValidTrack(track, kTracksPerPattern) || findSample(newSampleId) == nullptr)
        return false;
    auto& t = pattern->tracks[static_cast<std::size_t>(track)];
    t.sampleId = newSampleId;
    for (auto& clip : t.clips)
        clip.sampleId = newSampleId;
    return true;
}

SampleId Song::patternTrackSample(PatternId patternId, int track) const noexcept
{
    const Pattern* pattern = findPattern(patternId);
    if (pattern == nullptr || !isValidTrack(track, kTracksPerPattern))
        return {};
    const auto& t = pattern->tracks[static_cast<std::size_t>(track)];
    if (t.sampleId.isValid())
        return t.sampleId;
    if (!t.clips.empty())
        return t.clips.front().sampleId;
    return {};
}

// --- Arrangement -------------------------------------------------------------

std::optional<BlockId> Song::placeBlock(int arrangerTrack, PatternId patternId, int startBar)
{
    const int length = patternLength(patternId);
    if (length == 0 || !isValidTrack(arrangerTrack, kArrangerTracks) || startBar < 0 ||
        startBar + length > kMaxBars || !blockFits(arrangerTrack, startBar, length, BlockId{}))
        return std::nullopt;

    const PatternBlock block{blockIds_.next(), patternId, startBar};
    arrangement_[static_cast<std::size_t>(arrangerTrack)].blocks.push_back(block);
    return block.id;
}

bool Song::moveBlock(BlockId blockId, int newTrack, int newStartBar)
{
    if (!isValidTrack(newTrack, kArrangerTracks) || newStartBar < 0)
        return false;

    for (auto& track : arrangement_)
    {
        auto it = std::find_if(track.blocks.begin(), track.blocks.end(),
                               [blockId](const PatternBlock& b) { return b.id == blockId; });
        if (it == track.blocks.end())
            continue;

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

ArrangerTrack* Song::arrangerTrack(int index) noexcept
{
    return isValidTrack(index, kArrangerTracks) ? &arrangement_[static_cast<std::size_t>(index)] : nullptr;
}

int Song::lengthBars() const noexcept
{
    int end = 0;
    for (const auto& track : arrangement_)
        for (const auto& block : track.blocks)
            end = std::max(end, block.startBar + patternLength(block.patternId));
    return end;
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
