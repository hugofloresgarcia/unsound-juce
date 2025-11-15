#include "Panner2DComponent.h"
#include "../CustomLookAndFeel.h"
#include <algorithm>

Panner2DComponent::Panner2DComponent()
{
    setOpaque(true);
    panX = 0.5f;
    panY = 0.5f;
    isDragging = false;
}

void Panner2DComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    
    // Fill background
    g.setColour(juce::Colours::black);
    g.fillRoundedRectangle(bounds, 4.0f);
    
    // Draw bright border
    g.setColour(juce::Colour(0xfff3d430)); // Bright yellow from CustomLookAndFeel
    g.drawRoundedRectangle(bounds, 4.0f, 3.0f);
    
    // Draw dense grid (16x16)
    g.setColour(juce::Colour(0xff333333));
    const int gridDivisions = 16;
    float gridSpacingX = bounds.getWidth() / gridDivisions;
    float gridSpacingY = bounds.getHeight() / gridDivisions;
    for (int i = 1; i < gridDivisions; ++i)
    {
        // Vertical lines
        g.drawLine(bounds.getX() + i * gridSpacingX, bounds.getY(),
                   bounds.getX() + i * gridSpacingX, bounds.getBottom(), 0.5f);
        // Horizontal lines
        g.drawLine(bounds.getX(), bounds.getY() + i * gridSpacingY,
                   bounds.getRight(), bounds.getY() + i * gridSpacingY, 0.5f);
    }
    
    // Draw center crosshair
    g.setColour(juce::Colour(0xff555555));
    float centerX = bounds.getCentreX();
    float centerY = bounds.getCentreY();
    float crosshairSize = 8.0f;
    g.drawLine(centerX - crosshairSize, centerY, centerX + crosshairSize, centerY, 1.0f);
    g.drawLine(centerX, centerY - crosshairSize, centerX, centerY + crosshairSize, 1.0f);
    
    // Draw pan indicator
    auto panPos = panToComponent(panX, panY);
    float indicatorRadius = 8.0f;
    
    // Draw indicator shadow
    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.fillEllipse(panPos.x - indicatorRadius + 1.0f, panPos.y - indicatorRadius + 1.0f,
                  indicatorRadius * 2.0f, indicatorRadius * 2.0f);
    
    // Draw indicator
    g.setColour(juce::Colour(0xffed1683)); // Pink from CustomLookAndFeel
    g.fillEllipse(panPos.x - indicatorRadius, panPos.y - indicatorRadius,
                  indicatorRadius * 2.0f, indicatorRadius * 2.0f);
    
    // Draw indicator outline
    g.setColour(juce::Colour(0xfff3d430)); // Yellow from CustomLookAndFeel
    g.drawEllipse(panPos.x - indicatorRadius, panPos.y - indicatorRadius,
                  indicatorRadius * 2.0f, indicatorRadius * 2.0f, 2.0f);
}

void Panner2DComponent::resized()
{
    // Trigger repaint when resized
    repaint();
}

void Panner2DComponent::mouseDown(const juce::MouseEvent& e)
{
    if (e.mods.isLeftButtonDown())
    {
        isDragging = true;
        auto panPos = componentToPan(e.position);
        setPanPosition(panPos.x, panPos.y, juce::sendNotification);
    }
}

void Panner2DComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (isDragging && e.mods.isLeftButtonDown())
    {
        auto panPos = componentToPan(e.position);
        setPanPosition(panPos.x, panPos.y, juce::sendNotification);
    }
}

void Panner2DComponent::mouseUp(const juce::MouseEvent& e)
{
    if (isDragging)
    {
        isDragging = false;
    }
}

void Panner2DComponent::setPanPosition(float x, float y, juce::NotificationType notification)
{
    clampPan(x, y);
    
    if (panX != x || panY != y)
    {
        panX = x;
        panY = y;
        
        repaint();
        
        if (notification == juce::sendNotification && onPanChange)
        {
            onPanChange(panX, panY);
        }
    }
}

juce::Point<float> Panner2DComponent::componentToPan(juce::Point<float> componentPos) const
{
    auto bounds = getLocalBounds().toFloat();
    
    // Clamp to component bounds
    componentPos.x = juce::jlimit(bounds.getX(), bounds.getRight(), componentPos.x);
    componentPos.y = juce::jlimit(bounds.getY(), bounds.getBottom(), componentPos.y);
    
    // Convert to normalized coordinates (0-1)
    float x = (componentPos.x - bounds.getX()) / bounds.getWidth();
    float y = (componentPos.y - bounds.getY()) / bounds.getHeight();
    
    // Invert Y axis: 0 = bottom, 1 = top
    y = 1.0f - y;
    
    return {x, y};
}

juce::Point<float> Panner2DComponent::panToComponent(float x, float y) const
{
    auto bounds = getLocalBounds().toFloat();
    
    // Clamp pan coordinates
    x = juce::jlimit(0.0f, 1.0f, x);
    y = juce::jlimit(0.0f, 1.0f, y);
    
    // Invert Y axis: 0 = bottom, 1 = top
    y = 1.0f - y;
    
    // Convert to component coordinates
    float componentX = bounds.getX() + x * bounds.getWidth();
    float componentY = bounds.getY() + y * bounds.getHeight();
    
    return {componentX, componentY};
}

void Panner2DComponent::clampPan(float& x, float& y) const
{
    x = juce::jlimit(0.0f, 1.0f, x);
    y = juce::jlimit(0.0f, 1.0f, y);
}

