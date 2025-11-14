#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_devices/juce_audio_devices.h>
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
        DBG_SEGFAULT("Initializing track engines");
        for (size_t i = 0; i < trackEngines.size(); ++i)
        {
            DBG_SEGFAULT("Initializing track engine " + juce::String(i));
            trackEngines[i].initialize(44100.0, maxBufferDurationSeconds);
            DBG_SEGFAULT("Track engine " + juce::String(i) + " initialized");
        }
        DBG_SEGFAULT("EXIT: MultiTrackLooperEngineTemplate::MultiTrackLooperEngineTemplate");
    }

    ~MultiTrackLooperEngineTemplate()
    {
        audioDeviceManager.removeAudioCallback(this);
        audioDeviceManager.closeAudioDevice();
    }

    void audioDeviceAboutToStart(juce::AudioIODevice* device) override
    {
        DBG_SEGFAULT("ENTRY: audioDeviceAboutToStart, device=" + juce::String(device != nullptr ? "non-null" : "null"));
        DBG("audioDeviceAboutToStart called");
        if (device != nullptr)
        {
            DBG_SEGFAULT("Getting sample rate");
            double sampleRate = device->getCurrentSampleRate();
            DBG_SEGFAULT("Sample rate=" + juce::String(sampleRate));
            currentSampleRate.store(sampleRate);

            DBG("Device starting - SampleRate: " << sampleRate
                << " BufferSize: " << device->getCurrentBufferSizeSamples()
                << " InputChannels: " << device->getActiveInputChannels().countNumberOfSetBits()
                << " OutputChannels: " << device->getActiveOutputChannels().countNumberOfSetBits());

            // Reallocate buffers with correct sample rate
            DBG_SEGFAULT("Calling audioDeviceAboutToStart on track engines");
            for (size_t i = 0; i < trackEngines.size(); ++i)
            {
                DBG_SEGFAULT("Calling audioDeviceAboutToStart on track " + juce::String(i));
                trackEngines[i].audioDeviceAboutToStart(sampleRate);
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
        for (auto& trackEngine : trackEngines)
        {
            trackEngine.audioDeviceStopped();
        }
    }

    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                         int numInputChannels,
                                         float* const* outputChannelData,
                                         int numOutputChannels,
                                         int numSamples,
                                         const juce::AudioIODeviceCallbackContext& context) override
    {
        static bool firstCallback = true;
        static int callbackCount = 0;
        callbackCount++;

        if (firstCallback)
        {
            DBG_SEGFAULT("ENTRY: audioDeviceIOCallbackWithContext (FIRST CALLBACK)");
            juce::Logger::writeToLog("*** First audio callback! InputChannels: " + juce::String(numInputChannels)
                + " OutputChannels: " + juce::String(numOutputChannels)
                + " NumSamples: " + juce::String(numSamples));
            firstCallback = false;
        }

        // Log every 10000 callbacks to verify it's running
        if (callbackCount % 10000 == 0)
        {
            juce::Logger::writeToLog("Audio callback running - count: " + juce::String(callbackCount));
        }

        // Clear output buffers
        DBG_SEGFAULT("Clearing output buffers, numOutputChannels=" + juce::String(numOutputChannels));
        for (int channel = 0; channel < numOutputChannels; ++channel)
        {
            if (outputChannelData[channel] != nullptr)
            {
                juce::FloatVectorOperations::clear(outputChannelData[channel], numSamples);
            }
        }
        DBG_SEGFAULT("Output buffers cleared");

        // Process each track
        static int debugCounter = 0;
        debugCounter++;
        bool shouldDebug = debugCounter % 2000 == 0;
        if (shouldDebug)
        {
            juce::Logger::writeToLog("\n--------------------------------");
        }

        DBG_SEGFAULT("Processing tracks, numTracks=" + juce::String(numTracks));
        if (shouldDebug)
        {
            auto* device = audioDeviceManager.getCurrentAudioDevice();
            DBG("[MultiTrackLooperEngineTemplate] Processing " << numTracks << " tracks");
            DBG("  numInputChannels: " << numInputChannels);
            DBG("  numOutputChannels: " << numOutputChannels);
            DBG("  numSamples: " << numSamples);
            if (device != nullptr)
            {
                auto activeOutputChannels = device->getActiveOutputChannels();
                DBG("  Active output channels: " << activeOutputChannels.toString(2));
                DBG("  Number of active output channels: " << activeOutputChannels.countNumberOfSetBits());
            }
        }
        for (int i = 0; i < numTracks; ++i)
        {
            DBG_SEGFAULT("Processing track " + juce::String(i));
            bool debugThisTrack = shouldDebug && i == 0;
            trackEngines[i].processBlock(inputChannelData, numInputChannels,
                                        outputChannelData, numOutputChannels,
                                        numSamples, debugThisTrack);
            DBG_SEGFAULT("Track " + juce::String(i) + " processed");
        }
        
        DBG_SEGFAULT("EXIT: audioDeviceIOCallbackWithContext");
    }

    auto& getTrack(int trackIndex)
    {
        jassert(trackIndex >= 0 && trackIndex < trackEngines.size());
        return trackEngines[trackIndex].getTrackState();
    }

    TrackEngineType& getTrackEngine(int trackIndex)
    {
        jassert(trackIndex >= 0 && trackIndex < trackEngines.size());
        return trackEngines[trackIndex];
    }

    int getNumTracks() const { return numTracks; }

    void setNumTracks(int num)
    {
        // For now, we keep it at 8 tracks as specified
        // This can be expanded later
        jassert(num > 0 && num <= 16); // Reasonable limit
    }

    void syncAllTracks()
    {
        // Reset all read head playheads to 0
        for (auto& trackEngine : trackEngines)
        {
            trackEngine.reset();
        }
    }

    juce::AudioDeviceManager& getAudioDeviceManager() { return audioDeviceManager; }
    
    void startAudio()
    {
        DBG("[MultiTrackLooperEngineTemplate] ENTRY: startAudio");
        DBG_SEGFAULT("ENTRY: startAudio");
        
        // Check current device setup before proceeding
        juce::AudioDeviceManager::AudioDeviceSetup currentSetup;
        audioDeviceManager.getAudioDeviceSetup(currentSetup);
        DBG("[MultiTrackLooperEngineTemplate] Current device setup before startAudio:");
        DBG("  outputDeviceName: " << currentSetup.outputDeviceName);
        DBG("  inputDeviceName: " << currentSetup.inputDeviceName);
        DBG("  useDefaultInputChannels: " << (currentSetup.useDefaultInputChannels ? "true" : "false"));
        DBG("  useDefaultOutputChannels: " << (currentSetup.useDefaultOutputChannels ? "true" : "false"));
        
        // Initialize audio device if not already initialized
        DBG_SEGFAULT("Getting current audio device");
        auto* device = audioDeviceManager.getCurrentAudioDevice();
        DBG("[MultiTrackLooperEngineTemplate] Current device: " << (device != nullptr ? device->getName() : "null"));
        DBG_SEGFAULT("Current device=" + juce::String(device != nullptr ? "non-null" : "null"));
        
        if (device == nullptr)
        {
            DBG("[MultiTrackLooperEngineTemplate] WARNING: Device is null!");
            DBG("[MultiTrackLooperEngineTemplate] Checking if device setup has a device name...");
            
            // Check if we have a device name in the setup - if so, try to open it
            if (currentSetup.outputDeviceName.isNotEmpty() || currentSetup.inputDeviceName.isNotEmpty())
            {
                DBG("[MultiTrackLooperEngineTemplate] Device setup has device names, attempting to open device...");
                juce::String error = audioDeviceManager.setAudioDeviceSetup(currentSetup, true);
                if (error.isNotEmpty())
                {
                    DBG("[MultiTrackLooperEngineTemplate] ERROR opening configured device: " << error);
                    DBG("[MultiTrackLooperEngineTemplate] Falling back to default devices...");
                    // Fall back to defaults only if the configured device fails
                    error = audioDeviceManager.initialiseWithDefaultDevices(2, 2);
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
                juce::String error = audioDeviceManager.initialiseWithDefaultDevices(2, 2);
                if (error.isNotEmpty())
                {
                    DBG("[MultiTrackLooperEngineTemplate] Audio device initialization error: " << error);
                    DBG_SEGFAULT("Initialization error, returning");
                    return;
                }
            }
            
            DBG_SEGFAULT("Getting device after initialization");
            device = audioDeviceManager.getCurrentAudioDevice();
            DBG("[MultiTrackLooperEngineTemplate] Device after init: " << (device != nullptr ? device->getName() : "null"));
            DBG_SEGFAULT("Device after init=" + juce::String(device != nullptr ? "non-null" : "null"));
        }
        
        if (device != nullptr)
        {
            DBG_SEGFAULT("Getting sample rate");
            double sampleRate = device->getCurrentSampleRate();
            DBG_SEGFAULT("Sample rate=" + juce::String(sampleRate));
            currentSampleRate.store(sampleRate);

            DBG("Audio device initialized: " << device->getName()
                << " SampleRate: " << sampleRate
                << " BufferSize: " << device->getCurrentBufferSizeSamples()
                << " InputChannels: " << device->getActiveInputChannels().countNumberOfSetBits()
                << " OutputChannels: " << device->getActiveOutputChannels().countNumberOfSetBits());

            // Update buffers with actual device sample rate
            DBG_SEGFAULT("Calling audioDeviceAboutToStart on track engines");
            for (size_t i = 0; i < trackEngines.size(); ++i)
            {
                DBG_SEGFAULT("Calling audioDeviceAboutToStart on track " + juce::String(i));
                trackEngines[i].audioDeviceAboutToStart(sampleRate);
                DBG_SEGFAULT("audioDeviceAboutToStart completed for track " + juce::String(i));
            }
            DBG_SEGFAULT("All track engines notified");
        }
        
        // Add audio callback now that setup is complete
        DBG_SEGFAULT("Adding audio callback");
        audioDeviceManager.addAudioCallback(this);
        DBG("Audio callback added to device manager - audio processing started");
        DBG_SEGFAULT("Audio callback added");
        
        // Verify device is running
        DBG_SEGFAULT("Verifying device");
        device = audioDeviceManager.getCurrentAudioDevice();
        if (device != nullptr)
        {
            DBG("Device check - IsOpen: " << (device->isOpen() ? "YES" : "NO")
                << " IsPlaying: " << (device->isPlaying() ? "YES" : "NO"));
        }
        DBG_SEGFAULT("EXIT: startAudio");
    }

private:
    static constexpr int numTracks = 8;
    static constexpr double maxBufferDurationSeconds = 10.0;

    std::array<TrackEngineType, 8> trackEngines;
    juce::AudioDeviceManager audioDeviceManager;
    std::atomic<double> currentSampleRate{44100.0};
};

// Include track engine headers for type aliases
#include "LooperTrackEngine.h"
#include "VampNetTrackEngine.h"

// Type aliases for convenience
using MultiTrackLooperEngine = MultiTrackLooperEngineTemplate<LooperTrackEngine>;
using VampNetMultiTrackLooperEngine = MultiTrackLooperEngineTemplate<VampNetTrackEngine>;
