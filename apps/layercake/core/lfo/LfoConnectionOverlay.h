#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

namespace LayerCakeApp
{

/**
 * Overlay component that draws dotted lines from an LFO to its connected knobs.
 * This sits on top of all other components and is mouse-transparent.
 */
class LfoConnectionOverlay : public juce::Component
{
public:
    LfoConnectionOverlay() { setInterceptsMouseClicks(false, false); }

    void paint(juce::Graphics& g) override;

    void set_source(juce::Point<int> source_center, juce::Colour colour);
    void add_target(juce::Point<int> target_center);
    void clear();
    bool has_connections() const { return !m_targets.empty(); }

private:
    juce::Point<int> m_source;
    juce::Colour m_colour{juce::Colours::white};
    std::vector<juce::Point<int>> m_targets;
};

} // namespace LayerCakeApp


