#include "OutputSelector.h"

using namespace Shared;

OutputSelector::OutputSelector()
    : outputChannelLabel("Out", "out")
{
    // Setup output channel selector
    outputChannelCombo.addItem("all", 1);
    outputChannelCombo.addItem("l", 2);
    outputChannelCombo.addItem("r", 3);
    outputChannelCombo.setSelectedId(1); // Default to "all"
    outputChannelCombo.onChange = [this]()
    {
        if (onChannelChange)
        {
            int selectedId = outputChannelCombo.getSelectedId();
            int channel = -1; // -1 means all channels
            if (selectedId == 2) channel = 0; // Left
            else if (selectedId == 3) channel = 1; // Right
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
    if (selectedId == 2) return 0; // Left
    else if (selectedId == 3) return 1; // Right
    return -1; // All
}

void OutputSelector::setSelectedChannel(int channelId, juce::NotificationType notification)
{
    outputChannelCombo.setSelectedId(channelId, notification);
}

