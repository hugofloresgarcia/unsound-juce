#include "STFTFeatureExtractor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <algorithm>

namespace EmbeddingSpaceSampler
{

std::vector<float> STFTFeatureExtractor::extract_features(
    const juce::File& audio_file,
    double duration_seconds,
    int hop_size,
    int fft_size)
{
    if (!audio_file.existsAsFile())
    {
        DBG("STFTFeatureExtractor: File does not exist: " + audio_file.getFullPathName());
        return {};
    }
    
    juce::AudioFormatManager format_manager;
    format_manager.registerBasicFormats();
    
    std::unique_ptr<juce::AudioFormatReader> reader(format_manager.createReaderFor(audio_file));
    if (reader == nullptr)
    {
        DBG("STFTFeatureExtractor: Could not create reader for file: " + audio_file.getFullPathName());
        return {};
    }
    
    double sample_rate = reader->sampleRate;
    int64_t max_samples = static_cast<int64_t>(duration_seconds * sample_rate);
    int64_t num_samples_to_read = juce::jmin(reader->lengthInSamples, max_samples);
    
    if (num_samples_to_read <= 0)
    {
        DBG("STFTFeatureExtractor: No samples to read");
        return {};
    }
    
    // Read audio data
    juce::AudioBuffer<float> temp_buffer(static_cast<int>(reader->numChannels), static_cast<int>(num_samples_to_read));
    
    if (!reader->read(&temp_buffer, 0, static_cast<int>(num_samples_to_read), 0, true, true))
    {
        DBG("STFTFeatureExtractor: Failed to read audio data");
        return {};
    }
    
    // Convert to mono if needed
    juce::AudioBuffer<float> mono_buffer(1, static_cast<int>(num_samples_to_read));
    if (temp_buffer.getNumChannels() == 1)
    {
        mono_buffer.copyFrom(0, 0, temp_buffer, 0, 0, static_cast<int>(num_samples_to_read));
    }
    else
    {
        // Mix down to mono
        mono_buffer.clear();
        for (int channel = 0; channel < temp_buffer.getNumChannels(); ++channel)
        {
            mono_buffer.addFrom(0, 0, temp_buffer, channel, 0, static_cast<int>(num_samples_to_read));
        }
        mono_buffer.applyGain(1.0f / static_cast<float>(temp_buffer.getNumChannels()));
    }
    
    return extract_features_from_buffer(mono_buffer, sample_rate, duration_seconds, hop_size, fft_size);
}

std::vector<float> STFTFeatureExtractor::extract_features_from_buffer(
    const juce::AudioBuffer<float>& audio_buffer,
    double sample_rate,
    double duration_seconds,
    int hop_size,
    int fft_size)
{
    if (audio_buffer.getNumSamples() == 0 || sample_rate <= 0)
    {
        DBG("STFTFeatureExtractor: Invalid buffer or sample rate");
        return {};
    }
    
    // Limit to duration_seconds
    int max_samples = static_cast<int>(duration_seconds * sample_rate);
    int num_samples = juce::jmin(audio_buffer.getNumSamples(), max_samples);
    
    if (num_samples <= 0)
    {
        return {};
    }
    
    // Calculate number of time frames
    int num_frames = static_cast<int>(std::ceil(static_cast<double>(num_samples - fft_size) / hop_size)) + 1;
    if (num_frames <= 0)
    {
        num_frames = 1; // At least one frame
    }
    
    // Number of frequency bins (DC + positive frequencies, excluding Nyquist if fft_size is even)
    int num_bins = (fft_size / 2) + 1;
    
    // Initialize FFT
    int fft_order = static_cast<int>(std::log2(fft_size));
    if ((1 << fft_order) != fft_size)
    {
        DBG("STFTFeatureExtractor: FFT size must be a power of 2");
        return {};
    }
    
    juce::dsp::FFT fft(fft_order);
    
    // Allocate buffers for FFT
    std::vector<juce::dsp::Complex<float>> fft_input(fft_size);
    std::vector<juce::dsp::Complex<float>> fft_output(fft_size);
    
    // Window function (Hamming)
    std::vector<float> window(fft_size);
    for (int i = 0; i < fft_size; ++i)
    {
        window[i] = 0.54f - 0.46f * std::cosf(2.0f * juce::MathConstants<float>::pi * i / (fft_size - 1));
    }
    
    // Extract features
    std::vector<float> features;
    features.reserve(num_frames * num_bins);
    
    const float* audio_data = audio_buffer.getReadPointer(0);
    
    for (int frame = 0; frame < num_frames; ++frame)
    {
        int start_sample = frame * hop_size;
        
        // Prepare windowed input
        for (int i = 0; i < fft_size; ++i)
        {
            int sample_idx = start_sample + i;
            float sample_value = 0.0f;
            
            if (sample_idx < num_samples)
            {
                sample_value = audio_data[sample_idx];
            }
            
            // Apply window
            fft_input[i] = juce::dsp::Complex<float>(sample_value * window[i], 0.0f);
        }
        
        // Perform FFT
        fft.perform(fft_input.data(), fft_output.data(), false);
        
        // Extract magnitude spectrum (only positive frequencies)
        for (int bin = 0; bin < num_bins; ++bin)
        {
            float real = fft_output[bin].real();
            float imag = fft_output[bin].imag();
            float magnitude = std::sqrt(real * real + imag * imag);
            
            // Use log magnitude for better feature representation
            float log_magnitude = std::log10(1.0f + magnitude);
            features.push_back(log_magnitude);
        }
    }
    
    return features;
}

} // namespace EmbeddingSpaceSampler

