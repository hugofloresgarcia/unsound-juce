#include "Metro.h"
#include <juce_core/juce_core.h>

namespace
{
constexpr float kMinPeriodMs = 5.0f;
}

void Metro::prepare(double sample_rate)
{
    m_sample_rate = sample_rate;
    update_samples_per_period();
    reset();
}

void Metro::set_period_ms(float period_ms)
{
    m_period_ms = juce::jmax(kMinPeriodMs, period_ms);
    update_samples_per_period();
}

void Metro::reset()
{
    m_sample_counter = 0;
    m_tick_ready.store(false);
}

void Metro::process_sample()
{
    if (m_samples_per_period == 0)
        return;

    ++m_sample_counter;
    if (m_sample_counter >= m_samples_per_period)
    {
        m_sample_counter -= m_samples_per_period;
        m_tick_ready.store(true);
    }
}

bool Metro::consume_tick()
{
    return m_tick_ready.exchange(false);
}

float Metro::bpm_to_period_ms(float bpm)
{
    if (bpm <= 0.0f)
        return 0.0f;
    return 60000.0f / bpm;
}

float Metro::period_ms_to_bpm(float period_ms)
{
    if (period_ms <= 0.0f)
        return 0.0f;
    return 60000.0f / period_ms;
}

void Metro::update_samples_per_period()
{
    const double period_seconds = juce::jmax(0.001, static_cast<double>(m_period_ms) / 1000.0);
    m_samples_per_period = static_cast<uint64_t>(period_seconds * m_sample_rate);
    if (m_samples_per_period == 0)
        m_samples_per_period = 1;
}


