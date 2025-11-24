#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <flowerjuce/DSP/MultiChannelLoudnessMeter.h>
#include <array>
#include <atomic>

// Forward declarations
class LooperTrackEngine;
class VampNetTrackEngine;

// TODO: Remove this debug macro after fixing segmentation fault
#define DEBUG_SEGFAULT 0
#if DEBUG_SEGFAULT
#define DBG_SEGFAULT(msg) juce::Logger::writeToLog("[SEGFAULT] " + juce::String(__FILE__) + ":" + juce::String(__LINE__) + " - " + juce::String(msg))
#else
#define DBG_SEGFAULT(msg)
#endif

// Template-based MultiTrackLooperEngine that can work with any track engine type
template<typename TrackEngineType>
class MultiTrackLooperEngineTemplate : public juce::AudioIODeviceCallback
{
public:
    MultiTrackLooperEngineTemplate()
    {
        DBG_SEGFAULT("ENTRY: MultiTrackLooperEngineTemplate::MultiTrackLooperEngineTemplate");
        // Don't initialize audio device manager here - wait until setup is complete
        // This prevents conflicts when applying device settings from the startup dialog
        // Initialize buffers with default sample rate (will be updated when device starts)
        
        // Channel meter UGen will initialize itself
        
        DBG_SEGFAULT("Initializing track engines");
        for (size_t i = 0; i < m_track_engines.size(); ++i)
        {
            DBG_SEGFAULT("Initializing track engine " + juce::String(i));
            m_track_engines[i].initialize(44100.0, m_max_buffer_duration_seconds);
            DBG_SEGFAULT("Track engine " + juce::String(i) + " initialized");
        }
        DBG_SEGFAULT("EXIT: MultiTrackLooperEngineTemplate::MultiTrackLooperEngineTemplate");
    }

    ~MultiTrackLooperEngineTemplate()
    {
        m_audio_device_manager.removeAudioCallback(this);
        m_audio_device_manager.closeAudioDevice();
    }

    void audioDeviceAboutToStart(juce::AudioIODevice* device) override
    {
        DBG_SEGFAULT("ENTRY: audioDeviceAboutToStart, device=" + juce::String(device != nullptr ? "non-null" : "null"));
        DBG("audioDeviceAboutToStart called");
        if (device != nullptr)
        {
            DBG_SEGFAULT("Getting sample rate");
            double sample_rate = device->getCurrentSampleRate();
            DBG_SEGFAULT("Sample rate=" + juce::String(sample_rate));
            m_current_sample_rate.store(sample_rate);

            DBG("Device starting - SampleRate: " << sample_rate
                << " BufferSize: " << device->getCurrentBufferSizeSamples()
                << " InputChannels: " << device->getActiveInputChannels().countNumberOfSetBits()
                << " OutputChannels: " << device->getActiveOutputChannels().countNumberOfSetBits());

            // Reallocate buffers with correct sample rate
            DBG_SEGFAULT("Calling audioDeviceAboutToStart on track engines");
            for (size_t i = 0; i < m_track_engines.size(); ++i)
            {
                DBG_SEGFAULT("Calling audioDeviceAboutToStart on track " + juce::String(i));
                m_track_engines[i].audio_device_about_to_start(sample_rate);
                DBG_SEGFAULT("audioDeviceAboutToStart completed for track " + juce::String(i));
            }
            DBG_SEGFAULT("All track engines notified");
        }
        else
        {
            DBG("WARNING: audioDeviceAboutToStart called with null device!");
        }
        DBG_SEGFAULT("EXIT: audioDeviceAboutToStart");
    }

    void audioDeviceStopped() override
    {
        // Stop all tracks
        for (auto& track_engine : m_track_engines)
        {
            track_engine.audio_device_stopped();
        }
    }

