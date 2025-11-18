#include "SoundPaletteCreator.h"
#include "PaletteVisualization.h"
#include "STFTFeatureExtractor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_data_structures/juce_data_structures.h>

namespace CLAPText2Sound
{
    SoundPaletteCreator::SoundPaletteCreator()
    {
    }
    
    SoundPaletteCreator::~SoundPaletteCreator()
    {
    }
    
    juce::File SoundPaletteCreator::createPalette(
        const juce::File& sourceAudioFolder,
        int chunkSizeSeconds,
        std::function<void(const juce::String&)> progressCallback,
        FeatureType featureType)
    {
        if (m_isCreating)
        {
            return juce::File();
        }
        
        m_isCreating = true;
        m_cancelled = false;
        
        if (progressCallback)
            progressCallback("Finding audio files...");
        
        // Find all audio files
        auto audioFiles = findAudioFiles(sourceAudioFolder);
        if (audioFiles.isEmpty())
        {
            m_isCreating = false;
            return juce::File();
        }
        
        if (progressCallback)
            progressCallback("Found " + juce::String(audioFiles.size()) + " audio files");
        
        // Create output directory in ~/Documents/claptext2sound/
        auto docsDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
        auto paletteBaseDir = docsDir.getChildFile("claptext2sound");
        
        // Create base directory if it doesn't exist
        if (!paletteBaseDir.exists())
        {
            paletteBaseDir.createDirectory();
        }
        
        // Create palette directory with name based on source folder
        auto paletteName = sourceAudioFolder.getFileName() + "_SOUND_PALETTE";
        auto paletteDir = paletteBaseDir.getChildFile(paletteName);
        
        if (paletteDir.exists())
        {
            // TODO: Ask user if they want to overwrite or handle this
            paletteDir.deleteRecursively();
        }
        
        if (!paletteDir.createDirectory())
        {
            m_isCreating = false;
            return juce::File();
        }
        
        // Chunk all audio files and track source files
        juce::Array<juce::File> allChunks;
        juce::Array<juce::File> sourceFiles; // Track which source file each chunk came from
        int fileIndex = 0;
        
        for (const auto& audioFile : audioFiles)
        {
            if (m_cancelled)
            {
                m_isCreating = false;
                return juce::File();
            }
            
            if (progressCallback)
                progressCallback("Chunking " + audioFile.getFileName() + " (" + juce::String(fileIndex + 1) + "/" + juce::String(audioFiles.size()) + ")");
            
            auto chunks = chunkAudioFile(audioFile, chunkSizeSeconds, paletteDir, progressCallback);
            // Track source file for each chunk
            for (int i = 0; i < chunks.size(); ++i)
            {
                sourceFiles.add(audioFile);
            }
            allChunks.addArray(chunks);
            fileIndex++;
        }
        
        if (allChunks.isEmpty())
        {
            m_isCreating = false;
            return juce::File();
        }
        
        // Create features (CLAP embeddings or STFT features)
        std::vector<std::vector<float>> embeddings;
        
        if (featureType == FeatureType::CLAP)
        {
            if (progressCallback)
                progressCallback("Creating CLAP embeddings for " + juce::String(allChunks.size()) + " chunks...");
            
            // Initialize ONNX models
            ONNXModelManager modelManager;
            
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
            
            if (!modelManager.initialize(audioModelPath, textModelPath))
            {
                m_isCreating = false;
                return juce::File();
            }
            
            // Create CLAP embeddings
            if (!createEmbeddings(allChunks, paletteDir, modelManager, embeddings, progressCallback))
            {
                m_isCreating = false;
                return juce::File();
            }
        }
        else // FeatureType::STFT
        {
            if (progressCallback)
                progressCallback("Creating STFT features for " + juce::String(allChunks.size()) + " chunks...");
            
            // Create STFT features
            if (!createSTFTFeatures(allChunks, paletteDir, embeddings, progressCallback))
            {
                m_isCreating = false;
                return juce::File();
            }
        }
        
        DBG("SoundPaletteCreator: Created " + juce::String(embeddings.size()) + " embeddings/features");
        
        // Save palette data with source file information
        if (progressCallback)
            progressCallback("Saving palette data...");
        DBG("SoundPaletteCreator: Saving palette data...");
        
        if (!savePaletteData(paletteDir, allChunks, sourceFiles, embeddings))
        {
            DBG("SoundPaletteCreator: Failed to save palette data");
            m_isCreating = false;
            return juce::File();
        }
        
        DBG("SoundPaletteCreator: Palette data saved successfully");
        
        // Compute t-SNE visualization from embeddings
        if (progressCallback)
            progressCallback("Computing t-SNE visualization...");
        DBG("SoundPaletteCreator: Starting t-SNE computation...");
        
        bool tsne_success = EmbeddingSpaceSampler::PaletteVisualization::compute_tsne_from_embeddings(
            paletteDir,
            progressCallback
        );
        
        DBG("SoundPaletteCreator: t-SNE computation completed, success=" + juce::String(tsne_success ? "true" : "false"));
        
        if (!tsne_success)
        {
            DBG("SoundPaletteCreator: Warning - Failed to compute t-SNE visualization. Palette will use grid layout.");
            // Don't fail palette creation if t-SNE fails - grid layout is acceptable fallback
        }
        
        m_isCreating = false;
        
        if (progressCallback)
            progressCallback("Palette created successfully!");
        
        return paletteDir;
    }
    
