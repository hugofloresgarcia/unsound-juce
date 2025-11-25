#include "LayerCakeLibraryManager.h"
#include <juce_core/juce_core.h>

namespace
{
constexpr const char* kLayerCakeFolderName = "layercake";
constexpr const char* kPalettesFolderName = "palettes";
// Patterns folder removed
constexpr const char* kScenesFolderName = "scenes";
constexpr const char* kKnobsetsFolderName = "knobsets";
constexpr const char* kSceneJsonName = "scene.json";
constexpr const char* kPatternExtension = ".json";

juce::File ensure_directory(const juce::File& folder)
{
    if (!folder.exists())
        folder.createDirectory();
    return folder;
}

juce::var grain_state_to_var(const GrainState& state)
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty("loopStartSeconds", state.loop_start_seconds);
    obj->setProperty("durationMs", state.duration_ms);
    obj->setProperty("rateSemitones", state.rate_semitones);
    obj->setProperty("envAttackMs", state.env_attack_ms);
    obj->setProperty("envReleaseMs", state.env_release_ms);
    obj->setProperty("playForward", state.play_forward);
    obj->setProperty("layer", state.layer);
    obj->setProperty("pan", state.pan);
    obj->setProperty("shouldTrigger", state.should_trigger);
    return obj;
}

bool grain_state_from_var(const juce::var& value, GrainState& out_state)
{
    if (!value.isObject())
    {
        DBG("LayerCakeLibraryManager::grain_state_from_var invalid value");
        return false;
    }

    auto* obj = value.getDynamicObject();
    out_state.loop_start_seconds = static_cast<float>(obj->getProperty("loopStartSeconds"));
    out_state.duration_ms = static_cast<float>(obj->getProperty("durationMs"));
    out_state.rate_semitones = static_cast<float>(obj->getProperty("rateSemitones"));
    out_state.env_attack_ms = static_cast<float>(obj->getProperty("envAttackMs"));
    out_state.env_release_ms = static_cast<float>(obj->getProperty("envReleaseMs"));
    out_state.play_forward = obj->getProperty("playForward");
    out_state.layer = static_cast<int>(obj->getProperty("layer"));
    out_state.pan = static_cast<float>(obj->getProperty("pan"));
    out_state.should_trigger = static_cast<bool>(obj->getProperty("shouldTrigger"));
    return true;
}

juce::var knob_values_to_var(const juce::NamedValueSet& knobs)
{
    auto* obj = new juce::DynamicObject();
    for (const auto& entry : knobs)
    {
        obj->setProperty(entry.name, static_cast<double>(entry.value));
    }
    return obj;
}

void knob_values_from_var(const juce::var& value, juce::NamedValueSet& out_knobs)
{
    out_knobs.clear();
    if (!value.isObject())
        return;

    if (auto* obj = value.getDynamicObject())
    {
        const auto& props = obj->getProperties();
        for (const auto& entry : props)
            out_knobs.set(entry.name, static_cast<double>(entry.value));
    }
}

using LfoSlotArray = std::array<LayerCakePresetData::LfoSlotData, LayerCakePresetData::kNumLfos>;

juce::var lfo_slots_to_var(const LfoSlotArray& slots)
{
    juce::Array<juce::var> serialized;
    for (const auto& slot : slots)
    {
        auto* obj = new juce::DynamicObject();
        // Basic parameters
        obj->setProperty("mode", slot.mode);
        obj->setProperty("rateHz", slot.rate_hz);
        obj->setProperty("depth", slot.depth);
        obj->setProperty("tempoSync", slot.tempo_sync);
        obj->setProperty("clockDiv", slot.clock_division);
        obj->setProperty("patternLen", slot.pattern_length);
        
        juce::Array<juce::var> buffer;
        for (float v : slot.pattern_buffer)
            buffer.add(v);
        obj->setProperty("buffer", buffer);
        
        // PNW-style waveform shaping
        obj->setProperty("level", slot.level);
        obj->setProperty("width", slot.width);
        obj->setProperty("phaseOffset", slot.phase_offset);
        obj->setProperty("delay", slot.delay);
        obj->setProperty("delayDiv", slot.delay_div);
        
        // Humanization
        obj->setProperty("slop", slot.slop);
        
        // Euclidean rhythm
        obj->setProperty("euclideanSteps", slot.euclidean_steps);
        obj->setProperty("euclideanTriggers", slot.euclidean_triggers);
        obj->setProperty("euclideanRotation", slot.euclidean_rotation);
        
        // Random skip
        obj->setProperty("randomSkip", slot.random_skip);
        
        // Loop
        obj->setProperty("loopBeats", slot.loop_beats);
        
        // Random seed
        obj->setProperty("randomSeed", static_cast<juce::int64>(slot.random_seed));
        
        serialized.add(obj);
    }
    return serialized;
}

