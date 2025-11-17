#include "MainComponent.h"
#include <flowerjuce/CustomLookAndFeel.h>
#include <flowerjuce/Panners/PanningUtils.h>
#include <cmath>

namespace CLEATPinkNoiseTest
{

MainComponent::MainComponent()
    : panLabel("panLabel", "Pan: 0.50, 0.50"),
      levelLabel("levelLabel", "Level: -20.0 dB"),
      debugLabel("debugLabel", "Debug: --"),
      levelSlider(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight),
      startStopButton("Start")
{
    // Initialize random generator
    std::random_device rd;
    randomGenerator.seed(rd());
    
    // Initialize mono buffer
    monoBuffer.resize(maxBufferSize, 0.0f);
    
    // Initialize channel level meters
    for (auto& level : channelLevels)
    {
        level.store(0.0f);
    }
    
    // Initialize max gain channel and within-3dB tracking
    maxGainChannel.store(-1);
    for (auto& flag : channelsWithin3Db)
    {
        flag.store(false);
    }
    
    // Prepare CLEAT panner
    cleatPanner.prepare(44100.0);
    cleatPanner.set_pan(0.5f, 0.5f); // Center position
    
    // Compute initial max gain channel and channels within 3dB (center position)
    auto initialGains = PanningUtils::compute_cleat_gains(0.5f, 0.5f);
    int maxChannel = -1;
    float maxGain = 0.0f;
    for (int i = 0; i < 16; ++i)
    {
        if (initialGains[i] > maxGain)
        {
            maxGain = initialGains[i];
            maxChannel = i;
        }
    }
    maxGainChannel.store(maxChannel);
    
    // Find channels within 3dB of max
    float maxGainDb = linearToDb(maxGain);
    for (int i = 0; i < 16; ++i)
    {
        float gainDb = linearToDb(initialGains[i]);
        channelsWithin3Db[i].store((maxGainDb - gainDb) <= 3.0f);
    }
    
    DBG("[CLEATPinkNoiseTest] MainComponent constructor - panner prepared, pan set to (0.5, 0.5)");
    
    // Setup UI
    panLabel.setJustificationType(juce::Justification::centred);
    panLabel.setFont(juce::Font(16.0f));
    addAndMakeVisible(panLabel);
    
    levelLabel.setJustificationType(juce::Justification::centred);
    levelLabel.setFont(juce::Font(16.0f));
    addAndMakeVisible(levelLabel);
    
    debugLabel.setJustificationType(juce::Justification::centredLeft);
    debugLabel.setFont(juce::Font(12.0f));
    debugLabel.setColour(juce::Label::textColourId, juce::Colours::yellow);
    addAndMakeVisible(debugLabel);
    
    // Setup dB scale slider (-60dB to 0dB)
    levelSlider.setRange(-60.0, 0.0, 0.1);
    levelSlider.setValue(-20.0);
    levelSlider.setTextValueSuffix(" dB");
    levelSlider.addListener(this);
    addAndMakeVisible(levelSlider);
    
    // Initialize output level
    outputLevelDb = -20.0f;
    outputLevelLinear = dbToLinear(outputLevelDb);
    
    // Setup 2D panner component
    panner2DComponent = std::make_unique<Panner2DComponent>();
    panner2DComponent->set_pan_position(0.5f, 0.5f); // Center
    panner2DComponent->m_on_pan_change = [this](float x, float y) {
        panPositionChanged(x, y);
    };
    addAndMakeVisible(panner2DComponent.get());
    
    startStopButton.onClick = [this] { startStopButtonClicked(); };
    addAndMakeVisible(startStopButton);
    
    startAudioButton.setButtonText("Start Audio");
    startAudioButton.onClick = [this] { startAudioButtonClicked(); };
    addAndMakeVisible(startAudioButton);
    
    // Don't initialize audio device manager yet - wait for button click
    audioDeviceInitialized = false;
    
    // Start timer for UI updates
    startTimer(50); // Update every 50ms
    
    setSize(900, 1100);
}

MainComponent::~MainComponent()
{
    stopTimer();
    audioDeviceManager.closeAudioDevice();
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
    
    // Draw "Channel Meters" label above the meters
    if (metersArea.getHeight() > 0)
    {
        auto labelArea = metersArea;
        labelArea.setHeight(20);
        labelArea.translate(0, -20);
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(12.0f, juce::Font::bold));
        g.drawText("Channel Meters", labelArea, juce::Justification::centred);
    }
    
