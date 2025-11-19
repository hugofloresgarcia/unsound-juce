#include "LooperTrackEngine.h"
#include <cmath>

// TODO: Remove this debug macro after fixing segmentation fault
#define DEBUG_SEGFAULT 1
#if DEBUG_SEGFAULT
#define DBG_SEGFAULT(msg) juce::Logger::writeToLog("[SEGFAULT] " + juce::String(__FILE__) + ":" + juce::String(__LINE__) + " - " + juce::String(msg))
#else
#define DBG_SEGFAULT(msg)
#endif

LooperTrackEngine::LooperTrackEngine()
{
    m_format_manager.registerBasicFormats();
}

void LooperTrackEngine::initialize(double sample_rate, double max_buffer_duration_seconds)
{
    m_track_state.m_tape_loop.allocate_buffer(sample_rate, max_buffer_duration_seconds);
    m_max_buffer_duration_seconds = max_buffer_duration_seconds;
}

void LooperTrackEngine::audio_device_about_to_start(double sample_rate)
{
    m_track_state.m_tape_loop.allocate_buffer(sample_rate, m_max_buffer_duration_seconds);
    m_track_state.m_write_head.set_sample_rate(sample_rate);
    m_track_state.m_read_head.prepare(sample_rate);
    m_track_state.m_write_head.reset();
    m_track_state.m_read_head.reset();
    
    // Prepare filter and peak meter for new sample rate
    m_low_pass_filter.prepare(sample_rate, 512);
    m_peak_meter.prepare();
}

void LooperTrackEngine::audio_device_stopped()
{
    m_track_state.m_is_playing.store(false);
    m_track_state.m_read_head.set_playing(false);
}

void LooperTrackEngine::set_filter_cutoff(float cutoff_hz)
{
    m_low_pass_filter.set_cutoff(cutoff_hz);
}

void LooperTrackEngine::set_loop_end(size_t loop_end)
{
    // Update both read and write heads to keep them synchronized
    m_track_state.m_write_head.set_loop_end(loop_end);
    m_track_state.m_read_head.set_loop_end(static_cast<float>(loop_end));
}

void LooperTrackEngine::reset()
{
    m_track_state.m_read_head.reset();
    m_track_state.m_write_head.reset();
}

bool LooperTrackEngine::load_from_file(const juce::File& audio_file)
{
    if (!audio_file.existsAsFile())
    {
        DBG("Audio file does not exist: " << audio_file.getFullPathName());
        return false;
    }

    // Create reader for the audio file
    std::unique_ptr<juce::AudioFormatReader> reader(m_format_manager.createReaderFor(audio_file));
    if (reader == nullptr)
    {
        DBG("Could not create reader for file: " << audio_file.getFullPathName());
        return false;
    }

    const juce::ScopedLock sl(m_track_state.m_tape_loop.m_lock);
    auto& buffer = m_track_state.m_tape_loop.get_buffer();
    
    if (buffer.empty())
    {
        DBG("TapeLoop buffer not allocated. Call initialize() first.");
        return false;
    }

    // Clear the buffer first
    m_track_state.m_tape_loop.clear_buffer();

    // Determine how many samples to read (limited by buffer size)
    juce::int64 num_samples_to_read = juce::jmin(reader->lengthInSamples, static_cast<juce::int64>(buffer.size()));
    
    if (num_samples_to_read <= 0)
    {
        DBG("Audio file has no samples or buffer is empty");
        return false;
    }

    // Read audio data
    // If multi-channel, we'll mix down to mono by averaging channels
    juce::AudioBuffer<float> temp_buffer(static_cast<int>(reader->numChannels), static_cast<int>(num_samples_to_read));
    
    if (!reader->read(&temp_buffer, 0, static_cast<int>(num_samples_to_read), 0, true, true))
    {
        DBG("Failed to read audio data from file");
        return false;
    }

    // Convert to mono and write to TapeLoop buffer
    // If single channel, just copy. If multi-channel, average them.
    if (temp_buffer.getNumChannels() == 1)
    {
        // Direct copy
        const float* source = temp_buffer.getReadPointer(0);
        for (int i = 0; i < static_cast<int>(num_samples_to_read); ++i)
        {
            buffer[i] = source[i];
        }
    }
    else
    {
        // Mix down to mono by averaging all channels
        for (int i = 0; i < static_cast<int>(num_samples_to_read); ++i)
        {
            float sum = 0.0f;
            for (int channel = 0; channel < temp_buffer.getNumChannels(); ++channel)
            {
                sum += temp_buffer.getSample(channel, i);
            }
            buffer[i] = sum / static_cast<float>(temp_buffer.getNumChannels());
        }
    }

    // Update wrapPos to reflect the loaded audio length
    size_t loaded_length = static_cast<size_t>(num_samples_to_read);
    set_loop_end(loaded_length);
    m_track_state.m_write_head.set_pos(loaded_length);
    
    // Update TapeLoop metadata
    m_track_state.m_tape_loop.m_recorded_length.store(loaded_length);
    m_track_state.m_tape_loop.m_has_recorded.store(true);
    
    // Reset read head to start
    m_track_state.m_read_head.reset();
    m_track_state.m_read_head.set_pos(0.0f);

    DBG("Loaded audio file: " << audio_file.getFileName() 
        << " (" << num_samples_to_read << " samples, "
        << (num_samples_to_read / reader->sampleRate) << " seconds)");

    return true;
}

