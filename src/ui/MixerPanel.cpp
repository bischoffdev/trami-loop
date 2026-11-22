#include "ui/MixerPanel.h"

#include "ui/TramiLookAndFeel.h"

#include <algorithm>
#include <cmath>

namespace trami
{

namespace
{

juce::String formatDb(float gain)
{
    if (gain <= 0.0005f)
        return "-inf";
    const float db = 20.0f * std::log10(gain);
    if (std::abs(db) < 0.05f)
        return "0.0dB";
    return juce::String::formatted("%+.1fdB", db);
}

juce::String formatPan(float pan)
{
    if (std::abs(pan) < 0.02f)
        return "C";
    const int pct = juce::roundToInt(std::abs(pan) * 100.0f);
    return (pan < 0.0f ? "L" : "R") + juce::String(pct);
}

float parseDbText(const juce::String& text, float fallback)
{
    const juce::String raw = text.trim().toLowerCase();
    if (raw == "-inf" || raw == "inf" || raw == "mute" || raw == "off")
        return 0.0f;
    if (raw.endsWith("%"))
        return std::clamp(static_cast<float>(raw.dropLastCharacters(1).trim().getDoubleValue() / 100.0), 0.0f, 2.0f);
    if (raw.isNotEmpty())
    {
        juce::String cleaned = raw.endsWith("db") ? raw.dropLastCharacters(2).trim() : raw;
        const double db = cleaned.getDoubleValue();
        return (db <= -60.0) ? 0.0f : std::clamp(static_cast<float>(std::pow(10.0, db / 20.0)), 0.0f, 2.0f);
    }
    return fallback;
}

juce::Colour trackAccentColour(const PatternTrack& trk)
{
    if (trk.colorArgb != 0)
        return juce::Colour(trk.colorArgb);

    static constexpr std::uint32_t kModernPalette[] = {
        0xff63d988, 0xffb4a2f0, 0xff72c4fa, 0xfff2bf66, 0xfff294ab,
        0xff6ad8cc, 0xff9d94f2, 0xffa2dc6d, 0xffefa274, 0xff85b8f8
    };
    constexpr std::size_t kPaletteCount = sizeof(kModernPalette) / sizeof(kModernPalette[0]);

    SampleId sid = trk.sampleId;
    if (!sid.isValid() && !trk.clips.empty())
        sid = trk.clips.front().sampleId;
    if (sid.isValid())
        return juce::Colour(kModernPalette[(sid.value - 1) % kPaletteCount]);
    return Palette::accent;
}

class MixerFxButton final : public juce::TextButton
{
public:
    std::function<void()> onBypassToggle;
    void mouseDown(const juce::MouseEvent& e) override
    {
        if ((e.mods.isPopupMenu() || e.mods.isAltDown()) && onBypassToggle)
        {
            onBypassToggle();
            return;
        }
        juce::TextButton::mouseDown(e);
    }
};

void drawVerticalStereoMeter(juce::Graphics& g, juce::Rectangle<float> area, float peakL, float peakR)
{
    g.setColour(juce::Colour(0xff111115));
    g.fillRoundedRectangle(area, 2.0f);

    const float pad = 1.5f;
    const float gap = 1.5f;
    const float chW = std::max(2.0f, (area.getWidth() - pad * 2.0f - gap) * 0.5f);
    const float innerH = std::max(1.0f, area.getHeight() - pad * 2.0f);
    const float bottomY = area.getBottom() - pad;

    auto drawChan = [&](float cx, float level)
    {
        if (level <= 0.001f)
            return;
        const float db = 20.0f * std::log10(std::max(1e-5f, level));
        const float norm = std::clamp((db + 48.0f) / 51.0f, 0.0f, 1.0f);
        const float fillH = innerH * norm;
        if (fillH < 1.0f)
            return;

        juce::ColourGradient grad(juce::Colour(0xff22c55e), cx, bottomY,
                                  juce::Colour(0xffef4444), cx, area.getY() + pad, false);
        grad.addColour(0.68, juce::Colour(0xff84cc16));
        grad.addColour(0.84, juce::Colour(0xfff59e0b));
        g.setGradientFill(grad);
        g.fillRect(cx, bottomY - fillH, chW, fillH);
    };

    drawChan(area.getX() + pad, peakL);
    drawChan(area.getX() + pad + chW + gap, peakR);
}

} // namespace

// ============================================================================
// Track Channel Strip
// ============================================================================
class MixerPanel::ChannelStrip final : public juce::Component
{
public:
    ChannelStrip(MixerPanel& owner, int trackIndex)
        : owner_(owner), trackIndex_(trackIndex)
    {
        fxButton_.setButtonText("FX");
        fxButton_.onClick = [this]()
        {
            if (owner_.onTrackFxClicked)
                owner_.onTrackFxClicked(trackIndex_);
        };
        fxButton_.onBypassToggle = [this]()
        {
            if (owner_.onTrackFxBypass)
                owner_.onTrackFxBypass(trackIndex_);
        };
        addAndMakeVisible(fxButton_);

        muteButton_.setTooltip("Mute Track");
        muteButton_.onClick = [this]()
        {
            if (auto* pat = owner_.song_.findPattern(owner_.patternId_))
            {
                if (trackIndex_ >= 0 && trackIndex_ < static_cast<int>(pat->tracks.size()))
                    pat->tracks[static_cast<std::size_t>(trackIndex_)].muted = muteButton_.getToggleState();
            }
            if (owner_.onTrackMixChanged)
                owner_.onTrackMixChanged(trackIndex_);
        };
        addAndMakeVisible(muteButton_);

        soloButton_.setTooltip("Solo Track");
        soloButton_.onClick = [this]()
        {
            if (auto* pat = owner_.song_.findPattern(owner_.patternId_))
            {
                if (trackIndex_ >= 0 && trackIndex_ < static_cast<int>(pat->tracks.size()))
                    pat->tracks[static_cast<std::size_t>(trackIndex_)].solo = soloButton_.getToggleState();
            }
            if (owner_.onTrackMixChanged)
                owner_.onTrackMixChanged(trackIndex_);
        };
        addAndMakeVisible(soloButton_);

        panSlider_.setSliderStyle(juce::Slider::LinearHorizontal);
        panSlider_.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        panSlider_.setRange(-1.0, 1.0, 0.01);
        panSlider_.setDoubleClickReturnValue(true, 0.0);
        panSlider_.setScrollWheelEnabled(false);
        panSlider_.setTooltip("Track Pan (Double-click to center)");
        panSlider_.onValueChange = [this]()
        {
            const float pan = static_cast<float>(panSlider_.getValue());
            if (auto* pat = owner_.song_.findPattern(owner_.patternId_))
            {
                if (trackIndex_ >= 0 && trackIndex_ < static_cast<int>(pat->tracks.size()))
                    pat->tracks[static_cast<std::size_t>(trackIndex_)].mix.pan = pan;
            }
            panLabel_.setText(formatPan(pan), juce::dontSendNotification);
            if (owner_.onTrackMixChanged)
                owner_.onTrackMixChanged(trackIndex_);
        };
        addAndMakeVisible(panSlider_);

        panLabel_.setFont(Typography::bold(9.0f));
        panLabel_.setColour(juce::Label::textColourId, Palette::textDim);
        panLabel_.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(panLabel_);

        fader_.setSliderStyle(juce::Slider::LinearVertical);
        fader_.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        fader_.setRange(0.0, 2.0, 0.01);
        fader_.setDoubleClickReturnValue(true, 1.0);
        fader_.setScrollWheelEnabled(false);
        fader_.setTooltip("Track Volume (Double-click to reset to 0.0 dB)");
        fader_.onValueChange = [this]()
        {
            const float vol = static_cast<float>(fader_.getValue());
            if (auto* pat = owner_.song_.findPattern(owner_.patternId_))
            {
                if (trackIndex_ >= 0 && trackIndex_ < static_cast<int>(pat->tracks.size()))
                    pat->tracks[static_cast<std::size_t>(trackIndex_)].mix.volume = vol;
            }
            volLabel_.setText(formatDb(vol), juce::dontSendNotification);
            if (owner_.onTrackMixChanged)
                owner_.onTrackMixChanged(trackIndex_);
        };
        addAndMakeVisible(fader_);

        volLabel_.setFont(Typography::bold(9.5f));
        volLabel_.setBorderSize(juce::BorderSize<int>(1, 2, 1, 2));
        volLabel_.setColour(juce::Label::backgroundColourId, juce::Colour(0xff16161b));
        volLabel_.setColour(juce::Label::outlineColourId, juce::Colour(0xff32323c));
        volLabel_.setColour(juce::Label::textColourId, juce::Colour(0xffd8d8e4));
        volLabel_.setJustificationType(juce::Justification::centred);
        volLabel_.setEditable(true, true, false);
        volLabel_.setTooltip("Track Volume (Click to type dB)");
        volLabel_.onTextChange = [this]()
        {
            auto* pat = owner_.song_.findPattern(owner_.patternId_);
            if (pat == nullptr || trackIndex_ < 0 || trackIndex_ >= static_cast<int>(pat->tracks.size()))
                return;
            auto& trk = pat->tracks[static_cast<std::size_t>(trackIndex_)];
            const float vol = parseDbText(volLabel_.getText(), trk.mix.volume);
            trk.mix.volume = vol;
            fader_.setValue(vol, juce::dontSendNotification);
            volLabel_.setText(formatDb(vol), juce::dontSendNotification);
            if (owner_.onTrackMixChanged)
                owner_.onTrackMixChanged(trackIndex_);
        };
        addAndMakeVisible(volLabel_);

        syncFromModel();
        addMouseListener(this, true);
    }

