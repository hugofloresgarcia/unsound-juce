#include "LayerCakeKnob.h"
#include "LayerCakeLookAndFeel.h"
#include "lfo/LfoDragHelpers.h"
#include "LayerCakeSettings.h"
#include <cmath>

namespace LayerCakeApp
{

namespace
{
constexpr int kLabelHeight = 12;
constexpr int kLabelGap = 0;
constexpr int kValueAreaPadding = 6;
constexpr int kValueLabelInset = 8;
constexpr int kRecorderButtonSize = 20;
constexpr int kRecorderButtonMargin = 4;
constexpr double kBlinkIntervalMs = 320.0;
constexpr int kLfoButtonSize = 16;
constexpr int kLfoButtonMargin = 2;
const juce::Colour kSoftWhite(0xfff4f4f2);
}

LayerCakeKnob::LayerCakeKnob(const Config& config, Shared::MidiLearnManager* midiManager)
    : m_config(config),
      m_midi_manager(midiManager)
{
    m_slider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    m_slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    m_slider.setRange(config.minValue, config.maxValue, config.interval);
    if (config.skewFactor != 1.0)
    {
        m_slider.setSkewFactor(config.skewFactor);
    }
    m_slider.setMouseDragSensitivity(static_cast<int>(LayerCakeSettings::mainKnobSensitivity));

    m_slider.setValue(config.defaultValue, juce::dontSendNotification);
    m_slider.setWantsKeyboardFocus(false);
    m_slider.setDoubleClickReturnValue(true, config.defaultValue);
    if (config.suffix.isNotEmpty())
        m_slider.setTextValueSuffix(config.suffix);
    m_slider.setAlpha(0.0f);
    m_slider.setInterceptsMouseClicks(true, true);
    m_slider.addListener(this);
    if (sweep_recorder_enabled())
        m_slider.addMouseListener(this, true);
    addAndMakeVisible(m_slider);

    m_label.setText(config.labelText, juce::dontSendNotification);
    m_label.setJustificationType(juce::Justification::centred);
    m_label.addMouseListener(this, false);
    addAndMakeVisible(m_label);

    m_value_label.setJustificationType(juce::Justification::centred);
    m_value_label.setFont(juce::Font(juce::FontOptions()
                                     .withName(juce::Font::getDefaultMonospacedFontName())
                                     .withHeight(16.0f)));
    m_value_label.setInterceptsMouseClicks(false, false);
    m_value_label.addMouseListener(this, false);
    addAndMakeVisible(m_value_label);

    if (sweep_recorder_enabled())
    {
        m_recorder_button = std::make_unique<KnobRecorderButton>();
        if (m_recorder_button != nullptr)
        {
            m_recorder_button->onPressed = [this]() { handle_touch_begin(true); };
            m_recorder_button->onReleased = [this]() { handle_touch_end(); };
            addAndMakeVisible(m_recorder_button.get());
            update_recorder_button();
        }
    }

    if (m_config.enableLfoAssignment)
    {
        m_lfo_button = std::make_unique<LfoAssignmentButton>();
        if (m_lfo_button != nullptr)
        {
            m_lfo_button->onClicked = [this]()
            {
                if (!has_lfo_assignment())
                    return;

                if (m_lfo_release_handler != nullptr)
                    m_lfo_release_handler();
            };
            addAndMakeVisible(m_lfo_button.get());
            refresh_lfo_button_state();
        }
    }

    m_sweep_recorder.prepare(44100.0);
    m_sweep_recorder.set_idle_value(static_cast<float>(m_slider.getValue()));

    m_plot_history.resize(kPlotHistorySize, 0.0f);
    // Initialize plot with current value
    const float initialNorm = static_cast<float>((config.defaultValue - config.minValue) / (config.maxValue - config.minValue));
    std::fill(m_plot_history.begin(), m_plot_history.end(), juce::jlimit(0.0f, 1.0f, initialNorm));
    
    // Always start timer for plot updates in CLI mode
    if (config.cliMode)
        startTimerHz(60);

    register_midi_parameter();
    update_value_label();
    apply_look_and_feel_colours();
}

LayerCakeKnob::~LayerCakeKnob()
{
    if (m_text_editor != nullptr)
    {
        m_text_editor->removeListener(this);
        removeChildComponent(m_text_editor.get());
        m_text_editor.reset();
    }
    
    m_slider.removeListener(this);
    if (sweep_recorder_enabled())
        m_slider.removeMouseListener(this);
    m_label.removeMouseListener(this);
    m_value_label.removeMouseListener(this);

    if (m_midi_manager != nullptr && m_registered_parameter_id.isNotEmpty())
        m_midi_manager->unregisterParameter(m_registered_parameter_id);
}

void LayerCakeKnob::paint(juce::Graphics& g)
{
    if (m_config.cliMode)
    {
        paint_cli_mode(g);
        return;
    }
    
    auto bounds = getLocalBounds();
    auto& laf = getLookAndFeel();

    // Reserve space for label at the bottom
    auto labelBounds = bounds.removeFromBottom(kLabelHeight);
    juce::ignoreUnused(labelBounds); // label component handles its own paint
    bounds.removeFromBottom(kLabelGap);

    auto knobArea = bounds.toFloat().reduced(static_cast<float>(kValueAreaPadding));
    const float diameter = juce::jmin(knobArea.getWidth(), knobArea.getHeight());
    auto circle = juce::Rectangle<float>(diameter, diameter).withCentre(knobArea.getCentre());

    const auto surfaceBase = m_slider.findColour(juce::Slider::backgroundColourId, true);
    const auto surface = surfaceBase.darker(0.1f);
    const auto frame = m_slider.findColour(juce::Slider::rotarySliderOutlineColourId, true);
    const auto track = m_slider.findColour(juce::Slider::trackColourId, true);
    const auto accent = m_slider.findColour(juce::Slider::thumbColourId, true);

    g.setColour(surface);
    g.fillEllipse(circle);

    g.setColour(frame);
    g.drawEllipse(circle, 1.4f);

    if (m_drag_highlight)
    {
        auto highlightCircle = circle.expanded(6.0f);
        g.setColour(m_active_drag_colour.withAlpha(0.45f));
        g.drawEllipse(highlightCircle, 2.0f);
    }
    
    if (m_is_keyboard_focused)
    {
        auto focusCircle = circle.expanded(4.0f);
        g.setColour(juce::Colours::yellow.withAlpha(0.6f));
        g.drawEllipse(focusCircle, 2.0f);
    }

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

    if (m_modulation_indicator_value.has_value())
    {
        const float modNormalized = juce::jlimit(0.0f, 1.0f, m_modulation_indicator_value.value());
        const float modAngle = startAngle + modNormalized * sweepAngle;
        const float modRadius = trackRadius + 6.0f;
        juce::Path modPath;
        modPath.addCentredArc(circle.getCentreX(),
                              circle.getCentreY(),
                              modRadius,
                              modRadius,
                              0.0f,
                              startAngle,
                              modAngle,
                              true);
        auto saturatedColour = m_modulation_indicator_colour;
        saturatedColour = saturatedColour.withMultipliedSaturation(2.0f).brighter(0.3f);
        const auto brightColour = saturatedColour.withAlpha(0.95f);
        g.setColour(brightColour);
        g.strokePath(modPath, juce::PathStrokeType(2.5f,
                                                   juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
    }

    auto centre = circle.getCentre();
    const float pointerLength = circle.getWidth() * 0.38f;
    juce::Point<float> pointer(centre.x  + pointerLength * std::cos(angle - (4*3.1415)/8),
                               centre.y  + pointerLength * std::sin(angle - (4*3.1415)/8));

    g.setColour(accent);
    g.fillEllipse(pointer.x - 3.0f, pointer.y - 3.0f, 6.0f, 6.0f);

    if (sweep_recorder_enabled())
    {
        const auto recorderColour = accent.withAlpha(0.45f);
        switch (m_recorder_state)
        {
            case RecorderState::Armed:
                if (m_blink_visible)
                {
                    g.setColour(recorderColour);
                    g.drawEllipse(circle, 2.0f);
                }
                break;
            case RecorderState::Recording:
                g.setColour(recorderColour.brighter(0.2f));
                g.drawEllipse(circle.reduced(4.0f), 2.0f);
                break;
            case RecorderState::Looping:
                g.setColour(recorderColour.withAlpha(0.35f));
                g.drawEllipse(circle.reduced(6.0f), 1.6f);
                break;
            case RecorderState::Idle:
            default:
                break;
        }
    }
}

void LayerCakeKnob::paint_cli_mode(juce::Graphics& g)
{
    // Don't paint if text editor is visible
    if (m_is_editing && m_text_editor != nullptr)
        return;
    
    auto bounds = getLocalBounds().toFloat();
    
    // Monospace font for CLI aesthetic
    juce::FontOptions fontOpts;
    fontOpts = fontOpts.withName(juce::Font::getDefaultMonospacedFontName())
                       .withHeight(15.0f);
    juce::Font monoFont(fontOpts);
    g.setFont(monoFont);
    
    const auto accent = m_slider.findColour(juce::Slider::thumbColourId, true);
    
    // Highlight if drag target
    if (m_drag_highlight)
    {
        g.setColour(m_active_drag_colour.withAlpha(0.15f));
        g.fillRoundedRectangle(bounds, 2.0f);
    }
    
    if (m_is_keyboard_focused)
    {
        g.setColour(juce::Colours::yellow.withAlpha(0.2f));
        g.fillRoundedRectangle(bounds, 2.0f);
        g.setColour(juce::Colours::yellow.withAlpha(0.8f));
        g.drawRoundedRectangle(bounds, 2.0f, 1.0f);
    }
    
    // Recorder state indicator (leftmost)
    juce::String recIndicator;
    juce::Colour recColour;
    if (sweep_recorder_enabled())
    {
        switch (m_recorder_state)
        {
            case RecorderState::Armed:
                recIndicator = m_blink_visible ? "O" : " ";
                recColour = m_blink_visible ? juce::Colours::orange : juce::Colours::orange.darker(0.5f);
                break;
            case RecorderState::Recording:
                recIndicator = m_blink_visible ? "*" : " ";
                recColour = m_blink_visible ? juce::Colours::red : juce::Colours::red.darker(0.5f);
                break;
            case RecorderState::Looping:
                recIndicator = ">";
                recColour = juce::Colours::limegreen;
                break;
            default:
                break;
        }
    }
    
    if (recIndicator.isNotEmpty())
    {
        g.setColour(recColour);
        g.setFont(monoFont.withHeight(13.0f));
        g.drawText(recIndicator, bounds.removeFromLeft(16.0f), juce::Justification::centredLeft, false);
        g.setFont(monoFont);
    }
    
    // LFO assignment indicator (small colored dot)
    // Clear bounds first - will be set if indicator is drawn
    m_lfo_indicator_bounds = {};
    if (has_lfo_assignment() && m_lfo_button_accent.has_value())
    {
        const float dotSize = 6.0f;
        const float hitPadding = 4.0f;  // Extra padding for easier clicking
        const float dotX = bounds.getX() + 2.0f;
        const float dotY = bounds.getCentreY() - dotSize * 0.5f;
        
        // Store bounds for hit testing (with padding for easier clicking)
        m_lfo_indicator_bounds = juce::Rectangle<float>(
            dotX - hitPadding, 
            dotY - hitPadding, 
            dotSize + hitPadding * 2.0f, 
            dotSize + hitPadding * 2.0f
        );
        
        // Glow based on modulation value
        if (m_modulation_indicator_value.has_value())
        {
            const float modVal = std::abs(m_modulation_indicator_value.value());
            if (modVal > 0.01f)
            {
                g.setColour(m_lfo_button_accent.value().withAlpha(modVal * 0.4f));
                g.fillEllipse(dotX - 2.0f, dotY - 2.0f, dotSize + 4.0f, dotSize + 4.0f);
            }
        }
        
        g.setColour(m_lfo_button_accent.value());
        g.fillEllipse(dotX, dotY, dotSize, dotSize);
        
        bounds.removeFromLeft(dotSize + 4.0f);
    }
    
    // Key in accent color (or LFO color if assigned)
    const juce::Colour keyColour = (has_lfo_assignment() && m_lfo_button_accent.has_value()) 
        ? m_lfo_button_accent.value() 
        : accent;
    g.setColour(keyColour);
    const juce::String keyText = m_config.labelText + ":";
    const float keyWidth = 55.0f;  // Fixed width for alignment
    g.drawText(keyText, bounds.removeFromLeft(keyWidth), juce::Justification::centredLeft, false);
    
    // Value color: use LFO color when assigned and not editing, otherwise default
    juce::Colour valueColour = kSoftWhite.withAlpha(0.9f);
    if (has_lfo_assignment() && m_lfo_button_accent.has_value() && !m_show_base_value)
    {
        valueColour = m_lfo_button_accent.value();
    }

    // Draw live plot history
    if (!m_plot_history.empty())
    {
        const float plotHeight = bounds.getHeight() * 0.6f;
        // Position after key, before value (or behind value)
        
        // Actually, request was "right next to the number value"
        // So let's reserve space for value text first
        const float valueWidth = 50.0f;
        auto plotArea = bounds.removeFromRight(bounds.getWidth() - valueWidth); // Remaining space after value
        
        // If we want it next to number, maybe we draw number first then plot
        // Let's redraw value text with specific width
        g.setColour(valueColour);
        g.drawText(format_cli_value(), bounds.removeFromLeft(valueWidth), juce::Justification::centredLeft, false);
        
        // Now draw plot in remaining space
        if (plotArea.getWidth() > 10.0f)
        {
            plotArea = plotArea.reduced(4.0f, 0.0f); // Some padding
            
            g.setColour(keyColour.withAlpha(0.3f));
            // g.drawRect(plotArea, 1.0f); // Debug bounds

            juce::Path plotPath;
            const float stepX = plotArea.getWidth() / static_cast<float>(m_plot_history.size() - 1);
            
            // Start from write index (oldest) to end, then 0 to write index (newest)
            // This creates a scrolling effect from right to left
            bool firstPoint = true;
            
            auto addPoint = [&](int index, float x) {
                const float normalized = m_plot_history[static_cast<size_t>(index)];
                // Invert Y so 1.0 is top
                const float y = plotArea.getBottom() - (normalized * plotHeight) - (plotArea.getHeight() - plotHeight) * 0.5f;
                
                if (firstPoint)
                {
                    plotPath.startNewSubPath(x, y);
                    firstPoint = false;
                }
                else
                {
                    plotPath.lineTo(x, y);
                }
            };

            // Draw ring buffer in order
            float x = plotArea.getX();
            
            // Oldest to newest
            for (int i = 0; i < static_cast<int>(m_plot_history.size()); ++i)
            {
                int index = (m_plot_write_index + i) % static_cast<int>(m_plot_history.size());
                addPoint(index, x);
                x += stepX;
            }
            
            g.setColour(keyColour.withAlpha(0.6f));
            g.strokePath(plotPath, juce::PathStrokeType(1.2f));
            
            // Draw current value dot at end
            auto currentPos = plotPath.getCurrentPosition();
            g.setColour(valueColour);
            g.fillEllipse(currentPos.x - 2.0f, currentPos.y - 2.0f, 4.0f, 4.0f);
        }
    }
    else
    {
        g.setColour(valueColour);
        g.drawText(format_cli_value(), bounds, juce::Justification::centredLeft, false);
    }
    
    // Show MIDI CC indicator if mapped (rightmost)
    if (m_midi_manager != nullptr && m_config.parameterId.isNotEmpty())
    {
        const int cc = m_midi_manager->getMappingForParameter(m_config.parameterId);
        if (cc >= 0)
        {
            g.setColour(accent.withAlpha(0.5f));
            g.setFont(monoFont.withHeight(12.0f));
            const juce::String ccText = "CC" + juce::String(cc);
            g.drawText(ccText, getLocalBounds().toFloat().removeFromRight(32.0f), 
                       juce::Justification::centredRight, false);
        }
    }
}

juce::String LayerCakeKnob::format_cli_value() const
{
    juce::String result;
    double value = m_slider.getValue();
    
    const bool showModulatedValue = has_lfo_assignment()
        && m_modulation_indicator_value.has_value()
        && !m_show_base_value;
    
    if (showModulatedValue)
    {
        const double span = m_config.maxValue - m_config.minValue;
        if (span > 0.0)
        {
            // Calculate effective value: base slider value (center) + LFO modulation
            // m_modulation_indicator_value is 0-1 normalized from (lfo_value + 1.0) * 0.5
            // Convert back to LFO offset (-1 to 1 range)
            const double lfoNormalized = static_cast<double>(m_modulation_indicator_value.value());
            const double lfoOffset = (lfoNormalized * 2.0) - 1.0;  // Convert 0-1 back to -1 to 1
            
            // Normalize base slider value to 0-1
            const double baseValue = m_slider.getValue();
            const double baseNormalized = juce::jlimit(0.0, 1.0, (baseValue - m_config.minValue) / span);
            
            // Combine base (center) with LFO offset
            const double modNormalized = juce::jlimit(0.0, 1.0, baseNormalized + lfoOffset * 0.5);
            
            // Convert back to parameter range
            value = m_config.minValue + modNormalized * span;
        }
    }
    
    // Check if should display as percent (0-99 for 0-1 range)
    const bool isPercentDisplay = m_config.displayAsPercent && 
                                  std::abs(m_config.minValue) < 0.001 && 
                                  std::abs(m_config.maxValue - 1.0) < 0.001;
    
    if (isPercentDisplay)
    {
        const int displayValue = static_cast<int>(std::round(value * 99.0));
        result = juce::String(displayValue);
    }
    else if (m_config.decimals == 0)
    {
        result = juce::String(static_cast<int>(std::round(value)));
    }
    else
    {
        result = juce::String(value, m_config.decimals);
    }
    
    if (m_config.suffix.isNotEmpty())
        result += m_config.suffix;
    
    return result;
}

void LayerCakeKnob::resized()
{
    // In CLI mode, hide child components and use the full bounds for the slider interaction
    if (m_config.cliMode)
    {
        m_label.setVisible(false);
        m_value_label.setVisible(false);
        if (m_recorder_button != nullptr)
            m_recorder_button->setVisible(false);
        if (m_lfo_button != nullptr)
            m_lfo_button->setVisible(false);
        
        // Slider covers full area for mouse interaction
        m_slider.setBounds(getLocalBounds());
        
        // Update text editor bounds if editing
        if (m_text_editor != nullptr)
            m_text_editor->setBounds(getLocalBounds());
        
        return;
    }
    
    const int labelHeight = kLabelHeight;
    const int labelGap = kLabelGap;
    const int valuePadding = kValueAreaPadding;
    const int valueInset = kValueLabelInset;
    const int recorderButtonSize = kRecorderButtonSize;
    const int recorderButtonMargin = kRecorderButtonMargin;
    const int lfoButtonSize = kLfoButtonSize;
    const int lfoButtonMargin = kLfoButtonMargin;

    auto bounds = getLocalBounds();
    auto labelBounds = bounds.removeFromBottom(labelHeight);
    labelBounds.removeFromTop(labelGap);
    m_label.setBounds(labelBounds);
    m_label.setVisible(true);

    auto valueBounds = bounds.reduced(valuePadding);
    m_slider.setBounds(valueBounds);
    m_value_label.setBounds(valueBounds.reduced(valueInset));
    m_value_label.setVisible(true);

    if (m_recorder_button != nullptr)
    {
        m_recorder_button->setVisible(true);
        juce::Rectangle<int> buttonBounds(recorderButtonSize, recorderButtonSize);
        const int targetX = valueBounds.getRight() - recorderButtonMargin - recorderButtonSize;
        const int targetY = valueBounds.getY() + recorderButtonMargin;
        buttonBounds.setPosition(targetX, targetY);
        m_recorder_button->setBounds(buttonBounds);
        m_recorder_button->toFront(false);
    }

    if (m_lfo_button != nullptr)
    {
        m_lfo_button->setVisible(true);
        juce::Rectangle<int> lfoBounds(lfoButtonSize, lfoButtonSize);
        const int targetX = valueBounds.getRight() - lfoButtonMargin - lfoButtonSize;
        const int targetY = valueBounds.getBottom() - lfoButtonMargin - lfoButtonSize;
        lfoBounds.setPosition(targetX, targetY);
        m_lfo_button->setBounds(lfoBounds);
        m_lfo_button->toFront(false);
    }
}

void LayerCakeKnob::lookAndFeelChanged()
{
    apply_look_and_feel_colours();
}

void LayerCakeKnob::set_hover_changed_handler(const std::function<void(bool)>& handler)
{
    m_hover_changed_handler = handler;
}

void LayerCakeKnob::mouseEnter(const juce::MouseEvent& event)
{
    if (!m_is_hovered)
    {
        m_is_hovered = true;
        if (m_hover_changed_handler != nullptr)
            m_hover_changed_handler(true);
    }
    juce::Component::mouseEnter(event);
}

void LayerCakeKnob::mouseExit(const juce::MouseEvent& event)
{
    const auto localPos = event.getEventRelativeTo(this).getPosition();
    const bool stillInside = getLocalBounds().contains(localPos);
    if (!stillInside && m_is_hovered)
    {
        m_is_hovered = false;
        if (m_hover_changed_handler != nullptr)
            m_hover_changed_handler(false);
    }
    juce::Component::mouseExit(event);
}

void LayerCakeKnob::mouseDown(const juce::MouseEvent& event)
{
    if (m_is_editing)
    {
        DBG("LayerCakeKnob::mouseDown early return (already editing)");
        return;
    }

    if (event.mods.isPopupMenu())
    {
        if (show_context_menu(event))
        return;
    }

    // Command-click to show text editor (CLI mode)
    if (m_config.cliMode && event.mods.isCommandDown())
    {
        show_text_editor();
        DBG("LayerCakeKnob::mouseDown command-click text editor");
        return;
    }

    // Option-click on LFO indicator to clear assignment (CLI mode)
    if (m_config.cliMode && event.mods.isAltDown() && has_lfo_assignment())
    {
        const auto clickPos = event.position;
        if (!m_lfo_indicator_bounds.isEmpty() && m_lfo_indicator_bounds.contains(clickPos))
        {
            DBG("LayerCakeKnob::mouseDown option-click clearing LFO assignment");
            if (m_lfo_release_handler != nullptr)
                m_lfo_release_handler();
            repaint();
            return;
        }
    }

    juce::Component::mouseDown(event);
}

bool LayerCakeKnob::isInterestedInDragSource(const juce::DragAndDropTarget::SourceDetails& details)
{
    if (!m_config.enableLfoAssignment)
        return false;
    int lfoIndex = -1;
    juce::Colour accent;
    juce::String label;
    juce::ignoreUnused(label);
    if (m_lfo_drop_handler == nullptr)
        return false;
    return LfoDragHelpers::parse_description(details.description, lfoIndex, accent, label, false);
}

void LayerCakeKnob::itemDragEnter(const juce::DragAndDropTarget::SourceDetails& details)
{
    if (!m_config.enableLfoAssignment)
        return;
    int lfoIndex = -1;
    juce::Colour accent;
    juce::String label;
    juce::ignoreUnused(lfoIndex, label);

    if (!LfoDragHelpers::parse_description(details.description, lfoIndex, accent, label, false))
    {
        DBG("LayerCakeKnob::itemDragEnter early return (invalid payload)");
        return;
    }

    m_active_drag_colour = accent.isTransparent() ? m_lfo_highlight_colour : accent;
    m_drag_highlight = true;
    repaint();
}

void LayerCakeKnob::itemDragExit(const juce::DragAndDropTarget::SourceDetails& details)
{
    juce::ignoreUnused(details);
    if (!m_config.enableLfoAssignment)
        return;
    if (m_drag_highlight)
    {
        m_drag_highlight = false;
        repaint();
    }
}

void LayerCakeKnob::itemDropped(const juce::DragAndDropTarget::SourceDetails& details)
{
    if (!m_config.enableLfoAssignment)
        return;
    int lfoIndex = -1;
    juce::Colour accent;
    juce::String label;
    juce::ignoreUnused(accent, label);

    m_drag_highlight = false;
    repaint();

    if (!LfoDragHelpers::parse_description(details.description, lfoIndex, accent, label, true))
    {
        DBG("LayerCakeKnob::itemDropped early return (description parse failed)");
        return;
    }

    if (m_lfo_drop_handler == nullptr)
    {
        DBG("LayerCakeKnob::itemDropped early return (missing drop handler)");
        return;
    }

    m_lfo_drop_handler(*this, lfoIndex);
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
    {
        DBG("LayerCakeKnob::sliderValueChanged early return (mismatched slider)");
        return;
    }

    update_value_label();
    sync_recorder_idle_value();

    if (sweep_recorder_enabled() && m_sweep_recorder.is_recording() && !m_is_applying_loop_value)
    {
        const double now_ms = juce::Time::getMillisecondCounterHiRes();
        m_sweep_recorder.push_sample(now_ms, static_cast<float>(m_slider.getValue()));
    }

    repaint();
}

void LayerCakeKnob::sliderDragStarted(juce::Slider* slider)
{
    if (slider == &m_slider)
    {
        m_slider.setMouseDragSensitivity(static_cast<int>(LayerCakeSettings::mainKnobSensitivity));
        m_show_base_value = true;
        repaint();
        handle_touch_begin(false);
    }
    else
    {
        DBG("LayerCakeKnob::sliderDragStarted early return (mismatched slider)");
    }
}

void LayerCakeKnob::sliderDragEnded(juce::Slider* slider)
{
    if (slider == &m_slider)
    {
        m_show_base_value = false;
        repaint();
        handle_touch_end();
    }
    else
    {
        DBG("LayerCakeKnob::sliderDragEnded early return (mismatched slider)");
    }
}

void LayerCakeKnob::apply_look_and_feel_colours()
{
    auto& laf = getLookAndFeel();
    juce::Colour knobLabelColour = kSoftWhite;
    juce::Colour valueColour = kSoftWhite;
    juce::Colour accentColour = laf.findColour(juce::Slider::thumbColourId);

    if (m_custom_knob_colour.has_value())
        accentColour = m_custom_knob_colour.value();

    juce::Colour sliderBackground = laf.findColour(juce::Slider::backgroundColourId).darker(0.25f);
    if (auto* layercake = dynamic_cast<LayerCakeLookAndFeel*>(&laf))
        sliderBackground = layercake->getPanelColour().darker(0.35f);

    m_label.setColour(juce::Label::textColourId, knobLabelColour);
    m_value_label.setColour(juce::Label::textColourId, valueColour);
    m_slider.setColour(juce::Slider::thumbColourId, accentColour);
    m_slider.setColour(juce::Slider::trackColourId, accentColour.withAlpha(0.85f));
    m_slider.setColour(juce::Slider::rotarySliderFillColourId, accentColour.withAlpha(0.6f));
    m_slider.setColour(juce::Slider::rotarySliderOutlineColourId, accentColour.darker(0.55f));
    m_slider.setColour(juce::Slider::backgroundColourId, sliderBackground);

    if (m_recorder_button != nullptr)
    {
        juce::Colour idleColour = knobLabelColour.withAlpha(0.35f);
        juce::Colour armedColour = knobLabelColour.brighter(0.25f);
        juce::Colour recordingColour = juce::Colours::red;
        juce::Colour playingColour = juce::Colours::green;
        juce::Colour borderColour = valueColour;

        if (auto* layercake = dynamic_cast<LayerCakeLookAndFeel*>(&laf))
        {
            idleColour = layercake->getKnobRecorderIdleColour();
            armedColour = layercake->getKnobRecorderArmedColour();
            recordingColour = layercake->getKnobRecorderRecordingColour();
            playingColour = layercake->getKnobRecorderPlayingColour();
            borderColour = layercake->getControlAccentColour(LayerCakeLookAndFeel::ControlButtonType::Trigger);
        }

        m_recorder_button->setColour(KnobRecorderButton::idleColourId, idleColour);
        m_recorder_button->setColour(KnobRecorderButton::armedColourId, armedColour);
        m_recorder_button->setColour(KnobRecorderButton::recordingColourId, recordingColour);
        m_recorder_button->setColour(KnobRecorderButton::playingColourId, playingColour);
        m_recorder_button->setColour(KnobRecorderButton::textColourId, valueColour);
        m_recorder_button->setColour(KnobRecorderButton::borderColourId, borderColour);
    }

    if (m_lfo_button != nullptr)
    {
        const auto idleColour = accentColour.withAlpha(0.5f);
        m_lfo_button->setIdleColour(idleColour);
        if (m_lfo_button_accent.has_value())
            m_lfo_button->setAssignmentColour(m_lfo_button_accent);
    }
}

void LayerCakeKnob::timerCallback()
{
    // Update plot history if in CLI mode
    if (m_config.cliMode && !m_plot_history.empty())
    {
        double currentValue = m_slider.getValue();
        
        // If modulated, calculate actual effective value for plot
        if (has_lfo_assignment() && m_modulation_indicator_value.has_value() && !m_show_base_value)
        {
            const double span = m_config.maxValue - m_config.minValue;
            if (span > 0.0)
            {
                const double lfoNormalized = static_cast<double>(m_modulation_indicator_value.value());
                const double lfoOffset = (lfoNormalized * 2.0) - 1.0;
                const double baseValue = m_slider.getValue();
                const double baseNormalized = juce::jlimit(0.0, 1.0, (baseValue - m_config.minValue) / span);
                const double modNormalized = juce::jlimit(0.0, 1.0, baseNormalized + lfoOffset * 0.5);
                currentValue = m_config.minValue + modNormalized * span;
            }
        }

        // Normalize for plot
        const double range = m_config.maxValue - m_config.minValue;
        float normalized = 0.0f;
        if (range != 0.0)
        {
            normalized = static_cast<float>((currentValue - m_config.minValue) / range);
            normalized = juce::jlimit(0.0f, 1.0f, normalized);
        }
        
        m_plot_history[static_cast<size_t>(m_plot_write_index)] = normalized;
        m_plot_write_index = (m_plot_write_index + 1) % static_cast<int>(m_plot_history.size());
        
        repaint();
    }

    const bool shouldLoop = (m_recorder_state == RecorderState::Looping) && m_sweep_recorder.is_playing();
    if (shouldLoop)
    {
        const double now_ms = juce::Time::getMillisecondCounterHiRes();
        const float loopValue = m_sweep_recorder.get_value(now_ms);
        const juce::ScopedValueSetter<bool> playbackSetter(m_is_applying_loop_value, true);
        m_slider.setValue(loopValue, juce::sendNotificationSync);
    }

    if (m_recorder_state == RecorderState::Armed)
    {
        const double now_ms = juce::Time::getMillisecondCounterHiRes();
        if (now_ms - m_last_blink_toggle_ms >= kBlinkIntervalMs)
        {
            m_last_blink_toggle_ms = now_ms;
            m_blink_visible = !m_blink_visible;
            repaint();
        }
    }
    else if (m_blink_visible)
    {
        m_blink_visible = false;
        repaint();
    }

    if (!shouldLoop && !m_config.cliMode) // Keep timer running for CLI plot
        update_timer_activity();
}

bool LayerCakeKnob::show_context_menu(const juce::MouseEvent& event)
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
                                 DBG("LayerCakeKnob::show_context_menu midi learn action early return (no midi manager)");
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
                                     DBG("LayerCakeKnob::show_context_menu clear midi action early return (no midi manager)");
                                     return;
                                 }
                                 m_midi_manager->clearMapping(m_config.parameterId);
                                 repaint();
                                 if (auto* topLevel = getTopLevelComponent())
                                     topLevel->repaint();
                             }));
        }
    }

    // LFO assignment clear option
    if (m_config.enableLfoAssignment && has_lfo_assignment())
    {
        if (menu.getNumItems() > 0)
            menu.addSeparator();

        menu.addItem(juce::PopupMenu::Item("Clear LFO")
                         .setAction([this]() {
                             DBG("LayerCakeKnob::show_context_menu clearing LFO assignment");
                             if (m_lfo_release_handler != nullptr)
                                 m_lfo_release_handler();
                             repaint();
                         }));
    }

    if (sweep_recorder_enabled())
    {
        if (menu.getNumItems() > 0)
            menu.addSeparator();
        const bool canRecord = m_recorder_state != RecorderState::Recording;
        const bool canClear = m_recorder_state != RecorderState::Idle;

        menu.addItem(juce::PopupMenu::Item("Record sweep")
                         .setEnabled(canRecord)
                         .setAction([this]() { arm_sweep_recorder(); }));

        menu.addItem(juce::PopupMenu::Item("Clear sweep")
                         .setEnabled(canClear)
                         .setAction([this]() { clear_sweep_recorder("menu"); }));
    }

    if (m_context_menu_builder != nullptr)
        m_context_menu_builder(menu);

    if (menu.getNumItems() == 0)
    {
        DBG("LayerCakeKnob::show_context_menu early return (no items to show)");
        return false;
    }

    juce::Rectangle<int> screenArea(event.getScreenX(), event.getScreenY(), 1, 1);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this)
                                              .withTargetScreenArea(screenArea));
    return true;
}

