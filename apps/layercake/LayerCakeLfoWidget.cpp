#include "LayerCakeLfoWidget.h"
#include "LfoDragHelpers.h"
#include <cmath>

namespace LayerCakeApp
{

namespace
{
constexpr int kPreviewSamples = 128;
const juce::Colour kSoftWhite(0xfff4f4f2);

flower::LfoWaveform waveform_from_index(int index)
{
    switch (index)
    {
        case 1: return flower::LfoWaveform::Triangle;
        case 2: return flower::LfoWaveform::Square;
        case 3: return flower::LfoWaveform::Random;
        case 4: return flower::LfoWaveform::SmoothRandom;
        case 0:
        default: return flower::LfoWaveform::Sine;
    }
}

int waveform_to_index(flower::LfoWaveform waveform)
{
    switch (waveform)
    {
        case flower::LfoWaveform::Triangle: return 1;
        case flower::LfoWaveform::Square: return 2;
        case flower::LfoWaveform::Random: return 3;
        case flower::LfoWaveform::SmoothRandom: return 4;
        case flower::LfoWaveform::Sine:
        default: return 0;
    }
}
} // namespace

LayerCakeLfoWidget::LayerCakeLfoWidget(int lfo_index,
                                       flower::LayerCakeLfoUGen& generator,
                                       juce::Colour accent)
    : m_generator(generator),
      m_accent_colour(accent),
      m_lfo_index(lfo_index)
{
    m_drag_label = "LFO " + juce::String(lfo_index + 1);

    m_title_label.setText(m_drag_label, juce::dontSendNotification);
    m_title_label.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(m_title_label);

    m_mode_selector.addItem("sine", 1);
    m_mode_selector.addItem("tri", 2);
    m_mode_selector.addItem("square", 3);
    m_mode_selector.addItem("random", 4);
    m_mode_selector.addItem("smooth", 5);
    m_mode_selector.setSelectedItemIndex(waveform_to_index(generator.get_mode()));
    m_mode_selector.addListener(this);
    addAndMakeVisible(m_mode_selector);

    auto makeKnob = [this](const juce::String& label,
                           double minVal,
                           double maxVal,
                           double defaultVal,
                           double step) -> std::unique_ptr<LayerCakeKnob>
    {
        LayerCakeKnob::Config config;
        config.labelText = label;
        config.minValue = minVal;
        config.maxValue = maxVal;
        config.defaultValue = defaultVal;
        config.interval = step;
        config.suffix = label == "rate" ? " Hz" : "";
        config.enableSweepRecorder = false;
        return std::make_unique<LayerCakeKnob>(config, nullptr);
    };

    m_rate_knob = makeKnob("rate", 0.05, 12.0, generator.get_rate_hz(), 0.0001);
    if (m_rate_knob != nullptr)
    {
        configure_knob(*m_rate_knob, true);
        m_rate_knob->set_knob_colour(m_accent_colour);
        addAndMakeVisible(m_rate_knob.get());
    }

    m_depth_knob = makeKnob("depth", 0.0, 1.0, generator.get_depth(), 0.0001);
    if (m_depth_knob != nullptr)
    {
        configure_knob(*m_depth_knob, false);
        m_depth_knob->set_knob_colour(m_accent_colour.darker(0.2f));
        addAndMakeVisible(m_depth_knob.get());
    }

    m_wave_preview = std::make_unique<WavePreview>(*this);
    addAndMakeVisible(m_wave_preview.get());

    m_last_rate = generator.get_rate_hz();
    m_last_depth = generator.get_depth();
    m_last_mode = static_cast<int>(generator.get_mode());
    refresh_wave_preview();
    startTimerHz(20);
}

void LayerCakeLfoWidget::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    const float corner = juce::jmin(8.0f, bounds.getHeight() * 0.12f);
    g.setColour(m_accent_colour.withAlpha(0.15f));
    g.fillRoundedRectangle(bounds, corner);
    g.setColour(m_accent_colour.withAlpha(0.4f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), corner, 1.2f);
}

