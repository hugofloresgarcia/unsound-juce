#include "LayerCakeLfoWidget.h"
#include "LfoDragHelpers.h"
#include <cmath>
#include <array>

namespace LayerCakeApp
{

namespace
{
constexpr int kPreviewSamples = 128;

flower::LfoWaveform waveform_from_index(int index)
{
    switch (index)
    {
        case 1: return flower::LfoWaveform::Triangle;
        case 2: return flower::LfoWaveform::Square;
        case 3: return flower::LfoWaveform::Gate;
        case 4: return flower::LfoWaveform::Envelope;
        case 5: return flower::LfoWaveform::Random;
        case 6: return flower::LfoWaveform::SmoothRandom;
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
        case flower::LfoWaveform::Gate: return 3;
        case flower::LfoWaveform::Envelope: return 4;
        case flower::LfoWaveform::Random: return 5;
        case flower::LfoWaveform::SmoothRandom: return 6;
        case flower::LfoWaveform::Sine:
        default: return 0;
    }
}
} // namespace

//==============================================================================
// LfoParamRow implementation
//==============================================================================

LfoParamRow::LfoParamRow(const Config& config, Shared::MidiLearnManager* midiManager)
    : m_config(config),
      m_midi_manager(midiManager),
      m_value(config.defaultValue)
{
    setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
    register_midi_parameter();
}

LfoParamRow::~LfoParamRow()
{
    if (m_midi_manager != nullptr && m_registered_parameter_id.isNotEmpty())
        m_midi_manager->unregisterParameter(m_registered_parameter_id);
}

void LfoParamRow::paint(juce::Graphics& g)
{
    // Don't paint if text editor is visible
    if (m_is_editing) return;
    
    auto bounds = getLocalBounds().toFloat();
    
    // Monospace font for CLI aesthetic
    juce::FontOptions fontOpts;
    fontOpts = fontOpts.withName(juce::Font::getDefaultMonospacedFontName())
                       .withHeight(13.0f);
    juce::Font monoFont(fontOpts);
    g.setFont(monoFont);
    
    // Highlight if MIDI learning this parameter
    if (m_midi_manager != nullptr && 
        m_midi_manager->isLearning() && 
        m_midi_manager->getLearningParameterId() == m_config.parameterId)
    {
        g.setColour(m_accent.withAlpha(0.3f));
        g.fillRoundedRectangle(bounds, 2.0f);
    }
    
    // Key in accent color
    g.setColour(m_accent);
    const juce::String keyText = m_config.key + ":";
    const float keyWidth = 48.0f;  // Fixed width for alignment
    g.drawText(keyText, bounds.removeFromLeft(keyWidth), juce::Justification::centredLeft, false);
    
    // Value in white/light gray
    g.setColour(m_is_dragging ? m_accent.brighter(0.3f) : juce::Colours::white.withAlpha(0.9f));
    g.drawText(format_value(), bounds, juce::Justification::centredLeft, false);
    
    // Show MIDI CC indicator if mapped
    if (m_midi_manager != nullptr && m_config.parameterId.isNotEmpty())
    {
        const int cc = m_midi_manager->getMappingForParameter(m_config.parameterId);
        if (cc >= 0)
        {
            g.setColour(m_accent.withAlpha(0.5f));
            g.setFont(monoFont.withHeight(10.0f));
            const juce::String ccText = "CC" + juce::String(cc);
            g.drawText(ccText, getLocalBounds().toFloat().removeFromRight(28.0f), 
                       juce::Justification::centredRight, false);
        }
    }
}

void LfoParamRow::resized()
{
    if (m_text_editor != nullptr)
        m_text_editor->setBounds(getLocalBounds());
}

void LfoParamRow::mouseDown(const juce::MouseEvent& event)
{
    if (m_is_editing) return;
    
    if (event.mods.isRightButtonDown() || event.mods.isPopupMenu())
    {
        if (show_context_menu(event))
            return;
    }
    
    m_drag_start_value = m_value;
    m_drag_start_y = event.y;
    m_is_dragging = true;
    repaint();
}

void LfoParamRow::mouseDrag(const juce::MouseEvent& event)
{
    if (!m_is_dragging || m_is_editing) return;
    
    const int deltaY = m_drag_start_y - event.y;  // Up = positive
    const double range = m_config.maxValue - m_config.minValue;
    
    // Sensitivity: full range over ~200 pixels, shift for fine control
    double sensitivity = range / 200.0;
    if (event.mods.isShiftDown())
        sensitivity *= 0.1;
    
    double newValue = m_drag_start_value + deltaY * sensitivity;
    
    // Snap to interval
    if (m_config.interval > 0.0)
        newValue = std::round(newValue / m_config.interval) * m_config.interval;
    
    set_value(newValue);
}

void LfoParamRow::mouseUp(const juce::MouseEvent& event)
{
    juce::ignoreUnused(event);
    m_is_dragging = false;
    repaint();
}

void LfoParamRow::mouseDoubleClick(const juce::MouseEvent& event)
{
    juce::ignoreUnused(event);
    show_text_editor();
}

void LfoParamRow::set_value(double value, bool notify)
{
    value = juce::jlimit(m_config.minValue, m_config.maxValue, value);
    if (std::abs(value - m_value) < 1e-9) return;
    
    m_value = value;
    repaint();
    
    if (notify && m_on_value_changed)
        m_on_value_changed();
}

bool LfoParamRow::is_percent_display() const
{
    // Display as 0-99 if range is 0-1 and displayAsPercent is true
    return m_config.displayAsPercent && 
           std::abs(m_config.minValue) < 0.001 && 
           std::abs(m_config.maxValue - 1.0) < 0.001;
}

juce::String LfoParamRow::format_value() const
{
    juce::String result;
    
    if (is_percent_display())
    {
        // Display 0-1 as 0-99
        const int displayValue = static_cast<int>(std::round(m_value * 99.0));
        result = juce::String(displayValue);
    }
    else if (m_config.decimals == 0)
    {
        result = juce::String(static_cast<int>(std::round(m_value)));
    }
    else
    {
        result = juce::String(m_value, m_config.decimals);
    }
    
    if (m_config.suffix.isNotEmpty())
        result += m_config.suffix;
    
    return result;
}

void LfoParamRow::show_text_editor()
{
    if (m_is_editing) return;
    
    m_is_editing = true;
    m_text_editor = std::make_unique<juce::TextEditor>();
    m_text_editor->setMultiLine(false);
    m_text_editor->setReturnKeyStartsNewLine(false);
    m_text_editor->setScrollbarsShown(false);
    m_text_editor->setCaretVisible(true);
    m_text_editor->setPopupMenuEnabled(false);
    
    // Style to match CLI aesthetic
    juce::FontOptions fontOpts;
    fontOpts = fontOpts.withName(juce::Font::getDefaultMonospacedFontName())
                       .withHeight(13.0f);
    m_text_editor->setFont(juce::Font(fontOpts));
    m_text_editor->setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff2a2a2a));
    m_text_editor->setColour(juce::TextEditor::textColourId, juce::Colours::white);
    m_text_editor->setColour(juce::TextEditor::highlightColourId, m_accent.withAlpha(0.4f));
    m_text_editor->setColour(juce::TextEditor::outlineColourId, m_accent);
    m_text_editor->setColour(juce::TextEditor::focusedOutlineColourId, m_accent);
    
    // Set initial text (without suffix)
    juce::String initialText;
    if (is_percent_display())
        initialText = juce::String(static_cast<int>(std::round(m_value * 99.0)));
    else if (m_config.decimals == 0)
        initialText = juce::String(static_cast<int>(std::round(m_value)));
    else
        initialText = juce::String(m_value, m_config.decimals);
    
    m_text_editor->setText(initialText, false);
    m_text_editor->selectAll();
    m_text_editor->addListener(this);
    
    addAndMakeVisible(m_text_editor.get());
    m_text_editor->setBounds(getLocalBounds());
    m_text_editor->grabKeyboardFocus();
    
    repaint();
}

