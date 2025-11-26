#include "QuadPanner.h"
#include <algorithm>

QuadPanner::QuadPanner()
{
    m_pan_x.store(0.5f); // Default to center
    m_pan_y.store(0.5f); // Default to center
}

void QuadPanner::set_pan(float x, float y)
{
    x = juce::jlimit(0.0f, 1.0f, x);
    y = juce::jlimit(0.0f, 1.0f, y);
    m_pan_x.store(x);
    m_pan_y.store(y);
}

float QuadPanner::get_pan_x() const
{
    return m_pan_x.load();
}

float QuadPanner::get_pan_y() const
{
    return m_pan_y.load();
}

void QuadPanner::process_block(const float* const* input_channel_data,
                              int num_input_channels,
                              float* const* output_channel_data,
                              int num_output_channels,
                              int num_samples)
{
    // Verify we have the expected channels
    if (num_input_channels < 1 || num_output_channels < 4)
        return;

    // Get current pan position
    float x = m_pan_x.load();
    float y = m_pan_y.load();
    
    // Compute panning gains [FL, FR, BL, BR]
    auto gains = PanningUtils::compute_quad_gains(x, y);
    
    // Get input channel (mono)
    const float* input = input_channel_data[0];
    
    // Get output channels (quad: FL, FR, BL, BR)
    float* fl_out = output_channel_data[0];
    float* fr_out = output_channel_data[1];
    float* bl_out = output_channel_data[2];
    float* br_out = output_channel_data[3];
    
    // Process samples (accumulate for multi-track mixing)
    for (int sample = 0; sample < num_samples; ++sample)
    {
        float input_sample = input[sample];
        fl_out[sample] += input_sample * gains[0]; // FL
        fr_out[sample] += input_sample * gains[1]; // FR
        bl_out[sample] += input_sample * gains[2]; // BL
        br_out[sample] += input_sample * gains[3]; // BR
    }
}

