#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <flowerjuce/LooperEngine/MultiTrackLooperEngine.h>
#include "LooperTrack.h"
#include <flowerjuce/CustomLookAndFeel.h>
#include <flowerjuce/Components/MidiLearnManager.h>
#include <flowerjuce/Components/MidiLearnComponent.h>

namespace Shared
{
    class SettingsDialog;
}

namespace Basic
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

    // Update channel selectors for all tracks (call after device is initialized)
    void updateAllChannelSelectors();

private:
    MultiTrackLooperEngine looperEngine;
    
    // MIDI learn support - must be declared before tracks so it's destroyed after them
    Shared::MidiLearnManager midiLearnManager;
    
    std::vector<std::unique_ptr<Basic::LooperTrack>> tracks;
    
    juce::TextButton syncButton;
    juce::TextButton settingsButton;
    juce::Label titleLabel;
    juce::Label audioDeviceDebugLabel;
    CustomLookAndFeel customLookAndFeel;
    
    Shared::MidiLearnOverlay midiLearnOverlay;
    
    // Settings dialog
    std::unique_ptr<Shared::SettingsDialog> settingsDialog;

    void syncButtonClicked();
    void settingsButtonClicked();
    void showSettings();
    void updateAudioDeviceDebugInfo();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

} // namespace Basic