    void audioDeviceIOCallbackWithContext(const float* const* input_channel_data,
                                         int num_input_channels,
                                         float* const* output_channel_data,
                                         int num_output_channels,
                                         int num_samples,
                                         const juce::AudioIODeviceCallbackContext& context) override
    {
        static bool first_callback = true;
        static int callback_count = 0;
        callback_count++;

        if (first_callback)
        {
            DBG_SEGFAULT("ENTRY: audioDeviceIOCallbackWithContext (FIRST CALLBACK)");
            juce::Logger::writeToLog("*** First audio callback! InputChannels: " + juce::String(num_input_channels)
                + " OutputChannels: " + juce::String(num_output_channels)
                + " NumSamples: " + juce::String(num_samples));
            first_callback = false;
        }

        // Log every 10000 callbacks to verify it's running
        if (callback_count % 10000 == 0)
        {
            juce::Logger::writeToLog("Audio callback running - count: " + juce::String(callback_count));
        }

        // Clear output buffers
        DBG_SEGFAULT("Clearing output buffers, num_output_channels=" + juce::String(num_output_channels));
        for (int channel = 0; channel < num_output_channels; ++channel)
        {
            if (output_channel_data[channel] != nullptr)
            {
                juce::FloatVectorOperations::clear(output_channel_data[channel], num_samples);
            }
        }
        DBG_SEGFAULT("Output buffers cleared");

        // Process each track
        static int debug_counter = 0;
        debug_counter++;
        bool should_debug = debug_counter % 2000 == 0;
        if (should_debug)
        {
            juce::Logger::writeToLog("\n--------------------------------");
        }

        DBG_SEGFAULT("Processing tracks, m_num_tracks=" + juce::String(m_num_tracks));
        if (should_debug)
        {
            auto* device = m_audio_device_manager.getCurrentAudioDevice();
            DBG("[MultiTrackLooperEngineTemplate] Processing " << m_num_tracks << " tracks");
            DBG("  num_input_channels: " << num_input_channels);
            DBG("  num_output_channels: " << num_output_channels);
            DBG("  num_samples: " << num_samples);
            if (device != nullptr)
            {
                auto active_output_channels = device->getActiveOutputChannels();
                DBG("  Active output channels: " << active_output_channels.toString(2));
                DBG("  Number of active output channels: " << active_output_channels.countNumberOfSetBits());
            }
        }
        for (int i = 0; i < m_num_tracks; ++i)
        {
            DBG_SEGFAULT("Processing track " + juce::String(i));
            bool debug_this_track = should_debug && i == 0;
            m_track_engines[i].process_block(input_channel_data, num_input_channels,
                                        output_channel_data, num_output_channels,
                                        num_samples, debug_this_track);
            DBG_SEGFAULT("Track " + juce::String(i) + " processed");
        }
        
        // Update channel level meters using UGen
        m_channel_meter.process_block(output_channel_data, num_output_channels, num_samples);
        
                DBG_SEGFAULT("EXIT: audioDeviceIOCallbackWithContext");
    }

    auto& get_track(int track_index)
    {
        jassert(track_index >= 0 && track_index < m_track_engines.size());
        return m_track_engines[track_index].get_track_state();
    }

    TrackEngineType& get_track_engine(int track_index)
    {
        jassert(track_index >= 0 && track_index < m_track_engines.size());
        return m_track_engines[track_index];
    }

    int get_num_tracks() const { return m_num_tracks; }

    void set_num_tracks(int num)
    {
        // For now, we keep it at 8 tracks as specified
        // This can be expanded later
        jassert(num > 0 && num <= 16); // Reasonable limit
        // NOTE: should something be happening here? 
    }

    void sync_all_tracks()
    {
        // Reset all read head playheads to 0
        for (auto& track_engine : m_track_engines)
        {
            track_engine.reset();
        }
    }

    juce::AudioDeviceManager& get_audio_device_manager() { return m_audio_device_manager; }
    
