#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include "../../Engine/MultiTrackLooperEngine.h"

namespace VampNet
{

// ClickSynth generates short click sounds (sine wave bursts)
class ClickSynth
{
public:
    ClickSynth();
    ~ClickSynth() = default;
    
    // Trigger a click (generates a short burst)
    void triggerClick();
    
    // Generate next sample of click (returns 0.0 when click is finished)
    float getNextSample(double sampleRate);
    
    // Check if click is currently playing
    bool isClickActive() const { return samplesRemaining.load() > 0; }
    
    // Set click parameters
    void setFrequency(float freq) { frequency.store(freq); }
    void setDuration(float seconds) { durationSeconds.store(seconds); }
    void setAmplitude(float amp) { amplitude.store(amp); }
    
private:
    std::atomic<float> frequency{1000.0f};  // Click frequency in Hz
    std::atomic<float> durationSeconds{0.01f};  // Click duration in seconds
    std::atomic<float> amplitude{0.8f};  // Click amplitude (0.0 to 1.0)
    
    std::atomic<int> samplesRemaining{0};
    std::atomic<double> phase{0.0};
    std::atomic<double> phaseIncrement{0.0};
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClickSynth)
};

// ClickSynthWindow - UI window for controlling the click synth
class ClickSynthWindow : public juce::DialogWindow
{
public:
    ClickSynthWindow(VampNetMultiTrackLooperEngine& engine, int numTracks);
    ~ClickSynthWindow() override;
    
    void closeButtonPressed() override;
    
    int getSelectedTrack() const;
    bool isEnabled() const;
    
private:
    class ContentComponent : public juce::Component
    {
    public:
        ContentComponent(VampNetMultiTrackLooperEngine& engine, int numTracks);
        ~ContentComponent() override = default;
        
        void paint(juce::Graphics& g) override;
        void resized() override;
        
        int getSelectedTrack() const { return selectedTrack.load(); }
        bool isEnabled() const { return enabled.load(); }
        
    private:
        VampNetMultiTrackLooperEngine& looperEngine;
        
        juce::ToggleButton enableButton;
        juce::Label trackLabel;
        juce::ComboBox trackSelector;
        juce::Label frequencyLabel;
        juce::Slider frequencySlider;
        juce::Label durationLabel;
        juce::Slider durationSlider;
        juce::Label amplitudeLabel;
        juce::Slider amplitudeSlider;
        juce::Label instructionsLabel;
        
        std::atomic<int> selectedTrack{0};
        std::atomic<bool> enabled{false};
        
        void enableButtonChanged();
        void trackSelectorChanged();
        void frequencySliderChanged();
        void durationSliderChanged();
        void amplitudeSliderChanged();
        
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ContentComponent)
    };
    
    ContentComponent* contentComponent;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClickSynthWindow)
};

} // namespace VampNet

