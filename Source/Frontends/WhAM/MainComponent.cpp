#include "MainComponent.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <functional>
#include <array>
// Ensure VampNet types are fully defined (they're forward declared in VampNetTrackEngine.h)
#include "../VampNet/ClickSynth.h"
#include "../VampNet/Sampler.h"
#include "SettingsComponent.h"
#include "BinaryData.h"

using namespace WhAM;

// TODO: Remove this debug macro after fixing segmentation fault
#ifndef DEBUG_SEGFAULT
#define DEBUG_SEGFAULT 1
#endif

#ifdef DBG_SEGFAULT
#undef DBG_SEGFAULT
#endif

#if DEBUG_SEGFAULT
#define DBG_SEGFAULT(msg) juce::Logger::writeToLog("[SEGFAULT] " + juce::String(__FILE__) + ":" + juce::String(__LINE__) + " - " + juce::String(msg))
#else
#define DBG_SEGFAULT(msg)
#endif

namespace
{
    constexpr int kMaxAssetSearchDepth = 6;

    const std::array<const char*, 8> kColorVariantFilenames = {
        "WHAM-black.png",
        "WHAM-ColorVariant-1.png",
        "WHAM-ColorVariant-2.png",
        "WHAM-ColorVariant-3.png",
        "WHAM-ColorVariant-4.png",
        "WHAM-ColorVariant-5.png",
        "WHAM-ColorVariant-6.png",
        "WHAM-ColorVariant-7.png"
    };

    juce::Image loadEmbeddedColorVariant(const juce::String& fileName)
    {
        const juce::String normalisedTarget = juce::File(fileName).getFileName();

        for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
        {
            const juce::String originalName = juce::File(BinaryData::originalFilenames[i]).getFileName();
            if (originalName.equalsIgnoreCase(normalisedTarget))
            {
                int dataSize = 0;
                const char* data = BinaryData::getNamedResource(BinaryData::namedResourceList[i], dataSize);
                if (data != nullptr && dataSize > 0)
                    return juce::ImageFileFormat::loadFrom(data, static_cast<size_t>(dataSize));
            }
        }

        return {};
    }

    juce::Image tryLoadVariantFromDirectoryChain(juce::File baseDir, const juce::String& relativePath)
    {
        for (int depth = 0; depth < kMaxAssetSearchDepth && baseDir.isDirectory(); ++depth)
        {
            auto candidate = baseDir.getChildFile(relativePath);
            if (candidate.existsAsFile())
                return juce::ImageFileFormat::loadFrom(candidate);

            baseDir = baseDir.getParentDirectory();
        }

        return {};
    }

    juce::Image loadColorVariantFromDisk(const juce::String& fileName)
    {
        const juce::String relativePath = "Assets/Color Variants/" + fileName;

        if (auto img = tryLoadVariantFromDirectoryChain(juce::File::getCurrentWorkingDirectory(), relativePath); img.isValid())
            return img;

        auto trySpecialLocation = [&](juce::File::SpecialLocationType type) -> juce::Image
        {
            auto location = juce::File::getSpecialLocation(type);
            auto directory = location.isDirectory() ? location : location.getParentDirectory();
            if (directory.isDirectory())
                return tryLoadVariantFromDirectoryChain(directory, relativePath);
            return {};
        };

        if (auto img = trySpecialLocation(juce::File::currentExecutableFile); img.isValid())
            return img;

        if (auto img = trySpecialLocation(juce::File::invokedExecutableFile); img.isValid())
            return img;

        return {};
    }

    juce::Rectangle<int> computeOpaqueBounds(const juce::Image& image, juce::uint8 alphaThreshold = 8)
    {
        if (!image.isValid())
            return {};

        bool found = false;
        juce::Rectangle<int> bounds;
        juce::Image::BitmapData data(image, juce::Image::BitmapData::readOnly);

        for (int y = 0; y < data.height; ++y)
        {
            for (int x = 0; x < data.width; ++x)
            {
                auto alpha = data.getPixelColour(x, y).getAlpha();
                if (alpha > alphaThreshold)
                {
                    juce::Rectangle<int> pixelBounds(x, y, 1, 1);
                    bounds = found ? bounds.getUnion(pixelBounds) : pixelBounds;
                    found = true;
                }
            }
        }

        if (!found)
            return {};

        bounds = bounds.expanded(2);
        bounds = bounds.getIntersection({ 0, 0, image.getWidth(), image.getHeight() });
        return bounds;
    }

    juce::Image trimImageToOpaqueBounds(const juce::Image& source)
    {
        if (!source.isValid())
            return {};

        auto bounds = computeOpaqueBounds(source);
        if (bounds.isEmpty())
            return {};

        if (bounds == source.getBounds())
            return source;

        juce::Image trimmed(source.getFormat(), bounds.getWidth(), bounds.getHeight(), true);
        juce::Graphics g(trimmed);
        g.drawImageAt(source, -bounds.getX(), -bounds.getY());
        return trimmed;
    }
}

