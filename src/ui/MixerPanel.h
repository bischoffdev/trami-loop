#pragma once

#include "engine/AudioEngine.h"
#include "model/Ids.h"
#include "model/Song.h"
#include "ui/DAWTrackControls.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace trami
{

/// Docked multi-track horizontal channel-strip Mixer with live stereo meters,
/// M/S buttons, pan, volume faders, per-track FX buttons, and a pinned Master strip.
class MixerPanel : public juce::Component, private juce::Timer
{
public:
    MixerPanel(Song& song, AudioEngine& engine, PatternId patternId);
    ~MixerPanel() override;

    void setPattern(PatternId patternId);
    void refreshFromModel();
    void setSelectedTrack(int trackIndex);
    void setSelectedTracks(const std::unordered_set<int>& tracks, int primaryTrack = -1);
    const std::unordered_set<int>& selectedTracks() const noexcept { return selectedTracks_; }
    std::vector<int> targetTracksFor(int primaryTrack) const;
    void setMasterMeterLevels(float l, float r, bool clipped);

    std::function<void(int track)> onTrackMixChanged;
    std::function<void(int track)> onTrackFxClicked;
    std::function<void(int track)> onTrackFxBypass;
    std::function<void(int track)> onTrackSelected;
    std::function<void(const std::unordered_set<int>& tracks, int primaryTrack)> onTracksSelected;
    std::function<void()> onMasterFxClicked;
    std::function<void()> onMasterFxBypass;
    std::function<void()> onMasterMixChanged;
    std::function<void(bool muted)> onMuteAll;
    std::function<void(bool soloed)> onSoloAll;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    class ChannelStrip;
    class MasterStrip;

    void rebuildStrips();
    void layoutStrips();
    void timerCallback() override;

    static constexpr int kStripWidth = 86;
    static constexpr int kMasterWidth = 98;

    Song& song_;
    AudioEngine& engine_;
    PatternId patternId_;
    int selectedTrack_ = -1;
    std::unordered_set<int> selectedTracks_;
    int lastClickedTrack_ = -1;

    juce::Viewport stripViewport_;
    juce::Component stripContainer_;
    std::vector<std::unique_ptr<ChannelStrip>> strips_;
    std::unique_ptr<MasterStrip> masterStrip_;
};

} // namespace trami
