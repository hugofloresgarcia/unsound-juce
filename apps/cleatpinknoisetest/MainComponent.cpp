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
    
    // Set initial gain power
    cleatPanner.set_gain_power(cleatGainPower);
    
    // Compute initial max gain channel and channels within 3dB (center position)
    auto initialGains = PanningUtils::compute_cleat_gains(0.5f, 0.5f, cleatGainPower);
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
    
    // Setup sinks button
    sinksButton.setButtonText("Sinks");
    sinksButton.onClick = [this] { sinksButtonClicked(); };
    addAndMakeVisible(sinksButton);
    
    // Setup gain power slider
    gainPowerLabel.setText("CLEAT Gain Power:", juce::dontSendNotification);
    gainPowerLabel.setJustificationType(juce::Justification::centredLeft);
    gainPowerLabel.setFont(juce::Font(14.0f));
    addAndMakeVisible(gainPowerLabel);
    
    gainPowerSlider.setRange(0.1, 100.0, 0.1);
    gainPowerSlider.setValue(cleatGainPower);
    gainPowerSlider.setTextValueSuffix("");
    gainPowerSlider.addListener(this);
    addAndMakeVisible(gainPowerSlider);
    
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
    
    // Draw channel level meters in a 4x4 grid
    // Use the stored metersArea from resized()
    if (metersArea.getHeight() > 50 && metersArea.getWidth() > 50)
    {
        const int numChannels = 16;
        const int cols = 4;
        const int rows = 4;
        const int meterSpacing = 10;
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
    
    // Convert linear level to dB
    float levelDb = linearToDb(level);
    
    // Define dB range: -60dB to 0dB (full range), healthy range: -40dB to -15dB
    const float minDb = -60.0f;
    const float maxDb = 0.0f;
    const float silenceThresholdDb = -50.0f; // Below this is considered silence
    const float healthyMinDb = -40.0f;
    const float healthyMaxDb = -15.0f;
    
    // Check if level is effectively silent (below threshold or very small linear value)
    bool isSilent = (level < 0.0001f) || (levelDb < silenceThresholdDb);
    
    // Map dB to normalized value (0.0 = minDb, 1.0 = maxDb)
    float normalizedLevel = juce::jlimit(0.0f, 1.0f, (levelDb - minDb) / (maxDb - minDb));
    
    // Calculate circle center and maximum radius
    float centerX = area.getCentreX();
    float centerY = area.getCentreY() - 10.0f; // Offset upward to leave room for dB label
    float maxRadius = juce::jmin(static_cast<float>(area.getWidth()), static_cast<float>(area.getHeight()) - 20.0f) * 0.4f; // Leave room for label
    
    // Calculate radius based on normalized level (minimum radius for visibility)
    float minRadius = maxRadius * 0.1f; // Minimum 10% of max radius
    float radius = minRadius + (maxRadius - minRadius) * normalizedLevel;
    
    // Color based on dB range:
    // Silent (below -50dB or < 0.0001 linear): dim gray
    // Below -40dB: dim gray (too quiet)
    // -40dB to -15dB: green (healthy)
    // -15dB to 0dB: yellow/red (too loud)
    juce::Colour circleColour;
    if (isSilent || levelDb < healthyMinDb)
    {
        circleColour = juce::Colours::darkgrey.withBrightness(0.3f);
    }
    else if (levelDb <= healthyMaxDb)
    {
        circleColour = juce::Colours::green;
    }
    else if (levelDb < -5.0f)
    {
        circleColour = juce::Colours::yellow;
    }
    else
    {
        circleColour = juce::Colours::red;
    }
    
    // Draw circle
    if (!isSilent && levelDb > minDb)
    {
        g.setColour(circleColour);
        g.fillEllipse(centerX - radius, centerY - radius, radius * 2.0f, radius * 2.0f);
        
        // Draw border - highlight max channel with cyan, within-3dB with yellow
        if (isMaxChannel)
        {
            g.setColour(juce::Colours::cyan);
            g.drawEllipse(centerX - radius, centerY - radius, radius * 2.0f, radius * 2.0f, 2.5f);
        }
        else if (isWithin3Db)
        {
            g.setColour(juce::Colours::yellow);
            g.drawEllipse(centerX - radius, centerY - radius, radius * 2.0f, radius * 2.0f, 1.5f);
        }
        else
        {
            g.setColour(circleColour.brighter(0.3f));
            g.drawEllipse(centerX - radius, centerY - radius, radius * 2.0f, radius * 2.0f, 1.5f);
        }
    }
    else
    {
        // Draw very faint circle for silence
        g.setColour(juce::Colours::darkgrey.withAlpha(0.2f));
        g.drawEllipse(centerX - minRadius, centerY - minRadius, minRadius * 2.0f, minRadius * 2.0f, 1.0f);
        // Don't show dB value for silence - use minDb for display
        levelDb = minDb; // Use minDb for display when silent
    }
    
    // Channel number label (above circle) - highlight max channel and within-3dB channels
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
    auto channelLabelArea = area;
    channelLabelArea.removeFromBottom(area.getHeight() - static_cast<int>(centerY - maxRadius - 5));
    g.drawText(juce::String(channel), channelLabelArea, juce::Justification::centred);
    
    // Level value in dB (below circle) - make it clearly visible
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(10.0f));
    auto dbLabelArea = area;
    dbLabelArea.removeFromTop(static_cast<int>(centerY + maxRadius + 8));
    juce::String dbText;
    if (isSilent)
    {
        dbText = "-inf dB";
    }
    else
    {
        dbText = juce::String(levelDb, 1) + " dB";
    }
    g.drawText(dbText, dbLabelArea, juce::Justification::centred);
}