    // Draw channel level meters in a 4x4 grid at the bottom
    // Use the stored metersArea from resized()
    if (metersArea.getHeight() > 50 && metersArea.getWidth() > 50)
    {
        const int numChannels = 16;
        const int cols = 4;
        const int rows = 4;
        const int meterSpacing = 3;
        const int meterWidth = (metersArea.getWidth() - (cols + 1) * meterSpacing) / cols;
        const int meterHeight = (metersArea.getHeight() - (rows + 1) * meterSpacing) / rows;
        
        for (int row = 0; row < rows; ++row)
        {
            for (int col = 0; col < cols; ++col)
            {
                int channel = row * cols + col;
                int x = metersArea.getX() + col * (meterWidth + meterSpacing) + meterSpacing;
                int y = metersArea.getY() + row * (meterHeight + meterSpacing) + meterSpacing;
                
                juce::Rectangle<int> meterRect(x, y, meterWidth, meterHeight);
                float level = channelLevels[channel].load();
                drawChannelMeter(g, meterRect, channel, level);
            }
        }
    }
}

void MainComponent::drawChannelMeter(juce::Graphics& g, juce::Rectangle<int> area, int channel, float level)
{
    // Check if this is the max gain channel or within 3dB
    bool isMaxChannel = (maxGainChannel.load() == channel);
    bool isWithin3Db = channelsWithin3Db[channel].load();
    
    // Background
    g.setColour(juce::Colours::darkgrey);
    g.fillRoundedRectangle(area.toFloat(), 3.0f);
    
    // Border - highlight max channel with cyan, within-3dB with yellow
    if (isMaxChannel)
    {
        g.setColour(juce::Colours::cyan);
        g.drawRoundedRectangle(area.toFloat(), 3.0f, 3.0f); // Thicker border for highlight
    }
    else if (isWithin3Db)
    {
        g.setColour(juce::Colours::yellow);
        g.drawRoundedRectangle(area.toFloat(), 3.0f, 2.0f); // Medium border for within-3dB
    }
    else
    {
        g.setColour(juce::Colours::grey);
        g.drawRoundedRectangle(area.toFloat(), 3.0f, 1.0f);
    }
    
    // Level bar (vertical, bottom to top)
    if (level > 0.001f)
    {
        float levelHeight = area.getHeight() * juce::jlimit(0.0f, 1.0f, level);
        juce::Rectangle<float> levelRect(
            area.getX() + 2.0f,
            area.getBottom() - levelHeight - 2.0f,
            area.getWidth() - 4.0f,
            levelHeight
        );
        
        // Color: green for low, yellow for mid, red for high
        juce::Colour meterColour;
        if (level < 0.5f)
            meterColour = juce::Colours::green;
        else if (level < 0.8f)
            meterColour = juce::Colours::yellow;
        else
            meterColour = juce::Colours::red;
        
        g.setColour(meterColour);
        g.fillRoundedRectangle(levelRect, 2.0f);
    }
    
    // Channel number label - highlight max channel and within-3dB channels
    if (isMaxChannel)
    {
        g.setColour(juce::Colours::cyan);
        g.setFont(juce::Font(10.0f, juce::Font::bold));
    }
    else if (isWithin3Db)
    {
        g.setColour(juce::Colours::yellow);
        g.setFont(juce::Font(10.0f, juce::Font::bold));
    }
    else
    {
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(10.0f));
    }
    g.drawText(juce::String(channel), area, juce::Justification::centredTop);
    
    // Level value in dB
    float levelDb = linearToDb(level);
    g.setFont(juce::Font(8.0f));
    g.drawText(juce::String(levelDb, 1) + " dB", area, juce::Justification::centredBottom);
}

