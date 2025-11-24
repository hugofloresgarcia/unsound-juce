#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <optional>

class LfoAssignmentButton : public juce::Component
{
public:
    LfoAssignmentButton();
    ~LfoAssignmentButton() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;

    void setIdleColour(juce::Colour colour);
    void setAssignmentColour(std::optional<juce::Colour> colour);
    void setHasAssignment(bool hasAssignment);

    bool hasAssignment() const noexcept { return m_has_assignment; }

    std::function<void()> onClicked;

private:
    void trigger_click();

    juce::Colour m_idle_colour;
    std::optional<juce::Colour> m_assignment_colour;
    bool m_has_assignment{false};
    bool m_is_pressed{false};
};





