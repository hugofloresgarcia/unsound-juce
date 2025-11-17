#include "InputSelector.h"
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_utils/juce_audio_utils.h>

using namespace Shared;

InputSelector::InputSelector()
    : inputChannelLabel("In", "in")
{
    // Setup input channel selector
    // Note: "all" will be added last in updateChannels()
    inputChannelCombo.onChange = [this]()
    {
        if (onChannelChange)
        {
            int selectedId = inputChannelCombo.getSelectedId();
            int channel = -1; // -1 means all channels
            // "all" is the last item, so if selectedId is the last one, it's "all"
            int numItems = inputChannelCombo.getNumItems();
            if (selectedId == numItems)
                channel = -1; // "all"
            else
                channel = selectedId - 1; // Channel 0 = id 1, channel 1 = id 2, etc.
            onChannelChange(channel);
        }
    };
    
    addAndMakeVisible(inputChannelCombo);
    addAndMakeVisible(inputChannelLabel);
}

void InputSelector::resized()
{
    auto bounds = getLocalBounds();
    
    const int inputChannelLabelWidth = 40;
    const int spacingSmall = 5;
    
    inputChannelLabel.setBounds(bounds.removeFromLeft(inputChannelLabelWidth));
    bounds.removeFromLeft(spacingSmall);
    inputChannelCombo.setBounds(bounds);
}

int InputSelector::getSelectedChannel() const
{
    int selectedId = inputChannelCombo.getSelectedId();
    int numItems = inputChannelCombo.getNumItems();
    if (selectedId == numItems) return -1; // "all" is the last item
    return selectedId - 1; // Channel 0 = id 1, channel 1 = id 2, etc.
}

void InputSelector::setSelectedChannel(int channelId, juce::NotificationType notification)
{
    if (channelId == -1)
    {
        // "all" is the last item
        int numItems = inputChannelCombo.getNumItems();
        inputChannelCombo.setSelectedId(numItems, notification);
    }
    else
        inputChannelCombo.setSelectedId(channelId + 1, notification);
}

void InputSelector::updateChannels(juce::AudioDeviceManager& deviceManager)
{
    auto* device = deviceManager.getCurrentAudioDevice();
    
    // Clear existing items
    int currentSelection = inputChannelCombo.getSelectedId();
    inputChannelCombo.clear();
    
    int numChannels = 0;
    if (device != nullptr)
    {
        auto channelNames = device->getInputChannelNames();
        numChannels = channelNames.size();
        
        // Add channel options - use channel numbers (1-indexed for display)
        // id 1 = channel 0, id 2 = channel 1, etc.
        for (int i = 0; i < numChannels; ++i)
        {
            juce::String channelNumber = juce::String(i + 1); // Display as 1, 2, 3, etc.
            inputChannelCombo.addItem(channelNumber, i + 1);
        }
    }
    
    // Always add "all" as the last option (even if no device or no channels)
    int allId = numChannels + 1;
    inputChannelCombo.addItem("all", allId);
    
    // Restore selection if still valid, otherwise default to "all"
    if (currentSelection > 0 && currentSelection <= allId)
        inputChannelCombo.setSelectedId(currentSelection, juce::dontSendNotification);
    else
        inputChannelCombo.setSelectedId(allId, juce::dontSendNotification);
}
