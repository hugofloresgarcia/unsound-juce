#include "ConfigManager.h"

using namespace Shared;

ConfigManager::ConfigManager()
{
}

juce::File ConfigManager::getAppDataDirectory()
{
    auto appDataDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                        .getChildFile("TapeLooper");
    appDataDir.createDirectory();
    return appDataDir;
}

juce::File ConfigManager::getConfigFile(const juce::String& frontendName)
{
    return getAppDataDirectory().getChildFile(frontendName + "_config.xml");
}

std::unique_ptr<juce::XmlElement> ConfigManager::loadConfigXml(const juce::String& frontendName)
{
    auto configFile = getConfigFile(frontendName);
    
    if (configFile.existsAsFile())
    {
        auto xml = juce::XmlDocument::parse(configFile);
        if (xml != nullptr && xml->hasTagName("Config"))
        {
            return xml;
        }
    }
    
    // Create new config XML if file doesn't exist or is invalid
    return std::make_unique<juce::XmlElement>("Config");
}

bool ConfigManager::saveConfigXml(const juce::String& frontendName, juce::XmlElement* xml)
{
    if (xml == nullptr)
        return false;
    
    auto configFile = getConfigFile(frontendName);
    
    if (xml->writeTo(configFile))
    {
        DBG("ConfigManager: Saved config to: " + configFile.getFullPathName());
        return true;
    }
    else
    {
        DBG("ConfigManager: Failed to save config to: " + configFile.getFullPathName());
        return false;
    }
}

bool ConfigManager::saveStringValue(const juce::String& frontendName, const juce::String& key, const juce::String& value)
{
    auto xml = loadConfigXml(frontendName);
    if (xml == nullptr)
        return false;
    
    xml->setAttribute(key, value);
    return saveConfigXml(frontendName, xml.get());
}

juce::String ConfigManager::loadStringValue(const juce::String& frontendName, const juce::String& key, const juce::String& defaultValue)
{
    auto xml = loadConfigXml(frontendName);
    if (xml == nullptr)
        return defaultValue;
    
    return xml->getStringAttribute(key, defaultValue);
}

bool ConfigManager::saveIntValue(const juce::String& frontendName, const juce::String& key, int value)
{
    auto xml = loadConfigXml(frontendName);
    if (xml == nullptr)
        return false;
    
    xml->setAttribute(key, value);
    return saveConfigXml(frontendName, xml.get());
}

int ConfigManager::loadIntValue(const juce::String& frontendName, const juce::String& key, int defaultValue)
{
    auto xml = loadConfigXml(frontendName);
    if (xml == nullptr)
        return defaultValue;
    
    return xml->getIntAttribute(key, defaultValue);
}

bool ConfigManager::saveDoubleValue(const juce::String& frontendName, const juce::String& key, double value)
{
    auto xml = loadConfigXml(frontendName);
    if (xml == nullptr)
        return false;
    
    xml->setAttribute(key, value);
    return saveConfigXml(frontendName, xml.get());
}

double ConfigManager::loadDoubleValue(const juce::String& frontendName, const juce::String& key, double defaultValue)
{
    auto xml = loadConfigXml(frontendName);
    if (xml == nullptr)
        return defaultValue;
    
    return xml->getDoubleAttribute(key, defaultValue);
}

bool ConfigManager::saveBoolValue(const juce::String& frontendName, const juce::String& key, bool value)
{
    auto xml = loadConfigXml(frontendName);
    if (xml == nullptr)
        return false;
    
    xml->setAttribute(key, value ? "1" : "0");
    return saveConfigXml(frontendName, xml.get());
}

bool ConfigManager::loadBoolValue(const juce::String& frontendName, const juce::String& key, bool defaultValue)
{
    auto xml = loadConfigXml(frontendName);
    if (xml == nullptr)
        return defaultValue;
    
    juce::String valueStr = xml->getStringAttribute(key, defaultValue ? "1" : "0");
    return valueStr == "1" || valueStr.equalsIgnoreCase("true");
}

bool ConfigManager::removeValue(const juce::String& frontendName, const juce::String& key)
{
    auto xml = loadConfigXml(frontendName);
    if (xml == nullptr)
        return false;
    
    xml->removeAttribute(key);
    return saveConfigXml(frontendName, xml.get());
}

bool ConfigManager::hasValue(const juce::String& frontendName, const juce::String& key)
{
    auto xml = loadConfigXml(frontendName);
    if (xml == nullptr)
        return false;
    
    return xml->hasAttribute(key);
}

