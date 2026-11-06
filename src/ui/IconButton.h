#pragma once

#include "ui/TramiLookAndFeel.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace trami
{

/// A modern button featuring a visual vector icon on top with a text label underneath.
class IconButton : public juce::Button
{
public:
    enum class Icon
    {
        Play,
        Stop,
        Record,
        Rewind,
        Metronome,
        Swing,
        Browser,
        PatternMode,
        ArrangerMode,
        Undo,
        Redo,
        Save,
        Open,
        Export,
        Settings,
        Plus,
        Duplicate,
        Delete,
        Edit,
        ZoomIn,
        ZoomOut,
        ZoomReset
    };

    IconButton(const juce::String& buttonName, Icon icon, const juce::String& label);

    void setIcon(Icon icon);
    Icon getIcon() const noexcept { return icon_; }

    void setLabel(const juce::String& label);
    const juce::String& getLabel() const noexcept { return label_; }

    void setActiveColour(juce::Colour colour);

    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

private:
    void drawIcon(juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour);

    Icon icon_;
    juce::String label_;
    juce::Colour activeColour_{Palette::accent};
};

} // namespace trami

