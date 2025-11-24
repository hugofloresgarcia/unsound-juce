#include "TapeLoop.h"

TapeLoop::TapeLoop()
{
    // Buffer will be allocated when sample rate is known from audio device
}

void TapeLoop::allocate_buffer(double sample_rate, double max_duration_seconds)
{
    const juce::ScopedLock sl(m_lock);
    size_t buffer_size = static_cast<size_t>(sample_rate * max_duration_seconds);
    m_buffer.resize(buffer_size, 0.0f);
    m_recorded_length.store(0);
    m_has_recorded.store(false);
}

void TapeLoop::clear_buffer()
{
    const juce::ScopedLock sl(m_lock);
    std::fill(m_buffer.begin(), m_buffer.end(), 0.0f);
    m_recorded_length.store(0);
    m_has_recorded.store(false);
}