void LayerCakeLfoWidget::resized()
{
    const int margin = 6;
    const int headerHeight = 24;
    const int previewHeight = juce::jmax(40, static_cast<int>(getHeight() * 0.3f));
    const int knobSize = 52;
    const int knobStackHeight = knobSize + 26;
    const int knobSpacing = 8;

    auto bounds = getLocalBounds().reduced(margin);

    auto headerArea = bounds.removeFromTop(headerHeight);
    const int selectorWidth = juce::jmax(70, headerArea.getWidth() / 3);
    auto selectorArea = headerArea.removeFromRight(selectorWidth);
    m_mode_selector.setBounds(selectorArea);
    m_title_label.setBounds(headerArea);
    bounds.removeFromTop(6);

    auto previewArea = bounds.removeFromTop(previewHeight);
    if (m_wave_preview != nullptr)
        m_wave_preview->setBounds(previewArea);
    bounds.removeFromTop(6);

    auto knobsArea = bounds.removeFromTop(knobStackHeight);
    auto rateArea = knobsArea.removeFromLeft(knobSize);
    if (m_rate_knob != nullptr)
    {
        auto rateBounds = rateArea.withHeight(knobStackHeight);
        m_rate_knob->setBounds(rateBounds);
    }
    knobsArea.removeFromLeft(knobSpacing);
    if (m_depth_knob != nullptr)
        m_depth_knob->setBounds(knobsArea.removeFromLeft(knobSize).withHeight(knobStackHeight));
}

float LayerCakeLfoWidget::get_depth() const noexcept
{
    return m_depth_knob != nullptr
        ? static_cast<float>(m_depth_knob->slider().getValue())
        : 0.0f;
}

void LayerCakeLfoWidget::refresh_wave_preview()
{
    if (m_wave_preview == nullptr)
    {
        DBG("LayerCakeLfoWidget::refresh_wave_preview early return (preview nullptr)");
        return;
    }

    std::vector<float> samples(static_cast<size_t>(kPreviewSamples), 0.0f);
    flower::LayerCakeLfoUGen preview = m_generator;
    preview.reset_phase();
    preview.sync_time(0.0);

    const double window_seconds = 2.0;
    const double step = window_seconds / static_cast<double>(samples.size());
    const float depth = juce::jlimit(0.0f, 1.0f, preview.get_depth());

    for (size_t i = 0; i < samples.size(); ++i)
        samples[i] = preview.process_delta(step) * depth;

    m_wave_preview->set_points(samples);
}

void LayerCakeLfoWidget::set_drag_label(const juce::String& label)
{
    m_drag_label = label;
    m_title_label.setText(label, juce::dontSendNotification);
}

void LayerCakeLfoWidget::set_on_settings_changed(std::function<void()> callback)
{
    m_settings_changed_callback = std::move(callback);
}

void LayerCakeLfoWidget::sync_controls_from_generator()
{
    const int index = waveform_to_index(m_generator.get_mode());
    m_mode_selector.setSelectedItemIndex(index, juce::dontSendNotification);
    if (m_rate_knob != nullptr)
        m_rate_knob->slider().setValue(m_generator.get_rate_hz(), juce::dontSendNotification);
    if (m_depth_knob != nullptr)
        m_depth_knob->slider().setValue(m_generator.get_depth(), juce::dontSendNotification);
    refresh_wave_preview();
}

void LayerCakeLfoWidget::comboBoxChanged(juce::ComboBox* comboBoxThatHasChanged)
{
    if (comboBoxThatHasChanged != &m_mode_selector)
    {
        DBG("LayerCakeLfoWidget::comboBoxChanged early return (unexpected combo)");
        return;
    }

    update_generator_settings(false);
}

void LayerCakeLfoWidget::update_generator_settings(bool from_knob_change)
{
    juce::ignoreUnused(from_knob_change);
    m_generator.set_mode(waveform_from_index(m_mode_selector.getSelectedItemIndex()));
    if (m_rate_knob != nullptr)
        m_generator.set_rate_hz(static_cast<float>(m_rate_knob->slider().getValue()));
    if (m_depth_knob != nullptr)
        m_generator.set_depth(static_cast<float>(m_depth_knob->slider().getValue()));
    notify_settings_changed();
}

