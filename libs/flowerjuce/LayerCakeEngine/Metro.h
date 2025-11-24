#pragma once

#include <atomic>
#include <cstdint>

// Metro is a lightweight metronome UGen that tracks elapsed samples and raises
// a tick flag whenever the configured period elapses.
class Metro
{
public:
    Metro() = default;

    void prepare(double sample_rate);
    void set_period_ms(float period_ms);
    float get_period_ms() const { return m_period_ms; }

    void reset();
    void process_sample();
    bool consume_tick();

    static float bpm_to_period_ms(float bpm);
    static float period_ms_to_bpm(float period_ms);

private:
    void update_samples_per_period();

    double m_sample_rate{44100.0};
    float m_period_ms{500.0f};
    uint64_t m_samples_per_period{22050};
    uint64_t m_sample_counter{0};
    std::atomic<bool> m_tick_ready{false};
};