void MainComponent::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop(10);
    
    // Pan label
    auto panLabelArea = area.removeFromTop(30);
    panLabel.setBounds(panLabelArea.reduced(20, 5));
    
    area.removeFromTop(10);
    
    // 2D Panner component (square, but leave room for buttons and meters)
    // Reserve: 30 (pan label) + 10 (spacing) + 25 (debug) + 5 + 30 (level) + 10 + 40 (slider) + 20 + 50 (start audio) + 10 + 50 (stop) + 10 + 20 (label) + meters
    // Meters need about 200 pixels (4 rows * ~50px each)
    int reservedHeight = 30 + 10 + 25 + 5 + 30 + 10 + 40 + 20 + 50 + 10 + 50 + 10 + 20 + 200;
    auto pannerSize = juce::jmin(area.getWidth() - 40, area.getHeight() - reservedHeight);
    pannerSize = juce::jmax(200, pannerSize); // Minimum 200px
    auto pannerArea = area.removeFromTop(pannerSize);
    pannerArea = pannerArea.withSizeKeepingCentre(pannerSize, pannerSize);
    panner2DComponent->setBounds(pannerArea);
    
    area.removeFromTop(20);
    
    // Debug label
    auto debugLabelArea = area.removeFromTop(25);
    debugLabel.setBounds(debugLabelArea.reduced(20, 2));
    
    area.removeFromTop(5);
    
    // Level label
    auto levelLabelArea = area.removeFromTop(30);
    levelLabel.setBounds(levelLabelArea.reduced(20, 5));
    
    area.removeFromTop(10);
    
    // Level slider
    auto sliderArea = area.removeFromTop(40);
    levelSlider.setBounds(sliderArea.reduced(20, 10));
    
    area.removeFromTop(20);
    
    // Start Audio button
    auto startAudioArea = area.removeFromTop(50);
    startAudioButton.setBounds(startAudioArea.reduced(250, 10));
    
    area.removeFromTop(10);
    
    // Start/Stop button
    auto buttonArea = area.removeFromTop(50);
    startStopButton.setBounds(buttonArea.reduced(200, 10));
    
    area.removeFromTop(10);
    
    // Channel meters area (remaining space at bottom)
    // Reserve space for label
    if (area.getHeight() > 20)
    {
        area.removeFromTop(20); // Space for "Channel Meters" label
    }
    metersArea = area;
}

void MainComponent::timerCallback()
{
    static int timerCallCount = 0;
    timerCallCount++;
    
    // Update pan label (show smoothed values, which are what's actually being used)
    float panX = cleatPanner.get_smoothed_pan_x();
    float panY = cleatPanner.get_smoothed_pan_y();
    panLabel.setText("Pan: " + juce::String(panX, 2) + ", " + juce::String(panY, 2), juce::dontSendNotification);
    
    // Update max gain channel and channels within 3dB based on current smoothed pan position
    auto gains = PanningUtils::compute_cleat_gains(panX, panY);
    int maxChannel = -1;
    float maxGain = 0.0f;
    for (int i = 0; i < 16; ++i)
    {
        if (gains[i] > maxGain)
        {
            maxGain = gains[i];
            maxChannel = i;
        }
    }
    maxGainChannel.store(maxChannel);
    
    // Find channels within 3dB of max
    float maxGainDb = linearToDb(maxGain);
    for (int i = 0; i < 16; ++i)
    {
        float gainDb = linearToDb(gains[i]);
        channelsWithin3Db[i].store((maxGainDb - gainDb) <= 3.0f);
    }
    
    // Update level label
    levelLabel.setText("Level: " + juce::String(outputLevelDb, 1) + " dB", juce::dontSendNotification);
    
    // Update debug label with more info
    int callbacks = callbackCount.load();
    int samples = samplesProcessed.load();
    bool playingState = isPlaying;
    
    // Check device state
    auto* device = audioDeviceManager.getCurrentAudioDevice();
    juce::String deviceStatus = "N/A";
    if (device != nullptr)
    {
        deviceStatus = device->isOpen() ? (device->isPlaying() ? "PLAYING" : "OPEN") : "CLOSED";
    }
    
    juce::String debugText = "Callbacks: " + juce::String(callbacks) + 
                            " | Samples: " + juce::String(samples) +
                            " | isPlaying: " + (playingState ? "YES" : "NO") +
                            " | Device: " + deviceStatus;
    debugLabel.setText(debugText, juce::dontSendNotification);
    
    // Debug if we expect callbacks but aren't getting them
    if (playingState && callbacks == 0 && timerCallCount > 20) // After 1 second (20 * 50ms)
    {
        static bool warnedOnce = false;
        if (!warnedOnce)
        {
            DBG("[CLEATPinkNoiseTest] WARNING: isPlaying is TRUE but no callbacks received after 1 second!");
            DBG("  Device state: " << deviceStatus);
            if (device != nullptr)
            {
                DBG("  Device name: " << device->getName());
                DBG("  Device is open: " << (device->isOpen() ? "YES" : "NO"));
                DBG("  Device is playing: " << (device->isPlaying() ? "YES" : "NO"));
            }
            warnedOnce = true;
        }
    }
    
    // Decay channel levels
    for (auto& level : channelLevels)
    {
        float current = level.load();
        if (current > 0.001f)
        {
            level.store(current * levelDecayFactor);
        }
        else
        {
            level.store(0.0f);
        }
    }
    
    // Trigger repaint to update meters
    repaint();
}

