#include "app/MainWindow.h"

#include "ui/TramiLookAndFeel.h"

namespace trami
{
namespace
{

/// Placeholder content until the pattern and arranger views exist.
class PlaceholderComponent : public juce::Component
{
public:
    PlaceholderComponent() { setSize(1280, 800); }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(Palette::background);
        g.setColour(Palette::textDim);
        g.setFont(juce::FontOptions(18.0f));
        g.drawText("trami-loop", getLocalBounds(), juce::Justification::centred);
    }
};

} // namespace

MainWindow::MainWindow(const juce::String& name)
    : DocumentWindow(name, Palette::background, DocumentWindow::allButtons)
{
    setUsingNativeTitleBar(true);
    setContentOwned(new PlaceholderComponent(), true);
    setResizable(true, true);
    centreWithSize(getWidth(), getHeight());
    setVisible(true);
}

void MainWindow::closeButtonPressed()
{
    juce::JUCEApplication::getInstance()->systemRequestedQuit();
}

} // namespace trami
