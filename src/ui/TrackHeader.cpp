#include "ui/TrackHeader.h"

#include "ui/PatternGrid.h"
#include "ui/TramiLookAndFeel.h"

#include <algorithm>

namespace trami
{

class TrackColorPicker : public juce::Component
{
public:
    TrackColorPicker(std::uint32_t currentColor, std::function<void(std::uint32_t)> onPick)
        : currentColor_(currentColor), onPick_(std::move(onPick))
    {
        addAndMakeVisible(autoBtn_);
        autoBtn_.setButtonText("Reset to Auto Color");
        autoBtn_.onClick = [this]()
        {
            if (onPick_)
                onPick_(0);
            if (auto* box = findParentComponentOfClass<juce::CallOutBox>())
                box->dismiss();
        };
        setSize(kCols * (kSwatch + kGap) + kPad * 2 - kGap,
                kRows * (kSwatch + kGap) + kPad * 2 + 28);
    }

    void resized() override
    {
        autoBtn_.setBounds(kPad, getHeight() - kPad - 22, getWidth() - kPad * 2, 22);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(Palette::surface);
        for (int i = 0; i < 32; ++i)
        {
            const auto r = swatchRect(i).toFloat();
            const std::uint32_t col = Palette::kTrackColors[static_cast<std::size_t>(i)];
            g.setColour(juce::Colour(col));
            g.fillRoundedRectangle(r, 4.0f);

            if (col == currentColor_)
            {
                g.setColour(juce::Colours::white);
                g.drawRoundedRectangle(r.expanded(1.0f), 4.5f, 2.0f);
            }
            else if (i == hovered_)
            {
                g.setColour(juce::Colours::white.withAlpha(0.75f));
                g.drawRoundedRectangle(r, 4.0f, 1.5f);
            }
            else
            {
                g.setColour(juce::Colours::black.withAlpha(0.35f));
                g.drawRoundedRectangle(r, 4.0f, 1.0f);
            }
        }
    }

    void mouseMove(const juce::MouseEvent& e) override
    {
        const int idx = hitSwatch(e.getPosition());
        if (idx != hovered_)
        {
            hovered_ = idx;
            setMouseCursor(idx >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
            repaint();
        }
    }

    void mouseExit(const juce::MouseEvent&) override
    {
        hovered_ = -1;
        repaint();
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        const int idx = hitSwatch(e.getPosition());
        if (idx >= 0 && idx < 32)
        {
            if (onPick_)
                onPick_(Palette::kTrackColors[static_cast<std::size_t>(idx)]);
            if (auto* box = findParentComponentOfClass<juce::CallOutBox>())
                box->dismiss();
        }
    }

private:
    static constexpr int kCols = 8;
    static constexpr int kRows = 4;
    static constexpr int kSwatch = 22;
    static constexpr int kGap = 5;
    static constexpr int kPad = 8;

    juce::Rectangle<int> swatchRect(int index) const
    {
        const int col = index % kCols;
        const int row = index / kCols;
        return {kPad + col * (kSwatch + kGap), kPad + row * (kSwatch + kGap), kSwatch, kSwatch};
    }

    int hitSwatch(juce::Point<int> pt) const
    {
        for (int i = 0; i < 32; ++i)
            if (swatchRect(i).contains(pt))
                return i;
        return -1;
    }

    std::uint32_t currentColor_ = 0;
    std::function<void(std::uint32_t)> onPick_;
    juce::TextButton autoBtn_;
    int hovered_ = -1;
};

class TrackFxButton : public juce::TextButton
{
public:
    explicit TrackFxButton(const juce::String& text) : juce::TextButton(text) {}
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

class TrackInputButton : public juce::TextButton
{
public:
    explicit TrackInputButton(const juce::String& text) : juce::TextButton(text) {}
    std::function<void()> onQuickToggleStereo;

    void mouseDown(const juce::MouseEvent& e) override
    {
        if ((e.mods.isPopupMenu() || e.mods.isAltDown()) && onQuickToggleStereo)
        {
            onQuickToggleStereo();
            return;
        }
        juce::TextButton::mouseDown(e);
    }
};

class TrackNameLabel : public juce::Label
{
public:
    TrackNameLabel(const juce::String& name, const juce::String& text)
        : juce::Label(name, text) {}

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (auto* parent = getParentComponent())
        {
            if (e.mods.isPopupMenu())
            {
                parent->mouseDown(e.getEventRelativeTo(parent));
                return;
            }
            parent->mouseDown(e.getEventRelativeTo(parent));
        }
        juce::Label::mouseDown(e);
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (auto* parent = getParentComponent())
            parent->mouseDrag(e.getEventRelativeTo(parent));
        juce::Label::mouseDrag(e);
    }
};

namespace
{
juce::String formatVolumeDb(float gain)
{
    if (gain <= 0.0005f)
        return "-inf";
    const float db = 20.0f * std::log10(gain);
    if (std::abs(db) < 0.05f)
        return "0.0dB";
    return juce::String::formatted("%+.1fdB", db);
}

juce::String formatPanText(float pan)
{
    const int pct = juce::roundToInt(std::clamp(pan, -1.0f, 1.0f) * 100.0f);
    if (pct == 0)
        return "C";
    return (pct < 0 ? "L" : "R") + juce::String(std::abs(pct));
}

std::optional<float> parseVolumeInput(const juce::String& raw)
{
    const juce::String s = raw.trim().toLowerCase();
    if (s.isEmpty())
        return std::nullopt;
    if (s == "-inf" || s == "inf" || s == "mute" || s == "off")
        return 0.0f;
    if (s.endsWith("%"))
    {
        const double pct = s.dropLastCharacters(1).trim().getDoubleValue();
        return std::clamp(static_cast<float>(pct / 100.0), 0.0f, 2.0f);
    }
    juce::String cleaned = s;
    if (cleaned.endsWith("db"))
        cleaned = cleaned.dropLastCharacters(2).trim();
    const double db = cleaned.getDoubleValue();
    if (db <= -60.0)
        return 0.0f;
    return std::clamp(static_cast<float>(std::pow(10.0, db / 20.0)), 0.0f, 2.0f);
}

std::optional<float> parsePanInput(const juce::String& raw)
{
    const juce::String s = raw.trim().toUpperCase();
    if (s.isEmpty())
        return std::nullopt;
    if (s == "C" || s == "CENTER" || s == "0")
        return 0.0f;
    if (s.startsWith("L"))
    {
        const double v = s.substring(1).trim().getDoubleValue();
        const double norm = (v > 1.0) ? (v / 100.0) : v;
        return std::clamp(static_cast<float>(-std::abs(norm)), -1.0f, 1.0f);
    }
    if (s.startsWith("R"))
    {
        const double v = s.substring(1).trim().getDoubleValue();
        const double norm = (v > 1.0) ? (v / 100.0) : v;
        return std::clamp(static_cast<float>(std::abs(norm)), -1.0f, 1.0f);
    }
    const double v = s.getDoubleValue();
    if (std::abs(v) > 1.0)
        return std::clamp(static_cast<float>(v / 100.0), -1.0f, 1.0f);
    return std::clamp(static_cast<float>(v), -1.0f, 1.0f);
}
} // namespace

// ==============================================================================
// TrackSliderLookAndFeel (GarageBand-style recessed dotted pill + silver thumb)
// ==============================================================================
class TrackHeaderList::TrackSliderLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float /*minSliderPos*/, float /*maxSliderPos*/,
                          const juce::Slider::SliderStyle /*style*/, juce::Slider& slider) override
    {
        const bool isPan = (slider.getMinimum() < 0.0);
        const float slotH = std::clamp(static_cast<float>(height) - 2.0f, 14.0f, 20.0f);
        const float slotY = static_cast<float>(y) + (static_cast<float>(height) - slotH) * 0.5f;
        const auto r = juce::Rectangle<float>(static_cast<float>(x) + 1.0f, slotY,
                                              std::max(12.0f, static_cast<float>(width) - 2.0f), slotH);
        const float radius = r.getHeight() * 0.5f;

        // Recessed dark capsule slot
        juce::ColourGradient slotGrad(juce::Colour(0xff0d0d10), r.getX(), r.getY(),
                                      juce::Colour(0xff1c1c21), r.getX(), r.getBottom(), false);
        g.setGradientFill(slotGrad);
        g.fillRoundedRectangle(r, radius);

        g.setColour(juce::Colour(0xff09090b));
        g.drawRoundedRectangle(r, radius, 1.0f);
        g.setColour(juce::Colours::white.withAlpha(0.09f));
        g.drawHorizontalLine(juce::roundToInt(r.getBottom() + 0.5f), r.getX() + radius, r.getRight() - radius);

        const float thumbR = r.getHeight() * 0.45f;
        const float minX = r.getX() + thumbR + 1.5f;
        const float maxX = r.getRight() - thumbR - 1.5f;
        const float norm = static_cast<float>((slider.getValue() - slider.getMinimum()) /
                                              std::max(0.0001, slider.getMaximum() - slider.getMinimum()));
        const float tx = minX + std::clamp(norm, 0.0f, 1.0f) * std::max(0.0f, maxX - minX);
        const float ty = r.getCentreY();

        if (!isPan)
        {
            // Dual horizontal rows of LED dots inside the recessed volume capsule,
            // reflecting live audio / recording input level in real time!
            const float padX = 7.0f;
            const float usableW = std::max(6.0f, r.getWidth() - padX * 2.0f);
            const int numDots = std::clamp(static_cast<int>(usableW / 6.0f), 6, 16);
            const float stepX = (numDots > 1) ? (usableW / static_cast<float>(numDots - 1)) : 0.0f;
            const float rowY[2] = {ty - 2.8f, ty + 2.8f};

            const float peakL = static_cast<float>(slider.getProperties().getWithDefault("peakL", 0.0));
            const float peakR = static_cast<float>(slider.getProperties().getWithDefault("peakR", 0.0));
            const float peaks[2] = {peakL, peakR};

            for (int row = 0; row < 2; ++row)
            {
                const float lv = peaks[row];
                float levelNorm = 0.0f;
                if (lv > 0.001f)
                {
                    const float db = 20.0f * std::log10(std::max(1e-5f, lv));
                    levelNorm = std::clamp((db + 48.0f) / 51.0f, 0.0f, 1.0f);
                }

                for (int i = 0; i < numDots; ++i)
                {
                    const float dx = r.getX() + padX + static_cast<float>(i) * stepX;
                    const float dotNorm = (numDots > 1)
                        ? (static_cast<float>(i) / static_cast<float>(numDots - 1))
                        : 0.0f;

                    if (levelNorm > 0.001f && dotNorm <= levelNorm)
                    {
                        const juce::Colour litCol = (dotNorm >= 0.86f)
                            ? juce::Colour(0xffef4444)
                            : ((dotNorm >= 0.68f) ? juce::Colour(0xfff59e0b) : juce::Colour(0xff22c55e));
                        g.setColour(litCol);
                    }
                    else
                    {
                        const bool active = (dx <= tx);
                        g.setColour(active ? juce::Colour(0xff4a5562) : juce::Colour(0xff2a2a31));
                    }
                    g.fillEllipse(dx - 1.25f, rowY[row] - 1.25f, 2.5f, 2.5f);
                }
            }
        }
        else
        {
            // Center tick for Pan slider
            const float cx = r.getCentreX();
            g.setColour(juce::Colours::white.withAlpha(0.22f));
            g.drawVerticalLine(juce::roundToInt(cx), r.getY() + 3.0f, r.getBottom() - 3.0f);
        }

        // Brushed-silver circular thumb (matching GarageBand screenshot)
        g.setColour(juce::Colours::black.withAlpha(0.55f));
        g.fillEllipse(tx - thumbR, ty - thumbR + 1.0f, thumbR * 2.0f, thumbR * 2.0f);

        const bool hover = slider.isMouseOverOrDragging();
        juce::ColourGradient thumbGrad(hover ? juce::Colour(0xffdcdce4) : juce::Colour(0xffc6c6ce),
                                       tx - thumbR * 0.4f, ty - thumbR,
                                       hover ? juce::Colour(0xff787884) : juce::Colour(0xff62626c),
                                       tx + thumbR * 0.4f, ty + thumbR, false);
        thumbGrad.addColour(0.5, juce::Colour(0xff8e8e98));
        g.setGradientFill(thumbGrad);
        g.fillEllipse(tx - thumbR, ty - thumbR, thumbR * 2.0f, thumbR * 2.0f);

        g.setColour(juce::Colours::white.withAlpha(0.35f));
        g.drawEllipse(tx - thumbR + 0.8f, ty - thumbR + 0.8f, (thumbR - 0.8f) * 2.0f, (thumbR - 0.8f) * 2.0f, 0.8f);
        g.setColour(juce::Colour(0xff141418));
        g.drawEllipse(tx - thumbR, ty - thumbR, thumbR * 2.0f, thumbR * 2.0f, 1.0f);
        juce::ignoreUnused(sliderPos);
    }
};