bool LooperTrackEngine::process_block(const float* const* input_channel_data,
                                     int num_input_channels,
                                     float* const* output_channel_data,
                                     int num_output_channels,
                                     int num_samples,
                                     bool should_debug)
{
    static int call_count = 0;
    call_count++;
    bool is_first_call = (call_count == 1);
    
    if (is_first_call)
        DBG_SEGFAULT("ENTRY: LooperTrackEngine::process_block, num_samples=" + juce::String(num_samples));
    
    auto& track = m_track_state;
    if (is_first_call)
        DBG_SEGFAULT("Got track reference");

    // Safety check: if buffer is not allocated, return early
    {
        const juce::ScopedLock sl(track.m_tape_loop.m_lock);
        if (is_first_call)
            DBG_SEGFAULT("Checking if buffer is empty");
        if (track.m_tape_loop.get_buffer().empty()) {
            juce::Logger::writeToLog("WARNING: TapeLoop buffer is empty in process_block");
            if (is_first_call)
                DBG_SEGFAULT("Buffer is empty, returning false");
            return false;
        }
        if (is_first_call)
            DBG_SEGFAULT("Buffer is not empty, size=" + juce::String(track.m_tape_loop.get_buffer().size()));
    }

    bool is_playing = track.m_is_playing.load();
    bool has_existing_audio = track.m_tape_loop.m_has_recorded.load();
    
    if (is_first_call && should_debug)
    {
        DBG("[LooperTrackEngine] Track state check:");
        DBG("  is_playing: " << (is_playing ? "YES" : "NO"));
        DBG("  has_existing_audio: " << (has_existing_audio ? "YES" : "NO"));
        DBG("  recorded_length: " << track.m_tape_loop.m_recorded_length.load());
        DBG("  record_enable: " << (track.m_write_head.get_record_enable() ? "YES" : "NO"));
    }
    size_t recorded_length = track.m_tape_loop.m_recorded_length.load();
    float playhead_pos = track.m_read_head.get_pos();

    // Debug output
    if (should_debug)
    {
        float input_level = 0.0f;
        float max_input = 0.0f;
        if (input_channel_data[0] != nullptr && num_input_channels > 0 && num_samples > 0)
        {
            input_level = std::abs(input_channel_data[0][0]);
            for (int s = 0; s < num_samples && s < 100; ++s)
            {
                max_input = juce::jmax(max_input, std::abs(input_channel_data[0][s]));
            }
        }

        juce::Logger::writeToLog(juce::String("Track")
            + "\t - Play: " + (is_playing ? "YES" : "NO")
            + "\t RecEnable: " + (track.m_write_head.get_record_enable() ? "YES" : "NO")
            + "\t ActuallyRec: " + (track.m_write_head.get_record_enable() ? "YES" : "NO")
            + "\t Playhead: " + juce::String(playhead_pos)
            + "\t RecordedLen: " + juce::String(recorded_length)
            + "\t HasAudio: " + (has_existing_audio ? "YES" : "NO")
            + "\t InputLevel: " + juce::String(input_level)
            + "\t MaxInput: " + juce::String(max_input)
            + "\t InputChannels: " + juce::String(num_input_channels)
            + "\t NumSamples: " + juce::String(num_samples)
            + "\t WrapPos: " + juce::String(track.m_write_head.get_loop_end())
            + "\t LoopEnd: " + juce::String(track.m_tape_loop.get_buffer_size())
        );
    }

    // Check if we just started recording (wasn't recording before, but are now)
    bool this_block_is_first_time_recording = !m_was_recording && track.m_write_head.get_record_enable() && !has_existing_audio;

    // Check for recording finalization
    bool recording_finalized = false;
    bool should_finalize = finalize_recording_if_needed(track, m_was_recording, is_playing, has_existing_audio, recording_finalized);
    
    // Update wasRecording for next callback
    m_was_recording = track.m_write_head.get_record_enable();
    bool playback_just_stopped = m_was_playing && !is_playing;
    m_was_playing = is_playing;

    if (is_playing)
    {
        // If we just started recording, reset everything to 0 BEFORE processing
        if (this_block_is_first_time_recording) // REC_INIT state
        {
            const juce::ScopedLock sl(track.m_tape_loop.m_lock);
            track.m_tape_loop.clear_buffer(); // TODO: should NOT be in callback.
            track.m_write_head.reset();
            track.m_read_head.reset();
            juce::Logger::writeToLog("~~~ Reset playhead for new recording");
        }

        // Update read head state
        track.m_read_head.set_playing(true);

        // Allocate temporary mono buffer for playback samples
        juce::HeapBlock<float> mono_buffer(num_samples);
        const float* mono_input_channel_data[1] = { mono_buffer.getData() };

        if (is_first_call)
            DBG_SEGFAULT("Entering sample loop, num_samples=" + juce::String(num_samples));
        
        // First pass: collect playback samples and handle recording
        float max_mono_level = 0.0f;
        for (int sample = 0; sample < num_samples; ++sample)
        {
            if (is_first_call && sample == 0)
                DBG_SEGFAULT("First sample iteration");
            
            float current_position = track.m_read_head.get_pos();

            // Handle recording (overdub or new)
            process_recording(track, input_channel_data, num_input_channels, current_position, sample, is_first_call && sample == 0);

            // Get raw sample value BEFORE level gain/mute (pre-fader) for onset detection
            // Must hold lock since get_raw_sample() accesses tape loop buffer
            float raw_sample_value = 0.0f;
            if (track.m_is_playing.load() && track.m_read_head.get_playing())
            {
                const juce::ScopedLock sl(track.m_tape_loop.m_lock);
                raw_sample_value = track.m_read_head.get_raw_sample();
            }
            
            // Feed raw pre-fader sample to callback if set (for onset detection, etc.)
            // This ensures onset detection happens before any level control or filtering
            if (m_audio_sample_callback)
            {
                m_audio_sample_callback(raw_sample_value);
            }
            
            // Playback (read head processes the sample AND advances) - applies level gain and mute
            bool wrapped = false;
            float sample_value = process_playback(track, wrapped, is_first_call && sample == 0);
            
            // Store sample in mono buffer for panner processing
            mono_buffer[sample] = sample_value;
            
            // Track peak level for visualization
            max_mono_level = juce::jmax(max_mono_level, std::abs(sample_value));

            // Check for wrap and finalize recording if needed
            if (wrapped && !has_existing_audio)
            {
                track.m_write_head.set_record_enable(false); // Stop recording
                juce::Logger::writeToLog("~~~ WRAPPED! Finalized recording");
            }
        }
        
        // Apply low pass filter to mono buffer
        m_low_pass_filter.process_block(mono_buffer.getData(), num_samples);
        
        // Update peak meter
        m_peak_meter.process_block(mono_buffer.getData(), num_samples);
        

        jassert(track.m_panner != nullptr);
        // Use panner to distribute mono audio to all output channels with proper gains
        track.m_panner->process_block(mono_input_channel_data, 1, output_channel_data, num_output_channels, num_samples);
        
        if (is_first_call && should_debug)
        {
            DBG("[LooperTrackEngine] Panner applied - routing to all " << num_output_channels << " channels");
            DBG_SEGFAULT("Sample loop completed");
        }
    }
    else
    {
        // Not playing - stop read head
        track.m_read_head.set_playing(false);

        if (track.m_write_head.get_record_enable() && playback_just_stopped)
        {
            // finalize recording if we were recording and playback just stopped
            track.m_write_head.set_record_enable(false); // Stop recording and update UI
            set_loop_end(track.m_write_head.get_pos());
            recording_finalized = true;
            // Record enable is on but playback just stopped - prepare for new recording
            juce::Logger::writeToLog("WARNING: ActuallyRecording but not playing.");
        }
    }

    return recording_finalized;
}

