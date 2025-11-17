#pragma once

#include "Panner.h"
#include "PanningUtils.h"
#include <juce_dsp/juce_dsp.h>
#include <atomic>

// CLEAT panner: processes mono input to 16-channel output (4x4 grid)
// Pan control: (x, y) coordinates, both 0.0 to 1.0
// x: 0.0 = left, 1.0 = right
// y: 0.0 = bottom, 1.0 = top
// Channels are arranged row-major: channels 0-3 = bottom row left-to-right
class CLEATPanner : public Panner
{
public:
    CLEATPanner();
    ~CLEATPanner() override = default;

    // Prepare for audio processing (set sample rate for smoothing)
    void prepare(double sample_rate);

    // Panner interface
    void process_block(const float* const* input_channel_data,
                     int num_input_channels,
                     float* const* output_channel_data,
                     int num_output_channels,
                     int num_samples) override;

    int get_num_input_channels() const override { return 1; }
    int get_num_output_channels() const override { return 16; }

    // Pan control (both 0.0 to 1.0)
    void set_pan(float x, float y);
    float get_pan_x() const;
    float get_pan_y() const;
    
    // Get current smoothed pan positions (actual values being used for audio)
    float get_smoothed_pan_x() const { return m_smooth_x.getCurrentValue(); }
    float get_smoothed_pan_y() const { return m_smooth_y.getCurrentValue(); }

private:
    std::atomic<float> m_pan_x{0.5f}; // Default to center
    std::atomic<float> m_pan_y{0.5f}; // Default to center
    
    juce::SmoothedValue<float> m_smooth_x{0.5f}; // Smoothed x position
    juce::SmoothedValue<float> m_smooth_y{0.5f}; // Smoothed y position
};