void LayerCakeKnob::arm_sweep_recorder()
{
    if (sweep_recorder_enabled())
    {
        m_sweep_recorder.arm();
        update_recorder_state(RecorderState::Armed);
        update_blink_state(true);
        DBG("LayerCakeKnob::arm_sweep_recorder armed");
    }
    else
    {
        DBG("LayerCakeKnob::arm_sweep_recorder skipped (recorder disabled)");
    }
}

void LayerCakeKnob::clear_sweep_recorder(const juce::String& reason)
{
    if (sweep_recorder_enabled())
    {
        DBG("LayerCakeKnob::clear_sweep_recorder reason=" + reason);
        m_sweep_recorder.clear();
        update_recorder_state(RecorderState::Idle);
        update_blink_state(true);
    }
    else
    {
        DBG("LayerCakeKnob::clear_sweep_recorder skipped (recorder disabled)");
    }
}

void LayerCakeKnob::update_recorder_state(RecorderState next_state)
{
    if (m_recorder_state != next_state)
    {
        m_recorder_state = next_state;
        update_recorder_button();
        update_timer_activity();
        repaint();
    }
}

void LayerCakeKnob::begin_sweep_recording(double now_ms)
{
    if (sweep_recorder_enabled())
    {
        m_sweep_recorder.begin_record(now_ms);
        m_sweep_recorder.push_sample(now_ms, static_cast<float>(m_slider.getValue()));
        update_recorder_state(RecorderState::Recording);
    }
    else
    {
        DBG("LayerCakeKnob::begin_sweep_recording skipped (recorder disabled)");
    }
}

