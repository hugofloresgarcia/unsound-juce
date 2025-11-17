#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <functional>

namespace Shared
{

class InputSelector : public juce::Component
{
public:
    InputSelector();
    ~InputSelector() override = default;

    void resized() override;

    // Callback for channel selection (-1 = all, 0+ = specific channel)
    std::function<void(int)> onChannelChange;

    // Get/set selected channel
    int getSelectedChannel() const;
    void setSelectedChannel(int channelId, juce::NotificationType notification);
    
    // Update available channels based on current audio device
    void updateChannels(juce::AudioDeviceManager& deviceManager);

private:
    juce::ComboBox inputChannelCombo;
    juce::Label inputChannelLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InputSelector)
};

} // namespace Shared
