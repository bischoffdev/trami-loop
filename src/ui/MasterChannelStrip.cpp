#include "ui/MasterChannelStrip.h"

#include "ui/DAWTrackControls.h"

#include <algorithm>
#include <cmath>

namespace trami
{

// ==============================================================================
// MasterFaderLookAndFeel
// ==============================================================================
void MasterChannelStrip::MasterFaderLookAndFeel::drawLinearSlider(
    juce::Graphics& g, int x, int y, int width, int height,
    float sliderPos, float /*minSliderPos*/, float /*maxSliderPos*/,
    const juce::Slider::SliderStyle /*style*/, juce::Slider& slider)
{
    const float trackW = 4.0f;
    const float trackX = static_cast<float>(x) + (static_cast<float>(width) - trackW) * 0.5f;
    const float trackY = static_cast<float>(y);
    const float trackH = static_cast<float>(height);

    // Recessed dark groove track
    auto trackRect = juce::Rectangle<float>(trackX, trackY, trackW, trackH);
    g.setColour(juce::Colour(0xff101318));
    g.fillRoundedRectangle(trackRect, 2.0f);
    g.setColour(juce::Colour(0xff090c0f));
    g.drawRoundedRectangle(trackRect, 2.0f, 1.0f);

    // Unity gain tick mark (0.0 dB = 1.0 in slider range 0.0..2.0)
    const float zeroY = static_cast<float>(slider.getPositionOfValue(1.0));
    g.setColour(juce::Colour(0xffffc107).withAlpha(0.7f));
    g.drawHorizontalLine(juce::roundToInt(zeroY), trackX - 4.0f, trackX + trackW + 4.0f);

    // Fader thumb handle (silver/white rectangular fader cap with center horizontal notch)
    constexpr float thumbW = 22.0f;
    constexpr float thumbH = 12.0f;
    const float thumbX = static_cast<float>(x) + (static_cast<float>(width) - thumbW) * 0.5f;
    const float thumbY = std::clamp(sliderPos - thumbH * 0.5f, trackY, trackY + trackH - thumbH);
    auto thumbRect = juce::Rectangle<float>(thumbX, thumbY, thumbW, thumbH);

    const bool isDown = slider.isMouseButtonDown();
    const bool isHover = slider.isMouseOverOrDragging();

    // Metallic silver/white gradient
    juce::Colour topCol = isDown ? juce::Colour(0xffd5dbe2)
                        : isHover ? juce::Colour(0xffffffff)
                        : juce::Colour(0xfff0f4f8);
    juce::Colour botCol = isDown ? juce::Colour(0xffa8b0bb)
                        : isHover ? juce::Colour(0xffcdd4dc)
                        : juce::Colour(0xffbec5cd);

    juce::ColourGradient grad(topCol, thumbRect.getX(), thumbRect.getY(),
                              botCol, thumbRect.getX(), thumbRect.getBottom(), false);
    g.setGradientFill(grad);
    g.fillRoundedRectangle(thumbRect, 1.5f);

    // Dark outline
    g.setColour(juce::Colour(0xff323842));
    g.drawRoundedRectangle(thumbRect, 1.5f, 1.0f);

    // Top highlight line
    g.setColour(juce::Colours::white.withAlpha(0.8f));
    g.drawHorizontalLine(juce::roundToInt(thumbRect.getY() + 1.0f), thumbRect.getX() + 1.5f, thumbRect.getRight() - 1.5f);

    // Center horizontal groove line (notch)
    const float cy = thumbRect.getCentreY();
    g.setColour(juce::Colour(0xff4e5560));
    g.drawHorizontalLine(juce::roundToInt(cy) - 1, thumbRect.getX() + 2.0f, thumbRect.getRight() - 2.0f);
    g.setColour(juce::Colours::white.withAlpha(0.65f));
    g.drawHorizontalLine(juce::roundToInt(cy), thumbRect.getX() + 2.0f, thumbRect.getRight() - 2.0f);
}

// ==============================================================================
// MasterChannelStrip
// ==============================================================================
MasterChannelStrip::MasterChannelStrip(Song& song, AudioEngine& engine, std::function<void()> onModelModified)
    : song_(song), engine_(engine), onModelModified_(std::move(onModelModified))
{
    titleLabel_.setFont(Typography::bold(12.0f));
    titleLabel_.setColour(juce::Label::textColourId, juce::Colour(0xffffca28));
    titleLabel_.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(titleLabel_);

    dbDisplayButton_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff14171d));
    dbDisplayButton_.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffdbe2eb));
    dbDisplayButton_.setTooltip("Master output level & clip indicator. Click to reset clip; double-click to reset volume to 0.0 dB.");
    dbDisplayButton_.onClick = [this]() {
        engine_.sequencer().resetClipped();
        isClipped_ = false;
        dbDisplayButton_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff14171d));
        dbDisplayButton_.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffdbe2eb));
        repaint();
    };
    addAndMakeVisible(dbDisplayButton_);

    const auto updateFaderTooltip = [this](float val) {
        juce::String text;
        if (val <= 0.0001f)
            text = "-inf dB (Muted)";
        else
        {
            const float db = 20.0f * std::log10(val);
            text = juce::String::formatted("%+.1f dB", db);
            if (std::abs(val - 1.0f) < 0.01f)
                text += " (Unity)";
        }
        fader_.setTooltip("Master Volume: " + text + "\nDouble-click to reset to 0.0 dB");
    };

    fader_.setLookAndFeel(&faderLookAndFeel_);
    fader_.setRange(0.0, 2.0, 0.01);
    fader_.setSkewFactorFromMidPoint(1.0); // center unity 1.0 (0 dB)
    fader_.setValue(song_.masterVolume(), juce::dontSendNotification);
    fader_.setDoubleClickReturnValue(true, 1.0);
    updateFaderTooltip(song_.masterVolume());

    fader_.onValueChange = [this, updateFaderTooltip]() {
        const float val = static_cast<float>(fader_.getValue());
        song_.setMasterVolume(val);
        engine_.sequencer().setMasterVolume(val);
        if (muteButton_.getToggleState() && val > 0.001f)
            muteButton_.setToggleState(false, juce::dontSendNotification);
        updateFaderTooltip(val);
    };
    fader_.onDragEnd = [this]() {
        if (onModelModified_)
            onModelModified_();
    };
    addAndMakeVisible(fader_);

    zeroDbButton_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff252932));
    zeroDbButton_.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffb8c0cc));
    zeroDbButton_.setTooltip("Reset Master Volume to 0.0 dB (Unity)");
    zeroDbButton_.onClick = [this]() {
        fader_.setValue(1.0, juce::sendNotificationSync);
        if (onModelModified_)
            onModelModified_();
    };
    addAndMakeVisible(zeroDbButton_);

    muteButton_.setClickingTogglesState(true);
    muteButton_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff252932));
    muteButton_.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffb8c0cc));
    muteButton_.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffd32f2f));
    muteButton_.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    muteButton_.setTooltip("Mute Master Output");
    muteButton_.onClick = [this]() {
        if (muteButton_.getToggleState())
        {
            preMuteVolume_ = static_cast<float>(fader_.getValue());
            fader_.setValue(0.0, juce::sendNotificationSync);
        }
        else
        {
            fader_.setValue(preMuteVolume_ > 0.001f ? preMuteVolume_ : 1.0f, juce::sendNotificationSync);
        }
        if (onModelModified_)
            onModelModified_();
    };
    addAndMakeVisible(muteButton_);

    startTimerHz(60);
}

