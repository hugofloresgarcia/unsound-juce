#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "StartupDialog.h"
#include "MainComponent.h"
#include <flowerjuce/CustomLookAndFeel.h>

class BasicApplication : public juce::JUCEApplication
{
public:
    BasicApplication() {}

    const juce::String getApplicationName() override { return "Basic Tape Looper"; }
    const juce::String getApplicationVersion() override { return "1.0.0"; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise(const juce::String& commandLine) override
    {
        // Show startup dialog before creating main window
        int numTracks = 8; // Default value, will be updated from dialog
        juce::String selectedPanner = "Stereo"; // Default panner
        juce::AudioDeviceManager::AudioDeviceSetup deviceSetup;
        
        {
            juce::AudioDeviceManager tempDeviceManager;
            // Initialize with default devices so the dialog shows current audio setup
            tempDeviceManager.initialiseWithDefaultDevices(2, 2);
            
            auto startupDialog = std::make_unique<StartupDialog>(tempDeviceManager);
            StartupDialog* dialogPtr = startupDialog.get(); // Store pointer before releasing
            
            CustomLookAndFeel customLookAndFeel;
            startupDialog->setLookAndFeel(&customLookAndFeel);
            
            juce::DialogWindow::LaunchOptions dialogOptions;
            dialogOptions.content.setNonOwned(startupDialog.release()); // Don't auto-delete, we'll manage it
            dialogOptions.dialogTitle = "Basic Tape Looper Setup";
            dialogOptions.dialogBackgroundColour = juce::Colours::black;
            dialogOptions.escapeKeyTriggersCloseButton = false;
            dialogOptions.useNativeTitleBar = false;
            dialogOptions.resizable = false;
            
            #if JUCE_MODAL_LOOPS_PERMITTED
            dialogOptions.componentToCentreAround = juce::TopLevelWindow::getActiveTopLevelWindow();
            juce::Process::makeForegroundProcess();
            int result = dialogOptions.runModal();
            
            DBG("[Main] Dialog result: " << result);
            
            // Access the dialog component to read the value
            if (result == 1 && dialogPtr != nullptr)
            {
                if (dialogPtr->wasOkClicked())
                {
                    numTracks = dialogPtr->getNumTracks();
                    selectedPanner = dialogPtr->getSelectedPanner();
                    juce::Logger::writeToLog("Selected number of tracks: " + juce::String(numTracks));
                    juce::Logger::writeToLog("Selected panner: " + selectedPanner);
                    
                    // Get device setup from the dialog (which has the updated setup with all channels enabled)
                    DBG("[Main] Getting device setup from StartupDialog...");
                    deviceSetup = dialogPtr->getDeviceSetup();
                    
                    DBG("[Main] Device setup retrieved from StartupDialog:");
                    DBG("  outputDeviceName: " << deviceSetup.outputDeviceName);
                    DBG("  inputDeviceName: " << deviceSetup.inputDeviceName);
                    DBG("  sampleRate: " << deviceSetup.sampleRate);
                    DBG("  bufferSize: " << deviceSetup.bufferSize);
                    DBG("  useDefaultInputChannels: " << (deviceSetup.useDefaultInputChannels ? "true" : "false"));
                    DBG("  useDefaultOutputChannels: " << (deviceSetup.useDefaultOutputChannels ? "true" : "false"));
                    DBG("  inputChannels bits: " << deviceSetup.inputChannels.toString(2));
                    DBG("  outputChannels bits: " << deviceSetup.outputChannels.toString(2));
                }
                else
                {
                    juce::Logger::writeToLog("Dialog OK not clicked, exiting application");
                    DBG("[Main] Dialog OK not clicked, exiting");
                    // Clean up the dialog component manually since we set auto-delete to false
                    if (dialogPtr != nullptr)
                    {
                        delete dialogPtr;
                    }
                    quit();
                    return;
                }
            }
            else
            {
                juce::Logger::writeToLog("Dialog cancelled (result=" + juce::String(result) + "), exiting application");
                DBG("[Main] Dialog cancelled or dialogPtr is null, exiting");
                // Clean up the dialog component manually since we set auto-delete to false
                if (dialogPtr != nullptr)
                {
                    delete dialogPtr;
                }
                quit();
                return;
            }
            
            // Clean up the dialog component manually since we set auto-delete to false
            if (dialogPtr != nullptr)
            {
                delete dialogPtr;
            }
            #else
            // Fallback if modal loops not permitted - use async
            auto* dialogWindow = dialogOptions.launchAsync();
            if (dialogWindow != nullptr)
            {
                dialogWindow->setAlwaysOnTop(true);
                dialogWindow->toFront(true);
                dialogWindow->enterModalState(true, nullptr, true);
            }
            // Note: In async mode, we can't reliably get the result here
            // For now, default to 4 tracks
            #endif
        }
        
        mainWindow.reset(new MainWindow(getApplicationName(), numTracks, selectedPanner, deviceSetup));
    }

    void shutdown() override
    {
        mainWindow = nullptr;
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    void anotherInstanceStarted(const juce::String& commandLine) override
    {
    }

    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow(juce::String name, int numTracks, const juce::String& pannerType, const juce::AudioDeviceManager::AudioDeviceSetup& deviceSetup)
            : DocumentWindow(name,
                            juce::Desktop::getInstance().getDefaultLookAndFeel()
                                .findColour(juce::ResizableWindow::backgroundColourId),
                            DocumentWindow::allButtons)
        {
            DBG("[MainWindow] Constructor called");
            DBG("[MainWindow] numTracks: " << numTracks);
            DBG("[MainWindow] Device setup received:");
            DBG("  outputDeviceName: " << deviceSetup.outputDeviceName);
            DBG("  inputDeviceName: " << deviceSetup.inputDeviceName);
            DBG("  sampleRate: " << deviceSetup.sampleRate);
            DBG("  bufferSize: " << deviceSetup.bufferSize);
            DBG("  useDefaultInputChannels: " << (deviceSetup.useDefaultInputChannels ? "true" : "false"));
            DBG("  useDefaultOutputChannels: " << (deviceSetup.useDefaultOutputChannels ? "true" : "false"));
            DBG("  inputChannels bits: " << deviceSetup.inputChannels.toString(2));
            DBG("  outputChannels bits: " << deviceSetup.outputChannels.toString(2));
            
            setUsingNativeTitleBar(true);
            
            // Create Basic frontend component
            DBG("[MainWindow] Creating Basic frontend...");
            auto* basicComponent = new Basic::MainComponent(numTracks, pannerType);
            
            DBG("[MainWindow] Setting device setup on Basic looper engine...");
            auto& deviceManager = basicComponent->getLooperEngine().get_audio_device_manager();
            
            // CRITICAL: Set device type first, otherwise setAudioDeviceSetup will fail silently
            // Find the device type that contains our device
            juce::String deviceType;
            const auto& deviceTypes = deviceManager.getAvailableDeviceTypes();
            for (int i = 0; i < deviceTypes.size(); ++i)
            {
                auto* type = deviceTypes[i];
                auto outputDevices = type->getDeviceNames(false);
                auto inputDevices = type->getDeviceNames(true);
                
                bool foundDevice = false;
                if (deviceSetup.outputDeviceName.isNotEmpty())
                    foundDevice = outputDevices.contains(deviceSetup.outputDeviceName);
                if (!foundDevice && deviceSetup.inputDeviceName.isNotEmpty())
                    foundDevice = inputDevices.contains(deviceSetup.inputDeviceName);
                
                if (foundDevice)
                {
                    deviceType = type->getTypeName();
                    DBG("[MainWindow] Found device type: " << deviceType);
                    break;
                }
            }
            
            if (deviceType.isNotEmpty())
            {
                DBG("[MainWindow] Setting device type to: " << deviceType);
                deviceManager.setCurrentAudioDeviceType(deviceType, false);
            }
            else
            {
                DBG("[MainWindow] WARNING: Could not find device type, using default");
            }
            
            auto error = deviceManager.setAudioDeviceSetup(deviceSetup, true);
            if (error.isNotEmpty())
            {
                DBG("[MainWindow] ERROR setting device setup: " << error);
            }
            else
            {
                DBG("[MainWindow] Device setup applied successfully");
                
                // Verify device after setup
                auto* verifyDevice = deviceManager.getCurrentAudioDevice();
                if (verifyDevice != nullptr)
                {
                    DBG("[MainWindow] Device after setup: " << verifyDevice->getName());
                    DBG("[MainWindow] Active input channels: " << verifyDevice->getActiveInputChannels().countNumberOfSetBits());
                    DBG("[MainWindow] Active output channels: " << verifyDevice->getActiveOutputChannels().countNumberOfSetBits());
                }
                else
                {
                    DBG("[MainWindow] WARNING: No device after setup!");
                }
            }
            
            DBG("[MainWindow] Starting audio...");
            basicComponent->getLooperEngine().start_audio();
            
            // Update channel selectors now that device is initialized
            basicComponent->updateAllChannelSelectors();
            
            // Verify device after startAudio
            auto* finalDevice = deviceManager.getCurrentAudioDevice();
            if (finalDevice != nullptr)
            {
                DBG("[MainWindow] Final device after startAudio: " << finalDevice->getName());
                DBG("[MainWindow] Final active input channels: " << finalDevice->getActiveInputChannels().countNumberOfSetBits());
                DBG("[MainWindow] Final active output channels: " << finalDevice->getActiveOutputChannels().countNumberOfSetBits());
            }
            
            setContentOwned(basicComponent, true);

            #if JUCE_IOS || JUCE_ANDROID
            setFullScreen(true);
            #else
            setResizable(false, false); // Fixed window size
            // Use the content component's size instead of a fixed size
            centreWithSize(basicComponent->getWidth(), basicComponent->getHeight());
            #endif

            setVisible(true);
        }

        void closeButtonPressed() override
        {
            JUCEApplication::getInstance()->systemRequestedQuit();
        }

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
    };

private:
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION(BasicApplication)

