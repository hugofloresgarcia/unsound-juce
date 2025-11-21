#include "MainComponent.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <flowerjuce/LayerCakeEngine/PatternClock.h>
#include <flowerjuce/LayerCakeEngine/Metro.h>
#include <cmath>

namespace LayerCakeApp
{

void VerticalMeter::setLevel(double level)
{
    const double clamped = juce::jlimit(0.0, 1.0, level);
    if (std::abs(clamped - m_level_value) >= 0.0005)
    {
        m_level_value = clamped;
        repaint();
    }
}

void VerticalMeter::paint(juce::Graphics& g)
{
    auto meter_bounds = getLocalBounds().toFloat().reduced(2.0f);
    if (!meter_bounds.isEmpty())
    {
        const float corner_radius = juce::jmin(8.0f, meter_bounds.getWidth() * 0.45f);
        const auto background = findColour(juce::ProgressBar::backgroundColourId).withAlpha(0.85f);
        const auto outline = findColour(juce::Slider::trackColourId).withAlpha(0.7f);
        const auto foreground = findColour(juce::ProgressBar::foregroundColourId);

        g.setColour(background);
        g.fillRoundedRectangle(meter_bounds, corner_radius);

        g.setColour(outline);
        g.drawRoundedRectangle(meter_bounds, corner_radius, 1.4f);

        auto fill_bounds = meter_bounds.reduced(3.0f);
        const float fill_height = fill_bounds.getHeight() * static_cast<float>(m_level_value);
        if (fill_height > 0.0f)
        {
            auto active_region = fill_bounds.removeFromBottom(fill_height);
            const float fill_corner = juce::jmin(corner_radius * 0.6f, active_region.getWidth() * 0.45f);
            g.setColour(foreground);
            g.fillRoundedRectangle(active_region, fill_corner);
        }
    }
}

namespace
{
constexpr double kDefaultSampleRate = 48000.0;
constexpr int kDefaultBlockSize = 512;

const juce::Colour kAccentCyan(0xff35c0ff);
const juce::Colour kAccentMagenta(0xfff45bff);
const juce::Colour kAccentAmber(0xfff2b950);
const juce::Colour kAccentRed(0xfff25f5c);
const juce::Colour kAccentIndigo(0xff7d6bff);
// const juce::Colour kTerminalGreen(0xff63ff87);

void configureControlButton(juce::TextButton& button,
                            const juce::String& label,
                            LayerCakeLookAndFeel::ControlButtonType type,
                            bool isToggle)
{
    button.setButtonText(label);
    button.setClickingTogglesState(isToggle);
    LayerCakeLookAndFeel::setControlButtonType(button, type);
    button.setWantsKeyboardFocus(false);
}
}

MainComponent::MainComponent()
    : m_title_label("title", "layercake"),
      m_record_layer_label("recordLayer", ""),
      m_record_status_label("recordStatus", ""),
      m_trigger_button("trg"),
      m_record_button("rec"),
      m_preset_button("pre"),
      m_clock_button("clk"),
      m_pattern_button("pr"),
      m_pattern_status_label("patternStatus", ""),
      m_display(m_engine),
      m_midi_learn_overlay(m_midi_learn_manager)
{
    DBG("LayerCakeApp::MainComponent ctor");
    setOpaque(true);
    setLookAndFeel(&m_custom_look_and_feel);

    addKeyListener(this);

    addAndMakeVisible(m_display);

    m_title_label.setJustificationType(juce::Justification::centred);
    m_title_label.setFont(juce::Font(juce::FontOptions()
                                         .withName(juce::Font::getDefaultMonospacedFontName())
                                         .withHeight(20.0f)));
    addAndMakeVisible(m_title_label);

    m_record_layer_label.setJustificationType(juce::Justification::centredLeft);
    m_record_layer_label.setColour(juce::Label::textColourId, kAccentMagenta);
    addAndMakeVisible(m_record_layer_label);

    m_record_status_label.setJustificationType(juce::Justification::centredLeft);
    m_record_status_label.setColour(juce::Label::textColourId, kAccentMagenta);
    addAndMakeVisible(m_record_status_label);

    auto makeKnob = [this](const LayerCakeKnob::Config& config) {
        auto knob = std::make_unique<LayerCakeKnob>(config, &m_midi_learn_manager);
        addAndMakeVisible(knob.get());
        return knob;
    };

    auto bindManualKnob = [this](LayerCakeKnob* knob) {
        if (knob == nullptr)
            return;
        knob->slider().onValueChange = [this]() {
            sync_manual_state_from_controls();
        };
    };

    m_master_gain_knob = makeKnob({ "master gain", -24.0, 6.0, 0.0, 0.1, " dB", "layercake_master_gain" });
    m_master_gain_knob->slider().onValueChange = [this]() {
        m_engine.set_master_gain_db(static_cast<float>(m_master_gain_knob->slider().getValue()));
    };

    m_loop_start_knob = makeKnob({ "position", 0.0, 1.0, 0.5, 0.001, "", "layercake_position" });
    bindManualKnob(m_loop_start_knob.get());
    m_duration_knob = makeKnob({ "duration", 10.0, 1000.0, 300.0, 1.0, " ms", "layercake_duration" });
    bindManualKnob(m_duration_knob.get());
    m_rate_knob = makeKnob({ "rate", -24.0, 24.0, 0.0, 0.1, " st", "layercake_rate" });
    bindManualKnob(m_rate_knob.get());
    m_env_knob = makeKnob({ "env", 0.0, 1.0, 0.5, 0.01, "", "layercake_env" });
    bindManualKnob(m_env_knob.get());
    m_spread_knob = makeKnob({ "spread", 0.0, 1.0, 0.0, 0.01, "", "layercake_spread" });
    bindManualKnob(m_spread_knob.get());
    m_direction_knob = makeKnob({ "direction", 0.0, 1.0, 0.5, 0.01, "", "layercake_direction" });
    bindManualKnob(m_direction_knob.get());
    m_pan_knob = makeKnob({ "pan", 0.0, 1.0, 0.5, 0.01, "", "layercake_pan" });
    bindManualKnob(m_pan_knob.get());
    m_layer_select_knob = makeKnob({ "layer", 1.0, static_cast<double>(LayerCakeEngine::kNumLayers), 1.0, 1.0, "", "layercake_layer_select" });
    m_layer_select_knob->slider().setDoubleClickReturnValue(true, 1.0);
    m_layer_select_knob->slider().onValueChange = [this]() {
        const int raw = static_cast<int>(std::round(m_layer_select_knob->slider().getValue())) - 1;
        const int clamped = juce::jlimit(0, static_cast<int>(LayerCakeEngine::kNumLayers) - 1, raw);
        if (clamped != m_engine.get_record_layer())
        {
            m_engine.set_record_layer(clamped);
            update_record_labels();
        }
    };

    m_master_meter.setColour(juce::ProgressBar::foregroundColourId,
                             m_custom_look_and_feel.findColour(juce::ProgressBar::foregroundColourId));
    m_master_meter.setColour(juce::ProgressBar::backgroundColourId,
                             m_custom_look_and_feel.findColour(juce::ProgressBar::backgroundColourId));
    addAndMakeVisible(m_master_meter);

    configureControlButton(m_trigger_button,
                           "trg",
                           LayerCakeLookAndFeel::ControlButtonType::Trigger,
                           false);
    m_trigger_button.onClick = [this]() { trigger_manual_grain(); };
    addAndMakeVisible(m_trigger_button);

    configureControlButton(m_record_button,
                           "rec",
                           LayerCakeLookAndFeel::ControlButtonType::Record,
                           true);
    m_record_button.onClick = [this]() { toggle_record_enable(); };
    addAndMakeVisible(m_record_button);

    configureControlButton(m_clock_button,
                           "clk",
                           LayerCakeLookAndFeel::ControlButtonType::Clock,
                           true);
    m_clock_button.setToggleState(false, juce::dontSendNotification);
    m_clock_button.setTooltip("Toggle clocked auto grains");
    m_clock_button.onClick = [this]() { update_auto_grain_settings(); };
    addAndMakeVisible(m_clock_button);

    configureControlButton(m_pattern_button,
                           "pr",
                           LayerCakeLookAndFeel::ControlButtonType::Pattern,
                           false);
    m_pattern_button.setTooltip("Record/play the pattern sequencer");
    m_pattern_button.onClick = [this]() { handle_pattern_button(); };
    addAndMakeVisible(m_pattern_button);

    m_pattern_status_label.setJustificationType(juce::Justification::centredLeft);
    m_pattern_status_label.setColour(juce::Label::textColourId, kAccentMagenta);
    addAndMakeVisible(m_pattern_status_label);

    auto configurePatternKnob = [this](LayerCakeKnob& knob, bool rearm_on_change)
    {
        auto& slider = knob.slider();
        slider.onValueChange = [this, rearm_on_change]()
        {
            if (!m_loading_knob_values)
                apply_pattern_settings(rearm_on_change);
        };
        slider.onDragStart = [this]() { begin_pattern_parameter_edit(); };
        slider.onDragEnd = [this]() { end_pattern_parameter_edit(); };
    };

    m_pattern_length_knob = makeKnob({ "pattern length", 1.0, 128.0, 16.0, 1.0, "", "layercake_pattern_length" });
    configurePatternKnob(*m_pattern_length_knob, true);

    m_pattern_skip_knob = makeKnob({ "rskip", 0.0, 1.0, 0.0, 0.01, "", "layercake_pattern_rskip" });
    configurePatternKnob(*m_pattern_skip_knob, false);

    m_pattern_tempo_knob = makeKnob({ "tempo", 10.0, 600.0, 90.0, 0.1, " bpm", "layercake_pattern_tempo" });
    configurePatternKnob(*m_pattern_tempo_knob, true);

    m_pattern_subdiv_knob = makeKnob({ "subdiv", -3.0, 3.0, 0.0, 1.0, "", "layercake_pattern_subdiv" });
    m_pattern_subdiv_knob->slider().setDoubleClickReturnValue(true, 0.0);
    m_pattern_subdiv_knob->slider().setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    configurePatternKnob(*m_pattern_subdiv_knob, true);

    configureControlButton(m_preset_button,
                           "pre",
                           LayerCakeLookAndFeel::ControlButtonType::Preset,
                           false);
    m_preset_button.onClick = [this]() { open_library_window(); };
    addAndMakeVisible(m_preset_button);

    auto capturePattern = [this]() { return capture_pattern_data(); };
    auto captureLayers = [this]() { return capture_layer_buffers(); };
    auto applyPattern = [this](const LayerCakePresetData& data) { apply_pattern_snapshot(data); };
    auto applyLayers = [this](const LayerBufferArray& buffers) { apply_layer_buffers(buffers); };
    auto captureKnobset = [this]() { return capture_knobset_data(); };
    auto applyKnobset = [this](const LayerCakePresetData& data) { apply_knobset(data); };

    m_preset_panel = std::make_unique<LibraryBrowserComponent>(m_library_manager,
                                                               capturePattern,
                                                               captureLayers,
                                                               applyPattern,
                                                               applyLayers,
                                                               captureKnobset,
                                                               applyKnobset);
    if (m_preset_panel != nullptr)
    {
        m_preset_panel->setLookAndFeel(&m_custom_look_and_feel);
        m_preset_panel->setVisible(m_preset_panel_visible);
        addAndMakeVisible(m_preset_panel.get());
    }
    else
    {
        DBG("MainComponent ctor preset panel initialization failed");
    }

    m_midi_learn_manager.setMidiInputEnabled(true);
    addAndMakeVisible(m_midi_learn_overlay);
    addKeyListener(&m_midi_learn_overlay);

    auto appDataDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                        .getChildFile("LayerCake");
    appDataDir.createDirectory();
    m_midi_mappings_file = appDataDir.getChildFile("midi_mappings_layercake.xml");
    if (m_midi_mappings_file.existsAsFile())
        m_midi_learn_manager.loadMappings(m_midi_mappings_file);

    setSize(1500, 900);
    configure_audio_device();
    startTimerHz(30);
    m_manual_state.loop_start_seconds = 0.0f;
    m_manual_state.duration_ms = 250.0f;
    m_manual_state.rate_semitones = 0.0f;
    m_manual_state.env_attack_ms = 10.0f;
    m_manual_state.env_release_ms = 120.0f;
    m_manual_state.pan = 0.5f;
    m_manual_state.play_forward = true;
    m_manual_state.should_trigger = false;
    sync_manual_state_from_controls();
    m_display.set_record_layer(m_engine.get_record_layer());

    // set grain builder callback
    m_engine.get_pattern_clock()->set_grain_builder(
        [this](){
            return build_manual_grain_state();
        }
    );

}

MainComponent::~MainComponent()
{
    DBG("LayerCakeApp::MainComponent dtor");
    stopTimer();
    if (m_preset_panel != nullptr)
        m_preset_panel->setLookAndFeel(nullptr);
    if (m_midi_mappings_file != juce::File())
    {
        m_midi_mappings_file.getParentDirectory().createDirectory();
        m_midi_learn_manager.saveMappings(m_midi_mappings_file);
    }
    removeKeyListener(&m_midi_learn_overlay);
    removeKeyListener(this);
    m_device_manager.removeAudioCallback(this);
    m_device_manager.closeAudioDevice();
    setLookAndFeel(nullptr);
}

void MainComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    const auto background = m_custom_look_and_feel.findColour(juce::ResizableWindow::backgroundColourId);
    const auto panel = m_custom_look_and_feel.getPanelColour();

