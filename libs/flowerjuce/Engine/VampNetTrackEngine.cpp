#include "VampNetTrackEngine.h"
#include "../ClickSynth.h"
#include "../Sampler.h"
#include <cmath>

// TODO: Remove this debug macro after fixing segmentation fault
#ifndef DEBUG_SEGFAULT
#define DEBUG_SEGFAULT 1
#endif
#if DEBUG_SEGFAULT
#ifndef DBG_SEGFAULT
#define DBG_SEGFAULT(msg) juce::Logger::writeToLog("[SEGFAULT] " + juce::String(__FILE__) + ":" + juce::String(__LINE__) + " - " + juce::String(msg))
#endif
#else
#ifndef DBG_SEGFAULT
#define DBG_SEGFAULT(msg)
#endif
#endif

VampNetTrackEngine::VampNetTrackEngine()
    : m_click_synth(std::make_unique<VampNet::ClickSynth>()),
      m_sampler(std::make_unique<VampNet::Sampler>())
{
    m_format_manager.registerBasicFormats();
}

VampNetTrackEngine::~VampNetTrackEngine() = default;

void VampNetTrackEngine::initialize(double sample_rate, double max_buffer_duration_seconds)
{
    m_track_state.m_record_buffer.allocate_buffer(sample_rate, max_buffer_duration_seconds);
    m_track_state.m_output_buffer.allocate_buffer(sample_rate, max_buffer_duration_seconds);
    m_max_buffer_duration_seconds = max_buffer_duration_seconds;
}

void VampNetTrackEngine::audio_device_about_to_start(double sample_rate)
{
    m_track_state.m_record_buffer.allocate_buffer(sample_rate, m_max_buffer_duration_seconds);
    m_track_state.m_output_buffer.allocate_buffer(sample_rate, m_max_buffer_duration_seconds);
    m_track_state.m_write_head.set_sample_rate(sample_rate);
    m_track_state.m_record_read_head.set_sample_rate(sample_rate);
    m_track_state.m_output_read_head.set_sample_rate(sample_rate);
    m_track_state.m_write_head.reset();
    m_track_state.m_record_read_head.reset();
    m_track_state.m_output_read_head.reset();
}

void VampNetTrackEngine::audio_device_stopped()
{
    m_track_state.m_is_playing.store(false);
    m_track_state.m_record_read_head.set_playing(false);
    m_track_state.m_output_read_head.set_playing(false);
}

void VampNetTrackEngine::reset()
{
    m_track_state.m_record_read_head.reset();
    m_track_state.m_output_read_head.reset();
}

bool VampNetTrackEngine::load_from_file(const juce::File& audio_file)
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

    // Get the record buffer's length to truncate output buffer to match
    size_t record_buffer_length = m_track_state.m_record_buffer.m_recorded_length.load();
    size_t record_buffer_wrap_pos = m_track_state.m_write_head.get_wrap_pos();
    if (record_buffer_wrap_pos > 0)
    {
        record_buffer_length = record_buffer_wrap_pos;
    }
    
    // If no record buffer length, we can't truncate - return error
    if (record_buffer_length == 0)
    {
        DBG("Cannot load output buffer: record buffer has no recorded audio");
        return false;
    }

    const juce::ScopedLock sl(m_track_state.m_output_buffer.m_lock);
    auto& buffer = m_track_state.m_output_buffer.get_buffer();
    
    if (buffer.empty())
    {
        DBG("Output buffer not allocated. Call initialize() first.");
        return false;
    }

    // Clear the output buffer first
    m_track_state.m_output_buffer.clear_buffer();

    // Determine how many samples to read - truncate to record buffer length
    juce::int64 num_samples_to_read = juce::jmin(
        reader->lengthInSamples, 
        static_cast<juce::int64>(buffer.size()),
        static_cast<juce::int64>(record_buffer_length)
    );
    
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

    // Convert to mono and write to output buffer
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

    // Update output buffer metadata - always truncate to record buffer length
    // This ensures output buffer matches input buffer bounds exactly
    size_t loaded_length = static_cast<size_t>(num_samples_to_read);
    
    // Always use record buffer length as the final length (truncate if needed)
    // This ensures both buffers have the same playback length
    m_track_state.m_output_buffer.m_recorded_length.store(record_buffer_length);
    m_track_state.m_output_buffer.m_has_recorded.store(true);
    
    // If the loaded audio is shorter than the record buffer, pad with silence
    // If longer, it's already truncated by num_samples_to_read calculation above
    if (loaded_length < record_buffer_length)
    {
        // Zero out the remaining samples
        for (size_t i = loaded_length; i < record_buffer_length && i < buffer.size(); ++i)
        {
            buffer[i] = 0.0f;
        }
    }
    
    // Reset read heads to start (both buffers share the same playhead position)
    m_track_state.m_record_read_head.reset();
    m_track_state.m_output_read_head.reset();
    m_track_state.m_record_read_head.set_pos(0.0f);
    m_track_state.m_output_read_head.set_pos(0.0f);

    DBG("Loaded audio file into output buffer: " << audio_file.getFileName() 
        << " (" << num_samples_to_read << " samples, "
        << (num_samples_to_read / reader->sampleRate) << " seconds)");

    return true;
}

