#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "TapeLoop.h"
#include <atomic>

// LooperReadHead handles playback from a TapeLoop
// Multiple read heads can read from the same tape loop simultaneously
class LooperReadHead
{
public:
    LooperReadHead(TapeLoop& tape_loop);
    ~LooperReadHead() = default;
    
    // Playback control
    void set_playing(bool playing) { m_is_playing.store(playing); }
    bool get_playing() const { return m_is_playing.load(); }
    
    void set_muted(bool muted);
    bool get_muted() const { return m_is_muted.load(); }
    
    // Set sample rate (call when audio device starts)
    void set_sample_rate(double sample_rate);
    
    // Reset mute ramp for new sample rate (call when audio device starts)
    void reset_mute_ramp(double sample_rate);
    
    // Playback parameters
    void set_speed(float speed) { m_playback_speed.store(speed); }
    float get_speed() const { return m_playback_speed.load(); }
    
    void set_level_db(float db) { m_level_db.store(db); }
    float get_level_db() const { return m_level_db.load(); }
    
    void set_output_channel(int channel) { m_output_channel.store(channel); } // -1 = all channels
    int get_output_channel() const { return m_output_channel.load(); }
    
    // Playhead position
    std::atomic<float> m_pos{0.0f};
    
    // Level meter (for VU meter display)
    std::atomic<float> m_level_meter{0.0f};
    
    // Process playback for a single sample
    // Returns the output sample value, or 0.0f if not playing/muted
    float process_sample();
    
    // Get raw sample value before level gain and mute (pre-fader)
    // Returns the interpolated sample value without any gain/mute applied
    float get_raw_sample() const;
    
    // Advance playhead (call after process_sample for each sample)
    // Returns true if the playhead wrapped around the tape loop
    bool advance(float wrap_pos);
    
    // Reset playhead to start
    void reset();

    void set_pos(float pos) { m_pos.store(pos); }
    float get_pos() const { return m_pos.load(); }
    
    // Sync playhead to a specific position
    void sync_to(float position);
    
private:
    TapeLoop& m_tape_loop;
    std::atomic<bool> m_is_playing{false};
    std::atomic<bool> m_is_muted{false};
    std::atomic<float> m_playback_speed{1.0f};
    std::atomic<float> m_level_db{0.0f};
    std::atomic<int> m_output_channel{-1}; // -1 = all channels, 0+ = specific channel
    std::atomic<double> m_sample_rate{44100.0}; // Current sample rate
    
    juce::SmoothedValue<float> m_mute_gain{1.0f}; // Smooth mute ramp (10ms)
    
    float interpolate_sample(float position) const;
};