void MainComponent::startAudioButtonClicked()
{
    DBG("[CLEATPinkNoiseTest] ===== START AUDIO BUTTON CLICKED =====");
    
    if (!audioDeviceInitialized)
    {
        DBG("[CLEATPinkNoiseTest] Initializing audio device...");
        auto error = audioDeviceManager.initialiseWithDefaultDevices(0, 16);
        if (error.isNotEmpty())
        {
            DBG("[CLEATPinkNoiseTest] ERROR initializing device: " << error);
            juce::AlertWindow::showMessageBox(juce::AlertWindow::WarningIcon,
                                             "Audio Error",
                                             "Failed to initialize audio device: " + error);
            return;
        }
        audioDeviceInitialized = true;
        DBG("[CLEATPinkNoiseTest] Audio device initialized");
    }
    
    // Configure device to enable all output channels
    auto* device = audioDeviceManager.getCurrentAudioDevice();
    if (device == nullptr)
    {
        DBG("[CLEATPinkNoiseTest] ERROR: Device is null!");
        return;
    }
    
    DBG("[CLEATPinkNoiseTest] Configuring device: " << device->getName());
    
    // Get current setup
    juce::AudioDeviceManager::AudioDeviceSetup setup;
    audioDeviceManager.getAudioDeviceSetup(setup);
    
    // Get available output channels
    auto outputNames = device->getOutputChannelNames();
    int numOutputChannels = outputNames.size();
    DBG("[CLEATPinkNoiseTest] Available output channels: " << numOutputChannels);
    
    // Enable all available output channels explicitly
    if (numOutputChannels > 0)
    {
        setup.outputChannels.clear();
        int channelsToEnable = juce::jmin(16, numOutputChannels);
        for (int i = 0; i < channelsToEnable; ++i)
        {
            setup.outputChannels.setBit(i, true);
        }
        setup.useDefaultOutputChannels = false;
        
        DBG("[CLEATPinkNoiseTest] Enabling " << channelsToEnable << " output channels");
        
        // Apply the setup - this will start the device
        auto error = audioDeviceManager.setAudioDeviceSetup(setup, true);
        if (error.isNotEmpty())
        {
            DBG("[CLEATPinkNoiseTest] ERROR setting device setup: " << error);
            return;
        }
        
        DBG("[CLEATPinkNoiseTest] Device setup applied");
        
        // Verify device is playing
        device = audioDeviceManager.getCurrentAudioDevice();
        if (device != nullptr)
        {
            DBG("[CLEATPinkNoiseTest] Device state:");
            DBG("  isOpen: " << (device->isOpen() ? "YES" : "NO"));
            DBG("  isPlaying: " << (device->isPlaying() ? "YES" : "NO"));
            DBG("  Active output channels: " << device->getActiveOutputChannels().countNumberOfSetBits());
            
            if (!device->isPlaying())
            {
                DBG("[CLEATPinkNoiseTest] WARNING: Device is not playing!");
            }
        }
    }
    
    // Prepare panner with actual sample rate
    device = audioDeviceManager.getCurrentAudioDevice();
    if (device != nullptr)
    {
        currentSampleRate = device->getCurrentSampleRate();
        cleatPanner.prepare(currentSampleRate);
        DBG("[CLEATPinkNoiseTest] Panner prepared with sample rate: " << currentSampleRate);
    }
    
    // Add callback - this should start receiving callbacks if device is playing
    DBG("[CLEATPinkNoiseTest] Adding audio callback...");
    audioDeviceManager.addAudioCallback(this);
    DBG("[CLEATPinkNoiseTest] Audio callback added");
    
    // Set playing state
    isPlaying = true;
    callbackCount.store(0);
    samplesProcessed.store(0);
    
    startAudioButton.setButtonText("Audio Running");
    startAudioButton.setEnabled(false);
    
    DBG("[CLEATPinkNoiseTest] isPlaying = TRUE, waiting for callbacks...");
    DBG("[CLEATPinkNoiseTest] ==========================================");
}

