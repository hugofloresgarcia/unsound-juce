#include "PathGeneratorButtons.h"

PathGeneratorButtons::PathGeneratorButtons()
{
    // Use custom empty look and feel so no default drawing happens for toggle buttons
    circlePathButton.setLookAndFeel(&emptyToggleLookAndFeel);
    randomPathButton.setLookAndFeel(&emptyToggleLookAndFeel);
    wanderPathButton.setLookAndFeel(&emptyToggleLookAndFeel);
    swirlsPathButton.setLookAndFeel(&emptyToggleLookAndFeel);
    bouncePathButton.setLookAndFeel(&emptyToggleLookAndFeel);
    spiralPathButton.setLookAndFeel(&emptyToggleLookAndFeel);
    horizontalLinePathButton.setLookAndFeel(&emptyToggleLookAndFeel);
    verticalLinePathButton.setLookAndFeel(&emptyToggleLookAndFeel);
    
    // Setup path generation toggle buttons
    circlePathButton.setButtonText("c");
    circlePathButton.onClick = [this] {
        handleButtonToggle(circlePathButton, "circle");
    };
    addAndMakeVisible(circlePathButton);
    
    randomPathButton.setButtonText("r");
    randomPathButton.onClick = [this] {
        handleButtonToggle(randomPathButton, "random");
    };
    addAndMakeVisible(randomPathButton);
    
    wanderPathButton.setButtonText("w");
    wanderPathButton.onClick = [this] {
        handleButtonToggle(wanderPathButton, "wander");
    };
    addAndMakeVisible(wanderPathButton);
    
    swirlsPathButton.setButtonText("s");
    swirlsPathButton.onClick = [this] {
        handleButtonToggle(swirlsPathButton, "swirls");
    };
    addAndMakeVisible(swirlsPathButton);
    
    bouncePathButton.setButtonText("b");
    bouncePathButton.onClick = [this] {
        handleButtonToggle(bouncePathButton, "bounce");
    };
    addAndMakeVisible(bouncePathButton);
    
    spiralPathButton.setButtonText("sp");
    spiralPathButton.onClick = [this] {
        handleButtonToggle(spiralPathButton, "spiral");
    };
    addAndMakeVisible(spiralPathButton);
    
    horizontalLinePathButton.setButtonText("hl");
    horizontalLinePathButton.onClick = [this] {
        handleButtonToggle(horizontalLinePathButton, "hl");
    };
    addAndMakeVisible(horizontalLinePathButton);
    
    verticalLinePathButton.setButtonText("vl");
    verticalLinePathButton.onClick = [this] {
        handleButtonToggle(verticalLinePathButton, "vl");
    };
    addAndMakeVisible(verticalLinePathButton);
}

void PathGeneratorButtons::handleButtonToggle(juce::ToggleButton& clickedButton, const juce::String& pathType)
{
    bool isOn = clickedButton.getToggleState();
    
    // If turning on, turn off all other buttons (mutually exclusive)
    if (isOn)
    {
        if (&clickedButton != &circlePathButton)
            circlePathButton.setToggleState(false, juce::dontSendNotification);
        if (&clickedButton != &randomPathButton)
            randomPathButton.setToggleState(false, juce::dontSendNotification);
        if (&clickedButton != &wanderPathButton)
            wanderPathButton.setToggleState(false, juce::dontSendNotification);
        if (&clickedButton != &swirlsPathButton)
            swirlsPathButton.setToggleState(false, juce::dontSendNotification);
        if (&clickedButton != &bouncePathButton)
            bouncePathButton.setToggleState(false, juce::dontSendNotification);
        if (&clickedButton != &spiralPathButton)
            spiralPathButton.setToggleState(false, juce::dontSendNotification);
        if (&clickedButton != &horizontalLinePathButton)
            horizontalLinePathButton.setToggleState(false, juce::dontSendNotification);
        if (&clickedButton != &verticalLinePathButton)
            verticalLinePathButton.setToggleState(false, juce::dontSendNotification);
    }
    
    // Notify callback
    if (onPathButtonToggled)
    {
        onPathButtonToggled(pathType, isOn);
    }
    
    repaint();
}

void PathGeneratorButtons::resetAllButtons()
{
    circlePathButton.setToggleState(false, juce::dontSendNotification);
    randomPathButton.setToggleState(false, juce::dontSendNotification);
    wanderPathButton.setToggleState(false, juce::dontSendNotification);
    swirlsPathButton.setToggleState(false, juce::dontSendNotification);
    bouncePathButton.setToggleState(false, juce::dontSendNotification);
    spiralPathButton.setToggleState(false, juce::dontSendNotification);
    horizontalLinePathButton.setToggleState(false, juce::dontSendNotification);
    verticalLinePathButton.setToggleState(false, juce::dontSendNotification);
    repaint();
}

