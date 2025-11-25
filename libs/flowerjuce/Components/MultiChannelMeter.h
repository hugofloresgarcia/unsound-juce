#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <vector>

namespace Shared
{

class MultiChannelMeter : public juce::Component
{
public:
    static constexpr int kMaxChannels = 8;

    void set_levels(const std::vector<double>& levels);
    void paint(juce::Graphics& g) override;

private:
    juce::Colour colour_for_db(double db) const;

    std::array<double, kMaxChannels> m_levels{};
    int m_active_channels{1};
};

} // namespace Shared