void LayerCakeLfoWidget::notify_settings_changed()
{
    refresh_wave_preview();
    if (m_settings_changed_callback != nullptr)
        m_settings_changed_callback();
}

void LayerCakeLfoWidget::configure_knob(LayerCakeKnob& knob, bool isRateKnob)
{
    knob.slider().setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    knob.slider().onValueChange = [this]() {
        update_generator_settings(true);
    };
    if (isRateKnob)
        knob.set_knob_colour(m_accent_colour);
}

void LayerCakeLfoWidget::timerCallback()
{
    const float rate = m_generator.get_rate_hz();
    const float depth = m_generator.get_depth();
    const int mode = static_cast<int>(m_generator.get_mode());

    const bool changed = (std::abs(rate - m_last_rate) > 0.0005f)
                      || (std::abs(depth - m_last_depth) > 0.0005f)
                      || (mode != m_last_mode);
    if (!changed)
        return;

    m_last_rate = rate;
    m_last_depth = depth;
    m_last_mode = mode;
    refresh_wave_preview();
}

//==============================================================================
LayerCakeLfoWidget::WavePreview::WavePreview(LayerCakeLfoWidget& owner)
    : m_owner(owner)
{
    setWantsKeyboardFocus(false);
}

void LayerCakeLfoWidget::WavePreview::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    const float corner = juce::jmin(6.0f, bounds.getHeight() * 0.2f);
    const auto accent = m_owner.get_accent_colour();
    g.setColour(accent.withAlpha(0.1f));
    g.fillRoundedRectangle(bounds, corner);
    g.setColour(accent.withAlpha(0.35f));
    g.drawRoundedRectangle(bounds, corner, 1.1f);

    if (m_points.empty())
        return;

    juce::Path wave;
    const float midY = bounds.getCentreY();
    const float amplitude = bounds.getHeight() * 0.4f;
    const float stepX = bounds.getWidth() / juce::jmax<size_t>(1, m_points.size() - 1);

    for (size_t i = 0; i < m_points.size(); ++i)
    {
        const float x = bounds.getX() + static_cast<float>(i) * stepX;
        const float value = juce::jlimit(-1.0f, 1.0f, m_points[i]);
        const float y = midY - value * amplitude;
        if (i == 0)
            wave.startNewSubPath(x, y);
        else
            wave.lineTo(x, y);
    }

    g.setColour(accent);
    g.strokePath(wave, juce::PathStrokeType(2.0f));
}

void LayerCakeLfoWidget::WavePreview::resized()
{
    repaint();
}

void LayerCakeLfoWidget::WavePreview::set_points(const std::vector<float>& points)
{
    m_points = points;
    repaint();
}

void LayerCakeLfoWidget::WavePreview::mouseDown(const juce::MouseEvent& event)
{
    begin_drag(event);
}

void LayerCakeLfoWidget::WavePreview::mouseDrag(const juce::MouseEvent& event)
{
    if (!m_is_dragging)
        begin_drag(event);
}

void LayerCakeLfoWidget::WavePreview::mouseUp(const juce::MouseEvent& event)
{
    juce::ignoreUnused(event);
    m_is_dragging = false;
}

void LayerCakeLfoWidget::WavePreview::begin_drag(const juce::MouseEvent& event)
{
    auto* container = juce::DragAndDropContainer::findParentDragContainerFor(this);
    if (container == nullptr)
    {
        DBG("WavePreview::begin_drag early return (missing DragAndDropContainer)");
        return;
    }

    const auto description = LfoDragHelpers::make_description(
        m_owner.m_lfo_index,
        m_owner.get_accent_colour(),
        m_owner.m_drag_label);

    container->startDragging(description, this);
    m_is_dragging = true;
    juce::ignoreUnused(event);
}

} // namespace LayerCakeApp