void LayerCakeKnob::finish_sweep_recording()
{
    if (sweep_recorder_enabled())
    {
        const double now_ms = juce::Time::getMillisecondCounterHiRes();
        m_sweep_recorder.push_sample(now_ms, static_cast<float>(m_slider.getValue()));
        m_sweep_recorder.end_record();

        if (m_sweep_recorder.is_playing())
            update_recorder_state(RecorderState::Looping);
        else
            update_recorder_state(RecorderState::Idle);
    }
    else
    {
        DBG("LayerCakeKnob::finish_sweep_recording skipped (recorder disabled)");
    }
}

void LayerCakeKnob::handle_touch_begin(bool initiated_by_button)
{
    if (sweep_recorder_enabled())
    {
        if (m_recorder_state == RecorderState::Looping)
            clear_sweep_recorder("touch");

        if (m_recorder_state == RecorderState::Armed)
            begin_sweep_recording(juce::Time::getMillisecondCounterHiRes());
    }
    else
    {
        DBG("LayerCakeKnob::handle_touch_begin skipped (recorder disabled)");
    }

    juce::ignoreUnused(initiated_by_button);
}

void LayerCakeKnob::handle_touch_end()
{
    if (sweep_recorder_enabled())
    {
        if (m_recorder_state == RecorderState::Recording)
            finish_sweep_recording();
    }
    else
    {
        DBG("LayerCakeKnob::handle_touch_end skipped (recorder disabled)");
    }
}

