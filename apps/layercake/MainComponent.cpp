#include "MainComponent.h"
#include "LfoDragHelpers.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <flowerjuce/LayerCakeEngine/Metro.h>
#include <cmath>

namespace LayerCakeApp
{

//==============================================================================
// LfoTriggerButton implementation
//==============================================================================

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

//==============================================================================
// LfoConnectionOverlay implementation
//==============================================================================

void LfoConnectionOverlay::paint(juce::Graphics& g)
{
    if (m_targets.empty())
        return;

    // Draw dotted lines from source to each target
    const float dashLengths[] = { 4.0f, 4.0f };
    g.setColour(m_colour.withAlpha(0.7f));

    for (const auto& target : m_targets)
    {
        juce::Path path;
        path.startNewSubPath(m_source.toFloat());
        path.lineTo(target.toFloat());

        juce::PathStrokeType stroke(2.0f);
        stroke.createDashedStroke(path, path, dashLengths, 2);
        g.strokePath(path, juce::PathStrokeType(2.0f));
    }

    // Draw small circles at connection points
    const float circleRadius = 4.0f;
    g.setColour(m_colour);
    g.fillEllipse(m_source.x - circleRadius, m_source.y - circleRadius,
                  circleRadius * 2, circleRadius * 2);

    for (const auto& target : m_targets)
    {
        g.fillEllipse(target.x - circleRadius, target.y - circleRadius,
                      circleRadius * 2, circleRadius * 2);
    }
}

void LfoConnectionOverlay::set_source(juce::Point<int> source_center, juce::Colour colour)
{
    m_source = source_center;
    m_colour = colour;
}

void LfoConnectionOverlay::add_target(juce::Point<int> target_center)
{
    m_targets.push_back(target_center);
}

void LfoConnectionOverlay::clear()
{
    m_targets.clear();
    repaint();
}

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
const juce::Colour kKnobGray(0xff6a6a6a);  // Dark gray for all knobs

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
      m_record_button("rec"),
      m_clock_button("play"),
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

    // Vibrant, cheerful LFO color palette
    const std::array<juce::Colour, 4> lfoPalette = {
        juce::Colour(0xffff6b6b),  // Coral red
        juce::Colour(0xff4ecdc4),  // Turquoise
        juce::Colour(0xffffe66d),  // Sunny yellow
        juce::Colour(0xffff9ff3)   // Bubblegum pink
    };
    const std::array<juce::Colour, 4> secondaryLfoPalette = {
        juce::Colour(0xff54a0ff),  // Bright blue
        juce::Colour(0xff5f27cd),  // Purple
        juce::Colour(0xff00d2d3),  // Cyan
        juce::Colour(0xfff368e0)   // Magenta
    };