void MainComponent::startStopButtonClicked()
{
    if (isPlaying)
    {
        DBG("[CLEATPinkNoiseTest] Stopping audio...");
        DBG("[CLEATPinkNoiseTest] Current callback count: " << callbackCount.load());
        DBG("[CLEATPinkNoiseTest] Current samples processed: " << samplesProcessed.load());
        audioDeviceManager.removeAudioCallback(this);
        audioDeviceManager.closeAudioDevice();
        startStopButton.setButtonText("Start");
        startAudioButton.setButtonText("Start Audio");
        startAudioButton.setEnabled(true);
        isPlaying = false;
        audioDeviceInitialized = false;
        callbackCount.store(0);
        samplesProcessed.store(0);
        DBG("[CLEATPinkNoiseTest] Audio stopped, isPlaying = false");
    }
    else
    {
        // This button now just toggles the test signal generation
        // Actual audio start is handled by startAudioButton
        DBG("[CLEATPinkNoiseTest] Start button clicked (but audio must be started with Start Audio button first)");
    }
}

void MainComponent::levelSliderValueChanged()
{
    outputLevelDb = static_cast<float>(levelSlider.getValue());
    outputLevelLinear = dbToLinear(outputLevelDb);
}

void MainComponent::panPositionChanged(float x, float y)
{
    DBG("[CLEATPinkNoiseTest] Pan position changed: (" << x << ", " << y << ")");
    cleatPanner.set_pan(x, y);
    
    // Get and log current gains for debugging
    auto gains = PanningUtils::compute_cleat_gains(x, y);
    DBG("[CLEATPinkNoiseTest] Computed gains:");
    
    // Find max gain channel
    int maxChannel = -1;
    float maxGain = 0.0f;
    for (int i = 0; i < 16; ++i)
    {
        if (gains[i] > 0.001f)
        {
            DBG("  Channel " << i << ": " << gains[i] << " (" << linearToDb(gains[i]) << " dB)");
        }
        if (gains[i] > maxGain)
        {
            maxGain = gains[i];
            maxChannel = i;
        }
    }
    maxGainChannel.store(maxChannel);
    
    // Find channels within 3dB of max
    float maxGainDb = linearToDb(maxGain);
    for (int i = 0; i < 16; ++i)
    {
        float gainDb = linearToDb(gains[i]);
        channelsWithin3Db[i].store((maxGainDb - gainDb) <= 3.0f);
    }
    
    DBG("[CLEATPinkNoiseTest] Max gain channel: " << maxChannel << " (gain: " << maxGain << ", " << maxGainDb << " dB)");
}

float MainComponent::dbToLinear(float db)
{
    return std::pow(10.0f, db / 20.0f);
}

float MainComponent::linearToDb(float linear)
{
    if (linear > 0.0f)
        return 20.0f * std::log10(linear);
    return -60.0f; // Return minimum dB for zero/negative values
}

void MainComponent::sliderValueChanged(juce::Slider* slider)
{
    if (slider == &levelSlider)
    {
        levelSliderValueChanged();
    }
}

