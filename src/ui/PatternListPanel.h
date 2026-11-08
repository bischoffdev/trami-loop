#pragma once

#include "model/Ids.h"
#include "model/Song.h"
#include "ui/IconButton.h"
#include "ui/TramiLookAndFeel.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace trami
{

class PatternListPanel : public juce::Component
{
public:
    using PatternSelectedCallback = std::function<void(PatternId id)>;
    using PatternModifiedCallback = std::function<void()>;

    PatternListPanel(Song& song, PatternId activePattern,
                     PatternSelectedCallback onSelected,
                     PatternModifiedCallback onModified);
    ~PatternListPanel() override = default;

    void setActivePattern(PatternId id);
    PatternId activePattern() const noexcept { return activePattern_; }

    void updateFromModel();

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;

    void paintContent(juce::Graphics& g);
    void contentMouseDown(const juce::MouseEvent& e);
    void contentMouseDrag(const juce::MouseEvent& e);

    static constexpr int kDefaultWidth = 210;

private:
    struct PatternRow
    {
        PatternId id;
        std::unique_ptr<juce::Label> nameLabel;
        std::unique_ptr<juce::Label> infoLabel;
        std::unique_ptr<IconButton> editButton;
        std::unique_ptr<IconButton> duplicateButton;
        std::unique_ptr<IconButton> deleteButton;
        juce::Rectangle<int> bounds;
    };

    Song& song_;
    PatternId activePattern_;
    PatternSelectedCallback onSelected_;
    PatternModifiedCallback onModified_;

    juce::Label titleLabel_;
    IconButton newPatternButton_{"NewPat", IconButton::Icon::Plus, "+ New"};

    std::vector<PatternRow> rows_;
    juce::Viewport viewport_;
    std::unique_ptr<juce::Component> contentComp_;

    void addNewPattern();
    void duplicatePattern(PatternId id);
    void deletePattern(PatternId id);
    void startPatternDrag(PatternId id, const juce::String& name);
};

} // namespace trami
