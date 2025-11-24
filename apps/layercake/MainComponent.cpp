#include "MainComponent.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <flowerjuce/LayerCakeEngine/PatternClock.h>
#include <flowerjuce/LayerCakeEngine/Metro.h>
#include <cmath>

namespace LayerCakeApp
{

juce::Font SettingsButtonLookAndFeel::getTextButtonFont(juce::TextButton& button, int buttonHeight)
{
    auto font = LayerCakeLookAndFeel::getTextButtonFont(button, buttonHeight);
    const float reducedHeight = juce::jmax(10.0f, font.getHeight() * 0.7f);
    return font.withHeight(reducedHeight);
}

void MultiChannelMeter::setLevels(const std::vector<double>& levels)
{
    const int desired_channels = juce::jlimit(1,
                                              kMaxChannels,
                                              static_cast<int>(levels.empty() ? 1 : levels.size()));
    bool changed = desired_channels != m_active_channels;

    for (int i = 0; i < desired_channels; ++i)
    {
        const double clamped = juce::jlimit(0.0, 1.0, levels.empty() ? 0.0 : levels[static_cast<size_t>(i)]);
        changed = changed || std::abs(clamped - m_levels[static_cast<size_t>(i)]) > 0.0005;
        m_levels[static_cast<size_t>(i)] = clamped;
    }

    for (int i = desired_channels; i < kMaxChannels; ++i)
        m_levels[static_cast<size_t>(i)] = 0.0;

    if (desired_channels != m_active_channels)
        m_active_channels = desired_channels;

    if (changed)
        repaint();
}

void MultiChannelMeter::paint(juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat().reduced(2.0f);
    if (area.isEmpty())
        return;

    const int channels = juce::jmax(1, m_active_channels);
    const float spacing = channels > 1 ? 4.0f : 0.0f;
    const float total_spacing = spacing * static_cast<float>(channels - 1);
    const float slot_width = juce::jmax(6.0f, (area.getWidth() - total_spacing) / static_cast<float>(channels));
    const float corner = juce::jmin(6.0f, slot_width * 0.4f);

    auto slotArea = area;
    for (int channel = 0; channel < channels; ++channel)
    {
        juce::Rectangle<float> slot(slotArea.removeFromLeft(slot_width));
        slotArea.removeFromLeft(spacing);

        const auto background = findColour(juce::ProgressBar::backgroundColourId).withAlpha(0.85f);
        const auto outline = findColour(juce::Slider::trackColourId).withAlpha(0.45f);

        g.setColour(background);
        g.fillRoundedRectangle(slot, corner);

        auto fill_bounds = slot.reduced(2.0f);
        const float level = static_cast<float>(juce::jlimit(0.0, 1.0, m_levels[static_cast<size_t>(channel)]));
        const float fill_height = fill_bounds.getHeight() * level;
        if (fill_height > 0.0f)
        {
            auto filled = fill_bounds.removeFromBottom(fill_height);
            const double db = static_cast<double>(juce::Decibels::gainToDecibels(level, -60.0f));
            g.setColour(colour_for_db(db));
            g.fillRoundedRectangle(filled, corner * 0.5f);
        }

        g.setColour(outline);
        g.drawRoundedRectangle(slot, corner, 1.0f);
    }
}

juce::Colour MultiChannelMeter::colour_for_db(double db) const
{
    if (db < -18.0)
        return juce::Colour(0xff4caf50); // green
    if (db < -6.0)
        return juce::Colour(0xfffbc02d); // yellow
    return juce::Colour(0xfff44336);     // red
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
const juce::Colour kSoftWhite(0xfff4f4f2);
const juce::Colour kBlueGrey(0xff5d6f85);
const juce::Colour kWarmMagenta(0xfff25f8c);
const juce::Colour kPatternGreen(0xff63ff87);
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

MainComponent::MainComponent(std::optional<juce::AudioDeviceManager::AudioDeviceSetup> initialDeviceSetup)
    : m_title_label("title", "layercake"),
      m_record_layer_label("recordLayer", ""),
      m_record_status_label("recordStatus", ""),
      m_trigger_button("trg"),
      m_record_button("rec"),
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
    m_device_manager.addChangeListener(this);

    addAndMakeVisible(m_display);

    for (auto& meter_level : m_meter_levels)
        meter_level.store(0.0f, std::memory_order_relaxed);
    m_meter_channel_count.store(1, std::memory_order_relaxed);

    for (auto& value : m_lfo_last_values)
        value.store(0.0f, std::memory_order_relaxed);

    const std::array<juce::Colour, 4> lfoPalette = {
        m_custom_look_and_feel.getLayerColour(0).brighter(0.6f),
        m_custom_look_and_feel.getLayerColour(1).brighter(0.6f),
        m_custom_look_and_feel.getLayerColour(2).brighter(0.6f),
        m_custom_look_and_feel.getLayerColour(3).brighter(0.6f)
    };
    const std::array<juce::Colour, 4> secondaryLfoPalette = {
        juce::Colour(0xff8ecae6),
        juce::Colour(0xfff4a261),
        juce::Colour(0xff9b5de5),
        juce::Colour(0xff2ec4b6)
    };

    for (size_t i = 0; i < m_lfo_slots.size(); ++i)
    {
        auto& slot = m_lfo_slots[i];
        const bool isSecondRow = i >= lfoPalette.size();
        slot.accent = isSecondRow
            ? secondaryLfoPalette[i % secondaryLfoPalette.size()].withAlpha(0.9f)
            : lfoPalette[i % lfoPalette.size()];
        slot.label = "LFO " + juce::String(static_cast<int>(i) + 1);
        slot.generator.set_mode(flower::LfoWaveform::Sine);
        slot.generator.set_rate_hz(0.35f + static_cast<float>(i) * 0.15f);
        slot.generator.set_depth(0.5f);
        slot.generator.reset_phase(static_cast<double>(i) / static_cast<double>(m_lfo_slots.size()));

        slot.widget = std::make_unique<LayerCakeLfoWidget>(static_cast<int>(i), slot.generator, slot.accent);
        slot.widget->set_drag_label(slot.label);
        slot.widget->set_on_settings_changed([this, index = static_cast<int>(i)]()
        {
            if (index < 0 || index >= static_cast<int>(m_lfo_slots.size()))
            {
                DBG("MainComponent::lfo callback early return (index out of range)");
                return;
            }

            auto* widget = m_lfo_slots[static_cast<size_t>(index)].widget.get();
            if (widget != nullptr)
                widget->refresh_wave_preview();
            update_all_modulation_overlays();
        });
        if (slot.widget != nullptr)
        {
            slot.widget->set_tempo_provider([this]() -> double
            {
                if (m_pattern_tempo_knob != nullptr)
                    return juce::jmax(1.0, get_effective_knob_value(m_pattern_tempo_knob.get()));
                return 120.0;
            });
            slot.widget->set_tempo_sync_callback([this, slotIndex = static_cast<int>(i)](bool enabled)
            {
                if (!juce::isPositiveAndBelow(slotIndex, static_cast<int>(m_lfo_slots.size())))
                    return;
                m_lfo_slots[static_cast<size_t>(slotIndex)].tempo_sync = enabled;
            });
            slot.widget->set_tempo_sync_enabled(slot.tempo_sync, true);
            slot.widget->refresh_wave_preview();
            addAndMakeVisible(slot.widget.get());
        }
        else
        {
            DBG("MainComponent ctor LFO widget creation failed index=" + juce::String(static_cast<int>(i)));
        }
    }

    m_title_label.setJustificationType(juce::Justification::centredLeft);
    juce::FontOptions titleOptions;
    titleOptions = titleOptions.withName(juce::Font::getDefaultMonospacedFontName())
                               .withHeight(48.0f);
    juce::Font titleFont(titleOptions);
    titleFont.setBold(true);
    m_title_label.setFont(titleFont);
    addAndMakeVisible(m_title_label);

    m_record_layer_label.setJustificationType(juce::Justification::centredLeft);
    m_record_layer_label.setColour(juce::Label::textColourId, kSoftWhite);
    addAndMakeVisible(m_record_layer_label);

    m_record_status_label.setJustificationType(juce::Justification::centredLeft);
    m_record_status_label.setColour(juce::Label::textColourId, kSoftWhite);
    addAndMakeVisible(m_record_status_label);

    m_settings_button.setButtonText("settings");
    m_settings_button.setLookAndFeel(&m_settings_button_look_and_feel);
    m_settings_button.onClick = [this] { open_settings_window(); };
    addAndMakeVisible(m_settings_button);

    auto makeKnob = [this](LayerCakeKnob::Config config, bool enableRecorder = true) {
        config.enableSweepRecorder = enableRecorder;
        auto knob = std::make_unique<LayerCakeKnob>(config, &m_midi_learn_manager);
        register_knob_for_lfo(knob.get());
        addAndMakeVisible(knob.get());
        return knob;
    };

    auto tintKnob = [](LayerCakeKnob* knob, juce::Colour colour) {
        if (knob != nullptr)
            knob->set_knob_colour(colour);
    };

    auto bindManualKnob = [this](LayerCakeKnob* knob) {
        if (knob == nullptr)
            return;
        knob->slider().onValueChange = [this]() {
            sync_manual_state_from_controls();
        };
    };

    m_master_gain_knob = makeKnob({ "master gain", -24.0, 6.0, 0.0, 0.1, " dB", "layercake_master_gain" });
    tintKnob(m_master_gain_knob.get(), kSoftWhite);
    m_master_gain_knob->slider().onValueChange = [this]() {
        const float gain = static_cast<float>(get_effective_knob_value(m_master_gain_knob.get()));
        m_engine.set_master_gain_db(gain);
    };

    m_loop_start_knob = makeKnob({ "position", 0.0, 1.0, 0.5, 0.001, "", "layercake_position" });
    tintKnob(m_loop_start_knob.get(), kBlueGrey);
    bindManualKnob(m_loop_start_knob.get());
    m_duration_knob = makeKnob({ "duration", 10.0, 5000.0, 300.0, 1.0, " ms", "layercake_duration" });
    tintKnob(m_duration_knob.get(), kBlueGrey);
    bindManualKnob(m_duration_knob.get());
    m_rate_knob = makeKnob({ "rate", -24.0, 24.0, 0.0, 0.1, " st", "layercake_rate" });
    tintKnob(m_rate_knob.get(), kWarmMagenta);
    bindManualKnob(m_rate_knob.get());
    m_env_knob = makeKnob({ "env", 0.0, 1.0, 0.5, 0.01, "", "layercake_env" });
    tintKnob(m_env_knob.get(), kWarmMagenta);
    bindManualKnob(m_env_knob.get());
    m_direction_knob = makeKnob({ "direction", 0.0, 1.0, 0.5, 0.01, "", "layercake_direction" });
    tintKnob(m_direction_knob.get(), kWarmMagenta);
    bindManualKnob(m_direction_knob.get());
    m_pan_knob = makeKnob({ "pan", 0.0, 1.0, 0.5, 0.01, "", "layercake_pan" });
    tintKnob(m_pan_knob.get(), kWarmMagenta);
    bindManualKnob(m_pan_knob.get());
    m_layer_select_knob = makeKnob({ "layer", 1.0, static_cast<double>(LayerCakeEngine::kNumLayers), 1.0, 1.0, "", "layercake_layer_select" });
    tintKnob(m_layer_select_knob.get(), kBlueGrey);
    m_layer_select_knob->slider().setDoubleClickReturnValue(true, 1.0);
    m_layer_select_knob->slider().onValueChange = [this]() {
        const double effective = get_effective_knob_value(m_layer_select_knob.get());
        const int raw = static_cast<int>(std::round(effective)) - 1;
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
    m_master_meter.setLevels({ 0.0 });
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
    m_pattern_status_label.setColour(juce::Label::textColourId, kSoftWhite);
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
    tintKnob(m_pattern_length_knob.get(), kPatternGreen);
    configurePatternKnob(*m_pattern_length_knob, true);

    m_pattern_skip_knob = makeKnob({ "rskip", 0.0, 1.0, 0.0, 0.01, "", "layercake_pattern_rskip" });
    tintKnob(m_pattern_skip_knob.get(), kPatternGreen);
    configurePatternKnob(*m_pattern_skip_knob, false);

    m_pattern_tempo_knob = makeKnob({ "tempo", 10.0, 600.0, 90.0, 0.1, " bpm", "layercake_pattern_tempo" });
    tintKnob(m_pattern_tempo_knob.get(), kPatternGreen);
    configurePatternKnob(*m_pattern_tempo_knob, true);
    m_last_pattern_bpm = juce::jmax(1.0, get_effective_knob_value(m_pattern_tempo_knob.get()));

    m_pattern_subdiv_knob = makeKnob({ "subdiv", -3.0, 3.0, 0.0, 1.0, "", "layercake_pattern_subdiv" });
    tintKnob(m_pattern_subdiv_knob.get(), kPatternGreen);
    m_pattern_subdiv_knob->slider().setDoubleClickReturnValue(true, 0.0);
    m_pattern_subdiv_knob->slider().setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    configurePatternKnob(*m_pattern_subdiv_knob, false);

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
    configure_audio_device(std::move(initialDeviceSetup));
    // refresh_input_channel_selector(); // Moved to SettingsComponent
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
    m_device_manager.removeChangeListener(this);
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
    m_settings_button.setLookAndFeel(nullptr);
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

    g.setColour(kSoftWhite.withAlpha(0.35f));
    g.drawRect(bounds, 1.5f);
}

void MainComponent::resized()
{
    const int marginOuter = 20;
    const int sectionSpacing = 18;
    const int rowSpacing = 12;

    const int titleHeight = 64;
    const int labelHeight = 28;

    const int knobDiameter = 86;
    const int knobLabelHeight = 18;
    const int knobLabelGap = 4;
    const int knobStackHeight = knobDiameter + knobLabelGap + knobLabelHeight;
    const int knobSpacing = 14;
    const int knobGridColumns = 4;

    const int buttonHeight = 22;
    const int meterWidth = 40;
    const int meterHeight = 120;
    const int meterSpacing = 18;
    const int patternTransportHeight = 36;

    const int displayPanelWidth = 620;
    const int displayWidth = 560;
    const int displayHeight = 260;

    const int presetPanelSpacing = 16;
    const int presetPanelMargin = 10;
    const int presetPanelHeightVisible = 240;
    const int lfoRowHeight = 160;
    const int lfoSpacing = 14;
    const int lfoMargin = 10;
    const int lfoSlotMinWidth = 80;
    const int lfoVerticalGap = 12;
    const int lfoRowSpacing = 12;
    const int lfosPerRow = 4;
    const int inputSectionLabelHeight = 26;
    const int inputSectionHintHeight = 18;
    const int inputSectionButtonHeight = 24;
    const int inputSectionViewportHeight = 120;
    const int inputSectionSpacing = 8;

    auto bounds = getLocalBounds().reduced(marginOuter);
    if (m_preset_panel != nullptr)
    {
        if (m_preset_panel_visible)
        {
            const int panelHeight = juce::jmin(bounds.getHeight(), presetPanelHeightVisible);
            auto panelArea = bounds.removeFromBottom(panelHeight);
            bounds.removeFromBottom(presetPanelSpacing);
            m_preset_panel->setBounds(panelArea.reduced(presetPanelMargin));
        }
        else
        {
            m_preset_panel->setBounds({});
        }
    }

    auto titleGlobalArea = juce::Rectangle<int>(bounds.getX(),
                                                bounds.getY(),
                                                displayPanelWidth,
                                                titleHeight);
    m_title_label.setBounds(titleGlobalArea);

    auto meterSlice = bounds.removeFromRight(meterWidth);
    bounds.removeFromRight(meterSpacing);
    auto meterArea = meterSlice;
    if (meterArea.getHeight() > meterHeight)
        meterArea = meterArea.withHeight(meterHeight).withY(meterSlice.getBottom() - meterHeight);
    m_master_meter.setBounds(meterArea);

    const int lfoCount = static_cast<int>(m_lfo_slots.size());
    const int lfoRows = lfoCount > 0 ? juce::jmax(1, (lfoCount + lfosPerRow - 1) / lfosPerRow) : 0;
    const int lfoAreaHeight = lfoRows > 0 ? (lfoRows * lfoRowHeight + (lfoRows - 1) * lfoRowSpacing) : 0;

    auto displayColumn = bounds.removeFromLeft(displayPanelWidth);
    auto lfoArea = displayColumn.removeFromBottom(lfoAreaHeight);
    displayColumn.removeFromBottom(lfoVerticalGap);
    auto tvArea = displayColumn.withSizeKeepingCentre(displayWidth, displayHeight);
    m_display.setBounds(tvArea);

    bounds.removeFromLeft(sectionSpacing);
    auto panel = bounds;

    panel.removeFromTop(labelHeight);
    panel.removeFromTop(rowSpacing);

    auto recordArea = panel.removeFromTop(labelHeight);
    auto leftRecord = recordArea.removeFromLeft(recordArea.getWidth() / 2);
    m_record_layer_label.setBounds(leftRecord);
    m_record_status_label.setBounds(recordArea);
    panel.removeFromTop(rowSpacing);

    const int inputSelectorHeight = inputSectionLabelHeight
                                    + inputSectionHintHeight
                                    + inputSectionButtonHeight * 2
                                    + inputSectionSpacing * 3;
    // auto inputSelector = panel.removeFromTop(inputSelectorHeight); // Removed
    // auto inputLabelArea = inputSelector.removeFromTop(inputSectionLabelHeight);
    // m_input_section_label.setBounds(inputLabelArea);
    // inputSelector.removeFromTop(inputSectionSpacing);
    // auto inputHintArea = inputSelector.removeFromTop(inputSectionHintHeight);
    // m_input_section_hint.setBounds(inputHintArea);
    // inputSelector.removeFromTop(inputSectionSpacing);

    // auto selectorArea = inputSelector.removeFromTop(inputSectionButtonHeight);
    // m_input_channel_selector.setBounds(selectorArea);

    panel.removeFromTop(rowSpacing);

    const int knobGridRows = 4;
    const int knobGridHeight = knobGridRows * knobStackHeight + (knobGridRows - 1) * knobSpacing;
    auto knobGridArea = panel.removeFromTop(knobGridHeight);
    const int knobGridX = knobGridArea.getX();
    const int knobGridY = knobGridArea.getY();
    auto cellBounds = [&](int row, int col) {
        const int x = knobGridX + col * (knobDiameter + knobSpacing);
        const int y = knobGridY + row * (knobStackHeight + knobSpacing);
        return juce::Rectangle<int>(x, y, knobDiameter, knobStackHeight);
    };
    auto placeCell = [&](LayerCakeKnob* knob, int row, int col)
    {
        if (knob == nullptr)
            return;
        knob->setBounds(cellBounds(row, col));
    };

    placeCell(m_master_gain_knob.get(), 0, 3);
    placeCell(m_env_knob.get(), 1, 0);
    placeCell(m_direction_knob.get(), 1, 2);
    placeCell(m_pan_knob.get(), 1, 3);
    placeCell(m_pattern_length_knob.get(), 2, 0);
    placeCell(m_pattern_skip_knob.get(), 2, 1);
    placeCell(m_rate_knob.get(), 2, 2);
    placeCell(m_layer_select_knob.get(), 2, 3);
    placeCell(m_pattern_tempo_knob.get(), 3, 0);
    placeCell(m_pattern_subdiv_knob.get(), 3, 1);
    placeCell(m_loop_start_knob.get(), 3, 2);
    placeCell(m_duration_knob.get(), 3, 3);

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
    m_clock_button.setBounds(gridCellBounds(0, 1, buttonRowY, buttonHeight));
    m_record_button.setBounds(gridCellBounds(1, 1, buttonRowY, buttonHeight));
    m_pattern_button.setBounds(gridCellBounds(2, 1, buttonRowY, buttonHeight));
    m_trigger_button.setBounds(gridCellBounds(3, 1, buttonRowY, buttonHeight));

    panel.removeFromTop(rowSpacing);
    auto statusRow = panel.removeFromTop(buttonHeight);
    m_pattern_status_label.setBounds(statusRow);
    // panel.removeFromTop(sectionSpacing); // was after statusRow

    // Adding settings button
    auto settingsArea = titleGlobalArea.removeFromRight(100).reduced(10);
    m_settings_button.setBounds(settingsArea);

    auto lfoRowBounds = lfoArea.reduced(lfoMargin);
    if (lfoCount > 0 && !lfoRowBounds.isEmpty())
    {
        int slotIndex = 0;
        auto rowWalker = lfoRowBounds;
        for (int row = 0; row < lfoRows; ++row)
        {
            auto rowArea = rowWalker.removeFromTop(lfoRowHeight);
            if (row < lfoRows - 1)
                rowWalker.removeFromTop(lfoRowSpacing);

            const int remaining = lfoCount - row * lfosPerRow;
            const int columnsThisRow = juce::jlimit(1, lfosPerRow, remaining);
            const int totalSpacing = lfoSpacing * juce::jmax(0, columnsThisRow - 1);
            const int slotWidth = juce::jmax(lfoSlotMinWidth,
                                             (rowArea.getWidth() - totalSpacing) / juce::jmax(1, columnsThisRow));

            auto rowColumns = rowArea;
            for (int column = 0; column < columnsThisRow && slotIndex < lfoCount; ++column)
            {
                auto widgetBounds = rowColumns.removeFromLeft(slotWidth);
                if (column < columnsThisRow - 1)
                    rowColumns.removeFromLeft(lfoSpacing);

                if (auto* widget = m_lfo_slots[static_cast<size_t>(slotIndex)].widget.get())
                    widget->setBounds(widgetBounds);
                ++slotIndex;
            }
        }
    }

    m_midi_learn_overlay.setBounds(getLocalBounds());
}

void MainComponent::open_settings_window()
{
    if (m_settings_window == nullptr)
    {
        m_settings_window = std::make_unique<LayerCakeSettingsWindow>(m_device_manager);
    }
    m_settings_window->setVisible(true);
    m_settings_window->toFront(true);
}

SettingsComponent::SettingsComponent(juce::AudioDeviceManager& deviceManager)
    : m_device_manager(deviceManager)
{
    m_input_label.setText("Input Channel:", juce::dontSendNotification);
    addAndMakeVisible(m_input_label);

    m_input_selector.onChange = [this] { apply_selected_input_channels(); };
    addAndMakeVisible(m_input_selector);
    
    refresh_input_channel_selector();
    setSize(300, 200);
}

void SettingsComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::darkgrey);
}

