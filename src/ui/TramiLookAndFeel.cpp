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
}

} // namespace trami
