#include "app/MainWindow.h"

#include "ui/DevTransportComponent.h"
#include "ui/TramiLookAndFeel.h"

namespace trami
{

MainWindow::MainWindow(const juce::String& name, AudioEngine& engine)
    : DocumentWindow(name, Palette::background, DocumentWindow::allButtons)
{
    setUsingNativeTitleBar(true);
    setContentOwned(new DevTransportComponent(engine), true);
    setResizable(true, true);
    centreWithSize(getWidth(), getHeight());
    setVisible(true);
    getContentComponent()->grabKeyboardFocus();
}

void MainWindow::closeButtonPressed()
{
    juce::JUCEApplication::getInstance()->systemRequestedQuit();
}

} // namespace trami