void LfoParamRow::hide_text_editor(bool apply)
{
    if (!m_is_editing || m_text_editor == nullptr) return;
    
    if (apply)
    {
        const double newValue = parse_input(m_text_editor->getText());
        set_value(newValue);
    }
    
    m_text_editor->removeListener(this);
    removeChildComponent(m_text_editor.get());
    m_text_editor.reset();
    m_is_editing = false;
    
    repaint();
}

void LfoParamRow::textEditorReturnKeyPressed(juce::TextEditor& editor)
{
    juce::ignoreUnused(editor);
    hide_text_editor(true);
}

void LfoParamRow::textEditorEscapeKeyPressed(juce::TextEditor& editor)
{
    juce::ignoreUnused(editor);
    hide_text_editor(false);
}

void LfoParamRow::textEditorFocusLost(juce::TextEditor& editor)
{
    juce::ignoreUnused(editor);
    hide_text_editor(true);
}

double LfoParamRow::parse_input(const juce::String& text) const
{
    const double inputValue = text.getDoubleValue();
    
    if (is_percent_display())
    {
        // Convert 0-99 input back to 0-1
        return juce::jlimit(0.0, 1.0, inputValue / 99.0);
    }
    
    return inputValue;
}

void LfoParamRow::register_midi_parameter()
{
    if (m_midi_manager == nullptr || m_config.parameterId.isEmpty())
    {
        DBG("LfoParamRow::register_midi_parameter early return (missing midi manager or parameter id)");
        return;
    }

    m_registered_parameter_id = m_config.parameterId;

    const double minValue = m_config.minValue;
    const double maxValue = m_config.maxValue;

    m_midi_manager->registerParameter({
        m_config.parameterId,
        [this, minValue, maxValue](float normalized) {
            const double value = minValue + normalized * (maxValue - minValue);
            set_value(value, true);
        },
        [this, minValue, maxValue]() {
            return static_cast<float>((m_value - minValue) / (maxValue - minValue));
        },
        m_config.key,
        false
    });
}