    g.setColour(background);
    g.fillRect(bounds);

    g.fillRect(bounds);

    const auto scanlineColour = kAccentMagenta.withAlpha(0.12f);
    g.setColour(scanlineColour);
    for (float y = bounds.getY(); y < bounds.getBottom(); y += 6.0f)
    {
        const float thickness = (static_cast<int>(y / 6.0f) % 2 == 0) ? 0.7f : 0.35f;
        g.drawLine(bounds.getX(), y, bounds.getRight(), y, thickness);
    }

    g.setColour(kAccentIndigo.withAlpha(0.15f));
    for (float x = bounds.getX(); x < bounds.getRight(); x += 18.0f)
        g.drawLine(x, bounds.getY(), x, bounds.getBottom(), 0.3f);

    g.setColour(kAccentCyan.withAlpha(0.35f));
    g.drawRect(bounds, 1.5f);
}

void MainComponent::resized()
{
    const int marginOuter = 20;
    const int sectionSpacing = 18;
    const int rowSpacing = 12;

    const int labelHeight = 28;

    const int knobDiameter = 96;
    const int knobLabelHeight = 18;
    const int knobLabelGap = 4;
    const int knobStackHeight = knobDiameter + knobLabelGap + knobLabelHeight;
    const int knobSpacing = 14;
    const int knobGridColumns = 4;

    const int buttonHeight = 22;
    const int meterWidth = 32;
    const int meterSpacing = 18;
    const int patternTransportHeight = 36;

    const int displayPanelWidth = 560;
    const int displaySize = 500;

    const int presetPanelSpacing = 16;
    const int presetPanelMargin = 10;
    const int presetPanelWidthExpanded = 340;
    const int presetPanelWidthCollapsed = 0;

    auto bounds = getLocalBounds().reduced(marginOuter);

    const int presetPanelWidth = m_preset_panel_visible ? presetPanelWidthExpanded : presetPanelWidthCollapsed;
    if (presetPanelWidth > 0)
    {
        auto presetArea = bounds.removeFromRight(presetPanelWidth).reduced(presetPanelMargin);
        if (m_preset_panel != nullptr)
            m_preset_panel->setBounds(presetArea);
        bounds.removeFromRight(presetPanelSpacing);
    }
    else if (m_preset_panel != nullptr)
    {
        m_preset_panel->setBounds({});
    }

    auto meterArea = bounds.removeFromRight(meterWidth);
    bounds.removeFromRight(meterSpacing);
    m_master_meter.setBounds(meterArea);

    auto displayPanel = bounds.removeFromLeft(displayPanelWidth);
    auto tvArea = displayPanel.withSizeKeepingCentre(displaySize, displaySize);
    m_display.setBounds(tvArea);

    bounds.removeFromLeft(sectionSpacing);
    auto panel = bounds;
    const int knobGridX = panel.getX();

    auto titleArea = panel.removeFromTop(labelHeight);
    m_title_label.setBounds(titleArea);
    panel.removeFromTop(rowSpacing);

    auto recordArea = panel.removeFromTop(labelHeight);
    auto leftRecord = recordArea.removeFromLeft(recordArea.getWidth() / 2);
    m_record_layer_label.setBounds(leftRecord);
    m_record_status_label.setBounds(recordArea);
    panel.removeFromTop(rowSpacing);

    auto masterArea = panel.removeFromTop(knobStackHeight);
    m_master_gain_knob->setBounds(masterArea.withWidth(knobDiameter)
                                           .withHeight(knobStackHeight));
    panel.removeFromTop(sectionSpacing);

    const int manualRows = 2;
    const int manualHeight = manualRows * knobStackHeight + (manualRows - 1) * knobSpacing;
    auto manualArea = panel.removeFromTop(manualHeight);
    auto placeManualKnob = [&](LayerCakeKnob* knob, int row, int col)
    {
        if (knob == nullptr)
            return;
        const int x = manualArea.getX() + col * (knobDiameter + knobSpacing);
        const int y = manualArea.getY() + row * (knobStackHeight + knobSpacing);
        knob->setBounds(x, y, knobDiameter, knobStackHeight);
    };
    placeManualKnob(m_loop_start_knob.get(), 0, 0);
    placeManualKnob(m_duration_knob.get(), 0, 1);
    placeManualKnob(m_rate_knob.get(), 0, 2);
    placeManualKnob(m_layer_select_knob.get(), 0, 3);
    placeManualKnob(m_env_knob.get(), 1, 0);
    placeManualKnob(m_spread_knob.get(), 1, 1);
    placeManualKnob(m_pan_knob.get(), 1, 2);
    placeManualKnob(m_direction_knob.get(), 1, 3);

    panel.removeFromTop(rowSpacing);
    auto gridButtonArea = panel.removeFromTop(buttonHeight);
    auto gridCellBounds = [&](int columnIndex, int columnSpan, int y, int height)
    {
        const int span = juce::jmax(1, columnSpan);
        const int width = knobDiameter * span + knobSpacing * (span - 1);
        const int x = knobGridX + columnIndex * (knobDiameter + knobSpacing);
        return juce::Rectangle<int>(x, y, width, height);
    };
    const int buttonRowY = gridButtonArea.getY();
    m_trigger_button.setBounds(gridCellBounds(0, 1, buttonRowY, buttonHeight));
    m_record_button.setBounds(gridCellBounds(1, 1, buttonRowY, buttonHeight));
    m_preset_button.setBounds(gridCellBounds(2, 1, buttonRowY, buttonHeight));
    panel.removeFromTop(sectionSpacing);

    auto patternArea = panel.removeFromTop(knobStackHeight);
    auto placePatternKnob = [&](LayerCakeKnob& knob, int col)
    {
        juce::Rectangle<int> knobBounds(patternArea.getX() + col * (knobDiameter + knobSpacing),
                                        patternArea.getY(),
                                        knobDiameter,
                                        knobStackHeight);
        knob.setBounds(knobBounds);
        return knobBounds;
    };
    placePatternKnob(*m_pattern_length_knob, 0);
    placePatternKnob(*m_pattern_skip_knob, 1);
    placePatternKnob(*m_pattern_tempo_knob, 2);
    if (m_pattern_subdiv_knob != nullptr)
        placePatternKnob(*m_pattern_subdiv_knob, 3);

    panel.removeFromTop(rowSpacing);
    auto patternTransport = panel.removeFromTop(buttonHeight);
    const int transportRowY = patternTransport.getY();
    m_clock_button.setBounds(gridCellBounds(0, 1, transportRowY, buttonHeight));
    m_pattern_button.setBounds(gridCellBounds(1, 1, transportRowY, buttonHeight));
    auto statusBounds = gridCellBounds(2, 2, transportRowY, buttonHeight);
    m_pattern_status_label.setBounds(statusBounds);
    panel.removeFromTop(sectionSpacing);

    m_midi_learn_overlay.setBounds(getLocalBounds());
}