    ~ChannelStrip() override
    {
        removeMouseListener(this);
    }

    int trackIndex() const noexcept { return trackIndex_; }

    void setSelected(bool sel)
    {
        if (selected_ != sel)
        {
            selected_ = sel;
            repaint();
        }
    }

    void setMeterLevels(float l, float r)
    {
        const float nextL = std::max(l, peakL_ * 0.80f);
        const float nextR = std::max(r, peakR_ * 0.80f);
        if (std::abs(nextL - peakL_) > 0.002f || std::abs(nextR - peakR_) > 0.002f)
        {
            peakL_ = nextL < 0.001f ? 0.0f : nextL;
            peakR_ = nextR < 0.001f ? 0.0f : nextR;
            repaint(meterRect_.toNearestInt().expanded(2));
        }
    }

    void syncFromModel()
    {
        const auto* pat = owner_.song_.findPattern(owner_.patternId_);
        if (pat == nullptr || trackIndex_ < 0 || trackIndex_ >= static_cast<int>(pat->tracks.size()))
            return;

        const auto& trk = pat->tracks[static_cast<std::size_t>(trackIndex_)];
        accent_ = trackAccentColour(trk);

        trackName_ = trk.name.empty() ? juce::String() : juce::String::fromUTF8(trk.name.c_str());
        if (trackName_.isEmpty())
        {
            SampleId sid = trk.sampleId;
            if (!sid.isValid() && !trk.clips.empty())
                sid = trk.clips.front().sampleId;
            if (const Sample* s = owner_.song_.findSample(sid))
                trackName_ = juce::String::fromUTF8(s->name.c_str());
            else
                trackName_ = "Track " + juce::String(trackIndex_ + 1);
        }

        muteButton_.setToggleState(trk.muted, juce::dontSendNotification);
        soloButton_.setToggleState(trk.solo, juce::dontSendNotification);

        if (!panSlider_.isMouseButtonDown())
            panSlider_.setValue(trk.mix.pan, juce::dontSendNotification);
        panLabel_.setText(formatPan(trk.mix.pan), juce::dontSendNotification);

        if (!fader_.isMouseButtonDown())
            fader_.setValue(trk.mix.volume, juce::dontSendNotification);
        if (!volLabel_.isBeingEdited())
            volLabel_.setText(formatDb(trk.mix.volume), juce::dontSendNotification);

        const bool hasFx = !trk.fxChain.empty();
        const bool allBypassed = hasFx && std::all_of(trk.fxChain.begin(), trk.fxChain.end(),
                                                      [](const PluginSlot& s) { return s.bypassed; });
        fxButton_.setButtonText(hasFx ? ("FX (" + juce::String(trk.fxChain.size()) + ")") : "FX");
        if (hasFx && !allBypassed)
        {
            fxButton_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff06b6d4).withAlpha(0.32f));
            fxButton_.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff22d3ee));
        }
        else if (allBypassed)
        {
            fxButton_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffb45309).withAlpha(0.45f));
            fxButton_.setColour(juce::TextButton::textColourOffId, juce::Colour(0xfffbbf24));
        }
        else
        {
            fxButton_.setColour(juce::TextButton::buttonColourId, Palette::surface.brighter(0.08f));
            fxButton_.setColour(juce::TextButton::textColourOffId, Palette::textDim);
        }
        fxButton_.setTooltip("Track " + juce::String(trackIndex_ + 1) + " FX Chain (" +
                             juce::String(trk.fxChain.size()) + " plugins) - Right-click to bypass");
        repaint();
    }

    void mouseDown(const juce::MouseEvent&) override
    {
        if (owner_.onTrackSelected)
            owner_.onTrackSelected(trackIndex_);
    }

    void paint(juce::Graphics& g) override
    {
        const auto b = getLocalBounds();
        g.setColour(selected_ ? Palette::surface.brighter(0.06f) : Palette::panelBackground);
        g.fillRect(b);

        // Top color bar
        g.setColour(accent_);
        g.fillRect(0, 0, b.getWidth(), 4);

        // Right strip divider
        g.setColour(Palette::gridLine);
        g.drawVerticalLine(b.getWidth() - 1, 0.0f, static_cast<float>(b.getHeight()));

        if (selected_)
        {
            g.setColour(accent_.withAlpha(0.55f));
            g.drawRect(b.reduced(0, 0), 1);
        }

        // Track number + name header
        g.setColour(Palette::text);
        g.setFont(Typography::bold(10.5f));
        g.drawText(juce::String(trackIndex_ + 1) + "  " + trackName_,
                   5, 6, b.getWidth() - 10, 16, juce::Justification::centredLeft, true);

        // Meter
        if (!meterRect_.isEmpty())
            drawVerticalStereoMeter(g, meterRect_, peakL_, peakR_);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(5, 4);
        area.removeFromTop(20); // color bar + title

        // FX button
        fxButton_.setBounds(area.removeFromTop(18));
        area.removeFromTop(4);

        // M & S side by side
        auto msRow = area.removeFromTop(19);
        const int halfW = (msRow.getWidth() - 3) / 2;
        muteButton_.setBounds(msRow.removeFromLeft(halfW));
        msRow.removeFromLeft(3);
        soloButton_.setBounds(msRow);
        area.removeFromTop(4);

        // Pan slider + readout
        auto panRow = area.removeFromTop(18);
        panLabel_.setBounds(panRow.removeFromRight(24));
        panSlider_.setBounds(panRow);
        area.removeFromTop(3);

        // Bottom dB readout
        volLabel_.setBounds(area.removeFromBottom(17));
        area.removeFromBottom(3);

        // Middle: Volume fader + vertical stereo meter
        const int meterW = 14;
        meterRect_ = area.removeFromRight(meterW).toFloat().reduced(0.0f, 2.0f);
        area.removeFromRight(4);
        fader_.setBounds(area);
    }

