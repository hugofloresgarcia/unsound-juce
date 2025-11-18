#include "SoundPaletteManager.h"
#include <juce_data_structures/juce_data_structures.h>
#include <algorithm>

namespace Unsound4All
{
    SoundPaletteManager::SoundPaletteManager()
    {
    }
    
    SoundPaletteManager::~SoundPaletteManager()
    {
    }
    
    std::vector<SoundPaletteInfo> SoundPaletteManager::discoverPalettes(const juce::File& searchRoot)
    {
        std::vector<SoundPaletteInfo> palettes;
        
        // Always use the dedicated palette directory: ~/Documents/unsound4all/
        juce::File paletteBaseDir = getPaletteBaseDirectory();
        
        // Create directory if it doesn't exist
        if (!paletteBaseDir.exists())
        {
            paletteBaseDir.createDirectory();
        }
        
        if (!paletteBaseDir.exists() || !paletteBaseDir.isDirectory())
        {
            return palettes; // Return empty if we can't access the directory
        }
        
        // Search for directories ending with _SOUND_PALETTE in the base directory
        juce::Array<juce::File> subdirs;
        paletteBaseDir.findChildFiles(subdirs, juce::File::findDirectories, false);
        
        for (const auto& subdir : subdirs)
        {
            if (subdir.getFileName().endsWith("_SOUND_PALETTE"))
            {
                if (isValidPalette(subdir))
                {
                    auto info = getPaletteInfo(subdir);
                    palettes.push_back(info);
                }
            }
        }
        
        return palettes;
    }
    
    SoundPaletteInfo SoundPaletteManager::getPaletteInfo(const juce::File& paletteDir) const
    {
        SoundPaletteInfo info;
        info.path = paletteDir;
        info.name = paletteDir.getFileNameWithoutExtension();
        
        // Try to load metadata
        loadPaletteMetadata(paletteDir, info);
        
        return info;
    }
    
    bool SoundPaletteManager::isValidPalette(const juce::File& paletteDir) const
    {
        if (!paletteDir.exists() || !paletteDir.isDirectory())
            return false;
        
        // Check for required files: embeddings.bin (or embeddings.faiss) and metadata.json
        auto binFile = paletteDir.getChildFile("embeddings.bin");
        auto faissFile = paletteDir.getChildFile("embeddings.faiss");
        auto metadataFile = paletteDir.getChildFile("metadata.json");
        
        if (!metadataFile.existsAsFile() || (!binFile.existsAsFile() && !faissFile.existsAsFile()))
            return false;
        
        // Unsound4All only works with CLAP embeddings, not STFT features
        // Check embedding type from metadata
        juce::var metadata = juce::JSON::parse(metadataFile);
        if (metadata.isObject())
        {
            juce::String embeddingType = metadata.getProperty("embeddingType", juce::String("CLAP")).toString();
            if (embeddingType == "STFT")
            {
                DBG("SoundPaletteManager: Rejecting palette with STFT features: " + paletteDir.getFileName());
                return false;
            }
        }
        
        return true;
    }
    
    std::vector<juce::File> SoundPaletteManager::getDefaultSearchLocations() const
    {
        // Return the single palette base directory
        std::vector<juce::File> locations;
        locations.push_back(getPaletteBaseDirectory());
        return locations;
    }
    
    juce::File SoundPaletteManager::getPaletteBaseDirectory() const
    {
        // All palettes stored in ~/Documents/unsound4all/
        auto docsDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
        return docsDir.getChildFile("unsound4all");
    }
    
    bool SoundPaletteManager::loadPaletteMetadata(const juce::File& paletteDir, SoundPaletteInfo& info) const
    {
        auto metadataFile = paletteDir.getChildFile("metadata.json");
        if (!metadataFile.existsAsFile())
            return false;
        
        // TODO: Parse JSON metadata file
        // For now, count chunks by scanning directory
        juce::Array<juce::File> chunkFiles;
        paletteDir.findChildFiles(chunkFiles, juce::File::findFiles, false, "*.wav");
        info.numChunks = chunkFiles.size();
        
        return true;
    }
}

