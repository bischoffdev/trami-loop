#include "ui/ArrangerGrid.h"

#include "ui/DAWTrackControls.h"
#include "ui/TramiLookAndFeel.h"

#include <algorithm>
#include <cmath>

namespace trami
{

ArrangerGrid::ArrangerGrid(const Song& song, Listener& listener) : song_(song), listener_(listener)
{
    setOpaque(true);
    setWantsKeyboardFocus(true);
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

void ArrangerGrid::setLoopRegion(int startBar, int endBar)
{
    loopStartBar_ = std::max(0, startBar);
    loopEndBar_ = std::max(loopStartBar_ + 1, endBar);
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

juce::Colour ArrangerGrid::patternColour(PatternId id)
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
    const int numTracks = song_.numArrangerTracks();
    const int totalHeight = kRulerHeight + numTracks * trackHeight_;

    // 1. Ruler
    g.setColour(juce::Colour(0xff23272d));
    g.fillRect(0, 0, renderWidth, kRulerHeight);
    g.setColour(juce::Colour(0xff181a1f));
    g.drawHorizontalLine(kRulerHeight - 1, 0.0f, static_cast<float>(renderWidth));
    g.setFont(Typography::bold(11.0f));

    const int labelInterval = (barWidth_ < 22) ? 4 : ((barWidth_ < 36) ? 2 : 1);
    for (int bar = 0; bar < totalBars; ++bar)
    {
        const int x = bar * barWidth_;
        if (bar % labelInterval == 0)
        {
            g.setColour(Palette::text);
            g.drawText(juce::String(bar + 1), x + 4, 7, barWidth_ * labelInterval - 4, kRulerHeight - 8,
                       juce::Justification::centredLeft);
        }
        // Bottom ruler tick marks
        g.setColour(bar % 4 == 0 ? Palette::gridBarLine : Palette::gridLine);
        g.drawVerticalLine(x, static_cast<float>(kRulerHeight - 5), static_cast<float>(kRulerHeight));
    }

    // 2. Loop region highlight (down arranger tracks and Studio One banner in ruler)
    const int loopStartX = loopStartBar_ * barWidth_;
    const int loopEndX = loopEndBar_ * barWidth_;
    const int loopW = std::max(4, loopEndX - loopStartX);

    // Shaded loop region down arranger tracks
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

    for (int bar = 0; bar <= totalBars; ++bar)
    {
        const int x = bar * barWidth_;
        if (barWidth_ < 18 && bar % 4 != 0)
            continue;

        g.setColour(bar % 4 == 0 ? Palette::gridBarLine : Palette::gridLine);
        g.drawVerticalLine(x, static_cast<float>(kRulerHeight), static_cast<float>(totalHeight));
    }

    // 3. Blocks
    const auto& arrangement = song_.arrangement();
    for (int t = 0; t < numTracks; ++t)
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

    // 4. Drag hover preview
    if (dragHoverTrack_ >= 0 && dragHoverBar_ >= 0 && dragHoverPatternId_.isValid())
    {
        const int lengthBars = song_.findPattern(dragHoverPatternId_) != nullptr
                                   ? song_.findPattern(dragHoverPatternId_)->lengthBars
                                   : 1;
        const auto b = blockBounds(dragHoverTrack_, dragHoverBar_, lengthBars).reduced(2, 3);
        const auto col = patternColour(dragHoverPatternId_);

        g.setColour(col.withAlpha(0.28f));
        g.fillRoundedRectangle(b.toFloat(), 5.0f);
        g.setColour(col);
        g.drawRoundedRectangle(b.toFloat(), 5.0f, 2.0f);

        g.setColour(juce::Colours::white);
        g.setFont(Typography::bold(12.0f));
        juce::String name = "Pattern";
        if (const auto* p = song_.findPattern(dragHoverPatternId_))
            name = p->name;
        g.drawText("+ " + name, b.reduced(6, 2), juce::Justification::centredLeft, true);
    }

    // 5. Playhead
    if (playheadBar_ >= 0.0 && playheadBar_ <= static_cast<double>(totalBars))
    {
        const float px = static_cast<float>(playheadBar_ * barWidth_);
        g.setColour(Palette::accent);
        g.drawVerticalLine(static_cast<int>(px), static_cast<float>(kRulerHeight), static_cast<float>(totalHeight));

        // Downward playhead pointer in the ruler
        juce::Path p;
        p.startNewSubPath(px - 6.0f, 1.0f);
        p.lineTo(px + 6.0f, 1.0f);
        p.lineTo(px + 6.0f, static_cast<float>(kRulerHeight - 6));
        p.lineTo(px, static_cast<float>(kRulerHeight));
        p.lineTo(px - 6.0f, static_cast<float>(kRulerHeight - 6));
        p.closeSubPath();

        g.setColour(Palette::accent);
        g.fillPath(p);
        g.setColour(juce::Colours::white);
        g.strokePath(p, juce::PathStrokeType(1.2f));
    }
}

void ArrangerGrid::mouseMove(const juce::MouseEvent& e)
{
    if (drag_.active)
        return;

    const float playheadPx = static_cast<float>(playheadBar_ * barWidth_);
    const bool isNearPlayhead = (playheadBar_ >= 0.0 && std::abs(static_cast<float>(e.getPosition().x) - playheadPx) <= 6.0f);

    if (e.getPosition().y < kRulerHeight)
    {
        const int startX = loopStartBar_ * barWidth_;
        const int endX = loopEndBar_ * barWidth_;
        const int mx = e.getPosition().x;
        const int my = e.getPosition().y;

        // Generous 12px hit margin in the top 18px of the ruler for loop handles
        if (my <= 18 && (std::abs(mx - startX) <= 12 || std::abs(mx - endX) <= 12))
        {
            setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
        }
        else if (my <= 15 && mx > startX + 12 && mx < endX - 12)
        {
            setMouseCursor(juce::MouseCursor::DraggingHandCursor);
        }
        else if (isNearPlayhead || my > 15)
        {
            setMouseCursor(juce::MouseCursor::PointingHandCursor);
        }
        else
        {
            setMouseCursor(juce::MouseCursor::NormalCursor);
        }
        return;
    }

    if (isNearPlayhead)
    {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
        return;
    }
    setMouseCursor(juce::MouseCursor::NormalCursor);
}

void ArrangerGrid::mouseDown(const juce::MouseEvent& e)
{
    if (e.getPosition().y < kRulerHeight)
    {
        if (e.mods.isPopupMenu())
        {
            juce::PopupMenu m;
            m.addItem(1, "Loop Entire Song");
            m.addItem(2, "Loop 4 Bars");
            m.addItem(3, "Loop 8 Bars");
            m.addItem(4, "Loop 16 Bars");
            m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withTargetScreenArea(
                                juce::Rectangle<int>(e.getScreenX(), e.getScreenY(), 1, 1)),
                            [this](int result) {
                                if (result == 1)
                                {
                                    loopStartBar_ = 0;
                                    loopEndBar_ = std::max(2, song_.lengthBars());
                                    listener_.loopRegionChanged(loopStartBar_, loopEndBar_);
                                    repaint();
                                }
                                else if (result == 2)
                                {
                                    loopStartBar_ = 0;
                                    loopEndBar_ = 4;
                                    listener_.loopRegionChanged(loopStartBar_, loopEndBar_);
                                    repaint();
                                }
                                else if (result == 3)
                                {
                                    loopStartBar_ = 0;
                                    loopEndBar_ = 8;
                                    listener_.loopRegionChanged(loopStartBar_, loopEndBar_);
                                    repaint();
                                }
                                else if (result == 4)
                                {
                                    loopStartBar_ = 0;
                                    loopEndBar_ = 16;
                                    listener_.loopRegionChanged(loopStartBar_, loopEndBar_);
                                    repaint();
                                }
                            });
            return;
        }

        const int totalBars = std::max(16, song_.lengthBars() + 4);
        const int clickBar = std::clamp(e.getPosition().x / barWidth_, 0, totalBars);

        const int startX = loopStartBar_ * barWidth_;
        const int endX = loopEndBar_ * barWidth_;
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
                rulerDragAnchorBar_ = clickBar;
                rulerDragOriginalStart_ = loopStartBar_;
                rulerDragOriginalEnd_ = loopEndBar_;
            }
            else
            {
                rulerDragMode_ = RulerDragMode::SetRegion;
                rulerDragAnchorBar_ = clickBar;
                loopStartBar_ = clickBar;
                loopEndBar_ = std::min(totalBars, clickBar + 2);
            }
            repaint();
            return;
        }

