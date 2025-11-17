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
    void processBlock(const float* const* inputChannelData,
                     int numInputChannels,
                     float* const* outputChannelData,
                     int numOutputChannels,
                     int numSamples) override;

    int getNumInputChannels() const override { return 1; }
    int getNumOutputChannels() const override { return 2; }

    // Pan control (0.0 to 1.0)
    void setPan(float pan);
    float getPan() const;

private:
    std::atomic<float> panPosition{0.5f}; // Default to center
};

