#pragma once

#include <juce_core/juce_core.h>

// OutputBus handles routing audio samples to specific output channels
// -1 = route to all channels, 0+ = route to specific channel
class OutputBus
{
public:
    OutputBus() : outputChannel(-1) {}
    
    // Set which output channel to route to (-1 = all channels, 0+ = specific channel)
    void setOutputChannel(int channel) { outputChannel = channel; }
    
    // Get current output channel setting
    int getOutputChannel() const { return outputChannel; }
    
    // Process a sample and route it to the configured output channel(s)
    // outputChannelData: array of output channel buffers
    // numOutputChannels: total number of output channels available
    // sample: sample index within the buffer
    // sampleValue: the audio sample value to route
    void processSample(float* const* outputChannelData, int numOutputChannels, int sample, float sampleValue) const
    {
        if (outputChannel >= 0 && outputChannel < numOutputChannels)
        {
            // Route to specific channel
            if (outputChannelData[outputChannel] != nullptr)
            {
                outputChannelData[outputChannel][sample] += sampleValue;
            }
        }
        else
        {
            // Route to all channels (default behavior)
            for (int channel = 0; channel < numOutputChannels; ++channel)
            {
                if (outputChannelData[channel] != nullptr)
                {
                    outputChannelData[channel][sample] += sampleValue;
                }
            }
        }
    }

private:
    int outputChannel; // -1 = all channels, 0+ = specific channel
};

