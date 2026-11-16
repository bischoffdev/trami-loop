#include "ui/PatternGrid.h"

#include "model/Timing.h"
#include "ui/DAWTrackControls.h"
#include "ui/TramiLookAndFeel.h"

#include <algorithm>
#include <cmath>
#include <unordered_set>
#include <vector>

namespace trami
{

PatternGrid::PatternGrid(const Song& song, StretchCache& stretchCache, PatternId patternId, Listener& listener)
    : song_(song), stretchCache_(stretchCache), patternId_(patternId), listener_(listener)
{
    setOpaque(true);
    setWantsKeyboardFocus(true);
}

void PatternGrid::setPattern(PatternId patternId)
{
    patternId_ = patternId;
    selectedClips_.clear();
    cursorTrack_ = -1;
    cursorStep_ = -1;
    const auto* pat = song_.findPattern(patternId_);
    const int total = pat != nullptr ? pat->lengthSteps() : 16;
    loopStartStep_ = 0;
    loopEndStep_ = total;
    repaint();
}

void PatternGrid::setLoopRegion(int startStep, int endStep)
{
    loopStartStep_ = std::max(0, startStep);
    loopEndStep_ = std::max(loopStartStep_ + 1, endStep);
    repaint();
}

void PatternGrid::setSelectedClip(ClipId clipId)
{
    selectedClips_.clear();
    if (clipId.isValid())
    {
        selectedClips_.push_back(clipId);
        cursorTrack_ = -1;
        cursorStep_ = -1;
    }
    repaint();
}

void PatternGrid::setSelectedClips(const std::vector<ClipId>& clipIds)
{
    selectedClips_ = clipIds;
    if (!selectedClips_.empty())
    {
        cursorTrack_ = -1;
        cursorStep_ = -1;
    }
    repaint();
}

bool PatternGrid::isClipSelected(ClipId clipId) const noexcept
{
    if (!clipId.isValid())
        return false;
    return std::find(selectedClips_.begin(), selectedClips_.end(), clipId) != selectedClips_.end();
}

void PatternGrid::deselectClip(ClipId clipId)
{
    selectedClips_.erase(std::remove(selectedClips_.begin(), selectedClips_.end(), clipId), selectedClips_.end());
    repaint();
}

void PatternGrid::clearSelection()
{
    selectedClips_.clear();
    repaint();
}

void PatternGrid::startMarquee(juce::Point<int> pos, juce::ModifierKeys mods)
{
    grabKeyboardFocus();
    const bool isShiftOrCmd = mods.isShiftDown() || mods.isCommandDown();
    if (!isShiftOrCmd)
    {
        selectedClips_.clear();
        cursorTrack_ = -1;
        cursorStep_ = -1;
    }

    isMarquee_ = true;
    marqueeStart_ = pos;
    marqueeEnd_ = pos;
    preMarqueeSelectedClips_ = selectedClips_;
    repaint();
}

void PatternGrid::updateMarquee(juce::Point<int> pos)
{
    if (!isMarquee_)
        return;

    marqueeEnd_ = pos;
    const auto mr = juce::Rectangle<int>::leftTopRightBottom(
        std::min(marqueeStart_.x, marqueeEnd_.x),
        std::min(marqueeStart_.y, marqueeEnd_.y),
        std::max(marqueeStart_.x, marqueeEnd_.x),
        std::max(marqueeStart_.y, marqueeEnd_.y));

    const auto* pat = song_.findPattern(patternId_);
    std::vector<ClipId> newSelection = preMarqueeSelectedClips_;
    if (pat != nullptr)
    {
        for (int t = 0; t < static_cast<int>(pat->tracks.size()); ++t)
        {
            const auto& tr = pat->tracks[static_cast<std::size_t>(t)];
            for (const auto& c : tr.clips)
            {
                const int span = clipSpan(c, t);
                const auto b = clipBounds(t, c.startStep, span).reduced(2, 3);
                if (mr.intersects(b))
                {
                    if (std::find(newSelection.begin(), newSelection.end(), c.id) == newSelection.end())
                        newSelection.push_back(c.id);
                }
            }
        }
    }
    selectedClips_ = newSelection;
    repaint();
}

void PatternGrid::finishMarquee()
{
    if (isMarquee_)
    {
        isMarquee_ = false;
        repaint();
    }
}

void PatternGrid::setCursorPosition(int track, int step)
{
    cursorTrack_ = track;
    cursorStep_ = step;
    repaint();
}

void PatternGrid::clearCursor()
{
    cursorTrack_ = -1;
    cursorStep_ = -1;
    repaint();
}

void PatternGrid::setPlayheadStep(double step)
{
    playheadStep_ = step;
    repaint();
}

void PatternGrid::setRasterSteps(int steps)
{
    rasterSteps_ = std::max(1, steps);
    repaint();
}

void PatternGrid::setZoom(float zoomX, float zoomY)
{
    zoomX_ = zoomX;
    zoomY_ = zoomY;
    stepWidth_ = std::max(6, juce::roundToInt(static_cast<float>(kDefaultStepWidth) * zoomX_));
    trackHeight_ = std::max(20, juce::roundToInt(static_cast<float>(kDefaultTrackHeight) * zoomY_));
    repaint();
}

void PatternGrid::resized()
{
}

juce::Colour PatternGrid::sampleColour(SampleId id) const
{
    const Sample* sample = song_.findSample(id);
    const float hue = sample != nullptr ? sample->hue() : 0.5f;
    return juce::Colour::fromHSV(hue, 0.75f, 0.85f, 1.0f);
}

double PatternGrid::clipSpan(const Clip& clip, int trackIndex) const
{
    const Sample* sample = song_.findSample(clip.sampleId);
    if (sample == nullptr)
        return static_cast<double>(rasterSteps_);

    SampleKind kind = sample->kind;
    if (trackIndex >= 0)
    {
        kind = song_.patternTrackKind(patternId_, trackIndex);
    }
    else if (const auto* pat = song_.findPattern(patternId_))
    {
        for (std::size_t t = 0; t < pat->tracks.size(); ++t)
        {
            for (const auto& c : pat->tracks[t].clips)
            {
                if (c.id == clip.id)
                {
                    kind = song_.patternTrackKind(patternId_, static_cast<int>(t));
                    break;
                }
            }
        }
    }

    if (clip.lengthSteps > 0.0)
        return clip.lengthSteps;

    if (kind == SampleKind::Loop)
    {
        int bars = sample->loopBars;
        if (bars <= 1 && sample->durationSeconds > 0.0)
        {
            const double secPerBar = (song_.bpm() > 0.0) ? (60.0 / song_.bpm()) * 4.0 : 2.0;
            const int estBars = static_cast<int>(std::round(sample->durationSeconds / secPerBar));
            if (estBars >= 1)
                bars = estBars;
        }
        bars = std::max(1, bars);
        return static_cast<double>(bars * kStepsPerBar);
    }

    const double stepSec = 60.0 / song_.bpm() / kStepsPerBeat;
    const double naturalSteps = sample->durationSeconds / stepSec;
    return std::max(static_cast<double>(rasterSteps_), naturalSteps);
}

int PatternGrid::stepToX(double step) const
{
    const Timing timing(song_.bpm(), 48000.0, song_.swing());
    const double swungStep = timing.stepToSwungStep(step);
    return juce::roundToInt(swungStep * static_cast<double>(stepWidth_));
}

double PatternGrid::xToStep(int x) const
{
    const double rawSwungStep = static_cast<double>(x) / static_cast<double>(stepWidth_);
    const Timing timing(song_.bpm(), 48000.0, song_.swing());
    return timing.swungStepToStep(rawSwungStep);
}

juce::Rectangle<int> PatternGrid::clipBounds(int track, double step, double lengthSteps) const
{
    const int x = stepToX(step);
    const int right = stepToX(step + lengthSteps);
    return {x, kRulerHeight + track * trackHeight_, std::max(4, right - x), trackHeight_};
}

std::pair<int, int> PatternGrid::pointToTrackAndStep(juce::Point<int> pt) const
{
    const int track = (pt.y - kRulerHeight) / trackHeight_;
    const double rawStep = xToStep(pt.x);
    const int snappedStep = (juce::roundToInt(rawStep) / rasterSteps_) * rasterSteps_;
    return {track, snappedStep};
}

const Clip* PatternGrid::findClipAt(juce::Point<int> pt) const
{
    if (pt.y < kRulerHeight)
        return nullptr;

    const auto [trackIdx, stepIdx] = pointToTrackAndStep(pt);
    const auto* pat = song_.findPattern(patternId_);
    if (!pat || trackIdx < 0 || trackIdx >= static_cast<int>(pat->tracks.size()))
        return nullptr;

    const int patSteps = pat->lengthSteps();
    const auto& track = pat->tracks[static_cast<std::size_t>(trackIdx)];

    std::vector<const Clip*> hits;
    for (auto it = track.clips.rbegin(); it != track.clips.rend(); ++it)
    {
        if (it->startStep >= patSteps)
            continue;
        const double span = std::min(clipSpan(*it, trackIdx), static_cast<double>(patSteps) - it->startStep);
        auto b = clipBounds(trackIdx, it->startStep, span).reduced(2, 3);
        if (b.contains(pt))
            hits.push_back(&(*it));
    }

    if (hits.empty())
        return nullptr;
    if (hits.size() == 1)
        return hits.front();

    // If more than 1 clip is at this point and one is already selected, cycle to the next one underneath
    for (std::size_t i = 0; i < hits.size(); ++i)
    {
        if (isClipSelected(hits[i]->id))
            return hits[(i + 1) % hits.size()];
    }

    return hits.front();
}

PatternGrid::DragMode PatternGrid::hitTestResize(juce::Point<int> pt) const
{
    if (pt.y < kRulerHeight)
        return DragMode::None;

    const auto [trackIdx, stepIdx] = pointToTrackAndStep(pt);
    const auto* pat = song_.findPattern(patternId_);
    if (!pat || trackIdx < 0 || trackIdx >= static_cast<int>(pat->tracks.size()))
        return DragMode::None;

    const int patSteps = pat->lengthSteps();
    const auto& track = pat->tracks[static_cast<std::size_t>(trackIdx)];
    for (auto it = track.clips.rbegin(); it != track.clips.rend(); ++it)
    {
        if (it->startStep >= patSteps)
            continue;
        const double span = std::min(clipSpan(*it, trackIdx), static_cast<double>(patSteps) - it->startStep);
        auto b = clipBounds(trackIdx, it->startStep, span).reduced(2, 3);
        if (!b.contains(pt))
            continue;

        // 1. Left / Right edge resize (trim & horizontal loop rollout)
        const int edgeMargin = std::clamp(b.getWidth() / 4, 4, 10);
        if (pt.x <= b.getX() + edgeMargin)
            return DragMode::ResizeLeft;
        if (pt.x >= b.getRight() - edgeMargin)
            return DragMode::ResizeRight;

        // 2. Top handles: Fade In / Fade Out
        if (pt.y <= b.getY() + 14)
        {
            const int fadeInPx = juce::roundToInt(it->fadeInSteps * static_cast<float>(stepWidth_));
            if (std::abs(pt.x - (b.getX() + fadeInPx)) <= 8 || (fadeInPx == 0 && pt.x <= b.getX() + 12))
                return DragMode::FadeIn;

            const int fadeOutPx = juce::roundToInt(it->fadeOutSteps * static_cast<float>(stepWidth_));
            if (std::abs(pt.x - (b.getRight() - fadeOutPx)) <= 8 || (fadeOutPx == 0 && pt.x >= b.getRight() - 12))
                return DragMode::FadeOut;
        }

        // 3. Volume Gain line and center handle
        const float gainNorm = std::clamp(it->gain, 0.0f, 2.0f) * 0.5f;
        const float gainY = static_cast<float>(b.getY()) + static_cast<float>(b.getHeight()) * (1.0f - gainNorm);
        if (std::abs(static_cast<float>(pt.y) - gainY) <= 6.0f && pt.x > b.getX() + 8 && pt.x < b.getRight() - 8)
            return DragMode::Gain;

        return DragMode::Move;
    }

    return DragMode::None;
}

void PatternGrid::mouseMove(const juce::MouseEvent& e)
{
    if (drag_.active)
        return;

    const float playheadPx = static_cast<float>(stepToX(playheadStep_));
    const bool isNearPlayhead = (playheadStep_ >= 0.0 && std::abs(static_cast<float>(e.getPosition().x) - playheadPx) <= 6.0f);

    if (e.getPosition().y < kRulerHeight)
    {
        currentTooltip_.clear();
        const int startX = stepToX(loopStartStep_);
        const int endX = stepToX(loopEndStep_);
        const int mx = e.getPosition().x;
        const int my = e.getPosition().y;

        // Generous 12px hit margin in top 18px of ruler
        if (my <= 18 && (std::abs(mx - startX) <= 12 || std::abs(mx - endX) <= 12))
            setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
        else if (my <= 15 && mx > startX + 12 && mx < endX - 12)
            setMouseCursor(juce::MouseCursor::DraggingHandCursor);
        else if (my > 15)
            setMouseCursor(juce::MouseCursor::UpDownLeftRightResizeCursor);
        else if (isNearPlayhead)
            setMouseCursor(juce::MouseCursor::PointingHandCursor);
        else
            setMouseCursor(juce::MouseCursor::NormalCursor);
        return;
    }

    if (isNearPlayhead)
    {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
        return;
    }

    const auto [trackIdx, stepIdx] = pointToTrackAndStep(e.getPosition());
    const auto* pat = song_.findPattern(patternId_);
    if (pat != nullptr && trackIdx >= 0 && trackIdx < static_cast<int>(pat->tracks.size()))
    {
        const auto& track = pat->tracks[static_cast<std::size_t>(trackIdx)];
        const double stepFloat = xToStep(e.getPosition().x);
        int overlapCount = 0;
        for (const auto& c : track.clips)
        {
            const double span = clipSpan(c, trackIdx);
            if (stepFloat >= c.startStep && stepFloat < c.startStep + span)
                overlapCount++;
        }

        if (overlapCount > 1)
            currentTooltip_ = juce::String::formatted("%d clips stacked / overlapping (auto-crossfade active). Click to cycle selection.", overlapCount);
        else
            currentTooltip_.clear();
    }
    else
    {
        currentTooltip_.clear();
    }

    const auto mode = hitTestResize(e.getPosition());
    if (mode == DragMode::ResizeLeft || mode == DragMode::ResizeRight)
        setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
    else if (mode == DragMode::FadeIn || mode == DragMode::FadeOut)
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    else if (mode == DragMode::Gain)
        setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
    else if (mode == DragMode::Move)
        setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    else
        setMouseCursor(juce::MouseCursor::NormalCursor);
}

void PatternGrid::mouseExit(const juce::MouseEvent&)
{
    if (!drag_.active)
        setMouseCursor(juce::MouseCursor::NormalCursor);
}

void PatternGrid::paint(juce::Graphics& g)
{
    g.fillAll(Palette::background);

    const auto* pat = song_.findPattern(patternId_);
    const int numTracks = pat != nullptr ? static_cast<int>(pat->tracks.size()) : 0;
    const int totalSteps = pat != nullptr ? pat->lengthSteps() : kStepsPerBar;

    const int gridWidth = stepToX(totalSteps);
    const int renderWidth = std::max(getWidth(), gridWidth);
    const int totalHeight = kRulerHeight + std::max(1, numTracks) * trackHeight_;

    // 1. Ruler
    g.setColour(juce::Colour(0xff23272d));
    g.fillRect(0, 0, renderWidth, kRulerHeight);
    g.setColour(juce::Colour(0xff181a1f));
    g.drawHorizontalLine(kRulerHeight - 1, 0.0f, static_cast<float>(renderWidth));
    g.setFont(Typography::bold(11.0f));

    for (int step = 0; step < totalSteps; ++step)
    {
        const int x = stepToX(step);
        const bool isBar = (step % kStepsPerBar == 0);
        const bool isBeat = (step % kStepsPerBeat == 0);
        const bool isRaster = (step % rasterSteps_ == 0);

        if (isBar)
        {
            const int barNum = step / kStepsPerBar + 1;
            g.setColour(Palette::text);
            const int labelW = std::max(stepWidth_ * 2, stepWidth_ * kStepsPerBar);
            g.drawText(juce::String(barNum) + ".1", x + 4, 7, labelW, kRulerHeight - 8,
                       juce::Justification::centredLeft);
            g.setColour(Palette::gridBarLine);
            g.drawVerticalLine(x, static_cast<float>(kRulerHeight - 5), static_cast<float>(kRulerHeight));
        }
        else if (isBeat)
        {
            g.setColour(Palette::gridBeatLine);
            g.drawVerticalLine(x, static_cast<float>(kRulerHeight - 4), static_cast<float>(kRulerHeight));
        }
        else if (isRaster && stepWidth_ >= 6)
        {
            g.setColour(Palette::gridLine);
            g.drawVerticalLine(x, static_cast<float>(kRulerHeight - 3), static_cast<float>(kRulerHeight));
        }
    }

    // 2. Loop region highlight (down tracks and Studio One banner in ruler)
    const int loopStartX = stepToX(loopStartStep_);
    const int loopEndX = stepToX(loopEndStep_);
    const int loopW = std::max(4, loopEndX - loopStartX);

    // Shaded loop region down the tracks
    g.setColour(Palette::loopRegion.withAlpha(0.045f));
    g.fillRect(loopStartX, kRulerHeight, loopW, totalHeight - kRulerHeight);
    g.setColour(Palette::loopRegion.withAlpha(0.35f));
    g.drawVerticalLine(loopStartX, static_cast<float>(kRulerHeight), static_cast<float>(totalHeight));
    g.drawVerticalLine(loopEndX, static_cast<float>(kRulerHeight), static_cast<float>(totalHeight));

    // Studio One cycle indicator at the top of the ruler
    DAWLoopRuler::drawStudioOneLoopIndicator(g, loopStartX, loopEndX, kRulerHeight);

    // 3. Tracks & Grid lines
    for (int t = 0; t < numTracks; ++t)
    {
        const int y = kRulerHeight + t * trackHeight_;
        g.setColour(t % 2 == 0 ? Palette::gridFloor : Palette::gridLaneAlt);
        g.fillRect(0, y, renderWidth, trackHeight_);

        g.setColour(Palette::gridLine.withAlpha(0.6f));
        g.drawHorizontalLine(y + trackHeight_, 0.0f, static_cast<float>(renderWidth));
    }

    for (int step = 0; step <= totalSteps; ++step)
    {
        const int x = stepToX(step);
        const bool isBar = (step % kStepsPerBar == 0);
        const bool isBeat = (step % kStepsPerBeat == 0);
        const bool isRaster = (step % rasterSteps_ == 0);

        if (!isRaster && !isBeat && !isBar)
            continue;

        if (isBar)
            g.setColour(Palette::gridBarLine);
        else if (isBeat)
            g.setColour(Palette::gridBeatLine);
        else
            g.setColour(Palette::gridLine);

        g.drawVerticalLine(x, static_cast<float>(kRulerHeight), static_cast<float>(totalHeight));
    }

    // 3. Clips & Overlap Representation
    if (pat != nullptr)
    {
        for (int t = 0; t < numTracks; ++t)
        {
            const auto& track = pat->tracks[static_cast<std::size_t>(t)];
            const int patSteps = pat->lengthSteps();

        struct ClipInterval
        {
            ClipId id;
            double start = 0.0;
            double end = 0.0;
        };

        std::vector<ClipInterval> intervals;
        for (const auto& clip : track.clips)
        {
            if (clip.startStep >= patSteps)
                continue;

            double start = clip.startStep;
            double span = std::min(clipSpan(clip, t), static_cast<double>(patSteps) - start);
            if (drag_.active)
            {
                if (const auto* dItem = drag_.find(clip.id))
                {
                    if (dItem->currentTrack != t)
                        continue;
                    start = dItem->currentStartStep;
                    span = std::min(dItem->currentLengthSteps, std::max(0.0, static_cast<double>(patSteps) - start));
                }
            }
            if (span > 0.0)
                intervals.push_back({clip.id, start, start + span});
        }

        if (drag_.active)
        {
            for (const auto& dItem : drag_.items)
            {
                if (dItem.currentTrack == t && dItem.originalTrack != t)
                {
                    const double start = dItem.currentStartStep;
                    const double span = std::min(dItem.currentLengthSteps, std::max(0.0, static_cast<double>(patSteps) - start));
                    if (span > 0.0)
                        intervals.push_back({dItem.clipId, start, start + span});
                }
            }
        }

        struct OverlapSegment
        {
            double startStep = 0.0;
            double endStep = 0.0;
            int clipCount = 0;
        };
        std::vector<OverlapSegment> overlapSegments;
        std::unordered_set<std::uint32_t> overlappingClipIds;

        for (std::size_t i = 0; i < intervals.size(); ++i)
        {
            for (std::size_t j = i + 1; j < intervals.size(); ++j)
            {
                const auto& a = intervals[i];
                const auto& b = intervals[j];
                const double oStart = std::max(a.start, b.start);
                const double oEnd = std::min(a.end, b.end);
                if (oEnd > oStart)
                {
                    overlappingClipIds.insert(a.id.value);
                    overlappingClipIds.insert(b.id.value);
                    overlapSegments.push_back({oStart, oEnd, 2});
                }
            }
        }

        // Render each clip
        for (const auto& clip : track.clips)
        {
            int renderTrack = t;
            double renderStep = clip.startStep;
            double span = clipSpan(clip, renderTrack);
            double renderOffset = clip.offsetSteps;

            if (drag_.active)
            {
                if (const auto* dItem = drag_.find(clip.id))
                {
                    renderTrack = dItem->currentTrack;
                    renderStep = dItem->currentStartStep;
                    span = dItem->currentLengthSteps;
                    renderOffset = dItem->currentOffsetSteps;
                    if (renderTrack != t)
                        continue;
                }
            }

            if (renderStep >= patSteps)
                continue;

            // Cut off clip cleanly at the pattern loop point
            span = std::min(span, static_cast<double>(patSteps) - renderStep);
            if (span <= 0.0)
                continue;

            const SampleKind trackKind = song_.patternTrackKind(patternId_, renderTrack);
            const bool isLoop = (trackKind == SampleKind::Loop);

            const auto rawBounds = clipBounds(renderTrack, renderStep, span);
            auto bounds = (rawBounds.getWidth() > 6) ? rawBounds.reduced(2, 3) : rawBounds.reduced(0, 3);
            const juce::Colour col = sampleColour(clip.sampleId);

            const bool isOverlapping = overlappingClipIds.contains(clip.id.value);

            // Stacked card tab if overlapping
            if (isOverlapping)
            {
                auto tab = bounds.translated(0, -3).reduced(3, 0);
                g.setColour(col.darker(0.35f).withAlpha(0.6f));
                g.fillRoundedRectangle(tab.toFloat(), 3.0f);
                g.setColour(juce::Colour(0xfff59e0b).withAlpha(0.75f));
                g.drawRoundedRectangle(tab.toFloat(), 3.0f, 1.0f);
            }

            // Container
            g.setColour(juce::Colour(0xff1a1e26).withAlpha(isOverlapping ? 0.90f : 1.0f));
            g.fillRoundedRectangle(bounds.toFloat(), 4.0f);
            g.setColour(isLoop ? juce::Colour(0xfff59e0b).withAlpha(0.16f) : col.withAlpha(0.12f));
            g.fillRoundedRectangle(bounds.toFloat(), 4.0f);

            // Accent stripe
            g.setColour(isLoop ? juce::Colour(0xffffb300) : col);
            g.fillRoundedRectangle(static_cast<float>(bounds.getX()), static_cast<float>(bounds.getY()),
                                   static_cast<float>(bounds.getWidth()), 3.0f, 2.0f);

            // Waveform
            auto rawAudio = stretchCache_.getRawAudio(clip.sampleId);
            if (rawAudio != nullptr && rawAudio->numFrames() > 0 && bounds.getWidth() > 4)
            {
                const Sample* sample = song_.findSample(clip.sampleId);
                double fullSampleSteps = span;
                if (sample != nullptr)
                {
                    if (isLoop)
                    {
                        const int loopBars = std::max(1, sample->loopBars);
                        fullSampleSteps = static_cast<double>(loopBars * kStepsPerBar);
                    }
                    else
                    {
                        const double stepSec = 60.0 / song_.bpm() / kStepsPerBeat;
                        fullSampleSteps = std::max(0.1, sample->durationSeconds / stepSec);
                    }
                }

                const std::int64_t totalFrames = rawAudio->numFrames();
                const std::int64_t startFrame =
                    (fullSampleSteps > 0.0)
                        ? std::clamp<std::int64_t>(
                              static_cast<std::int64_t>(std::round((renderOffset / fullSampleSteps) * static_cast<double>(totalFrames))), 0,
                              totalFrames)
                        : 0;
                const std::int64_t clipFrames =
                    (fullSampleSteps > 0.0)
                        ? std::clamp<std::int64_t>(
                              static_cast<std::int64_t>(std::round((span / fullSampleSteps) * static_cast<double>(totalFrames))), 0,
                              totalFrames - startFrame)
                        : (totalFrames - startFrame);

                if (clipFrames > 0)
                {
                    const float* audioL = rawAudio->left();
                    const float* audioR = rawAudio->right();
                    const float cy = static_cast<float>(bounds.getCentreY());
                    const float maxAmp = static_cast<float>(bounds.getHeight() - 8) * 0.5f;

                    g.saveState();
                    g.reduceClipRegion(bounds.reduced(2, 2));

                    g.setColour(col.withAlpha(0.25f));
                    g.drawHorizontalLine(static_cast<int>(cy), static_cast<float>(bounds.getX() + 2),
                                         static_cast<float>(bounds.getRight() - 2));

                    g.setColour(col.brighter(0.45f));
                    const int w = bounds.getWidth() - 4;
                    const int bx = bounds.getX() + 2;

                    for (int px = 0; px < w; ++px)
                    {
                        const int samplePx = clip.reversed ? (w - 1 - px) : px;
                        const std::int64_t f0 =
                            startFrame + (static_cast<std::int64_t>(samplePx) * clipFrames) / w;
                        const std::int64_t f1 = std::min<std::int64_t>(
                            totalFrames,
                            startFrame + (static_cast<std::int64_t>(samplePx + 1) * clipFrames) / w);

                        float minVal = 0.0f;
                        float maxVal = 0.0f;

                        if (f1 > f0)
                        {
                            const std::int64_t stepSize = std::max<std::int64_t>(1, (f1 - f0) / 32);
                            for (std::int64_t f = f0; f < f1; f += stepSize)
                            {
                                const float s = 0.5f * (audioL[f] + audioR[f]);
                                minVal = std::min(minVal, s);
                                maxVal = std::max(maxVal, s);
                            }
                        }

                        const float y1 = cy - maxVal * maxAmp;
                        const float y2 = cy - minVal * maxAmp;
                        const float topY = std::min(y1, y2);
                        const float botY = std::max(y1, y2);
                        const float lineH = std::max(1.0f, botY - topY);

                        g.drawVerticalLine(bx + px, topY, topY + lineH);
                    }
                    g.restoreState();
                }
            }

            // Loop notches (only drawn for Loop tracks)
            const Sample* s = song_.findSample(clip.sampleId);
            if (isLoop && s != nullptr)
            {
                const int loopBars = std::max(1, s->loopBars);
                const int loopSteps = loopBars * kStepsPerBar;
                if (loopSteps > 0 && span > loopSteps)
                {
                    g.setColour(col.brighter(0.7f));
                    for (int st = loopSteps; st < span; st += loopSteps)
                    {
                        const int notchX = bounds.getX() + st * stepWidth_;
                        g.drawVerticalLine(notchX, static_cast<float>(bounds.getY() + 3),
                                           static_cast<float>(bounds.getY() + 8));
                        g.drawVerticalLine(notchX, static_cast<float>(bounds.getBottom() - 6),
                                           static_cast<float>(bounds.getBottom()));
                    }
                }
            }

            // Edge handles
            if (bounds.getWidth() >= 16)
            {
                g.setColour(juce::Colours::white.withAlpha(0.4f));
                g.fillRect(bounds.getX() + 2, bounds.getY() + 4, 2, bounds.getHeight() - 8);
                g.fillRect(bounds.getRight() - 4, bounds.getY() + 4, 2, bounds.getHeight() - 8);
            }

            // Kind badge: [L] or [1], with REV indicator if reversed
            if (bounds.getWidth() >= 20 && trackHeight_ >= 18)
            {
                const float fontSize = std::clamp(static_cast<float>(trackHeight_) * 0.3f, 8.0f, 11.0f);
                g.setFont(juce::FontOptions(fontSize).withStyle("Bold"));

                juce::String badgeText;
                if (clip.reversed)
                    badgeText += "REV ";
                if (clip.transposeSemitones != 0)
                    badgeText += (clip.transposeSemitones > 0 ? "+" : "") + juce::String(clip.transposeSemitones) + "st ";
                badgeText += isLoop ? "[L]" : "[1]";

                if (clip.reversed)
                    g.setColour(juce::Colour(0xff38bdf8));
                else if (isLoop)
                    g.setColour(juce::Colour(0xffffca28));
                else
                    g.setColour(Palette::textDim.withAlpha(0.65f));

                g.drawText(badgeText, bounds.reduced(6, 3), juce::Justification::topRight, false);
            }

            // Volume Envelope (Gain Line, Fade In/Out Curves & Handles)
            float effectiveGain = clip.gain;
            float effectiveFadeIn = clip.fadeInSteps;
            float effectiveFadeOut = clip.fadeOutSteps;

            if (drag_.active)
            {
                if (const auto* dItem = drag_.find(clip.id))
                {
                    if (drag_.mode == DragMode::Gain)
                        effectiveGain = dItem->currentGain;
                    else if (drag_.mode == DragMode::FadeIn)
                        effectiveFadeIn = dItem->currentFadeIn;
                    else if (drag_.mode == DragMode::FadeOut)
                        effectiveFadeOut = dItem->currentFadeOut;
                }
            }

            if (bounds.getWidth() >= 12 && bounds.getHeight() >= 16)
            {
                const float gainNorm = std::clamp(effectiveGain, 0.0f, 2.0f) * 0.5f;
                const float gainY = static_cast<float>(bounds.getY()) + static_cast<float>(bounds.getHeight()) * (1.0f - gainNorm);
                const int fadeInPx = std::min(bounds.getWidth() - 6, juce::roundToInt(effectiveFadeIn * static_cast<float>(stepWidth_)));
                const int fadeOutPx = std::min(bounds.getWidth() - 6, juce::roundToInt(effectiveFadeOut * static_cast<float>(stepWidth_)));

                // 1. Shaded area & curve for Fade In
                if (fadeInPx > 0)
                {
                    juce::Path p;
                    p.startNewSubPath(static_cast<float>(bounds.getX()), static_cast<float>(bounds.getBottom()));
                    p.lineTo(static_cast<float>(bounds.getX() + fadeInPx), gainY);
                    p.lineTo(static_cast<float>(bounds.getX()), gainY);
                    p.closeSubPath();
                    g.setColour(juce::Colours::black.withAlpha(0.32f));
                    g.fillPath(p);

                    juce::Path curve;
                    curve.startNewSubPath(static_cast<float>(bounds.getX()), static_cast<float>(bounds.getBottom()));
                    curve.lineTo(static_cast<float>(bounds.getX() + fadeInPx), gainY);
                    g.setColour(juce::Colours::white.withAlpha(0.85f));
                    g.strokePath(curve, juce::PathStrokeType(1.5f));

                    // Fade in handle
                    g.setColour(juce::Colour(0xff3ea2c5));
                    g.fillEllipse(static_cast<float>(bounds.getX() + fadeInPx - 3), static_cast<float>(bounds.getY() + 3), 7.0f, 7.0f);
                    g.setColour(juce::Colours::white);
                    g.drawEllipse(static_cast<float>(bounds.getX() + fadeInPx - 3), static_cast<float>(bounds.getY() + 3), 7.0f, 7.0f, 1.0f);
                }
                else
                {
                    // Subtle corner triangle hint
                    g.setColour(juce::Colours::white.withAlpha(0.35f));
                    juce::Path tri;
                    tri.startNewSubPath(static_cast<float>(bounds.getX() + 2), static_cast<float>(bounds.getY() + 2));
                    tri.lineTo(static_cast<float>(bounds.getX() + 8), static_cast<float>(bounds.getY() + 2));
                    tri.lineTo(static_cast<float>(bounds.getX() + 2), static_cast<float>(bounds.getY() + 8));
                    tri.closeSubPath();
                    g.fillPath(tri);
                }

                // 2. Shaded area & curve for Fade Out
                if (fadeOutPx > 0)
                {
                    juce::Path p;
                    p.startNewSubPath(static_cast<float>(bounds.getRight() - fadeOutPx), gainY);
                    p.lineTo(static_cast<float>(bounds.getRight()), static_cast<float>(bounds.getBottom()));
                    p.lineTo(static_cast<float>(bounds.getRight()), gainY);
                    p.closeSubPath();
                    g.setColour(juce::Colours::black.withAlpha(0.32f));
                    g.fillPath(p);

                    juce::Path curve;
                    curve.startNewSubPath(static_cast<float>(bounds.getRight() - fadeOutPx), gainY);
                    curve.lineTo(static_cast<float>(bounds.getRight()), static_cast<float>(bounds.getBottom()));
                    g.setColour(juce::Colours::white.withAlpha(0.85f));
                    g.strokePath(curve, juce::PathStrokeType(1.5f));

                    // Fade out handle
                    g.setColour(juce::Colour(0xff3ea2c5));
                    g.fillEllipse(static_cast<float>(bounds.getRight() - fadeOutPx - 4), static_cast<float>(bounds.getY() + 3), 7.0f, 7.0f);
                    g.setColour(juce::Colours::white);
                    g.drawEllipse(static_cast<float>(bounds.getRight() - fadeOutPx - 4), static_cast<float>(bounds.getY() + 3), 7.0f, 7.0f, 1.0f);
                }
                else
                {
                    // Subtle corner triangle hint
                    g.setColour(juce::Colours::white.withAlpha(0.35f));
                    juce::Path tri;
                    tri.startNewSubPath(static_cast<float>(bounds.getRight() - 2), static_cast<float>(bounds.getY() + 2));
                    tri.lineTo(static_cast<float>(bounds.getRight() - 8), static_cast<float>(bounds.getY() + 2));
                    tri.lineTo(static_cast<float>(bounds.getRight() - 2), static_cast<float>(bounds.getY() + 8));
                    tri.closeSubPath();
                    g.fillPath(tri);
                }

                // 3. Volume Gain Line across the active sustain area
                const float lineStartX = static_cast<float>(bounds.getX() + fadeInPx);
                const float lineEndX = static_cast<float>(bounds.getRight() - fadeOutPx);
                if (lineEndX > lineStartX)
                {
                    g.setColour(juce::Colours::white.withAlpha(0.55f));
                    g.drawHorizontalLine(juce::roundToInt(gainY), lineStartX, lineEndX);

                    // Center gain handle
                    const float cx = bounds.getCentreX();
                    if (cx >= lineStartX && cx <= lineEndX)
                    {
                        g.setColour(juce::Colour(0xff2d3139));
                        g.fillRoundedRectangle(cx - 6.0f, gainY - 3.5f, 12.0f, 7.0f, 2.0f);
                        g.setColour(juce::Colour(0xff3ea2c5));
                        g.drawRoundedRectangle(cx - 6.0f, gainY - 3.5f, 12.0f, 7.0f, 2.0f, 1.2f);
                    }
                }
            }

            // Selection / Overlap border
            const bool isSelected = isClipSelected(clip.id);

            if (isSelected)
            {
                g.setColour(juce::Colours::white);
                g.drawRoundedRectangle(bounds.toFloat(), 4.0f, 2.0f);
                if (isOverlapping)
                {
                    g.setColour(juce::Colour(0xfff59e0b).withAlpha(0.7f));
                    g.drawRoundedRectangle(bounds.toFloat().expanded(1.0f), 4.5f, 1.0f);
                }
            }
            else if (isOverlapping)
            {
                g.setColour(juce::Colour(0xfff59e0b)); // warm amber overlap outline
                g.drawRoundedRectangle(bounds.toFloat(), 4.0f, 2.0f);
            }
            else
            {
                g.setColour(col.withAlpha(0.6f));
                g.drawRoundedRectangle(bounds.toFloat(), 4.0f, 1.0f);
            }
        }

        // Draw Overlap / Crossfade regions and badges on track t
        for (const auto& seg : overlapSegments)
        {
            const int ox = juce::roundToInt(seg.startStep * static_cast<double>(stepWidth_));
            const int ow = std::max(2, juce::roundToInt((seg.endStep - seg.startStep) * static_cast<double>(stepWidth_)));
            const int oy = kRulerHeight + t * trackHeight_;
            const int oh = trackHeight_;

            if (ow <= 2)
                continue;

            const auto oBounds = juce::Rectangle<int>(ox, oy + 2, ow, oh - 4);

            // 1. Shaded crossfade background
            g.setColour(juce::Colour(0x35f59e0b)); // amber crossfade tint
            g.fillRoundedRectangle(oBounds.toFloat(), 3.0f);

            // 2. Crossfade 'X' curves
            juce::Path pXFade1, pXFade2;
            pXFade1.startNewSubPath(static_cast<float>(oBounds.getX()), static_cast<float>(oBounds.getY() + 3));
            pXFade1.lineTo(static_cast<float>(oBounds.getRight()), static_cast<float>(oBounds.getBottom() - 3));

            pXFade2.startNewSubPath(static_cast<float>(oBounds.getX()), static_cast<float>(oBounds.getBottom() - 3));
            pXFade2.lineTo(static_cast<float>(oBounds.getRight()), static_cast<float>(oBounds.getY() + 3));

            g.setColour(juce::Colour(0xbbf59e0b));
            g.strokePath(pXFade1, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved));
            g.strokePath(pXFade2, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved));

            // 3. Vertical boundary lines
            g.setColour(juce::Colour(0xfff59e0b));
            g.drawVerticalLine(oBounds.getX(), static_cast<float>(oBounds.getY()), static_cast<float>(oBounds.getBottom()));
            g.drawVerticalLine(oBounds.getRight(), static_cast<float>(oBounds.getY()), static_cast<float>(oBounds.getBottom()));

            // 4. Overlap Badge: e.g. [2 Clips] or [2x]
            const juce::String badgeText = juce::String::formatted("%d Clips", seg.clipCount);
            g.setFont(Typography::bold(9.5f));
            const int textW = juce::roundToInt(g.getCurrentFont().getStringWidthFloat(badgeText));
            const int badgeW = textW + 12;
            const int badgeH = 15;

            if (ow >= badgeW + 4 && oh >= 24)
            {
                const int badgeX = oBounds.getCentreX() - badgeW / 2;
                const int badgeY = oBounds.getY() + 2;
                const auto badgeRect = juce::Rectangle<float>(static_cast<float>(badgeX), static_cast<float>(badgeY),
                                                              static_cast<float>(badgeW), static_cast<float>(badgeH));

                // Crimson pill badge
                g.setColour(juce::Colour(0xffe11d48));
                g.fillRoundedRectangle(badgeRect, 3.5f);
                g.setColour(juce::Colours::white);
                g.drawRoundedRectangle(badgeRect, 3.5f, 1.0f);

                g.setColour(juce::Colours::white);
                g.drawText(badgeText, badgeRect, juce::Justification::centred, false);
            }
            else if (ow >= 18 && oh >= 20)
            {
                const juce::String shortText = juce::String(seg.clipCount) + "x";
                const int badgeW = 20;
                const int badgeX = oBounds.getCentreX() - badgeW / 2;
                const int badgeY = oBounds.getY() + 2;
                const auto badgeRect = juce::Rectangle<float>(static_cast<float>(badgeX), static_cast<float>(badgeY),
                                                              static_cast<float>(badgeW), 13.0f);

                g.setColour(juce::Colour(0xffe11d48));
                g.fillRoundedRectangle(badgeRect, 3.0f);
                g.setColour(juce::Colours::white);
                g.drawRoundedRectangle(badgeRect, 3.0f, 1.0f);
                g.setFont(Typography::bold(8.5f));
                g.drawText(shortText, badgeRect, juce::Justification::centred, false);
            }
        }
    }
    }

    // Active grid cursor / target cell (for paste and new clip insertion)
    if (cursorTrack_ >= 0 && cursorTrack_ < numTracks && cursorStep_ >= 0 && selectedClips_.empty())
    {
        const int cx = stepToX(cursorStep_);
        const int cy = kRulerHeight + cursorTrack_ * trackHeight_;
        const int cw = std::max(stepToX(cursorStep_ + rasterSteps_) - cx, 6);
        const auto cellRect = juce::Rectangle<float>(static_cast<float>(cx + 1), static_cast<float>(cy + 1),
                                                     static_cast<float>(cw - 2), static_cast<float>(trackHeight_ - 2));

        g.setColour(Palette::accent.withAlpha(0.12f));
        g.fillRoundedRectangle(cellRect, 3.0f);
        g.setColour(Palette::accent.withAlpha(0.75f));
        g.drawRoundedRectangle(cellRect, 3.0f, 1.5f);
    }

    // 4. Playhead
    if (playheadStep_ >= 0.0)
    {
        const int px = stepToX(playheadStep_);
        g.setColour(Palette::accent.withAlpha(0.4f));
        g.drawVerticalLine(px, 0.0f, static_cast<float>(totalHeight));
        g.setColour(Palette::accent);
        g.drawVerticalLine(px - 1, 0.0f, static_cast<float>(totalHeight));
        g.drawVerticalLine(px + 1, 0.0f, static_cast<float>(totalHeight));

        juce::Path cap;
        cap.startNewSubPath(static_cast<float>(px - 6), 0.0f);
        cap.lineTo(static_cast<float>(px + 6), 0.0f);
        cap.lineTo(static_cast<float>(px), static_cast<float>(kRulerHeight));
        cap.closeSubPath();
        g.fillPath(cap);
    }

    // 5. Drag & Drop Highlight
    if (dragHoverTrack_ >= 0)
    {
        if (dragHoverTrack_ < numTracks)
        {
            const int y = kRulerHeight + dragHoverTrack_ * trackHeight_;
            g.setColour(Palette::accent.withAlpha(0.18f));
            g.fillRect(0, y, renderWidth, trackHeight_);
            g.setColour(Palette::accent.withAlpha(0.85f));
            g.drawRect(0.0f, static_cast<float>(y), static_cast<float>(renderWidth), static_cast<float>(trackHeight_), 1.5f);

            if (dragHoverStep_ >= 0)
            {
                const int hx = stepToX(dragHoverStep_);
                const int hw = std::max(stepToX(dragHoverStep_ + rasterSteps_) - hx, stepWidth_);
                auto dropBox = juce::Rectangle<int>(hx + 2, y + 3, hw - 4, trackHeight_ - 6);
                g.setColour(Palette::accent.withAlpha(0.35f));
                g.fillRoundedRectangle(dropBox.toFloat(), 4.0f);
                g.setColour(Palette::accent);
                g.drawRoundedRectangle(dropBox.toFloat(), 4.0f, 1.5f);
            }
        }
        else
        {
            const int y = kRulerHeight + numTracks * trackHeight_;
            g.setColour(Palette::accent.withAlpha(0.20f));
            g.fillRect(0, y, renderWidth, trackHeight_);
            g.setColour(Palette::accent);
            g.drawRect(0.0f, static_cast<float>(y), static_cast<float>(renderWidth), static_cast<float>(trackHeight_), 1.5f);
            g.setColour(Palette::accent);
            g.setFont(Typography::bold(13.0f));
            g.drawText("+ Drop here to create new track", 12, y, renderWidth - 24, trackHeight_,
                       juce::Justification::centredLeft);
        }
    }

    // Marquee selection box
    if (isMarquee_)
    {
        const auto mr = juce::Rectangle<int>::leftTopRightBottom(
            std::min(marqueeStart_.x, marqueeEnd_.x),
            std::min(marqueeStart_.y, marqueeEnd_.y),
            std::max(marqueeStart_.x, marqueeEnd_.x),
            std::max(marqueeStart_.y, marqueeEnd_.y));
        g.setColour(Palette::accent.withAlpha(0.18f));
        g.fillRect(mr);
        g.setColour(Palette::accent.withAlpha(0.9f));
        g.drawRect(mr.toFloat(), 1.0f);
    }

    // 6. Floating HUD badge when dragging envelope handles or clip
    if (drag_.active && !dragTooltipText_.isEmpty())
    {
        g.setFont(Typography::bold(11.5f));
        const int textW = g.getCurrentFont().getStringWidth(dragTooltipText_) + 18;
        constexpr int textH = 22;
        const int badgeX = std::clamp(dragCurrentMousePos_.x - textW / 2, 4, getWidth() - textW - 4);
        const int badgeY = std::max(kRulerHeight + 4, dragCurrentMousePos_.y - 30);

        g.setColour(juce::Colour(0xff181a1f).withAlpha(0.94f));
        g.fillRoundedRectangle(static_cast<float>(badgeX), static_cast<float>(badgeY), static_cast<float>(textW), static_cast<float>(textH), 4.0f);
        g.setColour(Palette::accent);
        g.drawRoundedRectangle(static_cast<float>(badgeX), static_cast<float>(badgeY), static_cast<float>(textW), static_cast<float>(textH), 4.0f, 1.2f);
        g.setColour(juce::Colours::white);
        g.drawText(dragTooltipText_, badgeX, badgeY, textW, textH, juce::Justification::centred);
    }
}