    for (size_t i = 0; i < m_lfo_slots.size(); ++i)
    {
        auto& slot = m_lfo_slots[i];
        const bool isSecondRow = i >= lfoPalette.size();
        slot.accent = isSecondRow
            ? secondaryLfoPalette[i % secondaryLfoPalette.size()].withAlpha(0.9f)
            : lfoPalette[i % lfoPalette.size()];
        slot.label = "LFO " + juce::String(static_cast<int>(i) + 1);
        // First LFO defaults to Gate, others to Sine
        slot.generator.set_mode(i == 0 ? flower::LfoWaveform::Gate : flower::LfoWaveform::Sine);
        slot.generator.set_rate_hz(0.35f + static_cast<float>(i) * 0.15f);
        slot.generator.set_depth(0.5f);
        slot.generator.reset_phase(static_cast<double>(i) / static_cast<double>(m_lfo_slots.size()));
        slot.generator.set_clock_division(1.0f); // Default 1 step per beat

        slot.widget = std::make_unique<LayerCakeLfoWidget>(static_cast<int>(i), slot.generator, slot.accent, &m_midi_learn_manager);
        slot.widget->set_drag_label(slot.label);
        slot.widget->set_on_settings_changed([this, index = static_cast<int>(i)]()
        {
            if (index < 0 || index >= static_cast<int>(m_lfo_slots.size()))
                return;

            auto* widget = m_lfo_slots[static_cast<size_t>(index)].widget.get();
            if (widget != nullptr)
                widget->refresh_wave_preview();
            update_all_modulation_overlays();
        });
        if (slot.widget != nullptr)
        {
            slot.widget->set_tempo_provider([this]() -> double
            {
                if (m_tempo_knob != nullptr)
                    return juce::jmax(10.0, get_effective_knob_value(m_tempo_knob.get()));
                return 120.0;
            });
            slot.widget->set_on_hover_changed([this, index = static_cast<int>(i)](bool hovered)
            {
                update_lfo_connection_overlay(index, hovered);
            });
            slot.widget->refresh_wave_preview();
            addAndMakeVisible(slot.widget.get());
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

    // Record labels are no longer displayed (removed from UI)
    m_record_layer_label.setVisible(false);
    m_record_status_label.setVisible(false);

    m_settings_button.setButtonText("settings");
    m_settings_button.setLookAndFeel(&m_settings_button_look_and_feel);
    m_settings_button.onClick = [this] { open_settings_window(); };
    addAndMakeVisible(m_settings_button);

    // CLI-style knobs for grain controls (LayerCakeKnob with cliMode=true)
    auto makeCliKnob = [this](LayerCakeKnob::Config config) {
        config.cliMode = true;  // Enable CLI rendering mode
        auto knob = std::make_unique<LayerCakeKnob>(config, &m_midi_learn_manager);
        register_knob_for_lfo(knob.get());
        knob->set_knob_colour(kKnobGray);
        addAndMakeVisible(knob.get());
        return knob;
    };

    auto bindManualKnob = [this](LayerCakeKnob* knob) {
        if (knob == nullptr) return;
        knob->slider().onValueChange = [this]() {
            sync_manual_state_from_controls();
        };
    };

    // Master gain knob
    m_master_gain_knob = makeCliKnob({ "gain", -24.0, 6.0, 0.0, 0.1, " dB", "layercake_master_gain", false, true, true, true, false, 1 });
    m_master_gain_knob->slider().onValueChange = [this]() {
        const float gain = static_cast<float>(get_effective_knob_value(m_master_gain_knob.get()));
        m_engine.set_master_gain_db(gain);
    };

    // Grain parameter knobs
    m_position_knob = makeCliKnob({ "pos", 0.0, 1.0, 0.5, 0.001, "", "layercake_position", false, true, true, true, true, 2 });
    bindManualKnob(m_position_knob.get());

    m_duration_knob = makeCliKnob({ "dur", 10.0, 5000.0, 300.0, 1.0, " ms", "layercake_duration", false, true, true, true, false, 0 });
    bindManualKnob(m_duration_knob.get());

    m_rate_knob = makeCliKnob({ "rate", -24.0, 24.0, 0.0, 0.1, " st", "layercake_rate", false, true, true, true, false, 1 });
    bindManualKnob(m_rate_knob.get());

    m_env_knob = makeCliKnob({ "env", 0.0, 1.0, 0.5, 0.01, "", "layercake_env", false, true, true, true, true, 2 });
    bindManualKnob(m_env_knob.get());

    m_direction_knob = makeCliKnob({ "dir", 0.0, 1.0, 0.5, 0.01, "", "layercake_direction", false, true, true, true, true, 2 });
    bindManualKnob(m_direction_knob.get());

    m_pan_knob = makeCliKnob({ "pan", 0.0, 1.0, 0.5, 0.01, "", "layercake_pan", false, true, true, true, true, 2 });
    bindManualKnob(m_pan_knob.get());

    m_layer_knob = makeCliKnob({ "layer", 1.0, static_cast<double>(LayerCakeEngine::kNumLayers), 1.0, 1.0, "", "layercake_layer_select", false, true, true, true, false, 0 });
    m_layer_knob->slider().onValueChange = [this]() {
        const double effective = get_effective_knob_value(m_layer_knob.get());
        const int raw = static_cast<int>(std::round(effective)) - 1;
        const int clamped = juce::jlimit(0, static_cast<int>(LayerCakeEngine::kNumLayers) - 1, raw);
        if (clamped != m_engine.get_record_layer())
        {
            m_engine.set_record_layer(clamped);
            update_record_labels();
        }
    };

    m_tempo_knob = makeCliKnob({ "bpm", 10.0, 600.0, 140.0, 0.1, "", "layercake_tempo", false, true, true, true, false, 1 });
    m_tempo_knob->slider().onValueChange = [this]() {
        if (!m_loading_knob_values)
        {
            const double bpm = get_effective_knob_value(m_tempo_knob.get());
            m_engine.set_bpm(static_cast<float>(bpm));
        }
    };

    // Collect all knobs that can have LFO assigned for iteration
    m_lfo_enabled_knobs = { 
        m_position_knob.get(), m_duration_knob.get(), m_rate_knob.get(), 
        m_env_knob.get(), m_direction_knob.get(), m_pan_knob.get(),
        m_layer_knob.get(), m_tempo_knob.get(), m_master_gain_knob.get()
    };

    m_master_meter.setColour(juce::ProgressBar::foregroundColourId,
                             m_custom_look_and_feel.findColour(juce::ProgressBar::foregroundColourId));
    m_master_meter.setColour(juce::ProgressBar::backgroundColourId,
                             m_custom_look_and_feel.findColour(juce::ProgressBar::backgroundColourId));
    m_master_meter.setLevels({ 0.0 });
    addAndMakeVisible(m_master_meter);

    configureControlButton(m_trigger_button.button(),
                           "trg",
                           LayerCakeLookAndFeel::ControlButtonType::Trigger,
                           false);
    m_trigger_button.button().onClick = [this]() { trigger_manual_grain(); };
    m_trigger_button.on_lfo_assigned = [this](int lfoIndex) {
        DBG("LFO " << lfoIndex << " assigned to trigger button");
    };
    m_trigger_button.on_lfo_cleared = [this]() {
        DBG("LFO cleared from trigger button");
    };
    addAndMakeVisible(m_trigger_button);

    configureControlButton(m_record_button,
                           "rec",
                           LayerCakeLookAndFeel::ControlButtonType::Record,
                           true);
    m_record_button.onClick = [this]() { toggle_record_enable(); };
    addAndMakeVisible(m_record_button);

    configureControlButton(m_clock_button,
                           "play",
                           LayerCakeLookAndFeel::ControlButtonType::Clock,
                           true);
    m_clock_button.setToggleState(true, juce::dontSendNotification);
    m_clock_button.setTooltip("Start/Stop Master Clock");
    m_clock_button.onClick = [this]() { handle_clock_button(); };
    addAndMakeVisible(m_clock_button);

    auto captureLayers = [this]() { return capture_layer_buffers(); };
    auto applyLayers = [this](const LayerBufferArray& buffers) { apply_layer_buffers(buffers); };
    auto captureKnobset = [this]() { return capture_knobset_data(); };
    auto applyKnobset = [this](const LayerCakePresetData& data) { apply_knobset(data); };

    // We pass dummy pattern functions since they are removed
    auto dummyCapturePattern = [this]() { return capture_knobset_data(); }; 
    auto dummyApplyPattern = [this](const LayerCakePresetData& d) { apply_knobset(d); };

    m_preset_panel = std::make_unique<LibraryBrowserComponent>(m_library_manager,
                                                               dummyCapturePattern,
                                                               captureLayers,
                                                               dummyApplyPattern,
                                                               applyLayers,
                                                               captureKnobset,
                                                               applyKnobset);
    if (m_preset_panel != nullptr)
    {
        m_preset_panel->setLookAndFeel(&m_custom_look_and_feel);
        m_preset_panel->setVisible(m_preset_panel_visible);
        addAndMakeVisible(m_preset_panel.get());
    }

    m_midi_learn_manager.setMidiInputEnabled(true);
    addAndMakeVisible(m_midi_learn_overlay);
    addKeyListener(&m_midi_learn_overlay);
    addAndMakeVisible(m_lfo_connection_overlay);
    m_lfo_connection_overlay.setAlwaysOnTop(true);

    auto appDataDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                        .getChildFile("LayerCake");
    appDataDir.createDirectory();
    m_midi_mappings_file = appDataDir.getChildFile("midi_mappings_layercake.xml");
    if (m_midi_mappings_file.existsAsFile())
        m_midi_learn_manager.loadMappings(m_midi_mappings_file);

    setSize(800, 720);
    configure_audio_device(std::move(initialDeviceSetup));
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
    
    // Init transport
    m_engine.set_transport_playing(true);
    m_engine.set_bpm(90.0f);
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
    g.setColour(background);
    g.fillRect(bounds);
    g.setColour(kSoftWhite.withAlpha(0.35f));
    g.drawRect(bounds, 1.5f);
}

void MainComponent::resized()
{
    const int marginOuter = 10;
    const int sectionSpacing = 12;
    const int rowSpacing = 8;
    const int titleHeight = 24;
    const int labelHeight = 12;
    const int buttonHeight = 22;
    const int meterWidth = 40;
    const int meterHeight = 120;
    const int meterSpacing = 12;
    const int displayPanelWidth = 620;
    const int displayWidth = 560;
    const int displayHeight = 280;
    const int presetPanelSpacing = 12;
    const int presetPanelMargin = 6;
    const int presetPanelWidthVisible = 210;
    const int lfoRowHeight = 140;
    const int lfoSpacing = 10;
    const int lfoMargin = 8;
    const int lfoSlotMinWidth = 100;
    const int lfoVerticalGap = 8;
    const int lfoRowSpacing = 8;
    const int lfosPerRow = 4;
    
    // CLI param row layout
    const int paramRowHeight = 16;
    const int paramRowSpacing = 4;
    const int paramColumnWidth = 120;
    const int paramColumnsPerRow = 3;

    auto bounds = getLocalBounds().reduced(marginOuter);

    // Preset panel on the far right (vertical column layout)
    if (m_preset_panel != nullptr)
    {
        if (m_preset_panel_visible)
        {
            auto presetArea = bounds.removeFromRight(presetPanelWidthVisible);
            bounds.removeFromRight(presetPanelSpacing);
            m_preset_panel->setBounds(presetArea.reduced(presetPanelMargin));
        }
        else
        {
            m_preset_panel->setBounds({});
        }
    }

    // Meter on the right
    auto meterSlice = bounds.removeFromRight(meterWidth);
    bounds.removeFromRight(meterSpacing);
    auto meterArea = meterSlice;
    if (meterArea.getHeight() > meterHeight)
        meterArea = meterArea.withHeight(meterHeight).withY(meterSlice.getBottom() - meterHeight);
    m_master_meter.setBounds(meterArea);

    // Calculate LFO area height
    const int lfoCount = static_cast<int>(m_lfo_slots.size());
    const int lfoRows = lfoCount > 0 ? juce::jmax(1, (lfoCount + lfosPerRow - 1) / lfosPerRow) : 0;
    const int lfoAreaHeight = lfoRows > 0 ? (lfoRows * lfoRowHeight + (lfoRows - 1) * lfoRowSpacing) : 0;

    // Main display column
    auto displayColumn = bounds.removeFromLeft(displayPanelWidth);
    
    // LFOs at the bottom
    auto lfoArea = displayColumn.removeFromBottom(lfoAreaHeight);
    displayColumn.removeFromBottom(lfoVerticalGap);
    
    // CLI param rows between display and LFOs
    const int numParamRows = 3;  // 3 rows of params
    const int paramAreaHeight = numParamRows * paramRowHeight + (numParamRows - 1) * paramRowSpacing + rowSpacing;
    auto paramArea = displayColumn.removeFromBottom(paramAreaHeight);
    displayColumn.removeFromBottom(rowSpacing);
    
    // Title area
    auto titleArea = displayColumn.removeFromTop(titleHeight);
    m_title_label.setBounds(titleArea.removeFromLeft(displayPanelWidth - 100));
    m_settings_button.setBounds(titleArea.reduced(4));
    displayColumn.removeFromTop(rowSpacing);
    
    // Display
    auto tvArea = displayColumn.withSizeKeepingCentre(displayWidth, juce::jmin(displayHeight, displayColumn.getHeight()));
    m_display.setBounds(tvArea);

    // Layout CLI param rows in a grid (3 columns x 3 rows)
    // Row 1: bpm, gain, layer
    // Row 2: pos, dur, rate
    // Row 3: env, dir, pan + buttons
    auto paramWalker = paramArea;
    
    auto layoutParamRow = [&](std::initializer_list<LayerCakeKnob*> knobs) {
        auto rowArea = paramWalker.removeFromTop(paramRowHeight);
        for (auto* knob : knobs)
        {
            if (knob != nullptr)
                knob->setBounds(rowArea.removeFromLeft(paramColumnWidth));
        }
        paramWalker.removeFromTop(paramRowSpacing);
    };
    
    layoutParamRow({ m_tempo_knob.get(), m_master_gain_knob.get(), m_layer_knob.get() });
    layoutParamRow({ m_position_knob.get(), m_duration_knob.get(), m_rate_knob.get() });
    
    // Third row: env, dir, pan + buttons
    auto row3Area = paramWalker.removeFromTop(paramRowHeight);
    m_env_knob->setBounds(row3Area.removeFromLeft(paramColumnWidth));
    m_direction_knob->setBounds(row3Area.removeFromLeft(paramColumnWidth));
    m_pan_knob->setBounds(row3Area.removeFromLeft(paramColumnWidth));
    
    // Buttons after the params
    row3Area.removeFromLeft(sectionSpacing);
    const int buttonWidth = 50;
    m_clock_button.setBounds(row3Area.removeFromLeft(buttonWidth));
    row3Area.removeFromLeft(4);
    m_record_button.setBounds(row3Area.removeFromLeft(buttonWidth));
    row3Area.removeFromLeft(4);
    m_trigger_button.setBounds(row3Area.removeFromLeft(buttonWidth));

    // LFO Layout
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
    m_lfo_connection_overlay.setBounds(getLocalBounds());
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
                if (type == nullptr) continue;

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
        }
        m_device_manager.setAudioDeviceSetup(*initialSetup, true);
    }

