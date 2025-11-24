#pragma once

#include "Panner.h"
#include "PanningUtils.h"
#include <atomic>

// Stereo panner: processes mono input to stereo output
// Pan control: 0.0 = all left, 0.5 = center, 1.0 = all right
class StereoPanner : public Panner
{
public:
    StereoPanner();
    ~StereoPanner() override = default;

    // Panner interface
    void process_block(const float* const* input_channel_data,
                     int num_input_channels,
                     float* const* output_channel_data,
                     int num_output_channels,
                     int num_samples) override;

    int get_num_input_channels() const override { return 1; }
    int get_num_output_channels() const override { return 2; }

    // Pan control (0.0 to 1.0)
    void set_pan(float pan);
    float get_pan() const;

private:
    std::atomic<float> m_pan_position{0.5f}; // Default to center
};

