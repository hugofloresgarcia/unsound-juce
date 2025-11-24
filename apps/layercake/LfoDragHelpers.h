#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace LayerCakeApp::LfoDragHelpers
{

inline juce::var make_description(int lfo_index, juce::Colour accent, const juce::String& label)
{
    auto* payload = new juce::DynamicObject();
    payload->setProperty("dragType", "layercake.lfo");
    payload->setProperty("index", lfo_index);
    payload->setProperty("colour", static_cast<int>(accent.getARGB()));
    payload->setProperty("label", label);
    return juce::var(payload);
}

inline bool parse_description(const juce::var& description,
                              int& lfo_index,
                              juce::Colour& accent,
                              juce::String& label,
                              bool log_on_failure = false)
{
    if (!description.isObject())
    {
        if (log_on_failure)
            DBG("LfoDragHelpers::parse_description early return (non-object payload)");
        return false;
    }

    auto* object = description.getDynamicObject();
    if (object == nullptr)
    {
        if (log_on_failure)
            DBG("LfoDragHelpers::parse_description early return (missing dynamic object)");
        return false;
    }

    const auto type = object->getProperty("dragType").toString();
    if (type != "layercake.lfo")
    {
        if (log_on_failure)
            DBG("LfoDragHelpers::parse_description early return (unexpected type=" + type + ")");
        return false;
    }

    lfo_index = static_cast<int>(object->getProperty("index"));
    const auto colour_value = static_cast<uint32_t>(static_cast<int>(object->getProperty("colour")));
    accent = juce::Colour(colour_value);
    label = object->getProperty("label").toString();
    return true;
}

} // namespace LayerCakeApp::LfoDragHelpers



