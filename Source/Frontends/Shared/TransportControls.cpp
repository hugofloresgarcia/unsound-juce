#include "TransportControls.h"

using namespace Shared;

TransportControls::TransportControls()
    : recordEnableButton(""),
      playButton(""),
      muteButton(""),
      resetButton("x")
{
    // Setup buttons
    recordEnableButton.onClick = [this]
    {
        if (onRecordToggle)
            onRecordToggle(recordEnableButton.getToggleState());
    };
    
    playButton.onClick = [this]
    {
        if (onPlayToggle)
            onPlayToggle(playButton.getToggleState());
    };
    
    muteButton.onClick = [this]
    {
        if (onMuteToggle)
            onMuteToggle(muteButton.getToggleState());
    };
    
    resetButton.onClick = [this]
    {
        if (onReset)
            onReset();
    };
    
    // Use custom empty look and feel so no default drawing happens for toggle buttons
    recordEnableButton.setLookAndFeel(&emptyToggleLookAndFeel);
    playButton.setLookAndFeel(&emptyToggleLookAndFeel);
    muteButton.setLookAndFeel(&emptyToggleLookAndFeel);

    addAndMakeVisible(recordEnableButton);
    addAndMakeVisible(playButton);
    addAndMakeVisible(muteButton);
    addAndMakeVisible(resetButton);
}

TransportControls::~TransportControls()
{
    recordEnableButton.setLookAndFeel(nullptr);
    playButton.setLookAndFeel(nullptr);
    muteButton.setLookAndFeel(nullptr);
}

void TransportControls::paint(juce::Graphics& g)
{
    // Draw custom toggle buttons with specific colors
    // Record button: Red
    drawCustomToggleButton(g, recordEnableButton, "r", recordEnableButton.getBounds(),
                          juce::Colour(0xfff04e36), juce::Colour(0xfff04e36)); // Red-orange
    
    // Play button: Gray when on (playing), Green when off (idle)
    bool isPlaying = playButton.getToggleState();
    juce::Colour playOnColor = juce::Colour(0xff808080);  // Gray when playing
    juce::Colour playOffColor = juce::Colour(0xff00ff00); // Green when idle
    drawCustomToggleButton(g, playButton, "p", playButton.getBounds(),
                          playOnColor, playOffColor);
    
    // Mute button: Blue
    drawCustomToggleButton(g, muteButton, "m", muteButton.getBounds(),
                          juce::Colour(0xff4a90e2), juce::Colour(0xff4a90e2)); // Blue
}

void TransportControls::resized()
{
    auto bounds = getLocalBounds();
    
    // Layout buttons horizontally
    const int buttonWidth = 30;
    const int buttonSpacing = 5;
    
    recordEnableButton.setBounds(bounds.removeFromLeft(buttonWidth));
    bounds.removeFromLeft(buttonSpacing);
    playButton.setBounds(bounds.removeFromLeft(buttonWidth));
    bounds.removeFromLeft(buttonSpacing);
    muteButton.setBounds(bounds.removeFromLeft(buttonWidth));
}

void TransportControls::setRecordState(bool enabled)
{
    recordEnableButton.setToggleState(enabled, juce::dontSendNotification);
    repaint();
}

void TransportControls::setPlayState(bool playing)
{
    playButton.setToggleState(playing, juce::dontSendNotification);
    repaint();
}

void TransportControls::setMuteState(bool muted)
{
    muteButton.setToggleState(muted, juce::dontSendNotification);
    repaint();
}

void TransportControls::drawCustomToggleButton(juce::Graphics& g, juce::ToggleButton& button, 
                                                const juce::String& letter, juce::Rectangle<int> bounds,
                                                juce::Colour onColor, juce::Colour offColor)
{
    bool isOn = button.getToggleState();
    
    // Color scheme - use provided colors
    juce::Colour bgColor = isOn ? onColor : juce::Colours::black;
    juce::Colour textColor = isOn ? juce::Colours::black : offColor;
    juce::Colour borderColor = offColor;
    
    // Draw background
    g.setColour(bgColor);
    g.fillRoundedRectangle(bounds.toFloat(), 6.0f);
    
    // Draw border
    g.setColour(borderColor);
    g.drawRoundedRectangle(bounds.toFloat(), 6.0f, 2.0f);
    
    // Draw letter
    g.setColour(textColor);
    g.setFont(juce::Font(juce::FontOptions()
                        .withName(juce::Font::getDefaultMonospacedFontName())
                        .withHeight(18.0f)));
    g.drawText(letter, bounds, juce::Justification::centred);
}

