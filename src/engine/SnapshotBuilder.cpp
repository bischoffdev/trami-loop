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

    for (const auto& track : pattern->tracks)
    {
        const auto [panL, panR] = panGains(track.mix.pan);
        const float gain = track.mix.volume * song.masterVolume();

        for (const auto& clip : track.clips)
        {
            auto found = bank.find(clip.sampleId);
            if (found == bank.end() || found->second == nullptr)
                continue;

            // Note: loops play unstretched until the stretch cache exists (milestone 3).
            snapshot->events.push_back({timing.stepToSample(clip.startStep), found->second.get(), gain * panL,
                                        gain * panR});
            snapshot->keepAlive.push_back(found->second);
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
