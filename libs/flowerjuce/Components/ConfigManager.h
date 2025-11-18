#pragma once

#include <juce_core/juce_core.h>

namespace Shared
{

/**
 * Manages saving and loading application configuration settings.
 * Provides a common interface for persisting settings like URLs, preferences, etc.
 */
class ConfigManager
{
public:
    ConfigManager();
    ~ConfigManager() = default;
    
    /**
     * Get the config file path for a given frontend name.
     * @param frontendName Name of the frontend (e.g., "text2sound", "vampnet")
     * @return File path to the config file
     */
    static juce::File getConfigFile(const juce::String& frontendName);
    
    /**
     * Save a string value to the config file.
     * @param frontendName Name of the frontend
     * @param key Key name for the setting
     * @param value Value to save
     * @return true if successful, false otherwise
     */
    static bool saveStringValue(const juce::String& frontendName, const juce::String& key, const juce::String& value);
    
    /**
     * Load a string value from the config file.
     * @param frontendName Name of the frontend
     * @param key Key name for the setting
     * @param defaultValue Default value to return if key doesn't exist
     * @return The loaded value or defaultValue if not found
     */
    static juce::String loadStringValue(const juce::String& frontendName, const juce::String& key, const juce::String& defaultValue = juce::String());
    
    /**
     * Save an integer value to the config file.
     * @param frontendName Name of the frontend
     * @param key Key name for the setting
     * @param value Value to save
     * @return true if successful, false otherwise
     */
    static bool saveIntValue(const juce::String& frontendName, const juce::String& key, int value);
    
    /**
     * Load an integer value from the config file.
     * @param frontendName Name of the frontend
     * @param key Key name for the setting
     * @param defaultValue Default value to return if key doesn't exist
     * @return The loaded value or defaultValue if not found
     */
    static int loadIntValue(const juce::String& frontendName, const juce::String& key, int defaultValue = 0);
    
    /**
     * Save a double value to the config file.
     * @param frontendName Name of the frontend
     * @param key Key name for the setting
     * @param value Value to save
     * @return true if successful, false otherwise
     */
    static bool saveDoubleValue(const juce::String& frontendName, const juce::String& key, double value);
    
    /**
     * Load a double value from the config file.
     * @param frontendName Name of the frontend
     * @param key Key name for the setting
     * @param defaultValue Default value to return if key doesn't exist
     * @return The loaded value or defaultValue if not found
     */
    static double loadDoubleValue(const juce::String& frontendName, const juce::String& key, double defaultValue = 0.0);
    
    /**
     * Save a boolean value to the config file.
     * @param frontendName Name of the frontend
     * @param key Key name for the setting
     * @param value Value to save
     * @return true if successful, false otherwise
     */
    static bool saveBoolValue(const juce::String& frontendName, const juce::String& key, bool value);
    
    /**
     * Load a boolean value from the config file.
     * @param frontendName Name of the frontend
     * @param key Key name for the setting
     * @param defaultValue Default value to return if key doesn't exist
     * @return The loaded value or defaultValue if not found
     */
    static bool loadBoolValue(const juce::String& frontendName, const juce::String& key, bool defaultValue = false);
    
    /**
     * Remove a key from the config file.
     * @param frontendName Name of the frontend
     * @param key Key name to remove
     * @return true if successful, false otherwise
     */
    static bool removeValue(const juce::String& frontendName, const juce::String& key);
    
    /**
     * Check if a key exists in the config file.
     * @param frontendName Name of the frontend
     * @param key Key name to check
     * @return true if key exists, false otherwise
     */
    static bool hasValue(const juce::String& frontendName, const juce::String& key);
    
private:
    /**
     * Load the config XML file, creating it if it doesn't exist.
     * @param frontendName Name of the frontend
     * @return Unique pointer to XmlElement, or nullptr on error
     */
    static std::unique_ptr<juce::XmlElement> loadConfigXml(const juce::String& frontendName);
    
    /**
     * Save the config XML file.
     * @param frontendName Name of the frontend
     * @param xml XML element to save
     * @return true if successful, false otherwise
     */
    static bool saveConfigXml(const juce::String& frontendName, juce::XmlElement* xml);
    
    /**
     * Get the app data directory for storing config files.
     * @return File path to the app data directory
     */
    static juce::File getAppDataDirectory();
};

} // namespace Shared

