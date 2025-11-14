#include "MidiLearnManager.h"

using namespace Shared;

namespace
{
juce::String toAttributeTypeString(MidiMapping::MessageType type)
{
    return MidiMapping::getTypeName(type);
}

MidiMapping::MessageType messageTypeFromAttribute(const juce::String& typeName)
{
    return typeName.equalsIgnoreCase("note")
               ? MidiMapping::MessageType::Note
               : MidiMapping::MessageType::CC;
}
}

MidiLearnManager::MidiLearnManager()
{
}

MidiLearnManager::~MidiLearnManager()
{
    // Stop MIDI input first to prevent callbacks during destruction
    if (midiInput)
    {
        midiInput->stop();
        midiInput.reset();
    }
    midiEnabled = false;
    
    // Clear all mappings and parameters to prevent any callbacks
    juce::ScopedLock lock(mapLock);
    ccToParameterMap.clear();
    noteToParameterMap.clear();
    parameterToMessageMap.clear();
    parameters.clear();
    learningParameterId = juce::String();
}

void MidiLearnManager::registerParameter(const MidiLearnableParameter& param)
{
    juce::ScopedLock lock(mapLock);
    parameters[param.id] = param;
}

void MidiLearnManager::unregisterParameter(const juce::String& parameterId)
{
    juce::ScopedLock lock(mapLock);
    
    // Remove from parameters
    parameters.erase(parameterId);
    
    // Remove any mapping for this parameter
    auto it = parameterToMessageMap.find(parameterId);
    if (it != parameterToMessageMap.end())
    {
        if (it->second.type == MidiMapping::MessageType::CC)
            ccToParameterMap.erase(it->second.number);
        else
            noteToParameterMap.erase(it->second.number);

        parameterToMessageMap.erase(it);
    }
}

void MidiLearnManager::startLearning(const juce::String& parameterId)
{
    juce::ScopedLock lock(mapLock);
    
    // Check if parameter exists
    if (parameters.find(parameterId) == parameters.end())
    {
        juce::Logger::writeToLog("MidiLearnManager: Cannot learn unknown parameter: " + parameterId);
        return;
    }
    
    learningParameterId = parameterId;
    juce::String deviceName = midiInput ? midiInput->getName() : "No device";
    juce::Logger::writeToLog("MidiLearnManager: Started learning for: " + parameterId + " (MIDI device: " + deviceName + ", enabled: " + (midiEnabled ? "Yes" : "No") + ")");
}

void MidiLearnManager::stopLearning()
{
    juce::ScopedLock lock(mapLock);
    if (learningParameterId.isNotEmpty())
    {
        juce::Logger::writeToLog("MidiLearnManager: Stopped learning for: " + learningParameterId);
    }
    learningParameterId = juce::String();
}

void MidiLearnManager::clearMapping(const juce::String& parameterId)
{
    juce::ScopedLock lock(mapLock);
    
    auto it = parameterToMessageMap.find(parameterId);
    if (it != parameterToMessageMap.end())
    {
        if (it->second.type == MidiMapping::MessageType::CC)
            ccToParameterMap.erase(it->second.number);
        else
            noteToParameterMap.erase(it->second.number);

        juce::Logger::writeToLog(
            "MidiLearnManager: Cleared mapping for: " + parameterId + " (" +
            MidiMapping::getTypeName(it->second.type) + " " + juce::String(it->second.number) + ")");
        parameterToMessageMap.erase(it);
    }
}

void MidiLearnManager::clearAllMappings()
{
    juce::ScopedLock lock(mapLock);
    ccToParameterMap.clear();
    noteToParameterMap.clear();
    parameterToMessageMap.clear();
    juce::Logger::writeToLog("MidiLearnManager: Cleared all mappings");
}

std::vector<MidiMapping> MidiLearnManager::getAllMappings() const
{
    juce::ScopedLock lock(mapLock);
    
    std::vector<MidiMapping> mappings;
    for (const auto& pair : parameterToMessageMap)
    {
        MidiMapping mapping;
        mapping.parameterId = pair.first;
        mapping.type = pair.second.type;
        mapping.number = pair.second.number;
        mappings.push_back(mapping);
    }
    return mappings;
}

MidiMapping MidiLearnManager::getMappingForParameter(const juce::String& parameterId) const
{
    juce::ScopedLock lock(mapLock);
    
    MidiMapping mapping;
    mapping.parameterId = parameterId;

    auto it = parameterToMessageMap.find(parameterId);
    if (it != parameterToMessageMap.end())
    {
        mapping.type = it->second.type;
        mapping.number = it->second.number;
    }
    else
    {
        mapping.number = -1;
    }
    return mapping;
}