MainComponent::MainComponent(int numTracks, const juce::String& pannerType)
    : syncButton("sync all"),
      settingsButton("settings"),
      synthsButton("synths"),
      vizButton("viz"),
      overflowButton("..."),
      midiLearnOverlay(midiLearnManager)
{
    DBG_SEGFAULT("ENTRY: MainComponent::MainComponent, numTracks=" + juce::String(numTracks));
    // Apply custom look and feel
    DBG_SEGFAULT("Setting look and feel");
    setLookAndFeel(&customLookAndFeel);
    
    // Initialize MIDI learn
    DBG_SEGFAULT("Initializing MIDI learn");
    midiLearnManager.setMidiInputEnabled(true);

    // Create looper tracks (limit to available engines, max 4 for now)
    DBG_SEGFAULT("Creating tracks, numTracks=" + juce::String(numTracks));
    int actualNumTracks = juce::jmin(numTracks, looperEngine.getNumTracks());
    DBG_SEGFAULT("actualNumTracks=" + juce::String(actualNumTracks) + " (limited by engine max=" + juce::String(looperEngine.getNumTracks()) + ")");
    std::function<juce::String()> gradioUrlProvider = [this]() { return getGradioUrl(); };
    for (int i = 0; i < actualNumTracks; ++i)
    {
        DBG_SEGFAULT("Creating LooperTrack " + juce::String(i));
        tracks.push_back(std::make_unique<LooperTrack>(looperEngine, i, gradioUrlProvider, &midiLearnManager, pannerType));
        DBG_SEGFAULT("Adding LooperTrack " + juce::String(i) + " to container");
        tracksContainer.addAndMakeVisible(tracks[i].get());
    }
    DBG_SEGFAULT("All tracks created");

    tracksContainer.setInterceptsMouseClicks(false, true);
    trackViewport.setViewedComponent(&tracksContainer, false);
    trackViewport.setScrollBarsShown(false, true);
    addAndMakeVisible(trackViewport);
    
    // Load MIDI mappings AFTER tracks are created (so parameters are registered)
    auto appDataDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                        .getChildFile("TapeLooper");
    auto midiMappingsFile = appDataDir.getChildFile("midi_mappings_wham.xml");
    if (midiMappingsFile.existsAsFile())
        midiLearnManager.loadMappings(midiMappingsFile);
    
    loadDefaultSessionConfig();
    
    // Set size based on number of tracks
    // WhAM has 3 knobs instead of 2, so slightly wider tracks
    DBG_SEGFAULT("Setting size");
    const int fixedTrackWidth = 260;  // Slightly wider for 3 knobs
    const int trackSpacing = 5;
    const int horizontalMargin = 20;
    const int topControlsHeight = 40 + 10 + 40 + 10;
    const int fixedTrackHeight = 720; // Height adjusted for panner (was 650, added 70 for panner)
    const int verticalMargin = 20;
    
    int windowWidth = (fixedTrackWidth * actualNumTracks) + (trackSpacing * (actualNumTracks - 1)) + horizontalMargin;
    int windowHeight = topControlsHeight + fixedTrackHeight + verticalMargin;
    
    setSize(windowWidth, windowHeight);

    whamLogoImage = juce::ImageCache::getFromMemory(BinaryData::wham_png, BinaryData::wham_pngSize);
    if (!whamLogoImage.isValid())
        whamLogoImage = juce::ImageFileFormat::loadFrom(BinaryData::wham_png, static_cast<size_t>(BinaryData::wham_pngSize));

    if (whamLogoImage.isValid())
    {
        trimmedWhamLogoImage = trimImageToOpaqueBounds(whamLogoImage);
        if (!trimmedWhamLogoImage.isValid())
            trimmedWhamLogoImage = whamLogoImage;
    }

    // Load color variant images for animation
    loadColorVariantImages();

    headerLogoButton.setTooltip("about WhAM");
    headerLogoButton.onClick = [this]() { showAboutDialog(); };
    
    // Set initial logo (black variant)
    updateLogoButtonImage(0);  // 0 is black variant
    
    addAndMakeVisible(headerLogoButton);

    gitInfoLabel.setJustificationType(juce::Justification::centredLeft);
    gitInfoLabel.setFont(juce::Font(juce::FontOptions()
                                    .withName(juce::Font::getDefaultMonospacedFontName())
                                    .withHeight(12.0f)));
    gitInfoLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    gitInfoLabel.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(gitInfoLabel);
    gitInfoLabel.setVisible(false);

    gitInfo = Shared::GitInfoProvider::query();
    refreshGitInfoLabel();

    // Setup sync button
    syncButton.onClick = [this] { syncButtonClicked(); };
    addAndMakeVisible(syncButton);

    // Setup settings button (combines Gradio and MIDI)
    settingsButton.onClick = [this] { settingsButtonClicked(); };
    addAndMakeVisible(settingsButton);
    
    // Setup synths button (combines Click Synth and Sampler)
    synthsButton.onClick = [this] { showSynthsWindow(); };
    addAndMakeVisible(synthsButton);
    
    // Setup viz button
    vizButton.onClick = [this] { showVizWindow(); };
    addAndMakeVisible(vizButton);
    
    saveConfigButton.setButtonText("save");
    saveConfigButton.onClick = [this]() { saveConfigButtonClicked(); };
    addAndMakeVisible(saveConfigButton);

    loadConfigButton.setButtonText("load");
    loadConfigButton.onClick = [this]() { loadConfigButtonClicked(); };
    addAndMakeVisible(loadConfigButton);
    
    // Setup overflow button
    overflowButton.onClick = [this] { showOverflowMenu(); };
    addAndMakeVisible(overflowButton);
    
    // Setup MIDI learn overlay (covers entire window when active)
    addAndMakeVisible(midiLearnOverlay);
    addKeyListener(&midiLearnOverlay);
    
    // Setup keyboard listener for click synth
    addKeyListener(this); // Listen for 'k' key

    // Start timer to update UI
    startTimer(50); // Update every 50ms
    layoutTracks();
}

