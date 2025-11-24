#include "StartupDialog.h"

LayerCakeStartupDialog::LayerCakeStartupDialog(juce::AudioDeviceManager& deviceManager)
    : m_device_manager(deviceManager),
      m_title_label("startupTitle", "layercake audio routing"),
      m_hint_label("startupHint", "select input/output devices and channels before launching"),
      m_device_selector(deviceManager, 0, 256, 0, 256, true, true, true, false),
      m_ok_button("launch layercake")
{
    m_title_label.setJustificationType(juce::Justification::centred);
    const juce::Font titleFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(),
                                                 24.0f,
                                                 juce::Font::bold));
    m_title_label.setFont(titleFont);
    addAndMakeVisible(m_title_label);

    m_hint_label.setJustificationType(juce::Justification::centred);
    const juce::Font hintFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(),
                                                16.0f,
                                                juce::Font::plain));
    m_hint_label.setFont(hintFont);
    addAndMakeVisible(m_hint_label);

    addAndMakeVisible(m_device_selector);

    m_ok_button.addListener(this);
    addAndMakeVisible(m_ok_button);

    setSize(640, 720);
}

void LayerCakeStartupDialog::resized()
{
    const int kMargin = 20;
    const int kTitleHeight = 36;
    const int kHintHeight = 26;
    const int kSectionSpacing = 16;
    const int kButtonHeight = 46;
    const int kButtonWidth = 220;

    auto bounds = getLocalBounds().reduced(kMargin);

    auto titleArea = bounds.removeFromTop(kTitleHeight);
    m_title_label.setBounds(titleArea);

    bounds.removeFromTop(kSectionSpacing);
    auto hintArea = bounds.removeFromTop(kHintHeight);
    m_hint_label.setBounds(hintArea);

    bounds.removeFromTop(kSectionSpacing);
    auto buttonArea = bounds.removeFromBottom(kButtonHeight);
    m_ok_button.setBounds(buttonArea.removeFromRight(kButtonWidth));

    bounds.removeFromBottom(kSectionSpacing);
    m_device_selector.setBounds(bounds);
}

void LayerCakeStartupDialog::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
}

void LayerCakeStartupDialog::buttonClicked(juce::Button* button)
{
    if (button != &m_ok_button)
    {
        DBG("[LayerCakeStartupDialog] Ignoring click from unexpected button");
        return;
    }

    DBG("[LayerCakeStartupDialog] OK button clicked");

    juce::AudioDeviceManager::AudioDeviceSetup setup;
    m_device_manager.getAudioDeviceSetup(setup);

    auto* device = m_device_manager.getCurrentAudioDevice();
    if (device == nullptr)
    {
        DBG("[LayerCakeStartupDialog] No active device, aborting dialog confirmation");
        return;
    }

    const int numInputChannels = device->getInputChannelNames().size();
    const int numOutputChannels = device->getOutputChannelNames().size();

    setup.inputChannels.clear();
    setup.outputChannels.clear();

    if (numInputChannels > 0)
    {
        for (int i = 0; i < numInputChannels; ++i)
            setup.inputChannels.setBit(i, true);
        setup.useDefaultInputChannels = false;
        DBG("[LayerCakeStartupDialog] Enabled " << numInputChannels << " input channels");
    }
    else
    {
        DBG("[LayerCakeStartupDialog] No input channels available");
    }

    if (numOutputChannels > 0)
    {
        for (int i = 0; i < numOutputChannels; ++i)
            setup.outputChannels.setBit(i, true);
        setup.useDefaultOutputChannels = false;
        DBG("[LayerCakeStartupDialog] Enabled " << numOutputChannels << " output channels");
    }
    else
    {
        DBG("[LayerCakeStartupDialog] No output channels available");
    }

    const auto error = m_device_manager.setAudioDeviceSetup(setup, true);
    if (error.isNotEmpty())
    {
        DBG("[LayerCakeStartupDialog] Failed to apply device setup: " << error);
        return;
    }

    DBG("[LayerCakeStartupDialog] Device setup applied successfully");

    m_ok_clicked = true;
    if (auto* window = findParentComponentOfClass<juce::DialogWindow>())
    {
        window->exitModalState(1);
    }
    else
    {
        DBG("[LayerCakeStartupDialog] No DialogWindow parent; cannot close dialog");
    }
}

juce::AudioDeviceManager::AudioDeviceSetup LayerCakeStartupDialog::getDeviceSetup() const
{
    juce::AudioDeviceManager::AudioDeviceSetup setup;
    m_device_manager.getAudioDeviceSetup(setup);
    return setup;
}
