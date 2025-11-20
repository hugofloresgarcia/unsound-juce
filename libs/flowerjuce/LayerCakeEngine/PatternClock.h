#pragma once

#include "LayerCakeTypes.h"
#include "Metro.h"
#include <array>
#include <atomic>
#include <juce_core/juce_core.h>

class LayerCakeEngine;

struct PatternSnapshot
{
    int pattern_length{16};
    float skip_probability{0.0f};
    float period_ms{500.0f};
    bool enabled{false};
    std::array<GrainState, 128> steps{};
};

class PatternClock
{
public:
    enum class Mode
    {
        Idle,
        Recording,
        Playback
    };

    explicit PatternClock(LayerCakeEngine& engine);

    void prepare(double sample_rate);
    void set_enabled(bool enabled);
    bool is_enabled() const { return m_enabled.load(); }

    void set_pattern_length(int length);
    int get_pattern_length() const { return m_pattern_length; }

    void set_skip_probability(float probability);
    float get_skip_probability() const { return m_skip_probability; }

    void set_period_ms(float period_ms);
    float get_period_ms() const { return m_metro.get_period_ms(); }
    void set_bpm(float bpm);
    float get_bpm() const;

    void reset();
    void process_sample();

    void capture_step_grain(const GrainState& state);
    Mode get_mode() const { return m_mode; }
    void get_snapshot(PatternSnapshot& snapshot) const;
    void apply_snapshot(const PatternSnapshot& snapshot);
    void set_auto_fire_enabled(bool enabled);
    void set_auto_fire_state(const GrainState& state);

private:
    void advance_step();
    void handle_record_step();
    void handle_playback_step();
    void trigger_step_state(const GrainState& state);
    void clear_pattern();
    bool should_skip_step();
    void prime_pending_state();

    LayerCakeEngine& m_engine;
    Metro m_metro;
    std::array<GrainState, 128> m_pattern_steps;
    GrainState m_pending_record_state;
    std::atomic<bool> m_enabled{false};
    Mode m_mode{Mode::Idle};
    int m_pattern_length{16};
    int m_current_step{0};
    int m_recorded_steps{0};
    float m_skip_probability{0.0f};
    juce::Random m_random;
    std::atomic<bool> m_auto_fire_enabled{true};
    GrainState m_auto_fire_state;
    juce::SpinLock m_auto_state_lock;
};