        // Lower ruler area (y > 16): Scrub playhead
        rulerDragMode_ = RulerDragMode::SeekPlayhead;
        const double preciseBar = std::max(0.0, static_cast<double>(e.getPosition().x) / static_cast<double>(barWidth_));
        listener_.playheadSeekRequested(preciseBar);
        setPlayheadBar(preciseBar);
        return;
    }

    if (e.mods.isRightButtonDown())
    {
        if (const auto* block = findBlockAt(e.getPosition()))
            listener_.blockDeleteRequested(block->id);
        return;
    }

    const float playheadPx = static_cast<float>(playheadBar_ * barWidth_);
    const bool isNearPlayhead = (playheadBar_ >= 0.0 && std::abs(static_cast<float>(e.getPosition().x) - playheadPx) <= 6.0f);
    if (isNearPlayhead)
    {
        rulerDragMode_ = RulerDragMode::SeekPlayhead;
        const double preciseBar = std::max(0.0, static_cast<double>(e.getPosition().x) / static_cast<double>(barWidth_));
        listener_.playheadSeekRequested(preciseBar);
        setPlayheadBar(preciseBar);
        return;
    }

    const auto [track, bar] = pointToTrackAndBar(e.getPosition());
    if (track < 0 || track >= song_.numArrangerTracks() || bar < 0)
        return;

    if (const auto* block = findBlockAt(e.getPosition()))
    {
        grabKeyboardFocus();
        selectedBlock_ = block->id;
        drag_ = {block->id, track, block->startBar, track, block->startBar, e.getPosition(), true};
        listener_.blockClicked(block->id);
        repaint();
    }
    else
    {
        grabKeyboardFocus();
        selectedBlock_ = {};
        listener_.gridCellClicked(track, bar);
        repaint();
    }
}

