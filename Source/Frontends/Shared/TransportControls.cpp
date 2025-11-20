#include "TransportControls.h"

using namespace Shared;

TransportControls::TransportControls()
    : TransportControls(nullptr, "", true)
{
}

TransportControls::TransportControls(MidiLearnManager* midiManager, const juce::String& trackPrefix, bool includeMicButton)
    : recordEnableButton(""),
      playButton(""),
      muteButton(""),
      micButton(""),
      resetButton("x"),
      midiLearnManager(midiManager),
      trackIdPrefix(trackPrefix),
      micButtonAvailable(includeMicButton)
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

    if (micButtonAvailable)
    {
        micButton.onClick = [this]
        {
            if (onMicToggle)
                onMicToggle(micButton.getToggleState());
        };
    }

    resetButton.onClick = [this]
    {
        if (onReset)
            onReset();
    };

    // Use custom empty look and feel so no default drawing happens for toggle buttons
    recordEnableButton.setLookAndFeel(&emptyToggleLookAndFeel);
    playButton.setLookAndFeel(&emptyToggleLookAndFeel);
    muteButton.setLookAndFeel(&emptyToggleLookAndFeel);
    if (micButtonAvailable)
        micButton.setLookAndFeel(&emptyToggleLookAndFeel);

    addAndMakeVisible(recordEnableButton);
    addAndMakeVisible(playButton);
    addAndMakeVisible(muteButton);
    if (micButtonAvailable)
        addAndMakeVisible(micButton);
    addAndMakeVisible(resetButton);

    // By default, hide the mic button; frontends can enable it explicitly
    if (micButtonAvailable)
        micButton.setVisible(false);

    // Setup MIDI learn for buttons
    if (midiLearnManager)
    {
        recordLearnable = std::make_unique<MidiLearnable>(*midiLearnManager, trackIdPrefix + "_record", true);
        playLearnable = std::make_unique<MidiLearnable>(*midiLearnManager, trackIdPrefix + "_play", true);
        muteLearnable = std::make_unique<MidiLearnable>(*midiLearnManager, trackIdPrefix + "_mute", true);
        if (micButtonAvailable)
            micLearnable = std::make_unique<MidiLearnable>(*midiLearnManager, trackIdPrefix + "_mic", true);

        // Create mouse listeners for right-click handling
        recordMouseListener = std::make_unique<MidiLearnMouseListener>(*recordLearnable, this);
        playMouseListener = std::make_unique<MidiLearnMouseListener>(*playLearnable, this);
        muteMouseListener = std::make_unique<MidiLearnMouseListener>(*muteLearnable, this);
        if (micButtonAvailable)
            micMouseListener = std::make_unique<MidiLearnMouseListener>(*micLearnable, this);

        // Add mouse listeners to the actual buttons
        recordEnableButton.addMouseListener(recordMouseListener.get(), false);
        playButton.addMouseListener(playMouseListener.get(), false);
        muteButton.addMouseListener(muteMouseListener.get(), false);
        if (micButtonAvailable)
            micButton.addMouseListener(micMouseListener.get(), false);

        // Register parameters
        midiLearnManager->registerParameter({
            trackIdPrefix + "_record",
            [this](float value) {
                bool state = value > 0.5f;
                recordEnableButton.setToggleState(state, juce::dontSendNotification);
                if (onRecordToggle) onRecordToggle(state);
            },
            [this]() { return recordEnableButton.getToggleState() ? 1.0f : 0.0f; },
            trackIdPrefix + " Record",
            true
        });

        midiLearnManager->registerParameter({
            trackIdPrefix + "_play",
            [this](float value) {
                bool state = value > 0.5f;
                playButton.setToggleState(state, juce::dontSendNotification);
                if (onPlayToggle) onPlayToggle(state);
            },
            [this]() { return playButton.getToggleState() ? 1.0f : 0.0f; },
            trackIdPrefix + " Play",
            true
        });

        midiLearnManager->registerParameter({
            trackIdPrefix + "_mute",
            [this](float value) {
                bool state = value > 0.5f;
                muteButton.setToggleState(state, juce::dontSendNotification);
                if (onMuteToggle) onMuteToggle(state);
            },
            [this]() { return muteButton.getToggleState() ? 1.0f : 0.0f; },
            trackIdPrefix + " Mute",
            true
        });

        if (micButtonAvailable)
        {
            midiLearnManager->registerParameter({
                trackIdPrefix + "_mic",
                [this](float value) {
                    bool state = value > 0.5f;
                    micButton.setToggleState(state, juce::dontSendNotification);
                    if (onMicToggle) onMicToggle(state);
                },
                [this]() { return micButton.getToggleState() ? 1.0f : 0.0f; },
                trackIdPrefix + " Mic",
                true
            });
        }
    }
}

