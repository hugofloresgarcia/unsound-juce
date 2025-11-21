#include "KnobSweepRecorder.h"
#include <cmath>

namespace
{
constexpr double kMinSampleDeltaMs = 0.5;
constexpr double kMaxLoopDurationMs = 60000.0;

inline double to_relative_time(const double now_ms, const double start_ms)
{
    return juce::jmax(0.0, now_ms - start_ms);
}

inline bool should_store_sample(const KnobSweepRecorder::SamplePoint& last_sample,
                                double relative_time,
                                float value)
{
    const double delta_time = relative_time - last_sample.time_ms;
    const float delta_value = std::abs(value - last_sample.value);
    return delta_time >= kMinSampleDeltaMs || delta_value > 0.0001f;
}

inline float interpolate(const KnobSweepRecorder::SamplePoint& a,
                         const KnobSweepRecorder::SamplePoint& b,
                         double time_ms)
{
    const double span = juce::jmax(1.0, b.time_ms - a.time_ms);
    const double alpha = juce::jlimit(0.0, 1.0, (time_ms - a.time_ms) / span);
    return juce::jmap(static_cast<float>(alpha), 0.0f, 1.0f, a.value, b.value);
}
}

KnobSweepRecorder::KnobSweepRecorder()
{
    clear();
}

void KnobSweepRecorder::prepare(double sample_rate_hz)
{
    m_sample_rate = (sample_rate_hz > 0.0) ? sample_rate_hz : 44100.0;
    clear();
}

void KnobSweepRecorder::set_idle_value(float value)
{
    m_idle_value = value;
    if (!m_is_playing.load())
        m_last_recorded_value = value;
}

void KnobSweepRecorder::arm()
{
    m_is_armed.store(true);
    m_is_recording.store(false);
    m_is_playing.store(false);
}

void KnobSweepRecorder::begin_record(double now_ms)
{
    if (!m_is_armed.load())
    {
        DBG("KnobSweepRecorder::begin_record early return (not armed)");
        return;
    }

    const juce::SpinLock::ScopedLockType lock(m_sample_lock);

    m_record_samples.clear();
    m_record_samples.reserve(128);
    m_record_start_ms = now_ms;

    m_record_samples.push_back({0.0, m_idle_value});
    m_last_recorded_value = m_idle_value;

    m_is_recording.store(true);
    m_is_armed.store(false);
    m_is_playing.store(false);
}

void KnobSweepRecorder::push_sample(double now_ms, float value)
{
    if (!m_is_recording.load())
    {
        DBG("KnobSweepRecorder::push_sample early return (not recording)");
        return;
    }

    const double relative_time = to_relative_time(now_ms, m_record_start_ms);
    if (relative_time > kMaxLoopDurationMs)
    {
        DBG("KnobSweepRecorder::push_sample early return (duration exceeded)");
        return;
    }

    const juce::SpinLock::ScopedLockType lock(m_sample_lock);

    if (!m_record_samples.empty())
    {
        const auto& last = m_record_samples.back();
        if (!should_store_sample(last, relative_time, value))
            return;
    }

    m_record_samples.push_back({relative_time, value});
    m_last_recorded_value = value;
}

void KnobSweepRecorder::end_record()
{
    if (!m_is_recording.load())
    {
        DBG("KnobSweepRecorder::end_record early return (not recording)");
        return;
    }

    SampleBuffer local_samples;
    {
        const juce::SpinLock::ScopedLockType lock(m_sample_lock);
        local_samples = m_record_samples;
    }

    m_is_recording.store(false);

    if (local_samples.size() < 2)
    {
        DBG("KnobSweepRecorder::end_record early return (insufficient samples)");
        clear();
        return;
    }

    m_loop_duration_ms = juce::jlimit(1.0,
                                      kMaxLoopDurationMs,
                                      local_samples.back().time_ms);
    if (m_loop_duration_ms <= 0.0)
    {
        DBG("KnobSweepRecorder::end_record early return (invalid duration)");
        clear();
        return;
    }

    {
        const juce::SpinLock::ScopedLockType lock(m_sample_lock);
        m_loop_samples = std::move(local_samples);
    }

    m_is_playing.store(true);
    m_playback_start_ms = juce::Time::getMillisecondCounterHiRes();
}

void KnobSweepRecorder::clear()
{
    const juce::SpinLock::ScopedLockType lock(m_sample_lock);
    m_record_samples.clear();
    m_loop_samples.clear();
    m_loop_duration_ms = 0.0;
    m_playback_start_ms = 0.0;
    m_is_armed.store(false);
    m_is_recording.store(false);
    m_is_playing.store(false);
}

float KnobSweepRecorder::get_value(double now_ms) const
{
    float output_value = m_idle_value;
    if (m_is_playing.load() && m_loop_duration_ms > 0.0)
    {
        const double elapsed_ms = juce::jmax(0.0, now_ms - m_playback_start_ms);
        const double wrapped_ms = std::fmod(elapsed_ms, m_loop_duration_ms);
        const double relative_time = (wrapped_ms >= 0.0)
            ? wrapped_ms
            : wrapped_ms + m_loop_duration_ms;
        const juce::SpinLock::ScopedLockType lock(m_sample_lock);
        output_value = playback_value_for_time(relative_time);
    }
    return output_value;
}

float KnobSweepRecorder::playback_value_for_time(double relative_time_ms) const
{
    if (m_loop_samples.empty())
        return m_idle_value;

    if (m_loop_samples.size() == 1)
        return m_loop_samples.front().value;

    for (size_t index = 0; index + 1 < m_loop_samples.size(); ++index)
    {
        const auto& current = m_loop_samples[index];
        const auto& next = m_loop_samples[index + 1];
        if (relative_time_ms <= next.time_ms)
            return interpolate(current, next, relative_time_ms);
    }

    return m_loop_samples.back().value;
}


