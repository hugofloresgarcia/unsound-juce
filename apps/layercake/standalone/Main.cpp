#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "../core/LayerCakeProcessor.h"
#include "../core/LayerCakeComponent.h"
#include "StandaloneSettings.h"

namespace LayerCakeApp
{

class LayerCakeApplication : public juce::JUCEApplication
{
public:
    LayerCakeApplication() {}

    const juce::String getApplicationName() override { return "LayerCake"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise(const juce::String&) override
    {
        m_processor = std::make_unique<LayerCakeProcessor>();
        
        m_device_manager.initialise(2, 2, nullptr, true);
        
        m_player.setProcessor(m_processor.get());
        m_device_manager.addAudioCallback(&m_player);
        m_device_manager.addMidiInputCallback(juce::String(), &m_player);
        
        m_main_window = std::make_unique<MainWindow>(getApplicationName(), *m_processor, m_device_manager);
    }

    void shutdown() override
    {
        m_device_manager.removeAudioCallback(&m_player);
        m_device_manager.removeMidiInputCallback(juce::String(), &m_player);
        m_player.setProcessor(nullptr);
        m_main_window = nullptr;
        m_processor = nullptr;
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    void anotherInstanceStarted(const juce::String&) override {}

    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow(juce::String name, LayerCakeProcessor& processor, juce::AudioDeviceManager& deviceManager)
            : DocumentWindow(name, juce::Colours::black, DocumentWindow::allButtons),
              m_device_manager(deviceManager),
              m_processor(processor)
        {
            setUsingNativeTitleBar(true);
            
            // Create the editor (LayerCakeComponent)
            if (auto* editor = dynamic_cast<LayerCakeComponent*>(processor.createEditor()))
            {
                // Hook up settings callback
                editor->onSettingsRequested = [this]() { showSettings(); };
                setContentOwned(editor, true);
            }
            else
            {
                // Fallback if createEditor returns something else (unlikely)
                setContentOwned(processor.createEditor(), true);
            }

           #if JUCE_IOS
            setFullScreen(true);
           #else
            setResizable(true, true);
            setResizeLimits(720, 600, 3200, 2000);
            centreWithSize(getWidth(), getHeight());
           #endif
           
            setVisible(true);
        }

        void closeButtonPressed() override
        {
            JUCEApplication::getInstance()->systemRequestedQuit();
        }
        
        void showSettings()
        {
            if (m_settings_window == nullptr)
            {
                m_settings_window = std::make_unique<StandaloneSettingsWindow>(m_device_manager, m_processor.getEngine());
            }
            m_settings_window->setVisible(true);
            m_settings_window->toFront(true);
        }
        
        // Helper to forward audio status to editor for display
        void updateAudioStatus()
        {
            // Since we can't easily push to Editor from here without casting or event listener, 
            // and Editor pulls in timerCallback, we need to make sure Editor knows standalone state.
            // But Editor uses m_processor.getEngine()... wait, standalone handles audio callback.
            // The Processor doesn't know if audio callback is running unless we tell it?
            // In plugin, host calls processBlock.
            // In standalone, we added callback.
            // Processor::processBlock is called by player.
            // So if player is running, processBlock runs. 
            // We don't need to do anything special, except maybe update device name?
            // The status HUD in component checks "is_audio_enabled" flag in component which was removed.
            // Let's fix Component to check Engine or Processor.
        }

    private:
        juce::AudioDeviceManager& m_device_manager;
        LayerCakeProcessor& m_processor;
        std::unique_ptr<StandaloneSettingsWindow> m_settings_window;
        
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
    };

private:
    std::unique_ptr<LayerCakeProcessor> m_processor;
    juce::AudioProcessorPlayer m_player;
    juce::AudioDeviceManager m_device_manager;
    std::unique_ptr<MainWindow> m_main_window;
};

} // namespace LayerCakeApp

START_JUCE_APPLICATION(LayerCakeApp::LayerCakeApplication)
