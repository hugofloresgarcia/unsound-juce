#include "SessionConfig.h"

namespace
{
juce::String toString(Shared::MidiMapping::MessageType type)
{
    return Shared::MidiMapping::getTypeName(type);
}

Shared::MidiMapping::MessageType messageTypeFromString(const juce::String& type)
{
    return type.equalsIgnoreCase("note")
               ? Shared::MidiMapping::MessageType::Note
               : Shared::MidiMapping::MessageType::CC;
}

juce::String toString(Shared::MidiMapping::Mode mode)
{
    return mode == Shared::MidiMapping::Mode::Toggle ? "toggle" : "momentary";
}

Shared::MidiMapping::Mode modeFromString(const juce::String& mode)
{
    return mode.equalsIgnoreCase("toggle")
               ? Shared::MidiMapping::Mode::Toggle
               : Shared::MidiMapping::Mode::Momentary;
}
} // namespace

namespace WhAM
{

juce::var SessionConfig::toVar() const
{
    juce::DynamicObject::Ptr root = new juce::DynamicObject();
    root->setProperty("gradioUrl", gradioUrl);

    juce::Array<juce::var> trackArray;
    trackArray.ensureStorageAllocated(static_cast<int>(tracks.size()));
    for (const auto& track : tracks)
    {
        juce::DynamicObject::Ptr trackObj = new juce::DynamicObject();
        trackObj->setProperty("index", track.trackIndex);
        trackObj->setProperty("knobs", track.knobState);
        trackObj->setProperty("vampParams", track.vampNetParams);
        trackObj->setProperty("autogen", track.autogenEnabled);
        trackObj->setProperty("useOutputAsInput", track.useOutputAsInput);
        trackObj->setProperty("levelDb", track.levelDb);
        trackObj->setProperty("panner", track.pannerState);
        trackObj->setProperty("inputChannel", track.inputChannel);
        trackObj->setProperty("outputChannel", track.outputChannel);
        trackObj->setProperty("micEnabled", track.micEnabled);
        trackObj->setProperty("inputAudio", track.inputAudioFile);
        trackObj->setProperty("outputAudio", track.outputAudioFile);
        trackObj->setProperty("highPassHz", track.highPassHz);
        trackObj->setProperty("lowPassHz", track.lowPassHz);
        trackArray.add(juce::var(trackObj));
    }
    root->setProperty("tracks", juce::var(trackArray));

    juce::Array<juce::var> mappingArray;
    mappingArray.ensureStorageAllocated(static_cast<int>(midiMappings.size()));
    for (const auto& mapping : midiMappings)
    {
        juce::DynamicObject::Ptr mappingObj = new juce::DynamicObject();
        mappingObj->setProperty("parameterId", mapping.parameterId);
        mappingObj->setProperty("messageType", toString(mapping.type));
        mappingObj->setProperty("messageNumber", mapping.number);
        mappingObj->setProperty("mode", toString(mapping.mode));
        mappingArray.add(juce::var(mappingObj));
    }
    root->setProperty("midiMappings", juce::var(mappingArray));

    // Optional synth state (may be missing in older configs)
    if (synthState.isObject())
        root->setProperty("synth", synthState);

    return juce::var(root);
}

juce::Result SessionConfig::fromVar(const juce::var& data, SessionConfig& out)
{
    if (!data.isObject())
        return juce::Result::fail("Config data is not an object");

    auto* rootObj = data.getDynamicObject();
    if (rootObj == nullptr)
        return juce::Result::fail("Invalid config object");

    SessionConfig config;
    config.gradioUrl = rootObj->getProperty("gradioUrl").toString();

    // Optional synth state (may be missing in older configs)
    config.synthState = rootObj->getProperty("synth");

    auto tracksVar = rootObj->getProperty("tracks");
    if (tracksVar.isArray())
    {
        auto* arr = tracksVar.getArray();
        for (const auto& item : *arr)
        {
            if (!item.isObject())
                continue;

            TrackState state;
            auto* obj = item.getDynamicObject();
            state.trackIndex = static_cast<int>(obj->getProperty("index"));
            state.knobState = obj->getProperty("knobs");
            state.vampNetParams = obj->getProperty("vampParams");
            state.autogenEnabled = obj->getProperty("autogen");
            state.useOutputAsInput = obj->getProperty("useOutputAsInput");
            state.levelDb = static_cast<double>(obj->getProperty("levelDb"));
            state.pannerState = obj->getProperty("panner");
            auto inputVar = obj->getProperty("inputChannel");
            state.inputChannel = inputVar.isVoid() ? -1 : static_cast<int>(inputVar);

            auto outputVar = obj->getProperty("outputChannel");
            state.outputChannel = outputVar.isVoid() ? -1 : static_cast<int>(outputVar);

            auto micVar = obj->getProperty("micEnabled");
            state.micEnabled = micVar.isVoid() ? true : static_cast<bool>(micVar);
            state.inputAudioFile = obj->getProperty("inputAudio").toString();
            state.outputAudioFile = obj->getProperty("outputAudio").toString();
            auto hpVar = obj->getProperty("highPassHz");
            if (!hpVar.isVoid())
                state.highPassHz = static_cast<double>(hpVar);

            auto lpVar = obj->getProperty("lowPassHz");
            if (!lpVar.isVoid())
                state.lowPassHz = static_cast<double>(lpVar);
            config.tracks.push_back(state);
        }
    }

    auto mappingsVar = rootObj->getProperty("midiMappings");
    if (mappingsVar.isArray())
    {
        auto* arr = mappingsVar.getArray();
        for (const auto& item : *arr)
        {
            if (!item.isObject())
                continue;

            auto* obj = item.getDynamicObject();
            Shared::MidiMapping mapping;
            mapping.parameterId = obj->getProperty("parameterId").toString();
            mapping.type = messageTypeFromString(obj->getProperty("messageType").toString());
            mapping.number = static_cast<int>(obj->getProperty("messageNumber"));
            mapping.mode = modeFromString(obj->getProperty("mode").toString());
            if (mapping.isValid())
                config.midiMappings.push_back(mapping);
        }
    }

    out = config;
    return juce::Result::ok();
}

juce::Result SessionConfig::saveToFile(const juce::File& file) const
{
    auto data = toVar();
    juce::String json = juce::JSON::toString(data, true);
    if (file.replaceWithText(json))
        return juce::Result::ok();
    return juce::Result::fail("Failed to write config");
}

juce::Result SessionConfig::loadFromFile(const juce::File& file, SessionConfig& out)
{
    if (!file.existsAsFile())
        return juce::Result::fail("Config file not found");

    auto jsonText = file.loadFileAsString();
    if (jsonText.isEmpty())
        return juce::Result::fail("Config file is empty or unreadable");

    juce::var parsed = juce::JSON::parse(jsonText);
    if (parsed.isVoid())
        return juce::Result::fail("Unable to parse config JSON");

    auto result = fromVar(parsed, out);
    if (result.wasOk())
        out.audioDirectory = file.getParentDirectory();
    return result;
}

} // namespace WhAM


