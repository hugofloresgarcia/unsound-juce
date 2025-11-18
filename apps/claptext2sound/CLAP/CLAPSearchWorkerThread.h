#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <flowerjuce/Engine/MultiTrackLooperEngine.h>
#include "ONNXModelManager.h"
#include "SoundPaletteManager.h"
#include <functional>

namespace CLAPText2Sound
{
    // Background thread for CLAP-based sound search
    class CLAPSearchWorkerThread : public juce::Thread
    {
    public:
        CLAPSearchWorkerThread(
            MultiTrackLooperEngine& engine,
            int trackIndex,
            const juce::String& textPrompt,
            const juce::File& soundPalettePath,
            ONNXModelManager* sharedModelManager = nullptr  // Optional shared model manager (for caching)
        );
        
        ~CLAPSearchWorkerThread() override;
        
        void run() override;
        
        // Callbacks (similar to GradioWorkerThread interface)
        std::function<void(juce::Result, juce::Array<juce::File>, int)> onComplete;
        std::function<void(const juce::String& statusText)> onStatusUpdate;
        
    private:
        MultiTrackLooperEngine& looperEngine;
        int trackIndex;
        juce::String textPrompt;
        juce::File soundPalettePath;
        ONNXModelManager* m_sharedModelManager;  // Optional shared model manager
        
        // Search FAISS index for top-K matches
        // Note: CLAPText2Sound only works with CLAP embeddings, not STFT features
        juce::Array<juce::File> searchPalette(
            const juce::File& palettePath,
            const std::vector<float>& textEmbedding,
            int topK = 4
        ) const;
        
        // Load FAISS index from palette directory
        bool loadPaletteIndex(const juce::File& palettePath) const;
    };
}

