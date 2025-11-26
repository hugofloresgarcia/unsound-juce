# refactor plans

these are independent refactoring plans that can be executed in any order, though some have soft dependencies noted.

---

## plan a: lfo library restructure

**goal:** move LFO components from layercake into flowerjuce with proper DSP/Components separation.

**status:** not started

### current structure

```
flowerjuce/DSP/LfoUGen.h/.cpp     # Pure DSP - already good
apps/layercake/lfo/               # UI components - need to move
  ├── LayerCakeLfoWidget.h/.cpp
  ├── LfoParamRow (nested class)
  ├── LfoAssignmentButton.h/.cpp
  ├── LfoConnectionOverlay.h/.cpp
  ├── LfoDragHelpers.h
  └── LfoTriggerButton.h/.cpp
```

### target structure

```
flowerjuce/LFO/
  ├── DSP/
  │   └── LfoUGen.h/.cpp
  └── Components/
      ├── LfoWidget.h/.cpp
      ├── LfoParamRow.h/.cpp
      ├── LfoAssignmentButton.h/.cpp
      ├── LfoConnectionOverlay.h/.cpp
      ├── LfoDragHelpers.h
      └── LfoTriggerButton.h/.cpp
```

### tasks

- [ ] create `flowerjuce/LFO/DSP/` directory
- [ ] move `LfoUGen.h/.cpp` from `flowerjuce/DSP/` to `flowerjuce/LFO/DSP/`
- [ ] create `flowerjuce/LFO/Components/` directory
- [ ] copy LFO UI files from `apps/layercake/lfo/` to `flowerjuce/LFO/Components/`
- [ ] rename `LayerCakeLfoWidget` → `LfoWidget`
- [ ] extract `LfoParamRow` into its own `.h/.cpp` files
- [ ] remove layercake-specific dependencies from widgets:
  - [ ] `LayerCakeLookAndFeel` → use generic or make configurable
  - [ ] `LayerCakeLibraryManager` / `LayerCakePresetData` → abstract preset interface
- [ ] update `flowerjuce/CMakeLists.txt` with new paths
- [ ] update layercake includes to use `<flowerjuce/LFO/...>`
- [ ] delete old files from `apps/layercake/lfo/` after verification

### notes

- `LfoUGen` is already pure DSP, just needs path change
- the widgets have some layercake-specific coupling that needs abstracting

---

## plan b: panner DSP/components split + pathplayer

**goal:** separate panner DSP from UI, extract trajectory playback into reusable `PathPlayer` class.

**status:** not started

### current structure

```
flowerjuce/Panners/
  ├── Panner.h                    # Pure DSP interface ✓
  ├── StereoPanner.h/.cpp         # Pure DSP ✓
  ├── QuadPanner.h/.cpp           # Pure DSP ✓
  ├── CLEATPanner.h/.cpp          # Pure DSP ✓
  ├── PanningUtils.h/.cpp         # Pure DSP (path algorithms) ✓
  ├── Panner2DComponent.h/.cpp    # MIXED: UI + trajectory playback ✗
  └── PathGeneratorButtons.h/.cpp # Pure UI ✓
```

### target structure

```
flowerjuce/Panners/
  ├── DSP/
  │   ├── Panner.h
  │   ├── StereoPanner.h/.cpp
  │   ├── QuadPanner.h/.cpp
  │   ├── CLEATPanner.h/.cpp
  │   ├── PanningUtils.h/.cpp
  │   └── PathPlayer.h/.cpp       # NEW
  └── Components/
      ├── Panner2DComponent.h/.cpp
      └── PathGeneratorButtons.h/.cpp
```

### tasks

- [ ] create `flowerjuce/Panners/DSP/` directory
- [ ] move DSP files into `Panners/DSP/`:
  - [ ] `Panner.h`
  - [ ] `StereoPanner.h/.cpp`
  - [ ] `QuadPanner.h/.cpp`
  - [ ] `CLEATPanner.h/.cpp`
  - [ ] `PanningUtils.h/.cpp`
- [ ] create `flowerjuce/Panners/Components/` directory
- [ ] move UI files into `Panners/Components/`:
  - [ ] `Panner2DComponent.h/.cpp`
  - [ ] `PathGeneratorButtons.h/.cpp`
- [ ] create `PathPlayer` class (extract from `Panner2DComponent`):
  - [ ] trajectory storage and playback index
  - [ ] `start_playback()`, `stop_playback()`, `advance()`
  - [ ] `get_current_position()` with interpolation
  - [ ] speed and scale parameters
  - [ ] smoothing logic
- [ ] refactor `Panner2DComponent` to use `PathPlayer`
- [ ] update `flowerjuce/CMakeLists.txt`
- [ ] update all includes across apps

### pathplayer interface

```cpp
class PathPlayer
{
public:
    struct TrajectoryPoint { float x, y; double time; };
    
    void set_trajectory(const std::vector<TrajectoryPoint>& points);
    void start_playback();
    void stop_playback();
    void advance();
    
    std::pair<float, float> get_current_position() const;
    bool is_playing() const;
    
    void set_playback_speed(float speed);
    void set_scale(float scale);
    void set_offset(float x, float y);
    
private:
    std::vector<TrajectoryPoint> m_trajectory;
    size_t m_current_index{0};
    float m_playback_speed{1.0f};
    float m_scale{1.0f};
    juce::SmoothedValue<float> m_smoothed_x, m_smoothed_y;
};
```