// Helper method: Process recording for a single sample
void LooperTrackEngine::process_recording(TrackState& track, const float* const* input_channel_data, 
                                         int num_input_channels, float current_position, int sample, bool is_first_call)
{
    // Note: writeHead.processSample() locks the buffer internally, so we don't hold the lock here
    if (track.m_write_head.get_record_enable() && num_input_channels > 0)
    {
        int input_channel = track.m_write_head.get_input_channel();
        float input_sample = 0.0f;
        
        // Get input sample from selected channel
        if (input_channel == -1)
        {
            // All channels: use channel 0 (mono sum could be added later)
            if (input_channel_data[0] != nullptr)
                input_sample = input_channel_data[0][sample];
        }
        else if (input_channel >= 0 && input_channel < num_input_channels && input_channel_data[input_channel] != nullptr)
        {
            input_sample = input_channel_data[input_channel][sample];
        }
        
        if (is_first_call)
            DBG_SEGFAULT("Calling writeHead.process_sample");
        track.m_write_head.process_sample(input_sample, current_position);
        if (is_first_call)
            DBG_SEGFAULT("writeHead.process_sample completed");
    }
}

// Helper method: Process playback for a single sample
float LooperTrackEngine::process_playback(TrackState& track, bool& wrapped, bool is_first_call)
{
    const juce::ScopedLock sl(track.m_tape_loop.m_lock);
    
    if (is_first_call)
    {
        DBG_SEGFAULT("Calling readHead.process_sample");
        DBG("[LooperTrackEngine] Track playback state:");
        DBG("  is_playing: " << (track.m_is_playing.load() ? "YES" : "NO"));
        DBG("  has_recorded_audio: " << (track.m_tape_loop.m_recorded_length.load() > 0 ? "YES" : "NO"));
        DBG("  recorded_length: " << track.m_tape_loop.m_recorded_length.load());
        DBG("  readHead position: " << track.m_read_head.get_pos());
    }
    float sample_value = track.m_read_head.process_sample(wrapped);
    if (is_first_call)
    {
        DBG_SEGFAULT("readHead.process_sample completed, value=" + juce::String(sample_value) + ", wrapped=" + juce::String(wrapped ? "YES" : "NO"));
        DBG("[LooperTrackEngine] Track sample_value: " << sample_value);
    }
    
    return sample_value;
}

// Helper method: Check if recording should be finalized
bool LooperTrackEngine::finalize_recording_if_needed(TrackState& track, bool was_recording, bool is_playing, 
                                                   bool has_existing_audio, bool& recording_finalized)
{
    // If we just stopped recording (record enable turned off), finalize the loop
    if (was_recording && !track.m_write_head.get_record_enable() && is_playing && !has_existing_audio)
    {
        track.m_write_head.finalize_recording(track.m_write_head.get_pos());
        recording_finalized = true;
        juce::Logger::writeToLog("~~~ Finalized initial recording (it was needed)");
        return true;
    }
    return false;
}