TrackHeaderList::TrackHeaderList(Song& song, PatternId patternId, MixChangedCallback onMixChanged)
    : song_(song), patternId_(patternId), onMixChanged_(std::move(onMixChanged))
{
    sliderLnf_ = std::make_unique<TrackSliderLookAndFeel>();

    addTrackButton_ = std::make_unique<juce::TextButton>("+ Add Track");
    addTrackButton_->setColour(juce::TextButton::buttonColourId, Palette::surface);
    addTrackButton_->setColour(juce::TextButton::textColourOffId, Palette::text);
    addTrackButton_->onClick = [this]() {
        if (onAddTrack_)
            onAddTrack_();
    };
    addAndMakeVisible(addTrackButton_.get());

    updateFromModel();
}

TrackHeaderList::~TrackHeaderList()
{
    stopTimer();
    for (auto& c : controls_)
    {
        if (c.volumeSlider != nullptr)
            c.volumeSlider->setLookAndFeel(nullptr);
        if (c.panSlider != nullptr)
            c.panSlider->setLookAndFeel(nullptr);
    }
}

void TrackHeaderList::setTrackPeakProvider(TrackPeakProvider cb)
{
    trackPeakProvider_ = std::move(cb);
    if (trackPeakProvider_)
        startTimerHz(30);
    else
        stopTimer();
}

void TrackHeaderList::timerCallback()
{
    if (!trackPeakProvider_)
        return;
    const int n = static_cast<int>(controls_.size());
    if (static_cast<int>(peakL_.size()) != n)
    {
        peakL_.assign(static_cast<std::size_t>(n), 0.0f);
        peakR_.assign(static_cast<std::size_t>(n), 0.0f);
    }
    bool changed = false;
    for (int t = 0; t < n; ++t)
    {
        auto [curL, curR] = trackPeakProvider_(t);
        const float nextL = std::max(curL, peakL_[static_cast<std::size_t>(t)] * 0.80f);
        const float nextR = std::max(curR, peakR_[static_cast<std::size_t>(t)] * 0.80f);
        if (std::abs(nextL - peakL_[static_cast<std::size_t>(t)]) > 0.002f ||
            std::abs(nextR - peakR_[static_cast<std::size_t>(t)]) > 0.002f)
        {
            peakL_[static_cast<std::size_t>(t)] = nextL < 0.001f ? 0.0f : nextL;
            peakR_[static_cast<std::size_t>(t)] = nextR < 0.001f ? 0.0f : nextR;
            if (controls_[static_cast<std::size_t>(t)].volumeSlider != nullptr)
            {
                auto& props = controls_[static_cast<std::size_t>(t)].volumeSlider->getProperties();
                props.set("peakL", peakL_[static_cast<std::size_t>(t)]);
                props.set("peakR", peakR_[static_cast<std::size_t>(t)]);
                controls_[static_cast<std::size_t>(t)].volumeSlider->repaint();
            }
            changed = true;
        }
    }
    if (changed)
        repaint();
}

void TrackHeaderList::setPattern(PatternId patternId)
{
    patternId_ = patternId;
    updateFromModel();
}

int TrackHeaderList::pointToTrack(juce::Point<int> pt) const
{
    if (pt.y < PatternGrid::kRulerHeight)
        return -1;
    const int t = (pt.y - PatternGrid::kRulerHeight) / trackHeight_;
    const auto* pat = song_.findPattern(patternId_);
    if (!pat)
        return -1;
    if (t < 0 || t >= static_cast<int>(pat->tracks.size()))
        return -1;
    return t;
}

std::vector<int> TrackHeaderList::targetTracksFor(int primaryTrack) const
{
    const auto* pat = song_.findPattern(patternId_);
    const int numTracks = pat != nullptr ? static_cast<int>(pat->tracks.size()) : 0;
    if (selectedTracks_.size() > 1 && selectedTracks_.count(primaryTrack) > 0)
    {
        std::vector<int> result;
        result.reserve(selectedTracks_.size());
        for (int idx : selectedTracks_)
            if (idx >= 0 && idx < numTracks)
                result.push_back(idx);
        if (!result.empty())
            return result;
    }
    if (primaryTrack >= 0 && primaryTrack < numTracks)
        return {primaryTrack};
    return {};
}

void TrackHeaderList::showColorPickerForTrack(int track, juce::Rectangle<int> anchorRect)
{
    const auto* pat = song_.findPattern(patternId_);
    if (pat == nullptr || track < 0 || track >= static_cast<int>(pat->tracks.size()))
        return;

    const std::uint32_t curColor = pat->tracks[static_cast<std::size_t>(track)].colorArgb;
    auto picker = std::make_unique<TrackColorPicker>(
        curColor,
        [this, track](std::uint32_t chosenArgb)
        {
            for (int target : targetTracksFor(track))
                song_.setPatternTrackColor(patternId_, target, chosenArgb);
            if (onTrackColorChanged_)
                onTrackColorChanged_();
            repaint();
        });
    juce::CallOutBox::launchAsynchronously(std::move(picker), localAreaToGlobal(anchorRect), nullptr);
}

