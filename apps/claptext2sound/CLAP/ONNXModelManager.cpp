#include "ONNXModelManager.h"
#include "RobertaTokenizer.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <algorithm>

namespace CLAPText2Sound
{
    ONNXModelManager::ONNXModelManager()
#if defined(HAVE_ONNXRUNTIME) && HAVE_ONNXRUNTIME == 1
        : m_env(ORT_LOGGING_LEVEL_WARNING, "CLAPText2Sound")
#endif
    {
    }
    
    ONNXModelManager::~ONNXModelManager()
    {
        // Sessions will be automatically destroyed by unique_ptr
    }
    
    bool ONNXModelManager::initialize(const juce::File& audioModelPath, const juce::File& textModelPath)
    {
#if defined(HAVE_ONNXRUNTIME) && HAVE_ONNXRUNTIME == 1
        try
        {
            // Check if model files exist
            if (!audioModelPath.existsAsFile())
            {
                DBG("ONNXModelManager: Audio model file not found: " + audioModelPath.getFullPathName());
                m_initialized = false;
                return false;
            }
            
            if (!textModelPath.existsAsFile())
            {
                DBG("ONNXModelManager: Text model file not found: " + textModelPath.getFullPathName());
                m_initialized = false;
                return false;
            }
            
            // Convert JUCE File paths to std::wstring (ONNX Runtime expects wide strings on Windows, but std::string on Unix)
            #ifdef _WIN32
            std::wstring audioModelPathStr = audioModelPath.getFullPathName().toWideCharPointer();
            std::wstring textModelPathStr = textModelPath.getFullPathName().toWideCharPointer();
            #else
            std::string audioModelPathStr = audioModelPath.getFullPathName().toStdString();
            std::string textModelPathStr = textModelPath.getFullPathName().toStdString();
            #endif
            
            // Create sessions
            m_audioSession = std::make_unique<Ort::Session>(m_env, audioModelPathStr.c_str(), m_sessionOptions);
            m_textSession = std::make_unique<Ort::Session>(m_env, textModelPathStr.c_str(), m_sessionOptions);
            
            // Get input/output names for audio encoder
            m_audioInputNames = m_audioSession->GetInputNames();
            m_audioOutputNames = m_audioSession->GetOutputNames();
            
            // Get input shape for first input
            if (!m_audioInputNames.empty())
            {
                Ort::TypeInfo typeInfo = m_audioSession->GetInputTypeInfo(0);
                auto tensorInfo = typeInfo.GetTensorTypeAndShapeInfo();
                m_audioInputShape = tensorInfo.GetShape();
            }
            
            // Get input/output names for text encoder
            m_textInputNames = m_textSession->GetInputNames();
            m_textOutputNames = m_textSession->GetOutputNames();
            
            // Get input shapes for text inputs
            for (size_t i = 0; i < m_textInputNames.size(); ++i)
            {
                Ort::TypeInfo typeInfo = m_textSession->GetInputTypeInfo(i);
                auto tensorInfo = typeInfo.GetTensorTypeAndShapeInfo();
                auto shape = tensorInfo.GetShape();
                
                if (m_textInputNames[i].find("input_ids") != std::string::npos)
                {
                    m_textInputIdsShape = shape;
                }
                else if (m_textInputNames[i].find("attention_mask") != std::string::npos)
                {
                    m_textAttentionMaskShape = shape;
                }
            }
            
            // Initialize RoBERTa tokenizer
            // Find tokenizer files in the same directory as the models
            auto modelDir = textModelPath.getParentDirectory();
            juce::File vocabFile = modelDir.getChildFile("roberta_vocab.json");
            juce::File mergesFile = modelDir.getChildFile("roberta_merges.json");
            juce::File specialTokensFile = modelDir.getChildFile("roberta_special_tokens.json");
            
            // Also check in assets directory (for bundled app)
            if (!vocabFile.existsAsFile())
            {
                auto assetsDir = textModelPath.getParentDirectory().getParentDirectory().getChildFile("assets");
                if (!assetsDir.exists())
                {
                    // Try alternative path (for macOS app bundle)
                    assetsDir = textModelPath.getParentDirectory().getParentDirectory()
                                 .getParentDirectory().getParentDirectory().getChildFile("assets");
                }
                if (assetsDir.exists())
                {
                    vocabFile = assetsDir.getChildFile("roberta_vocab.json");
                    mergesFile = assetsDir.getChildFile("roberta_merges.json");
                    specialTokensFile = assetsDir.getChildFile("roberta_special_tokens.json");
                }
            }
            
            m_tokenizer = std::make_unique<RobertaTokenizer>();
            if (vocabFile.existsAsFile() && mergesFile.existsAsFile() && specialTokensFile.existsAsFile())
            {
                if (m_tokenizer->load(vocabFile, mergesFile, specialTokensFile))
                {
                    DBG("ONNXModelManager: RoBERTa tokenizer loaded successfully");
                }
                else
                {
                    DBG("ONNXModelManager: Warning: Failed to load RoBERTa tokenizer, using fallback");
                    m_tokenizer.reset();
                }
            }
            else
            {
                DBG("ONNXModelManager: Warning: Tokenizer files not found, using fallback tokenization");
                DBG("  Looking for: " + vocabFile.getFullPathName());
                m_tokenizer.reset();
            }
            
            m_initialized = true;
            DBG("ONNXModelManager: Models initialized successfully");
            return true;
        }
        catch (const std::exception& e)
        {
            DBG("ONNXModelManager: Error initializing models: " + juce::String(e.what()));
            m_initialized = false;
            return false;
        }
#else
        DBG("ONNXModelManager: ONNX Runtime not available. Please install ONNX Runtime C++ libraries.");
        m_initialized = false;
        return false;
#endif
    }
    
