#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace Shared
{

class OutputSelector : public juce::Component
{
public:
    OutputSelector();
    ~OutputSelector() override = default;

    void resized() override;

    // Callback for channel selection (-1 = all, 0 = left, 1 = right)
    std::function<void(int)> onChannelChange;

    // Get/set selected channel
    int getSelectedChannel() const;
    void setSelectedChannel(int channelId, juce::NotificationType notification);

private:
    juce::ComboBox outputChannelCombo;
    juce::Label outputChannelLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OutputSelector)
};

} // namespace Shared

