#include "Synths.h"
#include "../VampNet/ClickSynth.h"
#include "../VampNet/Sampler.h"

using namespace WhAM;

// SynthsWindow::ContentComponent implementation
SynthsWindow::ContentComponent::ContentComponent(VampNetMultiTrackLooperEngine& engine, int numTracks, Shared::MidiLearnManager* midiManager)
    : looperEngine(engine), midiLearnManager(midiManager),
      clickSynthParameterId("clicksynth_trigger"), samplerParameterId("sampler_trigger")
{
    // Setup tab buttons (Sampler first)
    samplerTabButton.setButtonText("Sampler");
    samplerTabButton.setToggleState(true, juce::dontSendNotification);
    samplerTabButton.onClick = [this] { tabButtonClicked(0); };
    addAndMakeVisible(samplerTabButton);
    
    clickSynthTabButton.setButtonText("Beep");
    clickSynthTabButton.setToggleState(false, juce::dontSendNotification);
    clickSynthTabButton.onClick = [this] { tabButtonClicked(1); };
    addAndMakeVisible(clickSynthTabButton);
    
    // Setup Beep controls
    clickSynthEnableButton.setButtonText("Enable Beep");
    clickSynthEnableButton.setToggleState(false, juce::dontSendNotification);
    clickSynthEnableButton.onClick = [this] { clickSynthEnableButtonChanged(); };
    addAndMakeVisible(clickSynthEnableButton);
    
    clickSynthTrackLabel.setText("Destination Track:", juce::dontSendNotification);
    clickSynthTrackLabel.attachToComponent(&clickSynthTrackSelector, true);
    addAndMakeVisible(clickSynthTrackLabel);
    
    clickSynthTrackSelector.addItem("All Tracks", 1);
    for (int i = 0; i < numTracks; ++i)
    {
        clickSynthTrackSelector.addItem("Track " + juce::String(i + 1), i + 2);
    }
    clickSynthTrackSelector.setSelectedId(2); // Track 0 by default
    clickSynthSelectedTrack.store(0);
    clickSynthTrackSelector.onChange = [this] { clickSynthTrackSelectorChanged(); };
    addAndMakeVisible(clickSynthTrackSelector);
    
    frequencyLabel.setText("Frequency (Hz):", juce::dontSendNotification);
    frequencyLabel.attachToComponent(&frequencySlider, true);
    addAndMakeVisible(frequencyLabel);
    
    frequencySlider.setRange(100.0, 5000.0, 10.0);
    frequencySlider.setValue(1000.0);
    frequencySlider.setTextValueSuffix(" Hz");
    frequencySlider.onValueChange = [this] { frequencySliderChanged(); };
    addAndMakeVisible(frequencySlider);
    
    durationLabel.setText("Duration (ms):", juce::dontSendNotification);
    durationLabel.attachToComponent(&durationSlider, true);
    addAndMakeVisible(durationLabel);
    
    durationSlider.setRange(1.0, 100.0, 1.0);
    durationSlider.setValue(10.0);
    durationSlider.setTextValueSuffix(" ms");
    durationSlider.onValueChange = [this] { durationSliderChanged(); };
    addAndMakeVisible(durationSlider);
    
    amplitudeLabel.setText("Amplitude:", juce::dontSendNotification);
    amplitudeLabel.attachToComponent(&amplitudeSlider, true);
    addAndMakeVisible(amplitudeLabel);
    
    amplitudeSlider.setRange(0.0, 1.0, 0.01);
    amplitudeSlider.setValue(0.8);
    amplitudeSlider.onValueChange = [this] { amplitudeSliderChanged(); };
    addAndMakeVisible(amplitudeSlider);
    
    clickSynthTriggerButton.setButtonText("Trigger");
    clickSynthTriggerButton.onClick = [this] { clickSynthTriggerButtonClicked(); };
    addAndMakeVisible(clickSynthTriggerButton);
    
    if (midiLearnManager)
    {
        clickSynthTriggerLearnable = std::make_unique<Shared::MidiLearnable>(*midiLearnManager, clickSynthParameterId);
        clickSynthTriggerMouseListener = std::make_unique<Shared::MidiLearnMouseListener>(*clickSynthTriggerLearnable, this);
        clickSynthTriggerButton.addMouseListener(clickSynthTriggerMouseListener.get(), false);
        
        midiLearnManager->registerParameter({
            clickSynthParameterId,
            [this](float value) {
                if (value > 0.5f && clickSynthEnabled.load())
                    clickSynthTriggerButtonClicked();
            },
            [this]() { return 0.0f; },
            "Beep Trigger",
            true
        });
    }
    
    clickSynthInstructionsLabel.setText("Press 'k' or click Trigger to trigger a beep", juce::dontSendNotification);
    clickSynthInstructionsLabel.setJustificationType(juce::Justification::centred);
    clickSynthInstructionsLabel.setFont(juce::Font(juce::FontOptions().withHeight(12.0f)));
    clickSynthInstructionsLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible(clickSynthInstructionsLabel);
    
    // Setup Sampler controls
    samplerEnableButton.setButtonText("Enable Sampler");
    samplerEnableButton.setToggleState(false, juce::dontSendNotification);
    samplerEnableButton.onClick = [this] { samplerEnableButtonChanged(); };
    addAndMakeVisible(samplerEnableButton);
    
    samplerTrackLabel.setText("Destination Track:", juce::dontSendNotification);
    samplerTrackLabel.attachToComponent(&samplerTrackSelector, true);
    addAndMakeVisible(samplerTrackLabel);
    
    samplerTrackSelector.addItem("All Tracks", 1);
    for (int i = 0; i < numTracks; ++i)
    {
        samplerTrackSelector.addItem("Track " + juce::String(i + 1), i + 2);
    }
    samplerTrackSelector.setSelectedId(2); // Track 0 by default
    samplerSelectedTrack.store(0);
    samplerTrackSelector.onChange = [this] { samplerTrackSelectorChanged(); };
    addAndMakeVisible(samplerTrackSelector);
    
    loadSampleButton.setButtonText("Load Sample...");
    loadSampleButton.onClick = [this] { loadSampleButtonClicked(); };
    addAndMakeVisible(loadSampleButton);
    
    sampleNameLabel.setText("No sample loaded", juce::dontSendNotification);
    sampleNameLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(sampleNameLabel);
    
    samplerTriggerButton.setButtonText("Trigger");
    samplerTriggerButton.onClick = [this] { samplerTriggerButtonClicked(); };
    addAndMakeVisible(samplerTriggerButton);
    
    if (midiLearnManager)
    {
        samplerTriggerLearnable = std::make_unique<Shared::MidiLearnable>(*midiLearnManager, samplerParameterId);
        samplerTriggerMouseListener = std::make_unique<Shared::MidiLearnMouseListener>(*samplerTriggerLearnable, this);
        samplerTriggerButton.addMouseListener(samplerTriggerMouseListener.get(), false);
        
        midiLearnManager->registerParameter({
            samplerParameterId,
            [this](float value) {
                if (value > 0.5f && samplerEnabled.load())
                    samplerTriggerButtonClicked();
            },
            [this]() { return 0.0f; },
            "Sampler Trigger",
            true
        });
    }
    
    samplerInstructionsLabel.setText("Press 'k' or click Trigger to trigger the sample", juce::dontSendNotification);
    samplerInstructionsLabel.setJustificationType(juce::Justification::centred);
    samplerInstructionsLabel.setFont(juce::Font(juce::FontOptions().withHeight(12.0f)));
    samplerInstructionsLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible(samplerInstructionsLabel);
}

