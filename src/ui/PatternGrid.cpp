#include "ui/PatternGrid.h"

#include "model/Timing.h"
#include "ui/TramiLookAndFeel.h"

#include <algorithm>
#include <cmath>

namespace trami
{

PatternGrid::PatternGrid(const Song& song, PatternId patternId, Listener& listener)
    : song_(song), patternId_(patternId), listener_(listener)
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

void PatternGrid::resized()
{
}

juce::Colour PatternGrid::sampleColour(SampleId id) const
{
    const Sample* sample = song_.findSample(id);
    const float hue = sample != nullptr ? sample->hue() : 0.5f;
    return juce::Colour::fromHSV(hue, 0.75f, 0.85f, 1.0f);
}

juce::Rectangle<int> PatternGrid::clipBounds(int track, int step, int lengthSteps) const
{
    return {step * kStepWidth, kRulerHeight + track * kTrackHeight, std::max(1, lengthSteps) * kStepWidth,
            kTrackHeight};
}

std::pair<int, int> PatternGrid::pointToTrackAndStep(juce::Point<int> pt) const
{
    const int track = (pt.y - kRulerHeight) / kTrackHeight;
    const int step = pt.x / kStepWidth;
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
    for (const auto& clip : track.clips)
    {
        const Sample* sample = song_.findSample(clip.sampleId);
        int span = 1;
        if (sample != nullptr)
        {
            if (sample->kind == SampleKind::Loop)
                span = sample->loopBars * kStepsPerBar;
            else
            {
                const double stepSec = 60.0 / song_.bpm() / kStepsPerBeat;
                span = std::max(1, static_cast<int>(std::round(sample->durationSeconds / stepSec)));
            }
        }

        if (stepIdx >= clip.startStep && stepIdx < clip.startStep + span)
            return &clip;
    }

    return nullptr;
}

void PatternGrid::paint(juce::Graphics& g)
{
    g.fillAll(Palette::background);

    const Pattern* pattern = song_.findPattern(patternId_);
    const int totalSteps = pattern != nullptr ? pattern->lengthSteps() : 16;
    const int gridWidth = totalSteps * kStepWidth;
    const int totalHeight = kRulerHeight + kTracksPerPattern * kTrackHeight;

    // 1. Ruler
    g.setColour(Palette::surface);
    g.fillRect(0, 0, gridWidth, kRulerHeight);
    g.setColour(Palette::textDim);
    g.setFont(juce::FontOptions(11.0f));

    for (int step = 0; step < totalSteps; ++step)
    {
        const int x = step * kStepWidth;
        const bool isBar = (step % kStepsPerBar == 0);
        const bool isBeat = (step % kStepsPerBeat == 0);

        if (isBar)
        {
            const int barNum = step / kStepsPerBar + 1;
            g.setColour(Palette::text);
            g.drawText(juce::String(barNum), x + 4, 2, kStepWidth * 2, kRulerHeight - 4,
                       juce::Justification::centredLeft);
            g.setColour(Palette::gridLine.brighter(0.4f));
            g.drawVerticalLine(x, 0.0f, static_cast<float>(kRulerHeight));
        }
        else if (isBeat)
        {
            g.setColour(Palette::gridLine);
            g.drawVerticalLine(x, static_cast<float>(kRulerHeight - 6), static_cast<float>(kRulerHeight));
        }
    }

    // 2. Tracks & Grid lines
    for (int t = 0; t < kTracksPerPattern; ++t)
    {
        const int y = kRulerHeight + t * kTrackHeight;
        g.setColour(t % 2 == 0 ? Palette::background : Palette::surface.withAlpha(0.4f));
        g.fillRect(0, y, gridWidth, kTrackHeight);

        g.setColour(Palette::gridLine.withAlpha(0.6f));
        g.drawHorizontalLine(y + kTrackHeight, 0.0f, static_cast<float>(gridWidth));
    }

    for (int step = 0; step <= totalSteps; ++step)
    {
        const int x = step * kStepWidth;
        const bool isBar = (step % kStepsPerBar == 0);
        const bool isBeat = (step % kStepsPerBeat == 0);

        g.setColour(isBar    ? Palette::gridLine.brighter(0.4f)
                    : isBeat ? Palette::gridLine
                             : Palette::gridLine.withAlpha(0.3f));
        g.drawVerticalLine(x, static_cast<float>(kRulerHeight), static_cast<float>(totalHeight));
    }

    // 3. Clips
    if (pattern != nullptr)
    {
        for (int t = 0; t < kTracksPerPattern; ++t)
        {
            for (const auto& clip : pattern->tracks[static_cast<std::size_t>(t)].clips)
            {
                int renderTrack = t;
                int renderStep = clip.startStep;

                if (drag_.active && drag_.clipId == clip.id)
                {
                    renderTrack = drag_.currentTrack;
                    renderStep = drag_.currentStep;
                }

                const Sample* sample = song_.findSample(clip.sampleId);
                int span = 1;
                bool isLoop = false;
                juce::String name = "Clip";

                if (sample != nullptr)
                {
                    isLoop = (sample->kind == SampleKind::Loop);
                    name = sample->name;
                    if (isLoop)
                        span = sample->loopBars * kStepsPerBar;
                    else
                    {
                        const double stepSec = 60.0 / song_.bpm() / kStepsPerBeat;
                        span = std::max(1, static_cast<int>(std::round(sample->durationSeconds / stepSec)));
                    }
                }

                auto bounds = clipBounds(renderTrack, renderStep, span).reduced(2, 3);
                const juce::Colour col = sampleColour(clip.sampleId);

                g.setColour(col.withAlpha(0.85f));
                g.fillRoundedRectangle(bounds.toFloat(), 4.0f);

                const bool isSelected = (clip.id == selectedClip_);
                g.setColour(isSelected ? juce::Colours::white : col.brighter(0.3f));
                g.drawRoundedRectangle(bounds.toFloat(), 4.0f, isSelected ? 2.0f : 1.0f);

                // Indicator tag [L] or [1]
                auto textArea = bounds.reduced(4, 2);
                g.setColour(Palette::background);
                g.setFont(juce::FontOptions(11.0f));

                const juce::String tag = isLoop ? "[L] " : "[1] ";
                g.drawText(tag + name, textArea, juce::Justification::centredLeft, true);
            }
        }
    }

    // 4. Playhead
    if (playheadStep_ >= 0.0 && playheadStep_ <= static_cast<double>(totalSteps))
    {
        const float px = static_cast<float>(playheadStep_ * kStepWidth);
        g.setColour(Palette::accent);
        g.drawVerticalLine(static_cast<int>(px), 0.0f, static_cast<float>(totalHeight));
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
        drag_ = {clip->id, track, clip->startStep, track, clip->startStep, true};
        listener_.clipClicked(clip->id);
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

    if (clampedTrack != drag_.currentTrack || clampedStep != drag_.currentStep)
    {
        drag_.currentTrack = clampedTrack;
        drag_.currentStep = clampedStep;
        repaint();
    }
}

void PatternGrid::mouseUp(const juce::MouseEvent&)
{
    if (!drag_.active)
        return;

    if (drag_.currentTrack != drag_.originalTrack || drag_.currentStep != drag_.originalStep)
        listener_.clipMoved(drag_.clipId, drag_.currentTrack, drag_.currentStep);

    drag_.active = false;
    repaint();
}

} // namespace trami