void PatternGrid::mouseDown(const juce::MouseEvent& e)
{
    dragCurrentMousePos_ = e.getPosition();

    if (e.mods.isRightButtonDown())
    {
        if (const Clip* clip = findClipAt(e.getPosition()))
        {
            const ClipId cid = clip->id;
            if (!isClipSelected(cid))
            {
                setSelectedClip(cid);
                listener_.clipClicked(cid);
            }

            const float curGain = clip->gain;
            const float curFadeIn = clip->fadeInSteps;
            const float curFadeOut = clip->fadeOutSteps;
            const int curTranspose = clip->transposeSemitones;

            juce::PopupMenu m;
            juce::PopupMenu volMenu;
            volMenu.addItem(101, "0.0 dB (100%)", true, std::abs(curGain - 1.0f) < 0.01f);
            volMenu.addItem(102, "+3.0 dB (141%)", true, std::abs(curGain - 1.41f) < 0.05f);
            volMenu.addItem(103, "+6.0 dB (200%)", true, std::abs(curGain - 2.0f) < 0.05f);
            volMenu.addItem(104, "-3.0 dB (71%)", true, std::abs(curGain - 0.71f) < 0.05f);
            volMenu.addItem(105, "-6.0 dB (50%)", true, std::abs(curGain - 0.50f) < 0.05f);
            volMenu.addItem(106, "-12.0 dB (25%)", true, std::abs(curGain - 0.25f) < 0.05f);
            volMenu.addItem(107, "Mute Clip (0%)", true, curGain < 0.01f);
            m.addSubMenu("Volume Gain", volMenu);

            juce::PopupMenu fadeInMenu;
            fadeInMenu.addItem(201, "None (0 steps)", true, curFadeIn < 0.01f);
            fadeInMenu.addItem(202, "1/2 Step", true, std::abs(curFadeIn - 0.5f) < 0.1f);
            fadeInMenu.addItem(203, "1 Step (1/16)", true, std::abs(curFadeIn - 1.0f) < 0.1f);
            fadeInMenu.addItem(204, "2 Steps (1/8)", true, std::abs(curFadeIn - 2.0f) < 0.1f);
            fadeInMenu.addItem(205, "4 Steps (1/4)", true, std::abs(curFadeIn - 4.0f) < 0.1f);
            fadeInMenu.addItem(206, "8 Steps (1/2 Bar)", true, std::abs(curFadeIn - 8.0f) < 0.1f);
            m.addSubMenu("Fade In", fadeInMenu);

            juce::PopupMenu fadeOutMenu;
            fadeOutMenu.addItem(301, "None (0 steps)", true, curFadeOut < 0.01f);
            fadeOutMenu.addItem(302, "1/2 Step", true, std::abs(curFadeOut - 0.5f) < 0.1f);
            fadeOutMenu.addItem(303, "1 Step (1/16)", true, std::abs(curFadeOut - 1.0f) < 0.1f);
            fadeOutMenu.addItem(304, "2 Steps (1/8)", true, std::abs(curFadeOut - 2.0f) < 0.1f);
            fadeOutMenu.addItem(305, "4 Steps (1/4)", true, std::abs(curFadeOut - 4.0f) < 0.1f);
            fadeOutMenu.addItem(306, "8 Steps (1/2 Bar)", true, std::abs(curFadeOut - 8.0f) < 0.1f);
            m.addSubMenu("Fade Out", fadeOutMenu);

            juce::PopupMenu transposeMenu;
            transposeMenu.addItem(401, "Transpose Up (+1 st) [+]", true, false);
            transposeMenu.addItem(402, "Transpose Down (-1 st) [-]", true, false);
            transposeMenu.addSeparator();
            transposeMenu.addItem(412, "+12 st (Octave Up)", true, curTranspose == 12);
            transposeMenu.addItem(407, "+7 st (Fifth Up)", true, curTranspose == 7);
            transposeMenu.addItem(405, "+5 st (Fourth Up)", true, curTranspose == 5);
            transposeMenu.addItem(404, "+4 st (Major 3rd)", true, curTranspose == 4);
            transposeMenu.addItem(403, "+3 st (Minor 3rd)", true, curTranspose == 3);
            transposeMenu.addItem(400, "0 st (Original Pitch)", true, curTranspose == 0);
            transposeMenu.addItem(423, "-3 st (Minor 3rd)", true, curTranspose == -3);
            transposeMenu.addItem(424, "-4 st (Major 3rd)", true, curTranspose == -4);
            transposeMenu.addItem(425, "-5 st (Fourth Down)", true, curTranspose == -5);
            transposeMenu.addItem(427, "-7 st (Fifth Down)", true, curTranspose == -7);
            transposeMenu.addItem(432, "-12 st (Octave Down)", true, curTranspose == -12);
            transposeMenu.addSeparator();
            transposeMenu.addItem(450, "Reset Transpose (0 st)", true, false);

            juce::String transposeHeader = "Transpose (Pitch)";
            if (curTranspose != 0)
                transposeHeader += juce::String::formatted(" [%+d st]", curTranspose);
            m.addSubMenu(transposeHeader, transposeMenu);

            const bool multi = (isClipSelected(cid) && selectedClips_.size() > 1);
            m.addSeparator();
            m.addItem(5, "Reverse (R)", true, clip->reversed);
            m.addSeparator();
            if (multi)
            {
                m.addItem(2, juce::String::formatted("Cut %d Clips (Cmd+X)", static_cast<int>(selectedClips_.size())));
                m.addItem(3, juce::String::formatted("Copy %d Clips (Cmd+C)", static_cast<int>(selectedClips_.size())));
                m.addItem(4, juce::String::formatted("Duplicate %d Clips (Cmd+D)", static_cast<int>(selectedClips_.size())));
                m.addItem(1, juce::String::formatted("Delete %d Clips (Del)", static_cast<int>(selectedClips_.size())));
            }
            else
            {
                m.addItem(2, "Cut Clip (Cmd+X)");
                m.addItem(3, "Copy Clip (Cmd+C)");
                m.addItem(4, "Duplicate Clip (Cmd+D)");
                m.addItem(1, "Delete Clip (Del)");
            }

            m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withTargetScreenArea(
                                juce::Rectangle<int>(e.getScreenX(), e.getScreenY(), 1, 1)),
                            [this, cid, curGain, curFadeIn, curFadeOut](int res) {
                                if (res == 1)
                                {
                                    if (isClipSelected(cid) && selectedClips_.size() > 1)
                                        listener_.clipsDeleteRequested(selectedClips_);
                                    else
                                        listener_.clipDeleteRequested(cid);
                                }
                                else if (res == 2) listener_.clipCutRequested(cid);
                                else if (res == 3) listener_.clipCopyRequested(cid);
                                else if (res == 4) listener_.clipDuplicateRequested(cid);
                                else if (res == 5)
                                {
                                    if (isClipSelected(cid) && selectedClips_.size() > 1)
                                        listener_.clipsReverseRequested(selectedClips_);
                                    else
                                        listener_.clipReverseRequested(cid);
                                }
                                else if (res == 101) listener_.clipEnvelopeChanged(cid, 1.0f, curFadeIn, curFadeOut);
                                else if (res == 102) listener_.clipEnvelopeChanged(cid, 1.41f, curFadeIn, curFadeOut);
                                else if (res == 103) listener_.clipEnvelopeChanged(cid, 2.0f, curFadeIn, curFadeOut);
                                else if (res == 104) listener_.clipEnvelopeChanged(cid, 0.71f, curFadeIn, curFadeOut);
                                else if (res == 105) listener_.clipEnvelopeChanged(cid, 0.50f, curFadeIn, curFadeOut);
                                else if (res == 106) listener_.clipEnvelopeChanged(cid, 0.25f, curFadeIn, curFadeOut);
                                else if (res == 107) listener_.clipEnvelopeChanged(cid, 0.0f, curFadeIn, curFadeOut);
                                else if (res == 201) listener_.clipEnvelopeChanged(cid, curGain, 0.0f, curFadeOut);
                                else if (res == 202) listener_.clipEnvelopeChanged(cid, curGain, 0.5f, curFadeOut);
                                else if (res == 203) listener_.clipEnvelopeChanged(cid, curGain, 1.0f, curFadeOut);
                                else if (res == 204) listener_.clipEnvelopeChanged(cid, curGain, 2.0f, curFadeOut);
                                else if (res == 205) listener_.clipEnvelopeChanged(cid, curGain, 4.0f, curFadeOut);
                                else if (res == 206) listener_.clipEnvelopeChanged(cid, curGain, 8.0f, curFadeOut);
                                else if (res == 301) listener_.clipEnvelopeChanged(cid, curGain, curFadeIn, 0.0f);
                                else if (res == 302) listener_.clipEnvelopeChanged(cid, curGain, curFadeIn, 0.5f);
                                else if (res == 303) listener_.clipEnvelopeChanged(cid, curGain, curFadeIn, 1.0f);
                                else if (res == 304) listener_.clipEnvelopeChanged(cid, curGain, curFadeIn, 2.0f);
                                else if (res == 305) listener_.clipEnvelopeChanged(cid, curGain, curFadeIn, 4.0f);
                                else if (res == 306) listener_.clipEnvelopeChanged(cid, curGain, curFadeIn, 8.0f);
                                else if (res == 401)
                                {
                                    const auto targets = (isClipSelected(cid) && selectedClips_.size() > 1) ? selectedClips_ : std::vector<ClipId>{cid};
                                    listener_.clipsTransposeDelta(targets, 1);
                                }
                                else if (res == 402)
                                {
                                    const auto targets = (isClipSelected(cid) && selectedClips_.size() > 1) ? selectedClips_ : std::vector<ClipId>{cid};
                                    listener_.clipsTransposeDelta(targets, -1);
                                }
                                else if (res == 400 || res == 450)
                                {
                                    const auto targets = (isClipSelected(cid) && selectedClips_.size() > 1) ? selectedClips_ : std::vector<ClipId>{cid};
                                    listener_.clipsTransposeSet(targets, 0);
                                }
                                else if (res == 412)
                                {
                                    const auto targets = (isClipSelected(cid) && selectedClips_.size() > 1) ? selectedClips_ : std::vector<ClipId>{cid};
                                    listener_.clipsTransposeSet(targets, 12);
                                }
                                else if (res == 407)
                                {
                                    const auto targets = (isClipSelected(cid) && selectedClips_.size() > 1) ? selectedClips_ : std::vector<ClipId>{cid};
                                    listener_.clipsTransposeSet(targets, 7);
                                }
                                else if (res == 405)
                                {
                                    const auto targets = (isClipSelected(cid) && selectedClips_.size() > 1) ? selectedClips_ : std::vector<ClipId>{cid};
                                    listener_.clipsTransposeSet(targets, 5);
                                }
                                else if (res == 404)
                                {
                                    const auto targets = (isClipSelected(cid) && selectedClips_.size() > 1) ? selectedClips_ : std::vector<ClipId>{cid};
                                    listener_.clipsTransposeSet(targets, 4);
                                }
                                else if (res == 403)
                                {
                                    const auto targets = (isClipSelected(cid) && selectedClips_.size() > 1) ? selectedClips_ : std::vector<ClipId>{cid};
                                    listener_.clipsTransposeSet(targets, 3);
                                }
                                else if (res == 423)
                                {
                                    const auto targets = (isClipSelected(cid) && selectedClips_.size() > 1) ? selectedClips_ : std::vector<ClipId>{cid};
                                    listener_.clipsTransposeSet(targets, -3);
                                }
                                else if (res == 424)
                                {
                                    const auto targets = (isClipSelected(cid) && selectedClips_.size() > 1) ? selectedClips_ : std::vector<ClipId>{cid};
                                    listener_.clipsTransposeSet(targets, -4);
                                }
                                else if (res == 425)
                                {
                                    const auto targets = (isClipSelected(cid) && selectedClips_.size() > 1) ? selectedClips_ : std::vector<ClipId>{cid};
                                    listener_.clipsTransposeSet(targets, -5);
                                }
                                else if (res == 427)
                                {
                                    const auto targets = (isClipSelected(cid) && selectedClips_.size() > 1) ? selectedClips_ : std::vector<ClipId>{cid};
                                    listener_.clipsTransposeSet(targets, -7);
                                }
                                else if (res == 432)
                                {
                                    const auto targets = (isClipSelected(cid) && selectedClips_.size() > 1) ? selectedClips_ : std::vector<ClipId>{cid};
                                    listener_.clipsTransposeSet(targets, -12);
                                }
                            });
        }
        return;
    }

    const auto* pat = song_.findPattern(patternId_);
    const int totalSteps = pat != nullptr ? pat->lengthSteps() : kStepsPerBar;

    const float playheadPx = static_cast<float>(stepToX(playheadStep_));
    const bool isNearPlayhead = (playheadStep_ >= 0.0 && std::abs(static_cast<float>(e.getPosition().x) - playheadPx) <= 6.0f);

    if (e.getPosition().y < kRulerHeight)
    {
        const bool isShiftOrCmd = e.mods.isShiftDown() || e.mods.isCommandDown();
        if (isShiftOrCmd && !e.mods.isPopupMenu())
        {
            startMarquee(e.getPosition(), e.mods);
            return;
        }

        if (e.mods.isPopupMenu())
        {
            juce::PopupMenu m;
            m.addItem(1, "Expand to Full Length");
            m.addItem(2, "Loop 1 Bar");
            m.addItem(3, "Loop 2 Bars");
            m.addItem(4, "Loop 4 Bars");
            m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withTargetScreenArea(
                                juce::Rectangle<int>(e.getScreenX(), e.getScreenY(), 1, 1)),
                            [this, totalSteps](int result) {
                                if (result == 1)
                                {
                                    loopStartStep_ = 0;
                                    loopEndStep_ = totalSteps;
                                    listener_.loopRegionChanged(loopStartStep_, loopEndStep_);
                                    repaint();
                                }
                                else if (result == 2)
                                {
                                    loopStartStep_ = 0;
                                    loopEndStep_ = std::min(totalSteps, kStepsPerBar);
                                    listener_.loopRegionChanged(loopStartStep_, loopEndStep_);
                                    repaint();
                                }
                                else if (result == 3)
                                {
                                    loopStartStep_ = 0;
                                    loopEndStep_ = std::min(totalSteps, kStepsPerBar * 2);
                                    listener_.loopRegionChanged(loopStartStep_, loopEndStep_);
                                    repaint();
                                }
                                else if (result == 4)
                                {
                                    loopStartStep_ = 0;
                                    loopEndStep_ = std::min(totalSteps, kStepsPerBar * 4);
                                    listener_.loopRegionChanged(loopStartStep_, loopEndStep_);
                                    repaint();
                                }
                            });
            return;
        }

        const int clickStep = std::clamp(juce::roundToInt(xToStep(e.getPosition().x)), 0, totalSteps);
        const int snapStep = (clickStep / rasterSteps_) * rasterSteps_;

        const int startX = stepToX(loopStartStep_);
        const int endX = stepToX(loopEndStep_);
        const int mx = e.getPosition().x;
        const int my = e.getPosition().y;

        // Top 16px of ruler: Loop bar and handles
        if (my <= 16)
        {
            if (std::abs(mx - startX) <= 12)
            {
                rulerDragMode_ = RulerDragMode::ResizeStart;
            }
            else if (std::abs(mx - endX) <= 12)
            {
                rulerDragMode_ = RulerDragMode::ResizeEnd;
            }
            else if (mx > startX && mx < endX)
            {
                rulerDragMode_ = RulerDragMode::MoveRegion;
                rulerDragRegionAnchorStep_ = snapStep;
                rulerDragOriginalStart_ = loopStartStep_;
                rulerDragOriginalEnd_ = loopEndStep_;
            }
            else
            {
                rulerDragMode_ = RulerDragMode::SetRegion;
                rulerDragRegionAnchorStep_ = snapStep;
                loopStartStep_ = snapStep;
                loopEndStep_ = std::min(totalSteps, snapStep + std::max(rasterSteps_, 2));
            }
            repaint();
            return;
        }

        // Lower ruler area (y > 16): Prepare for either Playhead seek or Up/Down zoom
        rulerDragMode_ = RulerDragMode::RulerPending;
        rulerDragStartScreenPos_ = e.getScreenPosition();
        rulerDragStartZoomX_ = zoomX_;
        rulerDragClickedStep_ = snapStep;

        auto* vp = findParentComponentOfClass<juce::Viewport>();
        const int vpX = vp != nullptr ? vp->getViewPositionX() : 0;
        const int vpW = vp != nullptr ? vp->getViewWidth() : getWidth();

        // Pin playhead if visible in viewport, so its relative position remains stable
        const double phStep = playheadStep_;
        const int phPixelX = phStep >= 0.0 ? juce::roundToInt(phStep * static_cast<double>(stepWidth_)) : -1;
        const bool phVisible = (phStep >= 0.0 && phPixelX >= vpX && phPixelX <= vpX + vpW);

        if (phVisible)
        {
            rulerDragAnchorStep_ = phStep;
            rulerDragAnchorViewportX_ = phPixelX - vpX;
        }
        else
        {
            rulerDragAnchorStep_ = xToStep(e.getPosition().x);
            rulerDragAnchorViewportX_ = vp != nullptr ? e.getEventRelativeTo(vp).getPosition().x : e.getPosition().x;
        }
        return;
    }

    if (isNearPlayhead)
    {
        const int clickStep = std::clamp(juce::roundToInt(xToStep(e.getPosition().x)), 0, totalSteps);
        const int snapStep = (clickStep / rasterSteps_) * rasterSteps_;
        rulerDragMode_ = RulerDragMode::SeekPlayhead;
        listener_.playheadSeekRequested(snapStep);
        setPlayheadStep(static_cast<double>(snapStep));
        return;
    }

    const auto [track, step] = pointToTrackAndStep(e.getPosition());
    if (!pat)
        return;
    const int numTracks = static_cast<int>(pat->tracks.size());

    if (const Clip* clip = findClipAt(e.getPosition()))
    {
        grabKeyboardFocus();
        const bool isShiftOrCmd = e.mods.isShiftDown() || e.mods.isCommandDown();
        const bool wasSelected = isClipSelected(clip->id);
        primaryClipWasSelectedOnDown_ = wasSelected;

        if (isShiftOrCmd)
        {
            if (!wasSelected)
                selectedClips_.push_back(clip->id);
        }
        else
        {
            if (!wasSelected)
            {
                selectedClips_.clear();
                selectedClips_.push_back(clip->id);
            }
        }

        cursorTrack_ = -1;
        cursorStep_ = -1;
        const auto mode = hitTestResize(e.getPosition());

        drag_.active = true;
        drag_.mode = mode;
        drag_.primaryClipId = clip->id;
        drag_.startMousePos = e.getPosition();
        drag_.items.clear();

        for (auto selId : selectedClips_)
        {
            for (int t = 0; t < numTracks; ++t)
            {
                const auto& tr = pat->tracks[static_cast<std::size_t>(t)];
                auto it = std::find_if(tr.clips.begin(), tr.clips.end(),
                                       [selId](const Clip& c) { return c.id == selId; });
                if (it != tr.clips.end())
                {
                    const int span = clipSpan(*it, t);
                    DragClipItem item;
                    item.clipId = it->id;
                    item.originalTrack = t;
                    item.originalStartStep = it->startStep;
                    item.originalLengthSteps = span;
                    item.originalOffsetSteps = it->offsetSteps;
                    item.originalGain = it->gain;
                    item.originalFadeIn = it->fadeInSteps;
                    item.originalFadeOut = it->fadeOutSteps;

                    item.currentTrack = t;
                    item.currentStartStep = it->startStep;
                    item.currentLengthSteps = span;
                    item.currentOffsetSteps = it->offsetSteps;
                    item.currentGain = it->gain;
                    item.currentFadeIn = it->fadeInSteps;
                    item.currentFadeOut = it->fadeOutSteps;
                    drag_.items.push_back(item);
                    break;
                }
            }
        }

        listener_.clipClicked(clip->id);
        if (mode == DragMode::ResizeLeft || mode == DragMode::ResizeRight)
            setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
        else if (mode == DragMode::FadeIn || mode == DragMode::FadeOut)
            setMouseCursor(juce::MouseCursor::PointingHandCursor);
        else if (mode == DragMode::Gain)
            setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
        repaint();
    }
    else
    {
        grabKeyboardFocus();
        const bool isShiftOrCmd = e.mods.isShiftDown() || e.mods.isCommandDown();
        if (!isShiftOrCmd)
        {
            selectedClips_.clear();
            cursorTrack_ = (track >= 0 && track < numTracks) ? track : -1;
            cursorStep_ = (step >= 0) ? step : -1;
        }

        if (e.mods.isRightButtonDown())
        {
            if (track >= 0 && track < numTracks && step >= 0)
            {
                juce::PopupMenu m;
                m.addItem(1, "New Clip (Double-click)");
                m.addItem(2, "Paste Clip (Cmd+V)");
                m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withTargetScreenArea(
                                    juce::Rectangle<int>(e.getScreenX(), e.getScreenY(), 1, 1)),
                                [this, track, step](int res) {
                                    if (res == 1)
                                        listener_.gridCellDoubleClicked(track, step);
                                    else if (res == 2)
                                        listener_.clipPasteRequested(track, step);
                                });
            }
            repaint();
            return;
        }

        startMarquee(e.getPosition(), e.mods);
        if (track >= 0 && track < numTracks && step >= 0)
            listener_.gridCellClicked(track, step);
        else
            listener_.gridCellClicked(-1, -1);
        repaint();
    }
}

