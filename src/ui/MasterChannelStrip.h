#pragma once

#include "engine/AudioEngine.h"
#include "model/Song.h"
#include "ui/TramiLookAndFeel.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace trami
{

class MasterChannelStrip : public juce::Component, public juce::Timer
{
public:
    static constexpr int kWidth = 64;

    MasterChannelStrip(Song& song, AudioEngine& engine, std::function<void()> onModelModified);
    ~MasterChannelStrip() override;

    void updateFromModel();
    void setLevels(float peakL, float peakR, float rmsL, float rmsR, bool clipped);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

private:
    Song& song_;
    AudioEngine& engine_;
    std::function<void()> onModelModified_;

    juce::Label titleLabel_{"title", "MASTER"};
    juce::TextButton dbDisplayButton_{"0.0 dB"};

    class MasterFaderLookAndFeel : public TramiLookAndFeel
    {
    public:
        void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                              float sliderPos, float minSliderPos, float maxSliderPos,
                              const juce::Slider::SliderStyle style, juce::Slider& slider) override;
    };
    MasterFaderLookAndFeel faderLookAndFeel_;

    juce::Slider fader_{juce::Slider::LinearVertical, juce::Slider::NoTextBox};
    juce::TextButton zeroDbButton_{"0 dB"};
    juce::TextButton muteButton_{"M"};

    float displayPeakL_ = 0.0f;
    float displayPeakR_ = 0.0f;
    float displayRmsL_ = 0.0f;
    float displayRmsR_ = 0.0f;
    float peakHoldL_ = 0.0f;
    float peakHoldR_ = 0.0f;
    int peakHoldCounterL_ = 0;
    int peakHoldCounterR_ = 0;
    bool isClipped_ = false;
    float preMuteVolume_ = 1.0f;
};

} // namespace trami

