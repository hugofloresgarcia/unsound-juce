#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>

namespace Shared
{

class AudioInfoDisplay : public juce::Component,
                         public juce::Timer
{
public:
    AudioInfoDisplay(juce::AudioDeviceManager& deviceManager)
        : audioDeviceManager(deviceManager)
    {
        startTimer(1000); // Update every second
    }
    
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds();
        
        // Semi-transparent background
        g.setColour(juce::Colours::black.withAlpha(0.7f));
        g.fillRoundedRectangle(bounds.toFloat(), 4.0f);
        
        // Border
        g.setColour(juce::Colour(0xff1eb19d)); // Teal
        g.drawRoundedRectangle(bounds.toFloat(), 4.0f, 1.0f);
        
        // Text content
        g.setColour(juce::Colour(0xff1eb19d));
        g.setFont(juce::Font(juce::FontOptions().withHeight(11.0f)));
        
        auto textBounds = bounds.reduced(8, 4);
        int lineHeight = 14;
        int y = textBounds.getY();
        
        // Device name
        if (auto* device = audioDeviceManager.getCurrentAudioDevice())
        {
            g.drawText("device: " + device->getName(), 
                      textBounds.getX(), y, textBounds.getWidth(), lineHeight,
                      juce::Justification::left);
            y += lineHeight;
            
            // Input channels
            auto activeInputs = device->getActiveInputChannels();
            int numActiveInputs = activeInputs.countNumberOfSetBits();
            g.drawText("in: " + juce::String(numActiveInputs) + " ch", 
                      textBounds.getX(), y, textBounds.getWidth(), lineHeight,
                      juce::Justification::left);
            y += lineHeight;
            
            // Output channels
            auto activeOutputs = device->getActiveOutputChannels();
            int numActiveOutputs = activeOutputs.countNumberOfSetBits();
            g.drawText("out: " + juce::String(numActiveOutputs) + " ch", 
                      textBounds.getX(), y, textBounds.getWidth(), lineHeight,
                      juce::Justification::left);
            y += lineHeight;
            
            // Audio callback status
            bool isPlaying = device->isPlaying();
            g.setColour(isPlaying ? juce::Colour(0xff00ff00) : juce::Colour(0xffff0000));
            g.drawText(isPlaying ? "● active" : "● stopped", 
                      textBounds.getX(), y, textBounds.getWidth(), lineHeight,
                      juce::Justification::left);
        }
        else
        {
            g.drawText("no audio device", 
                      textBounds.getX(), y, textBounds.getWidth(), lineHeight,
                      juce::Justification::left);
        }
    }
    
    void timerCallback() override
    {
        repaint(); // Update display every second
    }

private:
    juce::AudioDeviceManager& audioDeviceManager;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioInfoDisplay)
};

} // namespace Shared