private:
    MixerPanel& owner_;
    int trackIndex_ = 0;
    bool selected_ = false;
    juce::Colour accent_{Palette::accent};
    juce::String trackName_;

    MixerFxButton fxButton_;
    DAWMuteButton muteButton_;
    DAWSoloButton soloButton_;
    juce::Slider panSlider_;
    juce::Label panLabel_;
    juce::Slider fader_;
    juce::Label volLabel_;

    juce::Rectangle<float> meterRect_;
    float peakL_ = 0.0f;
    float peakR_ = 0.0f;
};

// ============================================================================
// Pinned Master Channel Strip
// ============================================================================
class MixerPanel::MasterStrip final : public juce::Component
{
public:
    explicit MasterStrip(MixerPanel& owner) : owner_(owner)
    {
        fxButton_.setButtonText("FX");
        fxButton_.onClick = [this]()
        {
            if (owner_.onMasterFxClicked)
                owner_.onMasterFxClicked();
        };
        fxButton_.onBypassToggle = [this]()
        {
            if (owner_.onMasterFxBypass)
                owner_.onMasterFxBypass();
        };
        addAndMakeVisible(fxButton_);

        muteAllButton_.setTooltip("Mute / Unmute All Tracks");
        muteAllButton_.onClick = [this]()
        {
            if (owner_.onMuteAll)
                owner_.onMuteAll(muteAllButton_.getToggleState());
        };
        addAndMakeVisible(muteAllButton_);

        soloAllButton_.setTooltip("Solo / Unsolo All Tracks");
        soloAllButton_.onClick = [this]()
        {
            if (owner_.onSoloAll)
                owner_.onSoloAll(soloAllButton_.getToggleState());
        };
        addAndMakeVisible(soloAllButton_);

        fader_.setSliderStyle(juce::Slider::LinearVertical);
        fader_.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        fader_.setRange(0.0, 2.0, 0.01);
        fader_.setDoubleClickReturnValue(true, 1.0);
        fader_.setScrollWheelEnabled(false);
        fader_.setTooltip("Master Volume (Double-click to reset to 0.0 dB)");
        fader_.onValueChange = [this]()
        {
            const float vol = static_cast<float>(fader_.getValue());
            owner_.song_.setMasterVolume(vol);
            owner_.engine_.sequencer().setMasterVolume(vol);
            volLabel_.setText(formatDb(vol), juce::dontSendNotification);
            if (owner_.onMasterMixChanged)
                owner_.onMasterMixChanged();
        };
        addAndMakeVisible(fader_);

        volLabel_.setFont(Typography::bold(9.5f));
        volLabel_.setBorderSize(juce::BorderSize<int>(1, 2, 1, 2));
        volLabel_.setColour(juce::Label::backgroundColourId, juce::Colour(0xff16161b));
        volLabel_.setColour(juce::Label::outlineColourId, juce::Colour(0xff32323c));
        volLabel_.setColour(juce::Label::textColourId, juce::Colour(0xffd8d8e4));
        volLabel_.setJustificationType(juce::Justification::centred);
        volLabel_.setEditable(true, true, false);
        volLabel_.setTooltip("Master Volume (Click to type dB)");
        volLabel_.onTextChange = [this]()
        {
            const float vol = parseDbText(volLabel_.getText(), owner_.song_.masterVolume());
            owner_.song_.setMasterVolume(vol);
            owner_.engine_.sequencer().setMasterVolume(vol);
            fader_.setValue(vol, juce::dontSendNotification);
            volLabel_.setText(formatDb(vol), juce::dontSendNotification);
            if (owner_.onMasterMixChanged)
                owner_.onMasterMixChanged();
        };
        addAndMakeVisible(volLabel_);

        syncFromModel();
    }

