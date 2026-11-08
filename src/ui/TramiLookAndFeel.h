#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace trami
{

/// Central colour palette. All UI colours come from here.
namespace Palette
{
inline const juce::Colour background{0xff0e1116};
inline const juce::Colour surface{0xff181d24};
inline const juce::Colour panelBackground{0xff13171e};
inline const juce::Colour panelHeader{0xff1c222c};
inline const juce::Colour trackHeaderSurface{0xff1a1f28};
inline const juce::Colour topBarBackground{0xff0c0e13};
inline const juce::Colour transportBackground{0xff14171e};
inline const juce::Colour gridFloor{0xff0b0d11};
inline const juce::Colour gridLaneAlt{0xff101318};
inline const juce::Colour gridLine{0xff28313d};
inline const juce::Colour gridBeatLine{0xff3e4c5e};
inline const juce::Colour gridBarLine{0xff647792};
inline const juce::Colour text{0xffe6eaf0};
inline const juce::Colour textDim{0xff8c96a5};
inline const juce::Colour accent{0xff4fc3f7};
inline const juce::Colour loopRegion{0xfff59e0b};
} // namespace Palette

/// Consistent, modern, readable typography.
namespace Typography
{
inline juce::FontOptions regular(float size = 13.5f)
{
    return juce::FontOptions(juce::Font::getDefaultSansSerifFontName(), size, juce::Font::plain);
}

inline juce::FontOptions bold(float size = 13.5f)
{
    return juce::FontOptions(juce::Font::getDefaultSansSerifFontName(), size, juce::Font::bold);
}
} // namespace Typography

/// Minimal dark look-and-feel for the whole application.
class TramiLookAndFeel : public juce::LookAndFeel_V4
{
public:
    TramiLookAndFeel();

    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override;
    juce::Font getLabelFont(juce::Label&) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;

    void drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
};

} // namespace trami
