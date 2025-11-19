#pragma once

#include <juce_core/juce_core.h>
#include <atomic>

// PeakMeter UGen - tracks peak level for a mono audio signal
class PeakMeter
{
public:
    PeakMeter();
    ~PeakMeter() = default;

    // Prepare the meter for processing
    void prepare();

    // Process a block of audio samples and update peak level
    // channel_data: pointer to mono audio buffer
    // num_samples: number of samples in the block
    void process_block(const float* channel_data, int num_samples);

    // Get peak level for the last processed block
    float get_peak() const { return m_peak_level.load(); }

    // Reset peak level
    void reset() { m_peak_level.store(0.0f); }

private:
    // Peak level tracking (peak hold with decay)
    std::atomic<float> m_peak_level{0.0f};
};

