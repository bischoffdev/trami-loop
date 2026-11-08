#include "engine/SnapshotBuilder.h"

#include "model/Timing.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace trami
{

std::unique_ptr<PlaybackSnapshot> SnapshotBuilder::buildPattern(const Song& song, PatternId patternId,
                                                                const SampleBank& bank, double sampleRate,
                                                                int loopStartStep, int loopEndStep,
                                                                const SampleBank* rawBank)
{
    auto snapshot = std::make_unique<PlaybackSnapshot>();
    const Timing timing(song.bpm(), sampleRate, song.swing());
    snapshot->samplesPerBeat = timing.samplesPerStep() * kStepsPerBeat;

    if (song.patterns().empty())
        return snapshot;

    const Pattern* p = song.findPattern(patternId);
    if (p == nullptr)
        p = &song.patterns().front();

    const int maxBars = std::max(1, p->lengthBars);
    snapshot->lengthSamples = std::llround(timing.samplesPerBar() * maxBars);
    snapshot->looping = true;
    snapshot->hasTrackMixes = true;

    if (loopEndStep > loopStartStep && loopStartStep >= 0)
    {
        snapshot->loopStartSample = timing.stepToSample(loopStartStep);
        snapshot->loopEndSample = std::min(timing.stepToSample(loopEndStep), snapshot->lengthSamples);
    }
    else
    {
        snapshot->loopStartSample = 0;
        snapshot->loopEndSample = snapshot->lengthSamples;
    }

    bool anySolo = false;
    for (const auto& trk : p->tracks)
    {
        if (trk.solo)
            anySolo = true;
    }

    const float master = song.masterVolume();
    const double stepDurationSamples = timing.samplesPerStep();
    const std::int64_t patternEndSample = timing.stepToSample(p->lengthSteps());

    for (std::size_t t = 0; t < p->tracks.size() && t < PlaybackSnapshot::kMaxTrackMixes; ++t)
    {
        const auto& trk = p->tracks[t];
        snapshot->trackMixes[t] = {trk.mix.volume, trk.mix.pan, trk.muted, trk.solo};

        if (anySolo ? (!trk.solo || trk.muted) : trk.muted)
            continue;

        const SampleKind trackKind = song.patternTrackKind(patternId, static_cast<int>(t));
        const SampleBank& sourceBank = (trackKind == SampleKind::OneShot && rawBank != nullptr) ? *rawBank : bank;

        for (const auto& clip : trk.clips)
        {
            auto found = sourceBank.find(clip.sampleId);
            if (found == sourceBank.end() || found->second == nullptr)
            {
                found = bank.find(clip.sampleId);
                if (found == bank.end() || found->second == nullptr)
                    continue;
            }

            const std::int64_t globalStep = clip.startStep;
            const std::int64_t eventStartSample = timing.stepToSample(globalStep);
            if (eventStartSample >= patternEndSample)
                continue;

            const std::int64_t framesUntilLoop = std::max<std::int64_t>(0, patternEndSample - eventStartSample);
            const std::int64_t audioOffset = static_cast<std::int64_t>(std::round(clip.offsetSteps * stepDurationSamples));
            std::int64_t maxFrames = (clip.lengthSteps > 0)
                ? static_cast<std::int64_t>(std::round(clip.lengthSteps * stepDurationSamples))
                : framesUntilLoop;
            maxFrames = std::min(maxFrames, framesUntilLoop);

            snapshot->events.push_back({eventStartSample, found->second.get(), master,
                                        master, audioOffset, maxFrames, static_cast<int>(t)});
            snapshot->keepAlive.push_back(found->second);
        }
    }

    std::stable_sort(snapshot->events.begin(), snapshot->events.end(),
                     [](const PlaybackEvent& a, const PlaybackEvent& b) { return a.startSample < b.startSample; });
    return snapshot;
}

std::unique_ptr<PlaybackSnapshot> SnapshotBuilder::buildArrangement(const Song& song, const SampleBank& bank,
                                                                    double sampleRate,
                                                                    int loopStartBar, int loopEndBar,
                                                                    const SampleBank* rawBank)
{
    auto snapshot = std::make_unique<PlaybackSnapshot>();
    const Timing timing(song.bpm(), sampleRate, song.swing());
    snapshot->samplesPerBeat = timing.samplesPerStep() * kStepsPerBeat;

    const int songBars = song.lengthBars();
    if (songBars == 0)
        return snapshot;

    snapshot->lengthSamples = std::llround(timing.samplesPerBar() * songBars);
    snapshot->looping = (loopEndBar > loopStartBar);
    snapshot->hasTrackMixes = true;

    if (loopEndBar > loopStartBar && loopStartBar >= 0)
    {
        snapshot->loopStartSample = timing.stepToSample(static_cast<std::int64_t>(loopStartBar) * kStepsPerBar);
        snapshot->loopEndSample = std::min(timing.stepToSample(static_cast<std::int64_t>(loopEndBar) * kStepsPerBar), snapshot->lengthSamples);
    }
    else
    {
        snapshot->loopStartSample = 0;
        snapshot->loopEndSample = snapshot->lengthSamples;
    }

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

            for (std::size_t ptIdx = 0; ptIdx < pattern->tracks.size(); ++ptIdx)
            {
                const auto& patTrack = pattern->tracks[ptIdx];
                if (anyPatSolo ? (!patTrack.solo || patTrack.muted) : patTrack.muted)
                    continue;

                const auto [patPanL, patPanR] = panGains(patTrack.mix.pan);
                const float innerGain = song.masterVolume() * patTrack.mix.volume;
                const float combinedGainL = innerGain * patPanL;
                const float combinedGainR = innerGain * patPanR;

                const SampleKind trackKind = song.patternTrackKind(block.patternId, static_cast<int>(ptIdx));
                const SampleBank& sourceBank = (trackKind == SampleKind::OneShot && rawBank != nullptr) ? *rawBank : bank;

                for (const auto& clip : patTrack.clips)
                {
                    auto found = sourceBank.find(clip.sampleId);
                    if (found == sourceBank.end() || found->second == nullptr)
                    {
                        found = bank.find(clip.sampleId);
                        if (found == bank.end() || found->second == nullptr)
                            continue;
                    }

                    const std::int64_t globalStep = blockBaseStep + clip.startStep;
                    const std::int64_t startSample = timing.stepToSample(globalStep);
                    const std::int64_t blockEndSample = timing.stepToSample(blockBaseStep + pattern->lengthSteps());
                    if (startSample >= blockEndSample)
                        continue;

                    const std::int64_t framesUntilEnd = std::max<std::int64_t>(0, blockEndSample - startSample);
                    const double stepDurationSamples = timing.samplesPerStep();
                    const std::int64_t audioOffset = static_cast<std::int64_t>(std::round(clip.offsetSteps * stepDurationSamples));
                    std::int64_t maxFrames = (clip.lengthSteps > 0)
                        ? static_cast<std::int64_t>(std::round(clip.lengthSteps * stepDurationSamples))
                        : framesUntilEnd;
                    maxFrames = std::min(maxFrames, framesUntilEnd);

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