bool LfoParamRow::show_context_menu(const juce::MouseEvent& event)
{
    juce::PopupMenu menu;
    
    if (m_midi_manager != nullptr && m_config.parameterId.isNotEmpty())
    {
        const int currentCc = m_midi_manager->getMappingForParameter(m_config.parameterId);
        juce::String learnLabel = "MIDI Learn...";
        if (currentCc >= 0)
            learnLabel += " (Currently CC " + juce::String(currentCc) + ")";

        menu.addItem(juce::PopupMenu::Item(learnLabel)
                         .setAction([this]() {
                             if (m_midi_manager == nullptr)
                             {
                                 DBG("LfoParamRow::show_context_menu midi learn action early return (no midi manager)");
                                 return;
                             }
                             m_midi_manager->startLearning(m_config.parameterId);
                             if (auto* topLevel = getTopLevelComponent())
                                 topLevel->repaint();
                         }));

        if (currentCc >= 0)
        {
            menu.addItem(juce::PopupMenu::Item("Clear MIDI Mapping")
                             .setAction([this]() {
                                 if (m_midi_manager == nullptr)
                                 {
                                     DBG("LfoParamRow::show_context_menu clear midi action early return (no midi manager)");
                                     return;
                                 }
                                 m_midi_manager->clearMapping(m_config.parameterId);
                                 repaint();
                                 if (auto* topLevel = getTopLevelComponent())
                                     topLevel->repaint();
                             }));
        }
    }

    // Reset to default option
    menu.addSeparator();
    menu.addItem(juce::PopupMenu::Item("Reset to Default")
                     .setAction([this]() {
                         set_value(m_config.defaultValue);
                     }));

    if (menu.getNumItems() == 0)
    {
        DBG("LfoParamRow::show_context_menu early return (no items to show)");
        return false;
    }

    juce::Rectangle<int> screenArea(event.getScreenX(), event.getScreenY(), 1, 1);
    menu.showMenuAsync(juce::PopupMenu::Options()
                           .withTargetScreenArea(screenArea)
                           .withMinimumWidth(150));
    return true;
}

