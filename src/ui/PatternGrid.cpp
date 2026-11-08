#include "ui/PatternGrid.h"

#include "model/Timing.h"
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
}

void PatternGrid::setPattern(PatternId patternId)
{
    patternId_ = patternId;
    selectedClip_ = {};
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
    selectedClip_ = clipId;
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

int PatternGrid::clipSpan(const Clip& clip, int trackIndex) const
{
    const Sample* sample = song_.findSample(clip.sampleId);
    if (sample == nullptr)
        return rasterSteps_;

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

    if (clip.lengthSteps > 0)
        return clip.lengthSteps;

    if (kind == SampleKind::Loop)
    {
        const int bars = std::max(1, sample->loopBars);
        return bars * kStepsPerBar;
    }

    const double stepSec = 60.0 / song_.bpm() / kStepsPerBeat;
    const int naturalSteps = static_cast<int>(std::round(sample->durationSeconds / stepSec));
    return std::max(rasterSteps_, naturalSteps);
}

juce::Rectangle<int> PatternGrid::clipBounds(int track, int step, int lengthSteps) const
{
    return {step * stepWidth_, kRulerHeight + track * trackHeight_, std::max(1, lengthSteps) * stepWidth_,
            trackHeight_};
}

std::pair<int, int> PatternGrid::pointToTrackAndStep(juce::Point<int> pt) const
{
    const int track = (pt.y - kRulerHeight) / trackHeight_;
    const int rawStep = pt.x / stepWidth_;
    const int snappedStep = (rawStep / rasterSteps_) * rasterSteps_;
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
    for (auto it = track.clips.rbegin(); it != track.clips.rend(); ++it)
    {
        if (it->startStep >= patSteps)
            continue;
        const int span = std::min(clipSpan(*it, trackIdx), patSteps - it->startStep);
        auto b = clipBounds(trackIdx, it->startStep, span).reduced(2, 3);
        if (b.contains(pt))
            return &(*it);
    }

    return nullptr;
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
        const int span = std::min(clipSpan(*it, trackIdx), patSteps - it->startStep);
        auto b = clipBounds(trackIdx, it->startStep, span).reduced(2, 3);
        if (!b.contains(pt))
            continue;

        constexpr int kEdgeMargin = 8;
        if (pt.x <= b.getX() + kEdgeMargin)
            return DragMode::ResizeLeft;
        if (pt.x >= b.getRight() - kEdgeMargin)
            return DragMode::ResizeRight;
        return DragMode::Move;
    }

    return DragMode::None;
}