void PatternGrid::mouseDrag(const juce::MouseEvent& e)
{
    if (rulerDragMode_ != RulerDragMode::None)
    {
        const auto* pat = song_.findPattern(patternId_);
        const int totalSteps = pat != nullptr ? pat->lengthSteps() : kStepsPerBar;
        const int currentStep = std::clamp(juce::roundToInt(xToStep(e.getPosition().x)), 0, totalSteps);
        const int snapStep = (currentStep / rasterSteps_) * rasterSteps_;

        if (rulerDragMode_ == RulerDragMode::RulerPending)
        {
            const int totalDx = std::abs(e.getScreenX() - rulerDragStartScreenPos_.x);
            const int totalDy = std::abs(e.getScreenY() - rulerDragStartScreenPos_.y);
            if (totalDx < 4 && totalDy < 4)
                return;

            if (totalDy > totalDx)
            {
                rulerDragMode_ = RulerDragMode::ZoomRuler;
                setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
            }
            else
            {
                rulerDragMode_ = RulerDragMode::SeekPlayhead;
                setMouseCursor(juce::MouseCursor::PointingHandCursor);
            }
        }

        if (rulerDragMode_ == RulerDragMode::ZoomRuler)
        {
            // Only zoom horizontally, playhead NEVER moves
            const int totalDy = e.getScreenY() - rulerDragStartScreenPos_.y;
            const float targetZoomX = rulerDragStartZoomX_ * std::exp(static_cast<float>(totalDy) * 0.007f);
            listener_.zoomToLevelRequested(targetZoomX, rulerDragAnchorStep_, rulerDragAnchorViewportX_);
            return;
        }

        if (rulerDragMode_ == RulerDragMode::SeekPlayhead)
        {
            // Only move playhead, NO zoom occurs
            listener_.playheadSeekRequested(snapStep);
            setPlayheadStep(static_cast<double>(snapStep));
            return;
        }

        if (rulerDragMode_ == RulerDragMode::SetRegion)
        {
            loopStartStep_ = std::min(rulerDragRegionAnchorStep_, snapStep);
            loopEndStep_ = std::max(rulerDragRegionAnchorStep_, snapStep);
            if (loopEndStep_ <= loopStartStep_)
                loopEndStep_ = std::min(totalSteps, loopStartStep_ + std::max(rasterSteps_, 2));
        }
        else if (rulerDragMode_ == RulerDragMode::ResizeStart)
        {
            loopStartStep_ = std::clamp(snapStep, 0, loopEndStep_ - 1);
        }
        else if (rulerDragMode_ == RulerDragMode::ResizeEnd)
        {
            loopEndStep_ = std::clamp(snapStep, loopStartStep_ + 1, totalSteps);
        }
        else if (rulerDragMode_ == RulerDragMode::MoveRegion)
        {
            const int delta = snapStep - rulerDragRegionAnchorStep_;
            const int len = rulerDragOriginalEnd_ - rulerDragOriginalStart_;
            const int newStart = std::clamp(rulerDragOriginalStart_ + delta, 0, totalSteps - len);
            loopStartStep_ = newStart;
            loopEndStep_ = newStart + len;
        }
        repaint();
        return;
    }

    if (isMarquee_)
    {
        updateMarquee(e.getPosition());
        return;
    }

    if (!drag_.active)
        return;

    const auto* pat = song_.findPattern(patternId_);
    if (!pat)
        return;

    dragCurrentMousePos_ = e.getPosition();

    if (drag_.mode == DragMode::Move)
    {
        if (e.getDistanceFromDragStart() < 3)
            return;

        const double deltaStepsRaw = xToStep(e.getPosition().x) - xToStep(drag_.startMousePos.x);
        const bool disableSnap = e.mods.isShiftDown();
        const double raster = static_cast<double>(std::max(1, rasterSteps_));

        // Primary clip item
        const auto* primaryItem = drag_.find(drag_.primaryClipId);
        if (!primaryItem && !drag_.items.empty())
            primaryItem = &drag_.items[0];

        double effectiveDelta = 0.0;
        if (primaryItem != nullptr)
        {
            if (disableSnap)
            {
                effectiveDelta = deltaStepsRaw;
            }
            else
            {
                // When dragging without the shift key, the new start snaps to the grid!
                const double targetStart = primaryItem->originalStartStep + deltaStepsRaw;
                const double snappedStart = std::round(targetStart / raster) * raster;
                effectiveDelta = snappedStart - primaryItem->originalStartStep;
            }
        }
        else
        {
            effectiveDelta = disableSnap ? deltaStepsRaw : (std::round(deltaStepsRaw / raster) * raster);
        }

        double minDelta = -100000.0;
        double maxDelta = 100000.0;
        for (const auto& item : drag_.items)
        {
            minDelta = std::max(minDelta, -item.originalStartStep);
            maxDelta = std::min(maxDelta, static_cast<double>(pat->lengthSteps()) - item.originalLengthSteps - item.originalStartStep);
        }
        const double clampedDelta = std::clamp(effectiveDelta, minDelta, maxDelta);

        bool anyChanged = false;
        for (auto& item : drag_.items)
        {
            double newStep = item.originalStartStep + clampedDelta;
            if (!disableSnap)
            {
                newStep = std::clamp(std::round(newStep / raster) * raster, 0.0,
                                     static_cast<double>(pat->lengthSteps()) - item.originalLengthSteps);
            }
            else
            {
                newStep = std::clamp(newStep, 0.0, static_cast<double>(pat->lengthSteps()) - item.originalLengthSteps);
            }

            if (std::abs(newStep - item.currentStartStep) > 0.001)
            {
                item.currentStartStep = newStep;
                anyChanged = true;
            }
        }

        const int numTracks = static_cast<int>(pat->tracks.size());
        const int deltaY = e.getPosition().y - drag_.startMousePos.y;
        const int deltaTracks = juce::roundToInt(static_cast<float>(deltaY) / static_cast<float>(trackHeight_));
        int minDeltaTrack = -1000;
        int maxDeltaTrack = 1000;
        for (const auto& item : drag_.items)
        {
            minDeltaTrack = std::max(minDeltaTrack, -item.originalTrack);
            maxDeltaTrack = std::min(maxDeltaTrack, numTracks - 1 - item.originalTrack);
        }
        const int clampedDeltaTrack = std::clamp(deltaTracks, minDeltaTrack, maxDeltaTrack);
        for (auto& item : drag_.items)
        {
            const int newTrack = item.originalTrack + clampedDeltaTrack;
            if (newTrack != item.currentTrack)
            {
                item.currentTrack = newTrack;
                anyChanged = true;
            }
        }

        if (anyChanged)
        {
            listener_.clipsDragging(drag_.items);
            if (drag_.items.size() > 1)
            {
                dragTooltipText_ = disableSnap
                    ? juce::String::formatted("Move %d clips (Free: %+.2f st)", static_cast<int>(drag_.items.size()), clampedDelta)
                    : juce::String::formatted("Move %d clips -> Step %g", static_cast<int>(drag_.items.size()),
                                              (primaryItem != nullptr ? primaryItem->currentStartStep + 1.0 : 1.0));
            }
            else if (!drag_.items.empty())
            {
                const double stepDisplay = drag_.items[0].currentStartStep + 1.0;
                if (e.mods.isAltDown())
                {
                    dragTooltipText_ = disableSnap
                        ? juce::String::formatted("Duplicate -> Step %.2f (Free)", stepDisplay)
                        : juce::String::formatted("Duplicate -> Step %g", stepDisplay);
                }
                else
                {
                    dragTooltipText_ = disableSnap
                        ? juce::String::formatted("Step %.2f (Free / No Snap)", stepDisplay)
                        : juce::String::formatted("Step %g", stepDisplay);
                }
            }
            repaint();
        }

        if (e.mods.isAltDown())
            setMouseCursor(juce::MouseCursor::CopyingCursor);
        else
            setMouseCursor(juce::MouseCursor::NormalCursor);
    }
    else if (drag_.mode == DragMode::ResizeRight)
    {
        if (e.getDistanceFromDragStart() < 3)
            return;

        const double deltaStepsRaw = xToStep(e.getPosition().x) - xToStep(drag_.startMousePos.x);
        const bool disableSnap = e.mods.isShiftDown();
        const double raster = static_cast<double>(std::max(1, rasterSteps_));

        bool anyChanged = false;
        for (auto& item : drag_.items)
        {
            const double patLen = static_cast<double>(pat->lengthSteps());
            double newLen = item.originalLengthSteps;
            if (disableSnap)
            {
                // Without snapping: make them exactly as small or big as user wants!
                const double maxLen = std::max(0.05, patLen - item.originalStartStep);
                newLen = std::clamp(item.originalLengthSteps + deltaStepsRaw, 0.05, maxLen);
            }
            else
            {
                const double targetEnd = item.originalStartStep + item.originalLengthSteps + deltaStepsRaw;
                const double snappedEnd = std::round(targetEnd / raster) * raster;
                const double maxLen = std::max(raster, patLen - item.originalStartStep);
                newLen = std::clamp(snappedEnd - item.originalStartStep, raster, maxLen);
            }

            if (std::abs(newLen - item.currentLengthSteps) > 0.001)
            {
                item.currentLengthSteps = newLen;
                anyChanged = true;
            }
        }

        if (anyChanged)
        {
            listener_.clipsDragging(drag_.items);
            if (drag_.items.size() > 1)
            {
                dragTooltipText_ = disableSnap
                    ? juce::String::formatted("Resize %d clips (Free: %+.2f st)", static_cast<int>(drag_.items.size()), deltaStepsRaw)
                    : juce::String::formatted("Resize %d clips (%+d st)", static_cast<int>(drag_.items.size()), juce::roundToInt(deltaStepsRaw / raster) * rasterSteps_);
            }
            else if (!drag_.items.empty())
            {
                dragTooltipText_ = disableSnap
                    ? juce::String::formatted("Length: %.2f steps (Free / No Snap)", drag_.items[0].currentLengthSteps)
                    : juce::String::formatted("Length: %g steps", drag_.items[0].currentLengthSteps);
            }
            repaint();
        }
    }
    else if (drag_.mode == DragMode::ResizeLeft)
    {
        if (e.getDistanceFromDragStart() < 3)
            return;

        const double deltaStepsRaw = xToStep(e.getPosition().x) - xToStep(drag_.startMousePos.x);
        const bool disableSnap = e.mods.isShiftDown();
        const double raster = static_cast<double>(std::max(1, rasterSteps_));

        bool anyChanged = false;
        for (auto& item : drag_.items)
        {
            const double originalEndStep = item.originalStartStep + item.originalLengthSteps;
            double newStart = item.originalStartStep;
            if (disableSnap)
            {
                // Without snapping: make them exactly as small or big as user wants!
                newStart = std::clamp(item.originalStartStep + deltaStepsRaw, 0.0, originalEndStep - 0.05);
            }
            else
            {
                const double targetStart = item.originalStartStep + deltaStepsRaw;
                const double snappedStart = std::round(targetStart / raster) * raster;
                newStart = std::clamp(snappedStart, 0.0, originalEndStep - raster);
            }

            const double delta = newStart - item.originalStartStep;
            const double newOffset = std::max(0.0, item.originalOffsetSteps + delta);
            const double newLen = originalEndStep - newStart;

            if (std::abs(newStart - item.currentStartStep) > 0.001 ||
                std::abs(newLen - item.currentLengthSteps) > 0.001 ||
                std::abs(newOffset - item.currentOffsetSteps) > 0.001)
            {
                item.currentStartStep = newStart;
                item.currentLengthSteps = newLen;
                item.currentOffsetSteps = newOffset;
                anyChanged = true;
            }
        }

        if (anyChanged)
        {
            listener_.clipsDragging(drag_.items);
            if (drag_.items.size() > 1)
            {
                dragTooltipText_ = disableSnap
                    ? juce::String::formatted("Resize %d clips (Free: %+.2f st)", static_cast<int>(drag_.items.size()), -deltaStepsRaw)
                    : juce::String::formatted("Resize %d clips (%+d st)", static_cast<int>(drag_.items.size()), -juce::roundToInt(deltaStepsRaw / raster) * rasterSteps_);
            }
            else if (!drag_.items.empty())
            {
                dragTooltipText_ = disableSnap
                    ? juce::String::formatted("Start: %.2f steps (Free / No Snap)", drag_.items[0].currentStartStep + 1.0)
                    : juce::String::formatted("Start: %g steps", drag_.items[0].currentStartStep + 1.0);
            }
            repaint();
        }
    }
    else if (drag_.mode == DragMode::FadeIn)
    {
        const int dx = e.getPosition().x - drag_.startMousePos.x;
        const float deltaSteps = static_cast<float>(dx) / static_cast<float>(stepWidth_);
        for (auto& item : drag_.items)
        {
            const float maxFade = static_cast<float>(item.originalLengthSteps) - item.originalFadeOut;
            item.currentFadeIn = std::clamp(item.originalFadeIn + deltaSteps, 0.0f, std::max(0.0f, maxFade));
        }
        listener_.clipsDragging(drag_.items);
        if (!drag_.items.empty())
            dragTooltipText_ = juce::String::formatted("Fade In: %.2f steps", drag_.items[0].currentFadeIn);
        repaint();
    }
    else if (drag_.mode == DragMode::FadeOut)
    {
        const int dx = drag_.startMousePos.x - e.getPosition().x;
        const float deltaSteps = static_cast<float>(dx) / static_cast<float>(stepWidth_);
        for (auto& item : drag_.items)
        {
            const float maxFade = static_cast<float>(item.originalLengthSteps) - item.originalFadeIn;
            item.currentFadeOut = std::clamp(item.originalFadeOut + deltaSteps, 0.0f, std::max(0.0f, maxFade));
        }
        listener_.clipsDragging(drag_.items);
        if (!drag_.items.empty())
            dragTooltipText_ = juce::String::formatted("Fade Out: %.2f steps", drag_.items[0].currentFadeOut);
        repaint();
    }
    else if (drag_.mode == DragMode::Gain)
    {
        const int dy = drag_.startMousePos.y - e.getPosition().y;
        const float deltaGain = static_cast<float>(dy) / 45.0f;
        for (auto& item : drag_.items)
            item.currentGain = std::clamp(item.originalGain + deltaGain, 0.0f, 2.0f);
        listener_.clipsDragging(drag_.items);
        if (!drag_.items.empty())
        {
            const float gVal = drag_.items[0].currentGain;
            const float dB = (gVal > 0.0001f) ? (20.0f * std::log10(gVal)) : -60.0f;
            dragTooltipText_ = juce::String::formatted("Gain: %+.1f dB (%d%%)", dB, juce::roundToInt(gVal * 100.0f));
        }
        repaint();
    }
}