//==============================================================================
// LayerCakeLfoWidget implementation
//==============================================================================

LayerCakeLfoWidget::LayerCakeLfoWidget(int lfo_index,
                                       flower::LayerCakeLfoUGen& generator,
                                       juce::Colour accent,
                                       Shared::MidiLearnManager* midiManager)
    : m_generator(generator),
      m_midi_manager(midiManager),
      m_accent_colour(accent),
      m_lfo_index(lfo_index)
{
    m_drag_label = "LFO " + juce::String(lfo_index + 1);

    m_title_label.setText(m_drag_label, juce::dontSendNotification);
    m_title_label.setJustificationType(juce::Justification::centredLeft);
    juce::FontOptions titleOpts;
    titleOpts = titleOpts.withName(juce::Font::getDefaultMonospacedFontName())
                         .withHeight(14.0f);
    juce::Font titleFont(titleOpts);
    titleFont.setBold(true);
    m_title_label.setFont(titleFont);
    m_title_label.setColour(juce::Label::textColourId, accent);
    addAndMakeVisible(m_title_label);

    // Mode selector with all PNW waveforms
    m_mode_selector.addItem("sin", 1);
    m_mode_selector.addItem("tri", 2);
    m_mode_selector.addItem("sq", 3);
    m_mode_selector.addItem("gt", 4);
    m_mode_selector.addItem("env", 5);
    m_mode_selector.addItem("rnd", 6);
    m_mode_selector.addItem("smo", 7);
    m_mode_selector.setSelectedItemIndex(waveform_to_index(generator.get_mode()));
    m_mode_selector.addListener(this);
    addAndMakeVisible(m_mode_selector);

    // Page navigation buttons
    m_prev_page_button.setButtonText("<");
    m_prev_page_button.setLookAndFeel(&m_button_lnf);
    m_prev_page_button.onClick = [this]() { prev_page(); };
    addAndMakeVisible(m_prev_page_button);

    m_next_page_button.setButtonText(">");
    m_next_page_button.setLookAndFeel(&m_button_lnf);
    m_next_page_button.onClick = [this]() { next_page(); };
    addAndMakeVisible(m_next_page_button);

    m_page_label.setJustificationType(juce::Justification::centred);
    m_page_label.setFont(juce::Font(juce::FontOptions().withHeight(12.0f)));
    // Page label hidden - just use < > buttons for navigation

    // Helper to create parameter rows with unique parameter IDs
    const juce::String lfoPrefix = "lfo" + juce::String(lfo_index) + "_";
    
    auto makeParam = [this, &lfoPrefix](const juce::String& key,
                                        double minVal,
                                        double maxVal,
                                        double defaultVal,
                                        double step,
                                        const juce::String& suffix = "",
                                        int decimals = 2,
                                        bool displayAsPercent = false) -> std::unique_ptr<LfoParamRow>
    {
        LfoParamRow::Config config;
        config.key = key;
        config.parameterId = lfoPrefix + key;
        config.minValue = minVal;
        config.maxValue = maxVal;
        config.defaultValue = defaultVal;
        config.interval = step;
        config.suffix = suffix;
        config.decimals = decimals;
        config.displayAsPercent = displayAsPercent;
        auto row = std::make_unique<LfoParamRow>(config, m_midi_manager);
        row->set_accent_colour(m_accent_colour);
        row->set_on_value_changed([this]() { update_generator_settings(); });
        return row;
    };

    // Create all parameters in order (for paging)
    // Page 0: div, depth, level, width, phase, delay
    // 0-1 range params use displayAsPercent=true to show as 0-99
    m_params.push_back(makeParam("div", 0.015625, 64.0, generator.get_clock_division(), 0.0001, "x", 3, false));
    
    auto depthParam = makeParam("depth", 0.0, 1.0, generator.get_depth(), 0.01, "", 2, true);
    m_depth_param = depthParam.get();
    m_params.push_back(std::move(depthParam));

    m_params.push_back(makeParam("level", 0.0, 1.0, generator.get_level(), 0.01, "", 2, true));
    m_params.push_back(makeParam("width", 0.0, 1.0, generator.get_width(), 0.01, "", 2, true));
    m_params.push_back(makeParam("phase", 0.0, 1.0, generator.get_phase_offset(), 0.01, "", 2, true));
    m_params.push_back(makeParam("delay", 0.0, 1.0, generator.get_delay(), 0.01, "", 2, true));

    // Page 1: dly/, slop, eStep, eTrig, eRot, rSkip
    m_params.push_back(makeParam("dly/", 1.0, 16.0, generator.get_delay_div(), 1.0, "", 0, false));
    m_params.push_back(makeParam("slop", 0.0, 1.0, generator.get_slop(), 0.01, "", 2, true));
    m_params.push_back(makeParam("eStep", 0.0, 64.0, generator.get_euclidean_steps(), 1.0, "", 0, false));
    m_params.push_back(makeParam("eTrig", 0.0, 64.0, generator.get_euclidean_triggers(), 1.0, "", 0, false));
    m_params.push_back(makeParam("eRot", 0.0, 64.0, generator.get_euclidean_rotation(), 1.0, "", 0, false));
    m_params.push_back(makeParam("rSkip", 0.0, 1.0, generator.get_random_skip(), 0.01, "", 2, true));

    // Page 2: loop
    m_params.push_back(makeParam("loop", 0.0, 64.0, generator.get_loop_beats(), 1.0, "", 0, false));

    // Add all params as child components
    for (auto& param : m_params)
    {
        if (param != nullptr)
            addChildComponent(param.get());
    }

    m_wave_preview = std::make_unique<WavePreview>(*this);
    addAndMakeVisible(m_wave_preview.get());

    // Cache initial values
    m_last_depth = generator.get_depth();
    m_last_mode = static_cast<int>(generator.get_mode());
    m_last_clock_div = generator.get_clock_division();
    
    go_to_page(0);
    refresh_wave_preview();
    startTimerHz(10);
}

