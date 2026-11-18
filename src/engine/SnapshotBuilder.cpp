#include "engine/SnapshotBuilder.h"
#include "engine/StretchCache.h"
#include "model/Timing.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace trami
{
namespace
{
void buildEventEnvelope(const std::vector<EnvelopePoint>& clipEnv,
                        double subStartNorm, double subEndNorm,
                        const std::vector<EnvelopePoint>& trackAuto,
                        double eventStartStep, double eventSpanSteps,
                        float defaultClipVal, float defaultTrackVal,
                        bool multiplicative,
                        std::array<PlaybackEnvelopeNode, kMaxPlaybackEnvNodes>& outArr,
                        int& outCount)
{
    outCount = 0;
    if (clipEnv.empty() && trackAuto.empty())
        return;

    constexpr int kNumSamples = 12;
    outCount = kNumSamples;
    const double normRange = std::max(1e-6, subEndNorm - subStartNorm);
    for (int i = 0; i < kNumSamples; ++i)
    {
        const double t = static_cast<double>(i) / static_cast<double>(kNumSamples - 1);
        const double clipPos = subStartNorm + t * normRange;
        const double trackStep = eventStartStep + t * eventSpanSteps;

        const float cVal = clipEnv.empty() ? defaultClipVal : evaluateEnvelope(clipEnv, clipPos, defaultClipVal);
        const float trVal = trackAuto.empty() ? defaultTrackVal : evaluateEnvelope(trackAuto, trackStep, defaultTrackVal);

        PlaybackEnvelopeNode node;
        node.positionNorm = static_cast<float>(t);
        node.value = multiplicative ? (cVal * trVal) : std::clamp(cVal + trVal, -1.0f, 1.0f);
        node.curve = 0;
        outArr[static_cast<std::size_t>(i)] = node;
    }
}
} // namespace

std::unique_ptr<PlaybackSnapshot> SnapshotBuilder::buildPattern(const Song& song, PatternId patternId,
                                                                const SampleBank& bank, double sampleRate,
                                                                int loopStartStep, int loopEndStep,
                                                                const SampleBank* rawBank,
                                                                const std::vector<DragClipOverride>* overrides,
                                                                StretchCache* stretchCache)
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

    const double stepDurationSamples = timing.samplesPerStep();
    const std::int64_t patternEndSample = timing.stepToSample(p->lengthSteps());

    for (std::size_t t = 0; t < p->tracks.size() && t < PlaybackSnapshot::kMaxTrackMixes; ++t)
    {
        const auto& trk = p->tracks[t];
        snapshot->trackMixes[t] = {trk.mix.volume, trk.mix.pan, trk.muted, trk.solo};

        if (anySolo ? (!trk.solo || trk.muted) : trk.muted)
            continue;

        const SampleKind trackKind = song.patternTrackKind(patternId, static_cast<int>(t));
        const bool isUnstretched = (trackKind == SampleKind::OneShot || trackKind == SampleKind::DiskStream);
        const SampleBank& sourceBank = (isUnstretched && rawBank != nullptr) ? *rawBank : bank;

        struct EffectiveClip
        {
            ClipId id;
            SampleId sampleId;
            double startStep = 0.0;
            double lengthSteps = 0.0;
            double offsetSteps = 0.0;
            float gain = 1.0f;
            float fadeInSteps = 0.0f;
            float fadeOutSteps = 0.0f;
            bool reversed = false;
            int transposeSemitones = 0;
            std::vector<EnvelopePoint> volumeEnvelope;
            std::vector<EnvelopePoint> panEnvelope;
        };

        std::vector<EffectiveClip> trackClips;

        if (overrides == nullptr || overrides->empty())
        {
            for (const auto& clip : trk.clips)
            {
                trackClips.push_back({clip.id, clip.sampleId, clip.startStep, clip.lengthSteps, clip.offsetSteps,
                                      clip.gain, clip.fadeInSteps, clip.fadeOutSteps, clip.reversed, clip.transposeSemitones,
                                      clip.volumeEnvelope, clip.panEnvelope});
            }
        }
        else
        {
            // 1. Clips that were originally on this track:
            for (const auto& clip : trk.clips)
            {
                const DragClipOverride* ov = nullptr;
                for (const auto& item : *overrides)
                {
                    if (item.clipId == clip.id)
                    {
                        ov = &item;
                        break;
                    }
                }

                if (ov == nullptr)
                {
                    trackClips.push_back({clip.id, clip.sampleId, clip.startStep, clip.lengthSteps, clip.offsetSteps,
                                          clip.gain, clip.fadeInSteps, clip.fadeOutSteps, clip.reversed, clip.transposeSemitones,
                                          clip.volumeEnvelope, clip.panEnvelope});
                }
                else if (ov->currentTrack == static_cast<int>(t))
                {
                    trackClips.push_back({clip.id, clip.sampleId, ov->currentStartStep, ov->currentLengthSteps, ov->currentOffsetSteps,
                                          ov->currentGain, ov->currentFadeIn, ov->currentFadeOut, clip.reversed, clip.transposeSemitones,
                                          clip.volumeEnvelope, clip.panEnvelope});
                }
                // If ov->currentTrack != t, the clip has been dragged away from track t; skip it here.
            }

            // 2. Clips dragged onto this track from other tracks:
            for (const auto& ov : *overrides)
            {
                if (ov.currentTrack == static_cast<int>(t) && ov.originalTrack != static_cast<int>(t))
                {
                    const Clip* origClip = nullptr;
                    for (const auto& otherTrack : p->tracks)
                    {
                        for (const auto& c : otherTrack.clips)
                        {
                            if (c.id == ov.clipId)
                            {
                                origClip = &c;
                                break;
                            }
                        }
                        if (origClip != nullptr)
                            break;
                    }

                    if (origClip != nullptr)
                    {
                        trackClips.push_back({ov.clipId, origClip->sampleId, ov.currentStartStep, ov.currentLengthSteps, ov.currentOffsetSteps,
                                              ov.currentGain, ov.currentFadeIn, ov.currentFadeOut, origClip->reversed, origClip->transposeSemitones,
                                              origClip->volumeEnvelope, origClip->panEnvelope});
                    }
                }
            }
        }

        auto getClipAudio = [&](SampleId sampleId, int semitones) -> std::shared_ptr<const AudioData> {
            const auto* s = song.findSample(sampleId);
            const int effSemitones = song.effectiveClipTranspose(s, semitones, trackKind);
            if (stretchCache != nullptr && s != nullptr)
            {
                if (trackKind == SampleKind::Loop || effSemitones != 0)
                {
                    if (auto a = stretchCache->getAudio(*s, song.bpm(), sampleRate, effSemitones, trackKind))
                        return a;
                }
            }
            auto it = sourceBank.find(sampleId);
            if (it != sourceBank.end() && it->second != nullptr)
                return it->second;
            it = bank.find(sampleId);
            if (it != bank.end() && it->second != nullptr)
                return it->second;
            return nullptr;
        };

        std::vector<TrackClipSpan> spans;
        for (const auto& clip : trackClips)
        {
            auto clipAudio = getClipAudio(clip.sampleId, clip.transposeSemitones);
            if (clipAudio == nullptr)
                continue;

            const double globalStep = clip.startStep;
            const std::int64_t eventStartSample = timing.stepToSample(globalStep);
            if (eventStartSample >= patternEndSample)
                continue;

            const std::int64_t audioOffset = static_cast<std::int64_t>(std::round(clip.offsetSteps * stepDurationSamples));
            const std::int64_t availFrames = std::max<std::int64_t>(0, clipAudio->numFrames() - audioOffset);

            double spanSteps = clip.lengthSteps;
            if (spanSteps <= 0.0)
            {
                if (trackKind == SampleKind::Loop)
                {
                    const auto* s = song.findSample(clip.sampleId);
                    spanSteps = (s != nullptr) ? s->loopCycleSteps() : static_cast<double>(kStepsPerBar);
                }
                else
                {
                    spanSteps = std::max(1.0, static_cast<double>(availFrames) / stepDurationSamples);
                }
            }

            spans.push_back({clip.id, clip.startStep, spanSteps, clip.fadeInSteps, clip.fadeOutSteps});
        }
        const auto crossfades = computeTrackCrossfades(spans);

        for (const auto& clip : trackClips)
        {
            auto clipAudio = getClipAudio(clip.sampleId, clip.transposeSemitones);
            if (clipAudio == nullptr)
                continue;

            const double globalStep = clip.startStep;
            const std::int64_t eventStartSample = timing.stepToSample(globalStep);
            if (eventStartSample >= patternEndSample)
                continue;

            const std::int64_t framesUntilPatternEnd = std::max<std::int64_t>(0, patternEndSample - eventStartSample);
            const std::int64_t audioOffset = static_cast<std::int64_t>(std::round(clip.offsetSteps * stepDurationSamples));

            const auto xfIt = crossfades.find(clip.id.value);
            const float effectiveFadeIn = (xfIt != crossfades.end()) ? xfIt->second.fadeInSteps : clip.fadeInSteps;
            const float effectiveFadeOut = (xfIt != crossfades.end()) ? xfIt->second.fadeOutSteps : clip.fadeOutSteps;

            const float clipGainL = clip.gain;
            const float clipGainR = clip.gain;

            if (trackKind == SampleKind::Loop)
            {
                const Sample* sample = song.findSample(clip.sampleId);
                const double loopCycleSteps = (sample != nullptr && sample->loopCycleSteps() > 0.0)
                    ? sample->loopCycleSteps()
                    : static_cast<double>(kStepsPerBar);
                const double clipSpanSteps = (clip.lengthSteps > 0.0) ? clip.lengthSteps : loopCycleSteps;
                const double initialPhaseSteps = (clip.offsetSteps > 0.0) ? std::fmod(clip.offsetSteps, loopCycleSteps) : 0.0;

                double offsetStep = 0.0;
                bool firstCycle = true;
                while (offsetStep < clipSpanSteps - 1e-6)
                {
                    const double cycleStartStep = globalStep + offsetStep;
                    const std::int64_t cycleEventStart = timing.stepToSample(cycleStartStep);
                    if (cycleEventStart >= patternEndSample)
                        break;

                    const double maxCycleSteps = firstCycle ? (loopCycleSteps - initialPhaseSteps) : loopCycleSteps;
                    const double thisCycleSteps = std::min(maxCycleSteps, clipSpanSteps - offsetStep);
                    if (thisCycleSteps <= 1e-6)
                        break;

                    const std::int64_t cycleFrames = static_cast<std::int64_t>(std::round(thisCycleSteps * stepDurationSamples));
                    std::int64_t maxFrames = std::min(patternEndSample - cycleEventStart, cycleFrames);

                    if (snapshot->looping && snapshot->loopEndSample > snapshot->loopStartSample)
                    {
                        if (cycleEventStart >= snapshot->loopStartSample && cycleEventStart < snapshot->loopEndSample)
                        {
                            const std::int64_t framesUntilLoop = std::max<std::int64_t>(0, snapshot->loopEndSample - cycleEventStart);
                            maxFrames = std::min(maxFrames, framesUntilLoop);
                        }
                    }

                    const std::int64_t curAudioOffset = firstCycle
                        ? static_cast<std::int64_t>(std::round(initialPhaseSteps * stepDurationSamples))
                        : 0;
                    const std::int64_t curFadeIn = firstCycle
                        ? std::min(maxFrames, static_cast<std::int64_t>(std::round(effectiveFadeIn * stepDurationSamples)))
                        : 0;
                    const bool isLastCycle = (offsetStep + thisCycleSteps >= clipSpanSteps - 1e-6);
                    const std::int64_t curFadeOut = isLastCycle
                        ? std::min(maxFrames, static_cast<std::int64_t>(std::round(effectiveFadeOut * stepDurationSamples)))
                        : 0;

                    PlaybackEvent ev{cycleEventStart, clipAudio.get(), clipGainL,
                                     clipGainR, curAudioOffset, maxFrames, static_cast<int>(t),
                                     curFadeIn, curFadeOut, clip.id.value, clip.reversed};
                    ev.fxSend = trk.fxAutomation.empty()
                        ? trk.fxSend
                        : evaluateEnvelope(trk.fxAutomation, cycleStartStep + 0.5 * thisCycleSteps, trk.fxSend);
                    if (clipSpanSteps > 1e-6)
                    {
                        const double sub0 = offsetStep / clipSpanSteps;
                        const double sub1 = (offsetStep + thisCycleSteps) / clipSpanSteps;
                        buildEventEnvelope(clip.volumeEnvelope, sub0, sub1, trk.volumeAutomation,
                                           cycleStartStep, thisCycleSteps, 1.0f, 1.0f, true, ev.volEnv, ev.volEnvCount);
                        buildEventEnvelope(clip.panEnvelope, sub0, sub1, trk.panAutomation,
                                           cycleStartStep, thisCycleSteps, 0.0f, 0.0f, false, ev.panEnv, ev.panEnvCount);
                    }
                    snapshot->events.push_back(ev);
                    snapshot->keepAlive.push_back(clipAudio);

                    offsetStep += thisCycleSteps;
                    firstCycle = false;
                }
            }
            else
            {
                std::int64_t maxFrames = (clip.lengthSteps > 0.0)
                    ? static_cast<std::int64_t>(std::round(clip.lengthSteps * stepDurationSamples))
                    : framesUntilPatternEnd;
                maxFrames = std::min(maxFrames, framesUntilPatternEnd);

                if (snapshot->looping && snapshot->loopEndSample > snapshot->loopStartSample)
                {
                    if (eventStartSample >= snapshot->loopStartSample && eventStartSample < snapshot->loopEndSample)
                    {
                        const std::int64_t framesUntilLoop = std::max<std::int64_t>(0, snapshot->loopEndSample - eventStartSample);
                        maxFrames = std::min(maxFrames, framesUntilLoop);
                    }
                }

                const std::int64_t fadeInSamples = std::min(maxFrames, static_cast<std::int64_t>(std::round(effectiveFadeIn * stepDurationSamples)));
                const std::int64_t fadeOutSamples = std::min(maxFrames, static_cast<std::int64_t>(std::round(effectiveFadeOut * stepDurationSamples)));

                PlaybackEvent ev{eventStartSample, clipAudio.get(), clipGainL,
                                 clipGainR, audioOffset, maxFrames, static_cast<int>(t),
                                 fadeInSamples, fadeOutSamples, clip.id.value, clip.reversed};
                const double evSpanSteps = std::max(0.1, static_cast<double>(maxFrames) / std::max(1.0, stepDurationSamples));
                ev.fxSend = trk.fxAutomation.empty()
                    ? trk.fxSend
                    : evaluateEnvelope(trk.fxAutomation, globalStep + 0.5 * evSpanSteps, trk.fxSend);
                buildEventEnvelope(clip.volumeEnvelope, 0.0, 1.0, trk.volumeAutomation,
                                   globalStep, evSpanSteps, 1.0f, 1.0f, true, ev.volEnv, ev.volEnvCount);
                buildEventEnvelope(clip.panEnvelope, 0.0, 1.0, trk.panAutomation,
                                   globalStep, evSpanSteps, 0.0f, 0.0f, false, ev.panEnv, ev.panEnvCount);
                snapshot->events.push_back(ev);
                snapshot->keepAlive.push_back(clipAudio);
            }
        }
    }

    std::stable_sort(snapshot->events.begin(), snapshot->events.end(),
                     [](const PlaybackEvent& a, const PlaybackEvent& b) { return a.startSample < b.startSample; });
    return snapshot;
}

