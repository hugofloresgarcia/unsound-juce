#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

class KnobRecorderButton : public juce::Component
{
public:
    enum class Status
    {
        Idle = 0,
        Armed,
        Recording,
        Playing
    };

    enum ColourIds
    {
        idleColourId = 0x1900001,
        armedColourId,
        recordingColourId,
        playingColourId,
        textColourId,
        borderColourId
    };

    KnobRecorderButton();
    ~KnobRecorderButton() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;

    void setStatus(Status status);
    Status getStatus() const noexcept { return m_status; }

    std::function<void()> onPressed;
    std::function<void()> onReleased;

private:
    void trigger_press();
    void trigger_release();

    Status m_status{Status::Idle};
    bool m_is_pressed{false};
};


