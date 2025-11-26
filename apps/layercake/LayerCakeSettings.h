#pragma once

namespace LayerCakeApp
{
struct LayerCakeSettings
{
    // Sensitivity is pixels per full range (or inverse sensitivity)
    // Higher value = finer control (more pixels to cover range)
    // Default for JUCE slider is around 250
    static inline double mainKnobSensitivity = 250.0;
    
    // For LFO widgets which use custom drag logic
    // Default was 200.0
    static inline double lfoKnobSensitivity = 200.0;
};
}