void PatternGrid::mouseUp(const juce::MouseEvent& e)
{
    if (rulerDragMode_ != RulerDragMode::None)
    {
        const auto mode = rulerDragMode_;
        rulerDragMode_ = RulerDragMode::None;
        setMouseCursor(juce::MouseCursor::NormalCursor);

        if (mode == RulerDragMode::RulerPending)
        {
            listener_.playheadSeekRequested(rulerDragClickedStep_);
            setPlayheadStep(static_cast<double>(rulerDragClickedStep_));
        }
        else if (mode != RulerDragMode::SeekPlayhead && mode != RulerDragMode::ZoomRuler)
        {
            listener_.loopRegionChanged(loopStartStep_, loopEndStep_);
        }
        repaint();
        return;
    }

    if (isMarquee_)
    {
        finishMarquee();
        return;
    }

    if (!drag_.active)
        return;

    listener_.clipsDragEnded();

    const bool didDrag = (e.getDistanceFromDragStart() >= 3);
    const bool isShiftOrCmd = e.mods.isShiftDown() || e.mods.isCommandDown();

    if (!didDrag)
    {
        if (isShiftOrCmd)
        {
            if (primaryClipWasSelectedOnDown_)
                deselectClip(drag_.primaryClipId);
        }
        else
        {
            if (selectedClips_.size() > 1 && drag_.primaryClipId.isValid())
            {
                selectedClips_.clear();
                selectedClips_.push_back(drag_.primaryClipId);
            }
        }
    }
    else
    {
        if (drag_.mode == DragMode::Move)
        {
            if (e.mods.isAltDown())
            {
                for (const auto& item : drag_.items)
                    listener_.clipDuplicatedTo(item.clipId, item.originalTrack, item.currentStartStep);
            }
            else
            {
                std::vector<ClipMoveInfo> moves;
                for (const auto& item : drag_.items)
                {
                    if (std::abs(item.currentStartStep - item.originalStartStep) > 0.001 || item.currentTrack != item.originalTrack)
                        moves.push_back({item.clipId, item.currentTrack, item.currentStartStep});
                }
                if (moves.size() == 1)
                    listener_.clipMoved(moves[0].clipId, moves[0].newTrack, moves[0].newStep);
                else if (moves.size() > 1)
                    listener_.clipsMoved(moves);
            }
        }
        else if (drag_.mode == DragMode::ResizeLeft || drag_.mode == DragMode::ResizeRight)
        {
            std::vector<ClipResizeInfo> resizes;
            for (const auto& item : drag_.items)
            {
                if (std::abs(item.currentStartStep - item.originalStartStep) > 0.001 ||
                    std::abs(item.currentLengthSteps - item.originalLengthSteps) > 0.001 ||
                    std::abs(item.currentOffsetSteps - item.originalOffsetSteps) > 0.001)
                {
                    resizes.push_back({item.clipId, item.currentStartStep, item.currentLengthSteps, item.currentOffsetSteps});
                }
            }
            if (resizes.size() == 1)
                listener_.clipResized(resizes[0].clipId, resizes[0].newStartStep, resizes[0].newLengthSteps, resizes[0].newOffsetSteps);
            else if (resizes.size() > 1)
                listener_.clipsResized(resizes);
        }
        else if (drag_.mode == DragMode::FadeIn || drag_.mode == DragMode::FadeOut || drag_.mode == DragMode::Gain)
        {
            for (const auto& item : drag_.items)
            {
                if (std::abs(item.currentGain - item.originalGain) > 0.005f ||
                    std::abs(item.currentFadeIn - item.originalFadeIn) > 0.01f ||
                    std::abs(item.currentFadeOut - item.originalFadeOut) > 0.01f)
                {
                    listener_.clipEnvelopeChanged(item.clipId, item.currentGain, item.currentFadeIn, item.currentFadeOut);
                }
            }
        }
    }

    drag_.active = false;
    drag_.mode = DragMode::None;
    dragTooltipText_ = {};
    mouseMove(e);
    repaint();
}

