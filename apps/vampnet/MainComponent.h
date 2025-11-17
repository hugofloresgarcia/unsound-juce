#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <flowerjuce/Engine/MultiTrackLooperEngine.h>
#include "LooperTrack.h"
#include <flowerjuce/ClickSynth.h>
#include <flowerjuce/Sampler.h>
#include <flowerjuce/CustomLookAndFeel.h>
#include <flowerjuce/Components/MidiLearnManager.h>
#include <flowerjuce/Components/MidiLearnComponent.h>

namespace Shared
{
    class SettingsDialog;
}

namespace VampNet
{

class MainComponent : public juce::Component,
                      public juce::Timer,
                      public juce::KeyListener
{
public:
    MainComponent(int numTracks = 8, const juce::String& pannerType = "Stereo");
    ~MainComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;
    bool keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent) override;
    
    VampNetMultiTrackLooperEngine& getLooperEngine() { return looperEngine; }

private:
    VampNetMultiTrackLooperEngine looperEngine;
    
    // MIDI learn support - must be declared before tracks so it's destroyed after them
    Shared::MidiLearnManager midiLearnManager;
    
    std::vector<std::unique_ptr<VampNet::LooperTrack>> tracks;
    
    juce::TextButton syncButton;
    juce::TextButton settingsButton;
    juce::TextButton clickSynthButton;
    juce::TextButton samplerButton;
    juce::Label titleLabel;
    juce::Label audioDeviceDebugLabel;
    CustomLookAndFeel customLookAndFeel;
    juce::String gradioUrl { "https://hugggof-vampnet-music.hf.space/" };
    mutable juce::CriticalSection gradioSettingsLock;
    
    Shared::MidiLearnOverlay midiLearnOverlay;
    
    // Click synth window
    std::unique_ptr<ClickSynthWindow> clickSynthWindow;
    
    // Sampler window
    std::unique_ptr<SamplerWindow> samplerWindow;
    
    // Settings dialog
    std::unique_ptr<Shared::SettingsDialog> settingsDialog;

    void syncButtonClicked();
    void showClickSynthWindow();
    void showSamplerWindow();
    void settingsButtonClicked();
    void showSettings();
    void updateAudioDeviceDebugInfo();
    void setGradioUrl(const juce::String& newUrl);
    juce::String getGradioUrl() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

} // namespace VampNet