void TrackHeaderList::showInputMenuForTrack(int track, juce::Component* anchorComp)
{
    const auto* pat = song_.findPattern(patternId_);
    if (pat == nullptr || track < 0 || track >= static_cast<int>(pat->tracks.size()))
        return;

    const auto& trk = pat->tracks[static_cast<std::size_t>(track)];
    const juce::StringArray chNames = inputChannelNamesProvider_ ? inputChannelNamesProvider_() : juce::StringArray{};
    const juce::String devName = inputDeviceNameProvider_ ? inputDeviceNameProvider_() : juce::String{};

    juce::PopupMenu m;
    DAWInputSources::populateMenu(m, chNames, trk.inputChannel, trk.inputStereo, devName,
                                  static_cast<bool>(onOpenAudioSettings_));

    auto opts = juce::PopupMenu::Options();
    opts = opts.withTargetComponent(anchorComp != nullptr ? anchorComp : static_cast<juce::Component*>(this));

    m.showMenuAsync(opts, [this, track](int res)
    {
        if (res == DAWInputSources::kAudioSettingsId)
        {
            if (onOpenAudioSettings_)
                onOpenAudioSettings_();
            return;
        }
        int newCh = 0;
        bool newStereo = true;
        if (!DAWInputSources::decode(res, newCh, newStereo))
            return;
        auto* p = song_.findPattern(patternId_);
        if (p == nullptr || track < 0 || track >= static_cast<int>(p->tracks.size()))
            return;

        if (onPushUndo_)
            onPushUndo_();

        // Only change the source; never touch the arm state.
        for (int idx : targetTracksFor(track))
        {
            song_.setPatternTrackInput(patternId_, idx, newCh, newStereo);
            if (onMixChanged_)
                onMixChanged_(idx);
        }
        updateFromModel();
    });
}