MainComponent::~MainComponent()
{
    stopTimer();
    
    removeKeyListener(&midiLearnOverlay);
    removeKeyListener(this);
    
    // Save MIDI mappings
    auto appDataDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                        .getChildFile("TapeLooper");
    appDataDir.createDirectory();
    auto midiMappingsFile = appDataDir.getChildFile("midi_mappings_wham.xml");
    midiLearnManager.saveMappings(midiMappingsFile);
    
    setLookAndFeel(nullptr);
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
}

void MainComponent::resized()
{
    auto bounds = getLocalBounds().withTrimmedTop(2).reduced(10, 0);

    const int headerHeight = 130;
    auto headerArea = bounds.removeFromTop(headerHeight);

    const int logoAreaWidth = juce::jmin(280, headerArea.getWidth());
    auto logoArea = headerArea.removeFromRight(logoAreaWidth).reduced(6);
    int logoSize = juce::jmin(logoArea.getWidth(), headerHeight - 6);
    gitInfoLabel.setBounds({});

    bounds.removeFromTop(4);

    // Control buttons with overflow logic
    auto controlArea = bounds.removeFromTop(40);
    const int buttonSpacing = 10;
    const int overflowButtonWidth = 60;

    struct ButtonInfo
    {
        juce::TextButton* button;
        int width;
    };

    std::vector<ButtonInfo> buttons = {
        {&vizButton, 70},
        {&syncButton, 120},
        {&settingsButton, 100},
        {&synthsButton, 100},
        {&saveConfigButton, 90},
        {&loadConfigButton, 90}
    };

    int availableWidth = controlArea.getWidth();
    int usedWidth = 0;
    int visibleButtonCount = 0;

    for (size_t i = 0; i < buttons.size(); ++i)
    {
        int buttonWidth = buttons[i].width;
        int spacing = (visibleButtonCount > 0) ? buttonSpacing : 0;
        int widthNeeded = usedWidth + spacing + buttonWidth;

        if (widthNeeded <= availableWidth)
        {
            usedWidth = widthNeeded;
            visibleButtonCount++;
        }
        else
        {
            break;
        }
    }

    bool hasOverflow = visibleButtonCount < static_cast<int>(buttons.size());
    if (hasOverflow)
    {
        int overflowSpace = buttonSpacing + overflowButtonWidth;
        visibleButtonCount = 0;
        usedWidth = 0;

        for (size_t i = 0; i < buttons.size(); ++i)
        {
            int buttonWidth = buttons[i].width;
            int spacing = (visibleButtonCount > 0) ? buttonSpacing : 0;
            int widthNeeded = usedWidth + spacing + buttonWidth;

            if (widthNeeded + overflowSpace <= availableWidth)
            {
                usedWidth = widthNeeded;
                visibleButtonCount++;
            }
            else
            {
                break;
            }
        }
        hasOverflow = visibleButtonCount < static_cast<int>(buttons.size());
    }

    for (size_t i = 0; i < buttons.size(); ++i)
        buttons[i].button->setVisible(false);

    int xPos = controlArea.getX();
    int yPos = controlArea.getY();
    for (int i = 0; i < visibleButtonCount; ++i)
    {
        if (i > 0)
            xPos += buttonSpacing;

        buttons[i].button->setBounds(xPos, yPos, buttons[i].width, controlArea.getHeight());
        buttons[i].button->setVisible(true);
        xPos += buttons[i].width;
    }

    for (size_t i = visibleButtonCount; i < buttons.size(); ++i)
        buttons[i].button->setVisible(false);

    if (hasOverflow)
    {
        xPos += buttonSpacing;
        overflowButton.setBounds(xPos, yPos, overflowButtonWidth, controlArea.getHeight());
        overflowButton.setVisible(true);
    }
    else
    {
        overflowButton.setVisible(false);
    }

    // Align logo vertically with control buttons row
    int logoX = logoArea.getX() + (logoArea.getWidth() - logoSize) / 2;
    int logoY = controlArea.getY() + (controlArea.getHeight() - logoSize) / 2 - 20;
    juce::Rectangle<int> logoButtonBounds(logoX, logoY, logoSize, logoSize);
    headerLogoButton.setBounds(logoButtonBounds);

    bounds.removeFromTop(10);
    trackViewport.setBounds(bounds);
    layoutTracks();

    midiLearnOverlay.setBounds(getLocalBounds());
}
void MainComponent::timerCallback()
{
    // Repaint tracks to show recording/playing state
    for (auto& track : tracks)
    {
        track->repaint();
    }
    
    // Update audio device debug info
    updateAudioDeviceDebugInfo();
    
    // Update logo animation
    updateLogoAnimation();
}

void MainComponent::syncButtonClicked()
{
    looperEngine.syncAllTracks();
}

void MainComponent::updateAudioDeviceDebugInfo()
{
    auto* device = looperEngine.getAudioDeviceManager().getCurrentAudioDevice();
    if (device != nullptr)
    {
        juce::String deviceName = device->getName();
        int numInputChannels = device->getActiveInputChannels().countNumberOfSetBits();
        int numOutputChannels = device->getActiveOutputChannels().countNumberOfSetBits();
        
        audioDeviceSummary = "IN: " + deviceName + " (" + juce::String(numInputChannels) + " ch)\n"
                             + "OUT: " + deviceName + " (" + juce::String(numOutputChannels) + " ch)";
    }
    else
    {
        audioDeviceSummary = "No audio device";
    }
}

