#pragma once

#include "engine/AudioEngine.h"
#include "engine/StretchCache.h"
#include "model/Song.h"
#include "ui/PatternView.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace trami
{

/// Top-level application window.
class MainWindow : public juce::DocumentWindow
{
public:
    MainWindow(const juce::String& name, AudioEngine& engine, Song& song, StretchCache& stretchCache);

    void closeButtonPressed() override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
};

} // namespace trami
