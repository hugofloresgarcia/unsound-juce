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

namespace Basic
{

class MainComponent : public juce::Component,
                      public juce::Timer
{
public:
    MainComponent(int numTracks = 8);
    ~MainComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;
    
    MultiTrackLooperEngine& getLooperEngine() { return looperEngine; }

private:
    MultiTrackLooperEngine looperEngine;
    
    // MIDI learn support - must be declared before tracks so it's destroyed after them
    Shared::MidiLearnManager midiLearnManager;
    
    std::vector<std::unique_ptr<Basic::LooperTrack>> tracks;
    
    juce::TextButton syncButton;
    juce::TextButton audioSettingsButton;
    juce::TextButton midiSettingsButton;
    juce::Label titleLabel;

    juce::DialogWindow* audioSettingsWindow = nullptr;
    CustomLookAndFeel customLookAndFeel;
    
    Shared::MidiLearnOverlay midiLearnOverlay;

    void syncButtonClicked();
    void audioSettingsButtonClicked();
    void showAudioSettings();
    void midiSettingsButtonClicked();
    void showMidiSettings();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

} // namespace Basic

