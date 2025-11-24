#include "LayerCakeKnob.h"
#include "LayerCakeLookAndFeel.h"
#include "LfoDragHelpers.h"
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

    register_midi_parameter();
    update_value_label();
    apply_look_and_feel_colours();
}

LayerCakeKnob::~LayerCakeKnob()
{
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

void LayerCakeKnob::resized()
{
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

    auto valueBounds = bounds.reduced(valuePadding);
    m_slider.setBounds(valueBounds);
    m_value_label.setBounds(valueBounds.reduced(valueInset));

    if (m_recorder_button != nullptr)
    {
        juce::Rectangle<int> buttonBounds(recorderButtonSize, recorderButtonSize);
        const int targetX = valueBounds.getRight() - recorderButtonMargin - recorderButtonSize;
        const int targetY = valueBounds.getY() + recorderButtonMargin;
        buttonBounds.setPosition(targetX, targetY);
        m_recorder_button->setBounds(buttonBounds);
        m_recorder_button->toFront(false);
    }

    if (m_lfo_button != nullptr)
    {
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

void LayerCakeKnob::mouseDown(const juce::MouseEvent& event)
{
    if (event.mods.isPopupMenu())
    {
        if (show_context_menu(event))
        return;
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

    if (!shouldLoop)
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

} // namespace LayerCakeApp