float MainComponent::generatePinkNoise()
{
    // Generate white noise
    float white = whiteNoiseDist(randomGenerator);
    
    // Apply pink noise filter (7-stage)
    pinkNoiseState[0] = 0.99886f * pinkNoiseState[0] + white * 0.0555179f;
    pinkNoiseState[1] = 0.99332f * pinkNoiseState[1] + white * 0.0750759f;
    pinkNoiseState[2] = 0.96900f * pinkNoiseState[2] + white * 0.1538520f;
    pinkNoiseState[3] = 0.86650f * pinkNoiseState[3] + white * 0.3104856f;
    pinkNoiseState[4] = 0.55000f * pinkNoiseState[4] + white * 0.5329522f;
    pinkNoiseState[5] = -0.7616f * pinkNoiseState[5] - white * 0.0168980f;
    
    float pink = pinkNoiseState[0] + pinkNoiseState[1] + pinkNoiseState[2] + 
                 pinkNoiseState[3] + pinkNoiseState[4] + pinkNoiseState[5] + 
                 pinkNoiseState[6] + white * 0.5362f;
    
    pinkNoiseState[6] = white * 0.115926f;
    
    return pink * 0.11f; // Scale to reasonable level
}

void MainComponent::audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                                     int numInputChannels,
                                                     float* const* outputChannelData,
                                                     int numOutputChannels,
                                                     int numSamples,
                                                     const juce::AudioIODeviceCallbackContext& context)
{
    static int callCount = 0;
    callCount++;
    callbackCount.store(callCount);
    
    // Always debug first 10 callbacks to see what's happening
    bool shouldDebug = (callCount <= 10) || (callCount % 1000 == 0);
    
    if (shouldDebug || callCount == 1)
    {
        DBG("[CLEATPinkNoiseTest] ===== AUDIO CALLBACK #" << callCount << " =====");
        DBG("  numInputChannels: " << numInputChannels);
        DBG("  numOutputChannels: " << numOutputChannels);
        DBG("  numSamples: " << numSamples);
        DBG("  isPlaying: " << (isPlaying ? "YES" : "NO"));
        DBG("  outputLevelLinear: " << outputLevelLinear);
        DBG("  outputLevelDb: " << outputLevelDb);
        
        // Check if output channels are valid
        int validOutputChannels = 0;
        for (int i = 0; i < numOutputChannels; ++i)
        {
            if (outputChannelData[i] != nullptr)
                validOutputChannels++;
        }
        DBG("  Valid output channel pointers: " << validOutputChannels << " / " << numOutputChannels);
    }
    
    // Clear all outputs
    for (int channel = 0; channel < numOutputChannels; ++channel)
    {
        if (outputChannelData[channel] != nullptr)
        {
            juce::FloatVectorOperations::clear(outputChannelData[channel], numSamples);
        }
        else if (shouldDebug && channel < 16)
        {
            DBG("  WARNING: outputChannelData[" << channel << "] is NULL!");
        }
    }
    
    // Always process audio even if isPlaying is false initially (might be race condition)
    // But log it for debugging
    if (!isPlaying)
    {
        if (callCount <= 10)
        {
            DBG("  WARNING: Callback called but isPlaying is FALSE!");
            DBG("  This might be a race condition - callback started before flag was set");
        }
        // Don't return early - process anyway to see if callback is working
        // return;
    }
    
    // Ensure mono buffer is large enough
    if (monoBuffer.size() < static_cast<size_t>(numSamples))
    {
        monoBuffer.resize(numSamples, 0.0f);
        if (shouldDebug)
            DBG("  Resized monoBuffer to " << numSamples << " samples");
    }
    
    // Generate pink noise into mono buffer
    float maxInputSample = 0.0f;
    for (int sample = 0; sample < numSamples; ++sample)
    {
        float noise = generatePinkNoise();
        monoBuffer[sample] = noise * outputLevelLinear;
        maxInputSample = juce::jmax(maxInputSample, std::abs(monoBuffer[sample]));
    }
    
    if (shouldDebug)
    {
        DBG("  Generated pink noise, max input sample: " << maxInputSample);
        DBG("  Input level (dB): " << linearToDb(maxInputSample));
    }
    
    // Prepare input channel data pointer array for panner
    const float* monoInputChannelData[1] = { monoBuffer.data() };
    
    // Get current pan position for debugging
    float panX = cleatPanner.get_pan_x();
    float panY = cleatPanner.get_pan_y();
    
    if (shouldDebug)
    {
        DBG("  Pan position: (" << panX << ", " << panY << ")");
    }
    
    // Process through CLEAT panner
    cleatPanner.process_block(monoInputChannelData, 1, outputChannelData, numOutputChannels, numSamples);
    
    // Update level meters and check for output
    float maxOutputSample = 0.0f;
    int activeChannels = 0;
    for (int channel = 0; channel < juce::jmin(16, numOutputChannels); ++channel)
    {
        if (outputChannelData[channel] != nullptr)
        {
            // Find peak in this channel
            float peak = 0.0f;
            for (int sample = 0; sample < numSamples; ++sample)
            {
                peak = juce::jmax(peak, std::abs(outputChannelData[channel][sample]));
            }
            
            // Update level meter (peak hold with decay)
            float currentLevel = channelLevels[channel].load();
            if (peak > currentLevel)
            {
                channelLevels[channel].store(peak);
            }
            
            maxOutputSample = juce::jmax(maxOutputSample, peak);
            if (peak > 0.001f)
                activeChannels++;
        }
    }
    
    samplesProcessed.store(samplesProcessed.load() + numSamples);
    
    if (shouldDebug)
    {
        DBG("  After panner processing:");
        DBG("    Max output sample: " << maxOutputSample << " (" << linearToDb(maxOutputSample) << " dB)");
        DBG("    Active channels (level > 0.001): " << activeChannels);
        
        // Log first few channel levels
        for (int i = 0; i < juce::jmin(4, numOutputChannels); ++i)
        {
            if (outputChannelData[i] != nullptr)
            {
                float sample = outputChannelData[i][0];
                DBG("    Channel " << i << " first sample: " << sample);
            }
        }
    }
    
    // Periodic debug every 5 seconds (at 44.1kHz, ~220500 samples)
    if (samplesProcessed.load() % 220500 < numSamples)
    {
        DBG("[CLEATPinkNoiseTest] Status update:");
        DBG("  Callbacks: " << callCount);
        DBG("  Samples processed: " << samplesProcessed.load());
        DBG("  Active channels: " << activeChannels);
        DBG("  Max output level: " << maxOutputSample << " (" << linearToDb(maxOutputSample) << " dB)");
    }
}

