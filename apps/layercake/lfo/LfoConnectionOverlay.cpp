#include "LfoConnectionOverlay.h"

namespace LayerCakeApp
{

void LfoConnectionOverlay::paint(juce::Graphics& g)
{
    if (m_targets.empty())
    {
        DBG("LfoConnectionOverlay::paint early return (no targets)");
        return;
    }

    // Draw dotted lines from source to each target
    constexpr float lineThickness = 1.25f;
    const float dashLengths[] = { 3.0f, 3.0f };
    g.setColour(m_colour.withAlpha(0.7f));

    for (const auto& target : m_targets)
    {
        juce::Path path;
        path.startNewSubPath(m_source.toFloat());
        path.lineTo(target.toFloat());

        juce::PathStrokeType stroke(lineThickness);
        stroke.createDashedStroke(path, path, dashLengths, 2);
        g.strokePath(path, juce::PathStrokeType(lineThickness));
    }

    // Draw small circles at connection points
    constexpr float circleRadius = 3.0f;
    g.setColour(m_colour);
    g.fillEllipse(m_source.x - circleRadius, m_source.y - circleRadius,
                  circleRadius * 2, circleRadius * 2);

    for (const auto& target : m_targets)
    {
        g.fillEllipse(target.x - circleRadius, target.y - circleRadius,
                      circleRadius * 2, circleRadius * 2);
    }
}

void LfoConnectionOverlay::set_source(juce::Point<int> source_center, juce::Colour colour)
{
    m_source = source_center;
    m_colour = colour;
}

void LfoConnectionOverlay::add_target(juce::Point<int> target_center)
{
    m_targets.push_back(target_center);
}

void LfoConnectionOverlay::clear()
{
    m_targets.clear();
    repaint();
}

} // namespace LayerCakeApp

