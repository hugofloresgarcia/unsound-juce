#include "KnobRecorderButton.h"

namespace
{
constexpr float kFontHeight = 11.0f;
constexpr float kCornerRadiusRatio = 0.35f;
constexpr float kBorderThickness = 1.0f;
}

KnobRecorderButton::KnobRecorderButton()
{
    setWantsKeyboardFocus(false);
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    setColour(idleColourId, juce::Colours::darkgrey);
    setColour(armedColourId, juce::Colours::yellow);
    setColour(recordingColourId, juce::Colours::red);
    setColour(playingColourId, juce::Colours::green);
    setColour(textColourId, juce::Colours::black);
    setColour(borderColourId, juce::Colours::black);
}

void KnobRecorderButton::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    const float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * kCornerRadiusRatio;

    auto selectColour = [this]() -> juce::Colour {
        switch (m_status)
        {
            case Status::Armed: return findColour(armedColourId);
            case Status::Recording: return findColour(recordingColourId);
            case Status::Playing: return findColour(playingColourId);
            case Status::Idle:
            default: return findColour(idleColourId);
        }
    };

    juce::Colour fill = selectColour();
    if (!isEnabled())
        fill = fill.withMultipliedAlpha(0.4f);
    else if (m_is_pressed)
        fill = fill.brighter(0.15f);

    g.setColour(fill);
    g.fillRoundedRectangle(bounds, radius);

    g.setColour(findColour(borderColourId));
    g.drawRoundedRectangle(bounds, radius, kBorderThickness);

    g.setColour(findColour(textColourId));
    g.setFont(kFontHeight);
    g.drawText("[kr]", bounds.toNearestInt(), juce::Justification::centred, false);
}

void KnobRecorderButton::resized()
{
    // No child components, but method provided for future layout changes.
}

void KnobRecorderButton::mouseDown(const juce::MouseEvent& event)
{
    if (!isEnabled())
    {
        DBG("KnobRecorderButton::mouseDown early return (disabled)");
        return;
    }

    m_is_pressed = true;
    repaint();
    trigger_press();
    juce::Component::mouseDown(event);
}

void KnobRecorderButton::mouseUp(const juce::MouseEvent& event)
{
    if (!isEnabled())
    {
        DBG("KnobRecorderButton::mouseUp early return (disabled)");
        return;
    }

    if (m_is_pressed)
    {
        m_is_pressed = false;
        repaint();
        trigger_release();
    }
    juce::Component::mouseUp(event);
}

void KnobRecorderButton::mouseExit(const juce::MouseEvent& event)
{
    if (m_is_pressed)
    {
        m_is_pressed = false;
        repaint();
        trigger_release();
    }
    juce::Component::mouseExit(event);
}

void KnobRecorderButton::setStatus(Status status)
{
    if (m_status != status)
    {
        m_status = status;
        repaint();
    }
}

void KnobRecorderButton::trigger_press()
{
    if (onPressed != nullptr)
        onPressed();
}

void KnobRecorderButton::trigger_release()
{
    if (onReleased != nullptr)
        onReleased();
}