void MainComponent::settingsButtonClicked()
{
    showSettings();
}

void MainComponent::showSettings()
{
    auto mappings = midiLearnManager.getAllMappings();
    auto ioSummary = getAudioDeviceSummary();

    auto* settingsComp = new SettingsComponent(
        getGradioUrl(),
        mappings,
        ioSummary);

    // Give the settings dialog a sensible default size so it doesn't open tiny
    settingsComp->setSize(600, 400);
    settingsComp->setOnUnbindMapping([this](const juce::String& parameterId) {
        midiLearnManager.clearMapping(parameterId);
    });
    settingsComp->setOnUnbindAll([this]() {
        midiLearnManager.clearAllMappings();
    });
    settingsComp->setMappingsProvider([this]() {
        return midiLearnManager.getAllMappings();
    });
    settingsComp->setIoInfoProvider([this]() { return getAudioDeviceSummary(); });

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(settingsComp);
    options.dialogTitle = "settings";
    options.dialogBackgroundColour = juce::Colours::black;
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = false;
    options.resizable = true;
    options.useBottomRightCornerResizer = true;
    options.componentToCentreAround = this;

    auto* dialog = options.launchAsync();
    if (dialog != nullptr)
    {
        settingsComp->setOnApply([this, dialog, settingsComp](const juce::String& url) {
            juce::String newUrl = url.trim();

            if (newUrl.isEmpty())
            {
                juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                                       "invalid url",
                                                       "the gradio url cannot be empty.");
                return;
            }

            juce::URL parsedUrl(newUrl);
            if (!parsedUrl.isWellFormed() || parsedUrl.getScheme().isEmpty())
            {
                juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                                       "invalid url",
                                                       "please enter a valid gradio url, including the protocol (e.g., https://).");
                return;
            }

            if (!newUrl.endsWithChar('/'))
                newUrl += "/";

            setGradioUrl(newUrl);
            settingsComp->refreshIoSummary();
            dialog->closeButtonPressed();
        });

        settingsComp->setOnCancel([dialog]() {
            dialog->closeButtonPressed();
        });
        settingsComp->refreshIoSummary();
    }
}

void MainComponent::setGradioUrl(const juce::String& newUrl)
{
    const juce::ScopedLock lock(gradioSettingsLock);
    gradioUrl = newUrl;
}

juce::String MainComponent::getGradioUrl() const
{
    const juce::ScopedLock lock(gradioSettingsLock);
    return gradioUrl;
}

bool MainComponent::keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent)
{
    // Handle 1-8 keys for track selection
    int keyCode = key.getKeyCode();
    if (keyCode >= '1' && keyCode <= '8')
    {
        int trackNum = keyCode - '1';  // 0-7
        if (trackNum < static_cast<int>(tracks.size()))
        {
            activeTrackIndex = trackNum;
            DBG("Selected track " + juce::String(trackNum + 1));
            // Visual feedback: repaint all tracks to show selection
            for (auto& track : tracks)
                track->repaint();
        }
        return true;
    }
    
    // Handle 'k' key for synths window (click synth or sampler)
    if (key.getKeyCode() == 'k' || key.getKeyCode() == 'K')
    {
        if (synthsWindow != nullptr && synthsWindow->isEnabled())
        {
            int selectedTrack = synthsWindow->getSelectedTrack();
            
            // Check if sampler has samples loaded first, then fall back to click synth
            bool samplerUsed = false;
            if (selectedTrack >= 0 && selectedTrack < static_cast<int>(tracks.size()))
            {
                auto& trackEngine = looperEngine.getTrackEngine(selectedTrack);
                if (trackEngine.getSampler().hasSample())
                {
                    trackEngine.getSampler().trigger();
                    samplerUsed = true;
                    
                    auto& track = looperEngine.getTrack(selectedTrack);
                    if (!track.writeHead.getRecordEnable())
                    {
                        track.writeHead.setRecordEnable(true);
                        tracks[selectedTrack]->repaint();
                    }
                }
                else
                {
                    trackEngine.getClickSynth().triggerClick();
                    auto& track = looperEngine.getTrack(selectedTrack);
                    if (!track.writeHead.getRecordEnable())
                    {
                        track.writeHead.setRecordEnable(true);
                        tracks[selectedTrack]->repaint();
                    }
                }
            }
            else if (selectedTrack == -1)
            {
                // All tracks - try sampler first, then click synth
                for (size_t i = 0; i < tracks.size(); ++i)
                {
                    auto& trackEngine = looperEngine.getTrackEngine(static_cast<int>(i));
                    if (trackEngine.getSampler().hasSample())
                    {
                        trackEngine.getSampler().trigger();
                        samplerUsed = true;
                    }
                    else
                    {
                        trackEngine.getClickSynth().triggerClick();
                    }
                    
                    auto& track = looperEngine.getTrack(static_cast<int>(i));
                    if (!track.writeHead.getRecordEnable())
                    {
                        track.writeHead.setRecordEnable(true);
                        tracks[i]->repaint();
                    }
                }
            }
        }
        return true;
    }
    
    return false;
}

