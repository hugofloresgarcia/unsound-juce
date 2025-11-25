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
    
    auto bounds = getLocalBounds();
    
    // Monospace font for NES/CLI aesthetic
    juce::FontOptions fontOpts;
    fontOpts = fontOpts.withName(juce::Font::getDefaultMonospacedFontName())
                       .withHeight(11.0f);
    juce::Font monoFont(fontOpts);
    g.setFont(monoFont);
    
    // Highlight if MIDI learning this parameter (sharp rect)
    if (m_midi_manager != nullptr && 
        m_midi_manager->isLearning() && 
        m_midi_manager->getLearningParameterId() == m_config.parameterId)
    {
        g.setColour(m_accent.withAlpha(0.3f));
        g.fillRect(bounds);
    }
    
    // Key in accent color
    g.setColour(m_accent);
    const juce::String keyText = m_config.key + ":";
    const int keyWidth = 42;  // Fixed width for alignment
    g.drawText(keyText, bounds.removeFromLeft(keyWidth), juce::Justification::centredLeft, false);
    
    // Value in white/light gray (NES white)
    g.setColour(m_is_dragging ? m_accent : juce::Colour(0xfffcfcfc));
    g.drawText(format_value(), bounds.toFloat(), juce::Justification::centredLeft, false);
    
    // Show MIDI CC indicator if mapped
    if (m_midi_manager != nullptr && m_config.parameterId.isNotEmpty())
    {
        const int cc = m_midi_manager->getMappingForParameter(m_config.parameterId);
        if (cc >= 0)
        {
            g.setColour(m_accent.withAlpha(0.5f));
            g.setFont(monoFont.withHeight(9.0f));
            const juce::String ccText = "CC" + juce::String(cc);
            g.drawText(ccText, getLocalBounds().removeFromRight(24), 
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
                         .withHeight(12.0f);
    juce::Font titleFont(titleOpts);
    titleFont.setBold(true);
    m_title_label.setFont(titleFont);
    m_title_label.setColour(juce::Label::textColourId, accent);
    m_title_label.setColour(juce::Label::backgroundWhenEditingColourId, juce::Colour(0xff202020));
    m_title_label.setColour(juce::Label::outlineWhenEditingColourId, accent);
    m_title_label.setColour(juce::TextEditor::textColourId, juce::Colour(0xfffcfcfc));
    m_title_label.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff202020));
    m_title_label.setColour(juce::TextEditor::highlightColourId, accent.withAlpha(0.4f));
    m_title_label.setEditable(false, true, false);  // double-click to edit
    m_title_label.addListener(this);
    addAndMakeVisible(m_title_label);

    // Mode selector with all PNW waveforms - NES style
    m_mode_selector.setLookAndFeel(&m_button_lnf);
    m_mode_selector.setColour(juce::ComboBox::outlineColourId, accent);
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

    m_preset_button.setButtonText("PRE");
    m_preset_button.setTooltip("Save or load LFO presets");
    m_preset_button.setLookAndFeel(&m_button_lnf);
    m_preset_button.onClick = [this]() { show_preset_menu(); };
    m_preset_button.setEnabled(false);
    addAndMakeVisible(m_preset_button);

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

    // Page 2: loop, bi (bipolar toggle)
    m_params.push_back(makeParam("loop", 0.0, 64.0, generator.get_loop_beats(), 1.0, "", 0, false));
    m_params.push_back(makeParam("bi", 0.0, 1.0, generator.get_bipolar() ? 1.0 : 0.0, 1.0, "", 0, false));

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
    update_preset_button_state();
}

LayerCakeLfoWidget::~LayerCakeLfoWidget()
{
    m_mode_selector.setLookAndFeel(nullptr);
    m_prev_page_button.setLookAndFeel(nullptr);
    m_next_page_button.setLookAndFeel(nullptr);
    m_preset_button.setLookAndFeel(nullptr);
}