void MidiLearnManager::setMidiInputEnabled(bool enabled)
{
    if (enabled && !midiEnabled)
    {
        // Try to open first available MIDI device
        auto devices = juce::MidiInput::getAvailableDevices();
        if (!devices.isEmpty())
        {
            setMidiInputDevice(0);
        }
    }
    else if (!enabled && midiEnabled)
    {
        if (midiInput)
        {
            midiInput->stop();
            midiInput.reset();
        }
        midiEnabled = false;
    }
}

void MidiLearnManager::setMidiInputDevice(int deviceIndex)
{
    // Stop current input
    if (midiInput)
    {
        juce::Logger::writeToLog("MidiLearnManager: Closing MIDI device: " + midiInput->getName());
        midiInput->stop();
        midiInput.reset();
    }
    
    auto devices = juce::MidiInput::getAvailableDevices();
    juce::Logger::writeToLog("MidiLearnManager: Available MIDI devices: " + juce::String(devices.size()));
    for (int i = 0; i < devices.size(); ++i)
    {
        juce::Logger::writeToLog("  [" + juce::String(i) + "] " + devices[i].name + " (ID: " + devices[i].identifier + ")");
    }
    
    if (deviceIndex >= 0 && deviceIndex < devices.size())
    {
        juce::Logger::writeToLog("MidiLearnManager: Attempting to open device index " + juce::String(deviceIndex) + ": " + devices[deviceIndex].name);
        midiInput = juce::MidiInput::openDevice(devices[deviceIndex].identifier, this);
        if (midiInput)
        {
            midiInput->start();
            midiEnabled = true;
            juce::Logger::writeToLog("MidiLearnManager: Successfully opened and started MIDI device: " + devices[deviceIndex].name);
        }
        else
        {
            juce::Logger::writeToLog("MidiLearnManager: Failed to open MIDI device: " + devices[deviceIndex].name);
            midiEnabled = false;
        }
    }
    else
    {
        juce::Logger::writeToLog("MidiLearnManager: Invalid device index: " + juce::String(deviceIndex));
        midiEnabled = false;
    }
}

juce::StringArray MidiLearnManager::getAvailableMidiDevices() const
{
    juce::StringArray deviceNames;
    auto devices = juce::MidiInput::getAvailableDevices();
    for (const auto& device : devices)
        deviceNames.add(device.name);
    return deviceNames;
}

void MidiLearnManager::saveMappings(const juce::File& file)
{
    juce::ScopedLock lock(mapLock);
    
    juce::XmlElement root("MidiMappings");
    
    for (const auto& pair : parameterToMessageMap)
    {
        auto* mapping = root.createNewChildElement("Mapping");
        mapping->setAttribute("parameterId", pair.first);
        mapping->setAttribute("messageNumber", pair.second.number);
        mapping->setAttribute("ccNumber", pair.second.number); // Backward compatibility
        mapping->setAttribute("messageType", toAttributeTypeString(pair.second.type));
    }
    
    if (root.writeTo(file))
    {
        juce::Logger::writeToLog("MidiLearnManager: Saved mappings to: " + file.getFullPathName());
    }
    else
    {
        juce::Logger::writeToLog("MidiLearnManager: Failed to save mappings");
    }
}

void MidiLearnManager::loadMappings(const juce::File& file)
{
    if (!file.existsAsFile())
        return;
    
    auto xml = juce::XmlDocument::parse(file);
    if (!xml)
    {
        juce::Logger::writeToLog("MidiLearnManager: Failed to parse mappings file");
        return;
    }
    
    juce::ScopedLock lock(mapLock);
    
    ccToParameterMap.clear();
    noteToParameterMap.clear();
    parameterToMessageMap.clear();
    
    for (auto* mapping : xml->getChildWithTagNameIterator("Mapping"))
    {
        juce::String parameterId = mapping->getStringAttribute("parameterId");
        int number = mapping->getIntAttribute("messageNumber", mapping->getIntAttribute("ccNumber", -1));
        juce::String typeName = mapping->getStringAttribute("messageType", "CC");
        auto messageType = messageTypeFromAttribute(typeName);
        
        if (number < 0)
            continue;

        // Only restore mapping if parameter still exists
        if (parameters.find(parameterId) != parameters.end())
        {
            if (messageType == MidiMapping::MessageType::CC)
                ccToParameterMap[number] = parameterId;
            else
                noteToParameterMap[number] = parameterId;

            parameterToMessageMap[parameterId] = {messageType, number};
        }
    }
    
    juce::Logger::writeToLog("MidiLearnManager: Loaded " + juce::String(parameterToMessageMap.size()) + " mappings");
}