void MainComponent::configure_audio_device()
{
    juce::String error = m_device_manager.initialise(1, 2, nullptr, true);
    if (error.isNotEmpty())
    {
        DBG("Audio device init error: " + error);
    }
    m_device_manager.addAudioCallback(this);
}

void MainComponent::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    if (device == nullptr)
    {
        DBG("audioDeviceAboutToStart called with null device");
        return;
    }

    const double sample_rate = device->getCurrentSampleRate() > 0 ? device->getCurrentSampleRate() : kDefaultSampleRate;
    const int block_size = device->getCurrentBufferSizeSamples() > 0 ? device->getCurrentBufferSizeSamples() : kDefaultBlockSize;
    const int outputs = device->getActiveOutputChannels().countNumberOfSetBits();

    m_engine.prepare(sample_rate, block_size, juce::jmax(1, outputs));
    apply_pattern_settings(false);
    update_auto_grain_settings();
    m_device_ready = true;
    DBG("Audio device started sampleRate=" + juce::String(sample_rate) + " block=" + juce::String(block_size));
}

void MainComponent::audioDeviceStopped()
{
    DBG("Audio device stopped");
    m_device_ready = false;
}

void MainComponent::audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                                     int numInputChannels,
                                                     float* const* outputChannelData,
                                                     int numOutputChannels,
                                                     int numSamples,
                                                     const juce::AudioIODeviceCallbackContext&)
{
    if (!m_device_ready)
    {
        DBG("audioDeviceIOCallback called before device ready");
        for (int channel = 0; channel < numOutputChannels; ++channel)
        {
            if (outputChannelData[channel] != nullptr)
                juce::FloatVectorOperations::clear(outputChannelData[channel], numSamples);
        }
        return;
    }

    if (outputChannelData == nullptr)
    {
        DBG("audioDeviceIOCallback missing outputs");
        return;
    }

    m_engine.process_block(inputChannelData,
                           numInputChannels,
                           outputChannelData,
                           numOutputChannels,
                           numSamples);

    float peak = 0.0f;
    for (int channel = 0; channel < numOutputChannels; ++channel)
    {
        const float* channel_data = outputChannelData[channel];
        if (channel_data == nullptr)
            continue;
        for (int sample = 0; sample < numSamples; ++sample)
        {
            peak = juce::jmax(peak, std::abs(channel_data[sample]));
        }
    }
    m_meter_value.store(peak);
}

