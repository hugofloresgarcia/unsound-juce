#include "StartupDialog.h"

StartupDialog::StartupDialog(juce::AudioDeviceManager& deviceManager)
    : audioDeviceManager(deviceManager),
      titleLabel("Title", "tape looper setup"),
      numTracksLabel("Tracks", "number of tracks"),
      numTracksSlider(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight),
      frontendLabel("Frontend", "frontend"),
      audioDeviceSelector(deviceManager, 0, 256, 0, 256, true, true, true, false),
      okButton("ok")
{
    // Setup title
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setFont(juce::Font(juce::FontOptions()
                                  .withName(juce::Font::getDefaultMonospacedFontName())
                                  .withHeight(20.0f))); // Monospaced, no bold
    addAndMakeVisible(titleLabel);
    
    // Setup number of tracks slider
    numTracksSlider.setRange(1, 8, 1);
    numTracksSlider.setValue(4);
    numTracks = 4; // Initialize to match slider default
    numTracksSlider.onValueChange = [this]
    {
        numTracks = static_cast<int>(numTracksSlider.getValue());
    };
    addAndMakeVisible(numTracksSlider);
    addAndMakeVisible(numTracksLabel);
    
    // Setup frontend selector
    frontendCombo.addItem("basic", 1);
    frontendCombo.addItem("text2sound", 2);
    frontendCombo.addItem("vampnet", 3);
    frontendCombo.setSelectedId(1); // Default to "basic"
    frontendCombo.onChange = [this]
    {
        selectedFrontend = frontendCombo.getText();
    };
    addAndMakeVisible(frontendCombo);
    addAndMakeVisible(frontendLabel);
    
    // Setup audio device selector
    addAndMakeVisible(audioDeviceSelector);
    
    // Setup OK button
    okButton.addListener(this);
    addAndMakeVisible(okButton);
    
    setSize(600, 600);
}

void StartupDialog::resized()
{
    auto bounds = getLocalBounds().reduced(20);
    
    // Title at top
    titleLabel.setBounds(bounds.removeFromTop(30));
    bounds.removeFromTop(20);
    
    // Number of tracks section
    auto tracksArea = bounds.removeFromTop(40);
    numTracksLabel.setBounds(tracksArea.removeFromLeft(150));
    tracksArea.removeFromLeft(10);
    numTracksSlider.setBounds(tracksArea);
    bounds.removeFromTop(20);
    
    // Frontend selection section
    auto frontendArea = bounds.removeFromTop(40);
    frontendLabel.setBounds(frontendArea.removeFromLeft(150));
    frontendArea.removeFromLeft(10);
    frontendCombo.setBounds(frontendArea.removeFromLeft(200));
    bounds.removeFromTop(20);
    
    // OK button at bottom
    auto buttonArea = bounds.removeFromBottom(40);
    okButton.setBounds(buttonArea.removeFromRight(100).reduced(5));
    bounds.removeFromBottom(10);
    
    // Audio device selector takes remaining space
    audioDeviceSelector.setBounds(bounds);
}

void StartupDialog::buttonClicked(juce::Button* button)
{
    if (button == &okButton)
    {
        // Update numTracks and selectedFrontend from UI values when OK is clicked
        numTracks = static_cast<int>(numTracksSlider.getValue());
        selectedFrontend = frontendCombo.getText();
        okClicked = true;
        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState(1);
    }
}

void StartupDialog::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
}