    void SoundPaletteCreator::cancel()
    {
        m_cancelled = true;
    }
    
    juce::Array<juce::File> SoundPaletteCreator::findAudioFiles(const juce::File& rootFolder) const
    {
        juce::Array<juce::File> audioFiles;
        
        juce::StringArray extensions;
        extensions.add("*.wav");
        extensions.add("*.mp3");
        extensions.add("*.flac");
        extensions.add("*.ogg");
        extensions.add("*.m4a");
        extensions.add("*.aiff");
        extensions.add("*.aif");
        
        // Recursive search
        rootFolder.findChildFiles(audioFiles, juce::File::findFiles, true, extensions.joinIntoString(";"));
        
        return audioFiles;
    }
    
    juce::Array<juce::File> SoundPaletteCreator::chunkAudioFile(
        const juce::File& audioFile,
        int chunkSizeSeconds,
        const juce::File& outputDir,
        std::function<void(const juce::String&)> progressCallback) const
    {
        juce::Array<juce::File> chunkFiles;
        
        if (!audioFile.existsAsFile())
        {
            DBG("SoundPaletteCreator: Audio file does not exist: " + audioFile.getFullPathName());
            return chunkFiles;
        }
        
        // Create format manager and register formats
        juce::AudioFormatManager formatManager;
        formatManager.registerBasicFormats();
        
        // Create reader for audio file
        std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(audioFile));
        if (reader == nullptr)
        {
            DBG("SoundPaletteCreator: Could not create reader for file: " + audioFile.getFullPathName());
            return chunkFiles;
        }
        
        // Target sample rate for CLAP (48kHz)
        const double targetSampleRate = 48000.0;
        const int chunkSizeSamples = static_cast<int>(chunkSizeSeconds * targetSampleRate);
        
        // Resample if needed
        juce::AudioBuffer<float> audioBuffer;
        double actualSampleRate = reader->sampleRate;
        
        if (std::abs(actualSampleRate - targetSampleRate) > 1.0)
        {
            // Need to resample
            juce::int64 numSamples = reader->lengthInSamples;
            audioBuffer.setSize(static_cast<int>(reader->numChannels), static_cast<int>(numSamples));
            
            if (!reader->read(&audioBuffer, 0, static_cast<int>(numSamples), 0, true, true))
            {
                DBG("SoundPaletteCreator: Failed to read audio data");
                return chunkFiles;
            }
            
            // Simple resampling using linear interpolation
            int outputSize = static_cast<int>(numSamples * targetSampleRate / actualSampleRate);
            juce::AudioBuffer<float> resampledBuffer(static_cast<int>(reader->numChannels), outputSize);
            
            for (int channel = 0; channel < audioBuffer.getNumChannels(); ++channel)
            {
                for (int i = 0; i < outputSize; ++i)
                {
                    double srcPos = i * actualSampleRate / targetSampleRate;
                    int srcIndex = static_cast<int>(srcPos);
                    double frac = srcPos - srcIndex;
                    
                    if (srcIndex + 1 < audioBuffer.getNumSamples())
                    {
                        float sample1 = audioBuffer.getSample(channel, srcIndex);
                        float sample2 = audioBuffer.getSample(channel, srcIndex + 1);
                        resampledBuffer.setSample(channel, i, sample1 + frac * (sample2 - sample1));
                    }
                    else if (srcIndex < audioBuffer.getNumSamples())
                    {
                        resampledBuffer.setSample(channel, i, audioBuffer.getSample(channel, srcIndex));
                    }
                }
            }
            
            audioBuffer = std::move(resampledBuffer);
            actualSampleRate = targetSampleRate;
        }
        else
        {
            // No resampling needed
            juce::int64 numSamples = reader->lengthInSamples;
            audioBuffer.setSize(static_cast<int>(reader->numChannels), static_cast<int>(numSamples));
            
            if (!reader->read(&audioBuffer, 0, static_cast<int>(numSamples), 0, true, true))
            {
                DBG("SoundPaletteCreator: Failed to read audio data");
                return chunkFiles;
            }
        }
        