    m_device_manager.addAudioCallback(this);
}

void MainComponent::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    if (device == nullptr) return;

    const double sample_rate = device->getCurrentSampleRate() > 0 ? device->getCurrentSampleRate() : kDefaultSampleRate;
    const int block_size = device->getCurrentBufferSizeSamples() > 0 ? device->getCurrentBufferSizeSamples() : kDefaultBlockSize;
    const int outputs = device->getActiveOutputChannels().countNumberOfSetBits();

    m_engine.prepare(sample_rate, block_size, juce::jmax(1, outputs));
    m_device_ready = true;
    const int meter_channels = juce::jmax(1, juce::jmin(MultiChannelMeter::kMaxChannels, outputs));
    m_meter_channel_count.store(meter_channels, std::memory_order_relaxed);
    for (auto& meter_level : m_meter_levels)
        meter_level.store(0.0f, std::memory_order_relaxed);
    DBG("Audio device started sampleRate=" + juce::String(sample_rate));
}

void MainComponent::audioDeviceStopped()
{
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
        for (int channel = 0; channel < numOutputChannels; ++channel)
            if (outputChannelData[channel] != nullptr)
                juce::FloatVectorOperations::clear(outputChannelData[channel], numSamples);
        return;
    }

    m_engine.process_block(inputChannelData, numInputChannels, outputChannelData, numOutputChannels, numSamples);

    const int meter_channels = juce::jmax(1, juce::jmin(MultiChannelMeter::kMaxChannels, numOutputChannels));
    for (int channel = 0; channel < meter_channels; ++channel)
    {
        float peak = 0.0f;
        if (channel < numOutputChannels && outputChannelData[channel] != nullptr)
        {
            const float* channel_data = outputChannelData[channel];
            for (int sample = 0; sample < numSamples; ++sample)
                peak = juce::jmax(peak, std::abs(channel_data[sample]));
        }
        m_meter_levels[static_cast<size_t>(channel)].store(peak, std::memory_order_relaxed);
    }
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
    if (key == juce::KeyPress::spaceKey)
    {
        handle_clock_button();
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
    m_display.set_record_layer(m_engine.get_record_layer());
    
    // Update LFO LED indicators
    for (size_t i = 0; i < m_lfo_slots.size(); ++i)
    {
        if (m_lfo_slots[i].widget != nullptr)
        {
            // Map from -1..1 (bipolar) to 0..1 for LED display
            // This makes gate/square show on/off properly
            const float rawValue = m_lfo_last_values[i].load(std::memory_order_relaxed);
            const float ledValue = (rawValue + 1.0f) * 0.5f;  // -1..1 -> 0..1
            m_lfo_slots[i].widget->set_current_value(juce::jlimit(0.0f, 1.0f, ledValue));
        }
    }
    
    // Transport status check
    bool running = m_engine.is_transport_playing();
    if (m_clock_button.getToggleState() != running)
        m_clock_button.setToggleState(running, juce::dontSendNotification);
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
    const double normalized_start = (m_position_knob != nullptr)
        ? juce::jlimit(0.0, 1.0, get_effective_knob_value(m_position_knob.get()))
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

    m_engine.apply_direction_randomization(state, reverse_probability);
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
    if (m_layer_knob != nullptr && !m_layer_knob->has_lfo_assignment())
        m_layer_knob->slider().setValue(static_cast<double>(layer_index + 1), juce::sendNotificationSync);
    sync_manual_state_from_controls();
}