bool MainComponent::keyStateChanged(bool isKeyDown, juce::Component* originatingComponent)
{
    // Handle 'r' key for hold-to-record
    if (juce::KeyPress::isKeyCurrentlyDown('r') || juce::KeyPress::isKeyCurrentlyDown('R'))
    {
        if (!isRecordingHeld)
        {
            // 'r' key just pressed - start recording on active track
            isRecordingHeld = true;
            
            if (activeTrackIndex < static_cast<int>(tracks.size()))
            {
                auto& track = looperEngine.getTrack(activeTrackIndex);
                
                // Enable recording
                track.writeHead.setRecordEnable(true);
                
                // Start playback if not already playing
                if (!track.isPlaying.load())
                {
                    track.isPlaying = true;
                }
                
                DBG("Started recording on track " + juce::String(activeTrackIndex + 1));
                tracks[activeTrackIndex]->repaint();
            }
        }
        return true;
    }
    else if (isRecordingHeld)
    {
        // 'r' key just released - stop recording and trigger generate
        isRecordingHeld = false;
        
        if (activeTrackIndex < static_cast<int>(tracks.size()))
        {
            auto& track = looperEngine.getTrack(activeTrackIndex);
            
            // Disable recording
            track.writeHead.setRecordEnable(false);
            
            DBG("Stopped recording on track " + juce::String(activeTrackIndex + 1) + ", triggering generation");
            
            // Trigger generation using the public method  (broken)
            // juce::MessageManager::callAsync([this]() {
            //     if (activeTrackIndex < static_cast<int>(tracks.size()))
            //     {
            //         tracks[activeTrackIndex]->triggerGeneration();
            //     }
            // });

            
            tracks[activeTrackIndex]->repaint();
        }
        return true;
    }
    
    return false;
}

void MainComponent::showSynthsWindow()
{
    if (synthsWindow == nullptr)
    {
        int numTracks = static_cast<int>(tracks.size());
        synthsWindow = std::make_unique<SynthsWindow>(looperEngine, numTracks, &midiLearnManager);
    }
    
    synthsWindow->setVisible(true);
    synthsWindow->toFront(true);
}

void MainComponent::showVizWindow()
{
    if (vizWindow == nullptr)
    {
        int numTracks = static_cast<int>(tracks.size());
        // Convert unique_ptr vector to pointer vector for TokenVisualizerWindow
        std::vector<WhAM::LooperTrack*> trackPointers;
        trackPointers.reserve(tracks.size());
        for (auto& track : tracks)
        {
            trackPointers.push_back(track.get());
        }
        vizWindow = std::make_unique<TokenVisualizerWindow>(looperEngine, numTracks, trackPointers);
    }
    
    vizWindow->setVisible(true);
    vizWindow->toFront(true);
}


void MainComponent::showOverflowMenu()
{
    DBG("showOverflowMenu called");
    
    // Get the control area bounds to check which buttons should be visible
    auto bounds = getLocalBounds().reduced(10);
    bounds.removeFromTop(40 + 10); // Title + spacing
    auto controlArea = bounds.removeFromTop(40);
    int controlAreaRight = controlArea.getRight();
    
    // Define all buttons with their actions
    struct ButtonMenuItem {
        juce::TextButton* button;
        juce::String label;
        int menuId;
    };
    
    std::vector<ButtonMenuItem> allButtons = {
        {&vizButton, "viz", 1},
        {&syncButton, "sync all", 2},
        {&settingsButton, "settings", 3},
        {&synthsButton, "synths", 4},
        {&saveConfigButton, "save", 5},
        {&loadConfigButton, "load", 6}
    };
    
    // Add buttons to menu if they're outside the visible control area
    // Check bounds instead of isVisible() since visibility might be reset
    int hiddenCount = 0;
    juce::PopupMenu menu;
    DBG("[showOverflowMenu] Checking button bounds vs controlAreaRight=" + juce::String(controlAreaRight));
    
    // Calculate which buttons should be visible based on the same logic as resized()
    const int buttonSpacing = 10;
    const int overflowButtonWidth = 60;
    int availableWidth = controlArea.getWidth();
    
    // Calculate visible button count (same logic as resized())
    int visibleButtonCount = 0;
    int usedWidth = 0;
    std::vector<int> buttonWidths = {70, 120, 100, 100, 130, 130};
    
    // First pass: how many fit without overflow
    for (size_t i = 0; i < buttonWidths.size(); ++i)
    {
        int spacing = (visibleButtonCount > 0) ? buttonSpacing : 0;
        int widthNeeded = usedWidth + spacing + buttonWidths[i];
        if (widthNeeded <= availableWidth)
        {
            usedWidth = widthNeeded;
            visibleButtonCount++;
        }
        else
        {
            break;
        }
    }
    
    // If overflow, recalculate with overflow space
    if (visibleButtonCount < static_cast<int>(buttonWidths.size()))
    {
        int overflowSpace = buttonSpacing + overflowButtonWidth;
        visibleButtonCount = 0;
        usedWidth = 0;
        for (size_t i = 0; i < buttonWidths.size(); ++i)
        {
            int spacing = (visibleButtonCount > 0) ? buttonSpacing : 0;
            int widthNeeded = usedWidth + spacing + buttonWidths[i];
            if (widthNeeded + overflowSpace <= availableWidth)
            {
                usedWidth = widthNeeded;
                visibleButtonCount++;
            }
            else
            {
                break;
            }
        }
    }
    
    DBG("[showOverflowMenu] Calculated visibleButtonCount=" + juce::String(visibleButtonCount));
    
    // Add buttons that are beyond the visible count
    for (size_t i = 0; i < allButtons.size(); ++i)
    {
        if (i >= static_cast<size_t>(visibleButtonCount))
        {
            DBG("Adding to menu (hidden): " + allButtons[i].label);
            menu.addItem(allButtons[i].menuId, allButtons[i].label);
            hiddenCount++;
        }
    }
    
    DBG("Hidden buttons count: " + juce::String(hiddenCount));
    
    if (hiddenCount == 0)
    {
        DBG("No hidden buttons to show in menu - hiding overflow button");
        // If overflow button was clicked but there are no hidden buttons,
        // hide the overflow button (this shouldn't happen, but handle it gracefully)
        overflowButton.setVisible(false);
        return;
    }
    
    // Show menu below the overflow button
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&overflowButton),
                       [this](int result) {
                           DBG("Menu result: " + juce::String(result));
                           if (result == 0)
                               return; // Menu dismissed
                           
                           // Handle menu selection
                           switch (result)
                           {
                               case 1: showVizWindow(); break;
                               case 2: syncButtonClicked(); break;
                               case 3: settingsButtonClicked(); break;
                               case 4: showSynthsWindow(); break;
                               case 5: saveConfigButtonClicked(); break;
                               case 6: loadConfigButtonClicked(); break;
                           }
                       });
}

