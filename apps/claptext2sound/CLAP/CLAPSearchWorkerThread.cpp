#include "CLAPSearchWorkerThread.h"
#include "ONNXModelManager.h"
#include <algorithm>
#include <cmath>
#include <juce_data_structures/juce_data_structures.h>

namespace CLAPText2Sound
{
    CLAPSearchWorkerThread::CLAPSearchWorkerThread(
        MultiTrackLooperEngine& engine,
        int trackIndex,
        const juce::String& textPrompt,
        const juce::File& soundPalettePath,
        ONNXModelManager* sharedModelManager)
        : Thread("CLAPSearchWorkerThread"),
          looperEngine(engine),
          trackIndex(trackIndex),
          textPrompt(textPrompt),
          soundPalettePath(soundPalettePath),
          m_sharedModelManager(sharedModelManager)
    {
    }
    
    CLAPSearchWorkerThread::~CLAPSearchWorkerThread()
    {
        stopThread(1000);
    }
    
    void CLAPSearchWorkerThread::run()
    {
        // Notify status update: computing text embedding
        juce::MessageManager::callAsync([this]()
        {
            if (onStatusUpdate)
                onStatusUpdate("Computing text embedding...");
        });
        
        // Use shared model manager if provided, otherwise create a new one
        ONNXModelManager* modelManager = m_sharedModelManager;
        std::unique_ptr<ONNXModelManager> localModelManager;
        
        if (modelManager == nullptr || !modelManager->isInitialized())
        {
            // Create a new model manager
            localModelManager = std::make_unique<ONNXModelManager>();
            modelManager = localModelManager.get();
            
            // Find ONNX models in app bundle Resources (macOS) or executable directory (other platforms)
            auto executableFile = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
            juce::File audioModelPath, textModelPath;
            
            #if JUCE_MAC
                // On macOS, look in app bundle Resources folder
                auto resourcesDir = executableFile.getParentDirectory()
                                      .getParentDirectory()
                                      .getChildFile("Resources");
                audioModelPath = resourcesDir.getChildFile("clap_audio_encoder.onnx");
                textModelPath = resourcesDir.getChildFile("clap_text_encoder.onnx");
                
                // Fallback to executable directory if not found in Resources
                if (!audioModelPath.existsAsFile())
                    audioModelPath = executableFile.getParentDirectory().getChildFile("clap_audio_encoder.onnx");
                if (!textModelPath.existsAsFile())
                    textModelPath = executableFile.getParentDirectory().getChildFile("clap_text_encoder.onnx");
            #else
                // On other platforms, look in executable directory
                audioModelPath = executableFile.getParentDirectory().getChildFile("clap_audio_encoder.onnx");
                textModelPath = executableFile.getParentDirectory().getChildFile("clap_text_encoder.onnx");
            #endif
            
            if (!modelManager->initialize(audioModelPath, textModelPath))
            {
                juce::MessageManager::callAsync([this]()
                {
                    if (onComplete)
                        onComplete(juce::Result::fail("Failed to initialize ONNX models"), juce::Array<juce::File>(), trackIndex);
                });
                return;
            }
        }
        
        // Get text embedding
        auto textEmbedding = modelManager->getTextEmbedding(textPrompt);
        if (textEmbedding.empty())
        {
            juce::MessageManager::callAsync([this]()
            {
                if (onComplete)
                    onComplete(juce::Result::fail("Failed to compute text embedding"), juce::Array<juce::File>(), trackIndex);
            });
            return;
        }
        
        // Notify status update: searching palette
        juce::MessageManager::callAsync([this]()
        {
            if (onStatusUpdate)
                onStatusUpdate("Searching sound palette...");
        });
        
        // Search palette for top-4 matches
        auto resultFiles = searchPalette(soundPalettePath, textEmbedding, 4);
        
        // Notify completion
        juce::MessageManager::callAsync([this, resultFiles]()
        {
            if (onComplete)
            {
                if (resultFiles.isEmpty())
                {
                    onComplete(juce::Result::fail("No matches found in sound palette"), juce::Array<juce::File>(), trackIndex);
                }
                else
                {
                    onComplete(juce::Result::ok(), resultFiles, trackIndex);
                }
            }
        });
    }
    
