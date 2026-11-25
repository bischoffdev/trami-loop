#pragma once

#include "engine/StretchCache.h"
#include "model/Song.h"
#include "ui/TramiLookAndFeel.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace trami
{

/// Sonic Foundry ACID-style Track Properties panel (embedded in Bottom Dock) with
/// General, Stretch (interactive transient markers), and Summary metadata sub-tabs.
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
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    void mouseMagnify(const juce::MouseEvent& e, float scaleFactor) override;

    static void showDialog(juce::Component* parent, Song& song, StretchCache& stretchCache,
                           PatternId patternId, int trackIndex, PropertiesChangedCallback onChanged);

private:
    enum class ActiveTab
    {
        General = 0,
        Stretch = 1,
        Summary = 2
    };

    void switchTab(ActiveTab tab);
    void rebuildTrackSelector();
    void loadFromModel();
    void applyGeneralChanges();
    void applySummaryChanges();
    int hitTestMarker(juce::Point<int> pt) const;
    bool hasValidTrack() const;
    void setStretchZoom(double newZoom, double anchorNorm = -1.0);
    double stretchNormToX(double norm) const;
    double stretchXToNorm(int x) const;
    juce::Rectangle<int> stretchScrollThumbRect() const;

    Song& song_;
    StretchCache& stretchCache_;
    PatternId patternId_;
    int trackIndex_ = 0;
    SampleId sampleId_;
    PropertiesChangedCallback onChanged_;
    bool isLoading_ = false;

    ActiveTab activeTab_ = ActiveTab::General;

    // Header: Track selector + Sub-tab buttons
    juce::Label trackSelectLabel_{"trkSel", "Track:"};
    juce::ComboBox trackSelectorCombo_;
    juce::TextButton generalTabBtn_{"General"};
    juce::TextButton stretchTabBtn_{"Stretch"};
    juce::TextButton summaryTabBtn_{"Summary"};

    // General tab controls
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

    // Stretch tab controls
    juce::TextButton autoDetectBtn_{"Auto-Detect Transients"};
    juce::TextButton beatGridBtn_{"Even Beat Grid"};
    juce::TextButton clearMarkersBtn_{"Clear Markers"};
    juce::TextButton stretchZoomOutBtn_{"-"};
    juce::Label stretchZoomLabel_;
    juce::TextButton stretchZoomInBtn_{"+"};
    juce::TextButton stretchZoomResetBtn_{"1:1"};
    juce::Rectangle<int> stretchWaveformRect_;
    juce::Rectangle<int> stretchScrollRect_;
    double stretchZoom_ = 1.0;
    double stretchScrollNorm_ = 0.0;
    int draggingMarkerIdx_ = -1;
    bool pushedUndoForMarkerDrag_ = false;
    bool draggingStretchScroll_ = false;
    int scrollDragStartX_ = 0;
    double scrollDragStartNorm_ = 0.0;
    int hoveredMarkerIdx_ = -1;

    // Summary tab controls
    juce::Label fileInfoLabel_;
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
