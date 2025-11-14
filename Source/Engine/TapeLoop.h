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
    void allocateBuffer(double sampleRate, double maxDurationSeconds = 60.0);
    void clearBuffer();
    
    // Buffer access
    std::vector<float>& getBuffer() { return buffer; }
    const std::vector<float>& getBuffer() const { return buffer; }
    size_t getBufferSize() const { return buffer.size(); }
    
    // Recording metadata
    std::atomic<size_t> recordedLength{0}; // Actual length of recorded audio
    std::atomic<bool> hasRecorded{false};  // Whether any audio has been recorded
    
    // Thread safety
    juce::CriticalSection lock;
    
private:
    std::vector<float> buffer;
};