void MainComponent::update_meter()
{
    const int channel_count = juce::jlimit(1, MultiChannelMeter::kMaxChannels, m_meter_channel_count.load(std::memory_order_relaxed));
    std::vector<double> levels;
    levels.reserve(channel_count);
    for (int i = 0; i < channel_count; ++i)
        levels.push_back(juce::jlimit(0.0, 1.0, static_cast<double>(m_meter_levels[static_cast<size_t>(i)].load(std::memory_order_relaxed))));
    m_master_meter.setLevels(levels);
}

void MainComponent::handle_clock_button()
{
    bool shouldPlay = !m_engine.is_transport_playing();
    m_engine.set_transport_playing(shouldPlay);
    if (shouldPlay)
    {
        // Optionally reset transport on start
        // m_engine.reset_transport(); 
    }
    m_clock_button.setToggleState(shouldPlay, juce::dontSendNotification);
}

void MainComponent::open_library_window()
{
    if (m_preset_panel == nullptr) return;
    m_preset_panel_visible = !m_preset_panel_visible;
    m_preset_panel->setVisible(m_preset_panel_visible);
    resized();
}

LayerCakePresetData MainComponent::capture_knobset_data() const
{
    LayerCakePresetData data;
    data.master_gain_db = m_master_gain_knob != nullptr ? static_cast<float>(m_master_gain_knob->slider().getValue()) : 0.0f;
    data.clock_enabled = m_clock_button.getToggleState();
    data.manual_state = m_manual_state;
    data.manual_state.should_trigger = false;
    data.record_layer = m_engine.get_record_layer();
    data.reverse_probability = m_direction_knob != nullptr ? static_cast<float>(m_direction_knob->slider().getValue()) : 0.0f;

    auto capture = [&](LayerCakeKnob* knob)
    {
        if (knob == nullptr) return;
        const auto& parameterId = knob->parameter_id();
        if (parameterId.isEmpty()) return;
        data.knob_values.set(juce::Identifier(parameterId), knob->slider().getValue());
    };

    capture(m_master_gain_knob.get());
    capture(m_position_knob.get());
    capture(m_duration_knob.get());
    capture(m_rate_knob.get());
    capture(m_env_knob.get());
    capture(m_direction_knob.get());
    capture(m_pan_knob.get());
    capture(m_layer_knob.get());
    capture(m_tempo_knob.get());

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
        
        // Basic parameters
        slotData.mode = static_cast<int>(slot.generator.get_mode());
        slotData.rate_hz = slot.generator.get_rate_hz();
        slotData.depth = slot.generator.get_depth();
        slotData.tempo_sync = true; // LFOs are always clock-driven
        slotData.clock_division = slot.generator.get_clock_division();
        slotData.pattern_length = slot.generator.get_pattern_length();
        slotData.pattern_buffer = slot.generator.get_pattern_buffer();
        
        // PNW-style waveform shaping
        slotData.level = slot.generator.get_level();
        slotData.width = slot.generator.get_width();
        slotData.phase_offset = slot.generator.get_phase_offset();
        slotData.delay = slot.generator.get_delay();
        slotData.delay_div = slot.generator.get_delay_div();
        
        // Humanization
        slotData.slop = slot.generator.get_slop();
        
        // Euclidean rhythm
        slotData.euclidean_steps = slot.generator.get_euclidean_steps();
        slotData.euclidean_triggers = slot.generator.get_euclidean_triggers();
        slotData.euclidean_rotation = slot.generator.get_euclidean_rotation();
        
        // Random skip
        slotData.random_skip = slot.generator.get_random_skip();
        
        // Loop
        slotData.loop_beats = slot.generator.get_loop_beats();
        
        // Random seed
        slotData.random_seed = slot.generator.get_random_seed();
    }

    data.lfo_assignments.clear();
    for (auto* knob : m_lfo_enabled_knobs)
    {
        if (knob == nullptr) continue;
        const int assignment = knob->lfo_assignment_index();
        if (assignment < 0) continue;
        const auto& parameterId = knob->parameter_id();
        if (parameterId.isEmpty()) continue;
        data.lfo_assignments.set(juce::Identifier(parameterId), assignment);
    }
}