        // Convert to mono if needed
        if (audioBuffer.getNumChannels() > 1)
        {
            juce::AudioBuffer<float> monoBuffer(1, audioBuffer.getNumSamples());
            for (int sample = 0; sample < audioBuffer.getNumSamples(); ++sample)
            {
                float sum = 0.0f;
                for (int channel = 0; channel < audioBuffer.getNumChannels(); ++channel)
                {
                    sum += audioBuffer.getSample(channel, sample);
                }
                monoBuffer.setSample(0, sample, sum / static_cast<float>(audioBuffer.getNumChannels()));
            }
            audioBuffer = std::move(monoBuffer);
        }
        
        // Split into chunks
        int numChunks = static_cast<int>(std::ceil(static_cast<double>(audioBuffer.getNumSamples()) / chunkSizeSamples));
        juce::String baseName = audioFile.getFileNameWithoutExtension();
        
        for (int chunkIndex = 0; chunkIndex < numChunks; ++chunkIndex)
        {
            int startSample = chunkIndex * chunkSizeSamples;
            int numSamplesInChunk = juce::jmin(chunkSizeSamples, audioBuffer.getNumSamples() - startSample);
            
            if (numSamplesInChunk <= 0)
                break;
            
            // Create chunk buffer
            juce::AudioBuffer<float> chunkBuffer(1, chunkSizeSamples);
            chunkBuffer.clear();
            
            // Copy chunk data
            for (int i = 0; i < numSamplesInChunk; ++i)
            {
                chunkBuffer.setSample(0, i, audioBuffer.getSample(0, startSample + i));
            }
            
            // Check if chunk is silence (RMS below threshold)
            float rms = 0.0f;
            for (int i = 0; i < numSamplesInChunk; ++i)
            {
                float sample = chunkBuffer.getSample(0, i);
                rms += sample * sample;
            }
            rms = std::sqrt(rms / static_cast<float>(numSamplesInChunk));
            
            // Skip silent chunks (RMS threshold: -60dB = 0.001)
            const float silenceThreshold = 0.001f;
            if (rms < silenceThreshold)
            {
                DBG("SoundPaletteCreator: Skipping silent chunk " + juce::String(chunkIndex) + " (RMS: " + juce::String(rms) + ")");
                continue;
            }
            
            // Save chunk as WAV file
            juce::String chunkFileName = baseName + "_chunk" + juce::String(chunkIndex).paddedLeft('0', 4) + ".wav";
            juce::File chunkFile = outputDir.getChildFile(chunkFileName);
            
            juce::WavAudioFormat wavFormat;
            juce::AudioFormatWriterOptions options;
            options = options.withSampleRate(targetSampleRate)
                          .withNumChannels(1) // mono
                          .withBitsPerSample(16); // 16-bit
            
            std::unique_ptr<juce::OutputStream> outputStream = std::make_unique<juce::FileOutputStream>(chunkFile);
            std::unique_ptr<juce::AudioFormatWriter> writer(
                wavFormat.createWriterFor(outputStream, options)
            );
            
            if (writer != nullptr)
            {
                writer->writeFromAudioSampleBuffer(chunkBuffer, 0, chunkSizeSamples);
                chunkFiles.add(chunkFile);
            }
            else
            {
                DBG("SoundPaletteCreator: Failed to create writer for chunk: " + chunkFileName);
            }
        }
        
