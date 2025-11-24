#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include <atomic>

// TapeLoop represents a recorded audio loop
// It holds the buffer and metadata about the recording
class TapeLoop
{
public:
    TapeLoop();
    ~TapeLoop() = default;
    
    // Buffer management
    void allocate_buffer(double sample_rate, double max_duration_seconds = 60.0);
    void clear_buffer();
    
    // Buffer access
    std::vector<float>& get_buffer() { return m_buffer; }
    const std::vector<float>& get_buffer() const { return m_buffer; }
    size_t get_buffer_size() const { return m_buffer.size(); }
    
    // Recording metadata
    std::atomic<size_t> m_recorded_length{0}; // Actual length of recorded audio
    std::atomic<bool> m_has_recorded{false};  // Whether any audio has been recorded
    
    // Thread safety
    juce::CriticalSection m_lock;
    
private:
    std::vector<float> m_buffer;
};