void LayerCakeLfoWidget::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    
    // NES-style: sharp black background
    g.setColour(juce::Colour(0xff101010));
    g.fillRect(bounds);
    
    // Pixel border - white outer, accent inner
    g.setColour(juce::Colour(0xff404040));
    g.drawRect(bounds, 1);
    g.setColour(m_accent_colour.withAlpha(0.7f));
    g.drawRect(bounds.reduced(1), 1);
    
    // NES-style LED indicator (square, not round)
    if (!m_led_bounds.isEmpty())
    {
        auto ledRect = m_led_bounds;
        const float clampedValue = juce::jlimit(0.0f, 1.0f, m_current_lfo_value);
        const float brightness = 0.2f + clampedValue * 0.8f;

        // LED fill scales with LFO value
        g.setColour(m_accent_colour.withMultipliedBrightness(brightness));
        g.fillRect(ledRect);

        // Simple highlight pixel for extra punch when active
        if (clampedValue > 0.15f)
        {
            g.setColour(juce::Colour(0xfffcfcfc).withAlpha(0.4f));
            g.fillRect(ledRect.reduced(2));
        }
        
        // Pixel border on LED
        g.setColour(juce::Colour(0xff000000));
        g.drawRect(ledRect, 1);
    }
    
    // Scanlines overlay for CRT feel
    g.setColour(juce::Colour(0x20000000));
    for (int y = bounds.getY(); y < bounds.getBottom(); y += 2)
    {
        g.drawHorizontalLine(y, static_cast<float>(bounds.getX()), static_cast<float>(bounds.getRight()));
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
    auto prevArea = pageNavArea.removeFromLeft(navButtonWidth);
    auto nextArea = pageNavArea.removeFromRight(navButtonWidth);
    m_prev_page_button.setBounds(prevArea);
    m_next_page_button.setBounds(nextArea);
    if (!pageNavArea.isEmpty())
    {
        auto presetArea = pageNavArea.reduced(2, 0);
        m_preset_button.setBounds(presetArea);
    }
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
    m_title_label.setText(get_display_label(), juce::dontSendNotification);
}

void LayerCakeLfoWidget::set_custom_label(const juce::String& label)
{
    m_custom_label = label;
    m_title_label.setText(get_display_label(), juce::dontSendNotification);
}

juce::String LayerCakeLfoWidget::get_display_label() const
{
    return m_custom_label.isNotEmpty() ? m_custom_label : m_drag_label;
}

void LayerCakeLfoWidget::set_on_label_changed(std::function<void(const juce::String&)> callback)
{
    m_label_changed_callback = std::move(callback);
}

void LayerCakeLfoWidget::set_preset_handlers(PresetHandlers handlers)
{
    m_preset_handlers = std::move(handlers);
    update_preset_button_state();
}

void LayerCakeLfoWidget::labelTextChanged(juce::Label* labelThatHasChanged)
{
    if (labelThatHasChanged != &m_title_label) return;
    
    juce::String newLabel = m_title_label.getText().trim();
    
    // If empty or matches default, clear custom label
    if (newLabel.isEmpty() || newLabel == m_drag_label)
    {
        m_custom_label = {};
        m_title_label.setText(m_drag_label, juce::dontSendNotification);
    }
    else
    {
        m_custom_label = newLabel;
    }
    
    if (m_label_changed_callback)
        m_label_changed_callback(m_custom_label);
}

