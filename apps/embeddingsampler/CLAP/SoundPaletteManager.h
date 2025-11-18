#pragma once

#include <juce_core/juce_core.h>
#include <vector>

namespace CLAPText2Sound
{
    struct SoundPaletteInfo
    {
        juce::String name;
        juce::File path;
        int chunkSizeSeconds{10};
        int numChunks{0};
        juce::Time createdTime;
    };
    
    class SoundPaletteManager
    {
    public:
        SoundPaletteManager();
        ~SoundPaletteManager();
        
        // Discover existing sound palettes (look for folders ending with _SOUND_PALETTE)
        std::vector<SoundPaletteInfo> discoverPalettes(const juce::File& searchRoot = juce::File());
        
        // Get palette info from a palette directory
        SoundPaletteInfo getPaletteInfo(const juce::File& paletteDir) const;
        
        // Check if a directory is a valid sound palette
        bool isValidPalette(const juce::File& paletteDir) const;
        
        // Get default search locations (user documents, app data, etc.)
        std::vector<juce::File> getDefaultSearchLocations() const;
        
        // Get the base directory where all palettes are stored (~/Documents/claptext2sound/)
        juce::File getPaletteBaseDirectory() const;
        
    private:
        bool loadPaletteMetadata(const juce::File& paletteDir, SoundPaletteInfo& info) const;
    };
}