---

## plan c: layercake plugin/app split

**goal:** enable layercake to build as both standalone app and audio plugin (VST3/AU).

**status:** not started

**soft dependencies:** benefits from plan a and b being done first, but not required.

### current structure

```
apps/layercake/
  ├── Main.cpp
  ├── MainComponent.h/.cpp        # 1560 lines - UI + AudioIODeviceCallback
  ├── StartupDialog.h/.cpp
  └── ... (lfo/, ui/, focus/, input/)
```

### target structure

```
apps/layercake/
  ├── LayerCakeProcessor.h/.cpp   # Pure audio - no UI, no device manager
  ├── LayerCakeEditor.h/.cpp      # Main UI component
  ├── LayerCakeStandalone.h/.cpp  # App wrapper with AudioDeviceManager
  ├── Main.cpp                    # Entry point for standalone
  ├── plugin/
  │   ├── LayerCakePlugin.h/.cpp        # juce::AudioProcessor wrapper
  │   └── LayerCakePluginEditor.h/.cpp  # juce::AudioProcessorEditor wrapper
  └── ... (existing ui/, lfo/, etc.)
```

### tasks

#### step 1: create LayerCakeProcessor

- [ ] create `LayerCakeProcessor.h/.cpp`
- [ ] move `LayerCakeEngine` ownership from MainComponent
- [ ] implement `prepare()` and `process_block()` methods
- [ ] add meter level atomics

```cpp
class LayerCakeProcessor
{
public:
    void prepare(double sample_rate, int block_size, int num_channels);
    void process_block(const float* const* input, int num_in_ch,
                       float* const* output, int num_out_ch, int num_samples);
    
    LayerCakeEngine& get_engine();
    std::array<std::atomic<float>, 16>& get_meter_levels();
    
private:
    LayerCakeEngine m_engine;
    std::array<std::atomic<float>, 16> m_meter_levels;
};
```

#### step 2: create LayerCakeEditor

- [ ] rename/refactor `MainComponent` → `LayerCakeEditor`
- [ ] remove `AudioIODeviceCallback` implementation
- [ ] remove `AudioDeviceManager` ownership
- [ ] take `LayerCakeProcessor&` in constructor
- [ ] keep all UI code as-is

#### step 3: create LayerCakeStandalone

- [ ] create `LayerCakeStandalone.h/.cpp`
- [ ] owns `AudioDeviceManager`
- [ ] owns `LayerCakeProcessor`
- [ ] owns `LayerCakeEditor`
- [ ] implements `AudioIODeviceCallback`

```cpp
class LayerCakeStandalone : public juce::Component,
                            public juce::AudioIODeviceCallback
{
public:
    LayerCakeStandalone();
    
    void audioDeviceIOCallbackWithContext(...) override;
    void audioDeviceAboutToStart(juce::AudioIODevice*) override;
    void audioDeviceStopped() override;
    
private:
    juce::AudioDeviceManager m_device_manager;
    LayerCakeProcessor m_processor;
    LayerCakeEditor m_editor;
};
```

#### step 4: create plugin target

- [ ] create `plugin/LayerCakePlugin.h/.cpp`
- [ ] create `plugin/LayerCakePluginEditor.h/.cpp`
- [ ] update `CMakeLists.txt` with plugin target

```cmake
# Shared sources
set(LAYERCAKE_COMMON_SOURCES
    LayerCakeProcessor.cpp
    LayerCakeEditor.cpp
    # ... UI files
)

# Standalone app
juce_add_gui_app(LayerCake ...)
target_sources(LayerCake PRIVATE
    ${LAYERCAKE_COMMON_SOURCES}
    LayerCakeStandalone.cpp
    Main.cpp
)

# Plugin
juce_add_plugin(LayerCakePlugin
    PLUGIN_MANUFACTURER_CODE Flwr
    PLUGIN_CODE LyCk
    FORMATS VST3 AU
    ...)
target_sources(LayerCakePlugin PRIVATE
    ${LAYERCAKE_COMMON_SOURCES}
    plugin/LayerCakePlugin.cpp
    plugin/LayerCakePluginEditor.cpp
)
```

---

## notes

### what's already good (no changes needed)

- `LayerCakeEngine` - clean `prepare()` / `process_block()` interface
- `GrainVoice`, `TapeLoop`, read/write heads
- `SyncInterface` / `LinkSyncStrategy` / `InternalSyncStrategy`
- Core DSP: `LowPassFilter`, `PeakMeter`, `MultiChannelLoudnessMeter`, `OnsetDetector`

### testing opportunities

once these refactors are done:

1. unit test `LayerCakeProcessor` with synthetic buffers
2. unit test `PathPlayer` trajectory algorithms
3. unit test `LfoUGen` modulation curves
4. integration test plugin in DAW