bool MainComponent::keyPressed(const juce::KeyPress& key, juce::Component*)
{
    if (key == juce::KeyPress::upKey)
    {
        adjust_record_layer(-1);
        return true;
    }
    if (key == juce::KeyPress::downKey)
    {
        adjust_record_layer(1);
        return true;
    }
    if (key.getTextCharacter() == 'r' || key.getTextCharacter() == 'R')
    {
        toggle_record_enable();
        return true;
    }
    return false;
}

void MainComponent::timerCallback()
{
    update_record_labels();
    update_meter();
    update_pattern_labels();
    m_display.set_record_layer(m_engine.get_record_layer());
}

void MainComponent::adjust_record_layer(int delta)
{
    int current = m_engine.get_record_layer();
    int next = (current + delta + static_cast<int>(LayerCakeEngine::kNumLayers)) % static_cast<int>(LayerCakeEngine::kNumLayers);
    m_engine.set_record_layer(next);
    update_record_labels();
}

void MainComponent::toggle_record_enable()
{
    bool enabled = !m_engine.is_record_enabled();
    m_engine.set_record_enable(enabled);
    update_record_labels();
}

void MainComponent::trigger_manual_grain()
{
    sync_manual_state_from_controls();
    auto state = build_manual_grain_state();
    m_engine.trigger_grain(state);
}

