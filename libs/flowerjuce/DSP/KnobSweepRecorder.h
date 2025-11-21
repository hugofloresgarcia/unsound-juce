#pragma once

#include <juce_core/juce_core.h>
#include <atomic>
#include <vector>

// KnobSweepRecorder UGen - captures a knob gesture and replays it as a loop
class KnobSweepRecorder
{
public:
    struct SamplePoint
    {
        double time_ms{0.0};
        float value{0.0f};
    };

    using SampleBuffer = std::vector<SamplePoint>;

    KnobSweepRecorder();
    ~KnobSweepRecorder() = default;

    void prepare(double sample_rate_hz);
    void set_idle_value(float value);

    void arm();
    bool is_armed() const noexcept { return m_is_armed.load(); }
    bool is_recording() const noexcept { return m_is_recording.load(); }
    bool is_playing() const noexcept { return m_is_playing.load(); }

    void begin_record(double now_ms);
    void push_sample(double now_ms, float value);
    void end_record();

    void clear();

    float get_value(double now_ms) const;
    double get_loop_duration_ms() const noexcept { return m_loop_duration_ms; }

private:
    float playback_value_for_time(double relative_time_ms) const;

    double m_sample_rate{44100.0};
    double m_record_start_ms{0.0};
    double m_playback_start_ms{0.0};
    double m_loop_duration_ms{0.0};
    float m_idle_value{0.0f};
    float m_last_recorded_value{0.0f};

    SampleBuffer m_record_samples;
    SampleBuffer m_loop_samples;

    mutable juce::SpinLock m_sample_lock;
    std::atomic<bool> m_is_armed{false};
    std::atomic<bool> m_is_recording{false};
    std::atomic<bool> m_is_playing{false};
};