TransportControls::~TransportControls()
{
    // Remove mouse listeners first
    if (recordMouseListener)
        recordEnableButton.removeMouseListener(recordMouseListener.get());
    if (playMouseListener)
        playButton.removeMouseListener(playMouseListener.get());
    if (muteMouseListener)
        muteButton.removeMouseListener(muteMouseListener.get());
    if (micButtonAvailable && micMouseListener)
        micButton.removeMouseListener(micMouseListener.get());

    recordEnableButton.setLookAndFeel(nullptr);
    playButton.setLookAndFeel(nullptr);
    muteButton.setLookAndFeel(nullptr);
    if (micButtonAvailable)
        micButton.setLookAndFeel(nullptr);

    // Unregister MIDI parameters
    if (midiLearnManager)
    {
        midiLearnManager->unregisterParameter(trackIdPrefix + "_record");
        midiLearnManager->unregisterParameter(trackIdPrefix + "_play");
        midiLearnManager->unregisterParameter(trackIdPrefix + "_mute");
        if (micButtonAvailable)
            midiLearnManager->unregisterParameter(trackIdPrefix + "_mic");
    }
}

void TransportControls::paint(juce::Graphics& g)
{
    // Draw custom toggle buttons with specific colors
    // Record button: Red
    bool recordHasMidi = recordLearnable && recordLearnable->hasMidiMapping();
    drawCustomToggleButton(g, recordEnableButton, "r", recordEnableButton.getBounds(),
                          juce::Colour(0xfff04e36), juce::Colour(0xfff04e36), recordHasMidi);

    // Mic button: accent colour
    if (micButtonAvailable && micButton.isVisible())
    {
        bool micHasMidi = micLearnable && micLearnable->hasMidiMapping();
        drawCustomToggleButton(g, micButton, "mic", micButton.getBounds(),
                              accentColour, accentColour, micHasMidi);
    }

    // Play button: accent gradient
    bool isPlaying = playButton.getToggleState();
    juce::Colour playOnColor = accentColour.darker(0.4f);
    juce::Colour playOffColor = accentColour;
    bool playHasMidi = playLearnable && playLearnable->hasMidiMapping();
    drawCustomToggleButton(g, playButton, "p", playButton.getBounds(),
                          playOnColor, playOffColor, playHasMidi);

    // Mute button: Blue
    bool muteHasMidi = muteLearnable && muteLearnable->hasMidiMapping();
    drawCustomToggleButton(g, muteButton, "m", muteButton.getBounds(),
                          juce::Colour(0xff4a90e2), juce::Colour(0xff4a90e2), muteHasMidi);
}

void TransportControls::resized()
{
    auto bounds = getLocalBounds();

    // Layout buttons horizontally
    const int buttonWidth = 30;
    const int buttonSpacing = 5;

    recordEnableButton.setBounds(bounds.removeFromLeft(buttonWidth));
    bounds.removeFromLeft(buttonSpacing);

    if (micButtonAvailable && micButton.isVisible())
    {
        micButton.setBounds(bounds.removeFromLeft(buttonWidth));
        bounds.removeFromLeft(buttonSpacing);
    }
    else
    {
        micButton.setBounds(0, 0, 0, 0);
    }

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

void TransportControls::setMicState(bool enabled)
{
    if (!micButtonAvailable)
        return;

    micButton.setToggleState(enabled, juce::dontSendNotification);
    repaint();
}

void TransportControls::setMicButtonVisible(bool visible)
{
    if (!micButtonAvailable)
        return;

    micButton.setVisible(visible);
    resized();
    repaint();
}

void TransportControls::setMicEnabled(bool enabled)
{
    if (!micButtonAvailable)
        return;

    micButton.setEnabled(enabled);
}

void TransportControls::setAccentColour(juce::Colour colour)
{
    accentColour = colour;
    repaint();
}

void TransportControls::drawCustomToggleButton(juce::Graphics& g, juce::ToggleButton& button, 
                                                const juce::String& letter, juce::Rectangle<int> bounds,
                                                juce::Colour onColor, juce::Colour offColor,
                                                bool showMidiIndicator)
{
    bool isOn = button.getToggleState();
    
    // Color scheme - use provided colors
    juce::Colour bgColor = isOn ? onColor : juce::Colours::black;
    juce::Colour textColor = isOn ? juce::Colours::black : offColor;
    juce::Colour borderColor = offColor;
    
    // Draw background
    g.setColour(bgColor);
    g.fillRoundedRectangle(bounds.toFloat(), 6.0f);
    
    // Draw border (thicker if MIDI mapped)
    g.setColour(borderColor);
    g.drawRoundedRectangle(bounds.toFloat(), 6.0f, showMidiIndicator ? 3.0f : 2.0f);
    
    // Draw MIDI indicator dot in top right corner
    if (showMidiIndicator)
    {
        g.setColour(juce::Colour(0xffed1683));  // Pink
        g.fillEllipse(bounds.getRight() - 8.0f, bounds.getY() + 2.0f, 4.0f, 4.0f);
    }
    
    // Draw letter
    g.setColour(textColor);
    g.setFont(juce::Font(juce::FontOptions()
                        .withName(juce::Font::getDefaultMonospacedFontName())
                        .withHeight(18.0f)));
    g.drawText(letter, bounds, juce::Justification::centred);
}