void SettingsComponent::resized()
{
    auto area = getLocalBounds().reduced(20);
    auto inputRow = area.removeFromTop(30);
    m_input_label.setBounds(inputRow.removeFromLeft(100));
    inputRow.removeFromLeft(10);
    m_input_selector.setBounds(inputRow);
}

void SettingsComponent::refresh_input_channel_selector()
{
    m_input_selector.clear();
    auto* device = m_device_manager.getCurrentAudioDevice();
    if (device == nullptr)
    {
        m_input_channel_names.clear();
        return;
    }

    m_input_channel_names = device->getInputChannelNames();
    if (m_input_channel_names.isEmpty())
    {
        m_input_selector.addItem("No Inputs Available", 1);
        m_input_selector.setEnabled(false);
        return;
    }
    
    m_input_selector.setEnabled(true);
    for (int i = 0; i < m_input_channel_names.size(); ++i)
    {
        m_input_selector.addItem(juce::String(i + 1) + ". " + m_input_channel_names[i], i + 1);
    }

    juce::AudioDeviceManager::AudioDeviceSetup setup;
    m_device_manager.getAudioDeviceSetup(setup);
    
    int activeIndex = -1;
    // Find the first active input channel
    if (!setup.useDefaultInputChannels && setup.inputChannels.getHighestBit() >= 0)
    {
        for (int i = 0; i < m_input_channel_names.size(); ++i)
        {
            if (setup.inputChannels[i])
            {
                activeIndex = i;
                break;
            }
        }
    }

    if (activeIndex >= 0)
    {
        m_input_selector.setSelectedId(activeIndex + 1, juce::dontSendNotification);
    }
    else
    {
        // Default to first channel if none selected or using defaults
        m_input_selector.setSelectedId(1, juce::dontSendNotification);
    }
}

