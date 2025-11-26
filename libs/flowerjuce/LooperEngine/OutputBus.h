#pragma once

#include <juce_core/juce_core.h>
#include <flowerjuce/Debug/DebugAudioRate.h>

// OutputBus handles routing audio samples to specific output channels
// -1 = route to all channels, 0+ = route to specific channel
class OutputBus
{
public:
    OutputBus() : m_output_channel(-1) {}
    
    // Set which output channel to route to (-1 = all channels, 0+ = specific channel)
    void set_output_channel(int channel) { m_output_channel = channel; }
    
    // Get current output channel setting
    int get_output_channel() const { return m_output_channel; }
    
    // Process a sample and route it to the configured output channel(s)
    // output_channel_data: array of output channel buffers
    // num_output_channels: total number of output channels available
    // sample: sample index within the buffer
    // sample_value: the audio sample value to route
    void process_sample(float* const* output_channel_data, int num_output_channels, int sample, float sample_value, 
                       const juce::BigInteger* active_channels = nullptr) const
    {
        DBG_AUDIO_RATE(2000, {
            DBG("[OutputBus] process_sample call:");
            DBG("  m_output_channel setting: " << m_output_channel);
            DBG("  num_output_channels: " << num_output_channels);
            DBG("  sample_value: " << sample_value);
            if (active_channels != nullptr)
            {
                DBG("  Active channels: " << active_channels->toString(2));
                DBG("  Number of active channels: " << active_channels->countNumberOfSetBits());
            }
        });
        
        if (m_output_channel >= 0 && m_output_channel < num_output_channels)
        {
            // Route to specific channel
            DBG_AUDIO_RATE(2000, {
                DBG("[OutputBus] Routing to specific channel: " << m_output_channel);
                if (active_channels != nullptr)
                {
                    bool is_active = active_channels->getBitRangeAsInt(m_output_channel, 1) != 0;
                    DBG("  Channel " << m_output_channel << " is " << (is_active ? "ACTIVE" : "INACTIVE"));
                }
            });

            if (output_channel_data[m_output_channel] != nullptr)
            {
                // Check if channel is active (if active_channels provided)
                bool should_write = true;
                if (active_channels != nullptr)
                {
                    should_write = active_channels->getBitRangeAsInt(m_output_channel, 1) != 0;
                    if (!should_write)
                        DBG_AUDIO_RATE(2000, { DBG("[OutputBus] WARNING: Attempting to write to inactive channel " << m_output_channel); });
                }
                if (should_write)
                {
                    output_channel_data[m_output_channel][sample] += sample_value;
                    DBG_AUDIO_RATE(2000, { DBG("[OutputBus] Sample added to channel " << m_output_channel << ", new value: " << output_channel_data[m_output_channel][sample]); });
                }
            }
            else
            {
                DBG_AUDIO_RATE(2000, { DBG("[OutputBus] WARNING: output_channel_data[" << m_output_channel << "] is null!"); });
            }
        }
        else
        {
            // Route to all channels (default behavior)
            DBG_AUDIO_RATE(2000, { DBG("[OutputBus] Routing to all " << num_output_channels << " channels"); });
            for (int channel = 0; channel < num_output_channels; ++channel)
            {
                bool should_write = true;
                if (active_channels != nullptr)
                {
                    should_write = active_channels->getBitRangeAsInt(channel, 1) != 0;
                }
                if (output_channel_data[channel] != nullptr && should_write)
                {
                    output_channel_data[channel][sample] += sample_value;
                    if (channel < 3) // Only log first 3 channels
                        DBG_AUDIO_RATE(2000, { DBG("[OutputBus] Sample added to channel " << channel << ", new value: " << output_channel_data[channel][sample]); });
                }
                else
                {
                    if (channel < 3)
                    {
                        if (output_channel_data[channel] == nullptr)
                            DBG_AUDIO_RATE(2000, { DBG("[OutputBus] WARNING: output_channel_data[" << channel << "] is null!"); });
                        else if (!should_write)
                            DBG_AUDIO_RATE(2000, { DBG("[OutputBus] Skipping inactive channel " << channel); });
                    }
                }
            }
        }
    }

private:
    int m_output_channel; // -1 = all channels, 0+ = specific channel
};

