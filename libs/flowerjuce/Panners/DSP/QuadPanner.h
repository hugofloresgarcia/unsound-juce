#pragma once

#include "Panner.h"
#include "PanningUtils.h"
#include <atomic>

// Quad panner: processes mono input to 4-channel output (FL, FR, BL, BR)
// Pan control: (x, y) coordinates, both 0.0 to 1.0
// x: 0.0 = left, 1.0 = right
// y: 0.0 = back, 1.0 = front
class QuadPanner : public Panner
{
public:
    QuadPanner();
    ~QuadPanner() override = default;

    // Panner interface
    void process_block(const float* const* input_channel_data,
                     int num_input_channels,
                     float* const* output_channel_data,
                     int num_output_channels,
                     int num_samples) override;

    int get_num_input_channels() const override { return 1; }
    int get_num_output_channels() const override { return 4; }

    // Pan control (both 0.0 to 1.0)
    void set_pan(float x, float y);
    float get_pan_x() const;
    float get_pan_y() const;

private:
    std::atomic<float> m_pan_x{0.5f}; // Default to center
    std::atomic<float> m_pan_y{0.5f}; // Default to center
};

