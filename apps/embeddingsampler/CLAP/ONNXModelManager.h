#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include <memory>

#if defined(HAVE_ONNXRUNTIME) && HAVE_ONNXRUNTIME == 1
#include <onnxruntime_cxx_api.h>
#endif

namespace CLAPText2Sound
{
    // Forward declaration
    class RobertaTokenizer;
    class ONNXModelManager
    {
    public:
        ONNXModelManager();
        ~ONNXModelManager();
        
        // Initialize models from ONNX files
        bool initialize(const juce::File& audioModelPath, const juce::File& textModelPath);
        
        // Get audio embedding from preprocessed waveform (480000 samples at 48kHz)
        std::vector<float> getAudioEmbedding(const std::vector<float>& waveform);
        
        // Get text embedding from text string
        std::vector<float> getTextEmbedding(const juce::String& text);
        
        // Check if models are loaded
        bool isInitialized() const { return m_initialized; }
        
    private:
        bool m_initialized{false};
        
#if defined(HAVE_ONNXRUNTIME) && HAVE_ONNXRUNTIME == 1
        Ort::Env m_env;
        Ort::SessionOptions m_sessionOptions;
        std::unique_ptr<Ort::Session> m_audioSession;
        std::unique_ptr<Ort::Session> m_textSession;
        Ort::AllocatorWithDefaultOptions m_allocator;
        
        // Input/output names (stored as strings, converted to const char* when needed)
        std::vector<std::string> m_audioInputNames;
        std::vector<std::string> m_audioOutputNames;
        std::vector<std::string> m_textInputNames;
        std::vector<std::string> m_textOutputNames;
        
        // Input/output shapes
        std::vector<int64_t> m_audioInputShape;
        std::vector<int64_t> m_textInputIdsShape;
        std::vector<int64_t> m_textAttentionMaskShape;
#endif
        
        // Audio preprocessing helper
        std::vector<float> preprocessAudio(const std::vector<float>& audio, int targetLength = 480000, int targetSampleRate = 48000);
        
        // Text tokenization helper (RoBERTa)
        // Returns: input_ids (int64) and attention_mask (float32)
        void tokenizeText(const juce::String& text, std::vector<int64_t>& inputIds, std::vector<float>& attentionMask);
        
        // RoBERTa tokenizer instance
        std::unique_ptr<RobertaTokenizer> m_tokenizer;
    };
}