    std::vector<float> ONNXModelManager::getAudioEmbedding(const std::vector<float>& waveform)
    {
        if (!m_initialized)
        {
            return {};
        }
        
#if defined(HAVE_ONNXRUNTIME) && HAVE_ONNXRUNTIME == 1
        try
        {
            // Preprocess audio
            auto preprocessed = preprocessAudio(waveform);
            if (preprocessed.size() != 480000)
            {
                DBG("ONNXModelManager: Preprocessed waveform has wrong size: " + juce::String(preprocessed.size()));
                return {};
            }
            
            // Create input tensor (shape: [1, 480000])
            std::vector<int64_t> inputShape = {1, 480000};
            Ort::MemoryInfo memoryInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
            Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
                memoryInfo,
                preprocessed.data(),
                preprocessed.size(),
                inputShape.data(),
                inputShape.size()
            );
            
            // Convert string names to const char* for Run()
            std::vector<const char*> inputNames;
            std::vector<const char*> outputNames;
            for (const auto& name : m_audioInputNames)
                inputNames.push_back(name.c_str());
            for (const auto& name : m_audioOutputNames)
                outputNames.push_back(name.c_str());
            
            // Run inference
            auto outputTensors = m_audioSession->Run(
                Ort::RunOptions{nullptr},
                inputNames.data(),
                &inputTensor,
                1,
                outputNames.data(),
                1
            );
            
            // Extract output
            float* outputData = outputTensors[0].GetTensorMutableData<float>();
            auto outputShape = outputTensors[0].GetTensorTypeAndShapeInfo().GetShape();
            
            size_t outputSize = 1;
            for (auto dim : outputShape)
            {
                if (dim > 0)
                    outputSize *= dim;
            }
            
            // Remove batch dimension if present
            size_t embeddingSize = outputShape.size() > 1 ? outputShape[1] : outputSize;
            
            std::vector<float> embedding(outputData, outputData + embeddingSize);
            
            // Normalize embedding
            float norm = 0.0f;
            for (float val : embedding)
            {
                norm += val * val;
            }
            norm = std::sqrt(norm);
            if (norm > 1e-8f)
            {
                for (float& val : embedding)
                {
                    val /= norm;
                }
            }
            
            return embedding;
        }
        catch (const std::exception& e)
        {
            DBG("ONNXModelManager: Error computing audio embedding: " + juce::String(e.what()));
            return {};
        }
#else
        return {};
#endif
    }
    
    std::vector<float> ONNXModelManager::getTextEmbedding(const juce::String& text)
    {
        if (!m_initialized)
        {
            return {};
        }
        
#if defined(HAVE_ONNXRUNTIME) && HAVE_ONNXRUNTIME == 1
        try
        {
            // Tokenize text
            std::vector<int64_t> inputIds;
            std::vector<float> attentionMask;
            tokenizeText(text, inputIds, attentionMask);
            
            if (inputIds.empty())
            {
                DBG("ONNXModelManager: Tokenization failed");
                return {};
            }
            
            // Create input tensors
            std::vector<int64_t> inputIdsShape = {1, static_cast<int64_t>(inputIds.size())};
            std::vector<int64_t> attentionMaskShape = {1, static_cast<int64_t>(attentionMask.size())};
            
            Ort::MemoryInfo memoryInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
            Ort::Value inputIdsTensor = Ort::Value::CreateTensor<int64_t>(
                memoryInfo,
                inputIds.data(),
                inputIds.size(),
                inputIdsShape.data(),
                inputIdsShape.size()
            );
            
            Ort::Value attentionMaskTensor = Ort::Value::CreateTensor<float>(
                memoryInfo,
                attentionMask.data(),
                attentionMask.size(),
                attentionMaskShape.data(),
                attentionMaskShape.size()
            );
            
            // Prepare input array
            std::vector<Ort::Value> inputTensors;
            std::vector<const char*> inputNames;
            
            // Add inputs in the order expected by the model
            for (const auto& name : m_textInputNames)
            {
                if (name.find("input_ids") != std::string::npos)
                {
                    inputTensors.push_back(std::move(inputIdsTensor));
                    inputNames.push_back(name.c_str());
                }
                else if (name.find("attention_mask") != std::string::npos)
                {
                    inputTensors.push_back(std::move(attentionMaskTensor));
                    inputNames.push_back(name.c_str());
                }
            }
            
            // Convert output names to const char*
            std::vector<const char*> outputNames;
            for (const auto& name : m_textOutputNames)
                outputNames.push_back(name.c_str());
            
            // Run inference
            auto outputTensors = m_textSession->Run(
                Ort::RunOptions{nullptr},
                inputNames.data(),
                inputTensors.data(),
                inputTensors.size(),
                outputNames.data(),
                1
            );
            
            // Extract output
            float* outputData = outputTensors[0].GetTensorMutableData<float>();
            auto outputShape = outputTensors[0].GetTensorTypeAndShapeInfo().GetShape();
            
            size_t outputSize = 1;
            for (auto dim : outputShape)
            {
                if (dim > 0)
                    outputSize *= dim;
            }
            
            // Remove batch dimension if present
            size_t embeddingSize = outputShape.size() > 1 ? outputShape[1] : outputSize;
            
            std::vector<float> embedding(outputData, outputData + embeddingSize);
            
            // Normalize embedding
            float norm = 0.0f;
            for (float val : embedding)
            {
                norm += val * val;
            }
            norm = std::sqrt(norm);
            if (norm > 1e-8f)
            {
                for (float& val : embedding)
                {
                    val /= norm;
                }
            }
            
            DBG("ONNXModelManager: Text embedding computed for text: '" + text.substring(0, 50) + 
                "', size: " + juce::String(embedding.size()) + ", norm: " + juce::String(norm));
            
            return embedding;
        }
        catch (const std::exception& e)
        {
            DBG("ONNXModelManager: Error computing text embedding: " + juce::String(e.what()));
            return {};
        }
#else
        return {};
#endif
    }
    
    std::vector<float> ONNXModelManager::preprocessAudio(const std::vector<float>& audio, int targetLength, int targetSampleRate)
    {
        std::vector<float> result;
        
        if (audio.empty())
        {
            return result;
        }
        
        // Quantization: float32 -> int16 -> float32 (as done in CLAP)
        // This simulates the quantization process
        std::vector<float> quantized(audio.size());
        for (size_t i = 0; i < audio.size(); ++i)
        {
            // Clamp to [-1, 1] range
            float clamped = std::max(-1.0f, std::min(1.0f, audio[i]));
            
            // Quantize to int16
            int16_t quantizedInt = static_cast<int16_t>(clamped * 32767.0f);
            
            // Convert back to float32
            quantized[i] = static_cast<float>(quantizedInt) / 32767.0f;
        }
        
        // Pad or truncate to target length
        result.resize(targetLength, 0.0f);
        
        if (quantized.size() >= targetLength)
        {
            // Truncate: take first targetLength samples
            std::copy(quantized.begin(), quantized.begin() + targetLength, result.begin());
        }
        else
        {
            // Pad: repeat the audio to fill targetLength
            size_t pos = 0;
            while (pos < targetLength)
            {
                size_t toCopy = std::min(quantized.size(), targetLength - pos);
                std::copy(quantized.begin(), quantized.begin() + toCopy, result.begin() + pos);
                pos += toCopy;
            }
        }
        
        return result;
    }
    
    void ONNXModelManager::tokenizeText(const juce::String& text, std::vector<int64_t>& inputIds, std::vector<float>& attentionMask)
    {
        inputIds.clear();
        attentionMask.clear();
        
        // Use proper RoBERTa tokenizer if available
        if (m_tokenizer != nullptr && m_tokenizer->isLoaded())
        {
            m_tokenizer->tokenize(text, inputIds, attentionMask, 77, true);
            return;
        }
        
        // Fallback: Simplified tokenization (should not be used in production)
        DBG("ONNXModelManager: Using fallback tokenization (RoBERTa tokenizer not loaded)");
        
        const int64_t CLS_TOKEN = 0;      // <s>
        const int64_t SEP_TOKEN = 2;      // </s>
        const int64_t PAD_TOKEN = 1;      // <pad>
        const int64_t UNK_TOKEN = 3;      // <unk>
        const int MAX_LENGTH = 77;
        
        // Add CLS token
        inputIds.push_back(CLS_TOKEN);
        attentionMask.push_back(1.0f);
        
        // Simple word tokenization (split by spaces and punctuation)
        juce::StringArray tokens;
        juce::String currentToken;
        
        for (juce::juce_wchar c : text)
        {
            if (c == ' ' || c == '\t' || c == '\n')
            {
                if (currentToken.isNotEmpty())
                {
                    tokens.add(currentToken.toLowerCase());
                    currentToken = juce::String();
                }
            }
            else if (std::ispunct(static_cast<unsigned char>(c)))
            {
                if (currentToken.isNotEmpty())
                {
                    tokens.add(currentToken.toLowerCase());
                    currentToken = juce::String();
                }
                // Add punctuation as separate token
                tokens.add(juce::String::charToString(c).toLowerCase());
            }
            else
            {
                currentToken += c;
            }
        }
        
        if (currentToken.isNotEmpty())
        {
            tokens.add(currentToken.toLowerCase());
        }
        
        // Convert words to token IDs (simplified hash-based mapping)
        // In a real implementation, this would use a proper vocabulary
        for (const auto& token : tokens)
        {
            if (inputIds.size() >= MAX_LENGTH - 1) // -1 for SEP token
                break;
            
            // Simple hash-based token ID (modulo a large prime to get reasonable IDs)
            int64_t tokenId = static_cast<int64_t>(std::hash<std::string>{}(token.toStdString())) % 50265;
            if (tokenId < 4) tokenId += 4; // Avoid special tokens
            
            inputIds.push_back(tokenId);
            attentionMask.push_back(1.0f);
        }
        
        // Add SEP token
        inputIds.push_back(SEP_TOKEN);
        attentionMask.push_back(1.0f);
        
        // Pad to MAX_LENGTH
        while (inputIds.size() < MAX_LENGTH)
        {
            inputIds.push_back(PAD_TOKEN);
            attentionMask.push_back(0.0f);
        }
        
        // Truncate if too long
        if (inputIds.size() > MAX_LENGTH)
        {
            inputIds.resize(MAX_LENGTH);
            attentionMask.resize(MAX_LENGTH);
            // Ensure SEP token is at the end
            inputIds[MAX_LENGTH - 1] = SEP_TOKEN;
            attentionMask[MAX_LENGTH - 1] = 1.0f;
        }
    }
}