LayerBufferArray MainComponent::capture_layer_buffers() const
{
    LayerBufferArray buffers{};
    m_engine.capture_all_layer_snapshots(buffers);
    return buffers;
}

void MainComponent::apply_knobset(const LayerCakePresetData& data)
{
    const juce::ScopedValueSetter<bool> knob_guard(m_loading_knob_values, true);
    auto applyValue = [&](LayerCakeKnob* knob)
    {
        if (knob == nullptr) return;
        const auto& parameterId = knob->parameter_id();
        if (parameterId.isEmpty()) return;
        const auto identifier = juce::Identifier(parameterId);
        if (identifier.isNull()) return;
        if (const juce::var* value = data.knob_values.getVarPointer(identifier))
        {
            knob->slider().setValue(static_cast<double>(*value), juce::sendNotificationSync);
        }
    };

    applyValue(m_master_gain_knob.get());
    applyValue(m_position_knob.get());
    applyValue(m_duration_knob.get());
    applyValue(m_rate_knob.get());
    applyValue(m_env_knob.get());
    applyValue(m_direction_knob.get());
    applyValue(m_pan_knob.get());
    applyValue(m_layer_knob.get());
    applyValue(m_tempo_knob.get());

    apply_lfo_state(data);

    m_clock_button.setToggleState(data.clock_enabled, juce::dontSendNotification);
}

