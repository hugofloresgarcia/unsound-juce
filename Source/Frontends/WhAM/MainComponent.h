#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "../VampNet/ClickSynth.h"
#include "../VampNet/Sampler.h"
#include "../../Engine/MultiTrackLooperEngine.h"
#include "LooperTrack.h"
#include "ClickSynth.h"
#include "Sampler.h"
#include "TokenVisualizer.h"
#include "../../CustomLookAndFeel.h"
#include "../Shared/MidiLearnManager.h"
#include "../Shared/MidiLearnComponent.h"

namespace WhAM
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
    bool keyStateChanged(bool isKeyDown, juce::Component* originatingComponent) override;
    
    VampNetMultiTrackLooperEngine& getLooperEngine() { return looperEngine; }

private:
    VampNetMultiTrackLooperEngine looperEngine;
    
    // MIDI learn support - must be declared before tracks so it's destroyed after them
    Shared::MidiLearnManager midiLearnManager;
    
    std::vector<std::unique_ptr<WhAM::LooperTrack>> tracks;
    
    // Track selection and recording state
    int activeTrackIndex = 0;  // Currently selected track (0-7)
    bool isRecordingHeld = false;  // True while 'r' key is held
    
    juce::TextButton syncButton;
    juce::TextButton gradioSettingsButton;
    juce::TextButton midiSettingsButton;
    juce::TextButton clickSynthButton;
    juce::TextButton samplerButton;
    juce::TextButton vizButton;
    juce::TextButton overflowButton;  // "..." button for overflow menu
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
    
    // Token visualizer window
    std::unique_ptr<TokenVisualizerWindow> vizWindow;

    void syncButtonClicked();
    void showClickSynthWindow();
    void showSamplerWindow();
    void showVizWindow();
    void gradioSettingsButtonClicked();
    void updateAudioDeviceDebugInfo();
    void showGradioSettings();
    void setGradioUrl(const juce::String& newUrl);
    juce::String getGradioUrl() const;
    void midiSettingsButtonClicked();
    void showMidiSettings();
    void showOverflowMenu();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

} // namespace WhAM

