#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <flowerjuce/DSP/LfoUGen.h>
#include <flowerjuce/Components/MidiLearnManager.h>
#include "../LayerCakeLookAndFeel.h"
#include "../LayerCakeLibraryManager.h"
#include "../focus/FocusableTarget.h"
#include <functional>
#include <vector>

namespace LayerCakeApp
{

/**
 * A CLI-style parameter row: "key: value" with mouse drag to adjust.
 * Supports vertical drag to change value, double-click to reset, and cmd-click text edit.
 * For 0-1 ranges, displays as 0-99 integer for easier reading.
 */
class LfoParamRow : public juce::Component,
                    private juce::TextEditor::Listener
{
public:
    struct Config
    {
        juce::String key;
        juce::String parameterId;  // For MIDI learn
        double minValue{0.0};
        double maxValue{1.0};
        double defaultValue{0.0};
        double interval{0.01};
        juce::String suffix;
        int decimals{2};  // Decimal places for display
        bool displayAsPercent{false};  // If true and range is 0-1, display as 0-99
    };

    LfoParamRow(const Config& config, Shared::MidiLearnManager* midiManager);
    ~LfoParamRow() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseDoubleClick(const juce::MouseEvent& event) override;

    double get_value() const noexcept { return m_value; }
    void set_value(double value, bool notify = true);
    void set_accent_colour(juce::Colour colour) { m_accent = colour; repaint(); }
    void set_on_value_changed(std::function<void()> callback) { m_on_value_changed = std::move(callback); }
    const juce::String& parameter_id() const { return m_config.parameterId; }

private:
    juce::String format_value() const;
    void register_midi_parameter();
    bool show_context_menu(const juce::MouseEvent& event);
    void show_text_editor();
    void hide_text_editor(bool apply);
    void textEditorReturnKeyPressed(juce::TextEditor& editor) override;
    void textEditorEscapeKeyPressed(juce::TextEditor& editor) override;
    void textEditorFocusLost(juce::TextEditor& editor) override;
    double parse_input(const juce::String& text) const;
    bool is_percent_display() const;

    Config m_config;
    Shared::MidiLearnManager* m_midi_manager{nullptr};
    juce::String m_registered_parameter_id;
    double m_value{0.0};
    double m_drag_start_value{0.0};
    int m_drag_start_y{0};
    bool m_is_dragging{false};
    bool m_is_editing{false};
    juce::Colour m_accent{juce::Colours::cyan};
    std::function<void()> m_on_value_changed;
    std::unique_ptr<juce::TextEditor> m_text_editor;
};

class LayerCakeLfoWidget : public juce::Component,
                           public layercake::FocusableTarget,
                           private juce::ComboBox::Listener,
                           private juce::Timer,
                           private juce::Label::Listener
{
public:
    struct PresetHandlers
    {
        std::function<juce::StringArray()> getPresetNames;
        std::function<bool(const juce::String&, LayerCakePresetData::LfoSlotData&)> loadPreset;
        std::function<bool(const juce::String&, const LayerCakePresetData::LfoSlotData&)> savePreset;
    };

    LayerCakeLfoWidget(int lfo_index, 
                       flower::LayerCakeLfoUGen& generator, 
                       juce::Colour accent,
                       Shared::MidiLearnManager* midiManager = nullptr);
    ~LayerCakeLfoWidget() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseEnter(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;

    juce::Colour get_accent_colour() const noexcept { return m_accent_colour; }
    int get_lfo_index() const noexcept { return m_lfo_index; }

    void refresh_wave_preview();
    void set_drag_label(const juce::String& label);
    juce::String get_custom_label() const { return m_custom_label; }
    void set_custom_label(const juce::String& label);
    void set_on_settings_changed(std::function<void()> callback);
    void set_on_label_changed(std::function<void(const juce::String&)> callback);
    void set_preset_handlers(PresetHandlers handlers);
    void set_on_enabled_changed(std::function<void(bool)> callback);
    void set_enabled(bool enabled, bool notifyListeners = true);
    bool is_enabled() const noexcept { return m_enabled; }
    void sync_controls_from_generator();
    void set_tempo_provider(std::function<double()> tempo_bpm_provider);
    void set_on_hover_changed(std::function<void(bool)> callback);
    void set_current_value(float value);  // 0-1 for LED display

    // layercake::FocusableTarget
    juce::String getFocusID() const override;
    juce::String getDisplayName() const override;
    void onFocusGain() override;
    void onFocusLost() override;
    bool handleKeyPressed(const juce::KeyPress& key) override;
    juce::String getValueString() const override;
    juce::Component* getComponent() override { return this; }

private:
    class WavePreview : public juce::Component
    {
    public:
        explicit WavePreview(LayerCakeLfoWidget& owner);
        void paint(juce::Graphics& g) override;
        void resized() override;
        void set_points(const std::vector<float>& points);
        void mouseDown(const juce::MouseEvent& event) override;
        void mouseDrag(const juce::MouseEvent& event) override;
        void mouseUp(const juce::MouseEvent& event) override;

    private:
        void begin_drag(const juce::MouseEvent& event);
        LayerCakeLfoWidget& m_owner;
        std::vector<float> m_points;
        bool m_is_dragging{false};
    };

    void comboBoxChanged(juce::ComboBox* comboBoxThatHasChanged) override;
    void labelTextChanged(juce::Label* labelThatHasChanged) override;
    void editorShown(juce::Label* label, juce::TextEditor& editor) override;
    void update_generator_settings();
    void notify_settings_changed();
    void timerCallback() override;
    void mouseDown(const juce::MouseEvent& event) override;
    double get_tempo_bpm() const;
    void update_controls_visibility();
    void go_to_page(int page);
    void next_page();
    void prev_page();
    juce::String get_display_label() const;
    void show_preset_menu();
    void prompt_save_preset();
    void attempt_load_preset(const juce::String& presetName);
    LayerCakePresetData::LfoSlotData capture_slot_data() const;
    void apply_slot_data(const LayerCakePresetData::LfoSlotData& data);
    void update_preset_button_state();

    class SmallButtonLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        juce::Font getTextButtonFont(juce::TextButton& button, int buttonHeight) override;
        void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                  const juce::Colour& backgroundColour,
                                  bool shouldDrawButtonAsHighlighted,
                                  bool shouldDrawButtonAsDown) override;
        void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                            bool shouldDrawButtonAsHighlighted,
                            bool shouldDrawButtonAsDown) override;
        void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                          int buttonX, int buttonY, int buttonW, int buttonH,
                          juce::ComboBox& box) override;
        juce::Font getComboBoxFont(juce::ComboBox& box) override;
        void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override;
    };

    flower::LayerCakeLfoUGen& m_generator;
    Shared::MidiLearnManager* m_midi_manager{nullptr};
    juce::Colour m_accent_colour;
    int m_lfo_index{0};
    juce::Label m_title_label;
    juce::ComboBox m_mode_selector;
    
    // CLI-style parameter rows
    enum class ParamSlot : size_t
    {
        Div = 0,
        Level,
        Width,
        Loop,
        RandomSkip,
        Slop,
        Delay,
        Phase,
        DelayDivision,
        EuclideanSteps,
        EuclideanTriggers,
        EuclideanRotation,
        Count
    };

    static constexpr size_t kParamCount = static_cast<size_t>(ParamSlot::Count);

    std::vector<std::unique_ptr<LfoParamRow>> m_params;
    
    std::unique_ptr<WavePreview> m_wave_preview;
    juce::String m_drag_label;
    juce::String m_custom_label;  // User-editable label (empty = use default)
    std::function<void()> m_settings_changed_callback;
    std::function<void(const juce::String&)> m_label_changed_callback;
    PresetHandlers m_preset_handlers;
    juce::TextButton m_preset_button;
    std::function<void(bool)> m_enabled_changed_callback;
    bool m_enabled{true};
    juce::TextButton m_prev_page_button;
    juce::TextButton m_next_page_button;
    juce::Label m_page_label;
    SmallButtonLookAndFeel m_button_lnf;
    int m_current_page{0};
    static constexpr int kParamsPerPage = 8;
    std::function<double()> m_tempo_bpm_provider;
    std::function<void(bool)> m_hover_changed_callback;
    bool m_is_hovered{false};
    bool m_is_keyboard_focused{false};
    float m_current_lfo_value{0.0f};  // For LED display
    juce::Rectangle<int> m_led_bounds;
    
    // Cached last values for change detection
    float m_last_level{-1.0f};
    int m_last_mode{-1};
    float m_last_clock_div{-1.0f};
};

} // namespace LayerCakeApp

