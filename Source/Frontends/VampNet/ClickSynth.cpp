#include "ClickSynth.h"
#include <cmath>

using namespace VampNet;

// ClickSynth implementation
ClickSynth::ClickSynth()
{
}

void ClickSynth::triggerClick()
{
    // Reset phase and calculate samples needed
    phase.store(0.0);
    double sampleRate = 44100.0; // Default, will be updated per call
    double duration = durationSeconds.load();
    int samples = static_cast<int>(std::ceil(sampleRate * duration));
    samplesRemaining.store(samples);
    
    // Calculate phase increment for the frequency
    double freq = frequency.load();
    phaseIncrement.store(2.0 * juce::MathConstants<double>::pi * freq / sampleRate);
}

float ClickSynth::getNextSample(double sampleRate)
{
    int remaining = samplesRemaining.load();
    if (remaining <= 0)
        return 0.0f;
    
    // Update phase increment if sample rate changed
    double freq = frequency.load();
    double newPhaseIncrement = 2.0 * juce::MathConstants<double>::pi * freq / sampleRate;
    phaseIncrement.store(newPhaseIncrement);
    
    // Generate sine wave sample with envelope (simple linear decay)
    double currentPhase = phase.load();
    float sample = static_cast<float>(std::sin(currentPhase));
    
    // Apply envelope (linear decay from 1.0 to 0.0)
    float duration = durationSeconds.load();
    int totalSamples = static_cast<int>(std::ceil(sampleRate * duration));
    float envelope = static_cast<float>(remaining) / static_cast<float>(totalSamples);
    sample *= envelope;
    
    // Apply amplitude
    sample *= amplitude.load();
    
    // Update phase
    currentPhase += phaseIncrement.load();
    while (currentPhase >= 2.0 * juce::MathConstants<double>::pi)
        currentPhase -= 2.0 * juce::MathConstants<double>::pi;
    phase.store(currentPhase);
    
    // Decrement samples remaining
    samplesRemaining.store(remaining - 1);
    
    return sample;
}

// ClickSynthWindow::ContentComponent implementation
ClickSynthWindow::ContentComponent::ContentComponent(VampNetMultiTrackLooperEngine& engine, int numTracks)
    : looperEngine(engine)
{
    // Setup enable button
    enableButton.setButtonText("Enable Click Synth");
    enableButton.setToggleState(false, juce::dontSendNotification);
    enableButton.onClick = [this] { enableButtonChanged(); };
    addAndMakeVisible(enableButton);
    
    // Setup track selector
    trackLabel.setText("Destination Track:", juce::dontSendNotification);
    trackLabel.attachToComponent(&trackSelector, true);
    addAndMakeVisible(trackLabel);
    
    trackSelector.addItem("All Tracks", 1);
    for (int i = 0; i < numTracks; ++i)
    {
        trackSelector.addItem("Track " + juce::String(i + 1), i + 2);
    }
    trackSelector.setSelectedId(2); // Track 0 by default
    trackSelector.onChange = [this] { trackSelectorChanged(); };
    addAndMakeVisible(trackSelector);
    
    // Setup frequency slider
    frequencyLabel.setText("Frequency (Hz):", juce::dontSendNotification);
    frequencyLabel.attachToComponent(&frequencySlider, true);
    addAndMakeVisible(frequencyLabel);
    
    frequencySlider.setRange(100.0, 5000.0, 10.0);
    frequencySlider.setValue(1000.0);
    frequencySlider.setTextValueSuffix(" Hz");
    frequencySlider.onValueChange = [this] { frequencySliderChanged(); };
    addAndMakeVisible(frequencySlider);
    
    // Setup duration slider
    durationLabel.setText("Duration (ms):", juce::dontSendNotification);
    durationLabel.attachToComponent(&durationSlider, true);
    addAndMakeVisible(durationLabel);
    
    durationSlider.setRange(1.0, 100.0, 1.0);
    durationSlider.setValue(10.0);
    durationSlider.setTextValueSuffix(" ms");
    durationSlider.onValueChange = [this] { durationSliderChanged(); };
    addAndMakeVisible(durationSlider);
    
    // Setup amplitude slider
    amplitudeLabel.setText("Amplitude:", juce::dontSendNotification);
    amplitudeLabel.attachToComponent(&amplitudeSlider, true);
    addAndMakeVisible(amplitudeLabel);
    
    amplitudeSlider.setRange(0.0, 1.0, 0.01);
    amplitudeSlider.setValue(0.8);
    amplitudeSlider.onValueChange = [this] { amplitudeSliderChanged(); };
    addAndMakeVisible(amplitudeSlider);
    
    // Setup instructions label
    instructionsLabel.setText("Press 'k' to trigger a click", juce::dontSendNotification);
    instructionsLabel.setJustificationType(juce::Justification::centred);
    instructionsLabel.setFont(juce::Font(juce::FontOptions().withHeight(12.0f)));
    instructionsLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible(instructionsLabel);
}

void ClickSynthWindow::ContentComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
}