GrainState MainComponent::build_manual_grain_state()
{
    GrainState state;
    const int layer = m_engine.get_record_layer();
    const double recorded_seconds = get_layer_recorded_seconds(layer);
    const double normalized_start = juce::jlimit(0.0, 1.0, m_loop_start_knob->slider().getValue());
    double loop_start_seconds = normalized_start * recorded_seconds;

    double duration_ms = m_duration_knob->slider().getValue();
    double duration_seconds = duration_ms * 0.001;

    if (recorded_seconds > 0.0)
    {
        const double max_duration_seconds = juce::jmax(0.0, recorded_seconds - loop_start_seconds);
        duration_seconds = juce::jlimit(0.0, max_duration_seconds, duration_seconds);
    }
    duration_ms = duration_seconds * 1000.0;

    double env_value = m_env_knob != nullptr ? m_env_knob->slider().getValue() : 0.5;
    env_value = juce::jlimit(0.0, 1.0, env_value);
    const double attack_ms = duration_ms * (1.0 - env_value);
    const double release_ms = duration_ms * env_value;

    state.loop_start_seconds = static_cast<float>(loop_start_seconds);
    state.duration_ms = static_cast<float>(duration_ms);
    state.rate_semitones = static_cast<float>(m_rate_knob->slider().getValue());
    state.env_attack_ms = static_cast<float>(attack_ms);
    state.env_release_ms = static_cast<float>(release_ms);
    state.play_forward = true;
    state.layer = layer;
    state.pan = static_cast<float>(m_pan_knob->slider().getValue());
    float spread_amount = m_spread_knob != nullptr ? static_cast<float>(m_spread_knob->slider().getValue()) : 0.0f;
    float reverse_probability = m_direction_knob != nullptr ? static_cast<float>(m_direction_knob->slider().getValue()) : 0.0f;

    // TODO: modify loop_start according to spread
    m_engine.apply_spread_randomization(state, spread_amount);
    m_engine.apply_direction_randomization(state, reverse_probability);
    // TODO, modify direction according to reverse prob. 

    state.should_trigger = true;
    return state;
}

