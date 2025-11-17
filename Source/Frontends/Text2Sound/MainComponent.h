#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "../../Engine/MultiTrackLooperEngine.h"
#include "LooperTrack.h"
#include "../../CustomLookAndFeel.h"
#include "../Shared/MidiLearnManager.h"
#include "../Shared/MidiLearnComponent.h"
#include "../Shared/ConfigManager.h"

namespace Shared
{
    class SettingsDialog;
}

namespace Text2Sound
{

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
    
    std::vector<std::unique_ptr<Text2Sound::LooperTrack>> tracks;
    
    juce::TextButton syncButton;
    juce::TextButton gradioSettingsButton;
    juce::TextButton midiSettingsButton;
    juce::TextButton modelParamsButton;
    juce::TextButton settingsButton;
    juce::Label titleLabel;
    juce::Label audioDeviceDebugLabel;
    CustomLookAndFeel customLookAndFeel;
    juce::String gradioUrl { "https://opensound-ezaudio-controlnet.hf.space/" };
    mutable juce::CriticalSection gradioSettingsLock;
    
    // Shared model parameters for all tracks
    juce::var sharedModelParams;
    std::unique_ptr<Shared::ModelParameterDialog> modelParamsDialog;
    std::unique_ptr<Shared::SettingsDialog> settingsDialog;
    
    // Shared settings
    double pannerSmoothingTime{0.0}; // Smoothing time in seconds for panner trajectories
    
    Shared::MidiLearnOverlay midiLearnOverlay;

    void syncButtonClicked();
    void gradioSettingsButtonClicked();
    void updateAudioDeviceDebugInfo();
    void showGradioSettings();
    void setGradioUrl(const juce::String& newUrl);
    juce::String getGradioUrl() const;
    void midiSettingsButtonClicked();
    void showMidiSettings();
    void modelParamsButtonClicked();
    void showModelParams();
    void settingsButtonClicked();
    void showSettings();
    juce::var getSharedModelParams() const { return sharedModelParams; }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

} // namespace Text2Sound