        return chunkFiles;
    }
    
    bool SoundPaletteCreator::createEmbeddings(
        const juce::Array<juce::File>& chunkFiles,
        const juce::File& paletteDir,
        ONNXModelManager& modelManager,
        std::vector<std::vector<float>>& embeddings,
        std::function<void(const juce::String&)> progressCallback) const
    {
        if (!modelManager.isInitialized())
        {
            DBG("SoundPaletteCreator: Model manager not initialized");
            return false;
        }
        
        embeddings.clear();
        juce::AudioFormatManager formatManager;
        formatManager.registerBasicFormats();
        
        for (int i = 0; i < chunkFiles.size(); ++i)
        {
            if (m_cancelled)
            {
                return false;
            }
            
            const auto& chunkFile = chunkFiles[i];
            
            if (progressCallback)
            {
                progressCallback("Processing chunk " + juce::String(i + 1) + "/" + juce::String(chunkFiles.size()) + ": " + chunkFile.getFileName());
            }
            
            // Load audio file
            std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(chunkFile));
            if (reader == nullptr)
            {
                DBG("SoundPaletteCreator: Could not create reader for chunk: " + chunkFile.getFullPathName());
                continue;
            }
            
            // Read audio data
            juce::AudioBuffer<float> audioBuffer(static_cast<int>(reader->numChannels), static_cast<int>(reader->lengthInSamples));
            if (!reader->read(&audioBuffer, 0, static_cast<int>(reader->lengthInSamples), 0, true, true))
            {
                DBG("SoundPaletteCreator: Failed to read chunk audio data");
                continue;
            }
            
            // Convert to mono if needed
            std::vector<float> waveform;
            if (audioBuffer.getNumChannels() == 1)
            {
                waveform.resize(audioBuffer.getNumSamples());
                for (int j = 0; j < audioBuffer.getNumSamples(); ++j)
                {
                    waveform[j] = audioBuffer.getSample(0, j);
                }
            }
            else
            {
                waveform.resize(audioBuffer.getNumSamples());
                for (int j = 0; j < audioBuffer.getNumSamples(); ++j)
                {
                    float sum = 0.0f;
                    for (int channel = 0; channel < audioBuffer.getNumChannels(); ++channel)
                    {
                        sum += audioBuffer.getSample(channel, j);
                    }
                    waveform[j] = sum / static_cast<float>(audioBuffer.getNumChannels());
                }
            }
            
            // Get embedding
            auto embedding = modelManager.getAudioEmbedding(waveform);
            if (!embedding.empty())
            {
                embeddings.push_back(embedding);
            }
            else
            {
                DBG("SoundPaletteCreator: Failed to get embedding for chunk: " + chunkFile.getFileName());
            }
        }
        
        if (embeddings.size() != chunkFiles.size())
        {
            DBG("SoundPaletteCreator: Warning: Only " + juce::String(embeddings.size()) + " embeddings created for " + juce::String(chunkFiles.size()) + " chunks");
        }
        
        return true;
    }
    
    bool SoundPaletteCreator::createSTFTFeatures(
        const juce::Array<juce::File>& chunkFiles,
        const juce::File& paletteDir,
        std::vector<std::vector<float>>& features,
        std::function<void(const juce::String&)> progressCallback) const
    {
        DBG("SoundPaletteCreator::createSTFTFeatures: Starting extraction for " + juce::String(chunkFiles.size()) + " chunks");
        features.clear();
        
        for (int i = 0; i < chunkFiles.size(); ++i)
        {
            if (m_cancelled)
            {
                DBG("SoundPaletteCreator::createSTFTFeatures: Cancelled at chunk " + juce::String(i));
                return false;
            }
            
            const auto& chunkFile = chunkFiles[i];
            
            if (progressCallback)
            {
                progressCallback("Extracting STFT features " + juce::String(i + 1) + "/" + juce::String(chunkFiles.size()) + ": " + chunkFile.getFileName());
            }
            
            DBG("SoundPaletteCreator::createSTFTFeatures: Processing chunk " + juce::String(i + 1) + "/" + juce::String(chunkFiles.size()) + ": " + chunkFile.getFileName());
            
            // Extract STFT features from first 1.5 seconds
            auto stft_features = EmbeddingSpaceSampler::STFTFeatureExtractor::extract_features(chunkFile, 1.5);
            
            if (!stft_features.empty())
            {
                features.push_back(stft_features);
                DBG("SoundPaletteCreator::createSTFTFeatures: Extracted " + juce::String(stft_features.size()) + " features from chunk " + juce::String(i + 1));
            }
            else
            {
                DBG("SoundPaletteCreator::createSTFTFeatures: Failed to extract STFT features for chunk: " + chunkFile.getFileName());
            }
        }
        
        DBG("SoundPaletteCreator::createSTFTFeatures: Completed extraction. Created " + juce::String(features.size()) + " feature vectors");
        
        if (features.size() != chunkFiles.size())
        {
            DBG("SoundPaletteCreator::createSTFTFeatures: Warning: Only " + juce::String(features.size()) + " STFT features created for " + juce::String(chunkFiles.size()) + " chunks");
        }
        
        if (!features.empty())
        {
            DBG("SoundPaletteCreator::createSTFTFeatures: First feature vector size: " + juce::String(features[0].size()));
        }
        
        return true;
    }
    
    bool SoundPaletteCreator::savePaletteData(
        const juce::File& paletteDir,
        const juce::Array<juce::File>& chunkFiles,
        const juce::Array<juce::File>& sourceFiles,
        const std::vector<std::vector<float>>& embeddings) const
    {
        if (chunkFiles.size() != embeddings.size())
        {
            DBG("SoundPaletteCreator: Mismatch between chunk files and embeddings");
            return false;
        }
        
        // Save metadata JSON file
        juce::File metadataFile = paletteDir.getChildFile("metadata.json");
        juce::var metadata(new juce::DynamicObject());
        juce::Array<juce::var> chunksArray;
        
        // Store source files mapping (unique source files)
        juce::Array<juce::File> uniqueSourceFiles;
        juce::HashMap<juce::String, int> sourceFileIndexMap;
        
        for (int i = 0; i < chunkFiles.size(); ++i)
        {
            juce::var chunkInfo(new juce::DynamicObject());
            chunkInfo.getDynamicObject()->setProperty("index", i);
            chunkInfo.getDynamicObject()->setProperty("filename", chunkFiles[i].getFileName());
            chunkInfo.getDynamicObject()->setProperty("path", chunkFiles[i].getRelativePathFrom(paletteDir));
            
            // Store source file index
            juce::String sourcePath = sourceFiles[i].getFullPathName();
            int sourceIndex = -1;
            if (sourceFileIndexMap.contains(sourcePath))
            {
                sourceIndex = sourceFileIndexMap[sourcePath];
            }
            else
            {
                sourceIndex = uniqueSourceFiles.size();
                uniqueSourceFiles.add(sourceFiles[i]);
                sourceFileIndexMap.set(sourcePath, sourceIndex);
            }
            chunkInfo.getDynamicObject()->setProperty("sourceFileIndex", sourceIndex);
            
            chunksArray.add(chunkInfo);
        }
        
        // Store source files array
        juce::Array<juce::var> sourceFilesArray;
        for (const auto& sourceFile : uniqueSourceFiles)
        {
            sourceFilesArray.add(sourceFile.getFullPathName());
        }
        metadata.getDynamicObject()->setProperty("sourceFiles", juce::var(sourceFilesArray));
        
        metadata.getDynamicObject()->setProperty("numChunks", static_cast<int>(chunkFiles.size()));
        metadata.getDynamicObject()->setProperty("embeddingSize", embeddings.empty() ? 0 : static_cast<int>(embeddings[0].size()));
        metadata.getDynamicObject()->setProperty("chunks", juce::var(chunksArray));
        
        // Add t-SNE coordinates and cluster assignments if provided
        // These can be computed externally or added later via updatePaletteVisualization()
        
        // Write metadata JSON
        metadataFile.replaceWithText(juce::JSON::toString(metadata));
        
        // Save embeddings as binary file (simple format: num_embeddings, embedding_size, then all floats)
        juce::File embeddingsFile = paletteDir.getChildFile("embeddings.bin");
        DBG("SoundPaletteCreator::savePaletteData: Opening embeddings file for writing: " + embeddingsFile.getFullPathName());
        
        juce::FileOutputStream outputStream(embeddingsFile);
        
        if (outputStream.openedOk())
        {
            // Write header: num_embeddings (int32), embedding_size (int32)
            int32_t numEmbeddings = static_cast<int32_t>(embeddings.size());
            int32_t embeddingSize = embeddings.empty() ? 0 : static_cast<int32_t>(embeddings[0].size());
            
            DBG("SoundPaletteCreator::savePaletteData: Writing header - numEmbeddings=" + juce::String(numEmbeddings) + 
                ", embeddingSize=" + juce::String(embeddingSize));
            
            outputStream.write(&numEmbeddings, sizeof(int32_t));
            outputStream.write(&embeddingSize, sizeof(int32_t));
            
            // Write all embeddings
            int written_count = 0;
            for (const auto& embedding : embeddings)
            {
                if (embedding.size() == embeddingSize)
                {
                    outputStream.write(embedding.data(), sizeof(float) * embedding.size());
                    written_count++;
                }
                else
                {
                    DBG("SoundPaletteCreator::savePaletteData: Warning - embedding size mismatch: " + 
                        juce::String(embedding.size()) + " != " + juce::String(embeddingSize));
                }
            }
            
            outputStream.flush();
            DBG("SoundPaletteCreator::savePaletteData: Saved " + juce::String(written_count) + "/" + juce::String(numEmbeddings) + 
                " embeddings to " + embeddingsFile.getFullPathName());
            return true;
        }
        else
        {
            DBG("SoundPaletteCreator::savePaletteData: Failed to create embeddings file");
            return false;
        }
    }
}

