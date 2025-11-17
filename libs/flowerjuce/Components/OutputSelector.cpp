#include "OutputSelector.h"

using namespace Shared;

OutputSelector::OutputSelector()
    : outputChannelLabel("Out", "out")
{
    // Setup output channel selector
    // Note: "all" will be added last in updateChannels()
    outputChannelCombo.onChange = [this]()
    {
        if (onChannelChange)
        {
            int selectedId = outputChannelCombo.getSelectedId();
            int channel = -1; // -1 means all channels
            // "all" is the last item, so if selectedId is the last one, it's "all"
            int numItems = outputChannelCombo.getNumItems();
            if (selectedId == numItems)
                channel = -1; // "all"
            else
                channel = selectedId - 1; // Channel 0 = id 1, channel 1 = id 2, etc.
            onChannelChange(channel);
        }
    };
    
    addAndMakeVisible(outputChannelCombo);
    addAndMakeVisible(outputChannelLabel);
}

void OutputSelector::resized()
{
    auto bounds = getLocalBounds();
    
    const int outputChannelLabelWidth = 40;
    const int spacingSmall = 5;
    
    outputChannelLabel.setBounds(bounds.removeFromLeft(outputChannelLabelWidth));
    bounds.removeFromLeft(spacingSmall);
    outputChannelCombo.setBounds(bounds);
}

int OutputSelector::getSelectedChannel() const
{
    int selectedId = outputChannelCombo.getSelectedId();
    int numItems = outputChannelCombo.getNumItems();
    if (selectedId == numItems) return -1; // "all" is the last item
    return selectedId - 1; // Channel 0 = id 1, channel 1 = id 2, etc.
}

void OutputSelector::setSelectedChannel(int channelId, juce::NotificationType notification)
{
    if (channelId == -1)
    {
        // "all" is the last item
        int numItems = outputChannelCombo.getNumItems();
        outputChannelCombo.setSelectedId(numItems, notification);
    }
    else
        outputChannelCombo.setSelectedId(channelId + 1, notification);
}

void OutputSelector::updateChannels(juce::AudioDeviceManager& deviceManager)
{
    auto* device = deviceManager.getCurrentAudioDevice();
    
    // Clear existing items
    int currentSelection = outputChannelCombo.getSelectedId();
    outputChannelCombo.clear();
    
    int numChannels = 0;
    if (device != nullptr)
    {
        auto channelNames = device->getOutputChannelNames();
        numChannels = channelNames.size();
        
        // Add channel options - use channel numbers (1-indexed for display)
        // id 1 = channel 0, id 2 = channel 1, etc.
        for (int i = 0; i < numChannels; ++i)
        {
            juce::String channelNumber = juce::String(i + 1); // Display as 1, 2, 3, etc.
            outputChannelCombo.addItem(channelNumber, i + 1);
        }
    }
    
    // Always add "all" as the last option (even if no device or no channels)
    int allId = numChannels + 1;
    outputChannelCombo.addItem("all", allId);
    
    // Restore selection if still valid, otherwise default to "all"
    if (currentSelection > 0 && currentSelection <= allId)
        outputChannelCombo.setSelectedId(currentSelection, juce::dontSendNotification);
    else
        outputChannelCombo.setSelectedId(allId, juce::dontSendNotification);
}

