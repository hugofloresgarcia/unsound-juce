#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <flowerjuce/Engine/MultiTrackLooperEngine.h>
#include "LooperTrack.h"
#include <flowerjuce/CustomLookAndFeel.h>
#include <flowerjuce/Components/MidiLearnManager.h>
#include <flowerjuce/Components/MidiLearnComponent.h>
#include <flowerjuce/Components/ConfigManager.h>
#include <flowerjuce/Components/SinksWindow.h>
#include "CLAP/ONNXModelManager.h"

namespace Shared
{
    class SettingsDialog;
}

namespace CLAPText2Sound
{
    // Custom DialogWindow that properly handles close button
    class SinksDialogWindow : public juce::DialogWindow
    {
    public:
        SinksDialogWindow(const juce::String& name, juce::Colour colour)
            : juce::DialogWindow(name, colour, true, true)
        {
        }

        void closeButtonPressed() override
        {
            // Hide the window instead of asserting
            setVisible(false);
        }
    };

class MainComponent : public juce::Component,
                      public juce::Timer
{
public:
    MainComponent(int numTracks = 8, const juce::String& pannerType = "Stereo", const juce::String& soundPalettePath = juce::String());
    ~MainComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;
    
    MultiTrackLooperEngine& getLooperEngine() { return looperEngine; }

private:
    MultiTrackLooperEngine looperEngine;
    
    // MIDI learn support - must be declared before tracks so it's destroyed after them
    Shared::MidiLearnManager midiLearnManager;
    
    std::vector<std::shared_ptr<CLAPText2Sound::LooperTrack>> tracks;
    
    juce::TextButton syncButton;
    juce::TextButton settingsButton;
    juce::TextButton sinksButton;
    juce::Label titleLabel;
    juce::Label audioDeviceDebugLabel;
    CustomLookAndFeel customLookAndFeel;
    
    // Sound palette path
    juce::String soundPalettePath;
    
    // Trajectory directory for saving trajectories
    juce::String trajectoryDir;
    
    // Cached ONNX model manager (shared across all tracks for performance)
    std::unique_ptr<CLAPText2Sound::ONNXModelManager> cachedModelManager;
    
    // Shared settings
    double pannerSmoothingTime{0.0}; // Smoothing time in seconds for panner trajectories
    float cleatGainPower{1.0f}; // CLEAT gain power factor (default 1.0 = no change)
    
    Shared::MidiLearnOverlay midiLearnOverlay;

    void syncButtonClicked();
    void updateAudioDeviceDebugInfo();
    void settingsButtonClicked();
    void showSettings();
    void sinksButtonClicked();
    void setCLEATGainPower(float gainPower);
    
    // Settings dialog
    std::unique_ptr<Shared::SettingsDialog> settingsDialog;
    
    // Sinks window
    std::unique_ptr<SinksDialogWindow> sinksWindow;
    std::unique_ptr<flowerjuce::SinksWindow> sinksComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

} // namespace CLAPText2Sound