void LayerCakeKnob::update_recorder_button()
{
    if (m_recorder_button != nullptr)
    {
        auto status = KnobRecorderButton::Status::Idle;
        switch (m_recorder_state)
        {
            case RecorderState::Armed: status = KnobRecorderButton::Status::Armed; break;
            case RecorderState::Recording: status = KnobRecorderButton::Status::Recording; break;
            case RecorderState::Looping: status = KnobRecorderButton::Status::Playing; break;
            case RecorderState::Idle:
            default: status = KnobRecorderButton::Status::Idle; break;
        }

        m_recorder_button->setStatus(status);
    }
}

void LayerCakeKnob::update_timer_activity()
{
    // Always keep timer running in CLI mode for plot updates
    if (m_config.cliMode)
    {
        if (!isTimerRunning())
            startTimerHz(60);
        return;
    }

    if (sweep_recorder_enabled())
    {
        const bool needsTimer = (m_recorder_state == RecorderState::Armed)
                             || (m_recorder_state == RecorderState::Looping && m_sweep_recorder.is_playing());
        if (needsTimer && !isTimerRunning())
            startTimerHz(60);
        else if (!needsTimer && isTimerRunning())
            stopTimer();
    }
}

void LayerCakeKnob::update_blink_state(bool force_reset)
{
    if (force_reset)
        m_blink_visible = false;

    if (m_recorder_state == RecorderState::Armed)
    {
        m_last_blink_toggle_ms = juce::Time::getMillisecondCounterHiRes();
        m_blink_visible = true;
    }
}