void lfo_slots_from_var(const juce::var& value, LfoSlotArray& out_slots)
{
    if (!value.isArray())
        return;

    auto* array = value.getArray();
    const int count = juce::jmin(static_cast<int>(out_slots.size()), array->size());
    for (int i = 0; i < count; ++i)
    {
        auto slotVar = array->getReference(i);
        if (!slotVar.isObject())
            continue;
        auto* obj = slotVar.getDynamicObject();
        auto& slot = out_slots[static_cast<size_t>(i)];
        
        // Basic parameters
        slot.mode = static_cast<int>(obj->getProperty("mode"));
        slot.rate_hz = static_cast<float>(obj->getProperty("rateHz"));
        slot.depth = static_cast<float>(obj->getProperty("depth"));
        
        slot.tempo_sync = obj->hasProperty("tempoSync") 
            ? static_cast<bool>(obj->getProperty("tempoSync")) : false;
        slot.clock_division = obj->hasProperty("clockDiv") 
            ? static_cast<float>(obj->getProperty("clockDiv")) : 1.0f;
        slot.pattern_length = obj->hasProperty("patternLen") 
            ? static_cast<int>(obj->getProperty("patternLen")) : 0;
            
        if (obj->hasProperty("buffer") && obj->getProperty("buffer").isArray())
        {
            auto* bufArr = obj->getProperty("buffer").getArray();
            slot.pattern_buffer.clear();
            for (auto& v : *bufArr)
                slot.pattern_buffer.push_back(static_cast<float>(v));
        }
        
        // PNW-style waveform shaping
        slot.level = obj->hasProperty("level") 
            ? static_cast<float>(obj->getProperty("level")) : 1.0f;
        slot.width = obj->hasProperty("width") 
            ? static_cast<float>(obj->getProperty("width")) : 0.5f;
        slot.phase_offset = obj->hasProperty("phaseOffset") 
            ? static_cast<float>(obj->getProperty("phaseOffset")) : 0.0f;
        slot.delay = obj->hasProperty("delay") 
            ? static_cast<float>(obj->getProperty("delay")) : 0.0f;
        slot.delay_div = obj->hasProperty("delayDiv") 
            ? static_cast<int>(obj->getProperty("delayDiv")) : 1;
        
        // Humanization
        slot.slop = obj->hasProperty("slop") 
            ? static_cast<float>(obj->getProperty("slop")) : 0.0f;
        
        // Euclidean rhythm
        slot.euclidean_steps = obj->hasProperty("euclideanSteps") 
            ? static_cast<int>(obj->getProperty("euclideanSteps")) : 0;
        slot.euclidean_triggers = obj->hasProperty("euclideanTriggers") 
            ? static_cast<int>(obj->getProperty("euclideanTriggers")) : 0;
        slot.euclidean_rotation = obj->hasProperty("euclideanRotation") 
            ? static_cast<int>(obj->getProperty("euclideanRotation")) : 0;
        
        // Random skip
        slot.random_skip = obj->hasProperty("randomSkip") 
            ? static_cast<float>(obj->getProperty("randomSkip")) : 0.0f;
        
        // Loop
        slot.loop_beats = obj->hasProperty("loopBeats") 
            ? static_cast<int>(obj->getProperty("loopBeats")) : 0;
        
        // Random seed
        slot.random_seed = obj->hasProperty("randomSeed") 
            ? static_cast<uint64_t>(static_cast<juce::int64>(obj->getProperty("randomSeed"))) : 0;
    }
}

juce::var lfo_assignments_to_var(const juce::NamedValueSet& assignments)
{
    auto* obj = new juce::DynamicObject();
    for (const auto& entry : assignments)
        obj->setProperty(entry.name, static_cast<int>(entry.value));
    return obj;
}

