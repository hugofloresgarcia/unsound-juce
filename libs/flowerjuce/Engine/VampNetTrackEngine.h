#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "TapeLoop.h"
#include "LooperWriteHead.h"
#include "LooperReadHead.h"
#include "OutputBus.h"
#include <flowerjuce/Panners/Panner.h>
#include <atomic>

// Forward declarations
namespace VampNet { class ClickSynth; class Sampler; }

// VampNetTrackEngine handles processing for a single VampNet track with dual buffers
// Uses two LooperReadHead instances (one per buffer) and one LooperWriteHead
class VampNetTrackEngine
{
public:
    struct TrackState
    {
        TapeLoop m_record_buffer;  // Records input audio
        TapeLoop m_output_buffer;  // Stores generated outputs
        LooperWriteHead m_write_head;  // writes to recordBuffer
        LooperReadHead m_record_read_head;  // reads from recordBuffer
        LooperReadHead m_output_read_head;  // reads from outputBuffer
        OutputBus m_output_bus;
        Panner* m_panner{nullptr}; // Panner for spatial audio distribution
        
        // UI state (these could eventually be moved to the UI layer)
        std::atomic<bool> m_is_playing{false};
        std::atomic<float> m_dry_wet_mix{0.5f};  // 0.0 = all dry (record buffer), 1.0 = all wet (output buffer)
        
        TrackState() : m_write_head(m_record_buffer), m_record_read_head(m_record_buffer), m_output_read_head(m_output_buffer) {}
        
        // Non-copyable
        TrackState(const TrackState&) = delete;
        TrackState& operator=(const TrackState&) = delete;
    };

    VampNetTrackEngine();
    ~VampNetTrackEngine();

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

    // Get the click synth for this track
    VampNet::ClickSynth& get_click_synth() { return *m_click_synth; }
    const VampNet::ClickSynth& get_click_synth() const { return *m_click_synth; }
    
    // Get the sampler for this track
    VampNet::Sampler& get_sampler() { return *m_sampler; }
    const VampNet::Sampler& get_sampler() const { return *m_sampler; }

    // Handle audio device starting (update sample rate)
    void audio_device_about_to_start(double sample_rate);

    // Handle audio device stopping
    void audio_device_stopped();

    // Reset playhead to start
    void reset();

    // Load audio file into the output buffer
    // Returns true if successful, false otherwise
    bool load_from_file(const juce::File& audio_file);

    // Access to track state
    TrackState& get_track_state() { return m_track_state; }
    const TrackState& get_track_state() const { return m_track_state; }
    
    // Set panner for spatial audio distribution
    void set_panner(Panner* panner) { m_track_state.m_panner = panner; }

private:
    TrackState m_track_state;
    bool m_was_recording{false};
    bool m_was_playing{false};
    double m_max_buffer_duration_seconds{10.0};
    
    juce::AudioFormatManager m_format_manager;
    
    // Click synth owned by this track
    std::unique_ptr<VampNet::ClickSynth> m_click_synth;
    
    // Sampler owned by this track
    std::unique_ptr<VampNet::Sampler> m_sampler;
};