void LayerCakeKnob::sync_recorder_idle_value()
{
    if (sweep_recorder_enabled())
        m_sweep_recorder.set_idle_value(static_cast<float>(m_slider.getValue()));
}

void LayerCakeKnob::set_context_menu_builder(const std::function<void(juce::PopupMenu&)>& builder)
{
    m_context_menu_builder = builder;
}

void LayerCakeKnob::set_lfo_drop_handler(const std::function<void(LayerCakeKnob&, int)>& handler)
{
    m_lfo_drop_handler = handler;
}

void LayerCakeKnob::set_lfo_highlight_colour(juce::Colour colour)
{
    m_lfo_highlight_colour = colour;
    m_active_drag_colour = colour;
}

void LayerCakeKnob::set_modulation_indicator(std::optional<float> normalizedValue, juce::Colour colour)
{
    if (!normalizedValue.has_value())
    {
        if (m_modulation_indicator_value.has_value())
        {
            m_modulation_indicator_value.reset();
            repaint();
        }
        return;
    }

    const float clamped = juce::jlimit(0.0f, 1.0f, normalizedValue.value());
    const bool changed = !m_modulation_indicator_value.has_value()
                      || std::abs(m_modulation_indicator_value.value() - clamped) > 0.001f
                      || m_modulation_indicator_colour != colour;

    if (!changed)
        return;

    m_modulation_indicator_value = clamped;
    m_modulation_indicator_colour = colour;
    repaint();
}