void lfo_assignments_from_var(const juce::var& value, juce::NamedValueSet& out_assignments)
{
    out_assignments.clear();
    if (!value.isObject())
        return;

    if (auto* obj = value.getDynamicObject())
    {
        const auto& props = obj->getProperties();
        for (const auto& entry : props)
            out_assignments.set(entry.name, static_cast<int>(entry.value));
    }
}

juce::var serialize_preset_json(const LayerCakePresetData& data)
{
    auto* pattern = new juce::DynamicObject();
    // Removed global pattern properties
    pattern->setProperty("masterGainDb", data.master_gain_db);
    pattern->setProperty("recordLayer", data.record_layer);
    pattern->setProperty("spreadAmount", data.spread_amount);
    pattern->setProperty("reverseProbability", data.reverse_probability);
    pattern->setProperty("clockEnabled", data.clock_enabled);
    pattern->setProperty("manualState", grain_state_to_var(data.manual_state));
    pattern->setProperty("knobs", knob_values_to_var(data.knob_values));
    pattern->setProperty("lfos", lfo_slots_to_var(data.lfo_slots));
    pattern->setProperty("lfoAssignments", lfo_assignments_to_var(data.lfo_assignments));

    return pattern;
}

bool parse_preset_json(const juce::var& value, LayerCakePresetData& out_data)
{
    if (!value.isObject())
    {
        DBG("LayerCakeLibraryManager::parse_preset_json invalid value");
        return false;
    }

    auto* pattern = value.getDynamicObject();
    
    if (pattern->hasProperty("masterGainDb"))
        out_data.master_gain_db = static_cast<float>(pattern->getProperty("masterGainDb"));
    if (pattern->hasProperty("recordLayer"))
        out_data.record_layer = static_cast<int>(pattern->getProperty("recordLayer"));
    if (pattern->hasProperty("spreadAmount"))
        out_data.spread_amount = static_cast<float>(pattern->getProperty("spreadAmount"));
    if (pattern->hasProperty("reverseProbability"))
        out_data.reverse_probability = static_cast<float>(pattern->getProperty("reverseProbability"));
    if (pattern->hasProperty("clockEnabled"))
        out_data.clock_enabled = static_cast<bool>(pattern->getProperty("clockEnabled"));
    if (pattern->hasProperty("manualState"))
        grain_state_from_var(pattern->getProperty("manualState"), out_data.manual_state);
    if (pattern->hasProperty("knobs"))
        knob_values_from_var(pattern->getProperty("knobs"), out_data.knob_values);
    if (pattern->hasProperty("lfos"))
        lfo_slots_from_var(pattern->getProperty("lfos"), out_data.lfo_slots);
    else
        out_data.lfo_slots = {};
    if (pattern->hasProperty("lfoAssignments"))
        lfo_assignments_from_var(pattern->getProperty("lfoAssignments"), out_data.lfo_assignments);
    else
        out_data.lfo_assignments.clear();

    return true;
}

bool write_json_file(const juce::File& target, const juce::var& json, const juce::String& context)
{
    juce::TemporaryFile temp(target);
    {
        juce::FileOutputStream stream(temp.getFile());
        if (!stream.openedOk())
        {
            DBG(context + " failed to open json file for writing");
            return false;
        }
        stream.writeText(juce::JSON::toString(json, true), false, false, "\n");
        stream.flush();
        if (stream.getStatus().failed())
        {
            DBG(context + " stream write error");
            return false;
        }
    }

    if (!temp.overwriteTargetFileWithTemporary())
    {
        DBG(context + " failed to finalize json file");
        return false;
    }

    return true;
}

bool read_json_file(const juce::File& file, juce::var& out_var, const juce::String& context)
{
    if (!file.existsAsFile())
    {
        DBG(context + " missing file=" + file.getFullPathName());
        return false;
    }

    const auto file_text = file.loadFileAsString();
    if (file_text.isEmpty())
    {
        DBG(context + " file empty");
        return false;
    }

    auto result = juce::JSON::parse(file_text, out_var);
    if (result.failed())
    {
        DBG(context + " parse error: " + result.getErrorMessage());
        return false;
    }

    if (!out_var.isObject())
    {
        DBG(context + " parsed value is not object");
        return false;
    }

    return true;
}