LayerCakeLfoWidget::~LayerCakeLfoWidget()
{
    m_prev_page_button.setLookAndFeel(nullptr);
    m_next_page_button.setLookAndFeel(nullptr);
}

void LayerCakeLfoWidget::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    const float corner = juce::jmin(6.0f, bounds.getHeight() * 0.1f);
    
    // Dark terminal-like background
    g.setColour(juce::Colour(0xff1a1a1a));
    g.fillRoundedRectangle(bounds, corner);
    
    // Accent border
    g.setColour(m_accent_colour.withAlpha(0.5f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), corner, 1.0f);
    
    // LED indicator showing current LFO value
    if (!m_led_bounds.isEmpty())
    {
        auto ledRect = m_led_bounds.toFloat();
        
        // Outer glow when value is high
        if (m_current_lfo_value > 0.1f)
        {
            const float glowAlpha = m_current_lfo_value * 0.4f;
            g.setColour(m_accent_colour.withAlpha(glowAlpha));
            g.fillEllipse(ledRect.expanded(2.0f));
        }
        
        // LED background (dark when off)
        const float brightness = 0.15f + m_current_lfo_value * 0.85f;
        g.setColour(m_accent_colour.withMultipliedBrightness(brightness));
        g.fillEllipse(ledRect);
        
        // Highlight for 3D effect
        g.setColour(juce::Colours::white.withAlpha(0.3f * m_current_lfo_value));
        g.fillEllipse(ledRect.reduced(ledRect.getWidth() * 0.3f)
                            .translated(-ledRect.getWidth() * 0.1f, -ledRect.getHeight() * 0.1f));
    }
}

