#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <map>
#include <functional>
#include <vector>

namespace Shared
{

// Stores a MIDI message to parameter mapping
struct MidiMapping
{
    enum class MessageType
    {
        CC,
        Note
    };

    enum class Mode
    {
        Momentary,
        Toggle
    };

    MessageType type = MessageType::CC;
    int number = -1;
    juce::String parameterId;
    Mode mode = Mode::Momentary;
    
    bool operator==(const MidiMapping& other) const
    {
        return type == other.type &&
               number == other.number &&
               parameterId == other.parameterId &&
               mode == other.mode;
    }

    bool isValid() const
    {
        return number >= 0 && parameterId.isNotEmpty();
    }

    static juce::String getTypeName(MessageType messageType)
    {
        return messageType == MessageType::CC ? "CC" : "Note";
    }
};

// Represents a mappable parameter that can be controlled via MIDI
struct MidiLearnableParameter
{
    juce::String id;  // Unique identifier (e.g., "track0_level", "track0_play")
    std::function<void(float)> setValue;  // Callback to set value (0.0-1.0)
    std::function<float()> getValue;      // Callback to get current value (0.0-1.0)
    juce::String displayName;             // Human-readable name for UI
    bool isToggle;                        // True for buttons, false for continuous controls
    bool allowToggleMode = false;         // Whether MIDI learn menu should offer toggle mappings
    MidiMapping::Mode defaultMode = MidiMapping::Mode::Momentary;
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
    void startLearning(const juce::String& parameterId, MidiMapping::Mode mode = MidiMapping::Mode::Momentary);
    
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
    
    // Get mapping for a specific parameter (number < 0 if not mapped)
    MidiMapping getMappingForParameter(const juce::String& parameterId) const;
    
    // Enable/disable MIDI input
    void setMidiInputEnabled(bool enabled);
    
    // Set which MIDI input device to use (-1 for none)
    void setMidiInputDevice(int deviceIndex);
    
    // Get available MIDI input devices
    juce::StringArray getAvailableMidiDevices() const;
    
    // Save/load mappings to/from file
    void saveMappings(const juce::File& file);
    void loadMappings(const juce::File& file);
    void applyMappings(const std::vector<MidiMapping>& mappings);
    
    // MidiInputCallback interface
    void handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& message) override;
    
    // Callback for when a parameter is learned
    std::function<void(const MidiMapping& mapping)> onParameterLearned;
    
private:
    struct MidiAssignment
    {
        MidiMapping::MessageType type = MidiMapping::MessageType::CC;
        int number = -1;
        MidiMapping::Mode mode = MidiMapping::Mode::Momentary;
    };

    std::map<juce::String, MidiLearnableParameter> parameters;
    std::map<int, std::vector<juce::String>> ccToParameterMap;    // CC number -> parameter IDs
    std::map<int, std::vector<juce::String>> noteToParameterMap;  // Note number -> parameter IDs
    std::map<juce::String, MidiAssignment> parameterToMessageMap;  // parameter ID -> assignment
    
    juce::String learningParameterId;
    MidiMapping::Mode learningMode = MidiMapping::Mode::Momentary;
    std::unique_ptr<juce::MidiInput> midiInput;
    bool midiEnabled = false;
    
    juce::CriticalSection mapLock;
    
    void processControlChange(int ccNumber, int ccValue);
    void processNoteMessage(int noteNumber, bool isNoteOn);
    void handleToggleForParameter(const juce::String& parameterId);
    void applyParameterValue(const juce::String& parameterId, float normalizedValue);
    void storeMappingLocked(const juce::String& parameterId, MidiMapping::MessageType type, int number, MidiMapping::Mode mode);
    static void addParameterToMessageMap(std::map<int, std::vector<juce::String>>& map, int number, const juce::String& parameterId);
    static void removeParameterFromMessageMap(std::map<int, std::vector<juce::String>>& map, int number, const juce::String& parameterId);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiLearnManager)
};

} // namespace Shared