void MainComponent::saveConfigButtonClicked()
{
    SessionConfig config = buildSessionConfig();
    juce::FileChooser chooser("Save WhAM config", getConfigDirectory(), "*.json");
    if (!chooser.browseForFileToSave(true))
        return;

    auto file = chooser.getResult();
    config.audioDirectory = file.getParentDirectory();
    writeSessionAudioAssets(config, file);
    auto result = config.saveToFile(file);
    if (result.failed())
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                               "save failed",
                                               result.getErrorMessage());
        return;
    }

    juce::AlertWindow prompt("session saved",
                             "Config saved to:\n" + file.getFullPathName(),
                             juce::AlertWindow::NoIcon);
    auto* toggle = new juce::ToggleButton("Set as default");
    prompt.addCustomComponent(toggle);
    prompt.addButton("done", 1, juce::KeyPress(juce::KeyPress::returnKey));
    if (prompt.runModalLoop() == 1 && toggle->getToggleState())
    {
        config.saveToFile(getDefaultConfigFile());
    }
}

void MainComponent::loadConfigButtonClicked()
{
    juce::FileChooser chooser("Load WhAM config", getConfigDirectory(), "*.json");
    if (!chooser.browseForFileToOpen())
        return;

    auto file = chooser.getResult();
    SessionConfig config;
    auto result = SessionConfig::loadFromFile(file, config);
    if (result.failed())
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                               "load failed",
                                               result.getErrorMessage());
        return;
    }
    config.audioDirectory = file.getParentDirectory();

    juce::AlertWindow prompt("load session",
                             "Load this config?\n" + file.getFullPathName(),
                             juce::AlertWindow::NoIcon);
    auto* toggle = new juce::ToggleButton("Set as default");
    prompt.addCustomComponent(toggle);
    prompt.addButton("load", 1, juce::KeyPress(juce::KeyPress::returnKey));
    prompt.addButton("cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    if (prompt.runModalLoop() == 1)
    {
        applySessionConfig(config, true);
        if (toggle->getToggleState())
        {
            config.saveToFile(getDefaultConfigFile());
        }
    }
}

void MainComponent::loadDefaultSessionConfig()
{
    auto defaultFile = getDefaultConfigFile();
    if (!defaultFile.existsAsFile())
        return;

    SessionConfig config;
    auto result = SessionConfig::loadFromFile(defaultFile, config);
    if (result.wasOk())
        applySessionConfig(config, false);
    else
        juce::Logger::writeToLog("Failed to load default WhAM config: " + result.getErrorMessage());
}

SessionConfig MainComponent::buildSessionConfig() const
{
    SessionConfig config;
    config.gradioUrl = getGradioUrl();

    for (const auto& trackPtr : tracks)
    {
        if (!trackPtr)
            continue;

        SessionConfig::TrackState state;
        state.trackIndex = trackPtr->getTrackIndex();
        state.knobState = trackPtr->getKnobState();
        state.vampNetParams = trackPtr->getCustomParams();
        state.autogenEnabled = trackPtr->isAutogenEnabled();
        state.useOutputAsInput = trackPtr->isUseOutputAsInputEnabled();
        state.levelDb = trackPtr->getLevelDb();
        state.pannerState = trackPtr->getPannerState();
        state.inputChannel = trackPtr->getSelectedInputChannel();
        state.outputChannel = trackPtr->getSelectedOutputChannel();
        state.micEnabled = trackPtr->isMicEnabled();
        state.highPassHz = trackPtr->getHighPassCutoffHz();
        state.lowPassHz = trackPtr->getLowPassCutoffHz();
        config.tracks.push_back(state);
    }

    config.midiMappings = midiLearnManager.getAllMappings();

    if (synthsWindow != nullptr)
        config.synthState = synthsWindow->getState();

    return config;
}