MasterChannelStrip::~MasterChannelStrip()
{
    stopTimer();
    fader_.setLookAndFeel(nullptr);
}

void MasterChannelStrip::updateFromModel()
{
    fader_.setValue(song_.masterVolume(), juce::dontSendNotification);
    if (song_.masterVolume() < 0.001f)
        muteButton_.setToggleState(true, juce::dontSendNotification);
    else
        muteButton_.setToggleState(false, juce::dontSendNotification);
    repaint();
}

void MasterChannelStrip::setLevels(float peakL, float peakR, float rmsL, float rmsR, bool clipped)
{
    constexpr float decay = 0.90f;
    displayPeakL_ = std::max(peakL, displayPeakL_ * decay);
    displayPeakR_ = std::max(peakR, displayPeakR_ * decay);
    displayRmsL_ = std::max(rmsL, displayRmsL_ * decay);
    displayRmsR_ = std::max(rmsR, displayRmsR_ * decay);

    if (clipped)
        isClipped_ = true;

    if (displayPeakL_ > peakHoldL_)
    {
        peakHoldL_ = displayPeakL_;
        peakHoldCounterL_ = 36;
    }
    else if (peakHoldCounterL_ > 0)
    {
        --peakHoldCounterL_;
    }
    else
    {
        peakHoldL_ = std::max(displayPeakL_, peakHoldL_ * decay);
    }

    if (displayPeakR_ > peakHoldR_)
    {
        peakHoldR_ = displayPeakR_;
        peakHoldCounterR_ = 36;
    }
    else if (peakHoldCounterR_ > 0)
    {
        --peakHoldCounterR_;
    }
    else
    {
        peakHoldR_ = std::max(displayPeakR_, peakHoldR_ * decay);
    }

    // Update readout text
    if (isClipped_)
    {
        dbDisplayButton_.setButtonText("CLIP");
        dbDisplayButton_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffd32f2f));
        dbDisplayButton_.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    }
    else
    {
        const float maxP = std::max(displayPeakL_, displayPeakR_);
        if (maxP <= 0.0001f)
            dbDisplayButton_.setButtonText("-inf");
        else
        {
            const float db = 20.0f * std::log10(maxP);
            dbDisplayButton_.setButtonText(juce::String::formatted("%+.1f dB", db));
        }
        dbDisplayButton_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff14171d));
        dbDisplayButton_.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffdbe2eb));
    }

    repaint();
}

