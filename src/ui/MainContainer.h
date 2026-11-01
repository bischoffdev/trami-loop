#pragma once

#include "engine/AudioEngine.h"
#include "engine/StretchCache.h"
#include "model/Song.h"
#include "ui/ArrangerView.h"
#include "ui/PatternView.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace trami
{

enum class ViewMode
{
    Pattern,
    Arranger
};

class MainContainer : public juce::Component
{
public:
    MainContainer(Song& song, AudioEngine& engine, StretchCache& stretchCache);

    void setViewMode(ViewMode mode);
    void editPattern(PatternId patternId);

    void paint(juce::Graphics& g) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;

private:
    Song& song_;
    AudioEngine& engine_;
    StretchCache& stretchCache_;

    ViewMode currentMode_ = ViewMode::Pattern;

    juce::TextButton patternModeButton_{"Pattern Editor"};
    juce::TextButton arrangerModeButton_{"Arranger / Song"};

    std::unique_ptr<PatternView> patternView_;
    std::unique_ptr<ArrangerView> arrangerView_;
};

} // namespace trami
