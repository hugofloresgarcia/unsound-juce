#include "RobertaTokenizer.h"
#include <juce_data_structures/juce_data_structures.h>
#include <algorithm>
#include <sstream>
#include <cctype>

namespace Unsound4All
{
    RobertaTokenizer::RobertaTokenizer()
    {
    }
    
    RobertaTokenizer::~RobertaTokenizer()
    {
    }
    
    bool RobertaTokenizer::load(const juce::File& vocabFile, const juce::File& mergesFile, const juce::File& specialTokensFile)
    {
        m_loaded = false;
        m_vocab.clear();
        m_idToToken.clear();
        m_merges.clear();
        
        // Load vocabulary
        if (!vocabFile.existsAsFile())
        {
            DBG("RobertaTokenizer: Vocabulary file not found: " + vocabFile.getFullPathName());
            return false;
        }
        
        juce::var vocabVar = juce::JSON::parse(vocabFile);
        if (!vocabVar.isObject())
        {
            DBG("RobertaTokenizer: Failed to parse vocabulary JSON");
            return false;
        }
        
        auto* vocabObj = vocabVar.getDynamicObject();
        if (vocabObj == nullptr)
        {
            DBG("RobertaTokenizer: Invalid vocabulary object");
            return false;
        }
        
        // Load vocabulary mapping
        for (auto& prop : vocabObj->getProperties())
        {
            juce::String token = prop.name.toString();
            int64_t tokenId = static_cast<int64_t>(prop.value);
            m_vocab[token.toStdString()] = tokenId;
            m_idToToken[tokenId] = token.toStdString();
        }
        
        DBG("RobertaTokenizer: Loaded " + juce::String(m_vocab.size()) + " vocabulary tokens");
        
        // Load merges
        if (!mergesFile.existsAsFile())
        {
            DBG("RobertaTokenizer: Merges file not found: " + mergesFile.getFullPathName());
            return false;
        }
        
        juce::var mergesVar = juce::JSON::parse(mergesFile);
        if (!mergesVar.isArray())
        {
            DBG("RobertaTokenizer: Failed to parse merges JSON");
            return false;
        }
        
        auto* mergesArray = mergesVar.getArray();
        if (mergesArray == nullptr)
        {
            DBG("RobertaTokenizer: Invalid merges array");
            return false;
        }
        
        // Load merge rules (ranked by priority, lower index = higher priority)
        for (int i = 0; i < mergesArray->size(); ++i)
        {
            auto mergeVar = (*mergesArray)[i];
            if (mergeVar.isArray() && mergeVar.size() == 2)
            {
                std::string token1 = mergeVar[0].toString().toStdString();
                std::string token2 = mergeVar[1].toString().toStdString();
                m_merges[{token1, token2}] = i; // Lower index = higher priority
            }
        }
        
        DBG("RobertaTokenizer: Loaded " + juce::String(m_merges.size()) + " BPE merge rules");
        
        // Load special tokens
        if (specialTokensFile.existsAsFile())
        {
            juce::var specialVar = juce::JSON::parse(specialTokensFile);
            if (specialVar.isObject())
            {
                auto* specialObj = specialVar.getDynamicObject();
                if (specialObj != nullptr)
                {
                    auto bosVar = specialObj->getProperty("bos_token_id");
                    auto eosVar = specialObj->getProperty("eos_token_id");
                    auto padVar = specialObj->getProperty("pad_token_id");
                    auto unkVar = specialObj->getProperty("unk_token_id");
                    
                    m_bosTokenId = bosVar.isInt() || bosVar.isInt64() ? static_cast<int64_t>(bosVar) : 0;
                    m_eosTokenId = eosVar.isInt() || eosVar.isInt64() ? static_cast<int64_t>(eosVar) : 2;
                    m_padTokenId = padVar.isInt() || padVar.isInt64() ? static_cast<int64_t>(padVar) : 1;
                    m_unkTokenId = unkVar.isInt() || unkVar.isInt64() ? static_cast<int64_t>(unkVar) : 3;
                }
            }
        }
        
        m_loaded = true;
        DBG("RobertaTokenizer: Successfully loaded tokenizer");
        return true;
    }
    