bool VampNetTrackEngine::process_block(const float* const* input_channel_data,
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
        DBG_SEGFAULT("ENTRY: VampNetTrackEngine::process_block, num_samples=" + juce::String(num_samples));
    
    auto& track = m_track_state;
    if (is_first_call)
        DBG_SEGFAULT("Got track reference");

    // Safety check: if buffers are not allocated, return early
    {
        const juce::ScopedLock sl1(track.m_record_buffer.m_lock);
        const juce::ScopedLock sl2(track.m_output_buffer.m_lock);
        if (is_first_call)
            DBG_SEGFAULT("Checking if buffers are empty");
        if (track.m_record_buffer.get_buffer().empty() || track.m_output_buffer.get_buffer().empty()) {
            juce::Logger::writeToLog("WARNING: Buffers are empty in process_block");
            if (is_first_call)
                DBG_SEGFAULT("Buffers are empty, returning false");
            return false;
        }
        if (is_first_call)
            DBG_SEGFAULT("Buffers are not empty");
    }

    bool is_playing = track.m_is_playing.load();
    bool has_existing_audio = track.m_record_buffer.m_has_recorded.load();
    
    if (is_first_call && should_debug)
    {
        DBG("[VampNetTrackEngine] Track state check:");
        DBG("  is_playing: " << (is_playing ? "YES" : "NO"));
        DBG("  has_existing_audio: " << (has_existing_audio ? "YES" : "NO"));
        DBG("  recorded_length: " << track.m_record_buffer.m_recorded_length.load());
        DBG("  record_enable: " << (track.m_write_head.get_record_enable() ? "YES" : "NO"));
    }
    size_t recorded_length = track.m_record_buffer.m_recorded_length.load();
    float playhead_pos = track.m_record_read_head.get_pos();

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

        juce::Logger::writeToLog(juce::String("VampNetTrack")
            + "\t - Play: " + (is_playing ? "YES" : "NO")
            + "\t RecEnable: " + (track.m_write_head.get_record_enable() ? "YES" : "NO")
            + "\t Playhead: " + juce::String(playhead_pos)
            + "\t RecordedLen: " + juce::String(recorded_length)
            + "\t HasAudio: " + (has_existing_audio ? "YES" : "NO")
            + "\t InputLevel: " + juce::String(input_level)
            + "\t MaxInput: " + juce::String(max_input)
            + "\t InputChannels: " + juce::String(num_input_channels)
            + "\t NumSamples: " + juce::String(num_samples)
            + "\t WrapPos: " + juce::String(track.m_write_head.get_wrap_pos())
            + "\t DryWetMix: " + juce::String(track.m_dry_wet_mix.load())
        );
    }

    // Check if we just started recording (wasn't recording before, but are now)
    bool this_block_is_first_time_recording = !m_was_recording && track.m_write_head.get_record_enable() && !has_existing_audio;

    // If we just stopped recording (record enable turned off), finalize the loop
    bool recording_finalized = false;
    if (m_was_recording && !track.m_write_head.get_record_enable() && is_playing && !has_existing_audio)
    {
        track.m_write_head.finalize_recording(track.m_write_head.get_pos());
        track.m_record_read_head.reset(); // Reset playhead to start of loop
        track.m_output_read_head.reset();
        recording_finalized = true;
        juce::Logger::writeToLog("~~~ Finalized initial recording");
    }

    // Update wasRecording for next callback
    m_was_recording = track.m_write_head.get_record_enable();
    bool playback_just_stopped = m_was_playing && !is_playing;
    m_was_playing = is_playing;

    if (is_playing)
    {
        // If we just started recording, reset everything to 0 BEFORE processing
        if (this_block_is_first_time_recording) // REC_INIT state
        {
            const juce::ScopedLock sl(track.m_record_buffer.m_lock);
            track.m_record_buffer.clear_buffer(); // TODO: should NOT be in callback.
            track.m_write_head.reset();
            track.m_record_read_head.reset();
            track.m_output_read_head.reset();
            juce::Logger::writeToLog("~~~ Reset playhead for new recording");
        }

        // Update read head states
        track.m_record_read_head.set_playing(true);
        track.m_output_read_head.set_playing(true);

        // Allocate temporary mono buffer for playback samples
        juce::HeapBlock<float> mono_buffer(num_samples);
        const float* mono_input_channel_data[1] = { mono_buffer.getData() };

        if (is_first_call)
            DBG_SEGFAULT("Entering sample loop, num_samples=" + juce::String(num_samples));
        
        // OPTIMIZATION: Pre-cache values that don't change during the block
        float wrap_pos = static_cast<float>(track.m_write_head.get_wrap_pos());
        bool is_recording = track.m_write_head.get_record_enable() && num_input_channels > 0;
        int input_channel = track.m_write_head.get_input_channel();
        bool click_active = m_click_synth->isClickActive();
        bool sampler_active = m_sampler->isPlaying();
        double sample_rate = track.m_write_head.get_sample_rate();
        float mix = track.m_dry_wet_mix.load();
        
        // Store positions and input samples for writing (since writeHead locks internally)
        juce::Array<float> write_positions;
        juce::Array<float> write_samples;
        if (is_recording)
        {
            write_positions.resize(num_samples);
            write_samples.resize(num_samples);
        }
        
        // OPTIMIZATION: Lock buffers once per block for reading instead of per-sample
        // This dramatically reduces lock contention (from num_samples locks to 1 lock per block)
        {
            const juce::ScopedLock sl1(track.m_record_buffer.m_lock);
            const juce::ScopedLock sl2(track.m_output_buffer.m_lock);
            
            for (int sample = 0; sample < num_samples; ++sample)
            {
                if (is_first_call && sample == 0)
                    DBG_SEGFAULT("First sample iteration");
                
                float current_position = track.m_record_read_head.get_pos();

                // Prepare input sample for recording (store for writing after releasing locks)
                if (is_recording)
                {
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
                    
                    // Mix in click audio if click synth is active
                    if (click_active)
                    {
                        float click_sample = m_click_synth->getNextSample(sample_rate);
                        input_sample += click_sample; // Mix click into input
                    }
                    
                    // Mix in sampler audio if sampler is playing
                    if (sampler_active)
                    {
                        float sampler_sample = m_sampler->getNextSample();
                        input_sample += sampler_sample; // Mix sampler into input
                    }
                    
                    // Store position and sample for later writing
                    write_positions.set(sample, current_position);
                    write_samples.set(sample, input_sample);
                }

                // Playback - read from both buffers and mix dry/wet
                // Sync output read head position to match record read head
                track.m_output_read_head.set_pos(track.m_record_read_head.get_pos());
                
                float dry_sample = track.m_record_read_head.process_sample();
                float wet_sample = track.m_output_read_head.process_sample();
                
                // Mix dry and wet based on dry/wet mix parameter
                // m_dry_wet_mix: 0.0 = all dry (record buffer), 1.0 = all wet (output buffer)
                float sample_value = dry_sample * (1.0f - mix) + wet_sample * mix;
                
                // Store sample in mono buffer for panner processing
                mono_buffer[sample] = sample_value;

                // Advance both read heads together (same playhead position)
                bool wrapped_record = track.m_record_read_head.advance(wrap_pos);
                bool wrapped_output = track.m_output_read_head.advance(wrap_pos);
                
                // Sync positions in case they diverged
                track.m_output_read_head.set_pos(track.m_record_read_head.get_pos());
                
                if (wrapped_record && !has_existing_audio)
                {
                    recording_finalized = true;
                    juce::Logger::writeToLog("~~~ WRAPPED! Finalized recording");
                    track.m_write_head.set_record_enable(false);
                    track.m_write_head.finalize_recording(wrap_pos);
                    // Sync output buffer wrapPos to match record buffer
                    track.m_output_buffer.m_recorded_length.store(track.m_record_buffer.m_recorded_length.load());
                }
            }
        } // Locks released here
        
        // Apply panner to distribute audio to all output channels
        if (track.m_panner != nullptr)
        {
            // Use panner to distribute mono audio to all output channels with proper gains
            track.m_panner->process_block(mono_input_channel_data, 1, output_channel_data, num_output_channels, num_samples);
            
            if (is_first_call && should_debug)
            {
                DBG("[VampNetTrackEngine] Panner applied - routing to all " << num_output_channels << " channels");
            }
        }
        else
        {
            // Fallback: route to all channels at unity gain (when panner not set)
            for (int sample = 0; sample < num_samples; ++sample)
            {
                float sample_value = mono_buffer[sample];
                for (int channel = 0; channel < num_output_channels; ++channel)
                {
                    if (output_channel_data[channel] != nullptr)
                    {
                        output_channel_data[channel][sample] += sample_value;
                    }
                }
            }
            
            if (is_first_call && should_debug)
            {
                DBG("[VampNetTrackEngine] No panner set - routing to all channels at unity gain");
            }
        }
        
        // Process writes after releasing read locks to avoid deadlock
        // WriteHead locks internally, so we can safely call it now
        if (is_recording)
        {
            for (int sample = 0; sample < num_samples; ++sample)
            {
                float input_sample = write_samples[sample];
                if (input_sample != 0.0f)
                {
                    float current_write_pos = write_positions[sample];
                    track.m_write_head.process_sample(input_sample, current_write_pos);
                }
            }
        }
        if (is_first_call)
            DBG_SEGFAULT("Sample loop completed");
    }
    else
    {
        // Not playing - stop read heads
        track.m_record_read_head.set_playing(false);
        track.m_output_read_head.set_playing(false);

        if (track.m_write_head.get_record_enable() && playback_just_stopped)
        {
            // finalize recording if we were recording and playback just stopped
            track.m_write_head.finalize_recording(track.m_write_head.get_pos());
            recording_finalized = true;
            // Record enable is on but playback just stopped - prepare for new recording
            juce::Logger::writeToLog("WARNING: ActuallyRecording but not playing.");
        }
    }

    return recording_finalized;
}


