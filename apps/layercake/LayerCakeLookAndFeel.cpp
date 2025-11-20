#include "LayerCakeLookAndFeel.h"

namespace
{
constexpr float kButtonBorderThickness = 1.6f;
constexpr float kInnerBorderThickness = 0.9f;

juce::Colour getAccentForButton(const juce::Button& button)
{
    return button.findColour(juce::TextButton::buttonOnColourId);
}
} // namespace

LayerCakeLookAndFeel::LayerCakeLookAndFeel()
{
    m_background = juce::Colours::black;
    m_panel = juce::Colour(0xff050d17);
    m_border = juce::Colour(0xff2a3147);
    m_terminal = juce::Colour(0xfff1463a);
    m_scanline = juce::Colour(0x3300b5ff);
    m_accentCyan = juce::Colour(0xff35c0ff);
    m_accentMagenta = juce::Colour(0xfff45bff);

    setColour(juce::ResizableWindow::backgroundColourId, m_background);

    setColour(juce::TextButton::buttonColourId, m_panel);
    setColour(juce::TextButton::textColourOffId, m_terminal);
    setColour(juce::TextButton::textColourOnId, m_terminal);
    setColour(juce::TextButton::buttonOnColourId, m_accentCyan);

    setColour(juce::ToggleButton::textColourId, m_terminal);

    setColour(juce::Label::textColourId, m_terminal);
    setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);

    setColour(juce::Slider::thumbColourId, m_accentMagenta);
    setColour(juce::Slider::trackColourId, m_accentCyan);
    setColour(juce::Slider::rotarySliderFillColourId, m_accentMagenta);
    setColour(juce::Slider::rotarySliderOutlineColourId, m_border);
    setColour(juce::Slider::backgroundColourId, m_panel.darker(0.4f));
    setColour(juce::Slider::textBoxTextColourId, m_terminal);
    setColour(juce::Slider::textBoxBackgroundColourId, m_background);
    setColour(juce::Slider::textBoxOutlineColourId, m_border);

    setColour(juce::ComboBox::backgroundColourId, m_panel);
    setColour(juce::ComboBox::textColourId, m_terminal);
    setColour(juce::ComboBox::outlineColourId, m_border);

    setColour(juce::PopupMenu::backgroundColourId, m_panel);
    setColour(juce::PopupMenu::textColourId, m_terminal);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, m_accentCyan);
    setColour(juce::PopupMenu::highlightedTextColourId, m_background);

    setColour(juce::ProgressBar::foregroundColourId, m_accentCyan);
    setColour(juce::ProgressBar::backgroundColourId, m_panel);
}

juce::Font LayerCakeLookAndFeel::makeMonoFont(float size, juce::Font::FontStyleFlags style)
{
    juce::FontOptions options(juce::Font::getDefaultMonospacedFontName(), size, static_cast<int>(style));
    return juce::Font(options);
}

juce::Font LayerCakeLookAndFeel::getTextButtonFont(juce::TextButton&, int buttonHeight)
{
    const float size = juce::jlimit(10.0f, 20.0f, buttonHeight * 0.65f);
    return makeMonoFont(size, juce::Font::bold);
}

juce::Font LayerCakeLookAndFeel::getLabelFont(juce::Label&)
{
    return makeMonoFont(14.0f);
}

juce::Font LayerCakeLookAndFeel::getComboBoxFont(juce::ComboBox&)
{
    return makeMonoFont(14.0f);
}

juce::Font LayerCakeLookAndFeel::getPopupMenuFont()
{
    return makeMonoFont(13.0f);
}

juce::Typeface::Ptr LayerCakeLookAndFeel::getTypefaceForFont(const juce::Font& font)
{
    juce::Font mono(font);
    mono.setTypefaceName(juce::Font::getDefaultMonospacedFontName());
    return juce::Typeface::createSystemTypefaceFor(mono);
}

void LayerCakeLookAndFeel::drawButtonBackground(juce::Graphics& g,
                                                juce::Button& button,
                                                const juce::Colour& backgroundColour,
                                                bool shouldDrawButtonAsHighlighted,
                                                bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    const float radius = juce::jmin(6.0f, bounds.getHeight() * 0.45f);
    auto accent = getAccentForButton(button);

    auto fillColour = juce::Colours::black.withAlpha(0.9f);
    if (button.getToggleState() || shouldDrawButtonAsDown)
        fillColour = accent.withAlpha(0.25f);
    else if (shouldDrawButtonAsHighlighted)
        fillColour = backgroundColour.withAlpha(0.3f);

    g.setColour(fillColour);
    g.fillRoundedRectangle(bounds, radius);

    g.setColour(m_border);
    g.drawRoundedRectangle(bounds, radius, kButtonBorderThickness);

    g.setColour(accent.withAlpha(0.85f));
    g.drawRoundedRectangle(bounds.reduced(1.5f), juce::jmax(2.0f, radius - 2.0f), kInnerBorderThickness);
}

