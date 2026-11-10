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
inline const juce::Colour textDim{0xffa4b1c2}; // WCAG AA compliant contrast on dark panels
inline const juce::Colour accent{0xff4fc3f7};
inline const juce::Colour loopRegion{0xff42b4e6};
inline const juce::Colour danger{0xffe53935};

inline const juce::Colour patternAccent{0xff00b0ff};
inline const juce::Colour patternHeaderBg{0xff111823};
inline const juce::Colour patternTrackSurface{0xff151c27};
inline const juce::Colour patternTrackAlt{0xff182230};

inline const juce::Colour arrangerAccent{0xffb388ff};
inline const juce::Colour arrangerHeaderBg{0xff1a1426};
inline const juce::Colour arrangerTrackSurface{0xff1e172e};
inline const juce::Colour arrangerTrackAlt{0xff241b37};

inline const juce::Colour sidebarBackground{0xff0d1016};
inline const juce::Colour sidebarHeader{0xff161a22};
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

    void drawScrollbar(juce::Graphics& g, juce::ScrollBar& scrollbar, int x, int y, int width, int height,
                       bool isScrollbarVertical, int thumbStartPosition, int thumbSize, bool isMouseOver,
                       bool isMouseDown) override;

    void drawTooltip(juce::Graphics& g, const juce::String& text, int width, int height) override;
    juce::Rectangle<int> getTooltipBounds(const juce::String& text, juce::Point<int> screenPos,
                                          juce::Rectangle<int> parentArea) override;
};

} // namespace trami
