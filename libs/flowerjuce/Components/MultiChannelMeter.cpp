#include "MultiChannelMeter.h"
#include <juce_audio_basics/juce_audio_basics.h>

namespace Shared
{

void MultiChannelMeter::set_levels(const std::vector<double>& levels)
{
    const int desired_channels = juce::jlimit(1,
                                              kMaxChannels,
                                              static_cast<int>(levels.empty() ? 1 : levels.size()));
    bool changed = desired_channels != m_active_channels;

    for (int i = 0; i < desired_channels; ++i)
    {
        const double clamped = juce::jlimit(0.0, 1.0, levels.empty() ? 0.0 : levels[static_cast<size_t>(i)]);
        changed = changed || std::abs(clamped - m_levels[static_cast<size_t>(i)]) > 0.0005;
        m_levels[static_cast<size_t>(i)] = clamped;
    }

    for (int i = desired_channels; i < kMaxChannels; ++i)
        m_levels[static_cast<size_t>(i)] = 0.0;

    if (desired_channels != m_active_channels)
        m_active_channels = desired_channels;

    if (changed)
        repaint();
}

void MultiChannelMeter::paint(juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat().reduced(2.0f);
    if (area.isEmpty())
    {
        DBG("MultiChannelMeter::paint early return (empty area)");
        return;
    }

    const int channels = juce::jmax(1, m_active_channels);
    const float spacing = channels > 1 ? 4.0f : 0.0f;
    const float total_spacing = spacing * static_cast<float>(channels - 1);
    const float slot_width = juce::jmax(6.0f, (area.getWidth() - total_spacing) / static_cast<float>(channels));
    const float corner = juce::jmin(6.0f, slot_width * 0.4f);

    auto slotArea = area;
    for (int channel = 0; channel < channels; ++channel)
    {
        juce::Rectangle<float> slot(slotArea.removeFromLeft(slot_width));
        slotArea.removeFromLeft(spacing);

        const auto background = findColour(juce::ProgressBar::backgroundColourId).withAlpha(0.85f);
        const auto outline = findColour(juce::Slider::trackColourId).withAlpha(0.45f);

        g.setColour(background);
        g.fillRoundedRectangle(slot, corner);

        auto fill_bounds = slot.reduced(2.0f);
        const float level = static_cast<float>(juce::jlimit(0.0, 1.0, m_levels[static_cast<size_t>(channel)]));
        const float fill_height = fill_bounds.getHeight() * level;
        if (fill_height > 0.0f)
        {
            auto filled = fill_bounds.removeFromBottom(fill_height);
            const double db = static_cast<double>(juce::Decibels::gainToDecibels(level, -60.0f));
            g.setColour(colour_for_db(db));
            g.fillRoundedRectangle(filled, corner * 0.5f);
        }

        g.setColour(outline);
        g.drawRoundedRectangle(slot, corner, 1.0f);
    }
}

juce::Colour MultiChannelMeter::colour_for_db(double db) const
{
    if (db < -18.0)
        return juce::Colour(0xff4caf50); // green
    if (db < -6.0)
        return juce::Colour(0xfffbc02d); // yellow
    return juce::Colour(0xfff44336);     // red
}

} // namespace Shared



