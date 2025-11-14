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
    void processSample(float* const* outputChannelData, int numOutputChannels, int sample, float sampleValue, 
                       const juce::BigInteger* activeChannels = nullptr) const
    {
        static int callCount = 0;
        static bool firstCall = true;
        callCount++;
        bool isFirstCall = (callCount == 1);
        
        if (isFirstCall)
        {
            DBG("[OutputBus] First processSample call:");
            DBG("  outputChannel setting: " << outputChannel);
            DBG("  numOutputChannels: " << numOutputChannels);
            DBG("  sampleValue: " << sampleValue);
            if (activeChannels != nullptr)
            {
                DBG("  Active channels: " << activeChannels->toString(2));
                DBG("  Number of active channels: " << activeChannels->countNumberOfSetBits());
            }
        }
        
        if (outputChannel >= 0 && outputChannel < numOutputChannels)
        {
            // Route to specific channel
            if (isFirstCall)
            {
                DBG("[OutputBus] Routing to specific channel: " << outputChannel);
                if (activeChannels != nullptr)
                {
                    bool isActive = activeChannels->getBitRangeAsInt(outputChannel, 1) != 0;
                    DBG("  Channel " << outputChannel << " is " << (isActive ? "ACTIVE" : "INACTIVE"));
                }
            }
            if (outputChannelData[outputChannel] != nullptr)
            {
                // Check if channel is active (if activeChannels provided)
                bool shouldWrite = true;
                if (activeChannels != nullptr)
                {
                    shouldWrite = activeChannels->getBitRangeAsInt(outputChannel, 1) != 0;
                    if (isFirstCall && !shouldWrite)
                        DBG("[OutputBus] WARNING: Attempting to write to inactive channel " << outputChannel);
                }
                if (shouldWrite)
                {
                    outputChannelData[outputChannel][sample] += sampleValue;
                    if (isFirstCall)
                        DBG("[OutputBus] Sample added to channel " << outputChannel << ", new value: " << outputChannelData[outputChannel][sample]);
                }
            }
            else
            {
                if (isFirstCall)
                    DBG("[OutputBus] WARNING: outputChannelData[" << outputChannel << "] is null!");
            }
        }
        else
        {
            // Route to all channels (default behavior)
            if (isFirstCall)
                DBG("[OutputBus] Routing to all " << numOutputChannels << " channels");
            for (int channel = 0; channel < numOutputChannels; ++channel)
            {
                bool shouldWrite = true;
                if (activeChannels != nullptr)
                {
                    shouldWrite = activeChannels->getBitRangeAsInt(channel, 1) != 0;
                }
                if (outputChannelData[channel] != nullptr && shouldWrite)
                {
                    outputChannelData[channel][sample] += sampleValue;
                    if (isFirstCall && channel < 3) // Only log first 3 channels
                        DBG("[OutputBus] Sample added to channel " << channel << ", new value: " << outputChannelData[channel][sample]);
                }
                else
                {
                    if (isFirstCall && channel < 3)
                    {
                        if (outputChannelData[channel] == nullptr)
                            DBG("[OutputBus] WARNING: outputChannelData[" << channel << "] is null!");
                        else if (!shouldWrite)
                            DBG("[OutputBus] Skipping inactive channel " << channel);
                    }
                }
            }
        }
    }

private:
    int outputChannel; // -1 = all channels, 0+ = specific channel
};