SynthsWindow::ContentComponent::~ContentComponent()
{
    if (clickSynthTriggerMouseListener)
        clickSynthTriggerButton.removeMouseListener(clickSynthTriggerMouseListener.get());
    if (samplerTriggerMouseListener)
        samplerTriggerButton.removeMouseListener(samplerTriggerMouseListener.get());
    
    if (midiLearnManager)
    {
        midiLearnManager->unregisterParameter(clickSynthParameterId);
        midiLearnManager->unregisterParameter(samplerParameterId);
    }
}

void SynthsWindow::ContentComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
    
    // Draw MIDI indicators
    if (clickSynthTriggerLearnable && clickSynthTriggerLearnable->hasMidiMapping())
    {
        auto buttonBounds = clickSynthTriggerButton.getBounds();
        g.setColour(juce::Colour(0xffed1683));
        g.fillEllipse(buttonBounds.getRight() - 8.0f, buttonBounds.getY() + 2.0f, 6.0f, 6.0f);
    }
    
    if (samplerTriggerLearnable && samplerTriggerLearnable->hasMidiMapping())
    {
        auto buttonBounds = samplerTriggerButton.getBounds();
        g.setColour(juce::Colour(0xffed1683));
        g.fillEllipse(buttonBounds.getRight() - 8.0f, buttonBounds.getY() + 2.0f, 6.0f, 6.0f);
    }
}

