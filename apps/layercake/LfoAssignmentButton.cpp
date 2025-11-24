#include "LfoAssignmentButton.h"

namespace
{
constexpr float kBorderThickness = 1.2f;
constexpr float kHoverAlpha = 0.15f;
constexpr float kPressAlpha = 0.25f;
constexpr float kIdleAlpha = 0.35f;
constexpr float kIconScale = 0.48f;
}

LfoAssignmentButton::LfoAssignmentButton()
{
    setWantsKeyboardFocus(false);
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    m_idle_colour = juce::Colours::darkgrey;
    // Tooltips are currently managed by the parent overlay, so skip setting here.
}

void LfoAssignmentButton::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    const float radius = bounds.getWidth() * 0.5f;

    auto baseColour = m_idle_colour;
    if (m_has_assignment && m_assignment_colour.has_value())
        baseColour = m_assignment_colour.value();

    float alpha = m_has_assignment ? 1.0f : kIdleAlpha;
    if (m_is_pressed)
        alpha += kPressAlpha;
    else if (isMouseOver(true))
        alpha += kHoverAlpha;

    g.setColour(baseColour.withMultipliedAlpha(juce::jlimit(0.15f, 1.0f, alpha)));
    g.fillEllipse(bounds);

    g.setColour(baseColour.darker(0.6f));
    g.drawEllipse(bounds, kBorderThickness);

    auto iconBounds = bounds.reduced(bounds.getWidth() * (1.0f - kIconScale) * 0.5f);
    juce::Path icon;
    icon.addTriangle(iconBounds.getX() + iconBounds.getWidth() * 0.2f,
                     iconBounds.getBottom(),
                     iconBounds.getCentreX(),
                     iconBounds.getY(),
                     iconBounds.getRight() - iconBounds.getWidth() * 0.2f,
                     iconBounds.getBottom());
    g.setColour(juce::Colours::black.withAlpha(m_has_assignment ? 0.75f : 0.4f));
    g.fillPath(icon);
}

void LfoAssignmentButton::resized()
{
    // component is self-contained
}

void LfoAssignmentButton::mouseDown(const juce::MouseEvent& event)
{
    if (!isEnabled())
    {
        DBG("LfoAssignmentButton::mouseDown early return (disabled)");
        return;
    }

    m_is_pressed = true;
    repaint();
    juce::Component::mouseDown(event);
}

void LfoAssignmentButton::mouseUp(const juce::MouseEvent& event)
{
    if (!isEnabled())
    {
        DBG("LfoAssignmentButton::mouseUp early return (disabled)");
        return;
    }

    if (!m_is_pressed)
    {
        DBG("LfoAssignmentButton::mouseUp early return (not pressed)");
        return;
    }

    m_is_pressed = false;
    repaint();
    trigger_click();
    juce::Component::mouseUp(event);
}

void LfoAssignmentButton::mouseExit(const juce::MouseEvent& event)
{
    if (m_is_pressed)
    {
        m_is_pressed = false;
        repaint();
    }
    juce::Component::mouseExit(event);
}

void LfoAssignmentButton::setIdleColour(juce::Colour colour)
{
    if (m_idle_colour == colour)
        return;

    m_idle_colour = colour;
    repaint();
}

void LfoAssignmentButton::setAssignmentColour(std::optional<juce::Colour> colour)
{
    if (m_assignment_colour == colour)
        return;

    m_assignment_colour = colour;
    repaint();
}

void LfoAssignmentButton::setHasAssignment(bool hasAssignment)
{
    if (m_has_assignment == hasAssignment)
        return;

    m_has_assignment = hasAssignment;
    repaint();
}

void LfoAssignmentButton::trigger_click()
{
    if (onClicked != nullptr)
        onClicked();
}


