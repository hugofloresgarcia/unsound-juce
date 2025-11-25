#include "LfoTriggerButton.h"
#include "LfoDragHelpers.h"

namespace LayerCakeApp
{

LfoTriggerButton::LfoTriggerButton()
{
    addAndMakeVisible(m_button);
}

void LfoTriggerButton::paint(juce::Graphics& g)
{
    if (m_drag_highlight)
    {
        g.setColour(juce::Colours::white.withAlpha(0.3f));
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 4.0f);
    }
    
    // Draw LFO indicator if assigned
    if (m_lfo_index >= 0)
    {
        const int indicatorSize = 6;
        auto indicatorBounds = getLocalBounds().removeFromTop(indicatorSize + 2).removeFromRight(indicatorSize + 2);
        g.setColour(m_lfo_accent);
        g.fillEllipse(indicatorBounds.toFloat().reduced(1.0f));
    }
}

void LfoTriggerButton::resized()
{
    m_button.setBounds(getLocalBounds());
}

void LfoTriggerButton::mouseDown(const juce::MouseEvent& event)
{
    if (event.mods.isRightButtonDown() && m_lfo_index >= 0)
    {
        juce::PopupMenu menu;
        menu.addItem("Remove LFO Trigger", [this]() {
            clear_lfo_assignment();
            if (on_lfo_cleared)
                on_lfo_cleared();
        });
        menu.showMenuAsync(juce::PopupMenu::Options()
            .withTargetScreenArea({event.getScreenX(), event.getScreenY(), 1, 1}));
    }
}

void LfoTriggerButton::mouseEnter(const juce::MouseEvent& event)
{
    if (!m_is_hovered)
    {
        m_is_hovered = true;
        if (m_hover_changed_handler != nullptr)
            m_hover_changed_handler(true);
    }
    juce::Component::mouseEnter(event);
}

void LfoTriggerButton::mouseExit(const juce::MouseEvent& event)
{
    const auto localPos = event.getEventRelativeTo(this).getPosition();
    const bool stillInside = getLocalBounds().contains(localPos);
    if (!stillInside && m_is_hovered)
    {
        m_is_hovered = false;
        if (m_hover_changed_handler != nullptr)
            m_hover_changed_handler(false);
    }
    juce::Component::mouseExit(event);
}

bool LfoTriggerButton::isInterestedInDragSource(const juce::DragAndDropTarget::SourceDetails& details)
{
    int idx; juce::Colour c; juce::String l;
    return LfoDragHelpers::parse_description(details.description, idx, c, l);
}

void LfoTriggerButton::itemDragEnter(const juce::DragAndDropTarget::SourceDetails& details)
{
    juce::ignoreUnused(details);
    m_drag_highlight = true;
    repaint();
}

void LfoTriggerButton::itemDragExit(const juce::DragAndDropTarget::SourceDetails& details)
{
    juce::ignoreUnused(details);
    m_drag_highlight = false;
    repaint();
}

void LfoTriggerButton::itemDropped(const juce::DragAndDropTarget::SourceDetails& details)
{
    m_drag_highlight = false;
    
    int lfoIndex = -1;
    juce::Colour accent;
    juce::String label;
    
    if (LfoDragHelpers::parse_description(details.description, lfoIndex, accent, label))
    {
        set_lfo_assignment(lfoIndex, accent);
        if (on_lfo_assigned)
            on_lfo_assigned(lfoIndex);
    }
    
    repaint();
}

void LfoTriggerButton::set_lfo_assignment(int index, juce::Colour accent)
{
    m_lfo_index = index;
    m_lfo_accent = accent;
    repaint();
}

void LfoTriggerButton::clear_lfo_assignment()
{
    m_lfo_index = -1;
    repaint();
}

void LfoTriggerButton::set_hover_changed_handler(const std::function<void(bool)>& handler)
{
    m_hover_changed_handler = handler;
}

} // namespace LayerCakeApp