void MainComponent::update_record_labels()
{
    const int layer_index = m_engine.get_record_layer();
    m_record_layer_label.setText("record layer: " + juce::String(layer_index + 1), juce::dontSendNotification);

    const juce::String status = m_engine.is_record_enabled() ? "[REC]" : "[standby]";
    m_record_status_label.setText("record status: " + status, juce::dontSendNotification);
    m_record_button.setToggleState(m_engine.is_record_enabled(), juce::dontSendNotification);
    m_display.set_record_layer(layer_index);
    if (m_layer_select_knob != nullptr)
        m_layer_select_knob->slider().setValue(static_cast<double>(layer_index + 1), juce::dontSendNotification);
    sync_manual_state_from_controls();
}

void MainComponent::update_meter()
{
    const double new_value = juce::jlimit(0.0, 1.0, static_cast<double>(m_meter_value.load()));
    m_master_meter.setLevel(new_value);
}

void MainComponent::apply_pattern_settings(bool request_rearm)
{
    auto* clock = m_engine.get_pattern_clock();
    if (clock == nullptr)
    {
        DBG("MainComponent::apply_pattern_settings PatternClock unavailable");
        return;
    }

    clock->set_pattern_length(static_cast<int>(m_pattern_length_knob->slider().getValue()));
    clock->set_skip_probability(static_cast<float>(m_pattern_skip_knob->slider().getValue()));

    double base_bpm = m_pattern_tempo_knob->slider().getValue();
    double subdiv = m_pattern_subdiv_knob != nullptr ? m_pattern_subdiv_knob->slider().getValue() : 0.0;
    double multiplier = std::pow(2.0, subdiv);
    double effective_bpm = juce::jlimit(1.0, 2000.0, base_bpm * multiplier);
    clock->set_bpm(static_cast<float>(effective_bpm));
    if (request_rearm)
        request_pattern_rearm();
    update_pattern_labels();
}

void MainComponent::update_pattern_labels()
{
    auto* clock = m_engine.get_pattern_clock();
    if (clock == nullptr)
    {
        DBG("MainComponent::update_pattern_labels PatternClock unavailable");
        m_pattern_status_label.setText("pattern: unavailable", juce::dontSendNotification);
        m_pattern_button.setEnabled(false);
        m_clock_button.setEnabled(false);
        return;
    }
    m_pattern_button.setEnabled(true);
    m_clock_button.setEnabled(true);

    juce::String status_text = "pattern";
    juce::String button_text = "pat";
    const auto mode = clock->get_mode();
    switch (mode)
    {
        case PatternClock::Mode::Recording:
            status_text += " (recording)";
            break;
        case PatternClock::Mode::Playback:
            status_text += " (playing)";
            button_text = "[pp]";
            break;
        case PatternClock::Mode::Idle:
        default:
            status_text += " (idle)";
            break;
    }

    m_pattern_status_label.setText(status_text, juce::dontSendNotification);
    m_pattern_button.setButtonText(button_text);
}

