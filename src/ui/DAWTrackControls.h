#pragma once

#include "ui/TramiLookAndFeel.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace trami
{

class DAWMuteButton : public juce::Button
{
public:
    DAWMuteButton();
    ~DAWMuteButton() override = default;

    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
};

class DAWSoloButton : public juce::Button
{
public:
    DAWSoloButton();
    ~DAWSoloButton() override = default;

    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
};

class DAWArmButton : public juce::Button
{
public:
    DAWArmButton();
    ~DAWArmButton() override = default;

    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
};

/// Single-source recording input selector. Every hardware input is one entry in a flat list:
/// each mono channel ("Input 1") and each stereo pair ("Input 1+2") is its own source, recorded as-is.
class DAWInputButton : public juce::Component, public juce::SettableTooltipClient
{
public:
    DAWInputButton();
    ~DAWInputButton() override = default;

    void setInputState(int inputChannel, bool stereo, bool armed);
    int inputChannel() const noexcept { return inputChannel_; }
    bool isStereo() const noexcept { return stereo_; }
    bool isArmed() const noexcept { return armed_; }

    std::function<void()> onSelectInputChannel;

    void paint(juce::Graphics& g) override;
    void mouseEnter(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    void mouseDown(const juce::MouseEvent& e) override;

private:
    int inputChannel_ = 0;
    bool stereo_ = true;
    bool armed_ = false;
    bool hover_ = false;
};

/// Flat recording source list helpers shared by track headers, mixer strips and track properties.
namespace DAWInputSources
{
constexpr int kMonoBase = 100;
constexpr int kStereoBase = 200;

/// Display text, e.g. "Input 3 (Mono)" or "Inputs 3+4 (Stereo)".
juce::String sourceLabel(int channel, bool stereo, const juce::StringArray& deviceNames);

/// Adds every mono channel and every stereo pair as a separate entry to `menu` (ids kMonoBase+ch / kStereoBase+ch).
void populateMenu(juce::PopupMenu& menu, const juce::StringArray& deviceNames, int currentChannel, bool currentStereo);

/// Decodes a menu result id into (channel, stereo). Returns false if `id` is not a source id.
bool decode(int id, int& channel, bool& stereo);

/// Number of hardware channels to offer (at least 2, and always enough to contain the current selection).
int channelCount(const juce::StringArray& deviceNames, int currentChannel, bool currentStereo);
} // namespace DAWInputSources

namespace DAWTrackIcons
{
void drawTrackFoldChevron(juce::Graphics& g, float cx, float cy, float size = 7.0f);
void drawAudioWaveformIcon(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour colour = juce::Colour(0xff9aa0aa));
} // namespace DAWTrackIcons

namespace DAWLoopRuler
{
void drawStudioOneLoopIndicator(juce::Graphics& g, int loopStartX, int loopEndX, int rulerHeight, bool active = true);
} // namespace DAWLoopRuler

} // namespace trami

