#pragma once

#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>

// LowPassFilter UGen - processes audio blocks with a low pass filter
class LowPassFilter
{
public:
    LowPassFilter();
    ~LowPassFilter() = default;

    // Prepare the filter for processing (call when sample rate changes)
    void prepare(double sample_rate, int maximum_block_size = 512);

    // Process a block of audio samples (mono input/output)
    // channel_data: pointer to mono audio buffer
    // num_samples: number of samples in the block
    void process_block(float* channel_data, int num_samples);

    // Set cutoff frequency (in Hz)
    // Automatically clamps to valid range (20Hz to Nyquist)
    void set_cutoff(float cutoff_hz);

    // Get current cutoff frequency (in Hz)
    float get_cutoff() const { return m_filter_cutoff.load(); }

private:
    juce::dsp::IIR::Filter<float> m_low_pass_filter;
    juce::dsp::IIR::Coefficients<float>::Ptr m_filter_coefficients;
    std::atomic<float> m_filter_cutoff{20000.0f}; // Default to 20kHz (no filtering)
    double m_current_sample_rate{44100.0};
};