void ClickSynthWindow::ContentComponent::resized()
{
    auto bounds = getLocalBounds().reduced(10);
    const int rowHeight = 30;
    const int spacing = 10;
    
    enableButton.setBounds(bounds.removeFromTop(rowHeight));
    bounds.removeFromTop(spacing);
    
    auto trackArea = bounds.removeFromTop(rowHeight);
    trackLabel.setBounds(trackArea.removeFromLeft(120));
    trackArea.removeFromLeft(5);
    trackSelector.setBounds(trackArea);
    bounds.removeFromTop(spacing);
    
    auto freqArea = bounds.removeFromTop(rowHeight);
    frequencyLabel.setBounds(freqArea.removeFromLeft(120));
    freqArea.removeFromLeft(5);
    frequencySlider.setBounds(freqArea);
    bounds.removeFromTop(spacing);
    
    auto durArea = bounds.removeFromTop(rowHeight);
    durationLabel.setBounds(durArea.removeFromLeft(120));
    durArea.removeFromLeft(5);
    durationSlider.setBounds(durArea);
    bounds.removeFromTop(spacing);
    
    auto ampArea = bounds.removeFromTop(rowHeight);
    amplitudeLabel.setBounds(ampArea.removeFromLeft(120));
    ampArea.removeFromLeft(5);
    amplitudeSlider.setBounds(ampArea);
    bounds.removeFromTop(spacing);
    
    // Instructions at the bottom
    instructionsLabel.setBounds(bounds.removeFromTop(20));
}

void ClickSynthWindow::ContentComponent::enableButtonChanged()
{
    enabled.store(enableButton.getToggleState());
}

void ClickSynthWindow::ContentComponent::trackSelectorChanged()
{
    int selectedId = trackSelector.getSelectedId();
    if (selectedId == 1)
        selectedTrack.store(-1); // All tracks
    else
        selectedTrack.store(selectedId - 2); // Track index (0-based)
}

void ClickSynthWindow::ContentComponent::frequencySliderChanged()
{
    // Update frequency on selected track click synth(s)
    int trackIdx = selectedTrack.load();
    if (trackIdx >= 0 && trackIdx < looperEngine.getNumTracks())
    {
        looperEngine.getTrackEngine(trackIdx).getClickSynth().setFrequency(static_cast<float>(frequencySlider.getValue()));
    }
    else if (trackIdx == -1)
    {
        // Update all tracks
        for (int i = 0; i < looperEngine.getNumTracks(); ++i)
        {
            looperEngine.getTrackEngine(i).getClickSynth().setFrequency(static_cast<float>(frequencySlider.getValue()));
        }
    }
}

void ClickSynthWindow::ContentComponent::durationSliderChanged()
{
    // Update duration on selected track click synth(s)
    int trackIdx = selectedTrack.load();
    if (trackIdx >= 0 && trackIdx < looperEngine.getNumTracks())
    {
        looperEngine.getTrackEngine(trackIdx).getClickSynth().setDuration(static_cast<float>(durationSlider.getValue()) / 1000.0f);
    }
    else if (trackIdx == -1)
    {
        // Update all tracks
        for (int i = 0; i < looperEngine.getNumTracks(); ++i)
        {
            looperEngine.getTrackEngine(i).getClickSynth().setDuration(static_cast<float>(durationSlider.getValue()) / 1000.0f);
        }
    }
}

void ClickSynthWindow::ContentComponent::amplitudeSliderChanged()
{
    // Update amplitude on selected track click synth(s)
    int trackIdx = selectedTrack.load();
    if (trackIdx >= 0 && trackIdx < looperEngine.getNumTracks())
    {
        looperEngine.getTrackEngine(trackIdx).getClickSynth().setAmplitude(static_cast<float>(amplitudeSlider.getValue()));
    }
    else if (trackIdx == -1)
    {
        // Update all tracks
        for (int i = 0; i < looperEngine.getNumTracks(); ++i)
        {
            looperEngine.getTrackEngine(i).getClickSynth().setAmplitude(static_cast<float>(amplitudeSlider.getValue()));
        }
    }
}

// ClickSynthWindow implementation
ClickSynthWindow::ClickSynthWindow(VampNetMultiTrackLooperEngine& engine, int numTracks)
    : juce::DialogWindow("Click Synth",
                        juce::Colours::darkgrey,
                        true),
      contentComponent(new ContentComponent(engine, numTracks))
{
    setContentOwned(contentComponent, true);
    setResizable(true, true);
    setUsingNativeTitleBar(true);
    centreWithSize(400, 220); // Slightly taller to accommodate instructions
}

ClickSynthWindow::~ClickSynthWindow()
{
}

void ClickSynthWindow::closeButtonPressed()
{
    setVisible(false);
}

int ClickSynthWindow::getSelectedTrack() const
{
    if (contentComponent != nullptr)
        return contentComponent->getSelectedTrack();
    return 0;
}

bool ClickSynthWindow::isEnabled() const
{
    if (contentComponent != nullptr)
        return contentComponent->isEnabled();
    return false;
}