void TrackHeaderList::updateFromModel()
{
    const auto* pat = song_.findPattern(patternId_);
    const int numTracks = pat != nullptr ? static_cast<int>(pat->tracks.size()) : 0;

    if (static_cast<int>(controls_.size()) != numTracks)
    {
        for (auto& oldC : controls_)
        {
            if (oldC.volumeSlider != nullptr)
                oldC.volumeSlider->setLookAndFeel(nullptr);
            if (oldC.panSlider != nullptr)
                oldC.panSlider->setLookAndFeel(nullptr);
        }
        controls_.clear();
        controls_.resize(static_cast<std::size_t>(numTracks));

        for (int t = 0; t < numTracks; ++t)
        {
            auto& c = controls_[static_cast<std::size_t>(t)];

            c.nameLabel = std::make_unique<TrackNameLabel>("lbl", "[empty]");
            c.nameLabel->setFont(Typography::bold(13.0f));
            c.nameLabel->setColour(juce::Label::textColourId, Palette::textDim.withAlpha(0.7f));
            c.nameLabel->setJustificationType(juce::Justification::centredLeft);
            c.nameLabel->setEditable(false, true, false);
            c.nameLabel->onTextChange = [this, t, lbl = c.nameLabel.get()] {
                if (lbl != nullptr)
                {
                    juce::String txt = lbl->getText().trim();
                    if (txt.startsWith(juce::String(t + 1) + ". "))
                        txt = txt.substring((juce::String(t + 1) + ". ").length()).trim();
                    song_.renamePatternTrack(patternId_, t, txt.toStdString());
                    if (onTrackRenamed_)
                        onTrackRenamed_(t, txt.toStdString());
                    updateFromModel();
                }
            };
            addAndMakeVisible(c.nameLabel.get());

            auto fxBtn = std::make_unique<TrackFxButton>("fx");
            fxBtn->setColour(juce::TextButton::buttonColourId, Palette::surface.brighter(0.08f));
            fxBtn->setColour(juce::TextButton::textColourOffId, Palette::textDim);
            fxBtn->setTooltip("Track " + juce::String(t + 1) + " VST3/AU FX Rack (Right-click or Alt+click to bypass all FX)");
            fxBtn->onClick = [this, t]() {
                if (onTrackFx_)
                    onTrackFx_(t);
            };
            fxBtn->onBypassToggle = [this, t]() {
                if (onTrackFxBypass_)
                    onTrackFxBypass_(t);
            };
            c.fxButton = std::move(fxBtn);
            addAndMakeVisible(c.fxButton.get());

            c.propsButton = std::make_unique<juce::TextButton>("P");
            c.propsButton->setColour(juce::TextButton::buttonColourId, Palette::surface.brighter(0.10f));
            c.propsButton->setColour(juce::TextButton::textColourOffId, Palette::accent);
            c.propsButton->setTooltip("Track " + juce::String(t + 1) + " Properties (Stretch Markers / Key / Summary)");
            c.propsButton->onClick = [this, t]() {
                if (onTrackProperties_)
                    onTrackProperties_(t);
            };
            addAndMakeVisible(c.propsButton.get());

            c.typeButton = std::make_unique<juce::TextButton>("type");
            addAndMakeVisible(c.typeButton.get());

            c.deleteButton = std::make_unique<juce::TextButton>(juce::CharPointer_UTF8("\xc3\x97"));
            c.deleteButton->setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
            c.deleteButton->setColour(juce::TextButton::textColourOffId, Palette::textDim.withAlpha(0.5f));
            c.deleteButton->setColour(juce::TextButton::buttonOnColourId, Palette::danger.withAlpha(0.2f));
            c.deleteButton->setTooltip("Delete Track " + juce::String(t + 1));
            c.deleteButton->onClick = [this, t]() {
                if (onDeleteTrack_)
                    onDeleteTrack_(t);
            };
            addAndMakeVisible(c.deleteButton.get());

            c.muteButton = std::make_unique<DAWMuteButton>();
            c.muteButton->setTooltip("Mute Track " + juce::String(t + 1) + " (applies to all selected tracks)");
            c.muteButton->onClick = [this, t]() {
                if (auto* p = song_.findPattern(patternId_))
                {
                    if (t < static_cast<int>(p->tracks.size()) && t < static_cast<int>(controls_.size()))
                    {
                        const bool muted = controls_[static_cast<std::size_t>(t)].muteButton->getToggleState();
                        for (int idx : targetTracksFor(t))
                        {
                            p->tracks[static_cast<std::size_t>(idx)].muted = muted;
                            if (idx < static_cast<int>(controls_.size()))
                                controls_[static_cast<std::size_t>(idx)].muteButton->setToggleState(muted, juce::dontSendNotification);
                            if (onMixChanged_)
                                onMixChanged_(idx);
                        }
                    }
                }
            };
            addAndMakeVisible(c.muteButton.get());

            c.soloButton = std::make_unique<DAWSoloButton>();
            c.soloButton->setTooltip("Solo Track " + juce::String(t + 1) + " (applies to all selected tracks)");
            c.soloButton->onClick = [this, t]() {
                if (auto* p = song_.findPattern(patternId_))
                {
                    if (t < static_cast<int>(p->tracks.size()) && t < static_cast<int>(controls_.size()))
                    {
                        const bool solo = controls_[static_cast<std::size_t>(t)].soloButton->getToggleState();
                        for (int idx : targetTracksFor(t))
                        {
                            p->tracks[static_cast<std::size_t>(idx)].solo = solo;
                            if (idx < static_cast<int>(controls_.size()))
                                controls_[static_cast<std::size_t>(idx)].soloButton->setToggleState(solo, juce::dontSendNotification);
                            if (onMixChanged_)
                                onMixChanged_(idx);
                        }
                    }
                }
            };
            addAndMakeVisible(c.soloButton.get());

            c.armButton = std::make_unique<DAWArmButton>();
            c.armButton->setTooltip("Arm Track " + juce::String(t + 1) + " for Recording (applies to all selected tracks)");
            c.armButton->onClick = [this, t]() {
                if (auto* p = song_.findPattern(patternId_))
                {
                    if (t < static_cast<int>(p->tracks.size()) && t < static_cast<int>(controls_.size()))
                    {
                        const bool armed = controls_[static_cast<std::size_t>(t)].armButton->getToggleState();
                        for (int idx : targetTracksFor(t))
                        {
                            p->tracks[static_cast<std::size_t>(idx)].armed = armed;
                            if (idx < static_cast<int>(controls_.size()))
                                controls_[static_cast<std::size_t>(idx)].armButton->setToggleState(armed, juce::dontSendNotification);
                            if (onMixChanged_)
                                onMixChanged_(idx);
                        }
                        updateFromModel();
                    }
                }
            };
            addAndMakeVisible(c.armButton.get());

            auto inBtn = std::make_unique<DAWInputButton>();
            inBtn->onSelectInputChannel = [this, t]() {
                if (t < static_cast<int>(controls_.size()))
                    showInputMenuForTrack(t, controls_[static_cast<std::size_t>(t)].inputButton.get());
            };
            c.inputButton = std::move(inBtn);
            addAndMakeVisible(c.inputButton.get());

            c.volLabel = std::make_unique<juce::Label>("vol", "0.0dB");
            c.volLabel->setFont(Typography::bold(9.5f));
            c.volLabel->setBorderSize(juce::BorderSize<int>(1, 2, 1, 2));
            c.volLabel->setColour(juce::Label::backgroundColourId, juce::Colour(0xff16161b));
            c.volLabel->setColour(juce::Label::outlineColourId, juce::Colour(0xff32323c));
            c.volLabel->setColour(juce::Label::textColourId, juce::Colour(0xffd8d8e4));
            c.volLabel->setColour(juce::Label::backgroundWhenEditingColourId, juce::Colour(0xff101015));
            c.volLabel->setColour(juce::Label::textWhenEditingColourId, juce::Colours::white);
            c.volLabel->setJustificationType(juce::Justification::centred);
            c.volLabel->setEditable(true, true, false);
            c.volLabel->setMouseCursor(juce::MouseCursor::IBeamCursor);
            c.volLabel->setTooltip("Track " + juce::String(t + 1) + " Volume (Click to type dB, e.g. -6, +3, 0, -inf, or 80%)");
            c.volLabel->onEditorShow = [lbl = c.volLabel.get()] {
                if (auto* ed = lbl->getCurrentTextEditor())
                {
                    ed->setFont(Typography::bold(9.5f));
                    ed->setJustification(juce::Justification::centred);
                    ed->selectAll();
                }
            };
            c.volLabel->onTextChange = [this, t]() {
                if (auto* p = song_.findPattern(patternId_))
                {
                    if (t < static_cast<int>(p->tracks.size()) && t < static_cast<int>(controls_.size()))
                    {
                        auto& ctrl = controls_[static_cast<std::size_t>(t)];
                        if (auto parsed = parseVolumeInput(ctrl.volLabel->getText()))
                        {
                            p->tracks[static_cast<std::size_t>(t)].mix.volume = *parsed;
                            ctrl.volumeSlider->setValue(*parsed, juce::dontSendNotification);
                            if (onMixChanged_)
                                onMixChanged_(t);
                        }
                        ctrl.volLabel->setText(formatVolumeDb(p->tracks[static_cast<std::size_t>(t)].mix.volume),
                                               juce::dontSendNotification);
                    }
                }
            };
            addAndMakeVisible(c.volLabel.get());

            c.volumeSlider = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal, juce::Slider::NoTextBox);
            c.volumeSlider->setLookAndFeel(sliderLnf_.get());
            c.volumeSlider->setScrollWheelEnabled(false);
            c.volumeSlider->setRange(0.0, 2.0, 0.01);
            c.volumeSlider->setDoubleClickReturnValue(true, 1.0);
            c.volumeSlider->setValue(1.0, juce::dontSendNotification);
            c.volumeSlider->setTooltip("Track " + juce::String(t + 1) + " Volume (Double-click to reset; adjusts selected tracks relatively)");
            c.volumeSlider->onDragStart = [this, t]() {
                dragStartVols_.clear();
                if (const auto* p = song_.findPattern(patternId_))
                {
                    if (t < static_cast<int>(p->tracks.size()))
                    {
                        dragAnchorVol_ = p->tracks[static_cast<std::size_t>(t)].mix.volume;
                        for (int idx : targetTracksFor(t))
                            dragStartVols_[idx] = p->tracks[static_cast<std::size_t>(idx)].mix.volume;
                    }
                }
            };
            c.volumeSlider->onDragEnd = [this]() { dragStartVols_.clear(); };
            c.volumeSlider->onValueChange = [this, t]() {
                if (auto* p = song_.findPattern(patternId_))
                {
                    if (t < static_cast<int>(p->tracks.size()) && t < static_cast<int>(controls_.size()))
                    {
                        const float vol = static_cast<float>(controls_[static_cast<std::size_t>(t)].volumeSlider->getValue());
                        const auto targets = targetTracksFor(t);
                        if (targets.size() > 1 && !dragStartVols_.empty())
                        {
                            const float delta = vol - dragAnchorVol_;
                            for (int idx : targets)
                            {
                                const float base = dragStartVols_.count(idx) ? dragStartVols_[idx]
                                                                             : p->tracks[static_cast<std::size_t>(idx)].mix.volume;
                                const float nextV = std::clamp(base + delta, 0.0f, 2.0f);
                                p->tracks[static_cast<std::size_t>(idx)].mix.volume = nextV;
                                if (idx < static_cast<int>(controls_.size()))
                                {
                                    if (idx != t)
                                        controls_[static_cast<std::size_t>(idx)].volumeSlider->setValue(nextV, juce::dontSendNotification);
                                    controls_[static_cast<std::size_t>(idx)].volLabel->setText(formatVolumeDb(nextV), juce::dontSendNotification);
                                }
                                if (onMixChanged_)
                                    onMixChanged_(idx);
                            }
                        }
                        else
                        {
                            for (int idx : targets)
                            {
                                p->tracks[static_cast<std::size_t>(idx)].mix.volume = vol;
                                if (idx < static_cast<int>(controls_.size()))
                                {
                                    if (idx != t)
                                        controls_[static_cast<std::size_t>(idx)].volumeSlider->setValue(vol, juce::dontSendNotification);
                                    controls_[static_cast<std::size_t>(idx)].volLabel->setText(formatVolumeDb(vol), juce::dontSendNotification);
                                }
                                if (onMixChanged_)
                                    onMixChanged_(idx);
                            }
                        }
                    }
                }
            };
            addAndMakeVisible(c.volumeSlider.get());

            c.panLabel = std::make_unique<juce::Label>("pan", "C");
            c.panLabel->setFont(Typography::bold(9.5f));
            c.panLabel->setBorderSize(juce::BorderSize<int>(1, 2, 1, 2));
            c.panLabel->setColour(juce::Label::backgroundColourId, juce::Colour(0xff16161b));
            c.panLabel->setColour(juce::Label::outlineColourId, juce::Colour(0xff32323c));
            c.panLabel->setColour(juce::Label::textColourId, juce::Colour(0xffd8d8e4));
            c.panLabel->setColour(juce::Label::backgroundWhenEditingColourId, juce::Colour(0xff101015));
            c.panLabel->setColour(juce::Label::textWhenEditingColourId, juce::Colours::white);
            c.panLabel->setJustificationType(juce::Justification::centred);
            c.panLabel->setEditable(true, true, false);
            c.panLabel->setMouseCursor(juce::MouseCursor::IBeamCursor);
            c.panLabel->setTooltip("Track " + juce::String(t + 1) + " Pan (Click to type, e.g. C, L50, R75, -25, +100)");
            c.panLabel->onEditorShow = [lbl = c.panLabel.get()] {
                if (auto* ed = lbl->getCurrentTextEditor())
                {
                    ed->setFont(Typography::bold(9.5f));
                    ed->setJustification(juce::Justification::centred);
                    ed->selectAll();
                }
            };
            c.panLabel->onTextChange = [this, t]() {
                if (auto* p = song_.findPattern(patternId_))
                {
                    if (t < static_cast<int>(p->tracks.size()) && t < static_cast<int>(controls_.size()))
                    {
                        auto& ctrl = controls_[static_cast<std::size_t>(t)];
                        if (auto parsed = parsePanInput(ctrl.panLabel->getText()))
                        {
                            p->tracks[static_cast<std::size_t>(t)].mix.pan = *parsed;
                            ctrl.panSlider->setValue(*parsed, juce::dontSendNotification);
                            if (onMixChanged_)
                                onMixChanged_(t);
                        }
                        ctrl.panLabel->setText(formatPanText(p->tracks[static_cast<std::size_t>(t)].mix.pan),
                                               juce::dontSendNotification);
                    }
                }
            };
            addAndMakeVisible(c.panLabel.get());

            c.panSlider = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal, juce::Slider::NoTextBox);
            c.panSlider->setLookAndFeel(sliderLnf_.get());
            c.panSlider->setScrollWheelEnabled(false);
            c.panSlider->setRange(-1.0, 1.0, 0.01);
            c.panSlider->setDoubleClickReturnValue(true, 0.0);
            c.panSlider->setValue(0.0, juce::dontSendNotification);
            c.panSlider->setTooltip("Track " + juce::String(t + 1) + " Pan (Double-click to center; adjusts selected tracks relatively)");
            c.panSlider->onDragStart = [this, t]() {
                dragStartPans_.clear();
                if (const auto* p = song_.findPattern(patternId_))
                {
                    if (t < static_cast<int>(p->tracks.size()))
                    {
                        dragAnchorPan_ = p->tracks[static_cast<std::size_t>(t)].mix.pan;
                        for (int idx : targetTracksFor(t))
                            dragStartPans_[idx] = p->tracks[static_cast<std::size_t>(idx)].mix.pan;
                    }
                }
            };
            c.panSlider->onDragEnd = [this]() { dragStartPans_.clear(); };
            c.panSlider->onValueChange = [this, t]() {
                if (auto* p = song_.findPattern(patternId_))
                {
                    if (t < static_cast<int>(p->tracks.size()) && t < static_cast<int>(controls_.size()))
                    {
                        const float pan = static_cast<float>(controls_[static_cast<std::size_t>(t)].panSlider->getValue());
                        const auto targets = targetTracksFor(t);
                        if (targets.size() > 1 && !dragStartPans_.empty())
                        {
                            const float delta = pan - dragAnchorPan_;
                            for (int idx : targets)
                            {
                                const float base = dragStartPans_.count(idx) ? dragStartPans_[idx]
                                                                             : p->tracks[static_cast<std::size_t>(idx)].mix.pan;
                                const float nextP = std::clamp(base + delta, -1.0f, 1.0f);
                                p->tracks[static_cast<std::size_t>(idx)].mix.pan = nextP;
                                if (idx < static_cast<int>(controls_.size()))
                                {
                                    if (idx != t)
                                        controls_[static_cast<std::size_t>(idx)].panSlider->setValue(nextP, juce::dontSendNotification);
                                    controls_[static_cast<std::size_t>(idx)].panLabel->setText(formatPanText(nextP), juce::dontSendNotification);
                                }
                                if (onMixChanged_)
                                    onMixChanged_(idx);
                            }
                        }
                        else
                        {
                            for (int idx : targets)
                            {
                                p->tracks[static_cast<std::size_t>(idx)].mix.pan = pan;
                                if (idx < static_cast<int>(controls_.size()))
                                {
                                    if (idx != t)
                                        controls_[static_cast<std::size_t>(idx)].panSlider->setValue(pan, juce::dontSendNotification);
                                    controls_[static_cast<std::size_t>(idx)].panLabel->setText(formatPanText(pan), juce::dontSendNotification);
                                }
                                if (onMixChanged_)
                                    onMixChanged_(idx);
                            }
                        }
                    }
                }
            };
            addAndMakeVisible(c.panSlider.get());
        }
        resized();
    }

    if (pat != nullptr)
    {
        for (int t = 0; t < numTracks; ++t)
        {
            const auto& track = pat->tracks[static_cast<std::size_t>(t)];
            auto& c = controls_[static_cast<std::size_t>(t)];

            c.volumeSlider->setValue(track.mix.volume, juce::dontSendNotification);
            c.volLabel->setText(formatVolumeDb(track.mix.volume), juce::dontSendNotification);
            c.panSlider->setValue(track.mix.pan, juce::dontSendNotification);
            c.panLabel->setText(formatPanText(track.mix.pan), juce::dontSendNotification);
            c.muteButton->setToggleState(track.muted, juce::dontSendNotification);
            c.soloButton->setToggleState(track.solo, juce::dontSendNotification);
            c.armButton->setToggleState(track.armed, juce::dontSendNotification);

            if (c.inputButton != nullptr)
            {
                const int availCh = inputChannelNamesProvider_ ? inputChannelNamesProvider_().size() : 2;
                const bool effStereo = track.inputStereo && (availCh != 1);
                const int effCh = (availCh == 1) ? 0 : track.inputChannel;
                c.inputButton->setInputState(effCh, effStereo, track.armed);
                c.inputButton->setTooltip(
                    "Track " + juce::String(t + 1) + " Recording Source: " +
                    (effStereo
                         ? ("Inputs " + juce::String(effCh + 1) + "+" + juce::String(effCh + 2) + " (Stereo)")
                         : ("Input " + juce::String(effCh + 1) + " (Mono)")) +
                    " - click to choose a different source (does not arm the track)");
            }

            if (c.fxButton != nullptr)
            {
                const bool hasFx = !track.fxChain.empty();
                const bool allBypassed = hasFx && std::all_of(track.fxChain.begin(), track.fxChain.end(),
                                                              [](const PluginSlot& s) { return s.bypassed; });
                if (hasFx && !allBypassed)
                {
                    c.fxButton->setButtonText("fx");
                    c.fxButton->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff06b6d4).withAlpha(0.32f));
                    c.fxButton->setColour(juce::TextButton::textColourOffId, juce::Colour(0xff22d3ee));
                }
                else if (allBypassed)
                {
                    c.fxButton->setButtonText("fx");
                    c.fxButton->setColour(juce::TextButton::buttonColourId, juce::Colour(0xffb45309).withAlpha(0.45f));
                    c.fxButton->setColour(juce::TextButton::textColourOffId, juce::Colour(0xfffbbf24));
                }
                else
                {
                    c.fxButton->setButtonText("fx");
                    c.fxButton->setColour(juce::TextButton::buttonColourId, Palette::surface.brighter(0.08f));
                    c.fxButton->setColour(juce::TextButton::textColourOffId, Palette::textDim);
                }
                c.fxButton->setTooltip("Track " + juce::String(t + 1) + " VST3/AU FX Rack (" +
                                       juce::String(track.fxChain.size()) + " plugin" +
                                       (track.fxChain.size() == 1 ? "" : "s") +
                                       (allBypassed ? ", BYPASSED" : "") +
                                       ") - Right-click or Alt+click to bypass all");
            }

            const SampleKind kind = song_.patternTrackKind(patternId_, t);
            if (kind == SampleKind::Loop)
            {
                c.typeButton->setButtonText("LOOP");
                c.typeButton->setColour(juce::TextButton::buttonColourId, juce::Colour(0xfff59e0b).withAlpha(0.28f));
                c.typeButton->setColour(juce::TextButton::textColourOffId, juce::Colour(0xffffca28));
                c.typeButton->setTooltip("Track " + juce::String(t + 1) + ": Loop (Time-Stretched to tempo). Click to cycle to 1-Shot / Disk.");
            }
            else if (kind == SampleKind::OneShot)
            {
                c.typeButton->setButtonText("1-SHOT");
                c.typeButton->setColour(juce::TextButton::buttonColourId, Palette::surface.brighter(0.08f));
                c.typeButton->setColour(juce::TextButton::textColourOffId, Palette::textDim.brighter(0.2f));
                c.typeButton->setTooltip("Track " + juce::String(t + 1) + ": One-Shot (Natural pitch/duration). Click to cycle to Disk / Loop.");
            }
            else
            {
                c.typeButton->setButtonText("DISK");
                c.typeButton->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff10b981).withAlpha(0.28f));
                c.typeButton->setColour(juce::TextButton::textColourOffId, juce::Colour(0xff34d399));
                c.typeButton->setTooltip("Track " + juce::String(t + 1) + ": Disk-Based (Streams long audio files). Click to cycle to Loop.");
            }
            c.typeButton->onClick = [this, t, kind]() {
                SampleKind newKind = SampleKind::Loop;
                if (kind == SampleKind::Loop)
                    newKind = SampleKind::OneShot;
                else if (kind == SampleKind::OneShot)
                    newKind = SampleKind::DiskStream;
                else
                    newKind = SampleKind::Loop;
                if (onTrackTypeChanged_)
                    onTrackTypeChanged_(t, newKind);
            };

            SampleId sid = track.sampleId;
            if (!sid.isValid() && !track.clips.empty())
                sid = track.clips.front().sampleId;

            const Sample* samplePtr = song_.findSample(sid);
            const bool isMissing = (samplePtr != nullptr && samplePtr->missing);

            const std::string customName = song_.patternTrackName(patternId_, t);
            if (!customName.empty())
            {
                const juce::String suffix = isMissing ? " [MISSING]" : "";
                c.nameLabel->setText(juce::String(t + 1) + ". " + customName + suffix, juce::dontSendNotification);
                c.nameLabel->setColour(juce::Label::textColourId, isMissing ? Palette::danger : Palette::text);
                c.nameLabel->setTooltip("Track " + juce::String(t + 1) + ": " + customName +
                                        (isMissing ? " (Missing file: " + juce::String(samplePtr->fileName) + ")"
                                                   : " (Double-click to rename)"));
            }
            else if (samplePtr != nullptr)
            {
                const juce::String fn = samplePtr->fileName.empty() ? juce::String(samplePtr->name) : juce::String(samplePtr->fileName);
                const juce::String suffix = isMissing ? " [MISSING]" : "";
                c.nameLabel->setText(juce::String(t + 1) + ". " + fn + suffix, juce::dontSendNotification);
                c.nameLabel->setColour(juce::Label::textColourId, isMissing ? Palette::danger : Palette::text);
                c.nameLabel->setTooltip("Track " + juce::String(t + 1) + ": " + fn +
                                        (isMissing ? " (Missing audio file)" : " (Double-click to rename)"));
            }
            else
            {
                c.nameLabel->setText("[empty]", juce::dontSendNotification);
                c.nameLabel->setColour(juce::Label::textColourId, Palette::textDim.withAlpha(0.65f));
                c.nameLabel->setTooltip("Track " + juce::String(t + 1) + " [empty] (Double-click to rename)");
            }
        }
    }
    repaint();
}

