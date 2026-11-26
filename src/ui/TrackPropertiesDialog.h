#pragma once

#include "engine/StretchCache.h"
#include "model/Song.h"
#include "ui/TramiLookAndFeel.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace trami
{

/// Unified Track Properties panel (embedded in Bottom Dock) combining General track settings
/// and Summary metadata in a single view. Stretch editing lives in the Wave tab.
class TrackPropertiesComponent : public juce::Component
{
public:
    using PropertiesChangedCallback = std::function<void()>;

    TrackPropertiesComponent(Song& song, StretchCache& stretchCache,
                             PatternId patternId, int trackIndex,
                             PropertiesChangedCallback onChanged);

    void setTrack(PatternId patternId, int trackIndex);
    void refreshFromModel();
    int currentTrackIndex() const noexcept { return trackIndex_; }

    std::function<void()> onPushUndo;

    void paint(juce::Graphics& g) override;
    void resized() override;

    static void showDialog(juce::Component* parent, Song& song, StretchCache& stretchCache,
                           PatternId patternId, int trackIndex, PropertiesChangedCallback onChanged);

private:
    void rebuildTrackSelector();
    void loadFromModel();
    void applyGeneralChanges();
    void applySummaryChanges();
    bool hasValidTrack() const;

    Song& song_;
    StretchCache& stretchCache_;
    PatternId patternId_;
    int trackIndex_ = 0;
    SampleId sampleId_;
    PropertiesChangedCallback onChanged_;
    bool isLoading_ = false;

    // Header: Track selector + File info summary
    juce::Label trackSelectLabel_{"trkSel", "Track:"};
    juce::ComboBox trackSelectorCombo_;
    juce::Label fileInfoLabel_;

    // General controls (left column)
    juce::Label trackTypeLabel_{"tType", "Track Type:"};
    juce::ComboBox trackTypeCombo_;

    juce::Label beatsLabel_{"beats", "Number of Beats:"};
    juce::ComboBox beatsCombo_;

    juce::Label rootNoteLabel_{"root", "Root Note (Transpose):"};
    juce::ComboBox rootNoteCombo_;

    juce::Label origBpmLabel_{"bpm", "Original Tempo (BPM):"};
    juce::TextEditor origBpmEditor_;

    juce::Label fxSendLabel_{"fx", "Aux FX Send (Delay):"};
    juce::Slider fxSendSlider_{juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight};

    juce::Label recordInputLabel_{"recIn", "Recording Source:"};
    juce::ComboBox recordInputCombo_;

    // Summary metadata controls (right column)
    juce::Label titleLabel_{"title", "Track / Sample Name:"};
    juce::TextEditor titleEditor_;
    juce::Label authorLabel_{"author", "Author / Artist:"};
    juce::TextEditor authorEditor_;
    juce::Label copyrightLabel_{"copy", "Copyright:"};
    juce::TextEditor copyrightEditor_;
    juce::Label commentsLabel_{"comm", "Comments:"};
    juce::TextEditor commentsEditor_;
};

} // namespace trami
