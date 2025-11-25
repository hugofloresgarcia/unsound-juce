#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <array>
#include <vector>
#include <flowerjuce/LayerCakeEngine/LayerCakeEngine.h>
#include <flowerjuce/LayerCakeEngine/LayerCakeTypes.h>

struct LayerCakePresetData
{
    static constexpr size_t kNumLfos = 8;
    struct LfoSlotData
    {
        // Custom label (empty = use default "LFO N")
        juce::String label;
        bool enabled{true};
        
        // Basic parameters
        int mode{0};
        float rate_hz{0.5f};
        float depth{0.5f};
        bool tempo_sync{false};
        float clock_division{1.0f};
        int pattern_length{0};
        std::vector<float> pattern_buffer;
        
        // PNW-style waveform shaping
        float level{1.0f};
        float width{0.5f};
        float phase_offset{0.0f};
        float delay{0.0f};
        int delay_div{1};
        
        // Humanization
        float slop{0.0f};
        
        // Euclidean rhythm
        int euclidean_steps{0};
        int euclidean_triggers{0};
        int euclidean_rotation{0};
        
        // Random skip
        float random_skip{0.0f};
        
        // Loop
        int loop_beats{0};
        
        // Polarity
        bool bipolar{true};  // true = -1 to 1, false = 0 to 1
        
        // Random seed for reproducible patterns
        uint64_t random_seed{0};
    };

    float master_gain_db{0.0f};
    GrainState manual_state;
    int record_layer{0};
    float spread_amount{0.0f};
    float reverse_probability{0.0f};
    bool clock_enabled{false};
    juce::NamedValueSet knob_values;
    std::array<LfoSlotData, kNumLfos> lfo_slots{};
    juce::NamedValueSet lfo_assignments;
};

using LayerBufferArray = std::array<LayerBufferSnapshot, LayerCakeEngine::kNumLayers>;

class LayerCakeLibraryManager
{
public:
    LayerCakeLibraryManager();

    void refresh();

    const juce::StringArray& get_palettes() const { return m_palette_names; }
    const juce::StringArray& get_scenes() const { return m_scene_names; }
    const juce::StringArray& get_knobsets() const { return m_knobset_names; }
    const juce::StringArray& get_lfo_presets() const { return m_lfo_preset_names; }

    bool save_palette(const juce::String& name, const LayerBufferArray& layers);
    bool load_palette(const juce::String& name, LayerBufferArray& out_layers) const;
    bool delete_palette(const juce::String& name);

    bool save_knobset(const juce::String& name, const LayerCakePresetData& data);
    bool load_knobset(const juce::String& name, LayerCakePresetData& out_data) const;
    bool delete_knobset(const juce::String& name);

    bool save_scene(const juce::String& name,
                    const LayerCakePresetData& data,
                    const LayerBufferArray& layers);
    bool load_scene(const juce::String& name,
                    LayerCakePresetData& out_data,
                    LayerBufferArray& out_layers) const;
    bool delete_scene(const juce::String& name);

    bool save_lfo_preset(const juce::String& name, const LayerCakePresetData::LfoSlotData& slot);
    bool load_lfo_preset(const juce::String& name, LayerCakePresetData::LfoSlotData& out_slot) const;
    bool delete_lfo_preset(const juce::String& name);

private:
    juce::File palettes_root() const;
    juce::File scenes_root() const;
    juce::File knobsets_root() const;
    juce::File lfo_presets_root() const;
    juce::File palette_folder(const juce::String& name) const;
    juce::File scene_folder(const juce::String& name) const;
    juce::File knobset_file(const juce::String& name) const;
    juce::File lfo_preset_file(const juce::String& name) const;
    static juce::String sanitize_name(const juce::String& name);

    bool write_layers(const juce::File& folder, const LayerBufferArray& layers) const;
    bool read_layers(const juce::File& folder, LayerBufferArray& out_layers) const;

    void refresh_palettes();
    void refresh_scenes();
    void refresh_knobsets();
    void refresh_lfo_presets();

    juce::StringArray m_palette_names;
    juce::StringArray m_scene_names;
    juce::StringArray m_knobset_names;
    juce::StringArray m_lfo_preset_names;
    juce::File m_root;
};