void MasterChannelStrip::timerCallback()
{
    const auto levels = engine_.sequencer().outputLevels();
    setLevels(levels.peakL, levels.peakR, levels.rmsL, levels.rmsR, levels.clipped);
}

void MasterChannelStrip::paint(juce::Graphics& g)
{
    // Background of master channel strip
    g.fillAll(juce::Colour(0xff181b22));

    // Left vertical border separating from main workspace
    g.setColour(juce::Colour(0xff2a303c));
    g.drawVerticalLine(0, 0.0f, static_cast<float>(getHeight()));

    // Divider under top section
    constexpr int topH = 44;
    g.setColour(juce::Colour(0xff222730));
    g.drawHorizontalLine(topH, 0.0f, static_cast<float>(getWidth()));

    // Divider above bottom section
    const int botY = getHeight() - 44;
    g.drawHorizontalLine(botY, 0.0f, static_cast<float>(getWidth()));

    // Meters area (left side of middle section)
    const int meterX = 6;
    const int meterY = topH + 8;
    const int meterH = botY - meterY - 8;
    constexpr int chW = 6;
    constexpr int chGap = 2;

    if (meterH <= 20)
        return;

    // Channel labels L and R above meter
    g.setFont(Typography::bold(8.0f));
    g.setColour(juce::Colour(0xff8a94a6));
    g.drawText("L", meterX, meterY - 8, chW, 8, juce::Justification::centred);
    g.drawText("R", meterX + chW + chGap, meterY - 8, chW, 8, juce::Justification::centred);

    // Recessed meter background
    auto meterBg = juce::Rectangle<float>(static_cast<float>(meterX), static_cast<float>(meterY),
                                          static_cast<float>(chW * 2 + chGap), static_cast<float>(meterH));
    g.setColour(juce::Colour(0xff101318));
    g.fillRoundedRectangle(meterBg, 2.0f);
    g.setColour(juce::Colour(0xff090c0f));
    g.drawRoundedRectangle(meterBg, 2.0f, 1.0f);

    // Range: -60 dB to +6 dB (total 66 dB)
    constexpr float minDb = -60.0f;
    constexpr float maxDb = 6.0f;
    const auto dbToNorm = [&](float gain) {
        if (gain <= 0.00005f) return 0.0f;
        const float db = 20.0f * std::log10(gain);
        return std::clamp((db - minDb) / (maxDb - minDb), 0.0f, 1.0f);
    };

    const float normL = dbToNorm(displayPeakL_);
    const float normR = dbToNorm(displayPeakR_);

    // Draw segmented LEDs for L and R channels
    const int numSegs = std::max(16, meterH / 4);
    const float segH = (static_cast<float>(meterH) - static_cast<float>(numSegs - 1)) / static_cast<float>(numSegs);

    for (int ch = 0; ch < 2; ++ch)
    {
        const float cx = static_cast<float>(meterX) + (ch == 0 ? 0.0f : static_cast<float>(chW + chGap));
        const float norm = (ch == 0 ? normL : normR);
        const int activeSegs = juce::roundToInt(norm * static_cast<float>(numSegs));

        for (int i = 0; i < numSegs; ++i)
        {
            // Bottom-up: i = 0 is lowest dB, i = numSegs - 1 is highest (+6 dB)
            const float sy = static_cast<float>(meterY + meterH) - static_cast<float>(i + 1) * (segH + 1.0f);
            const auto segRect = juce::Rectangle<float>(cx, sy, static_cast<float>(chW), segH);

            const float segRatio = static_cast<float>(i + 1) / static_cast<float>(numSegs);
            const float segDb = minDb + segRatio * (maxDb - minDb);

            const bool isLit = (i < activeSegs);
            juce::Colour col;

            if (segDb > 0.0f)
            {
                col = isLit ? (isClipped_ ? juce::Colour(0xffff1744) : juce::Colour(0xffff3344))
                            : juce::Colour(0xff2d0b0f);
            }
            else if (segDb >= -6.0f)
            {
                col = isLit ? juce::Colour(0xffffb300) : juce::Colour(0xff2a2007);
            }
            else
            {
                col = isLit ? juce::Colour(0xff00d455) : juce::Colour(0xff0b2414);
            }

            g.setColour(col);
            g.fillRect(segRect);
        }

        // Peak Hold line
        const float holdGain = (ch == 0 ? peakHoldL_ : peakHoldR_);
        if (holdGain > 0.0001f)
        {
            const float holdNorm = dbToNorm(holdGain);
            const float holdY = static_cast<float>(meterY + meterH) - holdNorm * static_cast<float>(meterH);
            const float holdDb = 20.0f * std::log10(holdGain);
            g.setColour(holdDb >= 0.0f ? juce::Colour(0xffff5252) : juce::Colour(0xffffffff));
            g.drawHorizontalLine(juce::roundToInt(holdY), cx, cx + static_cast<float>(chW));
        }
    }

    // Scale markings (-24, -12, -6 dB ticks)
    for (float dbVal : {-24.0f, -12.0f, -6.0f})
    {
        const float norm = (dbVal - minDb) / (maxDb - minDb);
        const float tickY = static_cast<float>(meterY + meterH) - norm * static_cast<float>(meterH);
        g.setColour(juce::Colour(0xff4b5360));
        g.drawHorizontalLine(juce::roundToInt(tickY), static_cast<float>(meterX + chW * 2 + chGap + 1),
                             static_cast<float>(meterX + chW * 2 + chGap + 3));
    }

    // 0 dB line and label
    const float zeroNorm = (0.0f - minDb) / (maxDb - minDb);
    const float zeroY = static_cast<float>(meterY + meterH) - zeroNorm * static_cast<float>(meterH);
    g.setColour(juce::Colour(0xffffc107).withAlpha(0.8f));
    g.drawHorizontalLine(juce::roundToInt(zeroY), static_cast<float>(meterX),
                         static_cast<float>(meterX + chW * 2 + chGap + 3));

    // Scale dB label next to 0 dB
    g.setFont(Typography::bold(8.5f));
    g.setColour(juce::Colour(0xffffc107));
    g.drawText("0", meterX + chW * 2 + chGap + 4, juce::roundToInt(zeroY) - 5, 8, 10, juce::Justification::centredLeft);
}

void MasterChannelStrip::resized()
{
    constexpr int topH = 44;
    titleLabel_.setBounds(2, 2, getWidth() - 4, 16);
    dbDisplayButton_.setBounds(4, 20, getWidth() - 8, 18);

    const int botY = getHeight() - 44;
    const int meterY = topH + 8;
    const int midH = botY - meterY - 8;

    // Fader occupies the right portion of the middle section
    constexpr int faderW = 30;
    const int faderX = getWidth() - faderW - 2;
    fader_.setBounds(faderX, meterY, faderW, midH);

    // Bottom controls
    const int btnW = (getWidth() - 10) / 2;
    zeroDbButton_.setBounds(4, botY + 10, btnW, 24);
    muteButton_.setBounds(4 + btnW + 2, botY + 10, btnW, 24);
}

} // namespace trami
