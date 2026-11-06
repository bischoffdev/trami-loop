#include "engine/SnapshotBuilder.h"

#include "model/Timing.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace trami
{

std::unique_ptr<PlaybackSnapshot> SnapshotBuilder::buildPattern(const Song& song, PatternId patternId,
                                                                const SampleBank& bank, double sampleRate)
{
    auto snapshot = std::make_unique<PlaybackSnapshot>();
    const Timing timing(song.bpm(), sampleRate, song.swing());
    snapshot->samplesPerBeat = timing.samplesPerStep() * kStepsPerBeat;

    const Pattern* pattern = song.findPattern(patternId);
    if (pattern == nullptr)
        return snapshot;

    snapshot->lengthSamples = std::llround(timing.samplesPerBar() * pattern->lengthBars);
    snapshot->looping = true;
    snapshot->hasTrackMixes = true;

    for (std::size_t t = 0; t < pattern->tracks.size() && t < 16; ++t)
    {
        const auto& trk = pattern->tracks[t];
        snapshot->trackMixes[t] = {trk.mix.volume, trk.mix.pan, trk.muted, trk.solo};
    }

    const bool anySolo = std::any_of(pattern->tracks.begin(), pattern->tracks.end(),
                                     [](const PatternTrack& t) { return t.solo; });

    for (int t = 0; t < static_cast<int>(pattern->tracks.size()); ++t)
    {
        const auto& track = pattern->tracks[static_cast<std::size_t>(t)];
        if (anySolo ? (!track.solo || track.muted) : track.muted)
            continue;

        const float master = song.masterVolume();

        for (const auto& clip : track.clips)
        {
            auto found = bank.find(clip.sampleId);
            if (found == bank.end() || found->second == nullptr)
                continue;

            const double stepDurationSamples = timing.samplesPerStep();
            const std::int64_t audioOffset = static_cast<std::int64_t>(std::round(clip.offsetSteps * stepDurationSamples));
            const std::int64_t maxFrames = (clip.lengthSteps > 0)
                ? static_cast<std::int64_t>(std::round(clip.lengthSteps * stepDurationSamples))
                : -1;

            snapshot->events.push_back({timing.stepToSample(clip.startStep), found->second.get(), master,
                                        master, audioOffset, maxFrames, t});
            snapshot->keepAlive.push_back(found->second);
        }
    }

    std::stable_sort(snapshot->events.begin(), snapshot->events.end(),
                     [](const PlaybackEvent& a, const PlaybackEvent& b) { return a.startSample < b.startSample; });
    return snapshot;
}

std::unique_ptr<PlaybackSnapshot> SnapshotBuilder::buildArrangement(const Song& song, const SampleBank& bank,
                                                                    double sampleRate)
{
    auto snapshot = std::make_unique<PlaybackSnapshot>();
    const Timing timing(song.bpm(), sampleRate, song.swing());
    snapshot->samplesPerBeat = timing.samplesPerStep() * kStepsPerBeat;

    const int songBars = song.lengthBars();
    if (songBars == 0)
        return snapshot;

    snapshot->lengthSamples = std::llround(timing.samplesPerBar() * songBars);
    snapshot->looping = false;
    snapshot->hasTrackMixes = true;

    const auto& arrangement = song.arrangement();
    for (std::size_t at = 0; at < arrangement.size() && at < 16; ++at)
    {
        const auto& trk = arrangement[at];
        snapshot->trackMixes[at] = {trk.mix.volume, trk.mix.pan, trk.muted, trk.solo};
    }

    const bool anyArrSolo = std::any_of(arrangement.begin(), arrangement.end(),
                                        [](const ArrangerTrack& at) { return at.solo; });

    for (int at = 0; at < kArrangerTracks; ++at)
    {
        const auto& arrangerTrack = arrangement[static_cast<std::size_t>(at)];
        if (anyArrSolo ? (!arrangerTrack.solo || arrangerTrack.muted) : arrangerTrack.muted)
            continue;

        for (const auto& block : arrangerTrack.blocks)
        {
            const Pattern* pattern = song.findPattern(block.patternId);
            if (pattern == nullptr)
                continue;

            const std::int64_t blockBaseStep = static_cast<std::int64_t>(block.startBar) * kStepsPerBar;

            const bool anyPatSolo = std::any_of(pattern->tracks.begin(), pattern->tracks.end(),
                                                [](const PatternTrack& pt) { return pt.solo; });

            for (const auto& patTrack : pattern->tracks)
            {
                if (anyPatSolo ? (!patTrack.solo || patTrack.muted) : patTrack.muted)
                    continue;

                const auto [patPanL, patPanR] = panGains(patTrack.mix.pan);
                const float innerGain = song.masterVolume() * patTrack.mix.volume;
                const float combinedGainL = innerGain * patPanL;
                const float combinedGainR = innerGain * patPanR;

                for (const auto& clip : patTrack.clips)
                {
                    auto found = bank.find(clip.sampleId);
                    if (found == bank.end() || found->second == nullptr)
                        continue;

                    const std::int64_t globalStep = blockBaseStep + clip.startStep;
                    const std::int64_t startSample = timing.stepToSample(globalStep);
                    const double stepDurationSamples = timing.samplesPerStep();
                    const std::int64_t audioOffset = static_cast<std::int64_t>(std::round(clip.offsetSteps * stepDurationSamples));
                    const std::int64_t maxFrames = (clip.lengthSteps > 0)
                        ? static_cast<std::int64_t>(std::round(clip.lengthSteps * stepDurationSamples))
                        : -1;

                    snapshot->events.push_back({startSample, found->second.get(), combinedGainL, combinedGainR,
                                                audioOffset, maxFrames, at});
                    snapshot->keepAlive.push_back(found->second);
                }
            }
        }
    }

    std::stable_sort(snapshot->events.begin(), snapshot->events.end(),
                     [](const PlaybackEvent& a, const PlaybackEvent& b) { return a.startSample < b.startSample; });
    return snapshot;
}

std::pair<float, float> SnapshotBuilder::panGains(float pan) noexcept
{
    const double angle = (std::clamp(pan, -1.0f, 1.0f) + 1.0) * std::numbers::pi / 4.0;
    const double unityAtCentre = std::numbers::sqrt2;
    return {static_cast<float>(std::cos(angle) * unityAtCentre), static_cast<float>(std::sin(angle) * unityAtCentre)};
}

} // namespace trami