    void setMeterLevels(float l, float r, bool clipped)
    {
        const float nextL = std::max(l, peakL_ * 0.82f);
        const float nextR = std::max(r, peakR_ * 0.82f);
        const bool nextClip = clipped;
        if (std::abs(nextL - peakL_) > 0.002f || std::abs(nextR - peakR_) > 0.002f || nextClip != clipped_)
        {
            peakL_ = nextL < 0.001f ? 0.0f : nextL;
            peakR_ = nextR < 0.001f ? 0.0f : nextR;
            clipped_ = nextClip;
            repaint();
        }
    }

    void syncFromModel()
    {
        const auto* pat = owner_.song_.findPattern(owner_.patternId_);
        const bool hasFx = (pat != nullptr && !pat->masterFxChain.empty());
        const bool allBypassed = hasFx && std::all_of(pat->masterFxChain.begin(), pat->masterFxChain.end(),
                                                      [](const PluginSlot& s) { return s.bypassed; });
        fxButton_.setButtonText(hasFx ? ("FX (" + juce::String(pat->masterFxChain.size()) + ")") : "FX");
        if (hasFx && !allBypassed)
        {
            fxButton_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff06b6d4).withAlpha(0.32f));
            fxButton_.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff22d3ee));
        }
        else if (allBypassed)
        {
            fxButton_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffb45309).withAlpha(0.45f));
            fxButton_.setColour(juce::TextButton::textColourOffId, juce::Colour(0xfffbbf24));
        }
        else
        {
            fxButton_.setColour(juce::TextButton::buttonColourId, Palette::surface.brighter(0.08f));
            fxButton_.setColour(juce::TextButton::textColourOffId, Palette::textDim);
        }

        bool allMuted = false;
        bool allSoloed = false;
        if (pat != nullptr && !pat->tracks.empty())
        {
            allMuted = std::all_of(pat->tracks.begin(), pat->tracks.end(), [](const PatternTrack& t) { return t.muted; });
            allSoloed = std::all_of(pat->tracks.begin(), pat->tracks.end(), [](const PatternTrack& t) { return t.solo; });
        }
        muteAllButton_.setToggleState(allMuted, juce::dontSendNotification);
        soloAllButton_.setToggleState(allSoloed, juce::dontSendNotification);

        if (!fader_.isMouseButtonDown())
            fader_.setValue(owner_.song_.masterVolume(), juce::dontSendNotification);
        if (!volLabel_.isBeingEdited())
            volLabel_.setText(formatDb(owner_.song_.masterVolume()), juce::dontSendNotification);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (clipRect_.toNearestInt().contains(e.getPosition()))
        {
            owner_.engine_.sequencer().resetClipped();
            clipped_ = false;
            repaint();
        }
    }

    void paint(juce::Graphics& g) override
    {
        const auto b = getLocalBounds();
        g.setColour(juce::Colour(0xff272329));
        g.fillRect(b);

        // Coral/red Master accent bar
        g.setColour(juce::Colour(0xffef4444));
        g.fillRect(0, 0, b.getWidth(), 4);

        // Left divider separating Master from tracks
        g.setColour(juce::Colour(0xff52525b));
        g.drawVerticalLine(0, 0.0f, static_cast<float>(b.getHeight()));

        g.setColour(juce::Colour(0xfff4f4f8));
        g.setFont(Typography::bold(11.0f));
        g.drawText("MASTER", 6, 6, b.getWidth() - 12, 16, juce::Justification::centred, false);

        // CLIP pill
        if (!clipRect_.isEmpty())
        {
            if (clipped_)
            {
                g.setColour(juce::Colour(0xffef4444));
                g.fillRoundedRectangle(clipRect_, 2.0f);
                g.setColour(juce::Colours::white);
            }
            else
            {
                g.setColour(juce::Colour(0xff18181d));
                g.fillRoundedRectangle(clipRect_, 2.0f);
                g.setColour(juce::Colour(0xff32323c));
                g.drawRoundedRectangle(clipRect_, 2.0f, 0.8f);
                g.setColour(juce::Colour(0xff52525e));
            }
            g.setFont(Typography::bold(8.0f));
            g.drawText("CLIP", clipRect_.toNearestInt(), juce::Justification::centred, false);
        }

        if (!meterRect_.isEmpty())
            drawVerticalStereoMeter(g, meterRect_, peakL_, peakR_);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(6, 4);
        area.removeFromTop(20);

        fxButton_.setBounds(area.removeFromTop(18));
        area.removeFromTop(4);

        auto msRow = area.removeFromTop(19);
        const int halfW = (msRow.getWidth() - 3) / 2;
        muteAllButton_.setBounds(msRow.removeFromLeft(halfW));
        msRow.removeFromLeft(3);
        soloAllButton_.setBounds(msRow);
        area.removeFromTop(4);

        clipRect_ = area.removeFromTop(14).toFloat();
        area.removeFromTop(4);

        volLabel_.setBounds(area.removeFromBottom(17));
        area.removeFromBottom(3);

        const int meterW = 18;
        meterRect_ = area.removeFromRight(meterW).toFloat().reduced(0.0f, 2.0f);
        area.removeFromRight(4);
        fader_.setBounds(area);
    }

