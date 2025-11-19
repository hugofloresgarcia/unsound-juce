#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <flowerjuce/LooperEngine/MultiTrackLooperEngine.h>
#include "SamplerTrack.h"
#include "EmbeddingSpaceView.h"
#include "SamplerAudioProcessor.h"
#include <flowerjuce/CustomLookAndFeel.h>
#include <flowerjuce/Components/MidiLearnManager.h>
#include <flowerjuce/Components/MidiLearnComponent.h>
#include <flowerjuce/Components/ConfigManager.h>
#include <flowerjuce/Components/SinksWindow.h>

namespace Shared
{
    class SettingsDialog;
}

namespace EmbeddingSpaceSampler
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
    
    // MIDI learn support
    Shared::MidiLearnManager midiLearnManager;
    
    // Sampler tracks
    std::vector<std::shared_ptr<SamplerTrack>> tracks;
    int current_track_index = 0; // Round-robin track selection
    
    // Audio processor for sampler tracks
    SamplerAudioProcessor sampler_audio_processor;
    
    // Embedding space visualization
    EmbeddingSpaceView embedding_view;
    
    juce::TextButton settingsButton;
    juce::TextButton sinksButton;
    juce::Label titleLabel;
    juce::Label audioDeviceDebugLabel;
    CustomLookAndFeel customLookAndFeel;
    
    // Sound palette path
    juce::String soundPalettePath;
    
    // Shared settings
    double pannerSmoothingTime{0.0};
    float cleatGainPower{1.0f};
    
    // DBScan settings
    int dbscanEps{15};
    int dbscanMinPts{3};
    
    Shared::MidiLearnOverlay midiLearnOverlay;

    void updateAudioDeviceDebugInfo();
    void settingsButtonClicked();
    void showSettings();
    void sinksButtonClicked();
    void setCLEATGainPower(float gainPower);
    void trigger_sample_on_track(int chunk_index, float velocity);
    
    // Settings dialog
    std::unique_ptr<Shared::SettingsDialog> settingsDialog;
    
    // Sinks window
    std::unique_ptr<SinksDialogWindow> sinksWindow;
    std::unique_ptr<flowerjuce::SinksWindow> sinksComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

} // namespace EmbeddingSpaceSampler