void TrackHeaderList::setSelectedTrack(int track)
{
    selectedTracks_.clear();
    if (track >= 0)
    {
        selectedTracks_.insert(track);
        lastClickedTrack_ = track;
    }
    else
    {
        lastClickedTrack_ = -1;
    }
    repaint();
}

void TrackHeaderList::setSelectedTracks(const std::unordered_set<int>& tracks, int primaryTrack)
{
    selectedTracks_ = tracks;
    if (primaryTrack >= 0)
        lastClickedTrack_ = primaryTrack;
    else if (!selectedTracks_.empty())
        lastClickedTrack_ = *selectedTracks_.begin();
    else
        lastClickedTrack_ = -1;
    repaint();
}

int TrackHeaderList::firstSelectedTrack() const noexcept
{
    if (lastClickedTrack_ >= 0)
        return lastClickedTrack_;
    if (!selectedTracks_.empty())
        return *selectedTracks_.begin();
    return -1;
}

void TrackHeaderList::setTrackHeight(int height)
{
    const int clamped = std::max(kMinTrackHeight, height);
    if (trackHeight_ != clamped)
    {
        trackHeight_ = clamped;
        resized();
        repaint();
    }
}

void TrackHeaderList::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff26262b));

    // Header top area (matching brushed-metal ruler)
    {
        juce::ColourGradient rulerGrad(juce::Colour(0xff34343a), 0.0f, 0.0f,
                                       juce::Colour(0xff222226), 0.0f, static_cast<float>(PatternGrid::kRulerHeight), false);
        g.setGradientFill(rulerGrad);
        g.fillRect(0, 0, getWidth(), PatternGrid::kRulerHeight);
        g.setColour(juce::Colour(0xff141417));
        g.drawHorizontalLine(PatternGrid::kRulerHeight - 1, 0.0f, static_cast<float>(getWidth()));
    }

    const auto* pat = song_.findPattern(patternId_);
    const int numTracks = pat != nullptr ? static_cast<int>(pat->tracks.size()) : 0;
    constexpr int kStripWidth = 6;

    // Matching modern luminous clip palette for the subtle left track accent bar
    static constexpr std::uint32_t kModernPalette[] = {
        0xff63d988, 0xffb4a2f0, 0xff72c4fa, 0xfff2bf66, 0xfff294ab,
        0xff6ad8cc, 0xff9d94f2, 0xffa2dc6d, 0xffefa274, 0xff85b8f8
    };
    constexpr std::size_t kPaletteCount = sizeof(kModernPalette) / sizeof(kModernPalette[0]);

    for (int t = 0; t < numTracks; ++t)
    {
        const int y = PatternGrid::kRulerHeight + t * trackHeight_;

        const bool isSelected = (selectedTracks_.count(t) > 0);
        // Sleek brushed dark-charcoal track row gradient (brighter when selected)
        const juce::Colour topRow = isSelected
                                        ? juce::Colour(0xff3e4652)
                                        : ((t % 2 == 0) ? juce::Colour(0xff36363c) : juce::Colour(0xff323238));
        const juce::Colour botRow = isSelected
                                        ? juce::Colour(0xff2f3640)
                                        : ((t % 2 == 0) ? juce::Colour(0xff2b2b30) : juce::Colour(0xff27272c));
        juce::ColourGradient rowGrad(topRow, 0.0f, static_cast<float>(y),
                                     botRow, 0.0f, static_cast<float>(y + trackHeight_), false);
        g.setGradientFill(rowGrad);
        g.fillRect(0, y, getWidth(), trackHeight_);

        // Subtle top inner highlight
        g.setColour(isSelected ? Palette::accent.withAlpha(0.55f) : juce::Colours::white.withAlpha(0.06f));
        g.drawHorizontalLine(y, 0.0f, static_cast<float>(getWidth()));

        // Clickable left color indicator bar (8px wide; custom track color overrides auto color)
        juce::Colour badgeCol = Palette::accent.withAlpha(0.75f);
        const auto& track = pat->tracks[static_cast<std::size_t>(t)];
        SampleId sid = track.sampleId;
        if (!sid.isValid() && !track.clips.empty())
            sid = track.clips.front().sampleId;

        if (track.colorArgb != 0)
            badgeCol = juce::Colour(track.colorArgb);
        else if (const Sample* s = song_.findSample(sid))
            badgeCol = s->missing ? Palette::danger.withAlpha(0.85f)
                                  : juce::Colour(kModernPalette[(sid.value - 1) % kPaletteCount]);

        g.setColour(badgeCol);
        g.fillRect(0, y, kStripWidth + 2, trackHeight_ - 1);

        if (isSelected)
        {
            g.setColour(Palette::accent.withAlpha(0.75f));
            g.drawRect(static_cast<float>(kStripWidth + 2), static_cast<float>(y),
                       static_cast<float>(getWidth() - kStripWidth - 3), static_cast<float>(trackHeight_ - 1), 1.2f);
        }

        // Horizontal separator between tracks
        g.setColour(juce::Colour(0xff16161a));
        g.drawHorizontalLine(y + trackHeight_ - 1, 0.0f, static_cast<float>(getWidth()));

        // Per-Track Live Stereo / Mono Peak Meter bar along the bottom of the track header
        {
            const float meterX = static_cast<float>(kStripWidth + 8);
            const float meterW = std::max(24.0f, static_cast<float>(getWidth() - kStripWidth - 16));
            const float meterY = static_cast<float>(y + trackHeight_ - 12);
            const float chanH = 3.5f;
            const float gapH = 1.5f;
            const float troughH = chanH * 2.0f + gapH + 2.0f;

            // Dark recessed meter trough (highlighted with red recording border when track is armed)
            g.setColour(juce::Colour(0xff111115));
            g.fillRoundedRectangle(meterX, meterY - 1.0f, meterW, troughH, 2.0f);
            if (track.armed)
            {
                g.setColour(Palette::danger.withAlpha(0.45f));
                g.drawRoundedRectangle(meterX, meterY - 1.0f, meterW, troughH, 2.0f, 1.0f);
            }

            const float lvL = (t < static_cast<int>(peakL_.size())) ? peakL_[static_cast<std::size_t>(t)] : 0.0f;
            const float lvR = (t < static_cast<int>(peakR_.size())) ? peakR_[static_cast<std::size_t>(t)] : 0.0f;

            auto drawChannelMeter = [&](float cy, float chH, float level) {
                if (level <= 0.001f)
                    return;
                // Convert linear peak to perceptual dB scale (-48dB .. +3dB)
                const float db = 20.0f * std::log10(std::max(1e-5f, level));
                const float norm = std::clamp((db + 48.0f) / 51.0f, 0.0f, 1.0f);
                const float fillW = meterW * norm;
                if (fillW < 1.0f)
                    return;

                juce::ColourGradient grad(juce::Colour(0xff22c55e), meterX, cy,
                                          juce::Colour(0xffef4444), meterX + meterW, cy, false);
                grad.addColour(0.68, juce::Colour(0xff84cc16));
                grad.addColour(0.84, juce::Colour(0xfff59e0b));
                g.setGradientFill(grad);
                g.fillRect(meterX + 1.0f, cy, std::max(1.0f, fillW - 2.0f), chH);
            };

            if (track.armed && !track.inputStereo)
            {
                // Unified full-height Mono meter bar
                drawChannelMeter(meterY, chanH * 2.0f + gapH, std::max(lvL, lvR));
            }
            else
            {
                drawChannelMeter(meterY, chanH, lvL);
                drawChannelMeter(meterY + chanH + gapH, chanH, lvR);
            }
        }

        // Drag hover highlight
        if (dragHoverTrack_ == t)
        {
            if (isReorderDrag_)
            {
                g.setColour(Palette::accent);
                g.fillRect(0, y, getWidth(), 3);
            }
            else
            {
                g.setColour(Palette::accent.withAlpha(0.22f));
                g.fillRect(0, y, getWidth(), trackHeight_);
                g.setColour(Palette::accent);
                g.drawRect(1.0f, static_cast<float>(y + 1), static_cast<float>(getWidth() - 2), static_cast<float>(trackHeight_ - 2), 2.0f);
            }
        }
    }

    if (dragHoverTrack_ >= numTracks && addTrackButton_ != nullptr)
    {
        g.setColour(Palette::accent.withAlpha(0.25f));
        g.fillRoundedRectangle(addTrackButton_->getBounds().toFloat().expanded(2.0f), 5.0f);
        g.setColour(Palette::accent);
        g.drawRoundedRectangle(addTrackButton_->getBounds().toFloat().expanded(2.0f), 5.0f, 1.5f);
    }

    // Right border dividing from PatternGrid
    g.setColour(juce::Colour(0xff141418));
    g.drawVerticalLine(getWidth() - 1, 0.0f, static_cast<float>(getHeight()));
}