private:
    MixerPanel& owner_;
    MixerFxButton fxButton_;
    DAWMuteButton muteAllButton_;
    DAWSoloButton soloAllButton_;
    juce::Slider fader_;
    juce::Label volLabel_;

    juce::Rectangle<float> clipRect_;
    juce::Rectangle<float> meterRect_;
    float peakL_ = 0.0f;
    float peakR_ = 0.0f;
    bool clipped_ = false;
};

// ============================================================================
// MixerPanel
// ============================================================================
MixerPanel::MixerPanel(Song& song, AudioEngine& engine, PatternId patternId)
    : song_(song), engine_(engine), patternId_(patternId)
{
    stripViewport_.setViewedComponent(&stripContainer_, false);
    stripViewport_.setScrollBarsShown(false, true, false, true);
    addAndMakeVisible(stripViewport_);

    masterStrip_ = std::make_unique<MasterStrip>(*this);
    addAndMakeVisible(masterStrip_.get());

    rebuildStrips();
    startTimerHz(30);
}

MixerPanel::~MixerPanel()
{
    stopTimer();
}

void MixerPanel::setPattern(PatternId patternId)
{
    patternId_ = patternId;
    rebuildStrips();
}

void MixerPanel::refreshFromModel()
{
    const auto* pat = song_.findPattern(patternId_);
    const int numTracks = (pat != nullptr) ? static_cast<int>(pat->tracks.size()) : 0;

    if (numTracks != static_cast<int>(strips_.size()))
    {
        rebuildStrips();
        return;
    }

    for (auto& s : strips_)
    {
        s->setSelected(s->trackIndex() == selectedTrack_);
        s->syncFromModel();
    }
    if (masterStrip_ != nullptr)
        masterStrip_->syncFromModel();
}