void MainComponent::apply_lfo_state(const LayerCakePresetData& data)
{
    const size_t slotCount = juce::jmin(m_lfo_slots.size(), data.lfo_slots.size());
    const int maxMode = static_cast<int>(flower::LfoWaveform::SmoothRandom);

    for (size_t i = 0; i < slotCount; ++i)
    {
        auto& slot = m_lfo_slots[i];
        const auto& slotData = data.lfo_slots[i];
        
        // Basic parameters
        const int modeIndex = juce::jlimit(0, maxMode, slotData.mode);
        slot.generator.set_mode(static_cast<flower::LfoWaveform>(modeIndex));
        slot.generator.set_rate_hz(juce::jlimit(0.01f, 20.0f, slotData.rate_hz));
        slot.generator.set_depth(juce::jlimit(0.0f, 1.0f, slotData.depth));
        slot.generator.set_clock_division(slotData.clock_division);
        slot.generator.set_pattern_length(slotData.pattern_length);
        slot.generator.set_pattern_buffer(slotData.pattern_buffer);
        
        // PNW-style waveform shaping
        slot.generator.set_level(juce::jlimit(0.0f, 1.0f, slotData.level));
        slot.generator.set_width(juce::jlimit(0.0f, 1.0f, slotData.width));
        slot.generator.set_phase_offset(juce::jlimit(0.0f, 1.0f, slotData.phase_offset));
        slot.generator.set_delay(juce::jlimit(0.0f, 1.0f, slotData.delay));
        slot.generator.set_delay_div(juce::jmax(1, slotData.delay_div));
        
        // Humanization
        slot.generator.set_slop(juce::jlimit(0.0f, 1.0f, slotData.slop));
        
        // Euclidean rhythm
        slot.generator.set_euclidean_steps(juce::jmax(0, slotData.euclidean_steps));
        slot.generator.set_euclidean_triggers(juce::jmax(0, slotData.euclidean_triggers));
        slot.generator.set_euclidean_rotation(juce::jmax(0, slotData.euclidean_rotation));
        
        // Random skip
        slot.generator.set_random_skip(juce::jlimit(0.0f, 1.0f, slotData.random_skip));
        
        // Loop
        slot.generator.set_loop_beats(juce::jmax(0, slotData.loop_beats));
        
        // Random seed (restore for reproducible patterns)
        if (slotData.random_seed != 0)
            slot.generator.set_random_seed(slotData.random_seed);
        
        slot.generator.reset_phase();
        m_lfo_last_values[i].store(slot.generator.get_last_value(), std::memory_order_relaxed);
        
        if (slot.widget != nullptr)
        {
            slot.widget->sync_controls_from_generator();
        }
        // LFOs are always clock-driven
    }

    for (auto* knob : m_lfo_enabled_knobs)
    {
        if (knob == nullptr) continue;

        knob->set_lfo_assignment_index(-1);
        knob->clear_modulation_indicator();

        const auto& parameterId = knob->parameter_id();
        if (parameterId.isEmpty()) continue;

        const auto identifier = juce::Identifier(parameterId);
        if (const juce::var* value = data.lfo_assignments.getVarPointer(identifier))
        {
            const int index = static_cast<int>(*value);
            if (index >= 0 && index < static_cast<int>(m_lfo_slots.size()))
            {
                assign_lfo_to_knob(index, *knob);
            }
        }
    }

    update_all_modulation_overlays();
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
    const double loop_start_normalized = (m_position_knob != nullptr)
        ? juce::jlimit(0.0, 1.0, get_effective_knob_value(m_position_knob.get()))
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
}

