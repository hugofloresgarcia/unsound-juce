#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <flowerjuce/Panners/CLEATPanner.h>
#include <flowerjuce/Panners/Panner2DComponent.h>
#include <random>
#include <array>

namespace CLEATPinkNoiseTest
{

class MainComponent : public juce::Component,
                      public juce::AudioIODeviceCallback,
                      public juce::Timer,
                      public juce::Slider::Listener
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

    // AudioIODeviceCallback  
    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                          int numInputChannels,
                                          float* const* outputChannelData,
                                          int numOutputChannels,
                                          int numSamples,
                                          const juce::AudioIODeviceCallbackContext& context) override;
    
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

private:
    juce::AudioDeviceManager audioDeviceManager;
    
    // CLEAT panner
    CLEATPanner cleatPanner;
    
    // Pink noise generator state
    std::array<float, 7> pinkNoiseState{0.0f};
    std::mt19937 randomGenerator;
    std::uniform_real_distribution<float> whiteNoiseDist{-1.0f, 1.0f};
    
    // Audio buffer for mono input to panner
    std::vector<float> monoBuffer;
    static constexpr int maxBufferSize{4096};
    
    double currentSampleRate{44100.0};
    
    // Level control (in dB, converted to linear)
    float outputLevelDb{-20.0f};
    float outputLevelLinear{0.1f};
    
    // Channel level meters (peak detection with decay)
    std::array<std::atomic<float>, 16> channelLevels;
    static constexpr float levelDecayFactor{0.9995f};
    
    // Debugging counters
    std::atomic<int> callbackCount{0};
    std::atomic<int> samplesProcessed{0};
    
    // UI components
    juce::Label panLabel;
    juce::Label levelLabel;
    juce::Label debugLabel;
    juce::Slider levelSlider;
    juce::TextButton startStopButton;
    juce::TextButton startAudioButton;
    std::unique_ptr<Panner2DComponent> panner2DComponent;
    bool isPlaying{false};
    bool audioDeviceInitialized{false};
    
    // Helper to draw a single channel level meter
    void drawChannelMeter(juce::Graphics& g, juce::Rectangle<int> area, int channel, float level);
    
    // Store meters area for paint()
    juce::Rectangle<int> metersArea;
    
    void startStopButtonClicked();
    void startAudioButtonClicked();
    void levelSliderValueChanged();
    void panPositionChanged(float x, float y);
    
    // Slider::Listener
    void sliderValueChanged(juce::Slider* slider) override;
    
    float generatePinkNoise();
    float dbToLinear(float db);
    float linearToDb(float linear);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

} // namespace CLEATPinkNoiseTest