void LayerCakeLfoWidget::editorShown(juce::Label* label, juce::TextEditor& editor)
{
    if (label != &m_title_label) return;
    
    // Style the editor for NES look
    juce::FontOptions opts;
    opts = opts.withName(juce::Font::getDefaultMonospacedFontName()).withHeight(12.0f);
    editor.setFont(juce::Font(opts));
    editor.selectAll();
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
    // Param order: div, depth, level, width, phase, delay, dly/, slop, eStep, eTrig, eRot, rSkip, loop, bi
    const std::array<double, 14> values = {
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
        static_cast<double>(m_generator.get_loop_beats()),
        m_generator.get_bipolar() ? 1.0 : 0.0
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

void LayerCakeLfoWidget::update_preset_button_state()
{
    const bool hasSave = static_cast<bool>(m_preset_handlers.savePreset);
    const bool hasLoad = static_cast<bool>(m_preset_handlers.loadPreset)
                      && static_cast<bool>(m_preset_handlers.getPresetNames);
    m_preset_button.setEnabled(hasSave || hasLoad);
}

void LayerCakeLfoWidget::show_preset_menu()
{
    if (!m_preset_button.isEnabled())
        return;

    juce::PopupMenu menu;
    if (m_preset_handlers.savePreset)
        menu.addItem("Save preset...", [this]() { prompt_save_preset(); });

    juce::StringArray presetNames;
    if (m_preset_handlers.getPresetNames)
        presetNames = m_preset_handlers.getPresetNames();

    if (m_preset_handlers.loadPreset && !presetNames.isEmpty())
    {
        if (menu.getNumItems() > 0)
            menu.addSeparator();
        juce::PopupMenu loadMenu;
        for (auto& name : presetNames)
        {
            loadMenu.addItem(name, [this, name]() { attempt_load_preset(name); });
        }
        menu.addSubMenu("Load preset", loadMenu);
    }

    if (menu.getNumItems() == 0)
        return;

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&m_preset_button));
}

void LayerCakeLfoWidget::prompt_save_preset()
{
    if (!m_preset_handlers.savePreset)
        return;

    auto* window = new juce::AlertWindow("Save LFO Preset",
                                         "Enter a name for this preset:",
                                         juce::AlertWindow::NoIcon);
    window->addTextEditor("presetName", get_display_label().trim(), "Preset");
    window->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    window->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    window->enterModalState(true,
                            juce::ModalCallbackFunction::create([this, window](int result) {
                                std::unique_ptr<juce::AlertWindow> cleanup(window);
                                if (result == 0)
                                    return;

                                auto name = window->getTextEditorContents("presetName").trim();
                                if (name.isEmpty())
                                {
                                    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                                                           "Save LFO Preset",
                                                                           "Please enter a preset name.");
                                    return;
                                }

                                if (!m_preset_handlers.savePreset)
                                    return;

                                auto slot = capture_slot_data();
                                const bool ok = m_preset_handlers.savePreset(name, slot);
                                if (!ok)
                                {
                                    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                                                           "Save LFO Preset",
                                                                           "Failed to save preset \"" + name + "\".");
                                }
                            }),
                            false);
}

void LayerCakeLfoWidget::attempt_load_preset(const juce::String& presetName)
{
    if (!m_preset_handlers.loadPreset)
        return;

    LayerCakePresetData::LfoSlotData slot;
    if (!m_preset_handlers.loadPreset(presetName, slot))
    {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                               "Load LFO Preset",
                                               "Failed to load preset \"" + presetName + "\".");
        return;
    }

    apply_slot_data(slot);
}

LayerCakePresetData::LfoSlotData LayerCakeLfoWidget::capture_slot_data() const
{
    LayerCakePresetData::LfoSlotData slot;
    slot.label = m_custom_label;
    slot.mode = static_cast<int>(m_generator.get_mode());
    slot.rate_hz = m_generator.get_rate_hz();
    slot.depth = m_generator.get_depth();
    slot.tempo_sync = true;
    slot.clock_division = m_generator.get_clock_division();
    slot.pattern_length = m_generator.get_pattern_length();
    slot.pattern_buffer = m_generator.get_pattern_buffer();
    slot.level = m_generator.get_level();
    slot.width = m_generator.get_width();
    slot.phase_offset = m_generator.get_phase_offset();
    slot.delay = m_generator.get_delay();
    slot.delay_div = m_generator.get_delay_div();
    slot.slop = m_generator.get_slop();
    slot.euclidean_steps = m_generator.get_euclidean_steps();
    slot.euclidean_triggers = m_generator.get_euclidean_triggers();
    slot.euclidean_rotation = m_generator.get_euclidean_rotation();
    slot.random_skip = m_generator.get_random_skip();
    slot.loop_beats = m_generator.get_loop_beats();
    slot.bipolar = m_generator.get_bipolar();
    slot.random_seed = m_generator.get_random_seed();
    return slot;
}