    std::string RobertaTokenizer::preprocessText(const juce::String& text) const
    {
        // RoBERTa normalizes text: lowercase and add spaces around punctuation
        juce::String normalized = text.trim().toLowerCase();
        
        // Add spaces around punctuation (simplified)
        juce::String result;
        for (juce::juce_wchar c : normalized)
        {
            if (std::ispunct(static_cast<unsigned char>(c)) && c != '\'' && c != '-')
            {
                if (!result.isEmpty() && !result.endsWithChar(' '))
                    result += ' ';
                result += c;
                result += ' ';
            }
            else
            {
                result += c;
            }
        }
        
        return result.toStdString();
    }
    
    std::vector<std::string> RobertaTokenizer::splitIntoWords(const std::string& text) const
    {
        // Split by whitespace
        std::vector<std::string> words;
        std::string currentWord;
        
        for (char c : text)
        {
            if (std::isspace(c))
            {
                if (!currentWord.empty())
                {
                    words.push_back(currentWord);
                    currentWord.clear();
                }
            }
            else
            {
                currentWord += c;
            }
        }
        
        if (!currentWord.empty())
        {
            words.push_back(currentWord);
        }
        
        return words;
    }
    
    std::vector<std::pair<std::string, std::string>> RobertaTokenizer::getPairs(const std::vector<std::string>& word) const
    {
        std::vector<std::pair<std::string, std::string>> pairs;
        if (word.size() < 2)
            return pairs;
        
        for (size_t i = 0; i < word.size() - 1; ++i)
        {
            pairs.push_back({word[i], word[i+1]});
        }
        
        return pairs;
    }
    
    std::vector<std::string> RobertaTokenizer::bpe(const std::string& word) const
    {
        // RoBERTa uses byte-level BPE with GPT-2 style encoding
        // First encode word as bytes, then apply BPE merges
        
        if (word.empty())
            return {};
        
        // Convert word to UTF-8 bytes
        std::vector<uint8_t> bytes(word.begin(), word.end());
        
        // Convert bytes to initial token strings
        // RoBERTa uses byte-level encoding where each byte becomes a character
        // For printable ASCII, use the character directly
        // For others, use byte representation
        std::vector<std::string> wordTokens;
        for (uint8_t byte : bytes)
        {
            // Check if this byte is in vocabulary as a single character
            std::string charToken(1, static_cast<char>(byte));
            
            // Try to find in vocabulary first
            if (m_vocab.find(charToken) != m_vocab.end())
            {
                wordTokens.push_back(charToken);
            }
            else
            {
                // Use byte representation (RoBERTa format)
                // Format: byte_0xXX or similar
                wordTokens.push_back(charToken); // Simplified - use char anyway
            }
        }
        
        // If word is empty or single character, return as-is
        if (wordTokens.size() <= 1)
        {
            return wordTokens;
        }
        
        // Apply BPE merges (optimized: only check pairs that exist in the word)
        int iterations = 0;
        const int maxIterations = 1000; // Safety limit
        
        while (iterations < maxIterations)
        {
            iterations++;
            
            // Get all pairs from current wordTokens
            auto pairs = getPairs(wordTokens);
            if (pairs.empty())
                break;
            
            // Find the pair with the lowest rank (highest priority) that exists in merges
            std::pair<std::string, std::string> bestPair;
            int bestRank = INT_MAX;
            bool found = false;
            
            // Only check pairs that actually exist in the word (much faster than checking all 50k merges)
            for (const auto& pair : pairs)
            {
                auto it = m_merges.find(pair);
                if (it != m_merges.end() && it->second < bestRank)
                {
                    bestPair = pair;
                    bestRank = it->second;
                    found = true;
                }
            }
            
            if (!found)
                break; // No more merges possible
            
            // Merge the best pair (optimized: use move semantics and reserve)
            std::vector<std::string> newWord;
            newWord.reserve(wordTokens.size() - 1); // Pre-allocate
            
            size_t i = 0;
            while (i < wordTokens.size())
            {
                if (i < wordTokens.size() - 1 &&
                    wordTokens[i] == bestPair.first &&
                    wordTokens[i+1] == bestPair.second)
                {
                    // Merge these two tokens
                    std::string merged = bestPair.first + bestPair.second;
                    newWord.push_back(std::move(merged));
                    i += 2;
                }
                else
                {
                    newWord.push_back(std::move(wordTokens[i]));
                    i += 1;
                }
            }
            
            wordTokens = std::move(newWord);
        }
        
        return wordTokens;
    }
    
