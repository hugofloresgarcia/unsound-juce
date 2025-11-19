#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <atomic>

// MultiChannelLoudnessMeter UGen - tracks peak levels for multiple channels
class MultiChannelLoudnessMeter
{
public:
    static constexpr int max_channels = 16;
    
    MultiChannelLoudnessMeter();
    ~MultiChannelLoudnessMeter() = default;

    // Prepare the meter for processing
    void prepare(int num_channels);

    // Process a block of audio samples and update channel levels
    // output_channel_data: array of output channel buffers
    // num_output_channels: number of output channels (max 16)
    // num_samples: number of samples in the block
    void process_block(float* const* output_channel_data, int num_output_channels, int num_samples);

    // Get channel levels for visualization (16 channels)
    std::array<std::atomic<float>, max_channels>& get_channel_levels() { return m_channel_levels; }
    const std::array<std::atomic<float>, max_channels>& get_channel_levels() const { return m_channel_levels; }

private:
    // Channel level meters (peak detection with decay)
    std::array<std::atomic<float>, max_channels> m_channel_levels;
    
    // Decay factor per audio callback (~11ms at 44.1kHz/512 samples)
    // Equivalent to 0.89 per 50ms for UI responsiveness
    static constexpr float m_level_decay_factor{0.975f};
};

