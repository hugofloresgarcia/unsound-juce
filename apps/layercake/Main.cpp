#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include "MainComponent.h"
#include "LayerCakeLookAndFeel.h"

class LayerCakeApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "LayerCake"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise(const juce::String&) override
    {
        // No startup dialog - audio starts OFF by default
        // User enables audio through the settings window
        DBG("[LayerCakeApplication] Starting with audio disabled");
        mainWindow.reset(new MainWindow(getApplicationName()));
    }

    void shutdown() override
    {
        mainWindow = nullptr;
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    class MainWindow : public juce::DocumentWindow
    {
    public:
        explicit MainWindow(juce::String name)
            : DocumentWindow(name,
                             juce::Colours::black,
                             DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar(true);
            auto* component = new LayerCakeApp::MainComponent();
            setContentOwned(component, true);
           #if JUCE_IOS
            setFullScreen(true);
           #else
            centreWithSize(component->getWidth(), component->getHeight());
            setResizable(true, true);
            setResizeLimits(720, 600, 3200, 2000);
           #endif
            setVisible(true);
        }

        void closeButtonPressed() override
        {
            JUCEApplication::getInstance()->systemRequestedQuit();
        }
    };

private:
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION(LayerCakeApplication)