void MidiLearnManager::handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& message)
{
    // Early exit if we're shutting down
    if (!midiEnabled)
        return;
    
    // Log all incoming MIDI messages for debugging
    juce::String deviceName = source ? source->getName() : "Unknown";
    juce::String messageType;
    juce::String messageDetails;
    
    if (message.isController())
    {
        messageType = "CC";
        int ccNumber = message.getControllerNumber();
        int ccValue = message.getControllerValue();
        int channel = message.getChannel();
        messageDetails = "CC=" + juce::String(ccNumber) + " Value=" + juce::String(ccValue) + " Ch=" + juce::String(channel);
    }
    else if (message.isNoteOn())
    {
        messageType = "NoteOn";
        int note = message.getNoteNumber();
        int velocity = message.getVelocity();
        int channel = message.getChannel();
        messageDetails = "Note=" + juce::String(note) + " Vel=" + juce::String(velocity) + " Ch=" + juce::String(channel);
    }
    else if (message.isNoteOff())
    {
        messageType = "NoteOff";
        int note = message.getNoteNumber();
        int velocity = message.getVelocity();
        int channel = message.getChannel();
        messageDetails = "Note=" + juce::String(note) + " Vel=" + juce::String(velocity) + " Ch=" + juce::String(channel);
    }
    else if (message.isPitchWheel())
    {
        messageType = "PitchBend";
        int value = message.getPitchWheelValue();
        int channel = message.getChannel();
        messageDetails = "Value=" + juce::String(value) + " Ch=" + juce::String(channel);
    }
    else if (message.isAftertouch())
    {
        messageType = "Aftertouch";
        int value = message.getAfterTouchValue();
        int channel = message.getChannel();
        messageDetails = "Value=" + juce::String(value) + " Ch=" + juce::String(channel);
    }
    else if (message.isChannelPressure())
    {
        messageType = "ChannelPressure";
        int value = message.getChannelPressureValue();
        int channel = message.getChannel();
        messageDetails = "Value=" + juce::String(value) + " Ch=" + juce::String(channel);
    }
    else if (message.isProgramChange())
    {
        messageType = "ProgramChange";
        int program = message.getProgramChangeNumber();
        int channel = message.getChannel();
        messageDetails = "Program=" + juce::String(program) + " Ch=" + juce::String(channel);
    }
    else if (message.isSysEx())
    {
        messageType = "SysEx";
        int size = message.getSysExDataSize();
        messageDetails = "Size=" + juce::String(size) + " bytes";
    }
    else if (message.isMidiClock())
    {
        messageType = "Clock";
        messageDetails = "";
    }
    else if (message.isMidiStart())
    {
        messageType = "Start";
        messageDetails = "";
    }
    else if (message.isMidiStop())
    {
        messageType = "Stop";
        messageDetails = "";
    }
    else if (message.isMidiContinue())
    {
        messageType = "Continue";
        messageDetails = "";
    }
    else if (message.isActiveSense())
    {
        messageType = "ActiveSense";
        messageDetails = "";
    }
    else
    {
        messageType = "Unknown";
        int rawData[3] = {0, 0, 0};
        int numBytes = message.getRawDataSize();
        if (numBytes > 0) rawData[0] = message.getRawData()[0];
        if (numBytes > 1) rawData[1] = message.getRawData()[1];
        if (numBytes > 2) rawData[2] = message.getRawData()[2];
        messageDetails = "Raw=[" + juce::String(rawData[0]) + "," + juce::String(rawData[1]) + "," + juce::String(rawData[2]) + "] Size=" + juce::String(numBytes);
    }
    
    juce::String logMessage = "[MIDI] Device: " + deviceName + " | Type: " + messageType;
    if (messageDetails.isNotEmpty())
        logMessage += " | " + messageDetails;
    logMessage += " | Learning: " + (learningParameterId.isNotEmpty() ? learningParameterId : "No");
    juce::Logger::writeToLog(logMessage);
    
    bool velocityIsZeroNoteOn = message.isNoteOn() && message.getVelocity() == 0;
    bool isEffectiveNoteOn = message.isNoteOn() && !velocityIsZeroNoteOn;
    bool isEffectiveNoteOff = message.isNoteOff() || velocityIsZeroNoteOn;
    
    if (message.isController())
    {
        int ccNumber = message.getControllerNumber();
        int ccValue = message.getControllerValue();
        
        juce::ScopedLock lock(mapLock);
        
        if (learningParameterId.isNotEmpty())
        {
            storeMappingLocked(learningParameterId, MidiMapping::MessageType::CC, ccNumber);
            learningParameterId = juce::String();
        }
        else
        {
            processControlChange(ccNumber, ccValue);
        }
    }
    else if (isEffectiveNoteOn || isEffectiveNoteOff)
    {
        int noteNumber = message.getNoteNumber();
        juce::ScopedLock lock(mapLock);
        
        if (learningParameterId.isNotEmpty())
        {
            if (isEffectiveNoteOn)
            {
                storeMappingLocked(learningParameterId, MidiMapping::MessageType::Note, noteNumber);
                learningParameterId = juce::String();
            }
        }
        else
        {
            processNoteMessage(noteNumber, isEffectiveNoteOn);
        }
    }
}

