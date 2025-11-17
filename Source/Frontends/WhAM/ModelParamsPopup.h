#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Shared/ParameterKnobs.h"

namespace WhAM
{

class ModelParamsPopup : public juce::DialogWindow
{
public:
    ModelParamsPopup(Shared::MidiLearnManager* midiManager, const juce::String& trackPrefix);
    ~ModelParamsPopup() override;

    Shared::ParameterKnobs& getKnobs();
    const Shared::ParameterKnobs& getKnobs() const;

    void show(const juce::Rectangle<int>& anchorArea);
    void dismiss();
    bool isPopupVisible() const { return isVisible(); }

    std::function<void()> onDismissed;

    void closeButtonPressed() override;

private:
    class ContentComponent;

    ContentComponent* contentComponent = nullptr;

    void positionRelativeTo(const juce::Rectangle<int>& anchorArea);
};

} // namespace WhAM


