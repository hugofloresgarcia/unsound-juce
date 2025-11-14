#include "MidiLearnManager.h"

using namespace Shared;

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
    parameterToCcMap.clear();
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
    auto it = parameterToCcMap.find(parameterId);
    if (it != parameterToCcMap.end())
    {
        int ccNumber = it->second;
        ccToParameterMap.erase(ccNumber);
        parameterToCcMap.erase(it);
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
    juce::Logger::writeToLog("MidiLearnManager: Started learning for: " + parameterId);
}

void MidiLearnManager::stopLearning()
{
    juce::ScopedLock lock(mapLock);
    learningParameterId = juce::String();
}

void MidiLearnManager::clearMapping(const juce::String& parameterId)
{
    juce::ScopedLock lock(mapLock);
    
    auto it = parameterToCcMap.find(parameterId);
    if (it != parameterToCcMap.end())
    {
        int ccNumber = it->second;
        ccToParameterMap.erase(ccNumber);
        parameterToCcMap.erase(it);
        juce::Logger::writeToLog("MidiLearnManager: Cleared mapping for: " + parameterId);
    }
}

void MidiLearnManager::clearAllMappings()
{
    juce::ScopedLock lock(mapLock);
    ccToParameterMap.clear();
    parameterToCcMap.clear();
    juce::Logger::writeToLog("MidiLearnManager: Cleared all mappings");
}

std::vector<MidiMapping> MidiLearnManager::getAllMappings() const
{
    juce::ScopedLock lock(mapLock);
    
    std::vector<MidiMapping> mappings;
    for (const auto& pair : parameterToCcMap)
    {
        mappings.push_back({pair.second, pair.first});
    }
    return mappings;
}

int MidiLearnManager::getMappingForParameter(const juce::String& parameterId) const
{
    juce::ScopedLock lock(mapLock);
    
    auto it = parameterToCcMap.find(parameterId);
    if (it != parameterToCcMap.end())
        return it->second;
    return -1;
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
        midiInput->stop();
        midiInput.reset();
    }
    
    auto devices = juce::MidiInput::getAvailableDevices();
    if (deviceIndex >= 0 && deviceIndex < devices.size())
    {
        midiInput = juce::MidiInput::openDevice(devices[deviceIndex].identifier, this);
        if (midiInput)
        {
            midiInput->start();
            midiEnabled = true;
            juce::Logger::writeToLog("MidiLearnManager: Opened MIDI device: " + devices[deviceIndex].name);
        }
        else
        {
            juce::Logger::writeToLog("MidiLearnManager: Failed to open MIDI device");
        }
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
    
    for (const auto& pair : parameterToCcMap)
    {
        auto* mapping = root.createNewChildElement("Mapping");
        mapping->setAttribute("parameterId", pair.first);
        mapping->setAttribute("ccNumber", pair.second);
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
    parameterToCcMap.clear();
    
    for (auto* mapping : xml->getChildWithTagNameIterator("Mapping"))
    {
        juce::String parameterId = mapping->getStringAttribute("parameterId");
        int ccNumber = mapping->getIntAttribute("ccNumber");
        
        // Only restore mapping if parameter still exists
        if (parameters.find(parameterId) != parameters.end())
        {
            ccToParameterMap[ccNumber] = parameterId;
            parameterToCcMap[parameterId] = ccNumber;
        }
    }
    
    juce::Logger::writeToLog("MidiLearnManager: Loaded " + juce::String(parameterToCcMap.size()) + " mappings");
}

void MidiLearnManager::handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& message)
{
    // Early exit if we're shutting down
    if (!midiEnabled)
        return;
    
    if (message.isController())
    {
        int ccNumber = message.getControllerNumber();
        int ccValue = message.getControllerValue();
        
        juce::ScopedLock lock(mapLock);
        
        // If in learn mode, assign this CC to the learning parameter
        if (learningParameterId.isNotEmpty())
        {
            // Remove old mapping for this CC if it exists
            auto oldParamIt = ccToParameterMap.find(ccNumber);
            if (oldParamIt != ccToParameterMap.end())
            {
                parameterToCcMap.erase(oldParamIt->second);
            }
            
            // Remove old CC mapping for this parameter if it exists
            auto oldCcIt = parameterToCcMap.find(learningParameterId);
            if (oldCcIt != parameterToCcMap.end())
            {
                ccToParameterMap.erase(oldCcIt->second);
            }
            
            // Create new mapping
            ccToParameterMap[ccNumber] = learningParameterId;
            parameterToCcMap[learningParameterId] = ccNumber;
            
            juce::Logger::writeToLog("MidiLearnManager: Mapped CC " + juce::String(ccNumber) + 
                                    " to " + learningParameterId);
            
            // Notify callback
            if (onParameterLearned)
            {
                juce::MessageManager::callAsync([this, paramId = learningParameterId, cc = ccNumber]()
                {
                    if (midiEnabled && onParameterLearned)
                        onParameterLearned(paramId, cc);
                });
            }
            
            learningParameterId = juce::String();
        }
        else
        {
            // Normal mode - process CC messages
            processControlChange(ccNumber, ccValue);
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
    
    const juce::String& parameterId = it->second;
    auto paramIt = parameters.find(parameterId);
    if (paramIt == parameters.end())
        return;
    
    const MidiLearnableParameter& param = paramIt->second;
    
    // Convert MIDI value (0-127) to normalized value (0.0-1.0)
    float normalizedValue = ccValue / 127.0f;
    
    // For toggle parameters, treat values > 64 as "on"
    if (param.isToggle)
    {
        normalizedValue = ccValue > 64 ? 1.0f : 0.0f;
    }
    
    // Update parameter on message thread, with safety check
    juce::MessageManager::callAsync([this, setValue = param.setValue, normalizedValue]()
    {
        if (midiEnabled && setValue)
            setValue(normalizedValue);
    });
}