void PatternGrid::mouseDoubleClick(const juce::MouseEvent& e)
{
    if (e.getPosition().y < kRulerHeight)
    {
        const auto* pat = song_.findPattern(patternId_);
        const int totalSteps = pat != nullptr ? pat->lengthSteps() : kStepsPerBar;
        loopStartStep_ = 0;
        loopEndStep_ = totalSteps;
        listener_.loopRegionChanged(loopStartStep_, loopEndStep_);
        repaint();
        return;
    }

    if (const Clip* clip = findClipAt(e.getPosition()))
    {
        const auto mode = hitTestResize(e.getPosition());
        if (mode == DragMode::Gain)
        {
            listener_.clipEnvelopeChanged(clip->id, 1.0f, clip->fadeInSteps, clip->fadeOutSteps);
            repaint();
            return;
        }
        if (mode == DragMode::FadeIn)
        {
            listener_.clipEnvelopeChanged(clip->id, clip->gain, 0.0f, clip->fadeOutSteps);
            repaint();
            return;
        }
        if (mode == DragMode::FadeOut)
        {
            listener_.clipEnvelopeChanged(clip->id, clip->gain, clip->fadeInSteps, 0.0f);
            repaint();
            return;
        }
        return;
    }

    const auto [track, step] = pointToTrackAndStep(e.getPosition());
    const auto* pat = song_.findPattern(patternId_);
    if (!pat)
        return;
    const int numTracks = static_cast<int>(pat->tracks.size());
    if (track < 0 || track >= numTracks || step < 0)
        return;

    cursorTrack_ = -1;
    cursorStep_ = -1;
    listener_.gridCellDoubleClicked(track, step);
}