void MainComponent::applySessionConfig(const SessionConfig& config, bool showErrorsOnFailure)
{
    if (config.gradioUrl.isNotEmpty())
        setGradioUrl(config.gradioUrl);

    for (const auto& trackState : config.tracks)
    {
        if (trackState.trackIndex < 0 || trackState.trackIndex >= static_cast<int>(tracks.size()))
        {
            if (showErrorsOnFailure)
                juce::Logger::writeToLog("Config refers to missing track index " + juce::String(trackState.trackIndex));
            continue;
        }

        auto* track = tracks[trackState.trackIndex].get();
        if (trackState.knobState.isObject())
            track->applyKnobState(trackState.knobState);

        if (trackState.vampNetParams.isObject())
            track->setCustomParams(trackState.vampNetParams);

        track->setAutogenEnabled(trackState.autogenEnabled);
        track->setUseOutputAsInputEnabled(trackState.useOutputAsInput);
        track->setLevelDb(trackState.levelDb, juce::sendNotificationSync);
        track->applyPannerState(trackState.pannerState);
        track->setSelectedInputChannel(trackState.inputChannel);
        track->setSelectedOutputChannel(trackState.outputChannel);
        track->setMicEnabled(trackState.micEnabled);
        track->setHighPassCutoffHz(static_cast<float>(trackState.highPassHz));
        track->setLowPassCutoffHz(static_cast<float>(trackState.lowPassHz));

        if (config.audioDirectory.exists())
        {
            if (trackState.inputAudioFile.isNotEmpty())
            {
                auto inputFile = config.audioDirectory.getChildFile(trackState.inputAudioFile);
                if (inputFile.existsAsFile())
                {
                    auto result = track->loadInputAudioFromFile(inputFile);
                    if (result.failed() && showErrorsOnFailure)
                        juce::Logger::writeToLog("Failed to load input audio for track "
                                                 + juce::String(trackState.trackIndex + 1) + ": "
                                                 + result.getErrorMessage());
                }
            }

            if (trackState.outputAudioFile.isNotEmpty())
            {
                auto outputFile = config.audioDirectory.getChildFile(trackState.outputAudioFile);
                if (outputFile.existsAsFile())
                {
                    auto result = track->loadOutputAudioFromFile(outputFile);
                    if (result.failed() && showErrorsOnFailure)
                        juce::Logger::writeToLog("Failed to load output audio for track "
                                                 + juce::String(trackState.trackIndex + 1) + ": "
                                                 + result.getErrorMessage());
                }
            }
        }
    }

    if (config.synthState.isObject())
    {
        if (synthsWindow == nullptr)
        {
            auto numTracks = static_cast<int>(tracks.size());
            synthsWindow = std::make_unique<SynthsWindow>(looperEngine, numTracks, &midiLearnManager);
            synthsWindow->setVisible(false);
        }
        synthsWindow->applyState(config.synthState);
    }

    if (!config.midiMappings.empty())
        midiLearnManager.applyMappings(config.midiMappings);
}

void MainComponent::writeSessionAudioAssets(SessionConfig& config, const juce::File& sessionFile)
{
    auto folder = sessionFile.getParentDirectory();
    if (!folder.exists())
        folder.createDirectory();

    config.audioDirectory = folder;
    auto baseName = sessionFile.getFileNameWithoutExtension();

    for (auto& trackState : config.tracks)
    {
        int idx = trackState.trackIndex;
        if (idx < 0 || idx >= static_cast<int>(tracks.size()))
            continue;

        auto* trackComponent = tracks[idx].get();
        if (trackComponent == nullptr)
            continue;

        juce::String suffix = "_track" + juce::String(idx + 1);

        if (trackComponent->hasInputAudio())
        {
            juce::File inputFile = folder.getChildFile(baseName + suffix + "_input.wav");
            auto inputResult = trackComponent->saveInputAudioToFile(inputFile);
            if (inputResult.wasOk())
                trackState.inputAudioFile = inputFile.getFileName();
            else
            {
                trackState.inputAudioFile.clear();
                if (inputResult.getErrorMessage().isNotEmpty())
                    juce::Logger::writeToLog("Failed to save input audio for track "
                                             + juce::String(idx + 1) + ": " + inputResult.getErrorMessage());
            }
        }
        else
        {
            trackState.inputAudioFile.clear();
        }

        if (trackComponent->hasOutputAudio())
        {
            juce::File outputFile = folder.getChildFile(baseName + suffix + "_output.wav");
            auto outputResult = trackComponent->saveOutputAudioToFile(outputFile);
            if (outputResult.wasOk())
                trackState.outputAudioFile = outputFile.getFileName();
            else
            {
                trackState.outputAudioFile.clear();
                if (outputResult.getErrorMessage().isNotEmpty())
                    juce::Logger::writeToLog("Failed to save output audio for track "
                                             + juce::String(idx + 1) + ": " + outputResult.getErrorMessage());
            }
        }
        else
        {
            trackState.outputAudioFile.clear();
        }
    }
}

juce::File MainComponent::getConfigDirectory() const
{
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                   .getChildFile("TapeLooper");
    dir.createDirectory();
    return dir;
}

juce::File MainComponent::getDefaultConfigFile() const
{
    return getConfigDirectory().getChildFile("wham_default_config.json");
}

void MainComponent::refreshGitInfoLabel()
{
    juce::String message;
    if (gitInfo.isValid())
    {
        juce::String shortCommit = gitInfo.commit;
        if (shortCommit.length() > 7)
            shortCommit = shortCommit.substring(0, 7);

        message << gitInfo.branch;
        if (shortCommit.isNotEmpty())
            message << "\n" << shortCommit;

        if (gitInfo.timestamp.isNotEmpty())
            message << "\n" << gitInfo.timestamp;
    }
    else
    {
        message = gitInfo.error.isNotEmpty() ? gitInfo.error : "git info unavailable";
    }

    gitInfoLabel.setText(message, juce::dontSendNotification);
}

