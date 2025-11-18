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

namespace Shared
{
    class SettingsDialog;
}

namespace Text2Sound
{
    class VizWindow;

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

    // Custom DialogWindow for viz window
    class VizDialogWindow : public juce::DialogWindow
    {
    public:
        VizDialogWindow(const juce::String& name, juce::Colour colour)
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
    MainComponent(int numTracks = 8, const juce::String& pannerType = "Stereo");
    ~MainComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;
    
    MultiTrackLooperEngine& getLooperEngine() { return looperEngine; }

private:
    MultiTrackLooperEngine looperEngine;
    
    // MIDI learn support - must be declared before tracks so it's destroyed after them
    Shared::MidiLearnManager midiLearnManager;
    
    std::vector<std::shared_ptr<Text2Sound::LooperTrack>> tracks;
    
    juce::TextButton syncButton;
    juce::TextButton modelParamsButton;
    juce::TextButton settingsButton;
    juce::TextButton sinksButton;
    juce::TextButton vizButton;
    juce::Label titleLabel;
    juce::Label audioDeviceDebugLabel;
    CustomLookAndFeel customLookAndFeel;
    juce::String gradioUrl { "https://opensound-ezaudio-controlnet.hf.space/" };
    mutable juce::CriticalSection gradioSettingsLock;
    
    // Trajectory directory for saving trajectories
    juce::String trajectoryDir;
    
    // Shared model parameters for all tracks
    juce::var sharedModelParams;
    std::unique_ptr<Shared::ModelParameterDialog> modelParamsDialog;
    std::unique_ptr<Shared::SettingsDialog> settingsDialog;
    
    // Shared settings
    double pannerSmoothingTime{0.0}; // Smoothing time in seconds for panner trajectories
    float cleatGainPower{1.0f}; // CLEAT gain power factor (default 1.0 = no change)
    bool generateTriggersNewPath{false}; // If true, automatically trigger random path when generation completes
    
    Shared::MidiLearnOverlay midiLearnOverlay;

    void syncButtonClicked();
    void updateAudioDeviceDebugInfo();
    void setGradioUrl(const juce::String& newUrl);
    juce::String getGradioUrl() const;
    void modelParamsButtonClicked();
    void showModelParams();
    void settingsButtonClicked();
    void showSettings();
    void sinksButtonClicked();
    void vizButtonClicked();
    juce::var getSharedModelParams() const { return sharedModelParams; }
    void setCLEATGainPower(float gainPower);
    
    // Sinks window
    std::unique_ptr<SinksDialogWindow> sinksWindow;
    std::unique_ptr<flowerjuce::SinksWindow> sinksComponent;
    
    // Viz window
    std::unique_ptr<VizDialogWindow> vizWindow;
    std::unique_ptr<Text2Sound::VizWindow> vizComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

} // namespace Text2Sound