void LayerCakeLfoWidget::resized()
{
    const int margin = 8;
    const int headerHeight = 20;
    const int previewHeight = juce::jmax(24, static_cast<int>(getHeight() * 0.15f));
    const int paramRowHeight = 18;
    const int paramSpacing = 4;
    const int pageNavHeight = 16;
    const int ledSize = 8;
    const int ledMargin = 4;

    auto bounds = getLocalBounds().reduced(margin);

    // Header row: LED, title, mode selector
    auto headerArea = bounds.removeFromTop(headerHeight);
    const int selectorWidth = juce::jmax(40, headerArea.getWidth() / 3);
    auto selectorArea = headerArea.removeFromRight(selectorWidth);
    m_mode_selector.setBounds(selectorArea);
    
    // LED next to title
    auto ledArea = headerArea.removeFromLeft(ledSize + ledMargin);
    m_led_bounds = ledArea.withSizeKeepingCentre(ledSize, ledSize);
    
    m_title_label.setBounds(headerArea);
    bounds.removeFromTop(4);

    // Wave preview
    auto previewArea = bounds.removeFromTop(previewHeight);
    if (m_wave_preview != nullptr)
        m_wave_preview->setBounds(previewArea);
    bounds.removeFromTop(6);

    // Page navigation at bottom (no label, just < > buttons)
    auto pageNavArea = bounds.removeFromBottom(pageNavHeight);
    const int navButtonWidth = 16;
    m_prev_page_button.setBounds(pageNavArea.removeFromLeft(navButtonWidth));
    m_next_page_button.setBounds(pageNavArea.removeFromRight(navButtonWidth));
    // m_page_label hidden - no "1/3" display

    bounds.removeFromBottom(4);

    // Parameter rows - 2 columns layout
    const int totalParams = static_cast<int>(m_params.size());
    const int startIdx = m_current_page * kParamsPerPage;
    const int colWidth = bounds.getWidth() / 2;

    for (int i = 0; i < kParamsPerPage; ++i)
    {
        int paramIdx = startIdx + i;
        if (paramIdx >= totalParams) continue;
        
        auto* param = m_params[static_cast<size_t>(paramIdx)].get();
        if (param == nullptr) continue;

        // 2 columns: left (0,2,4) and right (1,3,5)
        const int row = i / 2;
        const int col = i % 2;
        
        const int x = bounds.getX() + col * colWidth;
        const int y = bounds.getY() + row * (paramRowHeight + paramSpacing);
        param->setBounds(x, y, colWidth - 2, paramRowHeight);
    }

    update_controls_visibility();
}

float LayerCakeLfoWidget::get_depth() const noexcept
{
    return m_depth_param != nullptr
        ? static_cast<float>(m_depth_param->get_value())
        : 0.0f;
}

void LayerCakeLfoWidget::refresh_wave_preview()
{
    if (m_wave_preview == nullptr) return;

    std::vector<float> samples(static_cast<size_t>(kPreviewSamples), 0.0f);
    flower::LayerCakeLfoUGen preview = m_generator;
    preview.reset_phase();
    preview.sync_time(0.0);

    const double window_beats = 4.0;
    const double step = window_beats / static_cast<double>(samples.size());
    const float depth = juce::jlimit(0.0f, 1.0f, preview.get_depth());

    // Always clocked mode
    for (size_t i = 0; i < samples.size(); ++i)
    {
        samples[i] = preview.advance_clocked(i * step) * depth;
    }

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
    
    // Update all param values from generator
    // Param order: div, depth, level, width, phase, delay, dly/, slop, eStep, eTrig, eRot, rSkip, loop
    const std::array<double, 13> values = {
        m_generator.get_clock_division(),
        m_generator.get_depth(),
        m_generator.get_level(),
        m_generator.get_width(),
        m_generator.get_phase_offset(),
        m_generator.get_delay(),
        static_cast<double>(m_generator.get_delay_div()),
        m_generator.get_slop(),
        static_cast<double>(m_generator.get_euclidean_steps()),
        static_cast<double>(m_generator.get_euclidean_triggers()),
        static_cast<double>(m_generator.get_euclidean_rotation()),
        m_generator.get_random_skip(),
        static_cast<double>(m_generator.get_loop_beats())
    };

    for (size_t i = 0; i < m_params.size() && i < values.size(); ++i)
    {
        if (m_params[i] != nullptr)
            m_params[i]->set_value(values[i], false);
    }
        
    refresh_wave_preview();
    update_controls_visibility();
}