juce::File resolve_folder(const juce::File& root, const juce::String& name)
{
    const auto raw = juce::File::createLegalFileName(name);
    if (raw.isNotEmpty())
    {
        const auto f = root.getChildFile(raw);
        if (f.exists())
            return f;
    }

    const auto trimmed = juce::File::createLegalFileName(name.trim());
    return root.getChildFile(trimmed);
}

juce::File resolve_file(const juce::File& root, const juce::String& name, const juce::String& ext)
{
    const auto raw = juce::File::createLegalFileName(name);
    if (raw.isNotEmpty())
    {
        const auto f = root.getChildFile(raw + ext);
        if (f.existsAsFile())
            return f;
    }

    const auto trimmed = juce::File::createLegalFileName(name.trim());
    return root.getChildFile(trimmed + ext);
}
} // namespace

LayerCakeLibraryManager::LayerCakeLibraryManager()
{
    m_root = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                 .getChildFile(kLayerCakeFolderName);
    refresh();
}

void LayerCakeLibraryManager::refresh()
{
    ensure_directory(m_root);
    refresh_palettes();
    refresh_scenes();
    refresh_knobsets();
}

bool LayerCakeLibraryManager::save_palette(const juce::String& name, const LayerBufferArray& layers)
{
    const auto sanitized = sanitize_name(name);
    if (sanitized.isEmpty())
    {
        DBG("LayerCakeLibraryManager::save_palette invalid name");
        return false;
    }

    auto folder = ensure_directory(palette_folder(sanitized));
    if (!write_layers(folder, layers))
    {
        DBG("LayerCakeLibraryManager::save_palette failed to write layers");
        return false;
    }

    refresh_palettes();
    return true;
}

bool LayerCakeLibraryManager::load_palette(const juce::String& name, LayerBufferArray& out_layers) const
{
    const auto sanitized = sanitize_name(name);
    if (sanitized.isEmpty())
    {
        DBG("LayerCakeLibraryManager::load_palette invalid name");
        return false;
    }

    auto folder = resolve_folder(palettes_root(), name);
    if (!folder.exists())
    {
        DBG("LayerCakeLibraryManager::load_palette missing folder=" + folder.getFullPathName());
        return false;
    }

    if (!read_layers(folder, out_layers))
    {
        DBG("LayerCakeLibraryManager::load_palette failed to read layers");
        return false;
    }
    return true;
}

bool LayerCakeLibraryManager::delete_palette(const juce::String& name)
{
    const auto sanitized = sanitize_name(name);
    if (sanitized.isEmpty())
    {
        DBG("LayerCakeLibraryManager::delete_palette invalid name");
        return false;
    }

    auto folder = resolve_folder(palettes_root(), name);
    if (!folder.exists())
    {
        DBG("LayerCakeLibraryManager::delete_palette missing folder=" + folder.getFullPathName());
        return false;
    }

    const auto result = folder.deleteRecursively();
    if (!result)
        DBG("LayerCakeLibraryManager::delete_palette failed to delete folder=" + folder.getFullPathName());
    refresh_palettes();
    return result;
}

bool LayerCakeLibraryManager::save_knobset(const juce::String& name, const LayerCakePresetData& data)
{
    const auto sanitized = sanitize_name(name);
    if (sanitized.isEmpty())
    {
        DBG("LayerCakeLibraryManager::save_knobset invalid name");
        return false;
    }

    auto file = knobset_file(sanitized);
    auto json = serialize_preset_json(data);
    if (!write_json_file(file, json, "LayerCakeLibraryManager::save_knobset"))
    {
        DBG("LayerCakeLibraryManager::save_knobset failed to write json");
        return false;
    }

    refresh_knobsets();
    return true;
}

bool LayerCakeLibraryManager::load_knobset(const juce::String& name, LayerCakePresetData& out_data) const
{
    const auto sanitized = sanitize_name(name);
    if (sanitized.isEmpty())
    {
        DBG("LayerCakeLibraryManager::load_knobset invalid name");
        return false;
    }

    auto file = resolve_file(knobsets_root(), name, kPatternExtension);
    juce::var json;
    if (!read_json_file(file, json, "LayerCakeLibraryManager::load_knobset"))
    {
        DBG("LayerCakeLibraryManager::load_knobset failed to read json");
        return false;
    }

    out_data = LayerCakePresetData{};
    if (!parse_preset_json(json, out_data))
    {
        DBG("LayerCakeLibraryManager::load_knobset failed to parse knobset json");
        return false;
    }
    return true;
}

