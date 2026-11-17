#include "ui/TramiLookAndFeel.h"

namespace trami
{

TramiLookAndFeel::TramiLookAndFeel()
{
    setColour(juce::ResizableWindow::backgroundColourId, Palette::background);
    setColour(juce::DocumentWindow::textColourId, Palette::text);
    setColour(juce::Label::textColourId, Palette::text);
    setColour(juce::TextButton::buttonColourId, Palette::surface);
    setColour(juce::TextButton::textColourOffId, Palette::text);
    setColour(juce::Slider::thumbColourId, Palette::accent);

    setColour(juce::ComboBox::backgroundColourId, Palette::surface);
    setColour(juce::ComboBox::textColourId, Palette::text);
    setColour(juce::ComboBox::outlineColourId, Palette::gridLine);
    setColour(juce::PopupMenu::backgroundColourId, Palette::surface);
    setColour(juce::PopupMenu::textColourId, Palette::text);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, Palette::surface.brighter(0.15f));
    setColour(juce::PopupMenu::highlightedTextColourId, Palette::accent);

    setColour(juce::TooltipWindow::backgroundColourId, juce::Colour(0xf415181e));
    setColour(juce::TooltipWindow::outlineColourId, juce::Colour(0xff3e4550));
    setColour(juce::TooltipWindow::textColourId, juce::Colour(0xfff1f5f9));
}

juce::Font TramiLookAndFeel::getTextButtonFont(juce::TextButton&, int)
{
    return juce::Font(Typography::bold(12.5f));
}

juce::Font TramiLookAndFeel::getLabelFont(juce::Label& label)
{
    return label.getFont();
}

juce::Font TramiLookAndFeel::getComboBoxFont(juce::ComboBox&)
{
    return juce::Font(Typography::regular(13.0f));
}

juce::Font TramiLookAndFeel::getPopupMenuFont()
{
    return juce::Font(Typography::regular(14.5f));
}

void TramiLookAndFeel::getIdealPopupMenuItemSize(const juce::String& text, bool isSeparator,
                                                int standardMenuItemHeight, int& idealWidth, int& idealHeight)
{
    if (isSeparator)
    {
        idealWidth = 60;
        idealHeight = 9;
    }
    else
    {
        auto font = getPopupMenuFont();
        const int itemH = standardMenuItemHeight > 0 ? standardMenuItemHeight : 28;
        if (font.getHeight() > static_cast<float>(itemH) / 1.3f)
            font.setHeight(static_cast<float>(itemH) / 1.3f);

        idealHeight = itemH;
        idealWidth = juce::GlyphArrangement::getStringWidthInt(font, text) + idealHeight * 2 + 16;
    }
}

void TramiLookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int width, int height)
{
    const auto r = juce::Rectangle<float>(0.5f, 0.5f, static_cast<float>(width) - 1.0f, static_cast<float>(height) - 1.0f);
    g.setColour(Palette::surface.darker(0.12f));
    g.fillRoundedRectangle(r, 4.0f);

    g.setColour(Palette::gridLine.brighter(0.18f));
    g.drawRoundedRectangle(r, 4.0f, 1.0f);
}

void TramiLookAndFeel::drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                                        bool isSeparator, bool isActive, bool isHighlighted,
                                        bool isTicked, bool hasSubMenu, const juce::String& text,
                                        const juce::String& shortcutKeyText,
                                        const juce::Drawable* icon, const juce::Colour* textColourToUse)
{
    if (isSeparator)
    {
        auto r = area.reduced(8, 0);
        r.removeFromTop(juce::roundToInt((static_cast<float>(r.getHeight()) * 0.5f) - 0.5f));
        g.setColour(findColour(juce::PopupMenu::textColourId).withAlpha(0.2f));
        g.fillRect(r.removeFromTop(1));
        return;
    }

    const auto textColour = (textColourToUse == nullptr ? findColour(juce::PopupMenu::textColourId)
                                                        : *textColourToUse);
    auto r = area.reduced(2, 1);

    if (isHighlighted && isActive)
    {
        g.setColour(findColour(juce::PopupMenu::highlightedBackgroundColourId));
        g.fillRoundedRectangle(r.toFloat(), 3.0f);
        g.setColour(findColour(juce::PopupMenu::highlightedTextColourId));
    }
    else
    {
        g.setColour(textColour.withMultipliedAlpha(isActive ? 1.0f : 0.45f));
    }

    r.reduce(6, 0);

    auto font = getPopupMenuFont();
    const float maxFontHeight = static_cast<float>(r.getHeight()) / 1.3f;
    if (font.getHeight() > maxFontHeight)
        font.setHeight(maxFontHeight);

    g.setFont(font);

    auto iconArea = r.removeFromLeft(juce::roundToInt(maxFontHeight)).toFloat();

    if (icon != nullptr)
    {
        icon->drawWithin(g, iconArea, juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize, 1.0f);
        r.removeFromLeft(juce::roundToInt(maxFontHeight * 0.5f));
    }
    else if (isTicked)
    {
        auto tick = getTickShape(1.0f);
        g.fillPath(tick, tick.getTransformToScaleToFit(iconArea.reduced(iconArea.getWidth() / 5.0f, 0.0f), true));
    }

    if (hasSubMenu)
    {
        const float arrowH = 0.6f * font.getAscent();
        const float x = static_cast<float>(r.removeFromRight(juce::roundToInt(arrowH)).getX());
        const float halfH = static_cast<float>(r.getCentreY());

        juce::Path path;
        path.startNewSubPath(x, halfH - arrowH * 0.5f);
        path.lineTo(x + arrowH * 0.6f, halfH);
        path.lineTo(x, halfH + arrowH * 0.5f);

        g.strokePath(path, juce::PathStrokeType(1.5f));
    }

    r.removeFromRight(3);
    g.drawFittedText(text, r, juce::Justification::centredLeft, 1);

    if (shortcutKeyText.isNotEmpty())
    {
        auto f2 = font;
        f2.setHeight(f2.getHeight() * 0.8f);
        f2.setHorizontalScale(0.95f);
        g.setFont(f2);
        g.setColour(textColour.withMultipliedAlpha(0.6f));
        g.drawText(shortcutKeyText, r, juce::Justification::centredRight, true);
    }
}

void TramiLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                           const juce::Colour& backgroundColour,
                                           bool shouldDrawButtonAsHighlighted,
                                           bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    juce::Colour fill = backgroundColour;

    if (fill == Palette::surface || fill == Palette::surface.brighter(0.04f))
        fill = juce::Colour(0xff222832);

    if (shouldDrawButtonAsDown)
        fill = fill.darker(0.2f);
    else if (shouldDrawButtonAsHighlighted)
        fill = fill.brighter(0.18f);

    g.setColour(fill);
    g.fillRoundedRectangle(bounds, 4.0f);

    juce::Colour border = button.getToggleState()
                              ? (fill.getBrightness() > 0.4f ? fill.brighter(0.25f) : Palette::accent)
                              : (shouldDrawButtonAsHighlighted ? Palette::accent.withAlpha(0.6f) : juce::Colour(0xff3f4b5c));
    g.setColour(border);
    g.drawRoundedRectangle(bounds, 4.0f, button.getToggleState() ? 1.5f : 1.0f);
}

void TramiLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button,
                                      bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    const int h = button.getHeight();
    const int w = button.getWidth();
    float fontSz = (h <= 20 || w <= 26) ? 10.5f : 12.0f;
    auto font = juce::Font(Typography::bold(fontSz));

    const float textW = static_cast<float>(font.getStringWidth(button.getButtonText()));
    const float maxW = static_cast<float>(w) - 6.0f;
    if (textW > maxW && maxW > 10.0f)
    {
        fontSz = std::max(8.0f, fontSz * (maxW / textW));
        font = juce::Font(Typography::bold(fontSz));
    }
    g.setFont(font);

    auto textCol = button.findColour(button.getToggleState() ? juce::TextButton::textColourOnId
                                                            : juce::TextButton::textColourOffId);
    if (shouldDrawButtonAsHighlighted)
        textCol = textCol.brighter(0.2f);

    g.setColour(textCol.withMultipliedAlpha(button.isEnabled() ? 1.0f : 0.5f));
    auto r = button.getLocalBounds().toFloat();
    if (shouldDrawButtonAsDown)
        r.translate(0.0f, 1.0f);
    g.drawText(button.getButtonText(), r.toNearestInt(), juce::Justification::centred, false);
}

void TramiLookAndFeel::drawScrollbar(juce::Graphics& g, juce::ScrollBar& /*scrollbar*/,
                                     int x, int y, int width, int height,
                                     bool isScrollbarVertical, int thumbStartPosition, int thumbSize,
                                     bool isMouseOver, bool isMouseDown)
{
    g.fillAll(Palette::surface.darker(0.22f));

    if (thumbSize > 0)
    {
        juce::Rectangle<int> thumbBounds;
        if (isScrollbarVertical)
            thumbBounds = {x + 2, thumbStartPosition + 1, width - 4, thumbSize - 2};
        else
            thumbBounds = {thumbStartPosition + 1, y + 2, thumbSize - 2, height - 4};

        juce::Colour thumbCol = isMouseDown ? Palette::textDim.brighter(0.4f)
                              : isMouseOver ? Palette::textDim.brighter(0.2f)
                                            : Palette::textDim.withAlpha(0.5f);

        g.setColour(thumbCol);
        g.fillRoundedRectangle(thumbBounds.toFloat(), 3.0f);
    }
}

void TramiLookAndFeel::drawTooltip(juce::Graphics& g, const juce::String& text, int width, int height)
{
    const auto r = juce::Rectangle<float>(0.5f, 0.5f, static_cast<float>(width) - 1.0f, static_cast<float>(height) - 1.0f);
    g.setColour(findColour(juce::TooltipWindow::backgroundColourId));
    g.fillRoundedRectangle(r, 4.0f);

    g.setColour(findColour(juce::TooltipWindow::outlineColourId));
    g.drawRoundedRectangle(r, 4.0f, 1.0f);

    g.setColour(findColour(juce::TooltipWindow::textColourId));
    g.setFont(Typography::bold(11.5f));
    g.drawFittedText(text, 8, 2, width - 16, height - 4, juce::Justification::centred, 1);
}

juce::Rectangle<int> TramiLookAndFeel::getTooltipBounds(const juce::String& text, juce::Point<int> screenPos,
                                                       juce::Rectangle<int> parentArea)
{
    const juce::Font font(Typography::bold(11.5f));
    const int textW = juce::roundToInt(font.getStringWidthFloat(text));
    const int w = textW + 20;
    constexpr int h = 24;

    return juce::Rectangle<int>(screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 16,
                               screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 8)  : screenPos.y + 16,
                               w, h)
        .constrainedWithin(parentArea);
}

} // namespace trami
