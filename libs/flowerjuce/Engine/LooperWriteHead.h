#pragma once

#include <juce_core/juce_core.h>
#include "TapeLoop.h"
#include <atomic>

// LooperWriteHead handles recording to a TapeLoop
class LooperWriteHead
{
public:
    LooperWriteHead(TapeLoop& tape_loop);
    ~LooperWriteHead() = default;
    
    // Recording control
    void set_record_enable(bool enable) { m_record_enable.store(enable); }
    bool get_record_enable() const { return m_record_enable.load(); }
    
    // Overdub control
    void set_overdub_mix(float mix) { m_overdub_mix.store(mix); } // 0.0 = all new, 1.0 = all old
    float get_overdub_mix() const { return m_overdub_mix.load(); }
    
    // Process recording for a single sample
    // Returns true if a sample was written
    bool process_sample(float input_sample, float current_position);
    
    // Finalize recording (set recorded_length when recording stops)
    void finalize_recording(float final_position);
    
    // Reset for new recording
    void reset();

    // Get write position
    void set_pos(size_t pos) { m_pos.store(pos); }
    size_t get_pos() const { return m_pos.load(); }

    void set_wrap_pos(size_t wrap_pos) { m_wrap_pos.store(wrap_pos); }
    size_t get_wrap_pos() const { return m_wrap_pos.load(); }
    
    // Set sample rate (call when audio device starts)
    void set_sample_rate(double sample_rate) { m_sample_rate.store(sample_rate); }
    double get_sample_rate() const { return m_sample_rate.load(); }
    
    // Input channel selection (-1 = all channels, 0+ = specific channel)
    void set_input_channel(int channel) { m_input_channel.store(channel); }
    int get_input_channel() const { return m_input_channel.load(); }
    
private:
    // Write position tracking
    std::atomic<size_t> m_pos{0}; // Maximum position written to
    std::atomic<size_t> m_wrap_pos{0}; // Wrap position / end of loop
    
    TapeLoop& m_tape_loop;
    std::atomic<bool> m_record_enable{false}; // Actually recording (m_record_enable && m_is_playing)
    std::atomic<bool> m_is_playing{false};
    std::atomic<float> m_overdub_mix{0.5f};
    std::atomic<double> m_sample_rate{44100.0}; // Current sample rate
    std::atomic<int> m_input_channel{-1}; // -1 = all channels, 0+ = specific channel
};
