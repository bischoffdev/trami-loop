#pragma once

#include "engine/AudioEngine.h"
#include "model/Song.h"
#include "ui/TramiLookAndFeel.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace trami
{

class TransportBar : public juce::Component, private juce::Timer
{
public:
    using ActionCallback = std::function<void()>;

    TransportBar(Song& song, AudioEngine& engine,
                 ActionCallback onModelModified,
                 ActionCallback onRecordToggle);
    ~TransportBar() override;

    void setLooping(bool looping);
    bool isLooping() const noexcept { return isLooping_; }

    void setLoopRange(int start, int end, bool isBars);
    void setOnLoopRangeChanged(std::function<void(int start, int end)> callback) { onLoopRangeChanged_ = std::move(callback); }
    void setOnShowToast(std::function<void(const juce::String&)> cb) { onShowToast_ = std::move(cb); }
    void setOnSwingChanged(std::function<void(double)> cb) { onSwingChanged_ = std::move(cb); }
    void setOnTimeSignatureChanged(std::function<void()> cb) { onTimeSignatureChanged_ = std::move(cb); }

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;

    void updateDisplay();

    static constexpr int kHeight = 48;

private:
    enum class TransportIcon
    {
        Record,
        Play,
        Rewind,
        Stop,
        Loop,
        CountIn,
        Metronome,
        Groove
    };

    class TransportButton : public juce::Button
    {
    public:
        TransportButton(TransportIcon icon, const juce::String& tooltipText = {});
        void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

        TransportIcon icon() const noexcept { return icon_; }
        void setBorderSides(bool left, bool right) { borderLeft_ = left; borderRight_ = right; }

    private:
        TransportIcon icon_;
        bool borderLeft_ = false;
        bool borderRight_ = false;
    };

    class StepperButton : public juce::Button
    {
    public:
        explicit StepperButton(bool isUp);
        void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    private:
        bool isUp_;
    };

    class TimeSigButton : public juce::Button
    {
    public:
        TimeSigButton();
        void setTimeSignature(int numerator, int denominator);
        void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    private:
        int numerator_ = 4;
        int denominator_ = 4;
    };

    void timerCallback() override;

    Song& song_;
    AudioEngine& engine_;
    ActionCallback onModelModified_;
    ActionCallback onRecordToggle_;

    bool isLooping_ = false;
    bool isCountIn_ = false;
    bool showSeconds_ = false;

    int loopStart_ = 0;
    int loopEnd_ = 4;
    bool loopIsBars_ = true;
    std::function<void(int start, int end)> onLoopRangeChanged_;
    std::function<void(const juce::String&)> onShowToast_;
    std::function<void(double)> onSwingChanged_;
    std::function<void()> onTimeSignatureChanged_;

    // Transport buttons cluster
    TransportButton recordButton_{TransportIcon::Record, "Record (R)"};
    TransportButton playButton_{TransportIcon::Play, "Play / Pause (Space)"};
    TransportButton rewindButton_{TransportIcon::Rewind, "Return to Zero / Rewind (Return / Home)"};
    TransportButton stopButton_{TransportIcon::Stop, "Stop Playback (Space / 0)"};
    TransportButton loopButton_{TransportIcon::Loop, "Toggle Loop / Cycle (/)"};
    TransportButton countInButton_{TransportIcon::CountIn, "Toggle Pre-count / Count-in (1 Bar)"};

    // Position display
    juce::Label positionLabel_;

    // Mode button
    juce::TextButton modeButton_{"Measure"};

    // Tempo cluster
    juce::Label tempoLabel_;
    StepperButton tempoUp_{true};
    StepperButton tempoDown_{false};

    // Metronome
    TransportButton metroButton_{TransportIcon::Metronome, "Toggle Metronome Click (C)"};

    // Time Signature button
    TimeSigButton timeSigButton_;

    // Swing cluster
    class SwingLabel;
    std::unique_ptr<SwingLabel> swingLabel_;
    class SwingSlider;
    std::unique_ptr<SwingSlider> swingSlider_;

    // Master Project Key selector
    juce::ComboBox keyCombo_;

    // Real-time System / Audio Performance indicator (CPU & Voice/Disk load bars)
    class PerformanceIndicator;
    std::unique_ptr<PerformanceIndicator> performanceIndicator_;
};

} // namespace trami
