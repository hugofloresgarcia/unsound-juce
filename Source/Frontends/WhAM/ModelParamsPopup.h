#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Shared/ParameterKnobs.h"

namespace WhAM
{

class ModelParamsPopup : public juce::Component
{
public:
    ModelParamsPopup(Shared::MidiLearnManager* midiManager, const juce::String& trackPrefix);

    Shared::ParameterKnobs& getKnobs() { return parameterKnobs; }
    const Shared::ParameterKnobs& getKnobs() const { return parameterKnobs; }

    void show(const juce::Rectangle<int>& anchorArea);
    void dismiss();
    bool isPopupVisible() const { return isVisible(); }

    std::function<void()> onDismissed;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseUp(const juce::MouseEvent& event) override;

private:
    class PanelComponent : public juce::Component
    {
    public:
        void paint(juce::Graphics& g) override;
    };

    PanelComponent panel;
    Shared::ParameterKnobs parameterKnobs;
    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::TextButton closeButton;
    juce::Rectangle<int> anchorBounds;

    void updatePanelBounds();
};

} // namespace WhAM


