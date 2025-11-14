#include "TapeLoop.h"

TapeLoop::TapeLoop()
{
    // Buffer will be allocated when sample rate is known from audio device
}

void TapeLoop::allocateBuffer(double sampleRate, double maxDurationSeconds)
{
    const juce::ScopedLock sl(lock);
    size_t bufferSize = static_cast<size_t>(sampleRate * maxDurationSeconds);
    buffer.resize(bufferSize, 0.0f);
    recordedLength.store(0);
    hasRecorded.store(false);
}

void TapeLoop::clearBuffer()
{
    const juce::ScopedLock sl(lock);
    std::fill(buffer.begin(), buffer.end(), 0.0f);
    recordedLength.store(0);
    hasRecorded.store(false);
}

