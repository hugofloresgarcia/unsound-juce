#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class LayerCakeLookAndFeel : public juce::LookAndFeel_V4
{
public:
    LayerCakeLookAndFeel();

    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override;
    juce::Font getLabelFont(juce::Label&) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    juce::Typeface::Ptr getTypefaceForFont(const juce::Font& font) override;

    void drawButtonBackground(juce::Graphics&,
                              juce::Button&,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override;

    juce::Colour getTerminalColour() const noexcept { return m_terminal; }
    juce::Colour getPanelColour() const noexcept { return m_panel; }

private:
    static juce::Font makeMonoFont(float size, juce::Font::FontStyleFlags style = juce::Font::plain);

    juce::Colour m_background;
    juce::Colour m_panel;
    juce::Colour m_border;
    juce::Colour m_terminal;
    juce::Colour m_scanline;
    juce::Colour m_accentCyan;
    juce::Colour m_accentMagenta;
};

