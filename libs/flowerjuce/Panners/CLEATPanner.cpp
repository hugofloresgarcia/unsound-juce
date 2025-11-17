#include "CLEATPanner.h"
#include <algorithm>

CLEATPanner::CLEATPanner()
{
    m_pan_x.store(0.5f); // Default to center
    m_pan_y.store(0.5f); // Default to center
    
    // Initialize smoothing with default sample rate (will be updated in prepare())
    // 20ms ramp time matching Max/MSP line~ 0. with $1 20 message
    constexpr double default_sample_rate = 44100.0;
    constexpr double ramp_time_ms = 20.0;
    m_smooth_x.reset(default_sample_rate, ramp_time_ms / 1000.0);
    m_smooth_y.reset(default_sample_rate, ramp_time_ms / 1000.0);
    m_smooth_x.setCurrentAndTargetValue(0.5f);
    m_smooth_y.setCurrentAndTargetValue(0.5f);
}

void CLEATPanner::prepare(double sample_rate)
{
    // Set sample rate and ramp time for smoothing (20ms matching Max/MSP line~)
    constexpr double ramp_time_ms = 20.0;
    m_smooth_x.reset(sample_rate, ramp_time_ms / 1000.0);
    m_smooth_y.reset(sample_rate, ramp_time_ms / 1000.0);
    
    // Set current values to match atomic values
    m_smooth_x.setCurrentAndTargetValue(m_pan_x.load());
    m_smooth_y.setCurrentAndTargetValue(m_pan_y.load());
}

void CLEATPanner::set_pan(float x, float y)
{
    x = juce::jlimit(0.0f, 1.0f, x);
    y = juce::jlimit(0.0f, 1.0f, y);
    m_pan_x.store(x);
    m_pan_y.store(y);
    
    // Update smoothing targets
    m_smooth_x.setTargetValue(x);
    m_smooth_y.setTargetValue(y);
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

    // Get input channel (mono)
    const float* input = input_channel_data[0];
    
    // Process samples with per-sample smoothing (matching Max/MSP line~ behavior)
    for (int sample = 0; sample < num_samples; ++sample)
    {
        // Get smoothed pan positions for this sample
        float x = m_smooth_x.getNextValue();
        float y = m_smooth_y.getNextValue();
        
        // Compute panning gains using smoothed positions (16 channels, row-major)
        auto gains = PanningUtils::compute_cleat_gains(x, y);
        
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

