#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <memory>

namespace EmbeddingSpaceSampler
{

class SamplerTrack;

// Audio processor that handles sampler track audio rendering
class SamplerAudioProcessor : public juce::AudioIODeviceCallback
{
public:
    SamplerAudioProcessor();
    ~SamplerAudioProcessor() override = default;
    
    // Register a sampler track to be processed
    void register_sampler_track(SamplerTrack* track);
    
    // Unregister a sampler track
    void unregister_sampler_track(SamplerTrack* track);
    
    // AudioIODeviceCallback
    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData, int numInputChannels,
                                         float* const* outputChannelData, int numOutputChannels,
                                         int numSamples,
                                         const juce::AudioIODeviceCallbackContext& context) override;
    
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;
    
private:
    std::vector<SamplerTrack*> sampler_tracks;
    juce::CriticalSection tracks_lock;
    
    juce::AudioBuffer<float> temp_input_buffer;
    juce::AudioBuffer<float> temp_output_buffer;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SamplerAudioProcessor)
};

} // namespace EmbeddingSpaceSampler