void SettingsComponent::apply_selected_input_channels()
{
    const int selectedId = m_input_selector.getSelectedId();
    if (selectedId <= 0)
        return;

    const int channelIndex = selectedId - 1;

    juce::AudioDeviceManager::AudioDeviceSetup setup;
    m_device_manager.getAudioDeviceSetup(setup);

    setup.inputChannels.clear();
    setup.inputChannels.setBit(channelIndex, true);
    setup.useDefaultInputChannels = false;

    auto error = m_device_manager.setAudioDeviceSetup(setup, true);
    if (error.isNotEmpty())
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                               "LayerCake",
                                               "Unable to update input routing:\n" + error);
        refresh_input_channel_selector();
        return;
    }
}

void MainComponent::configure_audio_device(std::optional<juce::AudioDeviceManager::AudioDeviceSetup> initialSetup)
{
    juce::String error = m_device_manager.initialise(1, 2, nullptr, true);
    if (error.isNotEmpty())
    {
        DBG("Audio device init error: " + error);
        return;
    }

    if (initialSetup.has_value())
    {
        const auto findDeviceType = [this](const juce::AudioDeviceManager::AudioDeviceSetup& setup)
        {
            juce::String deviceType;
            const auto& deviceTypes = m_device_manager.getAvailableDeviceTypes();
            for (int i = 0; i < deviceTypes.size(); ++i)
            {
                auto* type = deviceTypes[i];
                if (type == nullptr)
                    continue;

                const auto outputDevices = type->getDeviceNames(false);
                const auto inputDevices = type->getDeviceNames(true);

                bool foundDevice = setup.outputDeviceName.isNotEmpty()
                                   && outputDevices.contains(setup.outputDeviceName);
                if (!foundDevice && setup.inputDeviceName.isNotEmpty())
                    foundDevice = inputDevices.contains(setup.inputDeviceName);

                if (foundDevice)
                {
                    deviceType = type->getTypeName();
                    break;
                }
            }
            return deviceType;
        };

        const auto deviceType = findDeviceType(*initialSetup);
        if (deviceType.isNotEmpty())
        {
            m_device_manager.setCurrentAudioDeviceType(deviceType, false);
            DBG("Audio device type switched to: " + deviceType);
        }
        else
        {
            DBG("Audio device type not found for startup selection");
        }

        auto setupError = m_device_manager.setAudioDeviceSetup(*initialSetup, true);
        if (setupError.isNotEmpty())
        {
            DBG("Audio device setup apply error: " + setupError);
        }
        else
        {
            DBG("Audio device setup from startup dialog applied");
        }
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
    const int meter_channels = juce::jmax(1, juce::jmin(MultiChannelMeter::kMaxChannels, outputs));
    m_meter_channel_count.store(meter_channels, std::memory_order_relaxed);
    for (auto& meter_level : m_meter_levels)
        meter_level.store(0.0f, std::memory_order_relaxed);
    DBG("Audio device started sampleRate=" + juce::String(sample_rate) + " block=" + juce::String(block_size));
}

void MainComponent::audioDeviceStopped()
{
    DBG("Audio device stopped");
    m_device_ready = false;
    m_meter_channel_count.store(1, std::memory_order_relaxed);
    for (auto& meter_level : m_meter_levels)
        meter_level.store(0.0f, std::memory_order_relaxed);
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

    const int meter_channels = juce::jmax(1, juce::jmin(MultiChannelMeter::kMaxChannels, numOutputChannels));
    for (int channel = 0; channel < meter_channels; ++channel)
    {
        float peak = 0.0f;
        if (channel < numOutputChannels)
        {
            const float* channel_data = outputChannelData[channel];
            if (channel_data != nullptr)
            {
                for (int sample = 0; sample < numSamples; ++sample)
                    peak = juce::jmax(peak, std::abs(channel_data[sample]));
            }
        }
        m_meter_levels[static_cast<size_t>(channel)].store(peak, std::memory_order_relaxed);
    }
    for (int channel = meter_channels; channel < MultiChannelMeter::kMaxChannels; ++channel)
        m_meter_levels[static_cast<size_t>(channel)].store(0.0f, std::memory_order_relaxed);
    m_meter_channel_count.store(meter_channels, std::memory_order_relaxed);
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
    const double now_ms = juce::Time::getMillisecondCounterHiRes();
    advance_lfos(now_ms);
    update_all_modulation_overlays();
    update_master_gain_from_knob();
    update_record_layer_from_lfo();
    update_record_labels();
    update_meter();
    apply_pattern_settings(false);
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
    const double normalized_start = (m_loop_start_knob != nullptr)
        ? juce::jlimit(0.0, 1.0, get_effective_knob_value(m_loop_start_knob.get()))
        : 0.0;
    double loop_start_seconds = normalized_start * recorded_seconds;

    double duration_ms = get_effective_knob_value(m_duration_knob.get());
    double duration_seconds = duration_ms * 0.001;

    if (recorded_seconds > 0.0)
    {
        const double max_duration_seconds = juce::jmax(0.0, recorded_seconds - loop_start_seconds);
        duration_seconds = juce::jlimit(0.0, max_duration_seconds, duration_seconds);
    }
    duration_ms = duration_seconds * 1000.0;

    double env_value = m_env_knob != nullptr ? get_effective_knob_value(m_env_knob.get()) : 0.5;
    env_value = juce::jlimit(0.0, 1.0, env_value);
    const double attack_ms = duration_ms * (1.0 - env_value);
    const double release_ms = duration_ms * env_value;

    state.loop_start_seconds = static_cast<float>(loop_start_seconds);
    state.duration_ms = static_cast<float>(duration_ms);
    state.rate_semitones = static_cast<float>(get_effective_knob_value(m_rate_knob.get()));
    state.env_attack_ms = static_cast<float>(attack_ms);
    state.env_release_ms = static_cast<float>(release_ms);
    state.play_forward = true;
    state.layer = layer;
    state.pan = static_cast<float>(get_effective_knob_value(m_pan_knob.get()));
    float reverse_probability = m_direction_knob != nullptr ? static_cast<float>(get_effective_knob_value(m_direction_knob.get())) : 0.0f;

    // Spread parameter removed; randomization disabled for manual grains.
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
    if (m_layer_select_knob != nullptr && !m_layer_select_knob->has_lfo_assignment())
        m_layer_select_knob->slider().setValue(static_cast<double>(layer_index + 1), juce::sendNotificationSync);
    sync_manual_state_from_controls();
}

void MainComponent::update_meter()
{
    const int channel_count = juce::jlimit(1,
                                           MultiChannelMeter::kMaxChannels,
                                           m_meter_channel_count.load(std::memory_order_relaxed));
    std::vector<double> levels;
    levels.reserve(channel_count);
    for (int i = 0; i < channel_count; ++i)
    {
        const double level = juce::jlimit(0.0,
                                          1.0,
                                          static_cast<double>(m_meter_levels[static_cast<size_t>(i)].load(std::memory_order_relaxed)));
        levels.push_back(level);
    }
    m_master_meter.setLevels(levels);
}

void MainComponent::apply_pattern_settings(bool request_rearm)
{
    auto* clock = m_engine.get_pattern_clock();
    if (clock == nullptr)
    {
        DBG("MainComponent::apply_pattern_settings PatternClock unavailable");
        return;
    }

    const double length_value = get_effective_knob_value(m_pattern_length_knob.get());
    clock->set_pattern_length(juce::jlimit(1, 128, static_cast<int>(std::round(length_value))));
    clock->set_skip_probability(static_cast<float>(juce::jlimit(0.0, 1.0, get_effective_knob_value(m_pattern_skip_knob.get()))));

    double base_bpm = juce::jmax(1.0, get_effective_knob_value(m_pattern_tempo_knob.get()));
    double subdiv = m_pattern_subdiv_knob != nullptr ? get_effective_knob_value(m_pattern_subdiv_knob.get()) : 0.0;
    double multiplier = std::pow(2.0, subdiv);
    double effective_bpm = juce::jlimit(1.0, 2000.0, base_bpm * multiplier);
    clock->set_bpm(static_cast<float>(effective_bpm));
    if (std::abs(effective_bpm - m_last_pattern_bpm) > 0.001)
    {
        m_last_pattern_bpm = effective_bpm;
        refresh_lfo_tempo_sync();
    }
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
    data.clock_enabled = m_clock_button.getToggleState();
    data.manual_state = m_manual_state;
    data.manual_state.should_trigger = false;
    data.record_layer = m_engine.get_record_layer();
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
    capture(m_direction_knob.get());
    capture(m_pan_knob.get());
    capture(m_layer_select_knob.get());
    capture(m_pattern_length_knob.get());
    capture(m_pattern_skip_knob.get());
    capture(m_pattern_tempo_knob.get());
    capture(m_pattern_subdiv_knob.get());

    capture_lfo_state(data);

    return data;
}

void MainComponent::capture_lfo_state(LayerCakePresetData& data) const
{
    const size_t slotCount = juce::jmin(m_lfo_slots.size(), data.lfo_slots.size());
    for (size_t i = 0; i < slotCount; ++i)
    {
        const auto& slot = m_lfo_slots[i];
        auto& slotData = data.lfo_slots[i];
        slotData.mode = static_cast<int>(slot.generator.get_mode());
        slotData.rate_hz = slot.generator.get_rate_hz();
        slotData.depth = slot.generator.get_depth();
        slotData.tempo_sync = slot.widget != nullptr && slot.widget->is_tempo_sync_enabled();
    }

    data.lfo_assignments.clear();
    for (auto* knob : m_lfo_enabled_knobs)
    {
        if (knob == nullptr)
            continue;
        const int assignment = knob->lfo_assignment_index();
        if (assignment < 0)
            continue;
        const auto& parameterId = knob->parameter_id();
        if (parameterId.isEmpty())
            continue;
        data.lfo_assignments.set(juce::Identifier(parameterId), assignment);
    }
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
    applyValue(m_direction_knob.get(), false);
    applyValue(m_pan_knob.get(), false);
    applyValue(m_layer_select_knob.get(), false);
    applyValue(m_pattern_length_knob.get(), true);
    applyValue(m_pattern_skip_knob.get(), true);
    applyValue(m_pattern_tempo_knob.get(), true);
    applyValue(m_pattern_subdiv_knob.get(), true);

    apply_lfo_state(data);

    m_clock_button.setToggleState(data.clock_enabled, juce::dontSendNotification);
    update_auto_grain_settings();

    if (update_pattern_engine && pattern_knob_touched)
        apply_pattern_settings(true);
}

void MainComponent::apply_lfo_state(const LayerCakePresetData& data)
{
    const size_t slotCount = juce::jmin(m_lfo_slots.size(), data.lfo_slots.size());
    const int maxMode = static_cast<int>(flower::LfoWaveform::SmoothRandom);

    for (size_t i = 0; i < slotCount; ++i)
    {
        auto& slot = m_lfo_slots[i];
        const auto& slotData = data.lfo_slots[i];
        const int modeIndex = juce::jlimit(0, maxMode, slotData.mode);
        slot.generator.set_mode(static_cast<flower::LfoWaveform>(modeIndex));
        slot.generator.set_rate_hz(juce::jlimit(0.01f, 20.0f, slotData.rate_hz));
        slot.generator.set_depth(juce::jlimit(0.0f, 1.0f, slotData.depth));
        slot.generator.reset_phase();
        m_lfo_last_values[i].store(slot.generator.get_last_value(), std::memory_order_relaxed);
        if (slot.widget != nullptr)
        {
            slot.widget->sync_controls_from_generator();
            slot.widget->set_tempo_sync_enabled(slotData.tempo_sync, true);
        }
        slot.tempo_sync = slotData.tempo_sync;
    }

    for (auto* knob : m_lfo_enabled_knobs)
    {
        if (knob == nullptr)
            continue;

        knob->set_lfo_assignment_index(-1);
        knob->clear_modulation_indicator();
        knob->set_lfo_button_accent(std::nullopt);

        const auto& parameterId = knob->parameter_id();
        if (parameterId.isEmpty())
            continue;

        const auto identifier = juce::Identifier(parameterId);
        if (const juce::var* value = data.lfo_assignments.getVarPointer(identifier))
        {
            const int index = static_cast<int>(*value);
            if (index >= 0 && index < static_cast<int>(m_lfo_slots.size()))
            {
                knob->set_lfo_assignment_index(index);
                knob->set_lfo_button_accent(m_lfo_slots[static_cast<size_t>(index)].accent);
            }
        }
    }

    update_all_modulation_overlays();
    refresh_lfo_tempo_sync();
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
    const double loop_start_normalized = (m_loop_start_knob != nullptr)
        ? juce::jlimit(0.0, 1.0, get_effective_knob_value(m_loop_start_knob.get()))
        : 0.0;
    m_manual_state.loop_start_seconds = static_cast<float>(juce::jlimit(0.0, recorded_seconds, loop_start_normalized * recorded_seconds));
    const double duration_ms = get_effective_knob_value(m_duration_knob.get());
    m_manual_state.duration_ms = static_cast<float>(duration_ms);
    m_manual_state.rate_semitones = static_cast<float>(get_effective_knob_value(m_rate_knob.get()));
    const double env_value = m_env_knob != nullptr ? juce::jlimit(0.0, 1.0, get_effective_knob_value(m_env_knob.get()))
                                                   : 0.5;
    m_manual_state.env_attack_ms = static_cast<float>(duration_ms * (1.0 - env_value));
    m_manual_state.env_release_ms = static_cast<float>(duration_ms * env_value);
    m_manual_state.play_forward = true;
    m_manual_state.pan = static_cast<float>(get_effective_knob_value(m_pan_knob.get()));
    m_manual_state.layer = layer;
    m_manual_state.should_trigger = false;
    m_display.set_position_indicator(static_cast<float>(loop_start_normalized));
    update_auto_grain_settings();
}

void MainComponent::advance_lfos(double now_ms)
{
    for (size_t i = 0; i < m_lfo_slots.size(); ++i)
    {
        auto& slot = m_lfo_slots[i];
        const float rawValue = slot.generator.advance(now_ms);
        const float scaled = rawValue * slot.generator.get_depth();
        m_lfo_last_values[i].store(scaled, std::memory_order_relaxed);
    }
}

void MainComponent::register_knob_for_lfo(LayerCakeKnob* knob)
{
    if (knob == nullptr)
    {
        DBG("MainComponent::register_knob_for_lfo early return (null knob)");
        return;
    }

    knob->set_lfo_highlight_colour(m_custom_look_and_feel.getKnobLabelColour());
    knob->set_lfo_drop_handler([this](LayerCakeKnob& target, int lfoIndex) {
        assign_lfo_to_knob(lfoIndex, target);
    });
    knob->set_lfo_release_handler([this, knob]() {
        remove_lfo_from_knob(*knob);
    });
    knob->set_lfo_button_accent(std::nullopt);
    knob->set_context_menu_builder([this, knob](juce::PopupMenu& menu) {
        if (knob == nullptr)
            return;
        const int assignment = knob->lfo_assignment_index();
        if (assignment < 0 || assignment >= static_cast<int>(m_lfo_slots.size()))
            return;
        if (menu.getNumItems() > 0)
            menu.addSeparator();
        juce::PopupMenu::Item removeItem("Remove LFO");
        removeItem.setColour(m_lfo_slots[static_cast<size_t>(assignment)].accent);
        removeItem.setAction([this, knob]() { remove_lfo_from_knob(*knob); });
        menu.addItem(removeItem);
    });

    m_lfo_enabled_knobs.push_back(knob);
}

void MainComponent::assign_lfo_to_knob(int lfo_index, LayerCakeKnob& knob)
{
    if (lfo_index < 0 || lfo_index >= static_cast<int>(m_lfo_slots.size()))
    {
        DBG("MainComponent::assign_lfo_to_knob early return (invalid index)");
        return;
    }

    knob.set_lfo_assignment_index(lfo_index);
    knob.set_lfo_button_accent(m_lfo_slots[static_cast<size_t>(lfo_index)].accent);
    DBG("MainComponent::assign_lfo_to_knob parameter=" + knob.parameter_id()
        + " lfo=" + juce::String(lfo_index + 1));
    update_all_modulation_overlays();
}

void MainComponent::remove_lfo_from_knob(LayerCakeKnob& knob)
{
    if (!knob.has_lfo_assignment())
    {
        DBG("MainComponent::remove_lfo_from_knob early return (no assignment)");
        return;
    }

    knob.set_lfo_assignment_index(-1);
    knob.clear_modulation_indicator();
    knob.set_lfo_button_accent(std::nullopt);
    DBG("MainComponent::remove_lfo_from_knob parameter=" + knob.parameter_id());
}

void MainComponent::update_all_modulation_overlays()
{
    for (auto* knob : m_lfo_enabled_knobs)
    {
        if (knob == nullptr)
            continue;

        const int assignment = knob->lfo_assignment_index();
        if (assignment < 0 || assignment >= static_cast<int>(m_lfo_slots.size()))
        {
            knob->clear_modulation_indicator();
            continue;
        }

        const auto range = knob->slider().getRange();
        const double span = range.getLength();
        if (span <= 0.0)
        {
            DBG("MainComponent::update_all_modulation_overlays early return (invalid span)");
            continue;
        }

        const double effective_value = get_effective_knob_value(knob);
        const float normalized = static_cast<float>((effective_value - range.getStart()) / span);
        knob->set_modulation_indicator(normalized, m_lfo_slots[static_cast<size_t>(assignment)].accent);
    }
}

void MainComponent::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source != &m_device_manager)
    {
        DBG("MainComponent::changeListenerCallback early return (unexpected source)");
        return;
    }

    if (m_settings_window != nullptr)
    {
        if (auto* settings = dynamic_cast<SettingsComponent*>(m_settings_window->getContentComponent()))
        {
            settings->refresh_input_channel_selector();
        }
    }
}

