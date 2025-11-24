#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include "TapeLoop.h"
#include "LooperWriteHead.h"
#include "LooperReadHead.h"
#include "OutputBus.h"
#include <flowerjuce/Panners/Panner.h>
#include <flowerjuce/DSP/LowPassFilter.h>
#include <flowerjuce/DSP/PeakMeter.h>
#include <atomic>
#include <functional>

// LooperTrackEngine handles processing for a single looper track
class LooperTrackEngine
{
public:
    LooperTrackEngine();
    ~LooperTrackEngine() = default;

    // Initialize the track with sample rate and buffer duration
    void initialize(double sample_rate, double max_buffer_duration_seconds);

    // Process a block of audio samples for this track
    // Returns true if recording was finalized during this block
    bool process_block(const float* const* input_channel_data,
                     int num_input_channels,
                     float* const* output_channel_data,
                     int num_output_channels,
                     int num_samples,
                     bool should_debug = false);

    // Handle audio device starting (update sample rate)
    void audio_device_about_to_start(double sample_rate);

    // Handle audio device stopping
    void audio_device_stopped();

    // Reset playhead to start (resets both read and write heads)
    void reset();

    // Load audio file into the loop
    // Returns true if successful, false otherwise
    bool load_from_file(const juce::File& audio_file);

    // Set callback for audio samples (for onset detection, etc.)
    void set_audio_sample_callback(std::function<void(float)> callback) { m_audio_sample_callback = callback; }
    
    // Set panner for spatial audio distribution
    void set_panner(Panner* panner) { m_track_state.m_panner = panner; }
    
    // Set low pass filter cutoff frequency (in Hz)
    void set_filter_cutoff(float cutoff_hz);
    
    // Get mono output level (for visualization)
    float get_mono_output_level() const { return m_peak_meter.get_peak(); }
    
    // Read head access methods
    void set_speed(float speed) { m_track_state.m_read_head.set_speed(speed); }
    float get_speed() const { return m_track_state.m_read_head.get_speed(); }
    void set_level_db(float db) { m_track_state.m_read_head.set_level_db(db); }
    float get_level_db() const { return m_track_state.m_read_head.get_level_db(); }
    void set_muted(bool muted) { m_track_state.m_read_head.set_muted(muted); }
    bool get_muted() const { return m_track_state.m_read_head.get_muted(); }
    void set_playing(bool playing) { m_track_state.m_read_head.set_playing(playing); m_track_state.m_is_playing.store(playing); }
    bool get_playing() const { return m_track_state.m_read_head.get_playing(); }
    void set_pos(float pos) { m_track_state.m_read_head.set_pos(pos); }
    float get_pos() const { return m_track_state.m_read_head.get_pos(); }
    void set_loop_start(float loop_start) { m_track_state.m_read_head.set_loop_start(loop_start); }
    float get_loop_start() const { return m_track_state.m_read_head.get_loop_start(); }
    
    // Loop end - managed centrally by LooperTrackEngine, synchronized for both read and write heads
    void set_loop_end(size_t loop_end);
    size_t get_loop_end() const { return m_track_state.m_write_head.get_loop_end(); }
    
    // Write head access methods
    void set_record_enable(bool enable) { m_track_state.m_write_head.set_record_enable(enable); }
    bool get_record_enable() const { return m_track_state.m_write_head.get_record_enable(); }
    double get_sample_rate() const { return m_track_state.m_write_head.get_sample_rate(); }
    void set_input_channel(int channel) { m_track_state.m_write_head.set_input_channel(channel); }
    int get_input_channel() const { return m_track_state.m_write_head.get_input_channel(); }
    void set_overdub_mix(float mix) { m_track_state.m_write_head.set_overdub_mix(mix); }
    float get_overdub_mix() const { return m_track_state.m_write_head.get_overdub_mix(); }
    void finalize_recording(size_t pos) { m_track_state.m_write_head.finalize_recording(pos); set_loop_end(pos); }
    size_t get_write_pos() const { return m_track_state.m_write_head.get_pos(); }
    void set_write_pos(size_t pos) { m_track_state.m_write_head.set_pos(pos); }
    
    // TapeLoop access methods
    bool has_recorded() const { return m_track_state.m_tape_loop.m_has_recorded.load(); }
    size_t get_recorded_length() const { return m_track_state.m_tape_loop.m_recorded_length.load(); }
    void clear_buffer() { const juce::ScopedLock sl(m_track_state.m_tape_loop.m_lock); m_track_state.m_tape_loop.clear_buffer(); }
    juce::CriticalSection& get_buffer_lock() { return m_track_state.m_tape_loop.m_lock; }
    const std::vector<float>& get_buffer() const { return m_track_state.m_tape_loop.get_buffer(); }
    std::vector<float>& get_buffer() { return m_track_state.m_tape_loop.get_buffer(); }
    size_t get_buffer_size() const { return m_track_state.m_tape_loop.get_buffer_size(); }
    void set_recorded_length(size_t length) { m_track_state.m_tape_loop.m_recorded_length.store(length); }
    void set_has_recorded(bool has_recorded) { m_track_state.m_tape_loop.m_has_recorded.store(has_recorded); }

protected:
    // TrackState struct - protected so derived classes can use it
    struct TrackState
    {
        TapeLoop m_tape_loop;
        LooperWriteHead m_write_head;
        LooperReadHead m_read_head;
        OutputBus m_output_bus;
        Panner* m_panner{nullptr}; // Panner for spatial audio distribution
        
        // UI state (these could eventually be moved to the UI layer)
        std::atomic<bool> m_is_playing{false};
        
        TrackState() : m_write_head(m_tape_loop), m_read_head(m_tape_loop) {}
        
        // Non-copyable
        TrackState(const TrackState&) = delete;
        TrackState& operator=(const TrackState&) = delete;
    };
    
    // Helper methods factored out for reuse by VampNetTrackEngine
    void process_recording(TrackState& track, const float* const* input_channel_data, 
                         int num_input_channels, float current_position, int sample, bool is_first_call);
    float process_playback(TrackState& track, bool& wrapped, bool is_first_call);
    bool finalize_recording_if_needed(TrackState& track, bool was_recording, bool is_playing, 
                                   bool has_existing_audio, bool& recording_finalized);

private:
    TrackState m_track_state;
    bool m_was_recording{false};
    bool m_was_playing{false};
    double m_max_buffer_duration_seconds{10.0};
    
    juce::AudioFormatManager m_format_manager;
    std::function<void(float)> m_audio_sample_callback; // Callback for audio samples (for onset detection)
    
    // Low pass filter UGen
    LowPassFilter m_low_pass_filter;
    
    // Peak meter UGen
    PeakMeter m_peak_meter;
};

