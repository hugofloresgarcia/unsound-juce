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

const juce::Identifier LayerCakeLookAndFeel::kControlButtonTypeProperty("layercake.controlButtonType");

LayerCakeLookAndFeel::LayerCakeLookAndFeel()
{
    m_background = juce::Colours::black;
    m_panel = juce::Colour(0xff050d17);
    m_border = juce::Colour(0xff2a3147);
    m_terminal = juce::Colour(0xffffaEa5).brighter();
    m_scanline = juce::Colour(0x3300b5ff);
    m_accentCyan = juce::Colour(0xff35c0ff);
    m_accentMagenta = juce::Colour(0xfff45bff);
    m_controlRed = juce::Colour(0xffff564a);
    m_controlGreen = juce::Colour(0xff3cff9f);
    m_controlYellow = juce::Colour(0xfff8d24b);
    m_controlCyan = juce::Colour(0xff35c0ff);
    m_controlMagenta = juce::Colour(0xfff45bff);
    m_waveform_colour = juce::Colour(0xffefefef);
    m_knob_label_colour = m_terminal.brighter(0.4f);
    m_disabled_button_fill = juce::Colours::black.withAlpha(0.85f);
    m_layer_colours = {
        juce::Colour(0xfff25f5c).darker(0.8),
        juce::Colour(0xff35c0ff).darker(0.8),
        juce::Colour(0xfff2b950).darker(0.8),
        juce::Colour(0xff7d6bff).darker(0.8),
        juce::Colour(0xff5aff8c).darker(0.8),
        juce::Colour(0xfff45bff).darker(0.8)
    };

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
    const auto controlType = getControlButtonType(button);

    if (controlType == ControlButtonType::Unknown)
    {
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
    else
    {
        const bool isEnabled = button.isEnabled();
        const bool isActive = button.getToggleState();
        auto fillColour = control_fill_colour(controlType,
                                              isEnabled,
                                              isActive,
                                              shouldDrawButtonAsHighlighted,
                                              shouldDrawButtonAsDown);
        auto borderColour = control_border_colour(controlType, isEnabled);

        g.setColour(fillColour);
        g.fillRoundedRectangle(bounds, radius);

        g.setColour(borderColour);
        g.drawRoundedRectangle(bounds, radius, kButtonBorderThickness);

        g.setColour(borderColour.withAlpha(juce::jlimit(0.2f, 1.0f, borderColour.getFloatAlpha() + 0.1f)));
        g.drawRoundedRectangle(bounds.reduced(1.5f), juce::jmax(2.0f, radius - 2.0f), kInnerBorderThickness);
    }
}

void LayerCakeLookAndFeel::setControlButtonType(juce::Button& button, ControlButtonType type)
{
    button.getProperties().set(kControlButtonTypeProperty, static_cast<int>(type));
}

LayerCakeLookAndFeel::ControlButtonType LayerCakeLookAndFeel::getControlButtonType(const juce::Button& button)
{
    const auto value = button.getProperties()[kControlButtonTypeProperty];
    if (value.isVoid())
        return ControlButtonType::Unknown;
    return static_cast<ControlButtonType>(static_cast<int>(value));
}

juce::Colour LayerCakeLookAndFeel::getLayerColour(size_t index) const noexcept
{
    if (m_layer_colours.empty())
        return m_terminal;
    return m_layer_colours[index % m_layer_colours.size()];
}

juce::Colour LayerCakeLookAndFeel::getKnobRecorderIdleColour() const noexcept
{
    return m_knob_label_colour.withAlpha(0.35f);
}

juce::Colour LayerCakeLookAndFeel::getKnobRecorderArmedColour() const noexcept
{
    return getControlAccentColour(ControlButtonType::Record).brighter(0.35f);
}

juce::Colour LayerCakeLookAndFeel::getKnobRecorderRecordingColour() const noexcept
{
    return getControlAccentColour(ControlButtonType::Record);
}

juce::Colour LayerCakeLookAndFeel::getKnobRecorderPlayingColour() const noexcept
{
    return getControlAccentColour(ControlButtonType::Clock);
}

juce::Colour LayerCakeLookAndFeel::getControlAccentColour(ControlButtonType type) const noexcept
{
    switch (type)
    {
        case ControlButtonType::Trigger: return m_controlCyan;
        case ControlButtonType::Record:  return m_controlRed;
        case ControlButtonType::Clock:   return m_controlGreen;
        case ControlButtonType::Pattern: return m_controlYellow;
        case ControlButtonType::Preset:  return m_controlMagenta;
        case ControlButtonType::Unknown:
        default:                         return m_accentCyan;
    }
}

juce::Colour LayerCakeLookAndFeel::getControlDisabledBorderColour(ControlButtonType type) const noexcept
{
    return control_border_colour(type, false);
}

juce::Colour LayerCakeLookAndFeel::control_fill_colour(ControlButtonType type,
                                                       bool isEnabled,
                                                       bool isActive,
                                                       bool isHighlighted,
                                                       bool isDown) const noexcept
{
    if (!isEnabled)
        return m_disabled_button_fill;

    auto accent = getControlAccentColour(type);
    if (isActive || isDown)
        return accent.withAlpha(0.35f);
    if (isHighlighted)
        return accent.withAlpha(0.22f);
    return accent.withAlpha(0.14f);
}

juce::Colour LayerCakeLookAndFeel::control_border_colour(ControlButtonType type, bool isEnabled) const noexcept
{
    auto accent = getControlAccentColour(type);
    const float alpha = isEnabled ? 0.95f : 0.4f;
    return accent.withAlpha(alpha);
}