    void start_audio()
    {
        DBG("[MultiTrackLooperEngineTemplate] ENTRY: start_audio");
        DBG_SEGFAULT("ENTRY: start_audio");
        
        // Check current device setup before proceeding
        juce::AudioDeviceManager::AudioDeviceSetup current_setup;
        m_audio_device_manager.getAudioDeviceSetup(current_setup);
        DBG("[MultiTrackLooperEngineTemplate] Current device setup before start_audio:");
        DBG("  outputDeviceName: " << current_setup.outputDeviceName);
        DBG("  inputDeviceName: " << current_setup.inputDeviceName);
        DBG("  useDefaultInputChannels: " << (current_setup.useDefaultInputChannels ? "true" : "false"));
        DBG("  useDefaultOutputChannels: " << (current_setup.useDefaultOutputChannels ? "true" : "false"));
        
        // Initialize audio device if not already initialized
        DBG_SEGFAULT("Getting current audio device");
        auto* device = m_audio_device_manager.getCurrentAudioDevice();
        DBG("[MultiTrackLooperEngineTemplate] Current device: " << (device != nullptr ? device->getName() : "null"));
        DBG_SEGFAULT("Current device=" + juce::String(device != nullptr ? "non-null" : "null"));
        
        if (device == nullptr)
        {
            DBG("[MultiTrackLooperEngineTemplate] WARNING: Device is null!");
            DBG("[MultiTrackLooperEngineTemplate] Checking if device setup has a device name...");
            
            // Check if we have a device name in the setup - if so, try to open it
            if (current_setup.outputDeviceName.isNotEmpty() || current_setup.inputDeviceName.isNotEmpty())
            {
                DBG("[MultiTrackLooperEngineTemplate] Device setup has device names, attempting to open device...");
                juce::String error = m_audio_device_manager.setAudioDeviceSetup(current_setup, true);
                if (error.isNotEmpty())
                {
                    DBG("[MultiTrackLooperEngineTemplate] ERROR opening configured device: " << error);
                    DBG("[MultiTrackLooperEngineTemplate] Falling back to default devices...");
                    // Fall back to defaults only if the configured device fails
                    error = m_audio_device_manager.initialiseWithDefaultDevices(2, 2);
                    if (error.isNotEmpty())
                    {
                        DBG("[MultiTrackLooperEngineTemplate] ERROR initializing with defaults: " << error);
                        DBG_SEGFAULT("Initialization error, returning");
                        return;
                    }
                }
                else
                {
                    DBG("[MultiTrackLooperEngineTemplate] Successfully opened configured device");
                }
            }
            else
            {
                DBG("[MultiTrackLooperEngineTemplate] No device name in setup, initializing with defaults");
                DBG_SEGFAULT("Device is null, initializing with defaults");
                // Device wasn't initialized yet, initialize with default settings
                juce::String error = m_audio_device_manager.initialiseWithDefaultDevices(2, 2);
                if (error.isNotEmpty())
                {
                    DBG("[MultiTrackLooperEngineTemplate] Audio device initialization error: " << error);
                    DBG_SEGFAULT("Initialization error, returning");
                    return;
                }
            }
            
            DBG_SEGFAULT("Getting device after initialization");
            device = m_audio_device_manager.getCurrentAudioDevice();
            DBG("[MultiTrackLooperEngineTemplate] Device after init: " << (device != nullptr ? device->getName() : "null"));
            DBG_SEGFAULT("Device after init=" + juce::String(device != nullptr ? "non-null" : "null"));
        }
        
        if (device != nullptr)
        {
            DBG_SEGFAULT("Getting sample rate");
            double sample_rate = device->getCurrentSampleRate();
            DBG_SEGFAULT("Sample rate=" + juce::String(sample_rate));
            m_current_sample_rate.store(sample_rate);

            DBG("Audio device initialized: " << device->getName()
                << " SampleRate: " << sample_rate
                << " BufferSize: " << device->getCurrentBufferSizeSamples()
                << " InputChannels: " << device->getActiveInputChannels().countNumberOfSetBits()
                << " OutputChannels: " << device->getActiveOutputChannels().countNumberOfSetBits());

            // Update buffers with actual device sample rate
            DBG_SEGFAULT("Calling audio_device_about_to_start on track engines");
            for (size_t i = 0; i < m_track_engines.size(); ++i)
            {
                DBG_SEGFAULT("Calling audio_device_about_to_start on track " + juce::String(i));
                m_track_engines[i].audio_device_about_to_start(sample_rate);
                DBG_SEGFAULT("audio_device_about_to_start completed for track " + juce::String(i));
            }
            DBG_SEGFAULT("All track engines notified");
        }
        
        // Add audio callback now that setup is complete
        DBG_SEGFAULT("Adding audio callback");
        m_audio_device_manager.addAudioCallback(this);
        DBG("Audio callback added to device manager - audio processing started");
        DBG_SEGFAULT("Audio callback added");
        
        // Verify device is running
        DBG_SEGFAULT("Verifying device");
        device = m_audio_device_manager.getCurrentAudioDevice();
        if (device != nullptr)
        {
            DBG("Device check - IsOpen: " << (device->isOpen() ? "YES" : "NO")
                << " IsPlaying: " << (device->isPlaying() ? "YES" : "NO"));
        }
        DBG_SEGFAULT("EXIT: start_audio");
    }

    // Get channel levels for visualization (16 channels)
    std::array<std::atomic<float>, 16>& getChannelLevels() { return m_channel_meter.get_channel_levels(); }
    const std::array<std::atomic<float>, 16>& getChannelLevels() const { return m_channel_meter.get_channel_levels(); }

private:
    static constexpr int m_num_tracks = 8;
    static constexpr double m_max_buffer_duration_seconds = 10.0;

    std::array<TrackEngineType, 8> m_track_engines;
    juce::AudioDeviceManager m_audio_device_manager;
    std::atomic<double> m_current_sample_rate{44100.0};
    
    // Channel level meter UGen
    MultiChannelLoudnessMeter m_channel_meter;
};

// Include track engine headers for type aliases
#include "LooperTrackEngine.h"
#include "VampNetTrackEngine.h"

// Type aliases for convenience
using MultiTrackLooperEngine = MultiTrackLooperEngineTemplate<LooperTrackEngine>;
using VampNetMultiTrackLooperEngine = MultiTrackLooperEngineTemplate<VampNetTrackEngine>;