void LayerCakeLfoWidget::set_tempo_provider(std::function<double()> tempo_bpm_provider)
{
    m_tempo_bpm_provider = std::move(tempo_bpm_provider);
}

void LayerCakeLfoWidget::set_on_hover_changed(std::function<void(bool)> callback)
{
    m_hover_changed_callback = std::move(callback);
}

void LayerCakeLfoWidget::set_current_value(float value)
{
    if (std::abs(value - m_current_lfo_value) > 0.01f)
    {
        m_current_lfo_value = juce::jlimit(0.0f, 1.0f, value);
        repaint(m_led_bounds.expanded(2));
    }
}

void LayerCakeLfoWidget::mouseEnter(const juce::MouseEvent& /*event*/)
{
    if (!m_is_hovered)
    {
        m_is_hovered = true;
        if (m_hover_changed_callback)
            m_hover_changed_callback(true);
    }
}

void LayerCakeLfoWidget::mouseExit(const juce::MouseEvent& event)
{
    // Only trigger exit if mouse is actually leaving the widget bounds
    // (not just moving to a child component)
    auto localPos = event.getEventRelativeTo(this).getPosition();
    if (!getLocalBounds().contains(localPos))
    {
        if (m_is_hovered)
        {
            m_is_hovered = false;
            if (m_hover_changed_callback)
                m_hover_changed_callback(false);
        }
    }
}

void LayerCakeLfoWidget::comboBoxChanged(juce::ComboBox* comboBoxThatHasChanged)
{
    if (comboBoxThatHasChanged != &m_mode_selector) return;
    update_generator_settings();
}

void LayerCakeLfoWidget::update_generator_settings()
{
    m_generator.set_mode(waveform_from_index(m_mode_selector.getSelectedItemIndex()));
    
    // Param order: div, depth, level, width, phase, delay, dly/, slop, eStep, eTrig, eRot, rSkip, loop
    if (m_params.size() >= 13)
    {
        if (m_params[0] != nullptr)
            m_generator.set_clock_division(static_cast<float>(m_params[0]->get_value()));
        if (m_params[1] != nullptr)
            m_generator.set_depth(static_cast<float>(m_params[1]->get_value()));
        if (m_params[2] != nullptr)
            m_generator.set_level(static_cast<float>(m_params[2]->get_value()));
        if (m_params[3] != nullptr)
            m_generator.set_width(static_cast<float>(m_params[3]->get_value()));
        if (m_params[4] != nullptr)
            m_generator.set_phase_offset(static_cast<float>(m_params[4]->get_value()));
        if (m_params[5] != nullptr)
            m_generator.set_delay(static_cast<float>(m_params[5]->get_value()));
        if (m_params[6] != nullptr)
            m_generator.set_delay_div(static_cast<int>(m_params[6]->get_value()));
        if (m_params[7] != nullptr)
            m_generator.set_slop(static_cast<float>(m_params[7]->get_value()));
        if (m_params[8] != nullptr)
            m_generator.set_euclidean_steps(static_cast<int>(m_params[8]->get_value()));
        if (m_params[9] != nullptr)
            m_generator.set_euclidean_triggers(static_cast<int>(m_params[9]->get_value()));
        if (m_params[10] != nullptr)
            m_generator.set_euclidean_rotation(static_cast<int>(m_params[10]->get_value()));
        if (m_params[11] != nullptr)
            m_generator.set_random_skip(static_cast<float>(m_params[11]->get_value()));
        if (m_params[12] != nullptr)
            m_generator.set_loop_beats(static_cast<int>(m_params[12]->get_value()));
    }
        
    notify_settings_changed();
}