void MainComponent::advance_lfos(double now_ms)
{
    juce::ignoreUnused(now_ms);
    const double master_beats = m_engine.get_master_beats();
    
    const int triggerLfoIndex = m_trigger_button.get_lfo_assignment();
    
    for (size_t i = 0; i < m_lfo_slots.size(); ++i)
    {
        auto& slot = m_lfo_slots[i];
        // LFOs are always clock-driven
        const float rawValue = slot.generator.advance_clocked(master_beats);
        const float scaled = rawValue * slot.generator.get_depth();
        
        // Check for positive zero-crossing to trigger grains
        if (static_cast<int>(i) == triggerLfoIndex)
        {
            const float prevValue = m_lfo_prev_values[i];
            // Trigger on rising edge crossing 0.0 (from negative/zero to positive)
            if (prevValue <= 0.0f && scaled > 0.0f)
            {
                trigger_manual_grain();
            }
        }
        
        m_lfo_prev_values[i] = scaled;
        m_lfo_last_values[i].store(scaled, std::memory_order_relaxed);
    }
}

void MainComponent::register_knob_for_lfo(LayerCakeKnob* knob)
{
    if (knob == nullptr) return;

    knob->set_lfo_drop_handler([this](LayerCakeKnob& target, int lfoIndex) {
        assign_lfo_to_knob(lfoIndex, target);
    });
    knob->set_lfo_release_handler([this, knob]() {
        remove_lfo_from_knob(*knob);
    });
}

void MainComponent::assign_lfo_to_knob(int lfo_index, LayerCakeKnob& knob)
{
    if (lfo_index < 0 || lfo_index >= static_cast<int>(m_lfo_slots.size())) return;
    knob.set_lfo_assignment_index(lfo_index);
    knob.set_lfo_button_accent(m_lfo_slots[static_cast<size_t>(lfo_index)].accent);
    update_all_modulation_overlays();
}

