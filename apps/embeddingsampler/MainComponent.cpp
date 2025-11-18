#include "MainComponent.h"
#include <flowerjuce/Components/SettingsDialog.h>
#include <flowerjuce/Components/ConfigManager.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <functional>

using namespace EmbeddingSpaceSampler;

MainComponent::MainComponent(int numTracks, const juce::String& pannerType, const juce::String& soundPalettePath)
    : settingsButton("settings"),
      sinksButton("sinks"),
      titleLabel("Title", "embedding space sampler"),
      audioDeviceDebugLabel("AudioDebug", ""),
      midiLearnOverlay(midiLearnManager),
      soundPalettePath(soundPalettePath)
{
    DBG("ENTRY: MainComponent::MainComponent, numTracks=" + juce::String(numTracks));
    DBG("Sound palette path: " + soundPalettePath);
    
    // Apply custom look and feel
    setLookAndFeel(&customLookAndFeel);
    
    // Initialize MIDI learn
    midiLearnManager.setMidiInputEnabled(true);
    
    // Create sampler tracks
    int actualNumTracks = juce::jmin(numTracks, looperEngine.get_num_tracks());
    DBG("actualNumTracks=" + juce::String(actualNumTracks));
    
    for (int i = 0; i < actualNumTracks; ++i)
    {
        DBG("Creating SamplerTrack " + juce::String(i));
        tracks.push_back(std::make_shared<SamplerTrack>(looperEngine, i, &midiLearnManager, pannerType));
        tracks[i]->set_panner_smoothing_time(pannerSmoothingTime);
        tracks[i]->set_cleat_gain_power(cleatGainPower);
        
        // Register track with audio processor
        sampler_audio_processor.register_sampler_track(tracks[i].get());
        
        DBG("Adding SamplerTrack " + juce::String(i) + " to view");
        addAndMakeVisible(tracks[i].get());
    }
    DBG("All tracks created");
    
    // Register audio processor with device manager (after engine's callback)
    looperEngine.get_audio_device_manager().addAudioCallback(&sampler_audio_processor);
    
    // Setup embedding view
    embedding_view.set_sample_trigger_callback([this](int chunk_index, float velocity) {
        trigger_sample_on_track(chunk_index, velocity);
    });
    addAndMakeVisible(embedding_view);
    
    // Load palette if provided
    if (soundPalettePath.isNotEmpty())
    {
        juce::File palette_dir(soundPalettePath);
        if (palette_dir.exists() && palette_dir.isDirectory())
        {
            bool loaded = embedding_view.load_palette(palette_dir);
            if (!loaded)
            {
                DBG("MainComponent: Failed to load palette from: " + palette_dir.getFullPathName());
            }
            else
            {
                DBG("MainComponent: Successfully loaded palette from: " + palette_dir.getFullPathName());
            }
        }
        else
        {
            DBG("MainComponent: Palette directory does not exist: " + palette_dir.getFullPathName());
        }
    }
    
    // Load MIDI mappings
    auto appDataDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                        .getChildFile("TapeLooper");
    auto midiMappingsFile = appDataDir.getChildFile("midi_mappings_embeddingsampler.xml");
    if (midiMappingsFile.existsAsFile())
        midiLearnManager.loadMappings(midiMappingsFile);
    
    // Set size - embedding view takes most space, tracks below
    const int embedding_height = 600;
    const int track_height = 150;
    const int track_width = 200;
    const int track_spacing = 5;
    const int margin = 20;
    const int controls_height = 60;
    
    int window_width = juce::jmax(800, (track_width + track_spacing) * actualNumTracks - track_spacing + margin * 2);
    int window_height = controls_height + embedding_height + track_height + margin * 2;
    
    setSize(window_width, window_height);
    
    // Setup settings button
    settingsButton.onClick = [this] { settingsButtonClicked(); };
    addAndMakeVisible(settingsButton);
    
    // Setup sinks button
    sinksButton.onClick = [this] { sinksButtonClicked(); };
    addAndMakeVisible(sinksButton);
    
    // Create settings dialog
    settingsDialog = std::make_unique<Shared::SettingsDialog>(
        pannerSmoothingTime,
        [this](double smoothingTime) {
            pannerSmoothingTime = smoothingTime;
            for (auto& track : tracks)
            {
                track->set_panner_smoothing_time(smoothingTime);
            }
        },
        juce::String(), // No Gradio URL
        [this](const juce::String&) {},
        &midiLearnManager,
        juce::String(), // No trajectory dir
        [this](const juce::String&) {},
        cleatGainPower,
        [this](float gainPower) {
            setCLEATGainPower(gainPower);
        },
        dbscanEps,
        [this](int eps) {
            dbscanEps = eps;
            DBG("MainComponent: DBScan Eps updated to " + juce::String(eps));
            // Recompute clusters with new settings
            // eps is in range 5-100 (audiostellar style), convert to normalized distance
            // Default eps=15 should map to ~0.05 (5% of normalized space)
            double eps_normalized = static_cast<double>(eps) / 300.0;  // 15->0.05, 5->0.017, 100->0.33
            embedding_view.recompute_clusters(eps_normalized, dbscanMinPts);
        },
        dbscanMinPts,
        [this](int minPts) {
            dbscanMinPts = minPts;
            DBG("MainComponent: DBScan MinPts updated to " + juce::String(minPts));
            // Recompute clusters with new settings
            double eps_normalized = static_cast<double>(dbscanEps) / 300.0;  // 15->0.05, 5->0.017, 100->0.33
            embedding_view.recompute_clusters(eps_normalized, minPts);
        }
    );
    
    // Setup title label
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setFont(juce::Font(juce::FontOptions()
                                  .withName(juce::Font::getDefaultMonospacedFontName())
                                  .withHeight(20.0f)));
    addAndMakeVisible(titleLabel);
    
    // Setup audio device debug label
    audioDeviceDebugLabel.setJustificationType(juce::Justification::topRight);
    audioDeviceDebugLabel.setFont(juce::Font(juce::FontOptions()
                                             .withName(juce::Font::getDefaultMonospacedFontName())
                                             .withHeight(11.0f)));
    audioDeviceDebugLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible(audioDeviceDebugLabel);
    
    // Setup MIDI learn overlay
    addAndMakeVisible(midiLearnOverlay);
    addKeyListener(&midiLearnOverlay);
    
    // Audio processing: MultiTrackLooperEngine handles its own tracks,
    // and SamplerAudioProcessor handles sampler tracks (registered above)
    
    // Start timer to update UI
    startTimer(50);
}

