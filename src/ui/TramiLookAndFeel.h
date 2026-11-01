#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace trami
{

/// Central colour palette. All UI colours come from here.
namespace Palette
{
inline const juce::Colour background{0xff121417};
inline const juce::Colour surface{0xff1b1e23};
inline const juce::Colour gridLine{0xff2a2e35};
inline const juce::Colour text{0xffe6e8eb};
inline const juce::Colour textDim{0xff8a9099};
inline const juce::Colour accent{0xff4fc3f7};
} // namespace Palette

/// Minimal dark look-and-feel for the whole application.
class TramiLookAndFeel : public juce::LookAndFeel_V4
{
public:
    TramiLookAndFeel();
};

} // namespace trami
