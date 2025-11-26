#include "InternalSyncStrategy.h"
#include <cmath>

namespace flower
{

InternalSyncStrategy::InternalSyncStrategy()
{
}

void InternalSyncStrategy::prepare(double sample_rate, int block_size)
{
    // No specific preparation needed for internal strategy
    juce::ignoreUnused(sample_rate, block_size);
}

double InternalSyncStrategy::get_current_beat()
{
    return m_current_beat.load(std::memory_order_relaxed);
}

double InternalSyncStrategy::get_tempo() const
{
    return m_bpm.load(std::memory_order_relaxed);
}

void InternalSyncStrategy::set_tempo(double bpm)
{
    m_bpm.store(juce::jlimit(10.0, 999.0, bpm));
}

bool InternalSyncStrategy::is_playing() const
{
    return m_playing.load(std::memory_order_relaxed);
}

void InternalSyncStrategy::set_playing(bool playing)
{
    m_playing.store(playing);
}

void InternalSyncStrategy::request_reset()
{
    m_current_beat.store(0.0);
}

void InternalSyncStrategy::process(int num_samples, double sample_rate)
{
    if (!m_playing.load(std::memory_order_relaxed))
        return;

    if (sample_rate <= 0.0)
        return;

    const double bpm = m_bpm.load(std::memory_order_relaxed);
    const double samples_per_second = sample_rate;
    const double beats_per_second = bpm / 60.0;
    const double beats_per_sample = beats_per_second / samples_per_second;
    
    double current = m_current_beat.load(std::memory_order_relaxed);
    current += beats_per_sample * num_samples;
    m_current_beat.store(current, std::memory_order_relaxed);
}

double InternalSyncStrategy::get_phase(double quantum) const
{
    if (quantum <= 0.0) return 0.0;
    return std::fmod(m_current_beat.load(std::memory_order_relaxed), quantum);
}

} // namespace flower
