#include "LooperReadHead.h"
#include <flowerjuce/Debug/DebugAudioRate.h>
#include <cmath>

// TODO: Remove this debug macro after fixing segmentation fault
#define DEBUG_SEGFAULT 1
#if DEBUG_SEGFAULT
#define DBG_SEGFAULT(msg) juce::Logger::writeToLog("[SEGFAULT] " + juce::String(__FILE__) + ":" + juce::String(__LINE__) + " - " + juce::String(msg))
#else
#define DBG_SEGFAULT(msg)
#endif

LooperReadHead::LooperReadHead(TapeLoop& tape_loop)
    : m_tape_loop(tape_loop)
{
    // Initialize mute ramp to 10ms at default sample rate (will be reset when device starts)
    double default_sample_rate = m_sample_rate.load();
    m_mute_gain.reset(default_sample_rate, 0.01); // 10ms ramp
    m_mute_gain.setCurrentAndTargetValue(1.0f); // Start unmuted
}

float LooperReadHead::get_raw_sample() const
{
    // Return raw interpolated sample without any gain/mute applied (pre-fader)
    return interpolate_sample(m_pos.load());
}

void LooperReadHead::prepare(double sample_rate)
{
    m_sample_rate.store(sample_rate);
    reset_mute_ramp(sample_rate);
}

float LooperReadHead::process_sample(bool& wrapped)
{   
    DBG_AUDIO_RATE(2000, {DBG_SEGFAULT("ENTRY: LooperReadHead::process_sample");});

    // Interpolate sample at current position
    DBG_AUDIO_RATE(2000, {DBG_SEGFAULT("Calling interpolate_sample, pos=" + juce::String(m_pos.load()));});
    float sample_value = interpolate_sample(m_pos.load());
    DBG_AUDIO_RATE(2000, {DBG_SEGFAULT("interpolate_sample returned, value=" + juce::String(sample_value));});
    
    // Apply level gain (convert dB to linear)
    float gain = juce::Decibels::decibelsToGain(m_level_db.load());
    sample_value *= gain;
    
    // Apply mute ramp (smooth transition to avoid clicks)
    sample_value *= m_mute_gain.getNextValue();
    
    // Track level for VU meter (peak detection with decay)
    float abs_value = std::abs(sample_value);
    float current_level = m_level_meter.load();
    if (abs_value > current_level)
        m_level_meter.store(abs_value);
    else
        m_level_meter.store(current_level * 0.999f); // Decay
    
    // Advance playhead and check for wrap
    wrapped = advance_playhead();
    
    DBG_AUDIO_RATE(2000, {DBG_SEGFAULT("EXIT: LooperReadHead::process_sample");});
    return sample_value;
}

bool LooperReadHead::advance_playhead()
{   
    float loop_start = m_loop_start.load();
    float loop_end = m_loop_end.load();
    
    // Safety check: if loop_end is 0 or invalid, don't advance
    if (loop_end <= loop_start)
    {
        DBG("WARNING: Loop end is invalid in advance_playhead");
        return false;
    }
    
    float current_pos = m_pos.load();
    float speed = m_playback_speed.load();
    float direction = m_direction_fwd.load() ? 1.0f : -1.0f;
    current_pos += speed * direction;

    // Check if we'll wrap around the tape loop
    float loop_len = loop_end - loop_start;
    bool wrapped = false;
    if (current_pos < loop_start)
    {
        current_pos = current_pos + loop_len;
        wrapped = true;
    }
    else if (current_pos >= loop_end)
    {
        current_pos = current_pos - loop_len;
        wrapped = true;
    }
    
    m_pos.store(current_pos);
    return wrapped;
}

void LooperReadHead::set_muted(bool muted)
{
    m_is_muted.store(muted);
    // Set target value for smooth mute ramp (0.0 = muted, 1.0 = unmuted)
    m_mute_gain.setTargetValue(muted ? 0.0f : 1.0f);
}

void LooperReadHead::set_sample_rate(double sample_rate)
{
    prepare(sample_rate);
}

void LooperReadHead::reset_mute_ramp(double sample_rate)
{
    // Reset mute ramp for new sample rate (10ms ramp)
    m_mute_gain.reset(sample_rate, 0.01); // 10ms ramp
    // Set current and target to match current mute state
    bool currently_muted = m_is_muted.load();
    m_mute_gain.setCurrentAndTargetValue(currently_muted ? 0.0f : 1.0f);
}

void LooperReadHead::reset()
{
    m_pos.store(0.0f);
}

void LooperReadHead::sync_to(float position)
{
    m_pos.store(position);
}

float LooperReadHead::interpolate_sample(float position) const
{
    DBG_AUDIO_RATE(2000, {DBG_SEGFAULT("ENTRY: LooperReadHead::interpolate_sample, position=" + juce::String(position));});
    
    DBG_AUDIO_RATE(2000, {DBG_SEGFAULT("Getting buffer reference");});
    const auto& buffer = m_tape_loop.get_buffer();
    
    DBG_AUDIO_RATE(2000, {DBG_SEGFAULT("Buffer size=" + juce::String(buffer.size()));});
    
    // Safety check: if buffer is empty, return silence
    if (buffer.empty()){
        juce::Logger::writeToLog("WARNING: Buffer is empty in interpolate_sample");
        DBG_AUDIO_RATE(2000, {DBG_SEGFAULT("Buffer is empty, returning 0.0f");});
        return 0.0f;
    }
    
    DBG_AUDIO_RATE(2000, {DBG_SEGFAULT("Calculating indices");});
    size_t index0 = static_cast<size_t>(position) % buffer.size();
    size_t index1 = (index0 + 1) % buffer.size();
    float fraction = position - std::floor(position);
    
    DBG_AUDIO_RATE(2000, {DBG_SEGFAULT("Accessing buffer[" + juce::String(index0) + "] and buffer[" + juce::String(index1) + "]");});
    float result = buffer[index0] * (1.0f - fraction) + buffer[index1] * fraction;
    
    DBG_AUDIO_RATE(2000, {DBG_SEGFAULT("EXIT: LooperReadHead::interpolate_sample, result=" + juce::String(result));});
    return result;  
}
