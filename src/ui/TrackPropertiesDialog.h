#pragma once

#include "engine/StretchCache.h"
#include "model/Song.h"
#include "ui/TramiLookAndFeel.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace trami
{

/// Sonic Foundry ACID-style Track Properties window with General, Stretch (interactive transient markers),
/// and Summary metadata tabs.
class TrackPropertiesComponent : public juce::Component
{
public:
    using PropertiesChangedCallback = std::function<void()>;

    TrackPropertiesComponent(Song& song, StretchCache& stretchCache,
                             PatternId patternId, int trackIndex,
                             PropertiesChangedCallback onChanged);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;

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
    void loadFromModel();
    void applyGeneralChanges();
    void applySummaryChanges();
    int hitTestMarker(juce::Point<int> pt) const;

    Song& song_;
    StretchCache& stretchCache_;
    PatternId patternId_;
    int trackIndex_ = 0;
    SampleId sampleId_;
    PropertiesChangedCallback onChanged_;

    ActiveTab activeTab_ = ActiveTab::General;

    // Tab buttons
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

    // Stretch tab controls
    juce::TextButton autoDetectBtn_{"Auto-Detect Transients"};
    juce::TextButton beatGridBtn_{"Even Beat Grid"};
    juce::TextButton clearMarkersBtn_{"Clear Markers"};
    juce::Rectangle<int> stretchWaveformRect_;
    int draggingMarkerIdx_ = -1;
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
