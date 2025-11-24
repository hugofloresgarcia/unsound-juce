#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <array>
#include <flowerjuce/LayerCakeEngine/LayerCakeEngine.h>
#include <flowerjuce/LayerCakeEngine/PatternClock.h>
#include <flowerjuce/LayerCakeEngine/LayerCakeTypes.h>

struct LayerCakePresetData
{
    static constexpr size_t kNumLfos = 8;
    struct LfoSlotData
    {
        int mode{0};
        float rate_hz{0.5f};
        float depth{0.5f};
        bool tempo_sync{false};
    };

    float master_gain_db{0.0f};
    GrainState manual_state;
    int record_layer{0};
    PatternSnapshot pattern_snapshot;
    float pattern_subdivision{0.0f};
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
    const juce::StringArray& get_patterns() const { return m_pattern_names; }
    const juce::StringArray& get_scenes() const { return m_scene_names; }
    const juce::StringArray& get_knobsets() const { return m_knobset_names; }

    bool save_palette(const juce::String& name, const LayerBufferArray& layers);
    bool load_palette(const juce::String& name, LayerBufferArray& out_layers) const;
    bool delete_palette(const juce::String& name);

    bool save_pattern(const juce::String& name, const LayerCakePresetData& data);
    bool load_pattern(const juce::String& name, LayerCakePresetData& out_data) const;
    bool delete_pattern(const juce::String& name);

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

private:
    juce::File palettes_root() const;
    juce::File patterns_root() const;
    juce::File scenes_root() const;
    juce::File knobsets_root() const;
    juce::File palette_folder(const juce::String& name) const;
    juce::File scene_folder(const juce::String& name) const;
    juce::File pattern_file(const juce::String& name) const;
    juce::File knobset_file(const juce::String& name) const;
    static juce::String sanitize_name(const juce::String& name);

    bool write_layers(const juce::File& folder, const LayerBufferArray& layers) const;
    bool read_layers(const juce::File& folder, LayerBufferArray& out_layers) const;

    void refresh_palettes();
    void refresh_patterns();
    void refresh_scenes();
    void refresh_knobsets();

    juce::StringArray m_palette_names;
    juce::StringArray m_pattern_names;
    juce::StringArray m_scene_names;
    juce::StringArray m_knobset_names;
    juce::File m_root;
};