    void RobertaTokenizer::tokenize(const juce::String& text,
                                   std::vector<int64_t>& inputIds,
                                   std::vector<float>& attentionMask,
                                   int maxLength,
                                   bool addSpecialTokens) const
    {
        inputIds.clear();
        attentionMask.clear();
        
        if (!m_loaded)
        {
            DBG("RobertaTokenizer: Tokenizer not loaded");
            return;
        }
        
        // Preprocess text
        std::string preprocessed = preprocessText(text);
        
        // Split into words
        std::vector<std::string> words = splitIntoWords(preprocessed);
        
        // Add BOS token if requested
        if (addSpecialTokens)
        {
            inputIds.push_back(m_bosTokenId);
            attentionMask.push_back(1.0f);
        }
        
        // Tokenize each word using BPE
        // First word doesn't get space prefix, subsequent words do
        bool isFirstWord = true;
        for (const auto& word : words)
        {
            if (static_cast<int>(inputIds.size()) >= maxLength - (addSpecialTokens ? 1 : 0))
                break;
            
            // Apply BPE to word
            std::vector<std::string> bpeTokens = bpe(word);
            
            // Add space prefix (Ġ) to tokens if not first word
            // RoBERTa uses \u0120 (Ġ) to mark word boundaries
            const std::string spacePrefix = "\u0120"; // Unicode space prefix character
            
            for (size_t i = 0; i < bpeTokens.size(); ++i)
            {
                if (static_cast<int>(inputIds.size()) >= maxLength - (addSpecialTokens ? 1 : 0))
                    break;
                
                std::string token = bpeTokens[i];
                
                // Add space prefix to first token of word if not first word
                if (!isFirstWord && i == 0)
                {
                    token = spacePrefix + token;
                }
                
                // Look up token ID (try with space prefix first, then without)
                auto it = m_vocab.find(token);
                if (it == m_vocab.end() && !isFirstWord && i == 0)
                {
                    // Try without space prefix as fallback
                    it = m_vocab.find(bpeTokens[i]);
                }
                
                int64_t tokenId = (it != m_vocab.end()) ? it->second : m_unkTokenId;
                
                inputIds.push_back(tokenId);
                attentionMask.push_back(1.0f);
            }
            
            isFirstWord = false;
        }
        
        // Add EOS token if requested
        if (addSpecialTokens && static_cast<int>(inputIds.size()) < maxLength)
        {
            inputIds.push_back(m_eosTokenId);
            attentionMask.push_back(1.0f);
        }
        
        // Pad to maxLength
        while (static_cast<int>(inputIds.size()) < maxLength)
        {
            inputIds.push_back(m_padTokenId);
            attentionMask.push_back(0.0f);
        }
        
        // Truncate if too long
        if (static_cast<int>(inputIds.size()) > maxLength)
        {
            inputIds.resize(maxLength);
            attentionMask.resize(maxLength);
            // Ensure EOS token is at the end if we had special tokens
            if (addSpecialTokens && maxLength > 0)
            {
                inputIds[maxLength - 1] = m_eosTokenId;
                attentionMask[maxLength - 1] = 1.0f;
            }
        }
    }
}