void MixerPanel::setSelectedTrack(int trackIndex)
{
    selectedTrack_ = trackIndex;
    for (auto& s : strips_)
        s->setSelected(s->trackIndex() == selectedTrack_);

    if (selectedTrack_ >= 0 && selectedTrack_ < static_cast<int>(strips_.size()))
    {
        const int xL = selectedTrack_ * kStripWidth;
        const int xR = xL + kStripWidth;
        const int curX = stripViewport_.getViewPositionX();
        const int visW = stripViewport_.getViewWidth();
        if (xL < curX)
            stripViewport_.setViewPosition(xL, 0);
        else if (xR > curX + visW)
            stripViewport_.setViewPosition(std::max(0, xR - visW), 0);
    }
}

void MixerPanel::setMasterMeterLevels(float l, float r, bool clipped)
{
    if (masterStrip_ != nullptr && isVisible())
        masterStrip_->setMeterLevels(l, r, clipped);
}

void MixerPanel::rebuildStrips()
{
    strips_.clear();
    stripContainer_.removeAllChildren();

    const auto* pat = song_.findPattern(patternId_);
    const int numTracks = (pat != nullptr) ? static_cast<int>(pat->tracks.size()) : 0;

    for (int i = 0; i < numTracks; ++i)
    {
        auto strip = std::make_unique<ChannelStrip>(*this, i);
        strip->setSelected(i == selectedTrack_);
        stripContainer_.addAndMakeVisible(strip.get());
        strips_.push_back(std::move(strip));
    }

    if (masterStrip_ != nullptr)
        masterStrip_->syncFromModel();

    layoutStrips();
    repaint();
}