void MainComponent::handle_pattern_button()
{
    auto* clock = m_engine.get_pattern_clock();
    if (clock == nullptr)
    {
        DBG("MainComponent::handle_pattern_button PatternClock unavailable");
        return;
    }

    // if we're currently idle, switch to recording
    // if we're recording or playback, switch to idle
    if (clock->get_mode() == PatternClock::Mode::Idle) {
        clock->set_mode(PatternClock::Mode::Recording);
    } else {
        clock->set_mode(PatternClock::Mode::Idle);
    }

    update_pattern_labels();
}

void MainComponent::open_library_window()
{
    if (m_preset_panel == nullptr)
    {
        DBG("MainComponent::open_library_window early return (preset panel missing)");
        return;
    }

    m_preset_panel_visible = !m_preset_panel_visible;
    m_preset_panel->setVisible(m_preset_panel_visible);
    DBG("MainComponent::open_library_window toggled preset panel visibility to "
        + juce::String(m_preset_panel_visible ? "visible" : "hidden"));
    resized();
}

LayerCakePresetData MainComponent::capture_knobset_data() const
{
    LayerCakePresetData data;
    data.master_gain_db = m_master_gain_knob != nullptr
                              ? static_cast<float>(m_master_gain_knob->slider().getValue())
                              : 0.0f;
    data.manual_state = m_manual_state;
    data.manual_state.should_trigger = false;
    data.record_layer = m_engine.get_record_layer();
    data.spread_amount = m_spread_knob != nullptr
                             ? static_cast<float>(m_spread_knob->slider().getValue())
                             : 0.0f;
    data.reverse_probability = m_direction_knob != nullptr
                                   ? static_cast<float>(m_direction_knob->slider().getValue())
                                   : 0.0f;

    auto capture = [&](LayerCakeKnob* knob)
    {
        if (knob == nullptr)
            return;
        const auto& parameterId = knob->parameter_id();
        if (parameterId.isEmpty())
            return;
        data.knob_values.set(juce::Identifier(parameterId), knob->slider().getValue());
    };

    capture(m_master_gain_knob.get());
    capture(m_loop_start_knob.get());
    capture(m_duration_knob.get());
    capture(m_rate_knob.get());
    capture(m_env_knob.get());
    capture(m_spread_knob.get());
    capture(m_direction_knob.get());
    capture(m_pan_knob.get());
    capture(m_layer_select_knob.get());
    capture(m_pattern_length_knob.get());
    capture(m_pattern_skip_knob.get());
    capture(m_pattern_tempo_knob.get());
    capture(m_pattern_subdiv_knob.get());

    return data;
}

LayerCakePresetData MainComponent::capture_pattern_data() const
{
    LayerCakePresetData data = capture_knobset_data();
    data.pattern_subdivision = m_pattern_subdiv_knob != nullptr
                                   ? static_cast<float>(m_pattern_subdiv_knob->slider().getValue())
                                   : 0.0f;

    if (auto* clock = m_engine.get_pattern_clock())
    {
        clock->get_snapshot(data.pattern_snapshot);
    }
    else
    {
        data.pattern_snapshot = PatternSnapshot{};
    }
    return data;
}

LayerBufferArray MainComponent::capture_layer_buffers() const
{
    LayerBufferArray buffers{};
    m_engine.capture_all_layer_snapshots(buffers);
    return buffers;
}

void MainComponent::apply_knobset(const LayerCakePresetData& data, bool update_pattern_engine)
{
    bool pattern_knob_touched = false;
    const juce::ScopedValueSetter<bool> knob_guard(m_loading_knob_values, true);
    auto applyValue = [&](LayerCakeKnob* knob, bool is_pattern_knob)
    {
        if (knob == nullptr)
            return;
        const auto& parameterId = knob->parameter_id();
        if (parameterId.isEmpty())
            return;
        const auto identifier = juce::Identifier(parameterId);
        if (identifier.isNull())
            return;
        if (const juce::var* value = data.knob_values.getVarPointer(identifier))
        {
            pattern_knob_touched = pattern_knob_touched || is_pattern_knob;
            knob->slider().setValue(static_cast<double>(*value), juce::sendNotificationSync);
        }
    };

    applyValue(m_master_gain_knob.get(), false);
    applyValue(m_loop_start_knob.get(), false);
    applyValue(m_duration_knob.get(), false);
    applyValue(m_rate_knob.get(), false);
    applyValue(m_env_knob.get(), false);
    applyValue(m_spread_knob.get(), false);
    applyValue(m_direction_knob.get(), false);
    applyValue(m_pan_knob.get(), false);
    applyValue(m_layer_select_knob.get(), false);
    applyValue(m_pattern_length_knob.get(), true);
    applyValue(m_pattern_skip_knob.get(), true);
    applyValue(m_pattern_tempo_knob.get(), true);
    applyValue(m_pattern_subdiv_knob.get(), true);

    if (update_pattern_engine && pattern_knob_touched)
        apply_pattern_settings(true);
}