void PatternGrid::mouseMove(const juce::MouseEvent& e)
{
    if (drag_.active)
        return;

    if (e.getPosition().y < kRulerHeight)
    {
        const int startX = loopStartStep_ * stepWidth_;
        const int endX = loopEndStep_ * stepWidth_;
        if (std::abs(e.getPosition().x - startX) <= 6 || std::abs(e.getPosition().x - endX) <= 6)
            setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
        else if (e.getPosition().x > startX && e.getPosition().x < endX)
            setMouseCursor(juce::MouseCursor::DraggingHandCursor);
        else
            setMouseCursor(juce::MouseCursor::NormalCursor);
        return;
    }

    const auto mode = hitTestResize(e.getPosition());
    if (mode == DragMode::ResizeLeft || mode == DragMode::ResizeRight)
        setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
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

    const int gridWidth = totalSteps * stepWidth_;
    const int renderWidth = std::max(getWidth(), gridWidth);
    const int totalHeight = kRulerHeight + std::max(1, numTracks) * trackHeight_;

    // 1. Ruler
    g.setColour(Palette::panelHeader);
    g.fillRect(0, 0, renderWidth, kRulerHeight);
    g.setColour(Palette::gridLine);
    g.drawHorizontalLine(kRulerHeight - 1, 0.0f, static_cast<float>(renderWidth));
    g.setColour(Palette::textDim);
    g.setFont(Typography::bold(12.0f));

    for (int step = 0; step < totalSteps; ++step)
    {
        const int x = step * stepWidth_;
        const bool isBar = (step % kStepsPerBar == 0);
        const bool isBeat = (step % kStepsPerBeat == 0);
        const bool isRaster = (step % rasterSteps_ == 0);

        if (isBar)
        {
            const int barNum = step / kStepsPerBar + 1;
            g.setColour(Palette::text);
            const int labelW = std::max(stepWidth_ * 2, stepWidth_ * kStepsPerBar);
            g.drawText(juce::String(barNum) + ".1", x + 4, 2, labelW, kRulerHeight - 4,
                       juce::Justification::centredLeft);
            g.setColour(Palette::gridBarLine);
            g.drawVerticalLine(x, 0.0f, static_cast<float>(kRulerHeight));
        }
        else if (isBeat)
        {
            g.setColour(Palette::gridBeatLine);
            g.drawVerticalLine(x, static_cast<float>(kRulerHeight - 10), static_cast<float>(kRulerHeight));
        }
        else if (isRaster && stepWidth_ >= 6)
        {
            g.setColour(Palette::gridLine);
            g.drawVerticalLine(x, static_cast<float>(kRulerHeight - 5), static_cast<float>(kRulerHeight));
        }
    }

    // 2. Loop region highlight (down tracks and banner in ruler)
    const int loopStartX = loopStartStep_ * stepWidth_;
    const int loopEndX = loopEndStep_ * stepWidth_;
    const int loopW = std::max(4, loopEndX - loopStartX);

    // Shaded loop region down the tracks
    g.setColour(Palette::loopRegion.withAlpha(0.07f));
    g.fillRect(loopStartX, kRulerHeight, loopW, totalHeight - kRulerHeight);
    g.setColour(Palette::loopRegion.withAlpha(0.45f));
    g.drawVerticalLine(loopStartX, static_cast<float>(kRulerHeight), static_cast<float>(totalHeight));
    g.drawVerticalLine(loopEndX, static_cast<float>(kRulerHeight), static_cast<float>(totalHeight));

    // Loop banner in ruler
    const auto loopBanner = juce::Rectangle<float>(static_cast<float>(loopStartX), 1.0f,
                                                   static_cast<float>(loopW), static_cast<float>(kRulerHeight - 3));
    g.setColour(Palette::loopRegion.withAlpha(0.28f));
    g.fillRoundedRectangle(loopBanner, 3.0f);
    g.setColour(Palette::loopRegion);
    g.drawRoundedRectangle(loopBanner, 3.0f, 1.5f);
    g.fillRect(static_cast<float>(loopStartX), 1.0f, 3.0f, static_cast<float>(kRulerHeight - 3));
    g.fillRect(static_cast<float>(loopEndX - 3), 1.0f, 3.0f, static_cast<float>(kRulerHeight - 3));

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
        const int x = step * stepWidth_;
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
            int start = 0;
            int end = 0;
        };

        std::vector<ClipInterval> intervals;
        for (const auto& clip : track.clips)
        {
            if (clip.startStep >= patSteps)
                continue;

            int start = clip.startStep;
            int span = std::min(clipSpan(clip, t), patSteps - start);
            if (drag_.active && drag_.clipId == clip.id)
            {
                if (drag_.currentTrack != t)
                    continue;
                start = drag_.currentStartStep;
                span = std::min(drag_.currentLengthSteps, std::max(0, patSteps - start));
            }
            if (span > 0)
                intervals.push_back({clip.id, start, start + span});
        }

        if (drag_.active && drag_.currentTrack == t && drag_.originalTrack != t)
        {
            const int start = drag_.currentStartStep;
            const int span = std::min(drag_.currentLengthSteps, std::max(0, patSteps - start));
            if (span > 0)
                intervals.push_back({drag_.clipId, start, start + span});
        }

        // Detect overlaps
        std::vector<std::pair<int, int>> overlapRanges;
        std::unordered_set<std::uint32_t> overlappingClipIds;
        for (std::size_t i = 0; i < intervals.size(); ++i)
        {
            for (std::size_t j = i + 1; j < intervals.size(); ++j)
            {
                const int oStart = std::max(intervals[i].start, intervals[j].start);
                const int oEnd = std::min(intervals[i].end, intervals[j].end);
                if (oStart < oEnd)
                {
                    overlappingClipIds.insert(intervals[i].id.value);
                    overlappingClipIds.insert(intervals[j].id.value);
                    overlapRanges.push_back({oStart, oEnd});
                }
            }
        }

        // Render each clip
        for (const auto& clip : track.clips)
        {
            int renderTrack = t;
            int renderStep = clip.startStep;
            int span = clipSpan(clip, renderTrack);
            int renderOffset = clip.offsetSteps;

            if (drag_.active && drag_.clipId == clip.id)
            {
                renderTrack = drag_.currentTrack;
                renderStep = drag_.currentStartStep;
                span = drag_.currentLengthSteps;
                renderOffset = drag_.currentOffsetSteps;
                if (renderTrack != t)
                    continue;
            }

            if (renderStep >= patSteps)
                continue;

            // Cut off clip cleanly at the pattern loop point
            span = std::min(span, patSteps - renderStep);
            if (span <= 0)
                continue;

            const SampleKind trackKind = song_.patternTrackKind(patternId_, renderTrack);
            const bool isLoop = (trackKind == SampleKind::Loop);

            auto bounds = clipBounds(renderTrack, renderStep, span).reduced(2, 3);
            const juce::Colour col = sampleColour(clip.sampleId);

            // Container
            g.setColour(juce::Colour(0xff1a1e26));
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
                int fullSampleSteps = span;
                if (sample != nullptr)
                {
                    if (isLoop)
                    {
                        const int loopBars = std::max(1, sample->loopBars);
                        fullSampleSteps = loopBars * kStepsPerBar;
                    }
                    else
                    {
                        const double stepSec = 60.0 / song_.bpm() / kStepsPerBeat;
                        fullSampleSteps =
                            std::max(1, static_cast<int>(std::round(sample->durationSeconds / stepSec)));
                    }
                }

                const std::int64_t totalFrames = rawAudio->numFrames();
                const std::int64_t startFrame =
                    (fullSampleSteps > 0)
                        ? std::clamp<std::int64_t>(
                              static_cast<std::int64_t>(renderOffset) * totalFrames / fullSampleSteps, 0,
                              totalFrames)
                        : 0;
                const std::int64_t clipFrames =
                    (fullSampleSteps > 0)
                        ? std::clamp<std::int64_t>(
                              static_cast<std::int64_t>(span) * totalFrames / fullSampleSteps, 0,
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
                        const std::int64_t f0 =
                            startFrame + (static_cast<std::int64_t>(px) * clipFrames) / w;
                        const std::int64_t f1 = std::min<std::int64_t>(
                            totalFrames,
                            startFrame + (static_cast<std::int64_t>(px + 1) * clipFrames) / w);

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

            // Kind badge: [L] or [1]
            if (bounds.getWidth() >= 20 && trackHeight_ >= 18)
            {
                const float fontSize = std::clamp(static_cast<float>(trackHeight_) * 0.3f, 8.0f, 11.0f);
                g.setFont(juce::FontOptions(fontSize).withStyle("Bold"));
                if (isLoop)
                {
                    g.setColour(juce::Colour(0xffffca28));
                    g.drawText("[L]", bounds.reduced(6, 3), juce::Justification::topRight, false);
                }
                else
                {
                    g.setColour(Palette::textDim.withAlpha(0.65f));
                    g.drawText("[1]", bounds.reduced(6, 3), juce::Justification::topRight, false);
                }
            }

            // Selection / Overlap border
            const bool isSelected = (clip.id == selectedClip_);
            const bool isOverlapping = overlappingClipIds.contains(clip.id.value);

            if (isSelected)
            {
                g.setColour(juce::Colours::white);
                g.drawRoundedRectangle(bounds.toFloat(), 4.0f, 2.0f);
            }
            else if (isOverlapping)
            {
                g.setColour(juce::Colour(0xffff4433));
                g.drawRoundedRectangle(bounds.toFloat(), 4.0f, 2.0f);
            }
            else
            {
                g.setColour(col.withAlpha(0.6f));
                g.drawRoundedRectangle(bounds.toFloat(), 4.0f, 1.0f);
            }
        }
    }
    }

    // 4. Playhead
    if (playheadStep_ >= 0.0)
    {
        const int px = juce::roundToInt(static_cast<float>(playheadStep_ * static_cast<double>(stepWidth_)));
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
                const int hx = dragHoverStep_ * stepWidth_;
                const int hw = std::max(stepWidth_ * rasterSteps_, stepWidth_);
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
}