std::unique_ptr<PlaybackSnapshot> SnapshotBuilder::buildArrangement(const Song& song, const SampleBank& bank,
                                                                    double sampleRate,
                                                                    int loopStartBar, int loopEndBar,
                                                                    const SampleBank* rawBank,
                                                                    StretchCache* stretchCache)
{
    auto snapshot = std::make_unique<PlaybackSnapshot>();
    const Timing timing(song.bpm(), sampleRate, song.swing());
    snapshot->samplesPerBeat = timing.samplesPerStep() * kStepsPerBeat;

    const int songBars = song.lengthBars();
    if (songBars == 0)
        return snapshot;

    const int effectiveBars = std::max(songBars, (loopEndBar > loopStartBar ? loopEndBar : 0));
    snapshot->lengthSamples = std::llround(timing.samplesPerBar() * effectiveBars);
    snapshot->looping = (loopEndBar > loopStartBar);
    snapshot->hasTrackMixes = true;

    if (loopEndBar > loopStartBar && loopStartBar >= 0)
    {
        snapshot->loopStartSample = timing.stepToSample(static_cast<double>(loopStartBar * kStepsPerBar));
        snapshot->loopEndSample = timing.stepToSample(static_cast<double>(loopEndBar * kStepsPerBar));
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
                const float innerGain = patTrack.mix.volume;
                const float combinedGainL = innerGain * patPanL;
                const float combinedGainR = innerGain * patPanR;

                const SampleKind trackKind = song.patternTrackKind(block.patternId, static_cast<int>(ptIdx));
                const bool isUnstretched = (trackKind == SampleKind::OneShot || trackKind == SampleKind::DiskStream);
                const SampleBank& sourceBank = (isUnstretched && rawBank != nullptr) ? *rawBank : bank;

                const double stepDurationSamples = timing.samplesPerStep();

                auto getClipAudio = [&](SampleId sampleId, int semitones) -> std::shared_ptr<const AudioData> {
                    const auto* s = song.findSample(sampleId);
                    const int effSemitones = song.effectiveClipTranspose(s, semitones, trackKind);
                    if (stretchCache != nullptr && s != nullptr)
                    {
                        if (trackKind == SampleKind::Loop || effSemitones != 0)
                        {
                            if (auto a = stretchCache->getAudio(*s, song.bpm(), sampleRate, effSemitones, trackKind))
                                return a;
                        }
                    }
                    auto it = sourceBank.find(sampleId);
                    if (it != sourceBank.end() && it->second != nullptr)
                        return it->second;
                    it = bank.find(sampleId);
                    if (it != bank.end() && it->second != nullptr)
                        return it->second;
                    return nullptr;
                };

                std::vector<TrackClipSpan> spans;
                for (const auto& clip : patTrack.clips)
                {
                    auto clipAudio = getClipAudio(clip.sampleId, clip.transposeSemitones);
                    if (clipAudio == nullptr)
                        continue;

                    const double globalStep = static_cast<double>(blockBaseStep) + clip.startStep;
                    const std::int64_t startSample = timing.stepToSample(globalStep);
                    const std::int64_t blockEndSample = timing.stepToSample(static_cast<double>(blockBaseStep + pattern->lengthSteps()));
                    if (startSample >= blockEndSample)
                        continue;

                    const std::int64_t audioOffset = static_cast<std::int64_t>(std::round(clip.offsetSteps * stepDurationSamples));
                    const std::int64_t availFrames = std::max<std::int64_t>(0, clipAudio->numFrames() - audioOffset);

                    double spanSteps = clip.lengthSteps;
                    if (spanSteps <= 0.0)
                    {
                        if (trackKind == SampleKind::Loop)
                        {
                            const auto* s = song.findSample(clip.sampleId);
                            spanSteps = (s != nullptr) ? s->loopCycleSteps() : static_cast<double>(kStepsPerBar);
                        }
                        else
                        {
                            spanSteps = std::max(1.0, static_cast<double>(availFrames) / stepDurationSamples);
                        }
                    }

                    spans.push_back({clip.id, clip.startStep, spanSteps, clip.fadeInSteps, clip.fadeOutSteps});
                }
                const auto crossfades = computeTrackCrossfades(spans);

                for (const auto& clip : patTrack.clips)
                {
                    auto clipAudio = getClipAudio(clip.sampleId, clip.transposeSemitones);
                    if (clipAudio == nullptr)
                        continue;

                    const double globalStep = static_cast<double>(blockBaseStep) + clip.startStep;
                    const std::int64_t startSample = timing.stepToSample(globalStep);
                    const std::int64_t blockEndSample = timing.stepToSample(static_cast<double>(blockBaseStep + pattern->lengthSteps()));
                    if (startSample >= blockEndSample)
                        continue;

                    const std::int64_t framesUntilEnd = std::max<std::int64_t>(0, blockEndSample - startSample);
                    const std::int64_t audioOffset = static_cast<std::int64_t>(std::round(clip.offsetSteps * stepDurationSamples));

                    const auto xfIt = crossfades.find(clip.id.value);
                    const float effectiveFadeIn = (xfIt != crossfades.end()) ? xfIt->second.fadeInSteps : clip.fadeInSteps;
                    const float effectiveFadeOut = (xfIt != crossfades.end()) ? xfIt->second.fadeOutSteps : clip.fadeOutSteps;

                    const float clipGainL = combinedGainL * clip.gain;
                    const float clipGainR = combinedGainR * clip.gain;

                    if (trackKind == SampleKind::Loop)
                    {
                        const Sample* sample = song.findSample(clip.sampleId);
                        const double loopCycleSteps = (sample != nullptr && sample->loopCycleSteps() > 0.0)
                            ? sample->loopCycleSteps()
                            : static_cast<double>(kStepsPerBar);
                        const double clipSpanSteps = (clip.lengthSteps > 0.0) ? clip.lengthSteps : loopCycleSteps;
                        const double initialPhaseSteps = (clip.offsetSteps > 0.0) ? std::fmod(clip.offsetSteps, loopCycleSteps) : 0.0;

                        double offsetStep = 0.0;
                        bool firstCycle = true;
                        while (offsetStep < clipSpanSteps - 1e-6)
                        {
                            const double cycleStartStep = globalStep + offsetStep;
                            const std::int64_t cycleEventStart = timing.stepToSample(cycleStartStep);
                            if (cycleEventStart >= blockEndSample)
                                break;

                            const double maxCycleSteps = firstCycle ? (loopCycleSteps - initialPhaseSteps) : loopCycleSteps;
                            const double thisCycleSteps = std::min(maxCycleSteps, clipSpanSteps - offsetStep);
                            if (thisCycleSteps <= 1e-6)
                                break;

                            const std::int64_t cycleFrames = static_cast<std::int64_t>(std::round(thisCycleSteps * stepDurationSamples));
                            std::int64_t maxFrames = std::min(blockEndSample - cycleEventStart, cycleFrames);

                            if (snapshot->looping && snapshot->loopEndSample > snapshot->loopStartSample)
                            {
                                if (cycleEventStart >= snapshot->loopStartSample && cycleEventStart < snapshot->loopEndSample)
                                {
                                    const std::int64_t framesUntilLoop = std::max<std::int64_t>(0, snapshot->loopEndSample - cycleEventStart);
                                    maxFrames = std::min(maxFrames, framesUntilLoop);
                                }
                            }

                            const std::int64_t curAudioOffset = firstCycle
                                ? static_cast<std::int64_t>(std::round(initialPhaseSteps * stepDurationSamples))
                                : 0;
                            const std::int64_t curFadeIn = firstCycle
                                ? std::min(maxFrames, static_cast<std::int64_t>(std::round(effectiveFadeIn * stepDurationSamples)))
                                : 0;
                            const bool isLastCycle = (offsetStep + thisCycleSteps >= clipSpanSteps - 1e-6);
                            const std::int64_t curFadeOut = isLastCycle
                                ? std::min(maxFrames, static_cast<std::int64_t>(std::round(effectiveFadeOut * stepDurationSamples)))
                                : 0;

                            PlaybackEvent ev{cycleEventStart, clipAudio.get(), clipGainL, clipGainR,
                                             curAudioOffset, maxFrames, static_cast<int>(at), curFadeIn, curFadeOut, clip.id.value, clip.reversed};
                            ev.fxSend = patTrack.fxSend;
                            if (clipSpanSteps > 1e-6)
                            {
                                const double sub0 = offsetStep / clipSpanSteps;
                                const double sub1 = (offsetStep + thisCycleSteps) / clipSpanSteps;
                                buildEventEnvelope(clip.volumeEnvelope, sub0, sub1, patTrack.volumeAutomation,
                                                   clip.startStep + offsetStep, thisCycleSteps, 1.0f, 1.0f, true, ev.volEnv, ev.volEnvCount);
                                buildEventEnvelope(clip.panEnvelope, sub0, sub1, patTrack.panAutomation,
                                                   clip.startStep + offsetStep, thisCycleSteps, 0.0f, 0.0f, false, ev.panEnv, ev.panEnvCount);
                            }
                            snapshot->events.push_back(ev);
                            snapshot->keepAlive.push_back(clipAudio);

                            offsetStep += thisCycleSteps;
                            firstCycle = false;
                        }
                    }
                    else
                    {
                        std::int64_t maxFrames = (clip.lengthSteps > 0.0)
                            ? static_cast<std::int64_t>(std::round(clip.lengthSteps * stepDurationSamples))
                            : framesUntilEnd;
                        maxFrames = std::min(maxFrames, framesUntilEnd);

                        if (snapshot->looping && snapshot->loopEndSample > snapshot->loopStartSample)
                        {
                            if (startSample >= snapshot->loopStartSample && startSample < snapshot->loopEndSample)
                            {
                                const std::int64_t framesUntilLoop = std::max<std::int64_t>(0, snapshot->loopEndSample - startSample);
                                maxFrames = std::min(maxFrames, framesUntilLoop);
                            }
                        }

                        const std::int64_t fadeInSamples = std::min(maxFrames, static_cast<std::int64_t>(std::round(effectiveFadeIn * stepDurationSamples)));
                        const std::int64_t fadeOutSamples = std::min(maxFrames, static_cast<std::int64_t>(std::round(effectiveFadeOut * stepDurationSamples)));

                        PlaybackEvent ev{startSample, clipAudio.get(), clipGainL, clipGainR,
                                         audioOffset, maxFrames, static_cast<int>(at), fadeInSamples, fadeOutSamples, clip.id.value, clip.reversed};
                        const double evSpanSteps = std::max(0.1, static_cast<double>(maxFrames) / std::max(1.0, stepDurationSamples));
                        ev.fxSend = patTrack.fxSend;
                        buildEventEnvelope(clip.volumeEnvelope, 0.0, 1.0, patTrack.volumeAutomation,
                                           clip.startStep, evSpanSteps, 1.0f, 1.0f, true, ev.volEnv, ev.volEnvCount);
                        buildEventEnvelope(clip.panEnvelope, 0.0, 1.0, patTrack.panAutomation,
                                           clip.startStep, evSpanSteps, 0.0f, 0.0f, false, ev.panEnv, ev.panEnvCount);
                        snapshot->events.push_back(ev);
                        snapshot->keepAlive.push_back(clipAudio);
                    }
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