void MidiLearnManager::processControlChange(int ccNumber, int ccValue)
{
    // mapLock should already be held by caller
    
    // Early exit if we're shutting down
    if (!midiEnabled)
        return;
    
    auto it = ccToParameterMap.find(ccNumber);
    if (it == ccToParameterMap.end())
        return;
    
    // Convert MIDI value (0-127) to normalized value (0.0-1.0)
    float normalizedValue = ccValue / 127.0f;
    
    applyParameterValue(it->second, normalizedValue);
}

void MidiLearnManager::processNoteMessage(int noteNumber, bool isNoteOn)
{
    // mapLock should already be held by caller
    
    if (!midiEnabled)
        return;
    
    auto it = noteToParameterMap.find(noteNumber);
    if (it == noteToParameterMap.end())
        return;
    
    float normalizedValue = isNoteOn ? 1.0f : 0.0f;
    applyParameterValue(it->second, normalizedValue);
}

void MidiLearnManager::applyParameterValue(const juce::String& parameterId, float normalizedValue)
{
    auto paramIt = parameters.find(parameterId);
    if (paramIt == parameters.end())
        return;
    
    const MidiLearnableParameter& param = paramIt->second;
    float valueToSend = normalizedValue;
    
    if (param.isToggle)
        valueToSend = normalizedValue > 0.5f ? 1.0f : 0.0f;
    
    juce::MessageManager::callAsync([this, setValue = param.setValue, valueToSend]()
    {
        if (midiEnabled && setValue)
            setValue(valueToSend);
    });
}

void MidiLearnManager::storeMappingLocked(const juce::String& parameterId,
                                          MidiMapping::MessageType type,
                                          int number)
{
    // Remove any existing mapping tied to this message number
    if (type == MidiMapping::MessageType::CC)
    {
        auto oldParamIt = ccToParameterMap.find(number);
        if (oldParamIt != ccToParameterMap.end())
        {
            parameterToMessageMap.erase(oldParamIt->second);
        }
        ccToParameterMap[number] = parameterId;
    }
    else
    {
        auto oldParamIt = noteToParameterMap.find(number);
        if (oldParamIt != noteToParameterMap.end())
        {
            parameterToMessageMap.erase(oldParamIt->second);
        }
        noteToParameterMap[number] = parameterId;
    }
    
    // Remove the previous assignment for this parameter, regardless of type
    auto parameterAssignment = parameterToMessageMap.find(parameterId);
    if (parameterAssignment != parameterToMessageMap.end())
    {
        if (parameterAssignment->second.type == MidiMapping::MessageType::CC)
            ccToParameterMap.erase(parameterAssignment->second.number);
        else
            noteToParameterMap.erase(parameterAssignment->second.number);
    }
    
    parameterToMessageMap[parameterId] = {type, number};
    
    juce::Logger::writeToLog("MidiLearnManager: Mapped " + MidiMapping::getTypeName(type) +
                             " " + juce::String(number) + " to " + parameterId);
    
    if (onParameterLearned)
    {
        MidiMapping mapping;
        mapping.parameterId = parameterId;
        mapping.type = type;
        mapping.number = number;
        
        juce::MessageManager::callAsync([this, mapping]()
        {
            if (midiEnabled && onParameterLearned)
                onParameterLearned(mapping);
        });
    }
}