void MainComponent::apply_pattern_snapshot(const LayerCakePresetData& data)
{
    apply_knobset(data, false);
    {
        const juce::ScopedValueSetter<bool> loading_guard(m_loading_knob_values, true);
        m_pattern_length_knob->slider().setValue(data.pattern_snapshot.pattern_length, juce::sendNotificationSync);
        m_pattern_skip_knob->slider().setValue(data.pattern_snapshot.skip_probability, juce::sendNotificationSync);
        const float bpm = Metro::period_ms_to_bpm(data.pattern_snapshot.period_ms);
        m_pattern_tempo_knob->slider().setValue(bpm, juce::sendNotificationSync);
        if (m_pattern_subdiv_knob != nullptr)
            m_pattern_subdiv_knob->slider().setValue(data.pattern_subdivision, juce::sendNotificationSync);
    }

    if (auto* clock = m_engine.get_pattern_clock())
    {
        clock->apply_snapshot(data.pattern_snapshot);
        // clock->set_enabled(data.pattern_snapshot.enabled);
    }

    apply_pattern_settings(true);
}

void MainComponent::apply_layer_buffers(const LayerBufferArray& buffers)
{
    for (int i = 0; i < static_cast<int>(buffers.size()); ++i)
        m_engine.apply_layer_snapshot(i, buffers[static_cast<size_t>(i)]);
    m_display.repaint();
}

void MainComponent::sync_manual_state_from_controls()
{
    const int layer = m_engine.get_record_layer();
    const double recorded_seconds = get_layer_recorded_seconds(layer);
    m_manual_state.loop_start_seconds = static_cast<float>(juce::jlimit(0.0, recorded_seconds,
        m_loop_start_knob->slider().getValue() * recorded_seconds));
    const double duration_ms = m_duration_knob->slider().getValue();
    m_manual_state.duration_ms = static_cast<float>(duration_ms);
    m_manual_state.rate_semitones = static_cast<float>(m_rate_knob->slider().getValue());
    const double env_value = m_env_knob != nullptr ? juce::jlimit(0.0, 1.0, m_env_knob->slider().getValue())
                                                   : 0.5;
    m_manual_state.env_attack_ms = static_cast<float>(duration_ms * (1.0 - env_value));
    m_manual_state.env_release_ms = static_cast<float>(duration_ms * env_value);
    m_manual_state.play_forward = true;
    m_manual_state.pan = static_cast<float>(m_pan_knob->slider().getValue());
    m_manual_state.layer = layer;
    m_manual_state.should_trigger = false;
    m_display.set_position_indicator(static_cast<float>(m_loop_start_knob->slider().getValue()));
    update_auto_grain_settings();
}

void MainComponent::update_auto_grain_settings()
{
    auto* clock = m_engine.get_pattern_clock();
    if (clock == nullptr)
        return;

    clock->set_enabled(m_clock_button.getToggleState());
    clock->set_auto_fire_enabled(m_clock_button.getToggleState());
    clock->set_auto_fire_state(m_manual_state);
}

void MainComponent::begin_pattern_parameter_edit()
{
    ++m_pattern_edit_depth;
}

void MainComponent::end_pattern_parameter_edit()
{
    if (m_pattern_edit_depth > 0)
        --m_pattern_edit_depth;

    if (m_pattern_edit_depth == 0 && m_pattern_rearm_requested)
        rearm_pattern_clock();
}

void MainComponent::request_pattern_rearm()
{
    m_pattern_rearm_requested = true;
    if (m_pattern_edit_depth == 0)
        rearm_pattern_clock();
}

void MainComponent::rearm_pattern_clock()
{
    auto* clock = m_engine.get_pattern_clock();
    if (clock == nullptr)
    {
        DBG("MainComponent::rearm_pattern_clock PatternClock unavailable");
        m_pattern_rearm_requested = false;
        return;
    }

    if (clock->get_mode() == PatternClock::Mode::Idle)
   {
        DBG("MainComponent::rearm_pattern_clock clock disabled, skipping rearm");
        m_pattern_rearm_requested = false;

    } else {

        DBG("MainComponent::rearm_pattern_clock rearming after parameter edit");
        clock->set_mode(PatternClock::Mode::Recording);
        m_pattern_rearm_requested = false;
        update_pattern_labels();
    }

}

double MainComponent::get_layer_recorded_seconds(int layer_index) const
{
    if (layer_index < 0 || layer_index >= static_cast<int>(LayerCakeEngine::kNumLayers))
        return 0.0;

    const auto& layers = m_engine.get_layers();
    const auto& loop = layers[static_cast<size_t>(layer_index)];
    const size_t recorded_samples = loop.m_recorded_length.load();
    const double sample_rate = m_engine.get_sample_rate();
    if (sample_rate <= 0.0)
        return 0.0;
    return static_cast<double>(recorded_samples) / sample_rate;
}

} // namespace LayerCakeApp