void PatternGrid::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (e.mods.isCommandDown() || e.mods.isCtrlDown())
    {
        const float delta = (wheel.deltaY != 0.0f) ? wheel.deltaY : wheel.deltaX;
        if (std::abs(delta) > 0.0001f)
        {
            const float factor = (delta > 0.0f) ? 1.15f : 0.87f;
            if (e.mods.isShiftDown() || e.mods.isAltDown())
                listener_.zoomRequested(1.0f, factor, e.getPosition());
            else
                listener_.zoomRequested(factor, 1.0f, e.getPosition());
            return;
        }
    }

    juce::Component::mouseWheelMove(e, wheel);
}

void PatternGrid::mouseMagnify(const juce::MouseEvent& e, float scaleFactor)
{
    if (std::abs(scaleFactor - 1.0f) > 0.001f)
    {
        listener_.zoomRequested(scaleFactor, 1.0f, e.getPosition());
    }
}

bool PatternGrid::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (const auto& f : files)
    {
        if (f.endsWithIgnoreCase(".wav") || f.endsWithIgnoreCase(".mp3"))
            return true;
    }
    return false;
}

void PatternGrid::fileDragEnter(const juce::StringArray& files, int x, int y)
{
    juce::ignoreUnused(files);
    const auto [track, step] = pointToTrackAndStep({x, y});
    dragHoverTrack_ = track;
    dragHoverStep_ = step;
    repaint();
}

