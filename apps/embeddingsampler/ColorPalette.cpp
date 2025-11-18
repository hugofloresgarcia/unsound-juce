#include "ColorPalette.h"

namespace EmbeddingSpaceSampler
{

ColorPalette::ColorPalette()
{
    // Initialize default colors from audiostellar palette
    default_colors = {
        juce::Colour(0x1f78b4),  // Blue
        juce::Colour(0xff7f00),  // Orange
        juce::Colour(0xfdbf6f),  // Light orange
        juce::Colour(0x9e1213),  // Dark red
        juce::Colour(0xfb9a99),  // Light red
        juce::Colour(0x4325af),  // Purple
        juce::Colour(0xa6cee3),  // Light blue
        juce::Colour(0x0991af),  // Cyan
        juce::Colour(0xcab2d6),  // Light purple
        juce::Colour(0xa58ac2),  // Lavender
        juce::Colour(0xffff99),  // Yellow
        juce::Colour(0xc7c7c7)   // Noise color (gray)
    };
    
    noise_color = juce::Colour(0xc7c7c7);
}

ColorPalette& ColorPalette::get_instance()
{
    static ColorPalette instance;
    return instance;
}

juce::Colour ColorPalette::get_color(int cluster_index) const
{
    // Match audiostellar's getColor logic exactly:
    // - For negative indices (NOISE = -2, NOT_CLASSIFIED = -1), return noise color (index 11)
    // - For non-negative indices, use modulo with colorCount (11) to cycle through colors 0-10
    if (cluster_index < 0)
    {
        // Return noise color (last color in palette, index 11)
        return default_colors[11];
    }
    
    // Use modulo to cycle through colors 0-10 (colorCount = 11)
    int color_count = get_color_count(); // Returns 11
    if (color_count > 0 && cluster_index >= 0)
    {
        return default_colors[cluster_index % color_count];
    }
    
    return juce::Colours::white; // Fallback
}

} // namespace EmbeddingSpaceSampler