void ArrangerGrid::mouseDrag(const juce::MouseEvent& e)
{
    if (rulerDragMode_ != RulerDragMode::None)
    {
        if (rulerDragMode_ == RulerDragMode::SeekPlayhead)
        {
            const double preciseBar = std::max(0.0, static_cast<double>(e.getPosition().x) / static_cast<double>(barWidth_));
            listener_.playheadSeekRequested(preciseBar);
            setPlayheadBar(preciseBar);
            return;
        }

        const int totalBars = std::max(16, song_.lengthBars() + 4);
        const int currentBar = std::clamp(e.getPosition().x / barWidth_, 0, totalBars);

        if (rulerDragMode_ == RulerDragMode::SetRegion)
        {
            loopStartBar_ = std::min(rulerDragAnchorBar_, currentBar);
            loopEndBar_ = std::max(rulerDragAnchorBar_, currentBar);
            if (loopEndBar_ <= loopStartBar_)
                loopEndBar_ = std::min(totalBars, loopStartBar_ + 1);
        }
        else if (rulerDragMode_ == RulerDragMode::ResizeStart)
        {
            loopStartBar_ = std::clamp(currentBar, 0, loopEndBar_ - 1);
        }
        else if (rulerDragMode_ == RulerDragMode::ResizeEnd)
        {
            loopEndBar_ = std::clamp(currentBar, loopStartBar_ + 1, totalBars);
        }
        else if (rulerDragMode_ == RulerDragMode::MoveRegion)
        {
            const int delta = currentBar - rulerDragAnchorBar_;
            const int len = rulerDragOriginalEnd_ - rulerDragOriginalStart_;
            const int newStart = std::clamp(rulerDragOriginalStart_ + delta, 0, totalBars - len);
            loopStartBar_ = newStart;
            loopEndBar_ = newStart + len;
        }
        repaint();
        return;
    }

    if (!drag_.active)
        return;

    if (e.getDistanceFromDragStart() < 3)
        return;

    const int deltaX = e.getPosition().x - drag_.startMousePos.x;
    const int deltaBars = juce::roundToInt(static_cast<float>(deltaX) / static_cast<float>(barWidth_));
    const int newBar = std::max(0, drag_.originalBar + deltaBars);

    // Arranger blocks are constrained horizontally to their dedicated pattern track
    drag_.currentTrack = drag_.originalTrack;

    if (newBar != drag_.currentBar)
    {
        drag_.currentBar = newBar;
        repaint();
    }
}

