#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace trami
{

/// Central colour palette. All UI colours come from here.
namespace Palette
{
inline const juce::Colour background{0xff1c1c1f};
inline const juce::Colour surface{0xff2a2a2f};
inline const juce::Colour panelBackground{0xff222226};
inline const juce::Colour panelHeader{0xff2f2f35};
inline const juce::Colour trackHeaderSurface{0xff28282d};
inline const juce::Colour topBarBackground{0xff2b2b30};
inline const juce::Colour transportBackground{0xff2b2b30};
inline const juce::Colour gridFloor{0xff252529};
inline const juce::Colour gridLaneAlt{0xff2b2b30};
inline const juce::Colour gridLine{0xff323238};
inline const juce::Colour gridBeatLine{0xff3a3a42};
inline const juce::Colour gridBarLine{0xff4a4a54};
inline const juce::Colour text{0xfff0f0f5};
inline const juce::Colour textDim{0xffa4a4b0}; // WCAG AA compliant contrast on dark panels
inline const juce::Colour accent{0xff5ac8fa};
inline const juce::Colour loopRegion{0xff5ac8fa};
inline const juce::Colour danger{0xffe53935};

inline const juce::Colour patternAccent{0xff5ac8fa};
inline const juce::Colour patternHeaderBg{0xff2d2d33};
inline const juce::Colour patternTrackSurface{0xff26262b};
inline const juce::Colour patternTrackAlt{0xff2c2c32};

inline const juce::Colour arrangerAccent{0xffb388ff};
inline const juce::Colour arrangerHeaderBg{0xff2a2634};
inline const juce::Colour arrangerTrackSurface{0xff25222e};
inline const juce::Colour arrangerTrackAlt{0xff2b2736};

inline const juce::Colour sidebarBackground{0xff1e1e22};
inline const juce::Colour sidebarHeader{0xff2a2a30};
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
    void getIdealPopupMenuItemSize(const juce::String& text, bool isSeparator, int standardMenuItemHeight,
                                   int& idealWidth, int& idealHeight) override;
    void drawPopupMenuBackground(juce::Graphics& g, int width, int height) override;
    void drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                            bool isSeparator, bool isActive, bool isHighlighted, bool isTicked,
                            bool hasSubMenu, const juce::String& text, const juce::String& shortcutKeyText,
                            const juce::Drawable* icon, const juce::Colour* textColour) override;

    void drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
    void drawButtonText(juce::Graphics& g, juce::TextButton& button, bool shouldDrawButtonAsHighlighted,
                        bool shouldDrawButtonAsDown) override;

    void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox& box) override;
    void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override;
    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                          bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    void drawScrollbar(juce::Graphics& g, juce::ScrollBar& scrollbar, int x, int y, int width, int height,
                       bool isScrollbarVertical, int thumbStartPosition, int thumbSize, bool isMouseOver,
                       bool isMouseDown) override;

    void drawTooltip(juce::Graphics& g, const juce::String& text, int width, int height) override;
    juce::Rectangle<int> getTooltipBounds(const juce::String& text, juce::Point<int> screenPos,
                                          juce::Rectangle<int> parentArea) override;
};

} // namespace trami
