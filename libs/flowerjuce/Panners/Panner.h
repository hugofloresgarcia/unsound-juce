#pragma once

#include <juce_core/juce_core.h>

// Base interface for audio panners (UGens)
// A Panner processes audio blocks with N input channels and M output channels
class Panner
{
public:
    virtual ~Panner() = default;

    // Process a block of audio samples
    // inputChannelData: array of input channel buffers
    // numInputChannels: number of input channels
    // outputChannelData: array of output channel buffers
    // numOutputChannels: number of output channels
    // numSamples: number of samples in the block
    virtual void processBlock(const float* const* inputChannelData,
                             int numInputChannels,
                             float* const* outputChannelData,
                             int numOutputChannels,
                             int numSamples) = 0;

    // Get the number of input channels this panner expects
    virtual int getNumInputChannels() const = 0;

    // Get the number of output channels this panner produces
    virtual int getNumOutputChannels() const = 0;
};