void SynthsWindow::ContentComponent::resized()
{
    auto bounds = getLocalBounds().reduced(10);
    const int rowHeight = 30;
    const int spacing = 10;
    const int tabHeight = 35;
    
    // Tab buttons (Sampler first)
    auto tabArea = bounds.removeFromTop(tabHeight);
    samplerTabButton.setBounds(tabArea.removeFromLeft(120));
    tabArea.removeFromLeft(5);
    clickSynthTabButton.setBounds(tabArea.removeFromLeft(120));
    bounds.removeFromTop(spacing);
    
    if (showingClickSynth.load())
    {
        // Beep controls
        clickSynthEnableButton.setVisible(true);
        clickSynthTrackLabel.setVisible(true);
        clickSynthTrackSelector.setVisible(true);
        frequencyLabel.setVisible(true);
        frequencySlider.setVisible(true);
        durationLabel.setVisible(true);
        durationSlider.setVisible(true);
        amplitudeLabel.setVisible(true);
        amplitudeSlider.setVisible(true);
        clickSynthTriggerButton.setVisible(true);
        clickSynthInstructionsLabel.setVisible(true);
        
        samplerEnableButton.setVisible(false);
        samplerTrackLabel.setVisible(false);
        samplerTrackSelector.setVisible(false);
        loadSampleButton.setVisible(false);
        sampleNameLabel.setVisible(false);
        samplerTriggerButton.setVisible(false);
        samplerInstructionsLabel.setVisible(false);
        
        clickSynthEnableButton.setBounds(bounds.removeFromTop(rowHeight));
        bounds.removeFromTop(spacing);
        
        auto trackArea = bounds.removeFromTop(rowHeight);
        clickSynthTrackLabel.setBounds(trackArea.removeFromLeft(120));
        trackArea.removeFromLeft(5);
        clickSynthTrackSelector.setBounds(trackArea);
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
        
        clickSynthTriggerButton.setBounds(bounds.removeFromTop(rowHeight));
        bounds.removeFromTop(spacing);
        clickSynthInstructionsLabel.setBounds(bounds.removeFromTop(20));
    }
    else
    {
        // Sampler controls (shown first by default)
        clickSynthEnableButton.setVisible(false);
        clickSynthTrackLabel.setVisible(false);
        clickSynthTrackSelector.setVisible(false);
        frequencyLabel.setVisible(false);
        frequencySlider.setVisible(false);
        durationLabel.setVisible(false);
        durationSlider.setVisible(false);
        amplitudeLabel.setVisible(false);
        amplitudeSlider.setVisible(false);
        clickSynthTriggerButton.setVisible(false);
        clickSynthInstructionsLabel.setVisible(false);
        
        samplerEnableButton.setVisible(true);
        samplerTrackLabel.setVisible(true);
        samplerTrackSelector.setVisible(true);
        loadSampleButton.setVisible(true);
        sampleNameLabel.setVisible(true);
        samplerTriggerButton.setVisible(true);
        samplerInstructionsLabel.setVisible(true);
        
        samplerEnableButton.setBounds(bounds.removeFromTop(rowHeight));
        bounds.removeFromTop(spacing);
        
        auto trackArea = bounds.removeFromTop(rowHeight);
        samplerTrackLabel.setBounds(trackArea.removeFromLeft(120));
        trackArea.removeFromLeft(5);
        samplerTrackSelector.setBounds(trackArea);
        bounds.removeFromTop(spacing);
        
        auto loadArea = bounds.removeFromTop(rowHeight);
        loadSampleButton.setBounds(loadArea.removeFromLeft(120));
        loadArea.removeFromLeft(5);
        sampleNameLabel.setBounds(loadArea);
        bounds.removeFromTop(spacing);
        
        samplerTriggerButton.setBounds(bounds.removeFromTop(rowHeight));
        bounds.removeFromTop(spacing);
        samplerInstructionsLabel.setBounds(bounds.removeFromTop(20));
    }
}

