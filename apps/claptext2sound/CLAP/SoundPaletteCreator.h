#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "ONNXModelManager.h"
#include <functional>

namespace CLAPText2Sound
{
    class SoundPaletteCreator
    {
    public:
        SoundPaletteCreator();
        ~SoundPaletteCreator();
        
        // Create a new sound palette from an audio folder
        // Returns the path to the created palette directory, or empty File on failure
        juce::File createPalette(
            const juce::File& sourceAudioFolder,
            int chunkSizeSeconds = 10,
            std::function<void(const juce::String&)> progressCallback = nullptr
        );
        
        // Check if creation is in progress
        bool isCreating() const { return m_isCreating; }
        
        // Cancel creation (if running in background thread)
        void cancel();
        
    private:
        bool m_isCreating{false};
        bool m_cancelled{false};
        
        // Find all audio files recursively
        juce::Array<juce::File> findAudioFiles(const juce::File& rootFolder) const;
        
        // Chunk an audio file into segments
        juce::Array<juce::File> chunkAudioFile(
            const juce::File& audioFile,
            int chunkSizeSeconds,
            const juce::File& outputDir,
            std::function<void(const juce::String&)> progressCallback = nullptr
        ) const;
        
        // Process all chunks and create embeddings
        bool createEmbeddings(
            const juce::Array<juce::File>& chunkFiles,
            const juce::File& paletteDir,
            ONNXModelManager& modelManager,
            std::vector<std::vector<float>>& embeddings,
            std::function<void(const juce::String&)> progressCallback = nullptr
        ) const;
        
        // Save FAISS index and metadata
        bool savePaletteData(
            const juce::File& paletteDir,
            const juce::Array<juce::File>& chunkFiles,
            const juce::Array<juce::File>& sourceFiles,
            const std::vector<std::vector<float>>& embeddings
        ) const;
    };
}

