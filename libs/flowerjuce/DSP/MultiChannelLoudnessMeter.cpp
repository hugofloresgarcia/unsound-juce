#include "MultiChannelLoudnessMeter.h"

MultiChannelLoudnessMeter::MultiChannelLoudnessMeter()
{
    // Initialize channel level meters
    for (auto& level : m_channel_levels)
    {
        level.store(0.0f);
    }
}

void MultiChannelLoudnessMeter::prepare(int num_channels)
{
    // Reset all channel levels
    for (auto& level : m_channel_levels)
    {
        level.store(0.0f);
    }
}

void MultiChannelLoudnessMeter::process_block(float* const* output_channel_data, int num_output_channels, int num_samples)
{
    // Update channel level meters (peak detection with decay)
    for (int channel = 0; channel < juce::jmin(max_channels, num_output_channels); ++channel)
    {
        if (output_channel_data[channel] != nullptr)
        {
            // Find peak in this channel
            float peak = 0.0f;
            for (int sample = 0; sample < num_samples; ++sample)
            {
                peak = juce::jmax(peak, std::abs(output_channel_data[channel][sample]));
            }
            
            // Update level meter (peak hold with decay)
            float currentLevel = m_channel_levels[channel].load();
            
            // Apply decay first
            if (currentLevel > 0.001f)
            {
                // Decay factor per audio callback (~11ms at 44.1kHz/512 samples)
                // Equivalent to 0.89 per 50ms for UI responsiveness
                m_channel_levels[channel].store(currentLevel * m_level_decay_factor);
            }
            else
            {
                m_channel_levels[channel].store(0.0f);
            }
            
            // Then update with new peak if higher
            float decayedLevel = m_channel_levels[channel].load();
            if (peak > decayedLevel)
            {
                m_channel_levels[channel].store(peak);
            }
        }
    }
    
    // Also decay channels that aren't being written to (if num_output_channels < max_channels)
    for (int channel = num_output_channels; channel < max_channels; ++channel)
    {
        float currentLevel = m_channel_levels[channel].load();
        if (currentLevel > 0.001f)
        {
            m_channel_levels[channel].store(currentLevel * m_level_decay_factor);
        }
        else
        {
            m_channel_levels[channel].store(0.0f);
        }
    }
}

