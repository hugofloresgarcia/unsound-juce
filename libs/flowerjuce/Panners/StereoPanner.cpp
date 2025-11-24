#include "StereoPanner.h"
#include <algorithm>

StereoPanner::StereoPanner()
{
    m_pan_position.store(0.5f); // Default to center
}

void StereoPanner::set_pan(float pan)
{
    pan = juce::jlimit(0.0f, 1.0f, pan);
    m_pan_position.store(pan);
}

float StereoPanner::get_pan() const
{
    return m_pan_position.load();
}

void StereoPanner::process_block(const float* const* input_channel_data,
                                int num_input_channels,
                                float* const* output_channel_data,
                                int num_output_channels,
                                int num_samples)
{
    // Verify we have the expected channels
    if (num_input_channels < 1 || num_output_channels < 2)
        return;

    // Get current pan position
    float pan = m_pan_position.load();
    
    // Compute panning gains
    auto [left_gain, right_gain] = PanningUtils::compute_stereo_gains(pan);
    
    // Get input channel (mono)
    const float* input = input_channel_data[0];
    
    // Get output channels (stereo)
    float* left_out = output_channel_data[0];
    float* right_out = output_channel_data[1];
    
    // Process samples (accumulate for multi-track mixing)
    for (int sample = 0; sample < num_samples; ++sample)
    {
        float input_sample = input[sample];
        left_out[sample] += input_sample * left_gain;
        right_out[sample] += input_sample * right_gain;
    }
}