void TrackHeaderList::resized()
{
    const int numTracks = static_cast<int>(controls_.size());
    constexpr int kStripWidth = 6;

    for (int t = 0; t < numTracks; ++t)
    {
        const int y = PatternGrid::kRulerHeight + t * trackHeight_;
        auto& c = controls_[static_cast<std::size_t>(t)];

        // Top row: Clean bold track name + FX + Props + Type + Delete
        const int nameX = kStripWidth + 8;
        const int rightBtnsW = 132;
        const int nameW = std::max(40, getWidth() - nameX - rightBtnsW);
        c.nameLabel->setBounds(nameX, y + 3, nameW, 20);
        if (c.fxButton != nullptr)
            c.fxButton->setBounds(getWidth() - 130, y + 3, 26, 19);
        if (c.propsButton != nullptr)
            c.propsButton->setBounds(getWidth() - 102, y + 3, 22, 19);
        if (c.typeButton != nullptr)
            c.typeButton->setBounds(getWidth() - 78, y + 3, 50, 19);
        if (c.deleteButton != nullptr)
            c.deleteButton->setBounds(getWidth() - 24, y + 3, 20, 19);

        // Middle row: [ M | S ]  ( • )   [=== Volume Pill ===]   Pan [==]
        // Leave bottom 14px free for the live stereo peak meter bar
        const int btnY = y + 26;
        const int btnH = std::clamp(trackHeight_ - 44, 19, 22);

        c.muteButton->setBounds(kStripWidth + 8, btnY, 22, btnH);
        c.soloButton->setBounds(kStripWidth + 30, btnY, 22, btnH);
        c.armButton->setBounds(kStripWidth + 55, btnY, 19, btnH);
        if (c.inputButton != nullptr)
            c.inputButton->setBounds(kStripWidth + 77, btnY, 84, btnH);

        const int slidersX = kStripWidth + 165;
        const int availW = std::max(40, getWidth() - slidersX - 8);

        // Bottom row: [ M | S ] (•) [⚭ Stereo | In 1+2 ▾]  [=== Volume ===] [0.0dB]  [=== Pan ===] [ C ]
        c.volumeSlider->setVisible(true);
        c.volLabel->setVisible(true);

        constexpr int volReadoutW = 42;
        constexpr int panReadoutW = 30;
        constexpr int gap = 3;

        if (availW >= 160)
        {
            const int totalSlidersW = availW - volReadoutW - panReadoutW - gap * 3;
            const int panW = std::clamp(totalSlidersW * 40 / 100, 42, 120);
            const int volW = std::max(44, totalSlidersW - panW);

            int curX = slidersX;
            c.volumeSlider->setBounds(curX, btnY, volW, btnH);
            curX += volW + gap;
            c.volLabel->setBounds(curX, btnY + 1, volReadoutW, btnH - 2);
            curX += volReadoutW + gap;

            c.panSlider->setVisible(true);
            c.panLabel->setVisible(true);
            c.panSlider->setBounds(curX, btnY, panW, btnH);
            curX += panW + gap;
            c.panLabel->setBounds(curX, btnY + 1, panReadoutW, btnH - 2);
        }
        else
        {
            const int volW = std::max(40, availW - volReadoutW - gap);
            c.volumeSlider->setBounds(slidersX, btnY, volW, btnH);
            c.volLabel->setBounds(slidersX + volW + gap, btnY + 1, volReadoutW, btnH - 2);
            c.panLabel->setVisible(false);
            c.panSlider->setVisible(false);
        }
    }

    if (addTrackButton_ != nullptr)
    {
        const int y = PatternGrid::kRulerHeight + numTracks * trackHeight_ + 8;
        addTrackButton_->setBounds(kStripWidth + 8, y, getWidth() - kStripWidth - 16, 26);
    }
}