void MainComponent::resized()
{
    // ============================================================================
    // Size and margin constants
    // ============================================================================
    const int topMargin = 10;
    const int horizontalMargin = 20;
    
    // Label sizes
    const int panLabelHeight = 30;
    const int panLabelVerticalMargin = 5;
    const int debugLabelHeight = 25;
    const int debugLabelVerticalMargin = 2;
    const int levelLabelHeight = 30;
    const int levelLabelVerticalMargin = 5;
    const int gainPowerLabelHeight = 30;
    const int gainPowerLabelVerticalMargin = 5;
    const int channelMetersLabelHeight = 20;
    
    // Spacing between elements
    const int spacingAfterPanLabel = 10;
    const int spacingAfterPanner = 20;
    const int spacingAfterDebugLabel = 5;
    const int spacingAfterLevelLabel = 10;
    const int spacingAfterLevelSlider = 20;
    const int spacingAfterGainPowerLabel = 5;
    const int spacingAfterGainPowerSlider = 20;
    const int spacingAfterStartAudioButton = 10;
    const int spacingAfterStartStopButton = 10;
    const int spacingAfterSinksButton = 10;
    
    // Slider sizes
    const int levelSliderHeight = 40;
    const int levelSliderVerticalMargin = 10;
    const int gainPowerSliderHeight = 40;
    const int gainPowerSliderVerticalMargin = 10;
    
    // Button sizes
    const int startAudioButtonHeight = 50;
    const int startAudioButtonHorizontalMargin = 250;
    const int startAudioButtonVerticalMargin = 10;
    const int startStopButtonHeight = 50;
    const int startStopButtonHorizontalMargin = 200;
    const int startStopButtonVerticalMargin = 10;
    const int sinksButtonHeight = 50;
    const int sinksButtonHorizontalMargin = 200;
    const int sinksButtonVerticalMargin = 10;
    
    // Panner settings
    const int pannerHorizontalMargin = 40;
    const int pannerMinSize = 200;
    
    // Meters area
    const int metersAreaEstimatedHeight = 200; // 4 rows * ~50px each
    
    // ============================================================================
    // Calculate reserved height for panner sizing
    // ============================================================================
    int reservedHeight = topMargin
                       + panLabelHeight + spacingAfterPanLabel
                       + spacingAfterPanner
                       + debugLabelHeight + spacingAfterDebugLabel
                       + levelLabelHeight + spacingAfterLevelLabel
                       + levelSliderHeight + spacingAfterLevelSlider
                       + gainPowerLabelHeight + spacingAfterGainPowerLabel
                       + gainPowerSliderHeight + spacingAfterGainPowerSlider
                       + startAudioButtonHeight + spacingAfterStartAudioButton
                       + startStopButtonHeight + spacingAfterStartStopButton
                       + sinksButtonHeight + spacingAfterSinksButton
                       + channelMetersLabelHeight
                       + metersAreaEstimatedHeight;
    
    // ============================================================================
    // Layout UI elements
    // ============================================================================
    auto area = getLocalBounds();
    area.removeFromTop(topMargin);
    
    // Pan label
    auto panLabelArea = area.removeFromTop(panLabelHeight);
    panLabel.setBounds(panLabelArea.reduced(horizontalMargin, panLabelVerticalMargin));
    
    area.removeFromTop(spacingAfterPanLabel);
    
    // 2D Panner component (square, but leave room for buttons and meters)
    auto pannerSize = juce::jmin(area.getWidth() - pannerHorizontalMargin, area.getHeight() - reservedHeight);
    pannerSize = juce::jmax(pannerMinSize, pannerSize);
    auto pannerArea = area.removeFromTop(pannerSize);
    pannerArea = pannerArea.withSizeKeepingCentre(pannerSize, pannerSize);
    panner2DComponent->setBounds(pannerArea);
    
    area.removeFromTop(spacingAfterPanner);
    
    // Debug label
    auto debugLabelArea = area.removeFromTop(debugLabelHeight);
    debugLabel.setBounds(debugLabelArea.reduced(horizontalMargin, debugLabelVerticalMargin));
    
    area.removeFromTop(spacingAfterDebugLabel);
    
    // Level label
    auto levelLabelArea = area.removeFromTop(levelLabelHeight);
    levelLabel.setBounds(levelLabelArea.reduced(horizontalMargin, levelLabelVerticalMargin));
    
    area.removeFromTop(spacingAfterLevelLabel);
    
    // Level slider
    auto levelSliderArea = area.removeFromTop(levelSliderHeight);
    levelSlider.setBounds(levelSliderArea.reduced(horizontalMargin, levelSliderVerticalMargin));
    
    area.removeFromTop(spacingAfterLevelSlider);
    
    // Gain power label
    auto gainPowerLabelArea = area.removeFromTop(gainPowerLabelHeight);
    gainPowerLabel.setBounds(gainPowerLabelArea.reduced(horizontalMargin, gainPowerLabelVerticalMargin));
    
    area.removeFromTop(spacingAfterGainPowerLabel);
    
    // Gain power slider
    auto gainPowerSliderArea = area.removeFromTop(gainPowerSliderHeight);
    gainPowerSlider.setBounds(gainPowerSliderArea.reduced(horizontalMargin, gainPowerSliderVerticalMargin));
    
    area.removeFromTop(spacingAfterGainPowerSlider);
    
    // Start Audio button
    auto startAudioArea = area.removeFromTop(startAudioButtonHeight);
    startAudioButton.setBounds(startAudioArea.reduced(startAudioButtonHorizontalMargin, startAudioButtonVerticalMargin));
    
    area.removeFromTop(spacingAfterStartAudioButton);
    
    // Start/Stop button
    auto startStopButtonArea = area.removeFromTop(startStopButtonHeight);
    startStopButton.setBounds(startStopButtonArea.reduced(startStopButtonHorizontalMargin, startStopButtonVerticalMargin));
    
    area.removeFromTop(spacingAfterStartStopButton);
    
    // Sinks button
    auto sinksButtonArea = area.removeFromTop(sinksButtonHeight);
    sinksButton.setBounds(sinksButtonArea.reduced(sinksButtonHorizontalMargin, sinksButtonVerticalMargin));
    
    area.removeFromTop(spacingAfterSinksButton);
    
    // Channel meters area (remaining space at bottom)
    // Reserve space for label
    if (area.getHeight() > channelMetersLabelHeight)
    {
        area.removeFromTop(channelMetersLabelHeight);
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
    auto gains = PanningUtils::compute_cleat_gains(panX, panY, cleatGainPower);
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

void MainComponent::gainPowerSliderValueChanged()
{
    float newGainPower = static_cast<float>(gainPowerSlider.getValue());
    setCLEATGainPower(newGainPower);
}

void MainComponent::setCLEATGainPower(float gainPower)
{
    cleatGainPower = gainPower;
    cleatPanner.set_gain_power(gainPower);
    DBG("[CLEATPinkNoiseTest] CLEAT gain power updated to " << gainPower);
}

void MainComponent::sinksButtonClicked()
{
    // Check if window needs to be created or recreated (if closed by user)
    if (sinksWindow == nullptr || sinksComponent == nullptr || 
        (sinksWindow != nullptr && !sinksWindow->isVisible()))
    {
        // If window exists but was closed, clean it up first
        if (sinksWindow != nullptr)
        {
            sinksWindow = nullptr;
            sinksComponent = nullptr;
        }
        
        // Create sinks component
        sinksComponent = std::make_unique<flowerjuce::SinksWindow>(&cleatPanner, channelLevels);
        
        // Create dialog window
        sinksWindow = std::make_unique<SinksDialogWindow>(
            "Sinks",
            juce::Colours::black
        );
        
        // Transfer ownership to DialogWindow (release from unique_ptr)
        sinksWindow->setContentOwned(sinksComponent.release(), true);
        sinksWindow->setResizable(true, true);
        sinksWindow->setSize(500, 500);
        sinksWindow->centreWithSize(500, 500);
        sinksWindow->setVisible(true);
        sinksWindow->toFront(true);
    }
    else
    {
        // Window already exists and is visible, just bring it to front
        sinksWindow->toFront(true);
    }
}

void MainComponent::panPositionChanged(float x, float y)
{
    DBG("[CLEATPinkNoiseTest] Pan position changed: (" << x << ", " << y << ")");
    cleatPanner.set_pan(x, y);
    
    // Get and log current gains for debugging
    auto gains = PanningUtils::compute_cleat_gains(x, y, cleatGainPower);
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
    else if (slider == &gainPowerSlider)
    {
        gainPowerSliderValueChanged();
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
        auto gains = PanningUtils::compute_cleat_gains(panX, panY, cleatGainPower);
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

