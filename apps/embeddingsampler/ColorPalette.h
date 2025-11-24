#pragma once

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <vector>
#include <unordered_map>

namespace EmbeddingSpaceSampler
{

class ColorPalette
{
public:
    static ColorPalette& get_instance();
    
    // Get color for a cluster index (returns noise color for negative indices)
    juce::Colour get_color(int cluster_index) const;
    
    // Get noise color (for unclustered points)
    juce::Colour get_noise_color() const { return noise_color; }
    
    // Get number of colors in palette (excluding noise color)
    int get_color_count() const { return static_cast<int>(default_colors.size() - 1); }
    
private:
    ColorPalette();
    ~ColorPalette() = default;
    
    static constexpr const char* DEFAULT_PALETTE = "Classic";
    
    // Default color palette (11 colors + noise color)
    // Colors from audiostellar: 0x1f78b4, 0xff7f00, 0xfdbf6f, 0x9e1213, 0xfb9a99,
    //                           0x4325af, 0xa6cee3, 0x0991af, 0xcab2d6, 0xa58ac2, 0xffff99
    // Noise color: 0xc7c7c7
    std::vector<juce::Colour> default_colors;
    juce::Colour noise_color;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ColorPalette)
};

} // namespace EmbeddingSpaceSampler

