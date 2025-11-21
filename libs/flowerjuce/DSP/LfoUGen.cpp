#include "LfoUGen.h"
#include <cmath>

namespace flower
{

namespace
{
constexpr float kMinDepth = 0.0f;
constexpr float kMaxDepth = 1.0f;
constexpr float kMinRateHz = 0.01f;
constexpr float kMaxRateHz = 20.0f;

float sine_wave(float phase) noexcept
{
    return std::sin(juce::MathConstants<float>::twoPi * phase);
}

float triangle_wave(float phase) noexcept
{
    const float wrapped = juce::jlimit(0.0f, 1.0f, static_cast<float>(phase));
    return 1.0f - 4.0f * std::abs(wrapped - 0.5f);
}

float square_wave(float phase) noexcept
{
    return (phase < 0.5f) ? 1.0f : -1.0f;
}
} // namespace

LayerCakeLfoUGen::LayerCakeLfoUGen()
{
    randomize_targets();
}

LayerCakeLfoUGen::LayerCakeLfoUGen(const LayerCakeLfoUGen& other)
{
    *this = other;
}

LayerCakeLfoUGen& LayerCakeLfoUGen::operator=(const LayerCakeLfoUGen& other)
{
    if (this != &other)
    {
        m_mode = other.m_mode;
        m_rate_hz = other.m_rate_hz;
        m_depth.store(other.m_depth.load(std::memory_order_relaxed), std::memory_order_relaxed);
        m_phase = other.m_phase;
        m_last_value = other.m_last_value;
        m_has_time_reference = other.m_has_time_reference;
        m_last_time_ms = other.m_last_time_ms;
        m_random_hold_value = other.m_random_hold_value;
        m_random_target_value = other.m_random_target_value;
        m_random = other.m_random;
    }
    return *this;
}

void LayerCakeLfoUGen::set_mode(LfoWaveform mode)
{
    m_mode = mode;
    randomize_targets();
}

void LayerCakeLfoUGen::set_rate_hz(float rate_hz)
{
    const float clamped = juce::jlimit(kMinRateHz, kMaxRateHz, rate_hz);
    if (std::abs(clamped - m_rate_hz) <= 1.0e-6f)
        return;
    m_rate_hz = clamped;
    DBG("LayerCakeLfoUGen::set_rate_hz rate=" + juce::String(clamped, 3));
}

void LayerCakeLfoUGen::set_depth(float depth)
{
    const float clamped = juce::jlimit(kMinDepth, kMaxDepth, depth);
    m_depth.store(clamped, std::memory_order_relaxed);
    DBG("LayerCakeLfoUGen::set_depth depth=" + juce::String(clamped, 3));
}

void LayerCakeLfoUGen::reset_phase(double normalized_phase)
{
    m_phase = juce::jlimit(0.0, 1.0, normalized_phase);
    m_last_value = render_wave(static_cast<float>(m_phase));
    DBG("LayerCakeLfoUGen::reset_phase phase=" + juce::String(m_phase, 4));
}

void LayerCakeLfoUGen::sync_time(double now_ms)
{
    m_last_time_ms = now_ms;
    m_has_time_reference = true;
}

float LayerCakeLfoUGen::advance(double now_ms)
{
    if (!m_has_time_reference)
    {
        sync_time(now_ms);
        DBG("LayerCakeLfoUGen::advance initialized time reference");
        return m_last_value;
    }

    const double delta_seconds = juce::jmax(0.0, (now_ms - m_last_time_ms) * 0.001);
    m_last_time_ms = now_ms;
    return process_delta(delta_seconds);
}

float LayerCakeLfoUGen::process_delta(double delta_seconds)
{
    if (delta_seconds <= 0.0)
    {
        DBG("LayerCakeLfoUGen::process_delta early return (non-positive delta)");
        return m_last_value;
    }

    if (m_rate_hz <= 0.0f)
    {
        DBG("LayerCakeLfoUGen::process_delta early return (rate <= 0)");
        return m_last_value;
    }

    double phase_increment = static_cast<double>(m_rate_hz) * delta_seconds;
    if (phase_increment >= 4.0) // guard unrealistic jumps
        phase_increment = std::fmod(phase_increment, 1.0);

    m_phase += phase_increment;
    while (m_phase >= 1.0)
    {
        m_phase -= 1.0;
        handle_cycle_wrap();
    }

    const float raw_value = render_wave(static_cast<float>(m_phase));
    m_last_value = raw_value;
    return m_last_value;
}

float LayerCakeLfoUGen::render_wave(float normalized_phase) const noexcept
{
    switch (m_mode)
    {
        case LfoWaveform::Triangle:
            return triangle_wave(normalized_phase);
        case LfoWaveform::Square:
            return square_wave(normalized_phase);
        case LfoWaveform::Random:
            return m_random_hold_value;
        case LfoWaveform::SmoothRandom:
            return juce::jmap(normalized_phase, 0.0f, 1.0f, m_random_hold_value, m_random_target_value);
        case LfoWaveform::Sine:
        default:
            return sine_wave(normalized_phase);
    }
}

void LayerCakeLfoUGen::handle_cycle_wrap()
{
    if (m_mode == LfoWaveform::Random || m_mode == LfoWaveform::SmoothRandom)
    {
        m_random_hold_value = m_random_target_value;
        m_random_target_value = juce::jmap(m_random.nextFloat(), -1.0f, 1.0f);
    }
}

void LayerCakeLfoUGen::randomize_targets()
{
    m_random_hold_value = juce::jmap(m_random.nextFloat(), -1.0f, 1.0f);
    m_random_target_value = juce::jmap(m_random.nextFloat(), -1.0f, 1.0f);
}

} // namespace flower



