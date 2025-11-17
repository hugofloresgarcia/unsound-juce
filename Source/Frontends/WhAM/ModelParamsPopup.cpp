#include "ModelParamsPopup.h"

namespace WhAM
{

namespace
{
constexpr int kPanelMaxWidth = 520;
constexpr int kPanelMaxHeight = 420;
constexpr int kPanelPadding = 20;
} // namespace

ModelParamsPopup::ModelParamsPopup(Shared::MidiLearnManager* midiManager, const juce::String& trackPrefix)
    : parameterKnobs(midiManager, trackPrefix)
{
    setInterceptsMouseClicks(true, true);
    setVisible(false);

    addAndMakeVisible(panel);
    panel.addAndMakeVisible(parameterKnobs);

    titleLabel.setText("Model parameters", juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setFont(juce::FontOptions(16.0f, juce::Font::bold));
    panel.addAndMakeVisible(titleLabel);

    subtitleLabel.setText("These knobs feed VampNet's advanced controls.", juce::dontSendNotification);
    subtitleLabel.setJustificationType(juce::Justification::centredLeft);
    subtitleLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    panel.addAndMakeVisible(subtitleLabel);

    closeButton.setButtonText("close");
    closeButton.onClick = [this]()
    {
        dismiss();
    };
    panel.addAndMakeVisible(closeButton);
}

void ModelParamsPopup::show(const juce::Rectangle<int>& anchorArea)
{
    anchorBounds = anchorArea;
    setVisible(true);
    toFront(false);
    updatePanelBounds();
    resized(); // Ensure layout is updated when shown
}

void ModelParamsPopup::dismiss()
{
    if (!isVisible())
        return;

    setVisible(false);

    if (onDismissed)
        onDismissed();
}

void ModelParamsPopup::paint(juce::Graphics& g)
{
    if (!isVisible())
        return;

    g.setColour(juce::Colours::black.withAlpha(0.45f));
    g.fillRect(getLocalBounds());
}

void ModelParamsPopup::PanelComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    juce::DropShadow shadow(juce::Colours::black.withAlpha(0.5f), 12, {});
    shadow.drawForRectangle(g, getLocalBounds());

    g.setColour(juce::Colours::darkgrey.withBrightness(0.18f));
    g.fillRoundedRectangle(bounds.reduced(6.0f), 12.0f);

    g.setColour(juce::Colours::white.withAlpha(0.15f));
    g.drawRoundedRectangle(bounds.reduced(6.5f), 12.0f, 1.5f);
}

void ModelParamsPopup::resized()
{
    panel.setBounds({});
    updatePanelBounds();

    auto panelBounds = panel.getLocalBounds().reduced(kPanelPadding);
    auto headerArea = panelBounds.removeFromTop(50);
    closeButton.setBounds(headerArea.removeFromRight(80));
    headerArea.removeFromRight(10);
    titleLabel.setBounds(headerArea.removeFromTop(24));
    subtitleLabel.setBounds(headerArea.removeFromTop(20));
    panelBounds.removeFromTop(10);

    parameterKnobs.setBounds(panelBounds);
}

void ModelParamsPopup::mouseUp(const juce::MouseEvent& event)
{
    if (isVisible() && !panel.getBounds().contains(event.getPosition()))
        dismiss();
}

void ModelParamsPopup::updatePanelBounds()
{
    auto bounds = getLocalBounds().reduced(kPanelPadding);
    if (bounds.isEmpty())
    {
        panel.setBounds({});
        return;
    }

    auto desired = juce::Rectangle<int>(
        juce::jmin(bounds.getWidth(), kPanelMaxWidth),
        juce::jmin(bounds.getHeight(), kPanelMaxHeight));

    desired.setCentre(anchorBounds.isEmpty() ? bounds.getCentre() : anchorBounds.getCentre());
    desired = desired.constrainedWithin(bounds);
    panel.setBounds(desired);
}

} // namespace WhAM