void MainComponent::remove_lfo_from_knob(LayerCakeKnob& knob)
{
    if (!knob.has_lfo_assignment()) return;
    knob.set_lfo_assignment_index(-1);
    knob.clear_modulation_indicator();
}

void MainComponent::update_all_modulation_overlays()
{
    for (auto* knob : m_lfo_enabled_knobs)
    {
        if (knob == nullptr) continue;

        const int assignment = knob->lfo_assignment_index();
        if (assignment < 0 || assignment >= static_cast<int>(m_lfo_slots.size()))
        {
            knob->clear_modulation_indicator();
            continue;
        }

        const float lfo_value = m_lfo_last_values[static_cast<size_t>(assignment)].load(std::memory_order_relaxed);
        const juce::Colour lfoColour = m_lfo_slots[static_cast<size_t>(assignment)].accent;
        // Normalize to 0-1 range for modulation indicator
        const float normalized = (lfo_value + 1.0f) * 0.5f;
        knob->set_modulation_indicator(normalized, lfoColour);
    }
}

void MainComponent::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source != &m_device_manager) return;
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
    if (knob == nullptr) return 0.0;
    const double base_value = knob->slider().getValue();
    const int assignment = knob->lfo_assignment_index();
    if (assignment < 0 || assignment >= static_cast<int>(m_lfo_slots.size()))
        return base_value;

    const auto& config = knob->config();
    const double span = config.maxValue - config.minValue;
    if (span <= 0.0) return base_value;

    const double base_normalized = juce::jlimit(0.0, 1.0, (base_value - config.minValue) / span);
    const double offset = static_cast<double>(m_lfo_last_values[static_cast<size_t>(assignment)].load(std::memory_order_relaxed));
    const double mod_normalized = juce::jlimit(0.0, 1.0, base_normalized + offset * 0.5);
    return config.minValue + mod_normalized * span;
}

void MainComponent::update_record_layer_from_lfo()
{
    if (m_layer_knob == nullptr) return;
    const int assignment = m_layer_knob->lfo_assignment_index();
    if (assignment < 0) return;

    const double effective_value = get_effective_knob_value(m_layer_knob.get());
    const int desired_layer = juce::jlimit(0,
                                           static_cast<int>(LayerCakeEngine::kNumLayers) - 1,
                                           static_cast<int>(std::round(effective_value)) - 1);
    if (desired_layer != m_engine.get_record_layer())
        m_engine.set_record_layer(desired_layer);
}

void MainComponent::update_master_gain_from_knob()
{
    if (m_master_gain_knob == nullptr) return;
    const float gain = static_cast<float>(get_effective_knob_value(m_master_gain_knob.get()));
    m_engine.set_master_gain_db(gain);
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

void MainComponent::update_lfo_connection_overlay(int lfo_index, bool hovered)
{
    m_lfo_connection_overlay.clear();

    if (!hovered || lfo_index < 0 || lfo_index >= static_cast<int>(m_lfo_slots.size()))
    {
        m_hovered_lfo_index = -1;
        return;
    }

    m_hovered_lfo_index = lfo_index;

    // Get the LFO widget center in MainComponent coordinates
    auto* widget = m_lfo_slots[static_cast<size_t>(lfo_index)].widget.get();
    if (widget == nullptr)
        return;

    auto widgetBounds = widget->getBoundsInParent();
    auto sourceCenter = widgetBounds.getCentre();
    auto lfoColour = m_lfo_slots[static_cast<size_t>(lfo_index)].accent;

    m_lfo_connection_overlay.set_source(sourceCenter, lfoColour);

    // Find all knobs assigned to this LFO
    for (auto* knob : m_lfo_enabled_knobs)
    {
        if (knob == nullptr)
            continue;

        const int assignment = knob->lfo_assignment_index();
        if (assignment == lfo_index)
        {
            // Get knob center in MainComponent coordinates
            auto knobCenter = knob->getBounds().getCentre();
            // Convert through parent hierarchy to MainComponent coordinates
            auto* parent = knob->getParentComponent();
            while (parent != nullptr && parent != this)
            {
                knobCenter.x += parent->getX();
                knobCenter.y += parent->getY();
                parent = parent->getParentComponent();
            }
            m_lfo_connection_overlay.add_target(knobCenter);
        }
    }

    // Also check the trigger button
    if (m_trigger_button.get_lfo_assignment() == lfo_index)
    {
        auto trigBounds = m_trigger_button.getBoundsInParent();
        m_lfo_connection_overlay.add_target(trigBounds.getCentre());
    }

    m_lfo_connection_overlay.repaint();
}

} // namespace LayerCakeApp
