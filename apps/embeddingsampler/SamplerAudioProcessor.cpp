#include "SamplerAudioProcessor.h"
#include "SamplerTrack.h"
#include <algorithm>

namespace EmbeddingSpaceSampler
{

SamplerAudioProcessor::SamplerAudioProcessor()
{
}

void SamplerAudioProcessor::register_sampler_track(SamplerTrack* track)
{
    juce::ScopedLock lock(tracks_lock);
    if (track != nullptr)
    {
        sampler_tracks.push_back(track);
    }
}

void SamplerAudioProcessor::unregister_sampler_track(SamplerTrack* track)
{
    juce::ScopedLock lock(tracks_lock);
    sampler_tracks.erase(
        std::remove(sampler_tracks.begin(), sampler_tracks.end(), track),
        sampler_tracks.end()
    );
}

void SamplerAudioProcessor::audioDeviceIOCallbackWithContext(const float* const* inputChannelData, int numInputChannels,
                                                            float* const* outputChannelData, int numOutputChannels,
                                                            int numSamples,
                                                            const juce::AudioIODeviceCallbackContext& context)
{
    // Clear output buffers first to prevent feedback and ensure clean output
    // This is safe because this app only uses sampler tracks, not looper tracks
    for (int channel = 0; channel < numOutputChannels; ++channel)
    {
        if (outputChannelData[channel] != nullptr)
        {
            juce::FloatVectorOperations::clear(outputChannelData[channel], numSamples);
        }
    }
    
    juce::ScopedLock lock(tracks_lock);
    
    // Process each sampler track
    for (auto* track : sampler_tracks)
    {
        if (track != nullptr)
        {
            // Ensure temp buffers are large enough
            if (temp_input_buffer.getNumSamples() < numSamples)
            {
                temp_input_buffer.setSize(numInputChannels, numSamples, false, false, true);
            }
            if (temp_output_buffer.getNumSamples() < numSamples)
            {
                temp_output_buffer.setSize(numOutputChannels, numSamples, false, false, true);
            }
            
            // Copy input (if needed) - but sampler tracks don't use input, so clear it
            temp_input_buffer.clear();
            
            // Clear temp output
            temp_output_buffer.clear();
            
            // Process track
            track->process_audio_block(
                temp_input_buffer.getArrayOfReadPointers(), numInputChannels,
                temp_output_buffer.getArrayOfWritePointers(), numOutputChannels,
                numSamples
            );
            
            // Mix into main output
            for (int channel = 0; channel < numOutputChannels; ++channel)
            {
                if (outputChannelData[channel] != nullptr && temp_output_buffer.getReadPointer(channel) != nullptr)
                {
                    juce::FloatVectorOperations::add(
                        outputChannelData[channel],
                        temp_output_buffer.getReadPointer(channel),
                        numSamples
                    );
                }
            }
        }
    }
}

void SamplerAudioProcessor::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    if (device != nullptr)
    {
        double sample_rate = device->getCurrentSampleRate();
        
        juce::ScopedLock lock(tracks_lock);
        for (auto* track : sampler_tracks)
        {
            if (track != nullptr)
            {
                track->set_sample_rate(sample_rate);
            }
        }
    }
}

void SamplerAudioProcessor::audioDeviceStopped()
{
    // Nothing special needed
}

} // namespace EmbeddingSpaceSampler

