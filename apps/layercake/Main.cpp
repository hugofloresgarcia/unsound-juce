#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <optional>
#include "MainComponent.h"
#include "StartupDialog.h"
#include "LayerCakeLookAndFeel.h"

class LayerCakeApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "LayerCake"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise(const juce::String&) override
    {
        std::optional<juce::AudioDeviceManager::AudioDeviceSetup> startupDeviceSetup;
        bool continueStartup = true;

        {
            juce::AudioDeviceManager tempDeviceManager;
            tempDeviceManager.initialiseWithDefaultDevices(2, 2);

            auto startupDialog = std::make_unique<LayerCakeStartupDialog>(tempDeviceManager);
            auto* dialogPtr = startupDialog.get();

            LayerCakeLookAndFeel dialogLookAndFeel;
            startupDialog->setLookAndFeel(&dialogLookAndFeel);

            juce::DialogWindow::LaunchOptions dialogOptions;
            dialogOptions.content.setNonOwned(startupDialog.release());
            dialogOptions.dialogTitle = "LayerCake Audio Setup";
            dialogOptions.dialogBackgroundColour = juce::Colours::black;
            dialogOptions.escapeKeyTriggersCloseButton = false;
            dialogOptions.useNativeTitleBar = false;
            dialogOptions.resizable = false;

           #if JUCE_MODAL_LOOPS_PERMITTED
            dialogOptions.componentToCentreAround = juce::TopLevelWindow::getActiveTopLevelWindow();
            juce::Process::makeForegroundProcess();
            const int result = dialogOptions.runModal();
            DBG("[LayerCakeApplication] Startup dialog result: " << result);

            if (dialogPtr != nullptr)
                dialogPtr->setLookAndFeel(nullptr);

            const bool okSelected = (result == 1 && dialogPtr != nullptr && dialogPtr->wasOkClicked());
            if (okSelected)
            {
                startupDeviceSetup = dialogPtr->getDeviceSetup();
                DBG("[LayerCakeApplication] Captured startup device selection");
            }
            else
            {
                DBG("[LayerCakeApplication] Startup dialog cancelled; exiting");
                continueStartup = false;
            }

            if (dialogPtr != nullptr)
                delete dialogPtr;

            if (!continueStartup)
            {
                quit();
                return;
            }
           #else
            auto* dialogWindow = dialogOptions.launchAsync();
            if (dialogWindow != nullptr)
            {
                dialogWindow->setAlwaysOnTop(true);
                dialogWindow->toFront(true);
                dialogWindow->enterModalState(true, nullptr, true);
            }
            DBG("[LayerCakeApplication] Modal loops disabled; continuing without blocking dialog");
           #endif
        }

        mainWindow.reset(new MainWindow(getApplicationName(), startupDeviceSetup));
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
        explicit MainWindow(juce::String name,
                            std::optional<juce::AudioDeviceManager::AudioDeviceSetup> deviceSetup)
            : DocumentWindow(name,
                             juce::Colours::black,
                             DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar(true);
            auto* component = new LayerCakeApp::MainComponent(std::move(deviceSetup));
            setContentOwned(component, true);
            centreWithSize(component->getWidth(), component->getHeight());
            setResizable(true, true);
            setResizeLimits(720, 600, 3200, 2000);
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