bool LayerCakeLibraryManager::delete_knobset(const juce::String& name)
{
    const auto sanitized = sanitize_name(name);
    if (sanitized.isEmpty())
    {
        DBG("LayerCakeLibraryManager::delete_knobset invalid name");
        return false;
    }

    auto file = resolve_file(knobsets_root(), name, kPatternExtension);
    if (!file.existsAsFile())
    {
        DBG("LayerCakeLibraryManager::delete_knobset missing file=" + file.getFullPathName());
        return false;
    }

    if (!file.deleteFile())
    {
        DBG("LayerCakeLibraryManager::delete_knobset failed to delete " + file.getFullPathName());
        return false;
    }

    refresh_knobsets();
    return true;
}

bool LayerCakeLibraryManager::save_scene(const juce::String& name,
                                         const LayerCakePresetData& data,
                                         const LayerBufferArray& layers)
{
    const auto sanitized = sanitize_name(name);
    if (sanitized.isEmpty())
    {
        DBG("LayerCakeLibraryManager::save_scene invalid name");
        return false;
    }

    auto folder = ensure_directory(scene_folder(sanitized));
    auto scene_file = folder.getChildFile(kSceneJsonName);

    auto json = serialize_preset_json(data);
    if (!write_json_file(scene_file, json, "LayerCakeLibraryManager::save_scene"))
    {
        DBG("LayerCakeLibraryManager::save_scene failed to write scene json");
        return false;
    }

    if (!write_layers(folder, layers))
    {
        DBG("LayerCakeLibraryManager::save_scene failed to write layers");
        return false;
    }

    refresh_scenes();
    return true;
}

bool LayerCakeLibraryManager::load_scene(const juce::String& name,
                                         LayerCakePresetData& out_data,
                                         LayerBufferArray& out_layers) const
{
    const auto sanitized = sanitize_name(name);
    if (sanitized.isEmpty())
    {
        DBG("LayerCakeLibraryManager::load_scene invalid name");
        return false;
    }

    auto folder = resolve_folder(scenes_root(), name);
    if (!folder.exists())
    {
        DBG("LayerCakeLibraryManager::load_scene missing folder=" + folder.getFullPathName());
        return false;
    }

    auto scene_file = folder.getChildFile(kSceneJsonName);
    juce::var json;
    if (!read_json_file(scene_file, json, "LayerCakeLibraryManager::load_scene"))
    {
        DBG("LayerCakeLibraryManager::load_scene failed to read scene json");
        return false;
    }

    out_data = LayerCakePresetData{};
    if (!parse_preset_json(json, out_data))
    {
        DBG("LayerCakeLibraryManager::load_scene failed to parse scene json");
        return false;
    }

    if (!read_layers(folder, out_layers))
    {
        DBG("LayerCakeLibraryManager::load_scene failed to read layers");
        return false;
    }
    return true;
}

bool LayerCakeLibraryManager::delete_scene(const juce::String& name)
{
    const auto sanitized = sanitize_name(name);
    if (sanitized.isEmpty())
    {
        DBG("LayerCakeLibraryManager::delete_scene invalid name");
        return false;
    }

    auto folder = resolve_folder(scenes_root(), name);
    if (!folder.exists())
    {
        DBG("LayerCakeLibraryManager::delete_scene missing folder=" + folder.getFullPathName());
        return false;
    }

    const auto result = folder.deleteRecursively();
    if (!result)
        DBG("LayerCakeLibraryManager::delete_scene failed to delete folder=" + folder.getFullPathName());
    refresh_scenes();
    return result;
}

juce::File LayerCakeLibraryManager::palettes_root() const
{
    return m_root.getChildFile(kPalettesFolderName);
}

juce::File LayerCakeLibraryManager::scenes_root() const
{
    return m_root.getChildFile(kScenesFolderName);
}

juce::File LayerCakeLibraryManager::knobsets_root() const
{
    return m_root.getChildFile(kKnobsetsFolderName);
}

