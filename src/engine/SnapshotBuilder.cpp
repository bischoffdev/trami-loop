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

    for (const auto& track : pattern->tracks)
    {
        const auto [panL, panR] = panGains(track.mix.pan);
        const float gain = track.mix.volume * song.masterVolume();

        for (const auto& clip : track.clips)
        {
            auto found = bank.find(clip.sampleId);
            if (found == bank.end() || found->second == nullptr)
                continue;

            snapshot->events.push_back({timing.stepToSample(clip.startStep), found->second.get(), gain * panL,
                                        gain * panR});
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

    const auto& arrangement = song.arrangement();
    for (int at = 0; at < kArrangerTracks; ++at)
    {
        const auto& arrangerTrack = arrangement[static_cast<std::size_t>(at)];
        if (arrangerTrack.muted)
            continue;

        const auto [arrPanL, arrPanR] = panGains(arrangerTrack.mix.pan);
        const float arrGain = arrangerTrack.mix.volume * song.masterVolume();

        for (const auto& block : arrangerTrack.blocks)
        {
            const Pattern* pattern = song.findPattern(block.patternId);
            if (pattern == nullptr)
                continue;

            const std::int64_t blockBaseStep = static_cast<std::int64_t>(block.startBar) * kStepsPerBar;

            for (const auto& patTrack : pattern->tracks)
            {
                const auto [patPanL, patPanR] = panGains(patTrack.mix.pan);
                const float combinedGainL = arrGain * patTrack.mix.volume * arrPanL * patPanL;
                const float combinedGainR = arrGain * patTrack.mix.volume * arrPanR * patPanR;

                for (const auto& clip : patTrack.clips)
                {
                    auto found = bank.find(clip.sampleId);
                    if (found == bank.end() || found->second == nullptr)
                        continue;

                    const std::int64_t globalStep = blockBaseStep + clip.startStep;
                    const std::int64_t startSample = timing.stepToSample(globalStep);

                    snapshot->events.push_back({startSample, found->second.get(), combinedGainL, combinedGainR});
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