MainComponent::~MainComponent()
{
    stopTimer();
    
    // Unregister audio processor
    looperEngine.get_audio_device_manager().removeAudioCallback(&sampler_audio_processor);
    
    // Unregister tracks from audio processor
    for (auto& track : tracks)
    {
        if (track != nullptr)
        {
            sampler_audio_processor.unregister_sampler_track(track.get());
        }
    }
    
    removeKeyListener(&midiLearnOverlay);
    
    // Close sinks window
    sinksWindow = nullptr;
    sinksComponent = nullptr;
    
    // Save MIDI mappings
    auto appDataDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                        .getChildFile("TapeLooper");
    appDataDir.createDirectory();
    auto midiMappingsFile = appDataDir.getChildFile("midi_mappings_embeddingsampler.xml");
    midiLearnManager.saveMappings(midiMappingsFile);
    
    // Clear LookAndFeel references
    for (auto& track : tracks)
    {
        if (track != nullptr)
        {
            track->clear_look_and_feel();
        }
    }
    
    setLookAndFeel(nullptr);
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
}

void MainComponent::resized()
{
    const int margin = 10;
    const int controls_height = 60;
    const int embedding_height = 600;
    const int track_height = 150;
    const int track_width = 200;
    const int track_spacing = 5;
    
    auto bounds = getLocalBounds().reduced(margin);
    
    // Title at top
    titleLabel.setBounds(bounds.removeFromTop(30));
    bounds.removeFromTop(10);
    
    // Control buttons
    auto control_area = bounds.removeFromTop(30);
    settingsButton.setBounds(control_area.removeFromLeft(120));
    control_area.removeFromLeft(10);
    sinksButton.setBounds(control_area.removeFromLeft(120));
    bounds.removeFromTop(10);
    
    // Embedding view takes most space
    embedding_view.setBounds(bounds.removeFromTop(embedding_height));
    bounds.removeFromTop(10);
    
    // Tracks arranged horizontally below embedding view
    if (!tracks.empty())
    {
        int available_width = bounds.getWidth();
        int total_tracks_width = (track_width + track_spacing) * static_cast<int>(tracks.size()) - track_spacing;
        
        if (total_tracks_width <= available_width)
        {
            // Center tracks
            bounds.removeFromLeft((available_width - total_tracks_width) / 2);
        }
        
        for (size_t i = 0; i < tracks.size(); ++i)
        {
            tracks[i]->setBounds(bounds.removeFromLeft(track_width));
            if (i < tracks.size() - 1)
            {
                bounds.removeFromLeft(track_spacing);
            }
        }
    }
    
    // MIDI learn overlay covers entire window
    midiLearnOverlay.setBounds(getLocalBounds());
    
    // Audio device debug label in top right corner
    auto debug_bounds = getLocalBounds().removeFromTop(60).removeFromRight(300);
    audioDeviceDebugLabel.setBounds(debug_bounds.reduced(10, 5));
}