void PatternGrid::fileDragMove(const juce::StringArray& files, int x, int y)
{
    juce::ignoreUnused(files);
    const auto [track, step] = pointToTrackAndStep({x, y});
    if (track != dragHoverTrack_ || step != dragHoverStep_)
    {
        dragHoverTrack_ = track;
        dragHoverStep_ = step;
        repaint();
    }
}

void PatternGrid::fileDragExit(const juce::StringArray& /*files*/)
{
    dragHoverTrack_ = -1;
    dragHoverStep_ = -1;
    repaint();
}

void PatternGrid::filesDropped(const juce::StringArray& files, int x, int y)
{
    dragHoverTrack_ = -1;
    dragHoverStep_ = -1;
    repaint();
    if (files.isEmpty())
        return;

    const auto [track, step] = pointToTrackAndStep({x, y});
    const auto* pat = song_.findPattern(patternId_);
    const int numTracks = pat != nullptr ? static_cast<int>(pat->tracks.size()) : 0;
    const int targetTrack = (track >= 0 && track < numTracks) ? track : -1;

    listener_.fileDroppedOnGrid(files[0], targetTrack, std::max(0, step));
}

bool PatternGrid::isInterestedInDragSource(const SourceDetails& dragSourceDetails)
{
    if (dragSourceDetails.description.isString())
    {
        const juce::String s = dragSourceDetails.description.toString();
        return s.endsWithIgnoreCase(".wav") || s.endsWithIgnoreCase(".mp3");
    }
    return false;
}