void PatternGrid::mouseDown(const juce::MouseEvent& e)
{
    if (e.mods.isRightButtonDown())
    {
        if (const Clip* clip = findClipAt(e.getPosition()))
            listener_.clipDeleteRequested(clip->id);
        return;
    }

    const auto* pat = song_.findPattern(patternId_);
    const int totalSteps = pat != nullptr ? pat->lengthSteps() : kStepsPerBar;

    if (e.getPosition().y < kRulerHeight)
    {
        const int clickStep = std::clamp(e.getPosition().x / stepWidth_, 0, totalSteps);
        const int snapStep = (clickStep / rasterSteps_) * rasterSteps_;

        const int startX = loopStartStep_ * stepWidth_;
        const int endX = loopEndStep_ * stepWidth_;

        if (std::abs(e.getPosition().x - startX) <= 8)
        {
            rulerDragMode_ = RulerDragMode::ResizeStart;
        }
        else if (std::abs(e.getPosition().x - endX) <= 8)
        {
            rulerDragMode_ = RulerDragMode::ResizeEnd;
        }
        else if (e.getPosition().x >= startX && e.getPosition().x <= endX)
        {
            rulerDragMode_ = RulerDragMode::MoveRegion;
            rulerDragAnchorStep_ = snapStep;
            rulerDragOriginalStart_ = loopStartStep_;
            rulerDragOriginalEnd_ = loopEndStep_;
        }
        else
        {
            rulerDragMode_ = RulerDragMode::SetRegion;
            rulerDragAnchorStep_ = snapStep;
            loopStartStep_ = snapStep;
            loopEndStep_ = std::min(totalSteps, snapStep + std::max(rasterSteps_, 2));
        }
        repaint();
        return;
    }

    const auto [track, step] = pointToTrackAndStep(e.getPosition());
    if (!pat)
        return;
    if (!pat)
        return;
    const int numTracks = static_cast<int>(pat->tracks.size());

    if (track < 0 || track >= numTracks || step < 0)
        return;

    if (const Clip* clip = findClipAt(e.getPosition()))
    {
        selectedClip_ = clip->id;
        const auto mode = hitTestResize(e.getPosition());
        const int span = clipSpan(*clip, track);
        drag_ = {clip->id, track, clip->startStep, span, clip->offsetSteps,
                 track,    clip->startStep, span, clip->offsetSteps, mode, true};
        listener_.clipClicked(clip->id);
        if (mode == DragMode::ResizeLeft || mode == DragMode::ResizeRight)
            setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
        repaint();
    }
    else
    {
        selectedClip_ = {};
        listener_.gridCellClicked(track, step);
        repaint();
    }
}

