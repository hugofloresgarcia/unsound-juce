#include "MainComponent.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <flowerjuce/LayerCakeEngine/PatternClock.h>
#include <flowerjuce/LayerCakeEngine/Metro.h>
#include <cmath>

namespace LayerCakeApp
{

namespace
{
constexpr double kDefaultSampleRate = 48000.0;
constexpr int kDefaultBlockSize = 512;

const juce::Colour kAccentCyan(0xff35c0ff);
const juce::Colour kAccentMagenta(0xfff45bff);
const juce::Colour kAccentAmber(0xfff2b950);
const juce::Colour kAccentRed(0xfff25f5c);
const juce::Colour kAccentIndigo(0xff7d6bff);
const juce::Colour kTerminalGreen(0xff63ff87);

void configureControlButton(juce::TextButton& button,
                            const juce::String& label,
                            juce::Colour accent,
                            bool isToggle)
{
    button.setButtonText(label);
    button.setClickingTogglesState(isToggle);
    const auto base = accent.darker(1.4f).withAlpha(0.65f);
    button.setColour(juce::TextButton::buttonColourId, base);
    button.setColour(juce::TextButton::buttonOnColourId, accent);
    button.setColour(juce::TextButton::textColourOffId, kTerminalGreen);
    button.setColour(juce::TextButton::textColourOnId, kTerminalGreen);
    button.setWantsKeyboardFocus(false);
}
}

MainComponent::MainComponent()
    : m_title_label("title", "layercake"),
      m_record_layer_label("recordLayer", ""),
      m_record_status_label("recordStatus", ""),
      m_master_meter(m_meter_display),
      m_trigger_button("[trg]"),
      m_record_button("[rec]"),
      m_preset_button("[pre]"),
      m_clock_button("[clk]"),
      m_pattern_button("[pr]"),
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
    m_record_layer_label.setColour(juce::Label::textColourId, kTerminalGreen);
    addAndMakeVisible(m_record_layer_label);

    m_record_status_label.setJustificationType(juce::Justification::centredLeft);
    m_record_status_label.setColour(juce::Label::textColourId, kTerminalGreen);
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

    m_loop_start_knob = makeKnob({ "position", 0.0, 1.0, 0.0, 0.001, "", "layercake_position" });
    bindManualKnob(m_loop_start_knob.get());
    m_duration_knob = makeKnob({ "duration", 10.0, 1000.0, 250.0, 1.0, " ms", "layercake_duration" });
    bindManualKnob(m_duration_knob.get());
    m_rate_knob = makeKnob({ "rate", -24.0, 24.0, 0.0, 0.1, " st", "layercake_rate" });
    bindManualKnob(m_rate_knob.get());
    m_env_knob = makeKnob({ "env", 0.0, 1.0, 0.5, 0.01, "", "layercake_env" });
    bindManualKnob(m_env_knob.get());
    m_spread_knob = makeKnob({ "spread", 0.0, 1.0, 0.0, 0.01, "", "layercake_spread" });
    bindManualKnob(m_spread_knob.get());
    m_direction_knob = makeKnob({ "direction", 0.0, 1.0, 0.0, 0.01, "", "layercake_direction" });
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

    configureControlButton(m_trigger_button, "[trg]", kAccentCyan, false);
    m_trigger_button.onClick = [this]() { trigger_manual_grain(); };
    addAndMakeVisible(m_trigger_button);

    configureControlButton(m_record_button, "[rec]", kAccentRed, true);
    m_record_button.onClick = [this]() { toggle_record_enable(); };
    addAndMakeVisible(m_record_button);

    configureControlButton(m_clock_button, "[clk]", kAccentAmber, true);
    m_clock_button.setToggleState(false, juce::dontSendNotification);
    m_clock_button.setTooltip("Toggle clocked auto grains");
    m_clock_button.onClick = [this]() { update_auto_grain_settings(); };
    addAndMakeVisible(m_clock_button);

    configureControlButton(m_pattern_button, "[pr]", kAccentIndigo, false);
    m_pattern_button.setTooltip("Record/play the pattern sequencer");
    m_pattern_button.onClick = [this]() { handle_pattern_button(); };
    addAndMakeVisible(m_pattern_button);

    m_pattern_status_label.setJustificationType(juce::Justification::centredLeft);
    m_pattern_status_label.setColour(juce::Label::textColourId, kTerminalGreen);
    addAndMakeVisible(m_pattern_status_label);

