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
}

juce::Font TramiLookAndFeel::getTextButtonFont(juce::TextButton&, int)
{
    return juce::Font(Typography::bold(12.5f));
}

juce::Font TramiLookAndFeel::getLabelFont(juce::Label&)
{
    return juce::Font(Typography::regular(13.0f));
}

juce::Font TramiLookAndFeel::getComboBoxFont(juce::ComboBox&)
{
    return juce::Font(Typography::regular(13.0f));
}

juce::Font TramiLookAndFeel::getPopupMenuFont()
{
    return juce::Font(Typography::regular(13.0f));
}

void TramiLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                           const juce::Colour& backgroundColour,
                                           bool shouldDrawButtonAsHighlighted,
                                           bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    juce::Colour fill = backgroundColour;

    if (shouldDrawButtonAsDown)
        fill = fill.darker(0.2f);
    else if (shouldDrawButtonAsHighlighted)
        fill = fill.brighter(0.15f);

    g.setColour(fill);
    g.fillRoundedRectangle(bounds, 4.0f);

    juce::Colour border = button.getToggleState() ? Palette::accent : Palette::gridLine;
    g.setColour(border);
    g.drawRoundedRectangle(bounds, 4.0f, button.getToggleState() ? 1.5f : 1.0f);
}

} // namespace trami