void LayerCakeKnob::clear_modulation_indicator()
{
    if (m_modulation_indicator_value.has_value())
    {
        m_modulation_indicator_value.reset();
        repaint();
    }
}

void LayerCakeKnob::set_lfo_assignment_index(int index)
{
    m_lfo_assignment_index.store(index, std::memory_order_relaxed);
    refresh_lfo_button_state();
    update_lfo_tooltip();
}

void LayerCakeKnob::set_lfo_button_accent(std::optional<juce::Colour> accent)
{
    m_lfo_button_accent = accent;
    if (m_lfo_button != nullptr)
    {
        m_lfo_button->setAssignmentColour(accent);
        refresh_lfo_button_state();
    }
}

void LayerCakeKnob::set_lfo_release_handler(const std::function<void()>& handler)
{
    m_lfo_release_handler = handler;
}

void LayerCakeKnob::set_knob_colour(juce::Colour colour)
{
    m_custom_knob_colour = colour;
    apply_look_and_feel_colours();
    repaint();
}

void LayerCakeKnob::clear_knob_colour()
{
    if (!m_custom_knob_colour.has_value())
        return;
    m_custom_knob_colour.reset();
    apply_look_and_feel_colours();
    repaint();
}

void LayerCakeKnob::refresh_lfo_button_state()
{
    if (m_lfo_button == nullptr)
        return;

    const bool assigned = has_lfo_assignment();
    m_lfo_button->setHasAssignment(assigned);
    m_lfo_button->setAssignmentColour(m_lfo_button_accent);
    m_lfo_button->setEnabled(assigned);
}

