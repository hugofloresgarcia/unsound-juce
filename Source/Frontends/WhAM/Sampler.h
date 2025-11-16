#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <atomic>
#include <memory>
#include "../../Engine/MultiTrackLooperEngine.h"
#include "../Shared/MidiLearnManager.h"
#include "../Shared/MidiLearnComponent.h"

namespace WhAM
{

// Sampler loads and plays back audio samples
class Sampler
{
public:
    Sampler();
    ~Sampler() = default;
    
    // Load sample from file
    bool loadSample(const juce::File& audioFile);
    
    // Trigger playback of the sample
    void trigger();
    
    // Generate next sample (returns 0.0 when sample is finished)
    float getNextSample();
    
    // Check if sample is currently playing
    bool isPlaying() const { return currentPosition.load() < sampleLength.load(); }
    
    // Check if a sample is loaded
    bool hasSample() const { return sampleLength.load() > 0; }
    
    // Get sample info
    juce::String getSampleName() const { return sampleName; }
    
private:
    juce::AudioFormatManager formatManager;
    std::unique_ptr<juce::AudioFormatReader> reader;
    std::vector<float> sampleData;  // Mono sample data
    std::atomic<size_t> currentPosition{0};
    std::atomic<size_t> sampleLength{0};
    juce::String sampleName;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Sampler)
};

// SamplerWindow - UI window for controlling the sampler
class SamplerWindow : public juce::DialogWindow
{
public:
    SamplerWindow(VampNetMultiTrackLooperEngine& engine, int numTracks, Shared::MidiLearnManager* midiManager = nullptr);
    ~SamplerWindow() override;
    
    void closeButtonPressed() override;
    
    int getSelectedTrack() const;
    bool isEnabled() const;
    
private:
    class ContentComponent : public juce::Component
    {
    public:
        ContentComponent(VampNetMultiTrackLooperEngine& engine, int numTracks, Shared::MidiLearnManager* midiManager);
        ~ContentComponent() override;
        
        void paint(juce::Graphics& g) override;
        void resized() override;
        
        int getSelectedTrack() const { return selectedTrack.load(); }
        bool isEnabled() const { return enabled.load(); }
        
    private:
        VampNetMultiTrackLooperEngine& looperEngine;
        Shared::MidiLearnManager* midiLearnManager;
        
        juce::ToggleButton enableButton;
        juce::Label trackLabel;
        juce::ComboBox trackSelector;
        juce::TextButton loadSampleButton;
        juce::TextButton triggerButton;
        juce::Label sampleNameLabel;
        juce::Label instructionsLabel;
        
        std::atomic<int> selectedTrack{0};
        std::atomic<bool> enabled{false};
        
        // MIDI learn support
        std::unique_ptr<Shared::MidiLearnable> triggerButtonLearnable;
        std::unique_ptr<Shared::MidiLearnMouseListener> triggerButtonMouseListener;
        juce::String parameterId;
        
        void enableButtonChanged();
        void trackSelectorChanged();
        void loadSampleButtonClicked();
        void triggerButtonClicked();
        
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ContentComponent)
    };
    
    ContentComponent* contentComponent;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SamplerWindow)
};

} // namespace WhAM