void TrackHeaderList::mouseMove(const juce::MouseEvent& e)
{
    const int t = pointToTrack(e.getPosition());
    if (t >= 0 && e.x <= 10)
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    else
        setMouseCursor(juce::MouseCursor::NormalCursor);
}

void TrackHeaderList::mouseDown(const juce::MouseEvent& e)
{
    const int t = pointToTrack(e.getPosition());
    const auto* pat = song_.findPattern(patternId_);
    if (!pat || t < 0 || t >= static_cast<int>(pat->tracks.size()))
    {
        if (onMarqueeStart)
            onMarqueeStart(e);
        return;
    }

    const auto& track = pat->tracks[static_cast<std::size_t>(t)];
    SampleId sid = track.sampleId;
    if (!sid.isValid() && !track.clips.empty())
        sid = track.clips.front().sampleId;

    const SampleKind kind = song_.patternTrackKind(patternId_, t);

    if (e.mods.isPopupMenu())
    {
        const TrackEnvelopeType visEnv = track.visibleEnvelope;
        const bool hasTrackFx = !track.fxChain.empty();
        const bool allTrackFxBypassed = hasTrackFx && std::all_of(track.fxChain.begin(), track.fxChain.end(),
                                                                  [](const PluginSlot& s) { return s.bypassed; });
        juce::PopupMenu m;
        m.addItem(21, "Track FX Chain (VST3 / AU)...");
        m.addItem(22, "Bypass All Track FX", hasTrackFx, allTrackFxBypassed);
        m.addItem(20, "Track Properties (Stretch / Key / Summary)...");
        m.addItem(1, "Rename Track...");
        m.addSeparator();
        m.addItem(2, "Track Type: Loop (Time-Stretched)", true, kind == SampleKind::Loop);
        m.addItem(3, "Track Type: One-Shot (Natural Speed)", true, kind == SampleKind::OneShot);
        m.addItem(6, "Track Type: Disk-Based (Stream Long Audio)", true, kind == SampleKind::DiskStream);
        m.addSeparator();
        m.addItem(40, "Recording Source...");
        m.addSeparator();
        juce::PopupMenu envMenu;
        envMenu.addItem(30, "Off (Hide Track Automation)", true, visEnv == TrackEnvelopeType::None);
        envMenu.addItem(31, "Volume Envelope", true, visEnv == TrackEnvelopeType::Volume);
        envMenu.addItem(32, "Pan Envelope", true, visEnv == TrackEnvelopeType::Pan);
        envMenu.addItem(33, "Aux FX Send Envelope", true, visEnv == TrackEnvelopeType::FxSend);
        m.addSubMenu("Track Envelope / Automation", envMenu);
        m.addSeparator();
        m.addItem(4, "Duplicate Track");
        m.addItem(10, "Move Track Up", t > 0);
        m.addItem(11, "Move Track Down", t < static_cast<int>(pat->tracks.size()) - 1);
        m.addSeparator();
        m.addItem(5, "Delete Track", pat->tracks.size() > 1);

        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withTargetScreenArea(
                            juce::Rectangle<int>(e.getScreenX(), e.getScreenY(), 1, 1)),
                        [this, t](int result) {
                            if (result == 21 && onTrackFx_)
                            {
                                onTrackFx_(t);
                            }
                            else if (result == 22 && onTrackFxBypass_)
                            {
                                onTrackFxBypass_(t);
                            }
                            else if (result == 20 && onTrackProperties_)
                            {
                                onTrackProperties_(t);
                            }
                            else if (result == 1 && t < static_cast<int>(controls_.size()))
                            {
                                if (controls_[static_cast<std::size_t>(t)].nameLabel != nullptr)
                                    controls_[static_cast<std::size_t>(t)].nameLabel->showEditor();
                            }
                            else if (result == 2 && onTrackTypeChanged_)
                                onTrackTypeChanged_(t, SampleKind::Loop);
                            else if (result == 3 && onTrackTypeChanged_)
                                onTrackTypeChanged_(t, SampleKind::OneShot);
                            else if (result == 6 && onTrackTypeChanged_)
                                onTrackTypeChanged_(t, SampleKind::DiskStream);
                            else if (result == 40)
                            {
                                showInputMenuForTrack(t, (t < static_cast<int>(controls_.size()))
                                                             ? controls_[static_cast<std::size_t>(t)].inputButton.get()
                                                             : nullptr);
                            }
                            else if (result >= 30 && result <= 33)
                            {
                                if (auto* p = song_.findPattern(patternId_))
                                {
                                    if (t < static_cast<int>(p->tracks.size()))
                                    {
                                        if (onPushUndo_)
                                            onPushUndo_();
                                        auto& trk = p->tracks[static_cast<std::size_t>(t)];
                                        if (result == 30) trk.visibleEnvelope = TrackEnvelopeType::None;
                                        else if (result == 31)
                                        {
                                            trk.visibleEnvelope = TrackEnvelopeType::Volume;
                                            if (trk.volumeAutomation.empty())
                                                trk.volumeAutomation = {{0.0, trk.mix.volume, EnvelopeCurve::Linear},
                                                                        {static_cast<double>(p->lengthSteps()), trk.mix.volume, EnvelopeCurve::Linear}};
                                        }
                                        else if (result == 32)
                                        {
                                            trk.visibleEnvelope = TrackEnvelopeType::Pan;
                                            if (trk.panAutomation.empty())
                                                trk.panAutomation = {{0.0, trk.mix.pan, EnvelopeCurve::Linear},
                                                                     {static_cast<double>(p->lengthSteps()), trk.mix.pan, EnvelopeCurve::Linear}};
                                        }
                                        else if (result == 33)
                                        {
                                            trk.visibleEnvelope = TrackEnvelopeType::FxSend;
                                            if (trk.fxAutomation.empty())
                                                trk.fxAutomation = {{0.0, trk.fxSend, EnvelopeCurve::Linear},
                                                                    {static_cast<double>(p->lengthSteps()), trk.fxSend, EnvelopeCurve::Linear}};
                                        }
                                        if (onMixChanged_)
                                            onMixChanged_(t);
                                    }
                                }
                            }
                            else if (result == 4 && onDuplicateTrack_)
                                onDuplicateTrack_(t);
                            else if (result == 10 && onMoveTrack_)
                                onMoveTrack_(t, t - 1);
                            else if (result == 11 && onMoveTrack_)
                                onMoveTrack_(t, t + 1);
                            else if (result == 5 && onDeleteTrack_)
                                onDeleteTrack_(t);
                        });
        return;
    }

    const int y = PatternGrid::kRulerHeight + t * trackHeight_;
    // Clicking the left track color bar (x <= 10) opens the 32-color picker
    if (e.x <= 10)
    {
        if (selectedTracks_.count(t) == 0)
        {
            selectedTracks_.clear();
            selectedTracks_.insert(t);
            lastClickedTrack_ = t;
            repaint();
        }
        showColorPickerForTrack(t, juce::Rectangle<int>(0, y, 10, trackHeight_));
        return;
    }

    // Multi-track selection: Cmd/Ctrl toggles, Shift selects range, plain click selects single
    if (e.mods.isCommandDown() || e.mods.isCtrlDown())
    {
        if (selectedTracks_.count(t) > 0)
            selectedTracks_.erase(t);
        else
            selectedTracks_.insert(t);
        lastClickedTrack_ = t;
        repaint();
    }
    else if (e.mods.isShiftDown() && lastClickedTrack_ >= 0)
    {
        const int lo = std::min(lastClickedTrack_, t);
        const int hi = std::max(lastClickedTrack_, t);
        selectedTracks_.clear();
        for (int i = lo; i <= hi; ++i)
            selectedTracks_.insert(i);
        repaint();
    }
    else
    {
        selectedTracks_.clear();
        selectedTracks_.insert(t);
        lastClickedTrack_ = t;
        repaint();
    }

    if (onTracksSelected_)
        onTracksSelected_(selectedTracks_, t);
    if (onTrackSelected_)
        onTrackSelected_(t, sid);
}

