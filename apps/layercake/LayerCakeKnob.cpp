#include "LayerCakeKnob.h"
#include <cmath>

namespace LayerCakeApp
{

namespace
{
constexpr int kLabelHeight = 18;
constexpr int kLabelGap = 4;
constexpr int kValueAreaPadding = 6;
constexpr int kValueLabelInset = 8;
}

LayerCakeKnob::LayerCakeKnob(const Config& config, Shared::MidiLearnManager* midiManager)
    : m_config(config),
      m_midi_manager(midiManager)
{
    m_slider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    m_slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    m_slider.setRange(config.minValue, config.maxValue, config.interval);
    m_slider.setValue(config.defaultValue, juce::dontSendNotification);
    m_slider.setWantsKeyboardFocus(false);
    m_slider.setDoubleClickReturnValue(true, config.defaultValue);
    if (config.suffix.isNotEmpty())
        m_slider.setTextValueSuffix(config.suffix);
    m_slider.setAlpha(0.0f);
    m_slider.setInterceptsMouseClicks(true, true);
    m_slider.addListener(this);
    addAndMakeVisible(m_slider);

    m_label.setText(config.labelText, juce::dontSendNotification);
    m_label.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_label);

    m_value_label.setJustificationType(juce::Justification::centred);
    m_value_label.setFont(juce::Font(juce::FontOptions()
                                     .withName(juce::Font::getDefaultMonospacedFontName())
                                     .withHeight(16.0f)));
    m_value_label.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(m_value_label);

    register_midi_parameter();
    update_value_label();
}

LayerCakeKnob::~LayerCakeKnob()
{
    m_slider.removeListener(this);
    if (m_mouse_listener != nullptr)
        m_slider.removeMouseListener(m_mouse_listener.get());

    if (m_midi_manager != nullptr && m_registered_parameter_id.isNotEmpty())
        m_midi_manager->unregisterParameter(m_registered_parameter_id);
}

void LayerCakeKnob::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    auto& laf = getLookAndFeel();

    // Reserve space for label at the bottom
    auto labelBounds = bounds.removeFromBottom(kLabelHeight);
    juce::ignoreUnused(labelBounds); // label component handles its own paint
    bounds.removeFromBottom(kLabelGap);

    auto knobArea = bounds.toFloat().reduced(static_cast<float>(kValueAreaPadding));
    const float diameter = juce::jmin(knobArea.getWidth(), knobArea.getHeight());
    auto circle = juce::Rectangle<float>(diameter, diameter).withCentre(knobArea.getCentre());

    const auto surface = laf.findColour(juce::Slider::backgroundColourId).darker(0.35f);
    const auto frame = laf.findColour(juce::Slider::rotarySliderOutlineColourId);
    const auto track = laf.findColour(juce::Slider::trackColourId);
    const auto accent = laf.findColour(juce::Slider::thumbColourId);

    g.setColour(surface);
    g.fillEllipse(circle);

    g.setColour(frame);
    g.drawEllipse(circle, 1.4f);

    const float startAngle = juce::MathConstants<float>::pi * 1.2f;
    const float sweepAngle = juce::MathConstants<float>::pi * 1.6f;
    const double range = m_config.maxValue - m_config.minValue;
    const double rawNormalized = (range != 0.0)
        ? (m_slider.getValue() - m_config.minValue) / range
        : 0.0;
    const float normalized = juce::jlimit(0.0f, 1.0f, static_cast<float>(rawNormalized));
    const float angle = startAngle + normalized * sweepAngle;

    juce::Path trackPath;
    const float trackRadius = circle.getWidth() * 0.4f;
    trackPath.addCentredArc(circle.getCentreX(),
                            circle.getCentreY(),
                            trackRadius,
                            trackRadius,
                            0.0f,
                            startAngle,
                            startAngle + sweepAngle,
                            true);
    g.setColour(track.withAlpha(0.3f));
    g.strokePath(trackPath, juce::PathStrokeType(2.0f));

    juce::Path indicatorPath;
    indicatorPath.addCentredArc(circle.getCentreX(),
                                circle.getCentreY(),
                                trackRadius,
                                trackRadius,
                                0.0f,
                                startAngle,
                                angle,
                                true);
    g.setColour(track);
    g.strokePath(indicatorPath, juce::PathStrokeType(2.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    auto centre = circle.getCentre();
    const float pointerLength = circle.getWidth() * 0.38f;
    juce::Point<float> pointer(centre.x  + pointerLength * std::cos(angle - (4*3.1415)/8),
                               centre.y  + pointerLength * std::sin(angle - (4*3.1415)/8));

    // g.setColour(accent);
    // g.drawLine(centre.x, centre.y, pointer.x, pointer.y, 2.4f);
    g.fillEllipse(pointer.x - 3.0f, pointer.y - 3.0f, 6.0f, 6.0f);
}

void LayerCakeKnob::resized()
{
    auto bounds = getLocalBounds();
    auto labelBounds = bounds.removeFromBottom(kLabelHeight);
    labelBounds.removeFromTop(kLabelGap);
    m_label.setBounds(labelBounds);

    auto valueBounds = bounds.reduced(kValueAreaPadding);
    m_slider.setBounds(valueBounds);
    m_value_label.setBounds(valueBounds.reduced(kValueLabelInset));
}

void LayerCakeKnob::register_midi_parameter()
{
    if (m_midi_manager == nullptr || m_config.parameterId.isEmpty())
    {
        DBG("LayerCakeKnob::register_midi_parameter early return (missing midi manager or parameter id)");
        return;
    }

    m_registered_parameter_id = m_config.parameterId;

    const double minValue = m_config.minValue;
    const double maxValue = m_config.maxValue;

    m_midi_manager->registerParameter({
        m_config.parameterId,
        [this, minValue, maxValue](float normalized) {
            const double value = minValue + normalized * (maxValue - minValue);
            m_slider.setValue(value, juce::sendNotificationSync);
        },
        [this, minValue, maxValue]() {
            const double value = m_slider.getValue();
            return static_cast<float>((value - minValue) / (maxValue - minValue));
        },
        m_config.labelText,
        false
    });

    m_learnable = std::make_unique<Shared::MidiLearnable>(*m_midi_manager, m_config.parameterId);
    m_mouse_listener = std::make_unique<Shared::MidiLearnMouseListener>(*m_learnable, &m_slider);
    m_slider.addMouseListener(m_mouse_listener.get(), false);
}

void LayerCakeKnob::update_value_label()
{
    juce::String valueText = m_slider.getTextFromValue(m_slider.getValue()).trim();
    if (valueText.isEmpty())
    {
        valueText = juce::String(m_slider.getValue(), 3).trim();
    }

    m_value_label.setText("(" + valueText + ")", juce::dontSendNotification);
}

void LayerCakeKnob::sliderValueChanged(juce::Slider* slider)
{
    if (slider == nullptr)
    {
        DBG("LayerCakeKnob::sliderValueChanged early return (null slider)");
        return;
    }

    if (slider != &m_slider)
        return;

    update_value_label();
    repaint();
}

} // namespace LayerCakeApp