void MainComponent::timerCallback()
{
    // Repaint tracks
    for (auto& track : tracks)
    {
        track->repaint();
    }
    
    embedding_view.repaint();
    
    // Update audio device debug info
    updateAudioDeviceDebugInfo();
}

// Audio processing is handled by SamplerAudioProcessor, which is registered with the audio device manager

void MainComponent::updateAudioDeviceDebugInfo()
{
    auto* device = looperEngine.get_audio_device_manager().getCurrentAudioDevice();
    if (device != nullptr)
    {
        juce::String info;
        info += "Device: " + device->getName() + "\n";
        info += "Sample Rate: " + juce::String(device->getCurrentSampleRate(), 0) + " Hz\n";
        info += "Buffer Size: " + juce::String(device->getCurrentBufferSizeSamples()) + " samples\n";
        info += "Input Channels: " + juce::String(device->getActiveInputChannels().countNumberOfSetBits()) + "\n";
        info += "Output Channels: " + juce::String(device->getActiveOutputChannels().countNumberOfSetBits());
        audioDeviceDebugLabel.setText(info, juce::dontSendNotification);
    }
    else
    {
        audioDeviceDebugLabel.setText("No audio device", juce::dontSendNotification);
    }
}

void MainComponent::settingsButtonClicked()
{
    showSettings();
}

void MainComponent::showSettings()
{
    if (settingsDialog != nullptr)
    {
        settingsDialog->setVisible(true);
        settingsDialog->toFront(true);
    }
}

void MainComponent::sinksButtonClicked()
{
    if (sinksWindow == nullptr || sinksComponent == nullptr || 
        (sinksWindow != nullptr && !sinksWindow->isVisible()))
    {
        if (sinksWindow != nullptr)
        {
            sinksWindow = nullptr;
            sinksComponent = nullptr;
        }
        
        const auto& channelLevels = looperEngine.getChannelLevels();
        sinksComponent = std::make_unique<flowerjuce::SinksWindow>(channelLevels);
        
        sinksWindow = std::make_unique<SinksDialogWindow>(
            "Sinks",
            juce::Colours::black
        );
        
        sinksWindow->setContentOwned(sinksComponent.release(), true);
        sinksWindow->setResizable(true, true);
        sinksWindow->setSize(800, 600);
    }
    
    sinksWindow->setVisible(true);
    sinksWindow->toFront(true);
}

void MainComponent::setCLEATGainPower(float gainPower)
{
    cleatGainPower = gainPower;
    for (auto& track : tracks)
    {
        track->set_cleat_gain_power(gainPower);
    }
}

void MainComponent::trigger_sample_on_track(int chunk_index, float velocity)
{
    if (tracks.empty())
        return;
    
    // Get audio file from embedding view
    juce::File audio_file = embedding_view.get_audio_file(chunk_index);
    if (!audio_file.existsAsFile())
    {
        DBG("MainComponent: Audio file not found for chunk " + juce::String(chunk_index));
        return;
    }
    
    // Round-robin track selection
    auto& track = tracks[current_track_index];
    current_track_index = (current_track_index + 1) % static_cast<int>(tracks.size());
    
    // Trigger sample
    track->trigger_sample(audio_file, velocity);
    
    DBG("MainComponent: Triggered sample on track " + juce::String(current_track_index) + 
        ", chunk " + juce::String(chunk_index) + ", velocity " + juce::String(velocity));
}