void TrackHeaderList::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (onMouseWheel)
        onMouseWheel(e, wheel);
    else
        juce::Component::mouseWheelMove(e, wheel);
}

void TrackHeaderList::mouseDrag(const juce::MouseEvent& e)
{
    const int startT = pointToTrack(e.getMouseDownPosition());
    const auto* pat = song_.findPattern(patternId_);
    if (!pat || startT < 0 || startT >= static_cast<int>(pat->tracks.size()))
    {
        if (onMarqueeDrag)
            onMarqueeDrag(e);
        return;
    }

    if (e.mouseWasDraggedSinceMouseDown() && !e.mods.isPopupMenu())
    {
        const int t = startT;
        if (pat != nullptr && t >= 0 && t < static_cast<int>(pat->tracks.size()))
        {
            if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor(this))
            {
                if (!container->isDragAndDropActive())
                {
                    const int trackY = PatternGrid::kRulerHeight + t * trackHeight_;
                    const auto trackBounds = juce::Rectangle<int>(0, trackY, getWidth(), trackHeight_);
                    auto dragImg = createComponentSnapshot(trackBounds, true, 1.0f);
                    {
                        juce::Graphics g(dragImg);
                        g.setColour(Palette::accent);
                        g.drawRect(dragImg.getBounds().toFloat(), 1.5f);
                    }
                    const auto offset = trackBounds.getPosition() - e.getPosition();
                    container->startDragging(juce::var("pattern_track_drag:" + juce::String(t)),
                                             this,
                                             juce::ScaledImage(dragImg, 1.0),
                                             true,
                                             &offset);
                }
            }
        }
    }
}

void TrackHeaderList::mouseUp(const juce::MouseEvent& e)
{
    if (onMarqueeEnd)
        onMarqueeEnd(e);
}

bool TrackHeaderList::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (const auto& f : files)
    {
        if (f.endsWithIgnoreCase(".wav") || f.endsWithIgnoreCase(".mp3"))
            return true;
    }
    return false;
}

void TrackHeaderList::fileDragEnter(const juce::StringArray& files, int /*x*/, int y)
{
    juce::ignoreUnused(files);
    dragHoverTrack_ = pointToTrack({10, y});
    repaint();
}

void TrackHeaderList::fileDragMove(const juce::StringArray& files, int /*x*/, int y)
{
    juce::ignoreUnused(files);
    const int t = pointToTrack({10, y});
    if (t != dragHoverTrack_)
    {
        dragHoverTrack_ = t;
        repaint();
    }
}

void TrackHeaderList::fileDragExit(const juce::StringArray& /*files*/)
{
    dragHoverTrack_ = -1;
    repaint();
}

void TrackHeaderList::filesDropped(const juce::StringArray& files, int /*x*/, int y)
{
    dragHoverTrack_ = -1;
    repaint();
    if (files.isEmpty())
        return;

    const int t = pointToTrack({10, y});
    if (onFileDropped_)
        onFileDropped_(files[0], t);
}

bool TrackHeaderList::isInterestedInDragSource(const SourceDetails& dragSourceDetails)
{
    if (dragSourceDetails.description.isString())
    {
        const juce::String s = dragSourceDetails.description.toString();
        if (s.startsWith("pattern_track_drag:"))
            return true;
        return s.endsWithIgnoreCase(".wav") || s.endsWithIgnoreCase(".mp3");
    }
    return false;
}

void TrackHeaderList::itemDragEnter(const SourceDetails& dragSourceDetails)
{
    isReorderDrag_ = dragSourceDetails.description.toString().startsWith("pattern_track_drag:");
    dragHoverTrack_ = pointToTrack(dragSourceDetails.localPosition);
    repaint();
}

void TrackHeaderList::itemDragMove(const SourceDetails& dragSourceDetails)
{
    isReorderDrag_ = dragSourceDetails.description.toString().startsWith("pattern_track_drag:");
    const int t = pointToTrack(dragSourceDetails.localPosition);
    if (t != dragHoverTrack_)
    {
        dragHoverTrack_ = t;
        repaint();
    }
}

void TrackHeaderList::itemDragExit(const SourceDetails& /*dragSourceDetails*/)
{
    dragHoverTrack_ = -1;
    isReorderDrag_ = false;
    repaint();
}

void TrackHeaderList::itemDropped(const SourceDetails& dragSourceDetails)
{
    dragHoverTrack_ = -1;
    isReorderDrag_ = false;
    repaint();
    if (!dragSourceDetails.description.isString())
        return;

    const juce::String desc = dragSourceDetails.description.toString();
    if (desc.startsWith("pattern_track_drag:"))
    {
        const int fromTrack = desc.fromFirstOccurrenceOf("pattern_track_drag:", false, false).getIntValue();
        const int toTrack = pointToTrack(dragSourceDetails.localPosition);
        const auto* pat = song_.findPattern(patternId_);
        if (pat != nullptr && fromTrack != toTrack && toTrack >= 0 && toTrack < static_cast<int>(pat->tracks.size()))
        {
            if (onMoveTrack_)
                onMoveTrack_(fromTrack, toTrack);
        }
        return;
    }

    const juce::String filePath = desc;
    const int t = pointToTrack(dragSourceDetails.localPosition);
    if (onFileDropped_)
        onFileDropped_(filePath, t);
}

} // namespace trami
