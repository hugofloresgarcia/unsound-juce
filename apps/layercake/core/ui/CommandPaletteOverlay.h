/*
  ==============================================================================

    CommandPaletteOverlay.h
    Created: 25 Nov 2025
    Author:  LayerCake

  ==============================================================================
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../focus/FocusRegistry.h"

namespace layercake {

class CommandPaletteOverlay : public juce::Component,
                              public juce::TextEditor::Listener,
                              public juce::Timer
{
public:
    CommandPaletteOverlay(FocusRegistry& reg, std::function<void()> onDismiss);
    ~CommandPaletteOverlay() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    
    void show();
    void hide();
    
    void textEditorTextChanged(juce::TextEditor& editor) override;
    void textEditorReturnKeyPressed(juce::TextEditor& editor) override;
    void textEditorEscapeKeyPressed(juce::TextEditor& editor) override;
    
    bool keyPressed(const juce::KeyPress& key) override;

private:
    void timerCallback() override;
    void updateSearchResults();
    void drawRadialMenu(juce::Graphics& g);

    FocusRegistry& focusRegistry;
    std::function<void()> onDismiss;
    
    juce::TextEditor searchBox;
    juce::Array<FocusableTarget*> searchResults;
    int selectedIndex = 0;
    
    // Radial menu state
    bool isRadialMode = true; // Starts in radial, typing switches to list?
    // Actually, let's combine them. Radial around the search box?
    // Or just a list for fuzzy search is more practical for "command palette".
    // The plan mentioned "radial menu ... plus center text field".
    
    struct RadialItem {
        juce::String label;
        juce::String id;
        float angle;
    };
    juce::Array<RadialItem> radialItems;
    int hoveredRadialItem = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CommandPaletteOverlay)
};

} // namespace layercake

