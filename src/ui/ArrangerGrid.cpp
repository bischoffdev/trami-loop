#include "ui/ArrangerGrid.h"

#include "ui/TramiLookAndFeel.h"

#include <algorithm>
#include <cmath>

namespace trami
{

ArrangerGrid::ArrangerGrid(const Song& song, Listener& listener) : song_(song), listener_(listener)
{
    setOpaque(true);
}

void ArrangerGrid::setSelectedBlock(BlockId blockId)
{
    selectedBlock_ = blockId;
    repaint();
}

void ArrangerGrid::setPlayheadBar(double bar)
{
    playheadBar_ = bar;
    repaint();
}

void ArrangerGrid::setZoom(float zoomX, float zoomY)
{
    zoomX_ = zoomX;
    zoomY_ = zoomY;
    barWidth_ = std::max(16, juce::roundToInt(static_cast<float>(kDefaultBarWidth) * zoomX_));
    trackHeight_ = std::max(22, juce::roundToInt(static_cast<float>(kDefaultTrackHeight) * zoomY_));
    repaint();
}

void ArrangerGrid::resized()
{
}

juce::Colour ArrangerGrid::patternColour(PatternId id) const
{
    constexpr double kGoldenRatio = 0.6180339887498949;
    double integral = 0.0;
    const float hue = static_cast<float>(std::modf(id.value * kGoldenRatio, &integral));
    return juce::Colour::fromHSV(hue, 0.70f, 0.85f, 1.0f);
}

juce::Rectangle<int> ArrangerGrid::blockBounds(int track, int bar, int lengthBars) const
{
    return {bar * barWidth_, kRulerHeight + track * trackHeight_, std::max(1, lengthBars) * barWidth_, trackHeight_};
}

std::pair<int, int> ArrangerGrid::pointToTrackAndBar(juce::Point<int> pt) const
{
    const int track = (pt.y - kRulerHeight) / trackHeight_;
    const int bar = pt.x / barWidth_;
    return {track, bar};
}

const PatternBlock* ArrangerGrid::findBlockAt(juce::Point<int> pt) const
{
    if (pt.y < kRulerHeight)
        return nullptr;

    const auto [trackIdx, barIdx] = pointToTrackAndBar(pt);
    if (trackIdx < 0 || trackIdx >= kArrangerTracks)
        return nullptr;

    const auto& track = song_.arrangement()[static_cast<std::size_t>(trackIdx)];
    for (const auto& block : track.blocks)
    {
        const Pattern* pattern = song_.findPattern(block.patternId);
        const int lengthBars = pattern != nullptr ? pattern->lengthBars : 1;
        if (barIdx >= block.startBar && barIdx < block.startBar + lengthBars)
            return &block;
    }

    return nullptr;
}

void ArrangerGrid::paint(juce::Graphics& g)
{
    g.fillAll(Palette::background);

    const int totalBars = std::max(16, song_.lengthBars() + 4);
    const int gridWidth = totalBars * barWidth_;
    const int renderWidth = std::max(getWidth(), gridWidth);
    const int totalHeight = kRulerHeight + kArrangerTracks * trackHeight_;

    // 1. Ruler
    g.setColour(Palette::surface);
    g.fillRect(0, 0, renderWidth, kRulerHeight);
    g.setColour(Palette::textDim);
    g.setFont(Typography::bold(12.0f));

    const int labelInterval = (barWidth_ < 22) ? 4 : ((barWidth_ < 36) ? 2 : 1);
    for (int bar = 0; bar < totalBars; ++bar)
    {
        const int x = bar * barWidth_;
        if (bar % labelInterval == 0)
        {
            g.setColour(Palette::text);
            g.drawText(juce::String(bar + 1), x + 4, 2, barWidth_ * labelInterval - 4, kRulerHeight - 4,
                       juce::Justification::centredLeft);
        }
        g.setColour(Palette::gridLine.brighter(0.4f));
        g.drawVerticalLine(x, 0.0f, static_cast<float>(kRulerHeight));
    }

    // 2. Tracks & Grid lines
    for (int t = 0; t < kArrangerTracks; ++t)
    {
        const int y = kRulerHeight + t * trackHeight_;
        g.setColour(t % 2 == 0 ? Palette::background : Palette::surface.withAlpha(0.35f));
        g.fillRect(0, y, renderWidth, trackHeight_);

        g.setColour(Palette::gridLine.withAlpha(0.6f));
        g.drawHorizontalLine(y + trackHeight_, 0.0f, static_cast<float>(renderWidth));
    }

    for (int bar = 0; bar <= totalBars; ++bar)
    {
        const int x = bar * barWidth_;
        if (barWidth_ < 18 && bar % 4 != 0)
            continue;

        g.setColour(bar % 4 == 0 ? Palette::gridLine.brighter(0.4f) : Palette::gridLine.withAlpha(0.4f));
        g.drawVerticalLine(x, static_cast<float>(kRulerHeight), static_cast<float>(totalHeight));
    }

    // 3. Blocks
    const auto& arrangement = song_.arrangement();
    for (int t = 0; t < kArrangerTracks; ++t)
    {
        for (const auto& block : arrangement[static_cast<std::size_t>(t)].blocks)
        {
            int renderTrack = t;
            int renderBar = block.startBar;

            if (drag_.active && drag_.blockId == block.id)
            {
                renderTrack = drag_.currentTrack;
                renderBar = drag_.currentBar;
            }

            const Pattern* pattern = song_.findPattern(block.patternId);
            const int lengthBars = pattern != nullptr ? pattern->lengthBars : 1;
            const juce::String name = pattern != nullptr ? pattern->name : "Pattern";

            auto bounds = blockBounds(renderTrack, renderBar, lengthBars).reduced(2, 4);
            const juce::Colour col = patternColour(block.patternId);

            g.setColour(col.withAlpha(0.85f));
            g.fillRoundedRectangle(bounds.toFloat(), 5.0f);

            const bool isSelected = (block.id == selectedBlock_);
            g.setColour(isSelected ? juce::Colours::white : col.brighter(0.35f));
            g.drawRoundedRectangle(bounds.toFloat(), 5.0f, isSelected ? 2.0f : 1.0f);

            if (bounds.getWidth() > 16 && trackHeight_ >= 18)
            {
                auto textArea = bounds.reduced(6, 2);
                g.setColour(Palette::background);
                const float fontSize = std::clamp(static_cast<float>(trackHeight_) * 0.3f, 9.0f, 12.0f);
                g.setFont(juce::FontOptions(fontSize).withStyle("Bold"));
                g.drawText(name + " (" + juce::String(lengthBars) + "b)", textArea, juce::Justification::centredLeft,
                           true);
            }
        }
    }

    // 4. Playhead
    if (playheadBar_ >= 0.0 && playheadBar_ <= static_cast<double>(totalBars))
    {
        const float px = static_cast<float>(playheadBar_ * barWidth_);
        g.setColour(Palette::accent);
        g.drawVerticalLine(static_cast<int>(px), 0.0f, static_cast<float>(totalHeight));
    }
}

void ArrangerGrid::mouseDown(const juce::MouseEvent& e)
{
    if (e.mods.isRightButtonDown())
    {
        if (const auto* block = findBlockAt(e.getPosition()))
            listener_.blockDeleteRequested(block->id);
        return;
    }

    const auto [track, bar] = pointToTrackAndBar(e.getPosition());
    if (track < 0 || track >= kArrangerTracks || bar < 0)
        return;

    if (const auto* block = findBlockAt(e.getPosition()))
    {
        selectedBlock_ = block->id;
        drag_ = {block->id, track, block->startBar, track, block->startBar, true};
        listener_.blockClicked(block->id);
        repaint();
    }
    else
    {
        selectedBlock_ = {};
        listener_.gridCellClicked(track, bar);
        repaint();
    }
}

void ArrangerGrid::mouseDrag(const juce::MouseEvent& e)
{
    if (!drag_.active)
        return;

    const auto [track, bar] = pointToTrackAndBar(e.getPosition());
    const int clampedTrack = std::clamp(track, 0, kArrangerTracks - 1);
    const int clampedBar = std::max(0, bar);

    if (clampedTrack != drag_.currentTrack || clampedBar != drag_.currentBar)
    {
        drag_.currentTrack = clampedTrack;
        drag_.currentBar = clampedBar;
        repaint();
    }
}

void ArrangerGrid::mouseUp(const juce::MouseEvent&)
{
    if (!drag_.active)
        return;

    if (drag_.currentTrack != drag_.originalTrack || drag_.currentBar != drag_.originalBar)
        listener_.blockMoved(drag_.blockId, drag_.currentTrack, drag_.currentBar);

    drag_.active = false;
    repaint();
}

void ArrangerGrid::mouseDoubleClick(const juce::MouseEvent& e)
{
    if (const auto* block = findBlockAt(e.getPosition()))
        listener_.blockDoubleClicked(block->patternId);
}

void ArrangerGrid::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
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

void ArrangerGrid::mouseMagnify(const juce::MouseEvent& e, float scaleFactor)
{
    if (std::abs(scaleFactor - 1.0f) > 0.001f)
    {
        listener_.zoomRequested(scaleFactor, 1.0f, e.getPosition());
    }
}

} // namespace trami
