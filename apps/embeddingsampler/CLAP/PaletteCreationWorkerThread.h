#pragma once

#include <juce_core/juce_core.h>
#include "SoundPaletteCreator.h"

namespace Unsound4All
{
    class PaletteCreationWorkerThread : public juce::Thread
    {
    public:
        PaletteCreationWorkerThread(const juce::File& sourceFolder, int chunkSizeSeconds, FeatureType featureType = FeatureType::CLAP)
            : Thread("PaletteCreationWorkerThread"),
              m_sourceFolder(sourceFolder),
              m_chunkSizeSeconds(chunkSizeSeconds),
              m_featureType(featureType)
        {
        }
        
        void run() override
        {
            SoundPaletteCreator creator;
            m_result = creator.createPalette(m_sourceFolder, m_chunkSizeSeconds,
                [this](const juce::String& status)
                {
                    // Parse status message to extract progress info
                    parseProgressMessage(status);
                    
                    // Update progress window on message thread
                    juce::MessageManager::callAsync([this, status]()
                    {
                        updateProgressWindow(status);
                    });
                },
                m_featureType);
        }
        
        juce::File getResult() const { return m_result; }
        bool wasCancelled() const { return threadShouldExit(); }
        
    private:
        juce::File m_sourceFolder;
        int m_chunkSizeSeconds;
        FeatureType m_featureType;
        juce::File m_result;
        
        int m_totalFiles{0};
        int m_currentFile{0};
        juce::String m_currentFileName;
        juce::String m_currentPhase;
        
        void parseProgressMessage(const juce::String& message)
        {
            // Parse messages like:
            // "Found 5 audio files"
            // "Chunking filename.wav (2/5)"
            // "Creating embeddings for 50 chunks..."
            // "Processing chunk 10/50: chunk_001.wav"
            
            if (message.startsWith("Found "))
            {
                // Extract total file count
                auto parts = juce::StringArray::fromTokens(message, " ", "");
                if (parts.size() >= 3)
                {
                    m_totalFiles = parts[1].getIntValue();
                }
                m_currentPhase = "Finding files";
            }
            else if (message.startsWith("Chunking "))
            {
                m_currentPhase = "Chunking";
                // Extract "filename.wav (2/5)"
                int openParen = message.indexOfChar('(');
                int slash = message.indexOfChar('/');
                int closeParen = message.indexOfChar(')');
                
                if (openParen > 0 && slash > openParen && closeParen > slash)
                {
                    m_currentFileName = message.substring(9, openParen).trim();
                    m_currentFile = message.substring(openParen + 1, slash).getIntValue();
                    m_totalFiles = message.substring(slash + 1, closeParen).getIntValue();
                }
            }
            else if (message.startsWith("Creating CLAP embeddings") || message.startsWith("Creating STFT features"))
            {
                m_currentPhase = message.startsWith("Creating CLAP") ? "Creating CLAP embeddings" : "Creating STFT features";
                // Extract chunk count from "Creating embeddings for 50 chunks..." or "Creating STFT features for 50 chunks..."
                auto parts = juce::StringArray::fromTokens(message, " ", "");
                for (int i = 0; i < parts.size() - 1; ++i)
                {
                    if (parts[i] == "for" && i + 1 < parts.size())
                    {
                        m_totalFiles = parts[i + 1].getIntValue();
                        m_currentFile = 0;
                        break;
                    }
                }
            }
            else if (message.startsWith("Processing chunk ") || message.startsWith("Extracting STFT features"))
            {
                m_currentPhase = message.startsWith("Extracting STFT") ? "Creating STFT features" : "Creating embeddings";
                // Extract "Processing chunk 10/50: chunk_001.wav"
                int colon = message.indexOfChar(':');
                if (colon > 0)
                {
                    m_currentFileName = message.substring(colon + 1).trim();
                    
                    // Extract "10/50" - find the number before and after the slash
                    int slash = message.indexOfChar('/');
                    if (slash > 0)
                    {
                        // Find the space before the slash (should be after "chunk ")
                        // Search backwards from slash to find the space
                        int space = -1;
                        for (int i = slash - 1; i >= 0; --i)
                        {
                            if (message[i] == ' ')
                            {
                                space = i;
                                break;
                            }
                        }
                        
                        if (space >= 0)
                        {
                            m_currentFile = message.substring(space + 1, slash).getIntValue();
                            int end = message.indexOfChar(':', slash);
                            if (end < 0) end = message.length();
                            m_totalFiles = message.substring(slash + 1, end).getIntValue();
                        }
                    }
                }
            }
            else if (message.contains("successfully"))
            {
                m_currentPhase = "Complete";
                m_currentFile = m_totalFiles;
            }
        }
        
        void updateProgressWindow(const juce::String& status)
        {
            auto* progressWindow = PaletteCreationProgressWindow::getInstance();
            if (progressWindow != nullptr)
            {
                progressWindow->setStatus(m_currentPhase.isNotEmpty() ? m_currentPhase : status);
                
                if (m_totalFiles > 0)
                {
                    progressWindow->setCurrentFile(m_currentFile, m_totalFiles);
                }
                
                if (m_currentFileName.isNotEmpty())
                {
                    progressWindow->setCurrentFileName(m_currentFileName);
                }
            }
        }
        
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PaletteCreationWorkerThread)
    };
}

