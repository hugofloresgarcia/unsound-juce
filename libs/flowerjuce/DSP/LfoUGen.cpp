#include "LfoUGen.h"
#include <cmath>
#include <array>

namespace flower
{

namespace
{
constexpr float kMinRateHz = 0.01f;
constexpr float kMaxRateHz = 20.0f;

float sine_wave(float phase) noexcept
{
    return std::sin(juce::MathConstants<float>::twoPi * phase);
}

float triangle_wave(float phase, float width) noexcept
{
    // Width/skew affects the peak position
    // At width=0.5, symmetric triangle
    // At width=1.0, saw up
    // At width=0.0, saw down
    const float peak_pos = juce::jlimit(0.01f, 0.99f, width);
    
    if (phase < peak_pos)
    {
        // Rising portion
        return -1.0f + 2.0f * (phase / peak_pos);
    }
    else
    {
        // Falling portion
        return 1.0f - 2.0f * ((phase - peak_pos) / (1.0f - peak_pos));
    }
}

float sine_wave_skewed(float phase, float width) noexcept
{
    // Skew the sine wave by remapping phase
    const float peak_pos = juce::jlimit(0.01f, 0.99f, width);
    float remapped_phase;
    
    if (phase < peak_pos)
    {
        // First half maps to 0-0.5
        remapped_phase = 0.5f * (phase / peak_pos);
    }
    else
    {
        // Second half maps to 0.5-1.0
        remapped_phase = 0.5f + 0.5f * ((phase - peak_pos) / (1.0f - peak_pos));
    }
    
    return std::sin(juce::MathConstants<float>::twoPi * remapped_phase);
}

float square_wave(float phase, float width) noexcept
{
    // Width controls duty cycle
    return (phase < width) ? 1.0f : -1.0f;
}

float gate_wave(float phase, float width) noexcept
{
    // Gate: 0 or 1 based on duty cycle (width)
    // Output is 0-1 range (unipolar)
    return (phase < width) ? 1.0f : 0.0f;
}

float envelope_wave(float phase, float width) noexcept
{
    // Envelope: instant attack, release controlled by width
    // Width = release time as fraction of step
    // Output is 0-1 range (unipolar)
    const float release_time = juce::jmax(0.01f, width);
    
    if (phase < release_time)
    {
        // In release phase - decay from 1 to 0
        return 1.0f - (phase / release_time);
    }
    return 0.0f;
}

// Scale helper
// Returns the closest semitone in the scale
float snap_to_scale(float semitones, LfoScale scale)
{
    // Scale definitions
    static const std::array<bool, 12> major = {1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1};
    static const std::array<bool, 12> minor = {1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0};
    static const std::array<bool, 12> pent_major = {1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 0};
    static const std::array<bool, 12> pent_minor = {1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0};
    static const std::array<bool, 12> whole_tone = {1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0};
    static const std::array<bool, 12> diminished = {1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0};
    
    const std::array<bool, 12>* scale_notes = nullptr;
    
    switch (scale)
    {
        case LfoScale::Chromatic:
            return std::round(semitones);
            
        case LfoScale::Major: scale_notes = &major; break;
        case LfoScale::Minor: scale_notes = &minor; break;
        case LfoScale::PentatonicMajor: scale_notes = &pent_major; break;
        case LfoScale::PentatonicMinor: scale_notes = &pent_minor; break;
        case LfoScale::WholeTone: scale_notes = &whole_tone; break;
        case LfoScale::Diminished: scale_notes = &diminished; break;
        case LfoScale::Off:
        default:
            return semitones;
    }
    
    // Precise nearest neighbor search in float space
    if (scale_notes)
    {
        int center = static_cast<int>(std::round(semitones));
        float best_val = static_cast<float>(center);
        float min_dist = 1000.0f;
        bool found_any = false;
        
        // Check +/- 6 semitones to guarantee finding a scale tone (max gap is small)
        for (int offset = -6; offset <= 6; ++offset)
        {
            int candidate = center + offset;
            int note_idx = candidate % 12;
            if (note_idx < 0) note_idx += 12;
            
            if ((*scale_notes)[(size_t)note_idx])
            {
                float dist = std::abs(semitones - static_cast<float>(candidate));
                if (!found_any || dist < min_dist)
                {
                    min_dist = dist;
                    best_val = static_cast<float>(candidate);
                    found_any = true;
                }
            }
        }
        return best_val;
    }
    
    return std::round(semitones);
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
        
        m_scale = other.m_scale;
        m_quantize_range = other.m_quantize_range;
        
        m_phase = other.m_phase;
        m_last_value = other.m_last_value;
        m_has_time_reference = other.m_has_time_reference;
        m_last_time_ms = other.m_last_time_ms;
        m_random_hold_value = other.m_random_hold_value;
        m_random_target_value = other.m_random_target_value;
        m_random = other.m_random;
        m_random_seed = other.m_random_seed;
        
        m_clock_division = other.m_clock_division;
        m_pattern_length = other.m_pattern_length;
        m_pattern_buffer = other.m_pattern_buffer;
        m_skip_buffer = other.m_skip_buffer;
        m_last_step_index = other.m_last_step_index;
        
        // PNW parameters
        m_level = other.m_level;
        m_width = other.m_width;
        m_phase_offset = other.m_phase_offset;
        m_delay = other.m_delay;
        m_delay_div = other.m_delay_div;
        m_slop = other.m_slop;
        m_euclidean_steps = other.m_euclidean_steps;
        m_euclidean_triggers = other.m_euclidean_triggers;
        m_euclidean_rotation = other.m_euclidean_rotation;
        m_random_skip = other.m_random_skip;
        m_loop_beats = other.m_loop_beats;
        m_bipolar = other.m_bipolar;
        
        m_current_step_skipped = other.m_current_step_skipped;
        m_current_step_slop_offset = other.m_current_step_slop_offset;
    }
    return *this;
}

void LayerCakeLfoUGen::set_mode(LfoWaveform mode)
{
    m_mode = mode;
}

void LayerCakeLfoUGen::set_rate_hz(float rate_hz)
{
    const float clamped = juce::jlimit(kMinRateHz, kMaxRateHz, rate_hz);
    if (std::abs(clamped - m_rate_hz) <= 1.0e-6f)
        return;
    m_rate_hz = clamped;
}

void LayerCakeLfoUGen::set_scale(LfoScale scale)
{
    m_scale = scale;
}

void LayerCakeLfoUGen::set_quantize_range(float semitones)
{
    m_quantize_range = juce::jmax(0.0f, semitones);
}

void LayerCakeLfoUGen::set_clock_division(float div)
{
    m_clock_division = juce::jmax(0.01f, div);
}

void LayerCakeLfoUGen::set_pattern_length(int length)
{
    m_pattern_length = juce::jmax(0, length);
}

void LayerCakeLfoUGen::set_pattern_buffer(const std::vector<float>& buffer)
{
    m_pattern_buffer = buffer;
}

void LayerCakeLfoUGen::set_level(float level)
{
    m_level = juce::jlimit(0.0f, 1.0f, level);
}

void LayerCakeLfoUGen::set_width(float width)
{
    m_width = juce::jlimit(0.0f, 1.0f, width);
}

void LayerCakeLfoUGen::set_phase_offset(float phase)
{
    m_phase_offset = juce::jlimit(0.0f, 1.0f, phase);
}

void LayerCakeLfoUGen::set_delay(float delay)
{
    m_delay = juce::jlimit(0.0f, 1.0f, delay);
}

void LayerCakeLfoUGen::set_delay_div(int div)
{
    m_delay_div = juce::jmax(1, div);
}

void LayerCakeLfoUGen::set_slop(float slop)
{
    m_slop = juce::jlimit(0.0f, 1.0f, slop);
}

void LayerCakeLfoUGen::set_euclidean_steps(int steps)
{
    m_euclidean_steps = juce::jmax(0, steps);
}

void LayerCakeLfoUGen::set_euclidean_triggers(int triggers)
{
    m_euclidean_triggers = juce::jmax(0, triggers);
}

void LayerCakeLfoUGen::set_euclidean_rotation(int rotation)
{
    m_euclidean_rotation = juce::jmax(0, rotation);
}

void LayerCakeLfoUGen::set_random_skip(float skip)
{
    m_random_skip = juce::jlimit(0.0f, 1.0f, skip);
}

void LayerCakeLfoUGen::set_loop_beats(int beats)
{
    m_loop_beats = juce::jmax(0, beats);
}

void LayerCakeLfoUGen::set_bipolar(bool bipolar)
{
    m_bipolar = bipolar;
}

void LayerCakeLfoUGen::set_random_seed(uint64_t seed)
{
    m_random_seed = seed;
    m_random.setSeed(static_cast<juce::int64>(seed));
    // Clear cached buffers to regenerate with new seed
    m_pattern_buffer.clear();
    m_skip_buffer.clear();
}

void LayerCakeLfoUGen::reset_phase(double normalized_phase)
{
    m_phase = juce::jlimit(0.0, 1.0, normalized_phase);
    float raw_value = render_wave(static_cast<float>(m_phase));
    
    // Apply level
    raw_value *= m_level;
    
    
    // Convert to unipolar if needed (0 to 1 instead of -1 to 1)
    if (!m_bipolar)
    raw_value = raw_value * 0.5f + 0.5f;

    // Apply quantization (if enabled)
    if (m_scale != LfoScale::Off)
    {
        raw_value = apply_quantization(raw_value);
    }

    m_last_value = raw_value;
    
    // Reset step tracking
    m_last_step_index = -1;
    m_current_step_skipped = false;
    m_current_step_slop_offset = 0.0f;
    
    if (m_mode == LfoWaveform::Random || m_mode == LfoWaveform::SmoothRandom)
    {
        update_clocked_step(0);
        m_last_step_index = 0;
    }
}

void LayerCakeLfoUGen::sync_time(double now_ms)
{
    m_last_time_ms = now_ms;
    m_has_time_reference = true;
}

float LayerCakeLfoUGen::advance(double now_ms)
{
    // Free-running mode based on time
    if (!m_has_time_reference)
    {
        sync_time(now_ms);
        return m_last_value;
    }

    const double delta_seconds = juce::jmax(0.0, (now_ms - m_last_time_ms) * 0.001);
    m_last_time_ms = now_ms;
    return process_delta(delta_seconds);
}

float LayerCakeLfoUGen::advance_clocked(double master_beats)
{
    // Apply loop if set
    double effective_beats = master_beats;
    if (m_loop_beats > 0)
    {
        effective_beats = std::fmod(master_beats, static_cast<double>(m_loop_beats));
    }
    
    // Calculate step position
    const double total_steps = effective_beats * m_clock_division;
    const int current_step = static_cast<int>(std::floor(total_steps));
    double phase_in_step = total_steps - static_cast<double>(current_step);
    
    // Handle step change
    if (current_step != m_last_step_index)
    {
        update_clocked_step(current_step);
        m_last_step_index = current_step;
        
        // Generate slop offset for this step
        if (m_slop > 0.0f)
        {
            m_current_step_slop_offset = (m_random.nextFloat() - 0.5f) * 2.0f * m_slop * 0.2f;
        }
        else
        {
            m_current_step_slop_offset = 0.0f;
        }
        
        // Check if this step should be skipped
        m_current_step_skipped = should_skip_step(current_step);
    }
    
    // If step is skipped, hold the last value (sample and hold behavior)
    if (m_current_step_skipped)
    {
        return m_last_value;
    }
    
    // Apply delay (only on certain steps based on delay_div)
    if (m_delay > 0.0f && m_delay_div > 0)
    {
        if ((current_step % m_delay_div) == 0)
        {
            // This step has delay applied
            if (phase_in_step < m_delay)
            {
                m_last_value = 0.0f;
                return m_last_value;
            }
            // Adjust phase to account for delay
            phase_in_step = (phase_in_step - m_delay) / (1.0f - m_delay);
        }
    }
    
    // Apply slop offset to phase
    phase_in_step += m_current_step_slop_offset;
    phase_in_step = juce::jlimit(0.0, 1.0, phase_in_step);
    
    // Apply phase offset
    double adjusted_phase = phase_in_step + m_phase_offset;
    if (adjusted_phase >= 1.0)
        adjusted_phase -= 1.0;
    
    m_phase = adjusted_phase;
    
    float raw_value = render_wave(static_cast<float>(m_phase));
    
    // Apply level
    raw_value *= m_level;
    
    // Convert to unipolar if needed (0 to 1 instead of -1 to 1)
    if (!m_bipolar)
        raw_value = raw_value * 0.5f + 0.5f;

    // Apply quantization
    if (m_scale != LfoScale::Off)
    {
        raw_value = apply_quantization(raw_value);
    }
    
    
    m_last_value = raw_value;
    return m_last_value;
}

float LayerCakeLfoUGen::process_delta(double delta_seconds)
{
    if (delta_seconds <= 0.0) return m_last_value;
    if (m_rate_hz <= 0.0f) return m_last_value;

    double phase_increment = static_cast<double>(m_rate_hz) * delta_seconds;
    if (phase_increment >= 4.0) phase_increment = std::fmod(phase_increment, 1.0);

    m_phase += phase_increment;
    while (m_phase >= 1.0)
    {
        m_phase -= 1.0;
        handle_cycle_wrap();
    }

    float raw_value = render_wave(static_cast<float>(m_phase));
    
    // Apply level in free-running mode too
    raw_value *= m_level;
    
    // Convert to unipolar if needed (0 to 1 instead of -1 to 1)
    if (!m_bipolar)
        raw_value = raw_value * 0.5f + 0.5f;

    // Apply quantization
    if (m_scale != LfoScale::Off)
    {
        raw_value = apply_quantization(raw_value);
    }
    
    m_last_value = raw_value;
    return m_last_value;
}

float LayerCakeLfoUGen::apply_quantization(float raw_value) const noexcept
{    
    float semitones = raw_value * m_quantize_range; 
    
    float quantized_semitones = snap_to_scale(semitones, m_scale);
    
    // Convert back
    // If semitones was 24, we want 1.0 back.
    // Avoid division by zero
    if (m_quantize_range < 0.001f) return raw_value;
    
    // show the raw value and the quantized value
    // DBG("raw_value: " + juce::String(raw_value) + " quantized_semitones: " + juce::String(quantized_semitones));
    
    return quantized_semitones / m_quantize_range;
}

float LayerCakeLfoUGen::render_wave(float normalized_phase) const noexcept
{
    switch (m_mode)
    {
        case LfoWaveform::Triangle:
            return triangle_wave(normalized_phase, m_width);
            
        case LfoWaveform::Square:
            return square_wave(normalized_phase, m_width);
            
        case LfoWaveform::Gate:
            // Gate outputs 0-1, we map to -1 to 1 for consistency with other waveforms
            // Actually, for modulation, 0-1 might be more useful. Let's keep it bipolar.
            return gate_wave(normalized_phase, m_width) * 2.0f - 1.0f;
            
        case LfoWaveform::Envelope:
            // Envelope outputs 0-1, map to bipolar
            return envelope_wave(normalized_phase, m_width) * 2.0f - 1.0f;
            
        case LfoWaveform::Random:
            return m_random_hold_value;
            
        case LfoWaveform::SmoothRandom:
            return juce::jmap(normalized_phase, 0.0f, 1.0f, m_random_hold_value, m_random_target_value);
            
        case LfoWaveform::Sine:
        default:
            return sine_wave_skewed(normalized_phase, m_width);
    }
}

void LayerCakeLfoUGen::handle_cycle_wrap()
{
    if (m_mode == LfoWaveform::Random || m_mode == LfoWaveform::SmoothRandom)
    {
        if (m_last_step_index < 0) m_last_step_index = 0;
        m_last_step_index++;
        update_clocked_step(m_last_step_index);
    }
}

void LayerCakeLfoUGen::randomize_targets()
{
    m_random_hold_value = juce::jmap(m_random.nextFloat(), -1.0f, 1.0f);
    m_random_target_value = juce::jmap(m_random.nextFloat(), -1.0f, 1.0f);
}

void LayerCakeLfoUGen::update_clocked_step(int step_index)
{
    m_random_hold_value = get_step_random_value(step_index);
    m_random_target_value = get_step_random_value(step_index + 1);
}

float LayerCakeLfoUGen::get_step_random_value(int step_index)
{
    if (step_index < 0) return 0.0f;

    int effective_index = step_index;
    
    // If looping pattern
    if (m_pattern_length > 0)
    {
        effective_index = step_index % m_pattern_length;
    }
    
    // Extend buffer if needed
    if (effective_index >= static_cast<int>(m_pattern_buffer.size()))
    {
        for (int i = static_cast<int>(m_pattern_buffer.size()); i <= effective_index; ++i)
        {
            m_pattern_buffer.push_back(juce::jmap(m_random.nextFloat(), -1.0f, 1.0f));
        }
    }
    
    return m_pattern_buffer[static_cast<size_t>(effective_index)];
}

bool LayerCakeLfoUGen::get_step_skip_decision(int step_index)
{
    if (step_index < 0) return false;
    
    int effective_index = step_index;
    
    // If looping pattern
    if (m_pattern_length > 0)
    {
        effective_index = step_index % m_pattern_length;
    }
    
    // Extend skip buffer if needed
    if (effective_index >= static_cast<int>(m_skip_buffer.size()))
    {
        for (int i = static_cast<int>(m_skip_buffer.size()); i <= effective_index; ++i)
        {
            bool skip = m_random.nextFloat() < m_random_skip;
            m_skip_buffer.push_back(skip);
        }
    }
    
    return m_skip_buffer[static_cast<size_t>(effective_index)];
}

bool LayerCakeLfoUGen::is_euclidean_hit(int step) const
{
    if (m_euclidean_steps <= 0 || m_euclidean_triggers <= 0)
        return true; // No euclidean = all hits
    
    if (m_euclidean_triggers >= m_euclidean_steps)
        return true; // All steps are hits
    
    // Apply rotation
    int rotated_step = (step + m_euclidean_rotation) % m_euclidean_steps;
    
    // Bjorklund/Euclidean algorithm check
    // This is an efficient way to check if a step is a hit without generating the full pattern
    return ((m_euclidean_triggers * rotated_step) % m_euclidean_steps) < m_euclidean_triggers;
}

bool LayerCakeLfoUGen::should_skip_step(int step)
{
    // First check Euclidean pattern
    if (!is_euclidean_hit(step))
        return true;
    
    // Then check random skip (cached for pattern looping)
    if (m_random_skip > 0.0f)
    {
        return get_step_skip_decision(step);
    }
    
    return false;
}

} // namespace flower
