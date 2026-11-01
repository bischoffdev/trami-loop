#include "app/MainWindow.h"
#include "engine/AudioEngine.h"
#include "ui/TramiLookAndFeel.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace trami
{

class TramiApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return JUCE_APPLICATION_NAME_STRING; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override { return false; }

    void initialise(const juce::String&) override
    {
        juce::LookAndFeel::setDefaultLookAndFeel(&lookAndFeel_);

        engine_ = std::make_unique<AudioEngine>();
        if (const auto error = engine_->start(); error.isNotEmpty())
        {
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Audio device error", error);
            return;
        }

        mainWindow_ = std::make_unique<MainWindow>(getApplicationName(), *engine_);
    }

    void shutdown() override
    {
        mainWindow_.reset();
        engine_.reset();
        juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
    }

    void systemRequestedQuit() override { quit(); }

private:
    TramiLookAndFeel lookAndFeel_;
    std::unique_ptr<AudioEngine> engine_;
    std::unique_ptr<MainWindow> mainWindow_;
};

} // namespace trami

START_JUCE_APPLICATION(trami::TramiApplication)