void LayerCakeLfoWidget::apply_slot_data(const LayerCakePresetData::LfoSlotData& data)
{
    const int maxMode = static_cast<int>(flower::LfoWaveform::SmoothRandom);
    const int mode = juce::jlimit(0, maxMode, data.mode);
    m_generator.set_mode(static_cast<flower::LfoWaveform>(mode));
    m_generator.set_rate_hz(juce::jlimit(0.01f, 20.0f, data.rate_hz));
    m_generator.set_depth(juce::jlimit(0.0f, 1.0f, data.depth));
    m_generator.set_clock_division(data.clock_division);
    m_generator.set_pattern_length(data.pattern_length);
    m_generator.set_pattern_buffer(data.pattern_buffer);
    m_generator.set_level(juce::jlimit(0.0f, 1.0f, data.level));
    m_generator.set_width(juce::jlimit(0.0f, 1.0f, data.width));
    m_generator.set_phase_offset(juce::jlimit(0.0f, 1.0f, data.phase_offset));
    m_generator.set_delay(juce::jlimit(0.0f, 1.0f, data.delay));
    m_generator.set_delay_div(juce::jmax(1, data.delay_div));
    m_generator.set_slop(juce::jlimit(0.0f, 1.0f, data.slop));
    m_generator.set_euclidean_steps(juce::jmax(0, data.euclidean_steps));
    m_generator.set_euclidean_triggers(juce::jmax(0, data.euclidean_triggers));
    m_generator.set_euclidean_rotation(juce::jmax(0, data.euclidean_rotation));
    m_generator.set_random_skip(juce::jlimit(0.0f, 1.0f, data.random_skip));
    m_generator.set_loop_beats(juce::jmax(0, data.loop_beats));
    m_generator.set_bipolar(data.bipolar);
    if (data.random_seed != 0)
        m_generator.set_random_seed(data.random_seed);
    m_generator.reset_phase();

    if (data.label.isNotEmpty())
        set_custom_label(data.label);
    else
        set_custom_label({});

    if (m_label_changed_callback)
        m_label_changed_callback(get_display_label());

    sync_controls_from_generator();
    notify_settings_changed();
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
    
    // Param order: div, depth, level, width, phase, delay, dly/, slop, eStep, eTrig, eRot, rSkip, loop, bi
    if (m_params.size() >= 14)
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
        if (m_params[13] != nullptr)
            m_generator.set_bipolar(m_params[13]->get_value() > 0.5);
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
    auto bounds = getLocalBounds();
    const auto accent = m_owner.get_accent_colour();
    
    // NES-style: sharp black background
    g.setColour(juce::Colour(0xff080808));
    g.fillRect(bounds);
    
    // Pixel border
    g.setColour(accent.withAlpha(0.4f));
    g.drawRect(bounds, 1);

    if (m_points.empty()) return;

    // NES-style stepped/blocky waveform
    const int midY = bounds.getCentreY();
    const int amplitude = bounds.getHeight() / 2 - 2;
    const int num_cols = juce::jmin(32, static_cast<int>(m_points.size())); // Limit columns for chunky look
    const int col_width = juce::jmax(1, bounds.getWidth() / num_cols);
    const int samples_per_col = juce::jmax(1, static_cast<int>(m_points.size()) / num_cols);

    g.setColour(accent);
    
    for (int col = 0; col < num_cols; ++col)
    {
        // Get average value for this column
        float sum = 0.0f;
        const int start_idx = col * samples_per_col;
        const int end_idx = juce::jmin(start_idx + samples_per_col, static_cast<int>(m_points.size()));
        for (int i = start_idx; i < end_idx; ++i)
        {
            sum += m_points[static_cast<size_t>(i)];
        }
        const float avg = sum / static_cast<float>(end_idx - start_idx);
        const float value = juce::jlimit(-1.0f, 1.0f, avg);
        
        // Draw as vertical bar from center
        const int x = bounds.getX() + col * col_width;
        const int bar_height = static_cast<int>(std::abs(value) * amplitude);
        
        if (value >= 0)
        {
            g.fillRect(x, midY - bar_height, col_width - 1, bar_height);
        }
        else
        {
            g.fillRect(x, midY, col_width - 1, bar_height);
        }
    }
    
    // Center line
    g.setColour(accent.withAlpha(0.3f));
    g.drawHorizontalLine(midY, static_cast<float>(bounds.getX()), static_cast<float>(bounds.getRight()));
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
    juce::ignoreUnused(button, buttonHeight);
    // NES-style small monospace font
    juce::FontOptions opts;
    opts = opts.withName(juce::Font::getDefaultMonospacedFontName()).withHeight(10.0f);
    return juce::Font(opts);
}