void MainComponent::showAboutDialog()
{
    juce::String message("Whale Acoustics Model tape looper interface.\nEarly alpha, CETI internal.");
    message << "\n\nThis software is strictly licensed for CETI or the WhAM team only.";

    if (gitInfo.isValid())
    {
        message << "\n\nCurrent build:\n"
                << "branch: " << gitInfo.branch << "\n"
                << "commit: " << gitInfo.commit;

        if (gitInfo.timestamp.isNotEmpty())
            message << "\nupdated: " << gitInfo.timestamp;
    }
    else if (gitInfo.error.isNotEmpty())
    {
        message << "\n\nGit info unavailable: " << gitInfo.error;
    }

    juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
                                           "WhAM",
                                           message);
}

void MainComponent::layoutTracks()
{
    const int fixedTrackWidth = 260;
    const int trackSpacing = 5;
    const int fixedTrackHeight = 720;

    int numTracks = static_cast<int>(tracks.size());
    int contentWidth = numTracks > 0
        ? (fixedTrackWidth * numTracks) + (trackSpacing * (numTracks - 1))
        : trackViewport.getWidth();
    contentWidth = juce::jmax(trackViewport.getWidth(), contentWidth);

    tracksContainer.setSize(contentWidth, fixedTrackHeight);

    int x = 0;
    for (auto& track : tracks)
    {
        track->setBounds(x, 0, fixedTrackWidth, fixedTrackHeight);
        x += fixedTrackWidth + trackSpacing;
    }
}

bool MainComponent::isAnyTrackGenerating() const
{
    for (const auto& track : tracks)
    {
        if (track != nullptr && track->isGenerating())
            return true;
    }
    return false;
}

void MainComponent::loadColorVariantImages()
{
    colorVariantImages.clear();
    colorVariantImages.reserve(kColorVariantFilenames.size());

    for (const auto* fileName : kColorVariantFilenames)
    {
        juce::Image img = loadEmbeddedColorVariant(fileName);
        if (!img.isValid())
            img = loadColorVariantFromDisk(fileName);

        if (img.isValid())
        {
            auto trimmed = trimImageToOpaqueBounds(img);
            if (trimmed.isValid())
                img = trimmed;
            colorVariantImages.push_back(img);
        }
        else if (trimmedWhamLogoImage.isValid())
        {
            DBG("WhAM: missing color variant '" + juce::String(fileName) + "', falling back to default logo");
            colorVariantImages.push_back(trimmedWhamLogoImage);
        }
    }

    if (colorVariantImages.empty() && trimmedWhamLogoImage.isValid())
        colorVariantImages.push_back(trimmedWhamLogoImage);

    currentAnimationFrame = 0;
    animationFrameCounter = 0;
    finalVariantIndex = -1;
    wasGenerating = false;
}

void MainComponent::updateLogoButtonImage(int variantIndex)
{
    juce::Image imgToUse;
    
    if (variantIndex >= 0 && variantIndex < static_cast<int>(colorVariantImages.size()))
    {
        imgToUse = colorVariantImages[variantIndex];
    }
    else if (trimmedWhamLogoImage.isValid())
    {
        imgToUse = trimmedWhamLogoImage;
    }
    else
    {
        headerLogoButton.setButtonText("WhAM");
        return;
    }
    
    if (imgToUse.isValid())
    {
        headerLogoButton.setImages(false, true, true,
                                   imgToUse, 1.0f, juce::Colours::transparentBlack,
                                   imgToUse, 0.9f, juce::Colours::transparentBlack,
                                   imgToUse, 0.8f, juce::Colours::transparentBlack);
    }
    else
    {
        headerLogoButton.setButtonText("WhAM");
    }
}

void MainComponent::updateLogoAnimation()
{
    bool currentlyGenerating = isAnyTrackGenerating();

    if (colorVariantImages.empty())
    {
        updateLogoButtonImage(0);
        wasGenerating = currentlyGenerating;
        return;
    }

    const int numVariants = static_cast<int>(colorVariantImages.size());

    if (currentlyGenerating)
    {
        if (!wasGenerating)
        {
            currentAnimationFrame = (numVariants > 1) ? 1 : 0;
            animationFrameCounter = 0;
            finalVariantIndex = -1;
        }

        if (numVariants > 1)
        {
            animationFrameCounter++;
            const int framesPerVariant = 2; // ~100ms per frame (timer is 50ms)

            if (animationFrameCounter >= framesPerVariant)
            {
                animationFrameCounter = 0;
                const int variantRange = numVariants - 1;
                const int relativeIndex = (currentAnimationFrame <= 0 ? 0 : currentAnimationFrame - 1);
                currentAnimationFrame = 1 + ((relativeIndex + 1) % variantRange);
            }
        }
        else
        {
            currentAnimationFrame = 0;
        }

        currentAnimationFrame = juce::jlimit(0, numVariants - 1, currentAnimationFrame);
        updateLogoButtonImage(currentAnimationFrame);
    }
    else
    {
        if (wasGenerating)
        {
            if (numVariants > 1)
            {
                auto& random = juce::Random::getSystemRandom();
                finalVariantIndex = 1 + random.nextInt(numVariants - 1);
            }
            else
            {
                finalVariantIndex = 0;
            }
        }

        int logoIndex = (finalVariantIndex >= 0) ? finalVariantIndex : 0;
        logoIndex = juce::jlimit(0, numVariants - 1, logoIndex);
        updateLogoButtonImage(logoIndex);
    }

    wasGenerating = currentlyGenerating;
}

