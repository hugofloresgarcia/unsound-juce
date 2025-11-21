#pragma once

#include <juce_core/juce_core.h>
#include <atomic>

namespace flower
{

enum class LfoWaveform
{
    Sine = 0,
    Triangle,
    Square,
    Random,
    SmoothRandom
};

class LayerCakeLfoUGen
{
public:
    LayerCakeLfoUGen();
    LayerCakeLfoUGen(const LayerCakeLfoUGen& other);
    LayerCakeLfoUGen& operator=(const LayerCakeLfoUGen& other);

    void set_mode(LfoWaveform mode);
    LfoWaveform get_mode() const noexcept { return m_mode; }

    void set_rate_hz(float rate_hz);
    float get_rate_hz() const noexcept { return m_rate_hz; }

    void set_depth(float depth);
    float get_depth() const noexcept { return m_depth.load(std::memory_order_relaxed); }

    void reset_phase(double normalized_phase = 0.0);
    void sync_time(double now_ms);

    float advance(double now_ms);
    float process_delta(double delta_seconds);
    float get_last_value() const noexcept { return m_last_value; }

private:
    float render_wave(float normalized_phase) const noexcept;
    void handle_cycle_wrap();
    void randomize_targets();

    LfoWaveform m_mode{LfoWaveform::Sine};
    float m_rate_hz{0.5f};
    std::atomic<float> m_depth{0.5f};
    double m_phase{0.0};
    float m_last_value{0.0f};
    bool m_has_time_reference{false};
    double m_last_time_ms{0.0};
    float m_random_hold_value{0.0f};
    float m_random_target_value{0.0f};
    juce::Random m_random;
};

} // namespace flower



