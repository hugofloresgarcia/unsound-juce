#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include "TapeLoop.h"
#include "LooperWriteHead.h"
#include "LooperReadHead.h"
#include "OutputBus.h"
#include <flowerjuce/Panners/Panner.h>
#include <atomic>
#include <functional>

// LooperTrackEngine handles processing for a single looper track
class LooperTrackEngine
{
public:
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

    // Reset playhead to start
    void reset();

    // Load audio file into the loop
    // Returns true if successful, false otherwise
    bool load_from_file(const juce::File& audio_file);

    // Access to track state
    TrackState& get_track_state() { return m_track_state; }
    const TrackState& get_track_state() const { return m_track_state; }
    
    // Set callback for audio samples (for onset detection, etc.)
    void set_audio_sample_callback(std::function<void(float)> callback) { m_audio_sample_callback = callback; }
    
    // Set panner for spatial audio distribution
    void set_panner(Panner* panner) { m_track_state.m_panner = panner; }
    
    // Set low pass filter cutoff frequency (in Hz)
    void set_filter_cutoff(float cutoff_hz);
    
    // Get mono output level (for visualization)
    float getMonoOutputLevel() const { return m_mono_output_level.load(); }

protected:
    // Helper methods factored out for reuse by VampNetTrackEngine
    void process_recording(TrackState& track, const float* const* input_channel_data, 
                         int num_input_channels, float current_position, int sample, bool is_first_call);
    float process_playback(TrackState& track, bool is_first_call);
    bool finalize_recording_if_needed(TrackState& track, bool was_recording, bool is_playing, 
                                   bool has_existing_audio, bool& recording_finalized);

private:
    TrackState m_track_state;
    bool m_was_recording{false};
    bool m_was_playing{false};
    double m_max_buffer_duration_seconds{10.0};
    
    juce::AudioFormatManager m_format_manager;
    std::function<void(float)> m_audio_sample_callback; // Callback for audio samples (for onset detection)
    
    // Mono output level tracking (for visualization)
    std::atomic<float> m_mono_output_level{0.0f};
    
    // Low pass filter
    juce::dsp::IIR::Filter<float> m_low_pass_filter;
    juce::dsp::IIR::Coefficients<float>::Ptr m_filter_coefficients;
    std::atomic<float> m_filter_cutoff{20000.0f}; // Default to 20kHz (no filtering)
    double m_current_sample_rate{44100.0};
};

