#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <map>
#include <functional>

namespace Shared
{

// Represents a mappable parameter that can be controlled via MIDI
struct MidiLearnableParameter
{
    juce::String id;  // Unique identifier (e.g., "track0_level", "track0_play")
    std::function<void(float)> setValue;  // Callback to set value (0.0-1.0)
    std::function<float()> getValue;      // Callback to get current value (0.0-1.0)
    juce::String displayName;             // Human-readable name for UI
    bool isToggle;                        // True for buttons, false for continuous controls
};

// Stores a MIDI CC to parameter mapping
struct MidiMapping
{
    int ccNumber;
    juce::String parameterId;
    
    bool operator==(const MidiMapping& other) const
    {
        return ccNumber == other.ccNumber && parameterId == other.parameterId;
    }
};

/**
 * Manages MIDI learn functionality for the application.
 * Allows users to assign MIDI CC messages to UI controls.
 */
class MidiLearnManager : public juce::MidiInputCallback
{
public:
    MidiLearnManager();
    ~MidiLearnManager() override;
    
    // Register a parameter that can be learned
    void registerParameter(const MidiLearnableParameter& param);
    
    // Unregister a parameter (e.g., when a track is removed)
    void unregisterParameter(const juce::String& parameterId);
    
    // Start MIDI learn mode for a specific parameter
    void startLearning(const juce::String& parameterId);
    
    // Stop MIDI learn mode
    void stopLearning();
    
    // Check if we're in learn mode
    bool isLearning() const { return learningParameterId.isNotEmpty(); }
    
    // Get the parameter currently being learned
    juce::String getLearningParameterId() const { return learningParameterId; }
    
    // Clear a specific mapping
    void clearMapping(const juce::String& parameterId);
    
    // Clear all mappings
    void clearAllMappings();
    
    // Get all current mappings
    std::vector<MidiMapping> getAllMappings() const;
    
    // Get mapping for a specific parameter (returns -1 if not mapped)
    int getMappingForParameter(const juce::String& parameterId) const;
    
    // Enable/disable MIDI input
    void setMidiInputEnabled(bool enabled);
    
    // Set which MIDI input device to use (-1 for none)
    void setMidiInputDevice(int deviceIndex);
    
    // Get available MIDI input devices
    juce::StringArray getAvailableMidiDevices() const;
    
    // Save/load mappings to/from file
    void saveMappings(const juce::File& file);
    void loadMappings(const juce::File& file);
    
    // MidiInputCallback interface
    void handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& message) override;
    
    // Callback for when a parameter is learned
    std::function<void(const juce::String& parameterId, int ccNumber)> onParameterLearned;
    
private:
    std::map<juce::String, MidiLearnableParameter> parameters;
    std::map<int, juce::String> ccToParameterMap;  // CC number -> parameter ID
    std::map<juce::String, int> parameterToCcMap;  // parameter ID -> CC number
    std::map<juce::String, int> pendingMappings;   // parameter ID -> CC number (for parameters not yet registered)
    
    juce::String learningParameterId;
    std::unique_ptr<juce::MidiInput> midiInput;
    bool midiEnabled = false;
    
    juce::CriticalSection mapLock;
    
    void processControlChange(int ccNumber, int ccValue);
    void restorePendingMapping(const juce::String& parameterId);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiLearnManager)
};

} // namespace Shared