void MainComponent::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    if (device != nullptr)
    {
        currentSampleRate = device->getCurrentSampleRate();
        DBG("[CLEATPinkNoiseTest] ===== Audio device about to start =====");
        DBG("  Device name: " << device->getName());
        DBG("  Sample rate: " << currentSampleRate);
        DBG("  Buffer size: " << device->getCurrentBufferSizeSamples());
        DBG("  Output channels (active): " << device->getActiveOutputChannels().countNumberOfSetBits());
        DBG("  Output channels (total): " << device->getOutputChannelNames().size());
        
        // Log all output channel names
        auto outputNames = device->getOutputChannelNames();
        DBG("  Output channel names:");
        for (int i = 0; i < outputNames.size(); ++i)
        {
            bool isActive = device->getActiveOutputChannels()[i];
            DBG("    [" << i << "] " << outputNames[i] << (isActive ? " (ACTIVE)" : " (inactive)"));
        }
        
        // Prepare panner with actual sample rate
        cleatPanner.prepare(currentSampleRate);
        DBG("  Panner prepared with sample rate: " << currentSampleRate);
        
        // Get initial gains
        float panX = cleatPanner.get_pan_x();
        float panY = cleatPanner.get_pan_y();
        auto gains = PanningUtils::compute_cleat_gains(panX, panY);
        DBG("  Initial pan position: (" << panX << ", " << panY << ")");
        DBG("  Initial gains:");
        for (int i = 0; i < 16; ++i)
        {
            if (gains[i] > 0.001f)
            {
                DBG("    Channel " << i << ": " << gains[i] << " (" << linearToDb(gains[i]) << " dB)");
            }
        }
        DBG("[CLEATPinkNoiseTest] ========================================");
    }
    else
    {
        DBG("[CLEATPinkNoiseTest] ERROR: audioDeviceAboutToStart called with null device!");
    }
}

void MainComponent::audioDeviceStopped()
{
    DBG("[CLEATPinkNoiseTest] ===== Audio device stopped =====");
    DBG("  Total callbacks processed: " << callbackCount.load());
    DBG("  Total samples processed: " << samplesProcessed.load());
    DBG("[CLEATPinkNoiseTest] ==================================");
}

} // namespace CLEATPinkNoiseTest