    juce::Array<juce::File> CLAPSearchWorkerThread::searchPalette(
        const juce::File& palettePath,
        const std::vector<float>& textEmbedding,
        int topK) const
    {
        juce::Array<juce::File> results;
        
        if (textEmbedding.empty())
        {
            return results;
        }
        
        // Load metadata
        juce::File metadataFile = palettePath.getChildFile("metadata.json");
        if (!metadataFile.existsAsFile())
        {
            DBG("CLAPSearchWorkerThread: Metadata file not found: " + metadataFile.getFullPathName());
            return results;
        }
        
        juce::var metadata = juce::JSON::parse(metadataFile);
        if (!metadata.isObject())
        {
            DBG("CLAPSearchWorkerThread: Failed to parse metadata JSON");
            return results;
        }
        
        // Load embeddings
        juce::File embeddingsFile = palettePath.getChildFile("embeddings.bin");
        if (!embeddingsFile.existsAsFile())
        {
            DBG("CLAPSearchWorkerThread: Embeddings file not found: " + embeddingsFile.getFullPathName());
            return results;
        }
        
        juce::FileInputStream inputStream(embeddingsFile);
        if (!inputStream.openedOk())
        {
            DBG("CLAPSearchWorkerThread: Failed to open embeddings file");
            return results;
        }
        
        // Read header
        int32_t numEmbeddings = 0;
        int32_t embeddingSize = 0;
        
        inputStream.read(&numEmbeddings, sizeof(int32_t));
        inputStream.read(&embeddingSize, sizeof(int32_t));
        
        if (embeddingSize != static_cast<int32_t>(textEmbedding.size()))
        {
            DBG("CLAPSearchWorkerThread: Embedding size mismatch: expected " + juce::String(embeddingSize) + ", got " + juce::String(textEmbedding.size()));
            return results;
        }
        
        // Read all embeddings and compute similarities
        std::vector<std::pair<float, int>> similarities; // (similarity, index)
        
        // Debug: Check text embedding norm
        float textNorm = 0.0f;
        for (size_t j = 0; j < textEmbedding.size(); ++j)
        {
            textNorm += textEmbedding[j] * textEmbedding[j];
        }
        textNorm = std::sqrt(textNorm);
        DBG("CLAPSearchWorkerThread: Text embedding norm: " + juce::String(textNorm) + ", size: " + juce::String(textEmbedding.size()));
        
        for (int32_t i = 0; i < numEmbeddings; ++i)
        {
            std::vector<float> embedding(embeddingSize);
            inputStream.read(embedding.data(), sizeof(float) * embeddingSize);
            
            // Compute cosine similarity
            float dotProduct = 0.0f;
            float norm1 = 0.0f;
            float norm2 = 0.0f;
            
            for (size_t j = 0; j < textEmbedding.size(); ++j)
            {
                dotProduct += textEmbedding[j] * embedding[j];
                norm1 += textEmbedding[j] * textEmbedding[j];
                norm2 += embedding[j] * embedding[j];
            }
            
            float similarity = 0.0f;
            float denom = std::sqrt(norm1) * std::sqrt(norm2);
            if (denom > 1e-8f)
            {
                similarity = dotProduct / denom;
            }
            
            similarities.push_back({similarity, i});
            
            // Debug: Log first few similarities
            if (i < 3)
            {
                DBG("CLAPSearchWorkerThread: Chunk " + juce::String(i) + " similarity: " + juce::String(similarity));
            }
        }
        
        // Debug: Log top similarities
        if (!similarities.empty())
        {
            std::sort(similarities.begin(), similarities.end(),
                      [](const std::pair<float, int>& a, const std::pair<float, int>& b) {
                          return a.first > b.first;
                      });
            DBG("CLAPSearchWorkerThread: Top similarity: " + juce::String(similarities[0].first));
            if (similarities.size() > 1)
            {
                DBG("CLAPSearchWorkerThread: Second similarity: " + juce::String(similarities[1].first));
            }
        }
        
        // Sort by similarity (descending)
        std::sort(similarities.begin(), similarities.end(),
                  [](const std::pair<float, int>& a, const std::pair<float, int>& b) {
                      return a.first > b.first;
                  });
        
        // Get top-K results
        juce::Array<juce::var> emptyArray;
        auto chunksArray = metadata.getProperty("chunks", juce::var(emptyArray));
        if (!chunksArray.isArray())
        {
            DBG("CLAPSearchWorkerThread: Invalid chunks array in metadata");
            return results;
        }
        
        // Get source files array
        juce::Array<juce::var> sourceFilesArray;
        auto sourceFilesVar = metadata.getProperty("sourceFiles", juce::var());
        if (sourceFilesVar.isArray())
        {
            sourceFilesArray = *sourceFilesVar.getArray();
        }
        
        DBG("CLAPSearchWorkerThread: Found " + juce::String(sourceFilesArray.size()) + " source files in metadata");
        DBG("CLAPSearchWorkerThread: Found " + juce::String(similarities.size()) + " similarity scores");
        
        int numResults = juce::jmin(topK, static_cast<int>(similarities.size()));
        DBG("CLAPSearchWorkerThread: Processing top " + juce::String(numResults) + " results");
        
        for (int i = 0; i < numResults; ++i)
        {
            int chunkIndex = similarities[i].second;
            float similarity = similarities[i].first;
            
            DBG("CLAPSearchWorkerThread: Result " + juce::String(i) + ": chunkIndex=" + juce::String(chunkIndex) + 
                ", similarity=" + juce::String(similarity));
            
            if (chunkIndex >= 0 && chunkIndex < chunksArray.size())
            {
                auto chunkInfo = chunksArray[chunkIndex];
                if (chunkInfo.isObject())
                {
                    // Get source file index
                    int sourceFileIndex = chunkInfo.getProperty("sourceFileIndex", -1);
                    DBG("CLAPSearchWorkerThread:   sourceFileIndex=" + juce::String(sourceFileIndex) + 
                        ", sourceFilesArray.size()=" + juce::String(sourceFilesArray.size()));
                    
                    // Try to get source file first
                    juce::File resultFile;
                    bool foundSourceFile = false;
                    
                    if (sourceFileIndex >= 0 && sourceFileIndex < sourceFilesArray.size())
                    {
                        juce::String sourceFilePath = sourceFilesArray[sourceFileIndex].toString();
                        juce::File sourceFile(sourceFilePath);
                        
                        DBG("CLAPSearchWorkerThread:   Checking source file: " + sourceFilePath);
                        DBG("CLAPSearchWorkerThread:   File exists: " + juce::String(sourceFile.existsAsFile() ? "YES" : "NO"));
                        
                        if (sourceFile.existsAsFile())
                        {
                            resultFile = sourceFile;
                            foundSourceFile = true;
                            DBG("CLAPSearchWorkerThread:   Using source file: " + sourceFile.getFileName());
                        }
                        else
                        {
                            DBG("CLAPSearchWorkerThread:   WARNING: Source file does not exist: " + sourceFilePath);
                        }
                    }
                    else
                    {
                        DBG("CLAPSearchWorkerThread:   WARNING: Invalid sourceFileIndex or empty sourceFilesArray");
                    }
                    
                    // Fallback to chunk file if source file not available
                    if (!foundSourceFile)
                    {
                        juce::String filename = chunkInfo.getProperty("filename", juce::String());
                        if (filename.isNotEmpty())
                        {
                            juce::File chunkFile = palettePath.getChildFile(filename);
                            if (chunkFile.existsAsFile())
                            {
                                resultFile = chunkFile;
                                DBG("CLAPSearchWorkerThread:   Fallback: Using chunk file: " + chunkFile.getFileName());
                            }
                            else
                            {
                                DBG("CLAPSearchWorkerThread:   ERROR: Chunk file also does not exist: " + chunkFile.getFullPathName());
                            }
                        }
                    }
                    
                    // Add to results if we found a valid file
                    if (resultFile.existsAsFile())
                    {
                        results.add(resultFile);
                        DBG("CLAPSearchWorkerThread:   Added file to results: " + resultFile.getFileName());
                    }
                    else
                    {
                        DBG("CLAPSearchWorkerThread:   ERROR: No valid file found for chunk " + juce::String(chunkIndex));
                    }
                }
                else
                {
                    DBG("CLAPSearchWorkerThread:   WARNING: chunkInfo is not an object");
                }
            }
            else
            {
                DBG("CLAPSearchWorkerThread:   WARNING: Invalid chunkIndex: " + juce::String(chunkIndex) + 
                    " (chunksArray.size()=" + juce::String(chunksArray.size()) + ")");
            }
        }
        
        DBG("CLAPSearchWorkerThread: Returning " + juce::String(results.size()) + " results");
        return results;
    }
    
    bool CLAPSearchWorkerThread::loadPaletteIndex(const juce::File& palettePath) const
    {
        // For simple linear search, we don't need to preload the index
        // It's loaded on-demand in searchPalette()
        return true;
    }
}