void SynthsWindow::ContentComponent::tabButtonClicked(int tabIndex)
{
    // tabIndex 0 = Sampler, tabIndex 1 = Beep
    showingClickSynth.store(tabIndex == 1);
    samplerTabButton.setToggleState(tabIndex == 0, juce::dontSendNotification);
    clickSynthTabButton.setToggleState(tabIndex == 1, juce::dontSendNotification);
    resized();
}

void SynthsWindow::ContentComponent::clickSynthEnableButtonChanged()
{
    clickSynthEnabled.store(clickSynthEnableButton.getToggleState());
}

void SynthsWindow::ContentComponent::clickSynthTrackSelectorChanged()
{
    int selectedId = clickSynthTrackSelector.getSelectedId();
    if (selectedId == 1)
        clickSynthSelectedTrack.store(-1); // All tracks
    else
        clickSynthSelectedTrack.store(selectedId - 2); // Track index (0-based)
}

void SynthsWindow::ContentComponent::frequencySliderChanged()
{
    int trackIdx = clickSynthSelectedTrack.load();
    if (trackIdx >= 0 && trackIdx < looperEngine.getNumTracks())
    {
        looperEngine.getTrackEngine(trackIdx).getClickSynth().setFrequency(static_cast<float>(frequencySlider.getValue()));
    }
    else if (trackIdx == -1)
    {
        for (int i = 0; i < looperEngine.getNumTracks(); ++i)
        {
            looperEngine.getTrackEngine(i).getClickSynth().setFrequency(static_cast<float>(frequencySlider.getValue()));
        }
    }
}

void SynthsWindow::ContentComponent::durationSliderChanged()
{
    int trackIdx = clickSynthSelectedTrack.load();
    if (trackIdx >= 0 && trackIdx < looperEngine.getNumTracks())
    {
        looperEngine.getTrackEngine(trackIdx).getClickSynth().setDuration(static_cast<float>(durationSlider.getValue()) / 1000.0f);
    }
    else if (trackIdx == -1)
    {
        for (int i = 0; i < looperEngine.getNumTracks(); ++i)
        {
            looperEngine.getTrackEngine(i).getClickSynth().setDuration(static_cast<float>(durationSlider.getValue()) / 1000.0f);
        }
    }
}

void SynthsWindow::ContentComponent::amplitudeSliderChanged()
{
    int trackIdx = clickSynthSelectedTrack.load();
    if (trackIdx >= 0 && trackIdx < looperEngine.getNumTracks())
    {
        looperEngine.getTrackEngine(trackIdx).getClickSynth().setAmplitude(static_cast<float>(amplitudeSlider.getValue()));
    }
    else if (trackIdx == -1)
    {
        for (int i = 0; i < looperEngine.getNumTracks(); ++i)
        {
            looperEngine.getTrackEngine(i).getClickSynth().setAmplitude(static_cast<float>(amplitudeSlider.getValue()));
        }
    }
}

