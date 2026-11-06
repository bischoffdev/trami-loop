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

void PatternGrid::setZoom(float zoomX, float zoomY)
{
    zoomX_ = zoomX;
    zoomY_ = zoomY;
    stepWidth_ = std::max(8, juce::roundToInt(static_cast<float>(kDefaultStepWidth) * zoomX_));
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

int PatternGrid::clipSpan(const Clip& clip) const
{
    if (clip.lengthSteps > 0)
        return clip.lengthSteps;

    const Sample* sample = song_.findSample(clip.sampleId);
    if (sample == nullptr)
        return 1;

    if (sample->kind == SampleKind::Loop)
        return sample->loopBars * kStepsPerBar;

    const double stepSec = 60.0 / song_.bpm() / kStepsPerBeat;
    return std::max(1, static_cast<int>(std::round(sample->durationSeconds / stepSec)));
}

juce::Rectangle<int> PatternGrid::clipBounds(int track, int step, int lengthSteps) const
{
    return {step * stepWidth_, kRulerHeight + track * trackHeight_, std::max(1, lengthSteps) * stepWidth_,
            trackHeight_};
}

std::pair<int, int> PatternGrid::pointToTrackAndStep(juce::Point<int> pt) const
{
    const int track = (pt.y - kRulerHeight) / trackHeight_;
    const int step = pt.x / stepWidth_;
    return {track, step};
}

const Clip* PatternGrid::findClipAt(juce::Point<int> pt) const
{
    const Pattern* pattern = song_.findPattern(patternId_);
    if (pattern == nullptr || pt.y < kRulerHeight)
        return nullptr;

    const auto [trackIdx, stepIdx] = pointToTrackAndStep(pt);
    if (trackIdx < 0 || trackIdx >= kTracksPerPattern)
        return nullptr;

    const auto& track = pattern->tracks[static_cast<std::size_t>(trackIdx)];
    for (auto it = track.clips.rbegin(); it != track.clips.rend(); ++it)
    {
        const int span = clipSpan(*it);
        auto b = clipBounds(trackIdx, it->startStep, span).reduced(2, 3);
        if (b.contains(pt))
            return &(*it);
    }

    return nullptr;
}

PatternGrid::DragMode PatternGrid::hitTestResize(juce::Point<int> pt) const
{
    const Pattern* pattern = song_.findPattern(patternId_);
    if (pattern == nullptr || pt.y < kRulerHeight)
        return DragMode::None;

    const auto [trackIdx, stepIdx] = pointToTrackAndStep(pt);
    if (trackIdx < 0 || trackIdx >= kTracksPerPattern)
        return DragMode::None;

    const auto& track = pattern->tracks[static_cast<std::size_t>(trackIdx)];
    for (auto it = track.clips.rbegin(); it != track.clips.rend(); ++it)
    {
        const int span = clipSpan(*it);
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

    const Pattern* pattern = song_.findPattern(patternId_);
    const int totalSteps = pattern != nullptr ? pattern->lengthSteps() : 16;
    const int gridWidth = totalSteps * stepWidth_;
    const int renderWidth = std::max(getWidth(), gridWidth);
    const int totalHeight = kRulerHeight + kTracksPerPattern * trackHeight_;

    // 1. Ruler
    g.setColour(Palette::surface);
    g.fillRect(0, 0, renderWidth, kRulerHeight);
    g.setColour(Palette::textDim);
    g.setFont(Typography::bold(12.0f));

    for (int step = 0; step < totalSteps; ++step)
    {
        const int x = step * stepWidth_;
        const bool isBar = (step % kStepsPerBar == 0);
        const bool isBeat = (step % kStepsPerBeat == 0);

        if (isBar)
        {
            const int barNum = step / kStepsPerBar + 1;
            g.setColour(Palette::text);
            const int labelW = std::max(stepWidth_ * 2, stepWidth_ * kStepsPerBar);
            g.drawText(juce::String(barNum) + ".1", x + 4, 2, labelW, kRulerHeight - 4,
                       juce::Justification::centredLeft);
            g.setColour(Palette::gridLine.brighter(0.4f));
            g.drawVerticalLine(x, 0.0f, static_cast<float>(kRulerHeight));
        }
        else if (isBeat && stepWidth_ >= 10)
        {
            g.setColour(Palette::gridLine);
            g.drawVerticalLine(x, static_cast<float>(kRulerHeight - 6), static_cast<float>(kRulerHeight));
        }
    }

    // 2. Tracks & Grid lines
    for (int t = 0; t < kTracksPerPattern; ++t)
    {
        const int y = kRulerHeight + t * trackHeight_;
        g.setColour(t % 2 == 0 ? Palette::background : Palette::surface.withAlpha(0.4f));
        g.fillRect(0, y, renderWidth, trackHeight_);

        g.setColour(Palette::gridLine.withAlpha(0.6f));
        g.drawHorizontalLine(y + trackHeight_, 0.0f, static_cast<float>(renderWidth));
    }

    for (int step = 0; step <= totalSteps; ++step)
    {
        const int x = step * stepWidth_;
        const bool isBar = (step % kStepsPerBar == 0);
        const bool isBeat = (step % kStepsPerBeat == 0);

        if (stepWidth_ < 10 && !isBar)
            continue;
        if (stepWidth_ < 16 && !isBeat && !isBar)
            continue;

        g.setColour(isBar    ? Palette::gridLine.brighter(0.4f)
                    : isBeat ? Palette::gridLine
                             : Palette::gridLine.withAlpha(0.3f));
        g.drawVerticalLine(x, static_cast<float>(kRulerHeight), static_cast<float>(totalHeight));
    }

    // 3. Clips & Overlap Representation
    if (pattern != nullptr)
    {
        for (int t = 0; t < kTracksPerPattern; ++t)
        {
            struct ClipInterval
            {
                ClipId id;
                int start = 0;
                int end = 0;
            };

            std::vector<ClipInterval> intervals;
            for (const auto& clip : pattern->tracks[static_cast<std::size_t>(t)].clips)
            {
                int start = clip.startStep;
                int span = clipSpan(clip);
                if (drag_.active && drag_.clipId == clip.id)
                {
                    if (drag_.currentTrack != t)
                        continue;
                    start = drag_.currentStartStep;
                    span = drag_.currentLengthSteps;
                }
                intervals.push_back({clip.id, start, start + span});
            }

            if (drag_.active && drag_.currentTrack == t && drag_.originalTrack != t)
            {
                intervals.push_back({drag_.clipId, drag_.currentStartStep,
                                     drag_.currentStartStep + drag_.currentLengthSteps});
            }

            // Detect overlaps between clips on this track
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

            // Render each clip on track
            for (const auto& clip : pattern->tracks[static_cast<std::size_t>(t)].clips)
            {
                int renderTrack = t;
                int renderStep = clip.startStep;
                int span = clipSpan(clip);
                int renderOffset = clip.offsetSteps;

                if (drag_.active && drag_.clipId == clip.id)
                {
                    renderTrack = drag_.currentTrack;
                    renderStep = drag_.currentStartStep;
                    span = drag_.currentLengthSteps;
                    renderOffset = drag_.currentOffsetSteps;
                    if (renderTrack != t)
                        continue; // Rendered when outer loop visits renderTrack
                }

                auto bounds = clipBounds(renderTrack, renderStep, span).reduced(2, 3);
                const juce::Colour col = sampleColour(clip.sampleId);

                // ACID-style dark block container
                g.setColour(juce::Colour(0xff1a1e26));
                g.fillRoundedRectangle(bounds.toFloat(), 4.0f);
                g.setColour(col.withAlpha(0.12f));
                g.fillRoundedRectangle(bounds.toFloat(), 4.0f);

                // Top accent stripe in track color
                g.setColour(col);
                g.fillRoundedRectangle(static_cast<float>(bounds.getX()), static_cast<float>(bounds.getY()),
                                       static_cast<float>(bounds.getWidth()), 3.0f, 2.0f);

                // Waveform representation on the block rendered in vibrant track color
                auto rawAudio = stretchCache_.getRawAudio(clip.sampleId);
                if (rawAudio != nullptr && rawAudio->numFrames() > 0 && bounds.getWidth() > 4)
                {
                    const Sample* sample = song_.findSample(clip.sampleId);
                    int fullSampleSteps = span;
                    if (sample != nullptr)
                    {
                        if (sample->kind == SampleKind::Loop)
                            fullSampleSteps = sample->loopBars * kStepsPerBar;
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

                        // Center axis line
                        g.setColour(col.withAlpha(0.25f));
                        g.drawHorizontalLine(static_cast<int>(cy), static_cast<float>(bounds.getX() + 2),
                                             static_cast<float>(bounds.getRight() - 2));

                        // Dual mirrored waveform envelope in track's signature vibrant color
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

                // ACID loop repetition notches
                const Sample* s = song_.findSample(clip.sampleId);
                const bool isLoop = (s != nullptr && s->kind == SampleKind::Loop);
                if (isLoop && s->loopBars > 0)
                {
                    const int loopSteps = s->loopBars * kStepsPerBar;
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

                // Left & right grab edge handle hints
                if (bounds.getWidth() >= 16)
                {
                    g.setColour(juce::Colours::white.withAlpha(0.4f));
                    g.fillRect(bounds.getX() + 2, bounds.getY() + 4, 2, bounds.getHeight() - 8);
                    g.fillRect(bounds.getRight() - 4, bounds.getY() + 4, 2, bounds.getHeight() - 8);
                }

                // Loop badge
                if (isLoop && bounds.getWidth() >= 24 && trackHeight_ >= 18)
                {
                    g.setColour(Palette::textDim.withAlpha(0.8f));
                    const float fontSize = std::clamp(static_cast<float>(trackHeight_) * 0.3f, 8.0f, 11.0f);
                    g.setFont(juce::FontOptions(fontSize).withStyle("Bold"));
                    g.drawText("[L]", bounds.reduced(6, 3), juce::Justification::topRight, false);
                }

                // Border: white if selected, red if overlapping, colored tint otherwise
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

            // Draw overlap warning regions on this track
            for (const auto& [oStart, oEnd] : overlapRanges)
            {
                auto oBounds = clipBounds(t, oStart, oEnd - oStart).reduced(2, 3);
                g.saveState();
                g.reduceClipRegion(oBounds);

                // Warning wash
                g.setColour(juce::Colour(0x55ff2211));
                g.fillRoundedRectangle(oBounds.toFloat(), 4.0f);

                // Diagonal hazard stripes
                g.setColour(juce::Colour(0xd0ff3322));
                for (int lx = oBounds.getX() - oBounds.getHeight();
                     lx < oBounds.getRight() + oBounds.getHeight(); lx += 10)
                {
                    g.drawLine(static_cast<float>(lx), static_cast<float>(oBounds.getBottom()),
                               static_cast<float>(lx + oBounds.getHeight()),
                               static_cast<float>(oBounds.getY()), 2.0f);
                }

                // Distinctive danger border
                g.setColour(juce::Colour(0xffff3322));
                g.drawRoundedRectangle(oBounds.toFloat(), 4.0f, 2.0f);

                if (oBounds.getWidth() >= 48 && trackHeight_ >= 22)
                {
                    g.setColour(juce::Colours::white);
                    g.setFont(juce::FontOptions(10.0f).withStyle("Bold"));
                    g.drawText("OVERLAP", oBounds, juce::Justification::centred, false);
                }
                g.restoreState();
            }
        }
    }

    // 4. Playhead (with ACID triangular cap on ruler)
    if (playheadStep_ >= 0.0 && playheadStep_ <= static_cast<double>(totalSteps))
    {
        const float px = static_cast<float>(playheadStep_ * stepWidth_);
        g.setColour(Palette::accent);
        g.drawVerticalLine(static_cast<int>(px), 0.0f, static_cast<float>(totalHeight));

        // Triangular playhead cap on the ruler
        juce::Path cap;
        cap.startNewSubPath(px - 5.0f, 0.0f);
        cap.lineTo(px + 5.0f, 0.0f);
        cap.lineTo(px, static_cast<float>(kRulerHeight));
        cap.closeSubPath();
        g.fillPath(cap);
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

    const auto [track, step] = pointToTrackAndStep(e.getPosition());
    const Pattern* pattern = song_.findPattern(patternId_);
    const int maxSteps = pattern != nullptr ? pattern->lengthSteps() : 16;

    if (track < 0 || track >= kTracksPerPattern || step < 0 || step >= maxSteps)
        return;

    if (const Clip* clip = findClipAt(e.getPosition()))
    {
        selectedClip_ = clip->id;
        const auto mode = hitTestResize(e.getPosition());
        const int span = clipSpan(*clip);
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
    if (!drag_.active)
        return;

    const auto [track, step] = pointToTrackAndStep(e.getPosition());
    const Pattern* pattern = song_.findPattern(patternId_);
    const int maxSteps = pattern != nullptr ? pattern->lengthSteps() : 16;

    const int clampedTrack = std::clamp(track, 0, kTracksPerPattern - 1);
    const int clampedStep = std::clamp(step, 0, maxSteps - 1);

    if (drag_.mode == DragMode::Move)
    {
        if (clampedTrack != drag_.currentTrack || clampedStep != drag_.currentStartStep)
        {
            drag_.currentTrack = clampedTrack;
            drag_.currentStartStep = clampedStep;
            repaint();
        }
    }
    else if (drag_.mode == DragMode::ResizeRight)
    {
        const int newLen = std::max(1, clampedStep - drag_.originalStartStep + 1);
        if (newLen != drag_.currentLengthSteps)
        {
            drag_.currentLengthSteps = newLen;
            repaint();
        }
    }
    else if (drag_.mode == DragMode::ResizeLeft)
    {
        const int originalEndStep = drag_.originalStartStep + drag_.originalLengthSteps;
        const int newStart = std::clamp(clampedStep, 0, originalEndStep - 1);
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

void PatternGrid::filesDropped(const juce::StringArray& files, int x, int y)
{
    if (files.isEmpty())
        return;

    const auto [track, step] = pointToTrackAndStep({x, y});
    const Pattern* pattern = song_.findPattern(patternId_);
    const int maxSteps = pattern != nullptr ? pattern->lengthSteps() : 16;

    const int clampedTrack = std::clamp(track, 0, kTracksPerPattern - 1);
    const int clampedStep = std::clamp(step, 0, maxSteps - 1);

    listener_.fileDroppedOnGrid(files[0], clampedTrack, clampedStep);
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

void PatternGrid::itemDropped(const SourceDetails& dragSourceDetails)
{
    if (!dragSourceDetails.description.isString())
        return;

    const juce::String filePath = dragSourceDetails.description.toString();
    const auto [track, step] = pointToTrackAndStep(dragSourceDetails.localPosition);
    const Pattern* pattern = song_.findPattern(patternId_);
    const int maxSteps = pattern != nullptr ? pattern->lengthSteps() : 16;

    const int clampedTrack = std::clamp(track, 0, kTracksPerPattern - 1);
    const int clampedStep = std::clamp(step, 0, maxSteps - 1);

    listener_.fileDroppedOnGrid(filePath, clampedTrack, clampedStep);
}

} // namespace trami