void PatternGrid::itemDragEnter(const SourceDetails& dragSourceDetails)
{
    const auto [track, step] = pointToTrackAndStep(dragSourceDetails.localPosition);
    dragHoverTrack_ = track;
    dragHoverStep_ = step;
    repaint();
}

void PatternGrid::itemDragMove(const SourceDetails& dragSourceDetails)
{
    const auto [track, step] = pointToTrackAndStep(dragSourceDetails.localPosition);
    if (track != dragHoverTrack_ || step != dragHoverStep_)
    {
        dragHoverTrack_ = track;
        dragHoverStep_ = step;
        repaint();
    }
}

void PatternGrid::itemDragExit(const SourceDetails& /*dragSourceDetails*/)
{
    dragHoverTrack_ = -1;
    dragHoverStep_ = -1;
    repaint();
}

void PatternGrid::itemDropped(const SourceDetails& dragSourceDetails)
{
    dragHoverTrack_ = -1;
    dragHoverStep_ = -1;
    repaint();
    if (!dragSourceDetails.description.isString())
        return;

    const juce::String filePath = dragSourceDetails.description.toString();
    const auto [track, step] = pointToTrackAndStep(dragSourceDetails.localPosition);
    const auto* pat = song_.findPattern(patternId_);
    const int numTracks = pat != nullptr ? static_cast<int>(pat->tracks.size()) : 0;
    const int targetTrack = (track >= 0 && track < numTracks) ? track : -1;

    listener_.fileDroppedOnGrid(filePath, targetTrack, std::max(0, step));
}

bool PatternGrid::keyPressed(const juce::KeyPress& key)
{
    if (key.getKeyCode() == juce::KeyPress::escapeKey)
    {
        selectedClips_.clear();
        cursorTrack_ = -1;
        cursorStep_ = -1;
        repaint();
        return true;
    }

    if (key.getKeyCode() == juce::KeyPress::deleteKey || key.getKeyCode() == juce::KeyPress::backspaceKey)
    {
        if (!selectedClips_.empty())
        {
            const auto toDelete = selectedClips_;
            selectedClips_.clear();
            listener_.clipsDeleteRequested(toDelete);
            return true;
        }
    }

    const auto* pat = song_.findPattern(patternId_);
    const int totalSteps = pat != nullptr ? pat->lengthSteps() : kStepsPerBar;
    const int numTracks = pat != nullptr ? static_cast<int>(pat->tracks.size()) : 0;

    // Arrow keys navigate grid cursor
    if (cursorTrack_ >= 0 && cursorStep_ >= 0 && selectedClips_.empty())
    {
        if (key.getKeyCode() == juce::KeyPress::leftKey)
        {
            cursorStep_ = std::max(0, cursorStep_ - rasterSteps_);
            listener_.gridCellClicked(cursorTrack_, cursorStep_);
            repaint();
            return true;
        }
        if (key.getKeyCode() == juce::KeyPress::rightKey)
        {
            cursorStep_ = std::min(std::max(0, totalSteps - rasterSteps_), cursorStep_ + rasterSteps_);
            listener_.gridCellClicked(cursorTrack_, cursorStep_);
            repaint();
            return true;
        }
        if (key.getKeyCode() == juce::KeyPress::upKey)
        {
            cursorTrack_ = std::max(0, cursorTrack_ - 1);
            listener_.gridCellClicked(cursorTrack_, cursorStep_);
            repaint();
            return true;
        }
        if (key.getKeyCode() == juce::KeyPress::downKey)
        {
            cursorTrack_ = std::min(std::max(0, numTracks - 1), cursorTrack_ + 1);
            listener_.gridCellClicked(cursorTrack_, cursorStep_);
            repaint();
            return true;
        }
        if (key.getKeyCode() == juce::KeyPress::returnKey)
        {
            listener_.gridCellDoubleClicked(cursorTrack_, cursorStep_);
            return true;
        }
    }

    const auto mods = key.getModifiers();
    const bool isCmdOrCtrl = mods.isCommandDown() || mods.isCtrlDown();

    // Arrow keys or Tab navigate between clips when clips are selected
    if (!selectedClips_.empty() && !isCmdOrCtrl && pat != nullptr)
    {
        const ClipId currentId = selectedClips_.back();
        int curTrack = -1;
        const Clip* curClip = nullptr;
        for (int t = 0; t < numTracks; ++t)
        {
            for (const auto& c : pat->tracks[static_cast<std::size_t>(t)].clips)
            {
                if (c.id == currentId)
                {
                    curTrack = t;
                    curClip = &c;
                    break;
                }
            }
            if (curTrack >= 0) break;
        }

        if (curClip != nullptr)
        {
            if (key.getKeyCode() == juce::KeyPress::tabKey)
            {
                std::vector<ClipId> allClips;
                for (const auto& tr : pat->tracks)
                    for (const auto& c : tr.clips)
                        allClips.push_back(c.id);

                if (!allClips.empty())
                {
                    auto it = std::find(allClips.begin(), allClips.end(), currentId);
                    int nextIdx = 0;
                    if (it != allClips.end())
                    {
                        const int idx = static_cast<int>(std::distance(allClips.begin(), it));
                        if (mods.isShiftDown())
                            nextIdx = (idx - 1 + static_cast<int>(allClips.size())) % static_cast<int>(allClips.size());
                        else
                            nextIdx = (idx + 1) % static_cast<int>(allClips.size());
                    }
                    setSelectedClip(allClips[static_cast<std::size_t>(nextIdx)]);
                    listener_.clipClicked(allClips[static_cast<std::size_t>(nextIdx)]);
                    return true;
                }
            }

            if (key.getKeyCode() == juce::KeyPress::leftKey)
            {
                const auto& tr = pat->tracks[static_cast<std::size_t>(curTrack)];
                const Clip* prevClip = nullptr;
                for (const auto& c : tr.clips)
                {
                    if (c.startStep < curClip->startStep)
                    {
                        if (prevClip == nullptr || c.startStep > prevClip->startStep)
                            prevClip = &c;
                    }
                }
                if (prevClip != nullptr)
                {
                    setSelectedClip(prevClip->id);
                    listener_.clipClicked(prevClip->id);
                    return true;
                }
            }

            if (key.getKeyCode() == juce::KeyPress::rightKey)
            {
                const auto& tr = pat->tracks[static_cast<std::size_t>(curTrack)];
                const Clip* nextClip = nullptr;
                for (const auto& c : tr.clips)
                {
                    if (c.startStep > curClip->startStep)
                    {
                        if (nextClip == nullptr || c.startStep < nextClip->startStep)
                            nextClip = &c;
                    }
                }
                if (nextClip != nullptr)
                {
                    setSelectedClip(nextClip->id);
                    listener_.clipClicked(nextClip->id);
                    return true;
                }
            }

            if (key.getKeyCode() == juce::KeyPress::upKey && curTrack > 0)
            {
                const auto& upperTr = pat->tracks[static_cast<std::size_t>(curTrack - 1)];
                const Clip* best = nullptr;
                int bestDist = 1000000;
                for (const auto& c : upperTr.clips)
                {
                    const int dist = std::abs(c.startStep - curClip->startStep);
                    if (dist < bestDist)
                    {
                        bestDist = dist;
                        best = &c;
                    }
                }
                if (best != nullptr)
                {
                    setSelectedClip(best->id);
                    listener_.clipClicked(best->id);
                    return true;
                }
            }

            if (key.getKeyCode() == juce::KeyPress::downKey && curTrack < numTracks - 1)
            {
                const auto& lowerTr = pat->tracks[static_cast<std::size_t>(curTrack + 1)];
                const Clip* best = nullptr;
                int bestDist = 1000000;
                for (const auto& c : lowerTr.clips)
                {
                    const int dist = std::abs(c.startStep - curClip->startStep);
                    if (dist < bestDist)
                    {
                        bestDist = dist;
                        best = &c;
                    }
                }
                if (best != nullptr)
                {
                    setSelectedClip(best->id);
                    listener_.clipClicked(best->id);
                    return true;
                }
            }
        }
    }

    if (selectedClips_.empty() && key.getKeyCode() == juce::KeyPress::tabKey && pat != nullptr)
    {
        for (const auto& tr : pat->tracks)
        {
            if (!tr.clips.empty())
            {
                setSelectedClip(tr.clips.front().id);
                listener_.clipClicked(tr.clips.front().id);
                return true;
            }
        }
    }

    if (!selectedClips_.empty() && (key.isKeyCode('R') || key.getKeyCode() == 'r' || key.getKeyCode() == 'R'))
    {
        if (selectedClips_.size() == 1)
            listener_.clipReverseRequested(selectedClips_.front());
        else
            listener_.clipsReverseRequested(selectedClips_);
        return true;
    }

    const bool isPlus = !isCmdOrCtrl && (key.getTextCharacter() == '+' ||
                                         key.getKeyCode() == juce::KeyPress::numberPadAdd ||
                                         key.isKeyCode('+') ||
                                         (key.getKeyCode() == '=' && !mods.isAltDown()));
    const bool isMinus = !isCmdOrCtrl && (key.getTextCharacter() == '-' ||
                                          key.getKeyCode() == juce::KeyPress::numberPadSubtract ||
                                          key.isKeyCode('-') ||
                                          key.getKeyCode() == '-');

    if (!selectedClips_.empty() && (isPlus || isMinus))
    {
        listener_.clipsTransposeDelta(selectedClips_, isPlus ? 1 : -1);
        return true;
    }

    if (isCmdOrCtrl)
    {
        if (key.isKeyCode('A'))
        {
            selectedClips_.clear();
            if (pat != nullptr)
            {
                for (const auto& track : pat->tracks)
                    for (const auto& c : track.clips)
                        selectedClips_.push_back(c.id);
            }
            cursorTrack_ = -1;
            cursorStep_ = -1;
            repaint();
            return true;
        }

        if (!selectedClips_.empty())
        {
            if (key.isKeyCode('C'))
            {
                listener_.clipCopyRequested(selectedClips_.back());
                return true;
            }
            if (key.isKeyCode('X'))
            {
                if (selectedClips_.size() == 1)
                {
                    listener_.clipCutRequested(selectedClips_.front());
                }
                else
                {
                    listener_.clipCopyRequested(selectedClips_.back());
                    const auto toDelete = selectedClips_;
                    selectedClips_.clear();
                    listener_.clipsDeleteRequested(toDelete);
                }
                return true;
            }
            if (key.isKeyCode('D'))
            {
                for (auto cid : selectedClips_)
                    listener_.clipDuplicateRequested(cid);
                return true;
            }
        }

        if (key.isKeyCode('V'))
        {
            listener_.clipPasteRequested(cursorTrack_, cursorStep_);
            return true;
        }
    }

    return false;
}

} // namespace trami
