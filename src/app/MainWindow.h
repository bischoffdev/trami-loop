#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace trami
{

class AudioEngine;

/// Top-level application window.
class MainWindow : public juce::DocumentWindow
{
public:
    MainWindow(const juce::String& name, AudioEngine& engine);

    void closeButtonPressed() override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
};

} // namespace trami
