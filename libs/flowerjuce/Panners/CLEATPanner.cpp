#include "CLEATPanner.h"
#include <algorithm>

CLEATPanner::CLEATPanner()
{
    m_pan_x.store(0.5f); // Default to center
    m_pan_y.store(0.5f); // Default to center
}

void CLEATPanner::set_pan(float x, float y)
{
    x = juce::jlimit(0.0f, 1.0f, x);
    y = juce::jlimit(0.0f, 1.0f, y);
    m_pan_x.store(x);
    m_pan_y.store(y);
}

float CLEATPanner::get_pan_x() const
{
    return m_pan_x.load();
}

float CLEATPanner::get_pan_y() const
{
    return m_pan_y.load();
}

void CLEATPanner::process_block(const float* const* input_channel_data,
                               int num_input_channels,
                               float* const* output_channel_data,
                               int num_output_channels,
                               int num_samples)
{
    // Verify we have the expected channels
    if (num_input_channels < 1 || num_output_channels < 16)
        return;

    // Get current pan position
    float x = m_pan_x.load();
    float y = m_pan_y.load();
    
    // Compute panning gains (16 channels, row-major)
    auto gains = PanningUtils::compute_cleat_gains(x, y);
    
    // Get input channel (mono)
    const float* input = input_channel_data[0];
    
    // Process samples
    for (int sample = 0; sample < num_samples; ++sample)
    {
        float input_sample = input[sample];
        
        // Apply gains to all 16 output channels (accumulate for multi-track mixing)
        for (int channel = 0; channel < 16; ++channel)
        {
            if (output_channel_data[channel] != nullptr)
            {
                output_channel_data[channel][sample] += input_sample * gains[channel];
            }
        }
    }
}

