#include "Sampler.h"
#include <cmath>

using namespace WhAM;

// Sampler implementation
Sampler::Sampler()
{
    formatManager.registerBasicFormats();
}

bool Sampler::loadSample(const juce::File& audioFile)
{
    if (!audioFile.existsAsFile())
    {
        DBG("Sampler: File does not exist: " << audioFile.getFullPathName());
        return false;
    }
    
    std::unique_ptr<juce::AudioFormatReader> newReader(formatManager.createReaderFor(audioFile));
    if (newReader == nullptr)
    {
        DBG("Sampler: Could not create reader for file: " << audioFile.getFullPathName());
        return false;
    }
    
    // Read audio data (convert to mono if needed)
    juce::AudioBuffer<float> tempBuffer(static_cast<int>(newReader->numChannels), 
                                        static_cast<int>(newReader->lengthInSamples));
    
    if (!newReader->read(&tempBuffer, 0, static_cast<int>(newReader->lengthInSamples), 0, true, true))
    {
        DBG("Sampler: Failed to read audio data");
        return false;
    }
    
    // Convert to mono and store
    size_t numSamples = static_cast<size_t>(newReader->lengthInSamples);
    sampleData.resize(numSamples);
    
    if (tempBuffer.getNumChannels() == 1)
    {
        // Already mono
        for (size_t i = 0; i < numSamples; ++i)
        {
            sampleData[i] = tempBuffer.getSample(0, static_cast<int>(i));
        }
    }
    else
    {
        // Mix down to mono
        for (size_t i = 0; i < numSamples; ++i)
        {
            float sum = 0.0f;
            for (int channel = 0; channel < tempBuffer.getNumChannels(); ++channel)
            {
                sum += tempBuffer.getSample(channel, static_cast<int>(i));
            }
            sampleData[i] = sum / static_cast<float>(tempBuffer.getNumChannels());
        }
    }
    
    sampleLength.store(numSamples);
    sampleName = audioFile.getFileName();
    reader = std::move(newReader);
    
    DBG("Sampler: Loaded sample: " << sampleName << " (" << numSamples << " samples)");
    return true;
}

void Sampler::trigger()
{
    currentPosition.store(0);
}

float Sampler::getNextSample()
{
    size_t pos = currentPosition.load();
    size_t length = sampleLength.load();
    
    if (pos >= length || sampleData.empty())
        return 0.0f;
    
    float sample = sampleData[pos];
    currentPosition.store(pos + 1);
    
    return sample;
}

// SamplerWindow::ContentComponent implementation
SamplerWindow::ContentComponent::ContentComponent(VampNetMultiTrackLooperEngine& engine, int numTracks, Shared::MidiLearnManager* midiManager)
    : looperEngine(engine), midiLearnManager(midiManager), parameterId("sampler_trigger")
{
    // Setup enable button
    enableButton.setButtonText("Enable Sampler");
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
    
    // Setup load sample button
    loadSampleButton.setButtonText("Load Sample...");
    loadSampleButton.onClick = [this] { loadSampleButtonClicked(); };
    addAndMakeVisible(loadSampleButton);
    
    // Setup sample name label
    sampleNameLabel.setText("No sample loaded", juce::dontSendNotification);
    sampleNameLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(sampleNameLabel);
    
    // Setup trigger button
    triggerButton.setButtonText("Trigger");
    triggerButton.onClick = [this] { triggerButtonClicked(); };
    addAndMakeVisible(triggerButton);
    
    // Setup MIDI learn for trigger button
    if (midiLearnManager)
    {
        triggerButtonLearnable = std::make_unique<Shared::MidiLearnable>(*midiLearnManager, parameterId);
        
        // Create mouse listener for right-click handling
        triggerButtonMouseListener = std::make_unique<Shared::MidiLearnMouseListener>(*triggerButtonLearnable, this);
        triggerButton.addMouseListener(triggerButtonMouseListener.get(), false);
        
        midiLearnManager->registerParameter({
            parameterId,
            [this](float value) {
                if (value > 0.5f && enabled.load())
                    triggerButtonClicked();
            },
            [this]() { return 0.0f; },
            "Sampler Trigger",
            true  // Toggle control
        });
    }
    
    // Setup instructions label
    instructionsLabel.setText("Press 'k' or click Trigger to trigger the sample", juce::dontSendNotification);
    instructionsLabel.setJustificationType(juce::Justification::centred);
    instructionsLabel.setFont(juce::Font(juce::FontOptions().withHeight(12.0f)));
    instructionsLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible(instructionsLabel);
}