void SynthsWindow::ContentComponent::clickSynthTriggerButtonClicked()
{
    if (!clickSynthEnabled.load())
        return;
    
    int trackIdx = clickSynthSelectedTrack.load();
    if (trackIdx >= 0 && trackIdx < looperEngine.getNumTracks())
    {
        looperEngine.getTrackEngine(trackIdx).getClickSynth().triggerClick();
    }
    else if (trackIdx == -1)
    {
        for (int i = 0; i < looperEngine.getNumTracks(); ++i)
        {
            looperEngine.getTrackEngine(i).getClickSynth().triggerClick();
        }
    }
}

void SynthsWindow::ContentComponent::samplerEnableButtonChanged()
{
    samplerEnabled.store(samplerEnableButton.getToggleState());
}

void SynthsWindow::ContentComponent::samplerTrackSelectorChanged()
{
    int selectedId = samplerTrackSelector.getSelectedId();
    if (selectedId == 1)
        samplerSelectedTrack.store(-1); // All tracks
    else
        samplerSelectedTrack.store(selectedId - 2); // Track index (0-based)
}

void SynthsWindow::ContentComponent::loadSampleButtonClicked()
{
    int trackIdx = samplerSelectedTrack.load();

    juce::FileChooser chooser("Select audio sample...",
                              juce::File(),
                              "*.wav;*.aif;*.aiff;*.mp3;*.ogg;*.flac");

    if (chooser.browseForFileToOpen())
    {
        juce::File selectedFile = chooser.getResult();
        lastSampleFilePath = selectedFile.getFullPathName();

        if (trackIdx >= 0 && trackIdx < looperEngine.getNumTracks())
        {
            if (looperEngine.getTrackEngine(trackIdx).getSampler().loadSample(selectedFile))
            {
                sampleNameLabel.setText(selectedFile.getFileName(), juce::dontSendNotification);
            }
        }
        else if (trackIdx == -1)
        {
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

void SynthsWindow::ContentComponent::samplerTriggerButtonClicked()
{
    if (!samplerEnabled.load())
        return;

    int trackIdx = samplerSelectedTrack.load();
    if (trackIdx >= 0 && trackIdx < looperEngine.getNumTracks())
    {
        if (looperEngine.getTrackEngine(trackIdx).getSampler().hasSample())
        {
            looperEngine.getTrackEngine(trackIdx).getSampler().trigger();
        }
    }
    else if (trackIdx == -1)
    {
        for (int i = 0; i < looperEngine.getNumTracks(); ++i)
        {
            if (looperEngine.getTrackEngine(i).getSampler().hasSample())
            {
                looperEngine.getTrackEngine(i).getSampler().trigger();
            }
        }
    }
}

int SynthsWindow::ContentComponent::getSelectedTrack() const
{
    return showingClickSynth.load() ? clickSynthSelectedTrack.load()
                                    : samplerSelectedTrack.load();
}

bool SynthsWindow::ContentComponent::isEnabled() const
{
    if (showingClickSynth.load())
        return clickSynthEnabled.load();
    else
        return samplerEnabled.load();
}

juce::var SynthsWindow::ContentComponent::getState() const
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();

    obj->setProperty("showingClickSynth", showingClickSynth.load());
    obj->setProperty("clickEnabled", clickSynthEnabled.load());
    obj->setProperty("samplerEnabled", samplerEnabled.load());
    obj->setProperty("frequency", frequencySlider.getValue());
    obj->setProperty("durationMs", durationSlider.getValue());
    obj->setProperty("amplitude", amplitudeSlider.getValue());
    obj->setProperty("clickTrackId", clickSynthTrackSelector.getSelectedId());
    obj->setProperty("samplerTrackId", samplerTrackSelector.getSelectedId());
    obj->setProperty("sampleName", sampleNameLabel.getText());
    obj->setProperty("samplePath", lastSampleFilePath);

    return juce::var(obj);
}

void SynthsWindow::ContentComponent::applyState(const juce::var& state)
{
    if (!state.isObject())
        return;

    auto* obj = state.getDynamicObject();
    if (obj == nullptr)
        return;

    auto clickTrackIdVar = obj->getProperty("clickTrackId");
    if (clickTrackIdVar.isInt())
        clickSynthTrackSelector.setSelectedId(static_cast<int>(clickTrackIdVar), juce::sendNotification);

    auto samplerTrackIdVar = obj->getProperty("samplerTrackId");
    if (samplerTrackIdVar.isInt())
        samplerTrackSelector.setSelectedId(static_cast<int>(samplerTrackIdVar), juce::sendNotification);

    auto clickEnabledVar = obj->getProperty("clickEnabled");
    if (clickEnabledVar.isBool())
        clickSynthEnableButton.setToggleState(static_cast<bool>(clickEnabledVar), juce::sendNotification);

    auto samplerEnabledVar = obj->getProperty("samplerEnabled");
    if (samplerEnabledVar.isBool())
        samplerEnableButton.setToggleState(static_cast<bool>(samplerEnabledVar), juce::sendNotification);

    auto freqVar = obj->getProperty("frequency");
    if (freqVar.isDouble() || freqVar.isInt())
        frequencySlider.setValue(static_cast<double>(freqVar), juce::sendNotification);

    auto durVar = obj->getProperty("durationMs");
    if (durVar.isDouble() || durVar.isInt())
        durationSlider.setValue(static_cast<double>(durVar), juce::sendNotification);

    auto ampVar = obj->getProperty("amplitude");
    if (ampVar.isDouble() || ampVar.isInt())
        amplitudeSlider.setValue(static_cast<double>(ampVar), juce::sendNotification);

    auto showingVar = obj->getProperty("showingClickSynth");
    if (showingVar.isBool())
        tabButtonClicked(static_cast<bool>(showingVar) ? 1 : 0);

    auto sampleNameVar = obj->getProperty("sampleName");
    if (sampleNameVar.isString())
        sampleNameLabel.setText(sampleNameVar.toString(), juce::dontSendNotification);

    auto samplePathVar = obj->getProperty("samplePath");
    if (samplePathVar.isString())
    {
        juce::File sampleFile(samplePathVar.toString());
        if (sampleFile.existsAsFile())
        {
            int trackIdx = samplerSelectedTrack.load();
            if (trackIdx >= 0 && trackIdx < looperEngine.getNumTracks())
            {
                if (looperEngine.getTrackEngine(trackIdx).getSampler().loadSample(sampleFile))
                    sampleNameLabel.setText(sampleFile.getFileName(), juce::dontSendNotification);
            }
            else if (trackIdx == -1)
            {
                bool success = false;
                for (int i = 0; i < looperEngine.getNumTracks(); ++i)
                {
                    if (looperEngine.getTrackEngine(i).getSampler().loadSample(sampleFile))
                        success = true;
                }
                if (success)
                    sampleNameLabel.setText(sampleFile.getFileName() + " (all tracks)", juce::dontSendNotification);
            }
            lastSampleFilePath = sampleFile.getFullPathName();
        }
    }
}

// SynthsWindow implementation
SynthsWindow::SynthsWindow(VampNetMultiTrackLooperEngine& engine, int numTracks, Shared::MidiLearnManager* midiManager)
    : juce::DialogWindow("Synths",
                        juce::Colours::darkgrey,
                        true),
      contentComponent(new ContentComponent(engine, numTracks, midiManager))
{
    setContentOwned(contentComponent, true);
    setResizable(true, true);
    setUsingNativeTitleBar(true);
    centreWithSize(400, 320); // Taller to accommodate tabs and both sets of controls
}

SynthsWindow::~SynthsWindow()
{
}

void SynthsWindow::closeButtonPressed()
{
    setVisible(false);
}

int SynthsWindow::getSelectedTrack() const
{
    if (contentComponent != nullptr)
        return contentComponent->getSelectedTrack();
    return 0;
}

bool SynthsWindow::isEnabled() const
{
    if (contentComponent != nullptr)
        return contentComponent->isEnabled();
    return false;
}

juce::var SynthsWindow::getState() const
{
    if (contentComponent != nullptr)
        return contentComponent->getState();
    return juce::var();
}

void SynthsWindow::applyState(const juce::var& state)
{
    if (contentComponent != nullptr)
        contentComponent->applyState(state);
}

