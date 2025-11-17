#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_utils/juce_audio_utils.h>

class StartupDialog : public juce::Component,
                      public juce::Button::Listener
{
public:
    StartupDialog(juce::AudioDeviceManager& deviceManager);
    
    int get_num_tracks() const { return numTracks; }
    juce::String getSelectedPanner() const { return selectedPanner; }
    juce::AudioDeviceManager::AudioDeviceSetup getDeviceSetup() const;
    
    void resized() override;
    void buttonClicked(juce::Button* button) override;
    
    bool wasOkClicked() const { return okClicked; }
    
    void paint(juce::Graphics& g) override;
    
private:
    juce::AudioDeviceManager& audioDeviceManager;
    int numTracks{8};
    juce::String selectedPanner{"Stereo"};
    bool okClicked{false};
    
    juce::Label titleLabel;
    juce::Label numTracksLabel;
    juce::Slider numTracksSlider;
    juce::Label pannerLabel;
    juce::ComboBox pannerCombo;
    juce::AudioDeviceSelectorComponent audioDeviceSelector;
    juce::TextButton okButton;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StartupDialog)
};