void PatternGrid::mouseDrag(const juce::MouseEvent& e)
{
    if (rulerDragMode_ != RulerDragMode::None)
    {
        const auto* pat = song_.findPattern(patternId_);
        const int totalSteps = pat != nullptr ? pat->lengthSteps() : kStepsPerBar;
        const int currentStep = std::clamp(e.getPosition().x / stepWidth_, 0, totalSteps);
        const int snapStep = (currentStep / rasterSteps_) * rasterSteps_;

        if (rulerDragMode_ == RulerDragMode::SetRegion)
        {
            loopStartStep_ = std::min(rulerDragAnchorStep_, snapStep);
            loopEndStep_ = std::max(rulerDragAnchorStep_, snapStep);
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
            const int delta = snapStep - rulerDragAnchorStep_;
            const int len = rulerDragOriginalEnd_ - rulerDragOriginalStart_;
            const int newStart = std::clamp(rulerDragOriginalStart_ + delta, 0, totalSteps - len);
            loopStartStep_ = newStart;
            loopEndStep_ = newStart + len;
        }
        repaint();
        return;
    }

    if (!drag_.active)
        return;

    const auto [track, step] = pointToTrackAndStep(e.getPosition());
    const auto* pat = song_.findPattern(patternId_);
    if (!pat)
        return;
    const int numTracks = static_cast<int>(pat->tracks.size());
    const int clampedTrack = std::clamp(track, 0, std::max(0, numTracks - 1));
    const int clampedStep = std::max(0, step);

    if (drag_.mode == DragMode::Move)
    {
        const int maxStart = std::max(0, pat->lengthSteps() - drag_.currentLengthSteps);
        const int validStep = std::clamp(clampedStep, 0, maxStart);
        if (clampedTrack != drag_.currentTrack || validStep != drag_.currentStartStep)
        {
            drag_.currentTrack = clampedTrack;
            drag_.currentStartStep = validStep;
            repaint();
        }
    }
    else if (drag_.mode == DragMode::ResizeRight)
    {
        const int maxLen = std::max(rasterSteps_, pat->lengthSteps() - drag_.originalStartStep);
        const int rawLen = clampedStep - drag_.originalStartStep + rasterSteps_;
        const int newLen = std::clamp(std::max(rasterSteps_, (rawLen / rasterSteps_) * rasterSteps_), rasterSteps_, maxLen);
        if (newLen != drag_.currentLengthSteps)
        {
            drag_.currentLengthSteps = newLen;
            repaint();
        }
    }
    else if (drag_.mode == DragMode::ResizeLeft)
    {
        const int originalEndStep = drag_.originalStartStep + drag_.originalLengthSteps;
        const int newStart = std::clamp((clampedStep / rasterSteps_) * rasterSteps_, 0, originalEndStep - rasterSteps_);
        const int delta = newStart - drag_.originalStartStep;
        const int newOffset = std::max(0, drag_.originalOffsetSteps + delta);
        const int newLen = originalEndStep - newStart;

        if (newStart != drag_.currentStartStep || newLen != drag_.currentLengthSteps ||
            newOffset != drag_.currentOffsetSteps)
        {
            drag_.currentStartStep = newStart;
            drag_.currentLengthSteps = newLen;
            drag_.currentOffsetSteps = newOffset;
            repaint();
        }
    }
}

void PatternGrid::mouseUp(const juce::MouseEvent& e)
{
    if (rulerDragMode_ != RulerDragMode::None)
    {
        rulerDragMode_ = RulerDragMode::None;
        listener_.loopRegionChanged(loopStartStep_, loopEndStep_);
        repaint();
        return;
    }

    if (!drag_.active)
        return;

    if (drag_.mode == DragMode::Move)
    {
        if (drag_.currentTrack != drag_.originalTrack || drag_.currentStartStep != drag_.originalStartStep)
            listener_.clipMoved(drag_.clipId, drag_.currentTrack, drag_.currentStartStep);
    }
    else if (drag_.mode == DragMode::ResizeLeft || drag_.mode == DragMode::ResizeRight)
    {
        if (drag_.currentStartStep != drag_.originalStartStep ||
            drag_.currentLengthSteps != drag_.originalLengthSteps ||
            drag_.currentOffsetSteps != drag_.originalOffsetSteps)
        {
            listener_.clipResized(drag_.clipId, drag_.currentStartStep, drag_.currentLengthSteps,
                                  drag_.currentOffsetSteps);
        }
    }

    drag_.active = false;
    drag_.mode = DragMode::None;
    mouseMove(e);
    repaint();
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

} // namespace trami