double MainComponent::get_effective_knob_value(const LayerCakeKnob* knob) const
{
    if (knob == nullptr)
    {
        DBG("MainComponent::get_effective_knob_value early return (null knob)");
        return 0.0;
    }

    const double base_value = knob->slider().getValue();
    const int assignment = knob->lfo_assignment_index();
    if (assignment < 0 || assignment >= static_cast<int>(m_lfo_slots.size()))
        return base_value;

    const auto range = knob->slider().getRange();
    const double span = range.getLength();
    if (span <= 0.0)
    {
        DBG("MainComponent::get_effective_knob_value early return (non-positive span)");
        return base_value;
    }

    const double base_normalized = juce::jlimit(0.0, 1.0, (base_value - range.getStart()) / span);
    const double offset = static_cast<double>(m_lfo_last_values[static_cast<size_t>(assignment)].load(std::memory_order_relaxed));
    const double mod_normalized = juce::jlimit(0.0, 1.0, base_normalized + offset * 0.5);
    return range.getStart() + mod_normalized * span;
}

void MainComponent::update_record_layer_from_lfo()
{
    if (m_layer_select_knob == nullptr)
        return;

    const int assignment = m_layer_select_knob->lfo_assignment_index();
    if (assignment < 0)
        return;

    const double effective_value = get_effective_knob_value(m_layer_select_knob.get());
    const int desired_layer = juce::jlimit(0,
                                           static_cast<int>(LayerCakeEngine::kNumLayers) - 1,
                                           static_cast<int>(std::round(effective_value)) - 1);
    if (desired_layer != m_engine.get_record_layer())
        m_engine.set_record_layer(desired_layer);
}

void MainComponent::update_master_gain_from_knob()
{
    if (m_master_gain_knob == nullptr)
        return;

    const float gain = static_cast<float>(get_effective_knob_value(m_master_gain_knob.get()));
    m_engine.set_master_gain_db(gain);
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

void MainComponent::refresh_lfo_tempo_sync()
{
    for (auto& slot : m_lfo_slots)
    {
        if (slot.widget != nullptr)
            slot.widget->refresh_tempo_sync();
    }
}

} // namespace LayerCakeApp


