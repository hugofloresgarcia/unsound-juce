#pragma once

#include <juce_core/juce_core.h>

// Base interface for audio panners (UGens)
// A Panner processes audio blocks with N input channels and M output channels
class Panner
{
public:
    virtual ~Panner() = default;

    // Process a block of audio samples
    // input_channel_data: array of input channel buffers
    // num_input_channels: number of input channels
    // output_channel_data: array of output channel buffers
    // num_output_channels: number of output channels
    // num_samples: number of samples in the block
    virtual void process_block(const float* const* input_channel_data,
                             int num_input_channels,
                             float* const* output_channel_data,
                             int num_output_channels,
                             int num_samples) = 0;

    // Get the number of input channels this panner expects
    virtual int get_num_input_channels() const = 0;

    // Get the number of output channels this panner produces
    virtual int get_num_output_channels() const = 0;
};

