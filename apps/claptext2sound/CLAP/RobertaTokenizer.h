#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include <map>
#include <unordered_map>
#include <string>

namespace CLAPText2Sound
{
    /**
     * RoBERTa BPE Tokenizer implementation.
     * 
     * This implements the Byte Pair Encoding (BPE) algorithm used by RoBERTa,
     * loading the vocabulary and merge rules from exported JSON files.
     */
    class RobertaTokenizer
    {
    public:
        RobertaTokenizer();
        ~RobertaTokenizer();
        
        /**
         * Load tokenizer from vocabulary and merges files.
         * 
         * @param vocabFile Path to roberta_vocab.json
         * @param mergesFile Path to roberta_merges.json
         * @param specialTokensFile Path to roberta_special_tokens.json
         * @return true if loaded successfully
         */
        bool load(const juce::File& vocabFile, const juce::File& mergesFile, const juce::File& specialTokensFile);
        
        /**
         * Tokenize text and return input_ids and attention_mask.
         * 
         * @param text Input text string
         * @param inputIds Output vector for token IDs (int64)
         * @param attentionMask Output vector for attention mask (float32)
         * @param maxLength Maximum sequence length (default 77)
         * @param addSpecialTokens Whether to add <s> and </s> tokens
         */
        void tokenize(const juce::String& text,
                     std::vector<int64_t>& inputIds,
                     std::vector<float>& attentionMask,
                     int maxLength = 77,
                     bool addSpecialTokens = true) const;
        
        /**
         * Check if tokenizer is loaded and ready.
         */
        bool isLoaded() const { return m_loaded; }
        
    private:
        bool m_loaded{false};
        
        // Vocabulary: token string -> token ID
        std::unordered_map<std::string, int64_t> m_vocab;
        
        // Reverse vocabulary: token ID -> token string
        std::map<int64_t, std::string> m_idToToken;
        
        // BPE merges: (token1, token2) -> rank (lower rank = higher priority)
        std::map<std::pair<std::string, std::string>, int> m_merges;
        
        // Special tokens
        int64_t m_bosTokenId{0};      // <s>
        int64_t m_eosTokenId{2};      // </s>
        int64_t m_padTokenId{1};      // <pad>
        int64_t m_unkTokenId{3};      // <unk>
        
        /**
         * Apply BPE encoding to a word.
         * 
         * @param word Input word (already split into characters with Ġ prefix for spaces)
         * @return List of BPE tokens
         */
        std::vector<std::string> bpe(const std::string& word) const;
        
        /**
         * Get all pairs of consecutive tokens.
         */
        std::vector<std::pair<std::string, std::string>> getPairs(const std::vector<std::string>& word) const;
        
        /**
         * Preprocess text: add space prefix to words (RoBERTa style).
         */
        std::string preprocessText(const juce::String& text) const;
        
        /**
         * Split text into words and add Ġ prefix (space prefix character used by RoBERTa).
         */
        std::vector<std::string> splitIntoWords(const std::string& text) const;
    };
}