void SamplerWindow::ContentComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
    
    // Draw MIDI indicator on trigger button if mapped
    if (triggerButtonLearnable && triggerButtonLearnable->hasMidiMapping())
    {
        auto buttonBounds = triggerButton.getBounds();
        g.setColour(juce::Colour(0xffed1683));  // Pink
        g.fillEllipse(buttonBounds.getRight() - 8.0f, buttonBounds.getY() + 2.0f, 6.0f, 6.0f);
    }
}

void SamplerWindow::ContentComponent::resized()
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
    
    // Load sample button and label
    auto loadArea = bounds.removeFromTop(rowHeight);
    loadSampleButton.setBounds(loadArea.removeFromLeft(120));
    loadArea.removeFromLeft(5);
    sampleNameLabel.setBounds(loadArea);
    bounds.removeFromTop(spacing);
    
    // Trigger button
    triggerButton.setBounds(bounds.removeFromTop(rowHeight));
    bounds.removeFromTop(spacing);
    
    // Instructions at the bottom
    instructionsLabel.setBounds(bounds.removeFromTop(20));
}

void SamplerWindow::ContentComponent::enableButtonChanged()
{
    enabled.store(enableButton.getToggleState());
}

void SamplerWindow::ContentComponent::trackSelectorChanged()
{
    int selectedId = trackSelector.getSelectedId();
    if (selectedId == 1)
        selectedTrack.store(-1); // All tracks
    else
        selectedTrack.store(selectedId - 2); // Track index (0-based)
}

void SamplerWindow::ContentComponent::loadSampleButtonClicked()
{
    int trackIdx = selectedTrack.load();
    
    juce::FileChooser chooser("Select audio sample...",
                              juce::File(),
                              "*.wav;*.aif;*.aiff;*.mp3;*.ogg;*.flac");
    
    if (chooser.browseForFileToOpen())
    {
        juce::File selectedFile = chooser.getResult();
        
        if (trackIdx >= 0 && trackIdx < looperEngine.getNumTracks())
        {
            // Load into selected track
            if (looperEngine.getTrackEngine(trackIdx).getSampler().loadSample(selectedFile))
            {
                sampleNameLabel.setText(selectedFile.getFileName(), juce::dontSendNotification);
            }
        }
        else if (trackIdx == -1)
        {
            // Load into all tracks
            bool success = false;
            for (int i = 0; i < looperEngine.getNumTracks(); ++i)
            {
                if (looperEngine.getTrackEngine(i).getSampler().loadSample(selectedFile))
                {
                    success = true;
                }
            }
            if (success)
            {
                sampleNameLabel.setText(selectedFile.getFileName() + " (all tracks)", juce::dontSendNotification);
            }
        }
    }
}

void SamplerWindow::ContentComponent::triggerButtonClicked()
{
    if (!enabled.load())
        return;
    
    int trackIdx = selectedTrack.load();
    
    // Trigger sample on selected track(s)
    if (trackIdx >= 0 && trackIdx < looperEngine.getNumTracks())
    {
        // Single track selected
        if (looperEngine.getTrackEngine(trackIdx).getSampler().hasSample())
        {
            looperEngine.getTrackEngine(trackIdx).getSampler().trigger();
        }
    }
    else if (trackIdx == -1)
    {
        // All tracks - trigger sample on all tracks that have samples loaded
        for (int i = 0; i < looperEngine.getNumTracks(); ++i)
        {
            if (looperEngine.getTrackEngine(i).getSampler().hasSample())
            {
                looperEngine.getTrackEngine(i).getSampler().trigger();
            }
        }
    }
}

SamplerWindow::ContentComponent::~ContentComponent()
{
    // Remove mouse listener first
    if (triggerButtonMouseListener)
        triggerButton.removeMouseListener(triggerButtonMouseListener.get());
    
    // Unregister MIDI parameter
    if (midiLearnManager)
    {
        midiLearnManager->unregisterParameter(parameterId);
    }
}

// SamplerWindow implementation
SamplerWindow::SamplerWindow(VampNetMultiTrackLooperEngine& engine, int numTracks, Shared::MidiLearnManager* midiManager)
    : juce::DialogWindow("Sampler",
                        juce::Colours::darkgrey,
                        true),
      contentComponent(new ContentComponent(engine, numTracks, midiManager))
{
    setContentOwned(contentComponent, true);
    setResizable(true, true);
    setUsingNativeTitleBar(true);
    centreWithSize(400, 230); // Taller to accommodate trigger button
}

SamplerWindow::~SamplerWindow()
{
}

void SamplerWindow::closeButtonPressed()
{
    setVisible(false);
}

int SamplerWindow::getSelectedTrack() const
{
    if (contentComponent != nullptr)
        return contentComponent->getSelectedTrack();
    return 0;
}

bool SamplerWindow::isEnabled() const
{
    if (contentComponent != nullptr)
        return contentComponent->isEnabled();
    return false;
}