void LayerCakeKnob::update_lfo_tooltip()
{
    if (!m_config.cliMode || !m_config.enableLfoAssignment)
    {
        setTooltip({});
        return;
    }

    if (has_lfo_assignment())
        setTooltip("Option-click LFO indicator to clear");
    else
        setTooltip({});
}

void LayerCakeKnob::show_text_editor()
{
    if (m_is_editing || !m_config.cliMode) return;
    
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
                   .withHeight(15.0f);
    m_text_editor->setFont(juce::Font(fontOpts));
    
    const auto accent = m_slider.findColour(juce::Slider::thumbColourId, true);
    const juce::Colour editorAccent = (has_lfo_assignment() && m_lfo_button_accent.has_value()) 
        ? m_lfo_button_accent.value() 
        : accent;
    
    m_text_editor->setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff2a2a2a));
    m_text_editor->setColour(juce::TextEditor::textColourId, juce::Colours::white);
    m_text_editor->setColour(juce::TextEditor::highlightColourId, editorAccent.withAlpha(0.4f));
    m_text_editor->setColour(juce::TextEditor::outlineColourId, editorAccent);
    m_text_editor->setColour(juce::TextEditor::focusedOutlineColourId, editorAccent);
    
    // Set initial text (without suffix) - use the current displayed value
    double currentValue = m_slider.getValue();
    juce::String initialText;
    
    const bool isPercentDisplay = m_config.displayAsPercent && 
                                  std::abs(m_config.minValue) < 0.001 && 
                                  std::abs(m_config.maxValue - 1.0) < 0.001;
    
    if (isPercentDisplay)
    {
        initialText = juce::String(static_cast<int>(std::round(currentValue * 99.0)));
    }
    else if (m_config.decimals == 0)
    {
        initialText = juce::String(static_cast<int>(std::round(currentValue)));
    }
    else
    {
        initialText = juce::String(currentValue, m_config.decimals);
    }
    
    m_text_editor->setText(initialText, false);
    m_text_editor->selectAll();
    m_text_editor->addListener(this);
    
    addAndMakeVisible(m_text_editor.get());
    m_text_editor->setBounds(getLocalBounds());
    m_text_editor->grabKeyboardFocus();
    
    repaint();
}

