#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace LayerCakeApp
{

/**
 * A button wrapper that can accept LFO drops for triggering.
 * When an LFO is assigned, it triggers on positive zero-crossings.
 */
class LfoTriggerButton : public juce::Component,
                         public juce::DragAndDropTarget
{
public:
    LfoTriggerButton();

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseEnter(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;

    juce::TextButton& button() { return m_button; }

    // DragAndDropTarget
    bool isInterestedInDragSource(const juce::DragAndDropTarget::SourceDetails& details) override;
    void itemDragEnter(const juce::DragAndDropTarget::SourceDetails& details) override;
    void itemDragExit(const juce::DragAndDropTarget::SourceDetails& details) override;
    void itemDropped(const juce::DragAndDropTarget::SourceDetails& details) override;

    void set_lfo_assignment(int index, juce::Colour accent);
    void clear_lfo_assignment();
    int get_lfo_assignment() const { return m_lfo_index; }
    bool has_lfo_assignment() const { return m_lfo_index >= 0; }

    void set_hover_changed_handler(const std::function<void(bool)>& handler);

    std::function<void(int)> on_lfo_assigned;
    std::function<void()> on_lfo_cleared;

private:
    juce::TextButton m_button{"trg"};
    int m_lfo_index{-1};
    juce::Colour m_lfo_accent;
    bool m_drag_highlight{false};
    std::function<void(bool)> m_hover_changed_handler;
    bool m_is_hovered{false};
};

} // namespace LayerCakeApp