juce::File LayerCakeLibraryManager::palette_folder(const juce::String& name) const
{
    return palettes_root().getChildFile(name);
}

juce::File LayerCakeLibraryManager::scene_folder(const juce::String& name) const
{
    return scenes_root().getChildFile(name);
}

juce::File LayerCakeLibraryManager::knobset_file(const juce::String& name) const
{
    return knobsets_root().getChildFile(name + kPatternExtension);
}

juce::String LayerCakeLibraryManager::sanitize_name(const juce::String& name)
{
    return juce::File::createLegalFileName(name.trim());
}

bool LayerCakeLibraryManager::write_layers(const juce::File& folder, const LayerBufferArray& layers) const
{
    ensure_directory(folder);
    for (size_t i = 0; i < layers.size(); ++i)
    {
        auto layer_file = folder.getChildFile("layer_" + juce::String(static_cast<int>(i)) + ".bin");
        if (!layers[i].has_audio || layers[i].recorded_length == 0)
        {
            if (layer_file.existsAsFile())
                layer_file.deleteFile();
            continue;
        }

        juce::FileOutputStream stream(layer_file);
        if (!stream.openedOk())
        {
            DBG("LayerCakeLibraryManager::write_layers failed to open " + layer_file.getFullPathName());
            return false;
        }

        stream.writeInt64(static_cast<juce::int64>(layers[i].recorded_length));
        stream.write(layers[i].samples.data(), static_cast<int>(layers[i].recorded_length * sizeof(float)));
        stream.flush();
        if (stream.getStatus().failed())
        {
            DBG("LayerCakeLibraryManager::write_layers write error");
            return false;
        }
    }
    return true;
}

bool LayerCakeLibraryManager::read_layers(const juce::File& folder, LayerBufferArray& out_layers) const
{
    for (size_t i = 0; i < out_layers.size(); ++i)
    {
        out_layers[i].samples.clear();
        out_layers[i].recorded_length = 0;
        out_layers[i].has_audio = false;

        auto layer_file = folder.getChildFile("layer_" + juce::String(static_cast<int>(i)) + ".bin");
        if (!layer_file.existsAsFile())
            continue;

        juce::FileInputStream stream(layer_file);
        if (!stream.openedOk())
        {
            DBG("LayerCakeLibraryManager::read_layers failed to open " + layer_file.getFullPathName());
            return false;
        }

        const auto recorded = stream.readInt64();
        if (recorded <= 0)
            continue;

        const size_t samples_to_read = static_cast<size_t>(recorded);
        out_layers[i].samples.resize(samples_to_read);
        const size_t bytes_to_read = samples_to_read * sizeof(float);
        if (stream.read(out_layers[i].samples.data(), static_cast<int>(bytes_to_read)) != static_cast<int>(bytes_to_read))
        {
            DBG("LayerCakeLibraryManager::read_layers truncated layer file");
            return false;
        }

        out_layers[i].recorded_length = samples_to_read;
        out_layers[i].has_audio = true;
    }
    return true;
}

void LayerCakeLibraryManager::refresh_palettes()
{
    auto root = ensure_directory(palettes_root());
    m_palette_names.clear();
    juce::Array<juce::File> dirs;
    root.findChildFiles(dirs, juce::File::findDirectories, false);
    for (const auto& dir : dirs)
        m_palette_names.add(dir.getFileName());
    m_palette_names.sort(true);
}

void LayerCakeLibraryManager::refresh_scenes()
{
    auto root = ensure_directory(scenes_root());
    m_scene_names.clear();
    juce::Array<juce::File> dirs;
    root.findChildFiles(dirs, juce::File::findDirectories, false);
    for (const auto& dir : dirs)
        m_scene_names.add(dir.getFileName());
    m_scene_names.sort(true);
}

void LayerCakeLibraryManager::refresh_knobsets()
{
    auto root = ensure_directory(knobsets_root());
    m_knobset_names.clear();
    juce::Array<juce::File> files;
    root.findChildFiles(files, juce::File::findFiles, false, juce::String("*") + kPatternExtension);
    for (const auto& file : files)
        m_knobset_names.add(file.getFileNameWithoutExtension());
    m_knobset_names.sort(true);
}