void LayerCakeLfoWidget::SmallButtonLookAndFeel::drawButtonBackground(juce::Graphics& g,
                                                                       juce::Button& button,
                                                                       const juce::Colour& backgroundColour,
                                                                       bool shouldDrawButtonAsHighlighted,
                                                                       bool shouldDrawButtonAsDown)
{
    juce::ignoreUnused(backgroundColour);
    auto bounds = button.getLocalBounds();
    
    // NES-style sharp rectangle button
    if (shouldDrawButtonAsDown)
        g.setColour(juce::Colour(0xff404040));
    else if (shouldDrawButtonAsHighlighted)
        g.setColour(juce::Colour(0xff303030));
    else
        g.setColour(juce::Colour(0xff202020));
    
    g.fillRect(bounds);
    
    // Pixel border
    g.setColour(juce::Colour(0xff606060));
    g.drawRect(bounds, 1);
}

void LayerCakeLfoWidget::SmallButtonLookAndFeel::drawButtonText(juce::Graphics& g,
                                                                 juce::TextButton& button,
                                                                 bool shouldDrawButtonAsHighlighted,
                                                                 bool shouldDrawButtonAsDown)
{
    juce::ignoreUnused(shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);
    auto font = getTextButtonFont(button, button.getHeight());
    g.setFont(font);
    g.setColour(juce::Colour(0xfffcfcfc));
    g.drawText(button.getButtonText(), button.getLocalBounds(), juce::Justification::centred, false);
}

void LayerCakeLfoWidget::SmallButtonLookAndFeel::drawComboBox(juce::Graphics& g,
                                                               int width, int height,
                                                               bool isButtonDown,
                                                               int buttonX, int buttonY,
                                                               int buttonW, int buttonH,
                                                               juce::ComboBox& box)
{
    juce::ignoreUnused(buttonX, buttonY, buttonW, buttonH);
    auto bounds = juce::Rectangle<int>(0, 0, width, height);
    
    // NES-style sharp rectangle
    if (isButtonDown)
        g.setColour(juce::Colour(0xff303030));
    else
        g.setColour(juce::Colour(0xff181818));
    
    g.fillRect(bounds);
    
    // Pixel border - use accent color from parent if available
    auto accent = box.findColour(juce::ComboBox::outlineColourId);
    if (accent == juce::Colour())
        accent = juce::Colour(0xff606060);
    g.setColour(accent.withAlpha(0.6f));
    g.drawRect(bounds, 1);
    
    // Small arrow indicator (NES style - just a simple triangle made of pixels)
    const int arrowSize = 4;
    const int arrowX = width - arrowSize - 4;
    const int arrowY = (height - arrowSize) / 2;
    
    g.setColour(juce::Colour(0xfffcfcfc));
    for (int row = 0; row < arrowSize / 2 + 1; ++row)
    {
        const int rowWidth = arrowSize - row * 2;
        const int rowX = arrowX + row;
        g.fillRect(rowX, arrowY + row, rowWidth, 1);
    }
}

juce::Font LayerCakeLfoWidget::SmallButtonLookAndFeel::getComboBoxFont(juce::ComboBox& box)
{
    juce::ignoreUnused(box);
    juce::FontOptions opts;
    opts = opts.withName(juce::Font::getDefaultMonospacedFontName()).withHeight(10.0f);
    return juce::Font(opts);
}

void LayerCakeLfoWidget::SmallButtonLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    // Leave room for arrow on right
    label.setBounds(2, 0, box.getWidth() - 12, box.getHeight());
    label.setFont(getComboBoxFont(box));
    label.setColour(juce::Label::textColourId, juce::Colour(0xfffcfcfc));
}

} // namespace LayerCakeApp

