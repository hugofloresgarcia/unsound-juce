#pragma once

#include <juce_core/juce_core.h>
#include <atomic>
#include <vector>

namespace flower
{

enum class LfoWaveform
{
    Sine = 0,
    Triangle,
    Square,
    Gate,
    Envelope,
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

    // Clocked mode parameters
    void set_clock_division(float div); // steps per beat (e.g. 4.0 = 16th notes, 0.25 = 1 bar)
    float get_clock_division() const noexcept { return m_clock_division; }

    void set_pattern_length(int length); // 0 = off (infinite/generative), >0 = loop length in steps
    int get_pattern_length() const noexcept { return m_pattern_length; }

    void set_pattern_buffer(const std::vector<float>& buffer);
    const std::vector<float>& get_pattern_buffer() const { return m_pattern_buffer; }

    void set_depth(float depth);
    float get_depth() const noexcept { return m_depth.load(std::memory_order_relaxed); }

    // PNW-style waveform shaping parameters
    void set_level(float level);  // 0-1 output level
    float get_level() const noexcept { return m_level; }

    void set_width(float width);  // 0-1 width/skew (duty cycle for gate, release for env)
    float get_width() const noexcept { return m_width; }

    void set_phase_offset(float phase);  // 0-1 phase offset
    float get_phase_offset() const noexcept { return m_phase_offset; }

    void set_delay(float delay);  // 0-1 delay before waveform starts
    float get_delay() const noexcept { return m_delay; }

    void set_delay_div(int div);  // Delay divisor (every Nth step)
    int get_delay_div() const noexcept { return m_delay_div; }

    // Humanization
    void set_slop(float slop);  // 0-1 timing randomization
    float get_slop() const noexcept { return m_slop; }

    // Euclidean rhythm
    void set_euclidean_steps(int steps);  // 0 = off, else number of steps
    int get_euclidean_steps() const noexcept { return m_euclidean_steps; }

    void set_euclidean_triggers(int triggers);  // Number of hits
    int get_euclidean_triggers() const noexcept { return m_euclidean_triggers; }

    void set_euclidean_rotation(int rotation);  // Pattern rotation
    int get_euclidean_rotation() const noexcept { return m_euclidean_rotation; }

    // Random skip
    void set_random_skip(float skip);  // 0-1 probability of skipping
    float get_random_skip() const noexcept { return m_random_skip; }

    // Loop
    void set_loop_beats(int beats);  // 0 = off, else loop length in beats
    int get_loop_beats() const noexcept { return m_loop_beats; }

    // Polarity: bipolar (-1 to 1) or unipolar (0 to 1)
    void set_bipolar(bool bipolar);
    bool get_bipolar() const noexcept { return m_bipolar; }

    // Random seed for reproducible patterns
    void set_random_seed(uint64_t seed);
    uint64_t get_random_seed() const noexcept { return m_random_seed; }

    void reset_phase(double normalized_phase = 0.0);
    void sync_time(double now_ms);

    float advance(double now_ms);
    float advance_clocked(double master_beats);
    
    float process_delta(double delta_seconds);
    float get_last_value() const noexcept { return m_last_value; }

    // Euclidean pattern check
    bool is_euclidean_hit(int step) const;
    
    // Skip check (combines euclidean and random skip)
    bool should_skip_step(int step);

private:
    float render_wave(float normalized_phase) const noexcept;
    float apply_width_skew(float phase) const noexcept;
    void handle_cycle_wrap();
    void randomize_targets();
    
    // Clocked mode helpers
    void update_clocked_step(int step_index);
    float get_step_random_value(int step_index);
    bool get_step_skip_decision(int step_index);

    LfoWaveform m_mode{LfoWaveform::Sine};
    float m_rate_hz{0.5f};
    
    // Clocked params
    float m_clock_division{1.0f}; // 1.0 = 1 step per beat (quarter note)
    int m_pattern_length{0};
    std::vector<float> m_pattern_buffer;
    std::vector<bool> m_skip_buffer;  // Cached skip decisions
    int m_last_step_index{-1};
    
    std::atomic<float> m_depth{0.5f};
    double m_phase{0.0};
    float m_last_value{0.0f};
    bool m_has_time_reference{false};
    double m_last_time_ms{0.0};
    
    float m_random_hold_value{0.0f};
    float m_random_target_value{0.0f};
    juce::Random m_random;
    uint64_t m_random_seed{0};
    
    // PNW-style parameters
    float m_level{1.0f};
    float m_width{0.5f};
    float m_phase_offset{0.0f};
    float m_delay{0.0f};
    int m_delay_div{1};
    float m_slop{0.0f};
    int m_euclidean_steps{0};
    int m_euclidean_triggers{0};
    int m_euclidean_rotation{0};
    float m_random_skip{0.0f};
    int m_loop_beats{0};
    bool m_bipolar{true};  // true = -1 to 1, false = 0 to 1
    
    // Current step state
    bool m_current_step_skipped{false};
    float m_current_step_slop_offset{0.0f};
};

} // namespace flower