void MixerPanel::layoutStrips()
{
    const int viewH = std::max(100, stripViewport_.getMaximumVisibleHeight());
    const int totalW = std::max(stripViewport_.getWidth(), static_cast<int>(strips_.size()) * kStripWidth);
    stripContainer_.setSize(totalW, viewH);

    for (int i = 0; i < static_cast<int>(strips_.size()); ++i)
        strips_[static_cast<std::size_t>(i)]->setBounds(i * kStripWidth, 0, kStripWidth, viewH);
}

void MixerPanel::timerCallback()
{
    if (!isVisible())
        return;

    for (auto& s : strips_)
    {
        const auto [l, r] = engine_.sequencer().trackPeakLevels(s->trackIndex());
        s->setMeterLevels(l, r);
    }
}

void MixerPanel::paint(juce::Graphics& g)
{
    g.fillAll(Palette::background);

    if (strips_.empty())
    {
        g.setColour(Palette::textDim.withAlpha(0.65f));
        g.setFont(Typography::regular(12.5f));
        g.drawText("No tracks yet. Add a track to view its mixer channel strip.",
                   stripViewport_.getBounds().reduced(12), juce::Justification::centred, false);
    }
}

void MixerPanel::resized()
{
    auto area = getLocalBounds();
    if (masterStrip_ != nullptr)
        masterStrip_->setBounds(area.removeFromRight(kMasterWidth));

    stripViewport_.setBounds(area);
    layoutStrips();
}

} // namespace trami