void LayerCakeLfoWidget::notify_settings_changed()
{
    refresh_wave_preview();
    if (m_settings_changed_callback != nullptr)
        m_settings_changed_callback();
}

void LayerCakeLfoWidget::timerCallback()
{
    // Check hover state - mouse may have moved to a child component
    // and we need to track when it truly leaves the widget
    if (m_is_hovered)
    {
        auto mousePos = juce::Desktop::getInstance().getMousePosition();
        auto localPos = getLocalPoint(nullptr, mousePos);
        if (!getLocalBounds().contains(localPos))
        {
            m_is_hovered = false;
            if (m_hover_changed_callback)
                m_hover_changed_callback(false);
        }
    }

    const float depth = m_generator.get_depth();
    const int mode = static_cast<int>(m_generator.get_mode());
    const float div = m_generator.get_clock_division();

    const bool changed = (std::abs(depth - m_last_depth) > 0.0005f)
                      || (mode != m_last_mode)
                      || (std::abs(div - m_last_clock_div) > 0.0005f);
                      
    if (!changed) return;

    m_last_depth = depth;
    m_last_mode = mode;
    m_last_clock_div = div;
    
    refresh_wave_preview();
}

double LayerCakeLfoWidget::get_tempo_bpm() const
{
    if (m_tempo_bpm_provider != nullptr)
    {
        double bpm = m_tempo_bpm_provider();
        if (bpm > 0.0) return bpm;
    }
    return 120.0;
}

void LayerCakeLfoWidget::update_controls_visibility()
{
    // Hide all params first
    for (auto& param : m_params)
    {
        if (param != nullptr)
            param->setVisible(false);
    }

    // Show params for current page
    const int totalParams = static_cast<int>(m_params.size());
    const int startIdx = m_current_page * kParamsPerPage;

    for (int i = 0; i < kParamsPerPage; ++i)
    {
        int paramIdx = startIdx + i;
        if (paramIdx >= totalParams) continue;
        
        auto* param = m_params[static_cast<size_t>(paramIdx)].get();
        if (param != nullptr)
            param->setVisible(true);
    }

    // Update page label
    const int totalPages = (totalParams + kParamsPerPage - 1) / kParamsPerPage;
    // Page label hidden
    juce::ignoreUnused(totalPages);
    m_page_label.setText("", 
                         juce::dontSendNotification);
}

void LayerCakeLfoWidget::go_to_page(int page)
{
    const int totalParams = static_cast<int>(m_params.size());
    const int totalPages = (totalParams + kParamsPerPage - 1) / kParamsPerPage;
    m_current_page = juce::jlimit(0, juce::jmax(0, totalPages - 1), page);
    update_controls_visibility();
    resized();
}

void LayerCakeLfoWidget::next_page()
{
    go_to_page(m_current_page + 1);
}

void LayerCakeLfoWidget::prev_page()
{
    go_to_page(m_current_page - 1);
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
    const float corner = juce::jmin(4.0f, bounds.getHeight() * 0.15f);
    const auto accent = m_owner.get_accent_colour();
    
    // Darker background for wave preview
    g.setColour(juce::Colour(0xff0d0d0d));
    g.fillRoundedRectangle(bounds, corner);
    g.setColour(accent.withAlpha(0.3f));
    g.drawRoundedRectangle(bounds, corner, 0.8f);

    if (m_points.empty()) return;

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
    g.strokePath(wave, juce::PathStrokeType(1.5f));
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
    if (container == nullptr) return;

    const auto description = LfoDragHelpers::make_description(
        m_owner.m_lfo_index,
        m_owner.get_accent_colour(),
        m_owner.m_drag_label);

    container->startDragging(description, this);
    m_is_dragging = true;
    juce::ignoreUnused(event);
}

juce::Font LayerCakeLfoWidget::SmallButtonLookAndFeel::getTextButtonFont(juce::TextButton& button, int buttonHeight)
{
    auto base = juce::LookAndFeel_V4::getTextButtonFont(button, buttonHeight);
    return base.withHeight(9.0f);
}

} // namespace LayerCakeApp