void ArrangerGrid::mouseUp(const juce::MouseEvent&)
{
    if (rulerDragMode_ != RulerDragMode::None)
    {
        const auto mode = rulerDragMode_;
        rulerDragMode_ = RulerDragMode::None;
        if (mode != RulerDragMode::SeekPlayhead)
            listener_.loopRegionChanged(loopStartBar_, loopEndBar_);
        repaint();
        return;
    }

    if (!drag_.active)
        return;

    if (drag_.currentTrack != drag_.originalTrack || drag_.currentBar != drag_.originalBar)
        listener_.blockMoved(drag_.blockId, drag_.currentTrack, drag_.currentBar);

    drag_.active = false;
    repaint();
}

void ArrangerGrid::mouseDoubleClick(const juce::MouseEvent& e)
{
    if (e.getPosition().y < kRulerHeight)
    {
        // Double clicking ruler sets loop to entire song
        loopStartBar_ = 0;
        loopEndBar_ = std::max(2, song_.lengthBars());
        listener_.loopRegionChanged(loopStartBar_, loopEndBar_);
        repaint();
        return;
    }

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

bool ArrangerGrid::isInterestedInDragSource(const SourceDetails& dragSourceDetails)
{
    if (dragSourceDetails.description.isString())
    {
        return dragSourceDetails.description.toString().startsWith("pattern:");
    }
    return false;
}

void ArrangerGrid::itemDragEnter(const SourceDetails& dragSourceDetails)
{
    itemDragMove(dragSourceDetails);
}

void ArrangerGrid::itemDragMove(const SourceDetails& dragSourceDetails)
{
    if (dragSourceDetails.description.isString())
    {
        const juce::String s = dragSourceDetails.description.toString();
        const int patVal = s.fromFirstOccurrenceOf("pattern:", false, false).getIntValue();
        dragHoverPatternId_ = PatternId{static_cast<std::uint32_t>(patVal)};

        const auto [track, bar] = pointToTrackAndBar(dragSourceDetails.localPosition);
        const int clampedTrack = std::clamp(track, 0, kArrangerTracks - 1);
        const int clampedBar = std::max(0, bar);

        int targetTrack = song_.findTrackForPattern(dragHoverPatternId_);
        if (targetTrack < 0)
        {
            if (!song_.arrangerTrackPattern(clampedTrack).isValid())
            {
                targetTrack = clampedTrack;
            }
            else
            {
                for (int t = 0; t < kArrangerTracks; ++t)
                {
                    if (!song_.arrangerTrackPattern(t).isValid())
                    {
                        targetTrack = t;
                        break;
                    }
                }
                if (targetTrack < 0)
                    targetTrack = clampedTrack;
            }
        }

        if (targetTrack != dragHoverTrack_ || clampedBar != dragHoverBar_)
        {
            dragHoverTrack_ = targetTrack;
            dragHoverBar_ = clampedBar;
            repaint();
        }
    }
}

void ArrangerGrid::itemDragExit(const SourceDetails& /*dragSourceDetails*/)
{
    dragHoverTrack_ = -1;
    dragHoverBar_ = -1;
    dragHoverPatternId_ = {};
    repaint();
}

void ArrangerGrid::itemDropped(const SourceDetails& /*dragSourceDetails*/)
{
    const int track = dragHoverTrack_;
    const int bar = dragHoverBar_;
    const auto patId = dragHoverPatternId_;

    dragHoverTrack_ = -1;
    dragHoverBar_ = -1;
    dragHoverPatternId_ = {};
    repaint();

    if (track >= 0 && track < kArrangerTracks && bar >= 0 && patId.isValid())
    {
        listener_.patternDropped(patId, track, bar);
    }
}

bool ArrangerGrid::keyPressed(const juce::KeyPress& key)
{
    if (key.getKeyCode() == juce::KeyPress::deleteKey || key.getKeyCode() == juce::KeyPress::backspaceKey)
    {
        if (selectedBlock_.isValid())
        {
            const auto bid = selectedBlock_;
            listener_.blockDeleteRequested(bid);
            return true;
        }
    }
    return false;
}

} // namespace trami