PathGeneratorButtons::~PathGeneratorButtons()
{
    // Remove look and feel from buttons before destruction
    circlePathButton.setLookAndFeel(nullptr);
    randomPathButton.setLookAndFeel(nullptr);
    wanderPathButton.setLookAndFeel(nullptr);
    swirlsPathButton.setLookAndFeel(nullptr);
    bouncePathButton.setLookAndFeel(nullptr);
    spiralPathButton.setLookAndFeel(nullptr);
    horizontalLinePathButton.setLookAndFeel(nullptr);
    verticalLinePathButton.setLookAndFeel(nullptr);
}

void PathGeneratorButtons::paint(juce::Graphics& g)
{
    // Draw path buttons
    if (circlePathButton.isVisible() && circlePathButton.getWidth() > 0)
    {
        drawCustomPathButton(g, circlePathButton, "c", circlePathButton.getBounds(), circleColor);
        drawCustomPathButton(g, randomPathButton, "r", randomPathButton.getBounds(), randomColor);
        drawCustomPathButton(g, wanderPathButton, "w", wanderPathButton.getBounds(), wanderColor);
        drawCustomPathButton(g, swirlsPathButton, "s", swirlsPathButton.getBounds(), swirlsColor);
        drawCustomPathButton(g, bouncePathButton, "b", bouncePathButton.getBounds(), bounceColor);
        drawCustomPathButton(g, spiralPathButton, "sp", spiralPathButton.getBounds(), spiralColor);
        drawCustomPathButton(g, horizontalLinePathButton, "hl", horizontalLinePathButton.getBounds(), horizontalLineColor);
        drawCustomPathButton(g, verticalLinePathButton, "vl", verticalLinePathButton.getBounds(), verticalLineColor);
    }
}

void PathGeneratorButtons::resized()
{
    auto bounds = getLocalBounds();
    
    const int pathButtonHeight = 25;
    const int pathButtonSpacing = 4;
    const int buttonWidth = 24;
    
    auto pathButtonArea = bounds.removeFromTop(pathButtonHeight);
    
    circlePathButton.setBounds(pathButtonArea.removeFromLeft(buttonWidth));
    pathButtonArea.removeFromLeft(pathButtonSpacing);
    randomPathButton.setBounds(pathButtonArea.removeFromLeft(buttonWidth));
    pathButtonArea.removeFromLeft(pathButtonSpacing);
    wanderPathButton.setBounds(pathButtonArea.removeFromLeft(buttonWidth));
    pathButtonArea.removeFromLeft(pathButtonSpacing);
    swirlsPathButton.setBounds(pathButtonArea.removeFromLeft(buttonWidth));
    pathButtonArea.removeFromLeft(pathButtonSpacing);
    bouncePathButton.setBounds(pathButtonArea.removeFromLeft(buttonWidth));
    pathButtonArea.removeFromLeft(pathButtonSpacing);
    spiralPathButton.setBounds(pathButtonArea.removeFromLeft(buttonWidth * 2)); // Wider for "sp"
    pathButtonArea.removeFromLeft(pathButtonSpacing);
    horizontalLinePathButton.setBounds(pathButtonArea.removeFromLeft(buttonWidth * 2)); // Wider for "hl"
    pathButtonArea.removeFromLeft(pathButtonSpacing);
    verticalLinePathButton.setBounds(pathButtonArea.removeFromLeft(buttonWidth * 2)); // Wider for "vl"
}

void PathGeneratorButtons::drawCustomPathButton(juce::Graphics& g, juce::ToggleButton& button, 
                                                const juce::String& label, juce::Rectangle<int> bounds,
                                                juce::Colour color)
{
    bool isOn = button.getToggleState();
    
    // Color scheme - like mute button: background changes when on
    juce::Colour bgColor = isOn ? color : juce::Colours::black;
    juce::Colour textColor = isOn ? juce::Colours::black : color;
    juce::Colour borderColor = color;
    
    // Draw background
    g.setColour(bgColor);
    g.fillRoundedRectangle(bounds.toFloat(), 6.0f);
    
    // Draw border
    g.setColour(borderColor);
    g.drawRoundedRectangle(bounds.toFloat(), 6.0f, 2.0f);
    
    // Draw label
    g.setColour(textColor);
    g.setFont(juce::Font(juce::FontOptions()
                        .withName(juce::Font::getDefaultMonospacedFontName())
                        .withHeight(14.0f)));
    g.drawText(label, bounds, juce::Justification::centred);
}