void LayerCakeKnob::hide_text_editor(bool apply)
{
    if (!m_is_editing || m_text_editor == nullptr) return;
    
    if (apply)
    {
        const double newValue = parse_input(m_text_editor->getText());
        const double clampedValue = juce::jlimit(m_config.minValue, m_config.maxValue, newValue);
        m_slider.setValue(clampedValue, juce::sendNotificationSync);
    }
    
    m_text_editor->removeListener(this);
    removeChildComponent(m_text_editor.get());
    m_text_editor.reset();
    m_is_editing = false;
    
    repaint();
}

void LayerCakeKnob::textEditorReturnKeyPressed(juce::TextEditor& editor)
{
    juce::ignoreUnused(editor);
    hide_text_editor(true);
}

void LayerCakeKnob::textEditorEscapeKeyPressed(juce::TextEditor& editor)
{
    juce::ignoreUnused(editor);
    hide_text_editor(false);
}

void LayerCakeKnob::textEditorFocusLost(juce::TextEditor& editor)
{
    juce::ignoreUnused(editor);
    hide_text_editor(true);
}

double LayerCakeKnob::parse_input(const juce::String& text) const
{
    const double inputValue = text.getDoubleValue();
    
    const bool isPercentDisplay = m_config.displayAsPercent && 
                                  std::abs(m_config.minValue) < 0.001 && 
                                  std::abs(m_config.maxValue - 1.0) < 0.001;
    
    if (isPercentDisplay)
    {
        // Convert 0-99 input back to 0-1
        return juce::jlimit(0.0, 1.0, inputValue / 99.0);
    }
    
    return inputValue;
}

void LayerCakeKnob::onFocusGain()
{
    m_is_keyboard_focused = true;
    repaint();
    DBG("LayerCakeKnob::onFocusGain " + m_config.parameterId);
}

void LayerCakeKnob::onFocusLost()
{
    m_is_keyboard_focused = false;
    repaint();
    // Ensure text editor is closed if open
    if (m_is_editing)
        hide_text_editor(true);
}

bool LayerCakeKnob::handleKeyPressed(const juce::KeyPress& key)
{
    if (m_is_editing)
    {
        // If we're editing text, let the text editor handle it
        return false; 
    }

    if (key.getKeyCode() == juce::KeyPress::returnKey)
    {
        if (m_config.cliMode)
        {
            show_text_editor();
            return true;
        }
    }
    
    // Arrow keys for adjustment
    double step = m_config.interval;
    if (key.getModifiers().isShiftDown()) step *= 10.0;
    if (key.getModifiers().isAltDown()) step *= 0.1;
    
    if (key.getKeyCode() == juce::KeyPress::upKey || key.getKeyCode() == juce::KeyPress::rightKey)
    {
        m_slider.setValue(m_slider.getValue() + step, juce::sendNotificationSync);
        return true;
    }
    else if (key.getKeyCode() == juce::KeyPress::downKey || key.getKeyCode() == juce::KeyPress::leftKey)
    {
        m_slider.setValue(m_slider.getValue() - step, juce::sendNotificationSync);
        return true;
    }
    
    // Bracket keys for stepping
    auto charCode = key.getTextCharacter();
    if (charCode == ']' || charCode == '.')
    {
        m_slider.setValue(m_slider.getValue() + step, juce::sendNotificationSync);
        return true;
    }
    else if (charCode == '[' || charCode == ',')
    {
        m_slider.setValue(m_slider.getValue() - step, juce::sendNotificationSync);
        return true;
    }

    return false;
}

juce::String LayerCakeKnob::getValueString() const
{
    return format_cli_value();
}

} // namespace LayerCakeApp