    auto configurePatternKnob = [this](LayerCakeKnob& knob, bool rearm_on_change)
    {
        auto& slider = knob.slider();
        slider.onValueChange = [this, rearm_on_change]() { apply_pattern_settings(rearm_on_change); };
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

    configureControlButton(m_preset_button, "[pre]", kAccentMagenta, false);
    m_preset_button.onClick = [this]() { open_library_window(); };
    addAndMakeVisible(m_preset_button);

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
    open_library_window();
}

MainComponent::~MainComponent()
{
    DBG("LayerCakeApp::MainComponent dtor");
    stopTimer();
    if (m_library_window != nullptr)
    {
        if (auto* content = m_library_window->getContentComponent())
            content->setLookAndFeel(nullptr);
        m_library_window->setLookAndFeel(nullptr);
    }
    if (m_midi_mappings_file != juce::File())
    {
        m_midi_mappings_file.getParentDirectory().createDirectory();
        m_midi_learn_manager.saveMappings(m_midi_mappings_file);
    }
    removeKeyListener(&m_midi_learn_overlay);
    removeKeyListener(this);
    m_library_window = nullptr;
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

    juce::ColourGradient gradient(panel,
                                  bounds.getBottomLeft(),
                                  background.darker(0.4f),
                                  bounds.getTopRight(),
                                  false);
    gradient.addColour(0.35f, panel.brighter(0.08f));
    gradient.addColour(0.75f, background.darker(0.15f));
    g.setGradientFill(gradient);
    g.fillRect(bounds);

    const auto scanlineColour = kTerminalGreen.withAlpha(0.12f);
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

    const int buttonHeight = 44;
    const int meterHeight = 34;
    const int patternTransportHeight = 36;

    const int displayPanelWidth = 560;
    const int displaySize = 500;

    auto bounds = getLocalBounds().reduced(marginOuter);

    auto displayPanel = bounds.removeFromLeft(displayPanelWidth);
    auto tvArea = displayPanel.withSizeKeepingCentre(displaySize, displaySize);
    m_display.setBounds(tvArea);

    bounds.removeFromLeft(sectionSpacing);
    auto panel = bounds;

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
    panel.removeFromTop(rowSpacing);
    m_master_meter.setBounds(panel.removeFromTop(meterHeight));
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

    auto buttonRow = panel.removeFromTop(buttonHeight);
    const int buttonWidth = (buttonRow.getWidth() - 2 * rowSpacing) / 3;
    m_trigger_button.setBounds(buttonRow.removeFromLeft(buttonWidth));
    buttonRow.removeFromLeft(rowSpacing);
    m_record_button.setBounds(buttonRow.removeFromLeft(buttonWidth));
    buttonRow.removeFromLeft(rowSpacing);
    m_preset_button.setBounds(buttonRow.removeFromLeft(buttonWidth));
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

    auto patternTransport = panel.removeFromTop(patternTransportHeight);
    const int clockWidth = 80;
    const int patternButtonWidth = 80;
    m_clock_button.setBounds(patternTransport.removeFromLeft(clockWidth));
    patternTransport.removeFromLeft(rowSpacing);
    m_pattern_button.setBounds(patternTransport.removeFromLeft(patternButtonWidth));
    patternTransport.removeFromLeft(rowSpacing);
    m_pattern_status_label.setBounds(patternTransport);
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
    m_meter_display = new_value;
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
    juce::String button_text = "[pr]";
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

    const auto mode = clock->get_mode();
    if (mode == PatternClock::Mode::Playback || mode == PatternClock::Mode::Recording)
    {
        clock->set_enabled(false);
    }
    else
    {
        clock->set_enabled(false);
        clock->set_enabled(true);
    }

    update_pattern_labels();
}

void MainComponent::open_library_window()
{
    if (m_library_window != nullptr)
    {
        DBG("MainComponent::open_library_window focusing existing window");
        m_library_window->toFront(true);
        return;
    }

    DBG("MainComponent::open_library_window creating library window");
    auto capture_pattern = [this]() { return capture_pattern_data(); };
    auto capture_layers = [this]() { return capture_layer_buffers(); };
    auto apply_pattern = [this](const LayerCakePresetData& data) { apply_pattern_snapshot(data); };
    auto apply_layers = [this](const LayerBufferArray& buffers) { apply_layer_buffers(buffers); };
    auto on_close = [this]() { m_library_window = nullptr; };

    m_library_window = std::make_unique<LibraryBrowserWindow>(m_library_manager,
                                                              capture_pattern,
                                                              capture_layers,
                                                              apply_pattern,
                                                              apply_layers,
                                                              on_close);
    m_library_window->setLookAndFeel(&m_custom_look_and_feel);
    if (auto* content = m_library_window->getContentComponent())
        content->setLookAndFeel(&m_custom_look_and_feel);
}

LayerCakePresetData MainComponent::capture_pattern_data() const
{
    LayerCakePresetData data;
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

void MainComponent::apply_pattern_snapshot(const LayerCakePresetData& data)
{
    m_pattern_length_knob->slider().setValue(data.pattern_snapshot.pattern_length, juce::dontSendNotification);
    m_pattern_skip_knob->slider().setValue(data.pattern_snapshot.skip_probability, juce::dontSendNotification);
    const float bpm = Metro::period_ms_to_bpm(data.pattern_snapshot.period_ms);
    m_pattern_tempo_knob->slider().setValue(bpm, juce::dontSendNotification);
    if (m_pattern_subdiv_knob != nullptr)
        m_pattern_subdiv_knob->slider().setValue(data.pattern_subdivision, juce::dontSendNotification);

    if (auto* clock = m_engine.get_pattern_clock())
    {
        clock->apply_snapshot(data.pattern_snapshot);
        clock->set_enabled(data.pattern_snapshot.enabled);
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

    if (!clock->is_enabled())
    {
        DBG("MainComponent::rearm_pattern_clock clock disabled, skipping rearm");
        m_pattern_rearm_requested = false;
        return;
    }

    DBG("MainComponent::rearm_pattern_clock rearming after parameter edit");
    clock->set_enabled(false);
    clock->set_enabled(true);
    m_pattern_rearm_requested = false;
    update_pattern_labels();
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


