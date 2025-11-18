#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>

namespace EmbeddingSpaceSampler
{

class STFTFeatureExtractor
{
public:
    // Extract STFT features from the first 1.5 seconds of audio
    // Returns a flattened vector of STFT magnitudes (frequency bins × time frames)
    // Parameters:
    //   - audio_file: Audio file to extract features from
    //   - duration_seconds: Duration to extract (default 1.5s)
    //   - hop_size: Hop size in samples (default 512)
    //   - fft_size: FFT size (default 2048)
    // Returns empty vector on failure
    static std::vector<float> extract_features(
        const juce::File& audio_file,
        double duration_seconds = 1.5,
        int hop_size = 512,
        int fft_size = 2048
    );
    
    // Extract STFT features from audio buffer
    static std::vector<float> extract_features_from_buffer(
        const juce::AudioBuffer<float>& audio_buffer,
        double sample_rate,
        double duration_seconds = 1.5,
        int hop_size = 512,
        int fft_size = 2048
    );
    
private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(STFTFeatureExtractor)
};

} // namespace EmbeddingSpaceSampler

