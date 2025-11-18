# Developer Documentation
(slop)

## Project Overview

**Tape Looper** is a JUCE-based multitrack audio looper application with support for AI-powered sound generation, spatial audio panning, and MIDI control. It provides four different frontends (Basic, Text2Sound, VampNet, and WhAM) that share a common audio engine architecture.

### Key Features
- Multiple independent looper tracks (configurable, default 8)
- Real-time recording and playback
- Variable-speed playback (0.25x to 4.0x)
- AI integration via Gradio for text-to-sound and VampNet
- Spatial audio panning (Stereo, Quad, CLEAT 16-channel)
- MIDI learn functionality for parameter control
- Click synth and sampler for metronome/sample playback
- Token visualization for VampNet models
- Cross-platform (macOS, Windows, Linux)

---

## Architecture Overview

The application follows a **Model-View-Controller (MVC)** pattern with clear separation between:
- **Engine Layer**: Audio processing and looper logic
- **Frontend Layer**: UI and user interactions
- **Integration Layer**: External services (Gradio HTTP client)

```
┌─────────────────────────────────────────────────┐
│              Application Entry                   │
│  Main.cpp → StartupDialog → Frontend Selection  │
└────────────────┬────────────────────────────────┘
                 │
      ┌──────────┴──────────┐
      │                     │
┌─────▼─────┐      ┌────────▼────────┐
│  Engine   │      │    Frontend     │
│   Layer   │◄─────┤     Layer       │
└───────────┘      └─────────────────┘
      │                     │
      │            ┌────────▼────────┐
      │            │  Shared UI      │
      │            │  Components     │
      │            └─────────────────┘
      │
┌─────▼──────────┐
│  Integration   │
│     Layer      │
└────────────────┘
```

---

## Directory Structure

### `/apps/`
Application frontends (each is a separate executable)

#### `/apps/basic/`
Minimal looper interface with essential controls

#### `/apps/text2sound/`
Extended UI with AI text-to-sound generation

- **`VizWindow.h/cpp`**: Multi-track panner visualization window showing track positions in 2D space

#### `/apps/vampnet/`
UI for VampNet audio variation generation

#### `/apps/wham/`
Advanced frontend with token visualization, click synth, sampler, and MIDI learn

#### `/apps/cleatpinknoisetest/`
Test application for CLEAT panner with pink noise generator

### `/libs/flowerjuce/`
Shared library containing core engine and UI components

#### `/libs/flowerjuce/Engine/`
Core audio processing engine (frontend-agnostic)

- **`MultiTrackLooperEngine.h`**
  - Template-based engine class (`MultiTrackLooperEngineTemplate`)
  - Top-level engine coordinating multiple tracks
  - Implements `juce::AudioIODeviceCallback` for real-time audio
  - Manages `juce::AudioDeviceManager`
  - Provides track synchronization
  - Supports both `LooperTrackEngine` and `VampNetTrackEngine` via templates
  - **Key Methods:**
    - `audioDeviceIOCallbackWithContext()`: Real-time audio callback
    - `get_track_engine(index)`: Access individual track engine
    - `sync_all_tracks()`: Align all track playheads
    - `get_channel_levels()`: Get per-channel level meters

- **`LooperTrackEngine.h/cpp`**
  - Single track processing logic for basic looper
  - Contains `TrackState` struct with all track data
  - Handles record/playback state transitions
  - **Key Methods:**
    - `process_block()`: Process audio for one track
    - `initialize()`: Set up buffers based on sample rate
    - `load_from_file()`: Load audio file into track

- **`VampNetTrackEngine.h/cpp`**
  - Dual-buffer track engine for VampNet frontends
  - Uses separate record buffer and output buffer
  - Supports dry/wet mixing between buffers
  - Includes integrated ClickSynth and Sampler per track
  - **Key Features:**
    - `m_record_buffer`: Records input audio
    - `m_output_buffer`: Stores generated outputs
    - `m_dry_wet_mix`: Mix between record and output buffers (0.0-1.0)
    - `get_click_synth()`: Access click generator
    - `get_sampler()`: Access sample player

- **`TapeLoop.h/cpp`**
  - Audio buffer container
  - Thread-safe buffer management
  - **Key Members:**
    - `buffer`: Vector holding audio samples
    - `recordedLength`: Atomic counter for recorded samples
    - `hasRecorded`: Flag indicating if track has content

- **`LooperWriteHead.h/cpp`**
  - Recording logic
  - Handles writing incoming audio to buffer
  - Manages recording state transitions
  - **Key Methods:**
    - `processBlock()`: Write input audio to tape loop

- **`LooperReadHead.h/cpp`**
  - Playback logic
  - Variable-speed playback with interpolation
  - Position tracking
  - **Key Members:**
    - `playbackSpeed`: Speed multiplier (0.25x - 4.0x)
    - `position`: Current playback position
  - **Key Methods:**
    - `processBlock()`: Read from tape loop to output

- **`OutputBus.h`**
  - Output routing and mixing
  - Per-track output destination selection

#### `/libs/flowerjuce/Components/`
Reusable UI components shared across frontends

- **`WaveformDisplay.h/cpp`**
  - Visual representation of audio buffer
  - Shows playback position
  - Click to seek

- **`DualWaveformDisplay.h/cpp`**
  - Dual waveform display for VampNet (record + output buffers)
  - Shows both buffers side-by-side

- **`TransportControls.h/cpp`**
  - Standard transport buttons (record, play, stop)
  - Consistent styling across frontends

- **`ParameterKnobs.h/cpp`**
  - Rotary controls for parameters
  - Speed, volume, etc.

- **`LevelControl.h/cpp`**
  - Volume/gain slider with meters

- **`InputSelector.h/cpp`** / **`OutputSelector.h/cpp`**
  - Input/output bus routing selectors

- **`MidiLearnManager.h/cpp`** / **`MidiLearnComponent.h`**
  - MIDI learn functionality
  - Map MIDI CC to any parameter
  - Save/load mappings

- **`SinksWindow.h/cpp`**
  - Multi-channel level meter visualization
  - 4x4 grid for 16-channel output
  - Shows pan position and gain distribution

- **`SettingsDialog.h`**
  - Settings dialog for Gradio URL, MIDI devices, etc.

- **`GradioUtilities.h/cpp`**
  - Utility functions for Gradio integration

- **`ConfigManager.h/cpp`**
  - Configuration persistence

- **`VariationSelector.h/cpp`**
  - UI for selecting VampNet variations

- **`AudioInfoDisplay.h`**
  - Audio device information display

#### `/libs/flowerjuce/Panners/`
Spatial audio panning system

- **`Panner.h`**
  - Base interface for audio panners
  - Abstract class for mono-to-multichannel panning

- **`StereoPanner.h/cpp`**
  - Stereo (2-channel) panner
  - Standard left/right panning

- **`QuadPanner.h/cpp`**
  - Quad (4-channel) panner
  - 2D panning across 4 speakers

- **`CLEATPanner.h/cpp`**
  - CLEAT (16-channel) panner
  - 4x4 grid spatial audio distribution
  - 2D panning with gain power control

- **`PanningUtils.h/cpp`**
  - Utility functions for panning calculations
  - Gain computation for CLEAT panner

- **`Panner2DComponent.h/cpp`**
  - 2D panning UI component
  - Visual pan position control

- **`PathGeneratorButtons.h/cpp`**
  - UI for generating panning paths

- **`PannerTests.h/cpp`** / **`RunPannerTests.cpp`**
  - Unit tests for panner system

#### `/libs/flowerjuce/DSP/`
Digital signal processing utilities

- **`OnsetDetector.h/cpp`**
  - Onset detection for audio analysis

#### `/libs/flowerjuce/GradioClient/`
HTTP client for Gradio API integration

- **`GradioClient.h/cpp`**
  - RESTful HTTP client for Gradio spaces
  - Handles file upload/download
  - Polling-based async request handling
  - **Key Methods:**
    - `process_request()`: Main entry point for generation
    - `upload_file_request()`: Upload audio to Gradio
    - `make_post_request_for_event_id()`: Initiate generation
    - `get_response_from_event_id()`: Poll for results
    - `download_file_from_url()`: Download generated audio

#### `/libs/flowerjuce/Utils/`
Utility functions

- **`Utils.h/cpp`**
  - General utility functions

#### `/libs/flowerjuce/`
Root library files

- **`ClickSynth.h/cpp`**
  - Click sound generator
  - Generates short sine wave bursts
  - Used for metronome/click tracks
  - Per-track instances in VampNetTrackEngine

- **`Sampler.h/cpp`**
  - Audio sample player
  - Loads and plays back audio files
  - Per-track instances in VampNetTrackEngine

- **`CustomLookAndFeel.h`**
  - Custom JUCE styling
  - Dark theme colors and fonts

### `/plugins/`
Audio plugin implementations

#### `/plugins/cleatpanner/`
CLEAT Panner VST3/AU plugin

- **`PluginProcessor.h/cpp`**
  - Audio processor for CLEAT panner plugin
  - Mono input, 16-channel output
  - Parameters: Pan X, Pan Y, Gain Power

- **`PluginEditor.h/cpp`**
  - Plugin UI editor
  - 2D panning control

### `/apps/*/`
Each app directory contains:

- **`Main.cpp`**
  - Application entry point
  - Creates `StartupDialog` on launch
  - Creates selected frontend window
  - Handles app lifecycle

- **`StartupDialog.h/cpp`**
  - Initial configuration dialog
  - Select number of tracks
  - Choose frontend (if multiple available)
  - Audio device setup
  - Panner type selection (for WhAM)

- **`MainComponent.h/cpp`**
  - Main window component
  - Contains track list and global controls
  - Implements timer for UI updates

- **`LooperTrack.h/cpp`**
  - Single track UI component
  - Frontend-specific controls and features

---

## Data Flow

### Recording Flow

```
User clicks "Record" button
        │
        ▼
Frontend updates TrackState.writeHead.isRecording
        │
        ▼
Audio callback (real-time thread)
        │
        ▼
MultiTrackLooperEngine::audioDeviceIOCallbackWithContext()
        │
        ▼
LooperTrackEngine::processBlock()
        │
        ▼
LooperWriteHead::processBlock()
        │
        ▼
Writes audio samples to TapeLoop buffer
        │
        ▼
User clicks "Record" again to stop
        │
        ▼
LooperWriteHead finalizes, sets recordedLength
        │
        ▼
Automatically transitions to playback
```

### Playback Flow

```
Audio callback (real-time thread)
        │
        ▼
MultiTrackLooperEngine::audioDeviceIOCallbackWithContext()
        │
        ▼
LooperTrackEngine::processBlock()
        │
        ▼
LooperReadHead::processBlock()
        │
        ▼
Reads from TapeLoop buffer with interpolation
        │
        ▼
Applies playback speed
        │
        ▼
Routes through OutputBus
        │
        ▼
Mixes to output channels
```

### AI Generation Flow (Text2Sound/VampNet)

```
User enters text prompt / selects audio
        │
        ▼
User clicks "Generate"
        │
        ▼
Frontend spawns background thread
        │
        ▼
GradioClient::process_request()
        │
        ├─► Upload audio file (if needed)
        │
        ├─► POST request with parameters
        │
        ├─► Get event ID
        │
        ├─► Poll for completion
        │
        ├─► Download generated audio
        │
        ▼
VampNetTrackEngine::load_from_file()
        │
        ▼
Audio loaded into output buffer
        │
        ▼
UI updates, user can play generated audio
```

### VampNet Dual-Buffer Flow

```
VampNetTrackEngine uses two buffers:
        │
        ├─► Record Buffer (m_record_buffer)
        │   - Records input audio
        │   - Read by m_record_read_head
        │
        └─► Output Buffer (m_output_buffer)
            - Stores generated outputs
            - Read by m_output_read_head
            │
            ▼
Dry/Wet Mix (m_dry_wet_mix)
        │
        ├─► 0.0 = all dry (record buffer only)
        ├─► 0.5 = 50/50 mix
        └─► 1.0 = all wet (output buffer only)
        │
        ▼
Mixed output routed through panner
```

---

## Key Design Patterns

### 1. Real-Time Audio Thread Safety
- **Pattern**: Lock-free atomic operations
- **Implementation**: 
  - All `TrackState` members that cross thread boundaries are `std::atomic`
  - `TapeLoop` uses `juce::CriticalSection` only for buffer allocation
  - No locks in audio callback path

### 2. State Management
- **Pattern**: Centralized state objects
- **Implementation**:
  - `TrackState` struct contains all per-track data
  - UI reads state in timer callback
  - UI writes control parameters (speed, mute, etc.)
  - Engine writes playback state (position, isPlaying)

### 3. Frontend Abstraction
- **Pattern**: Namespace-based polymorphism
- **Implementation**:
  - Each frontend in separate namespace
  - All implement `MainComponent` and `LooperTrack`
  - Main.cpp switches at runtime based on user selection
  - Shared components in `libs/flowerjuce/Components/`

### 4. Dependency Injection
- **Pattern**: Constructor injection
- **Implementation**:
  - `MainWindow` receives frontend choice and device setup
  - Frontends receive track count
  - GradioClient receives space info

### 5. Template-Based Engine Architecture
- **Pattern**: Template metaprogramming for engine types
- **Implementation**:
  - `MultiTrackLooperEngineTemplate<TrackEngineType>` allows different track engines
  - `MultiTrackLooperEngine` = `MultiTrackLooperEngineTemplate<LooperTrackEngine>`
  - `VampNetMultiTrackLooperEngine` = `MultiTrackLooperEngineTemplate<VampNetTrackEngine>`
  - Enables code reuse while supporting different track behaviors

---

## Threading Model

### Threads in the Application

1. **Main/Message Thread**
   - UI rendering and event handling
   - Button clicks, slider changes
   - Timer callbacks for UI updates
   - File I/O operations

2. **Audio Thread** (real-time priority)
   - `audioDeviceIOCallbackWithContext()` callback
   - Must be lock-free and deterministic
   - Processes all tracks in sequence
   - Cannot allocate memory

3. **Background Worker Threads**
   - Gradio HTTP requests
   - File loading operations
   - Spawned via `juce::Thread` or lambdas

### Thread Communication

- **Audio → UI**: Atomic reads in timer callback
- **UI → Audio**: Atomic writes of control parameters
- **Background → UI**: `juce::MessageManager::callAsync()`

---

## New Features

### WhAM Frontend

The WhAM (VampNet Advanced Mode) frontend provides advanced features for working with VampNet models:

#### Key Features
- **Token Visualization**: Real-time visualization of VampNet model tokens
- **Click Synth**: Per-track click generator for metronome/click tracks
- **Sampler**: Per-track sample player for loading and triggering audio files
- **MIDI Learn**: Full MIDI CC mapping support for all parameters
- **Keyboard Shortcuts**: 
  - Number keys (1-8): Select track
  - 'R' key: Hold to record on selected track
  - Space: Trigger click/sampler on selected track
- **Panner Selection**: Choose between Stereo, Quad, or CLEAT panners at startup

#### Token Visualizer
- Visualizes VampNet model tokens in real-time
- Shows token grid with color-coded blocks
- Updates during audio generation
- Accessible via "Viz" button in main window

#### Click Synth
- Generates short sine wave bursts
- Configurable frequency (100-5000 Hz)
- Adjustable duration (1-100 ms)
- Per-track instances
- Can trigger on all tracks simultaneously
- MIDI learnable trigger button

#### Sampler
- Load audio files (WAV, AIFF, MP3, FLAC)
- Trigger playback via button or MIDI
- Per-track instances
- Automatically converts stereo to mono
- Shows loaded sample name

### VampNetTrackEngine Dual-Buffer System

`VampNetTrackEngine` uses a dual-buffer architecture:

#### Record Buffer (`m_record_buffer`)
- Records input audio from microphone/line input
- Read by `m_record_read_head` for playback
- Used for "dry" signal in dry/wet mix

#### Output Buffer (`m_output_buffer`)
- Stores generated audio from VampNet/Gradio
- Read by `m_output_read_head` for playback
- Used for "wet" signal in dry/wet mix

#### Dry/Wet Mix (`m_dry_wet_mix`)
- Atomic float value (0.0 to 1.0)
- 0.0 = 100% dry (record buffer only)
- 0.5 = 50/50 mix
- 1.0 = 100% wet (output buffer only)
- Allows blending between recorded and generated audio

#### Integrated Components
Each `VampNetTrackEngine` includes:
- `ClickSynth`: Click generator instance
- `Sampler`: Sample player instance
- `Panner*`: Pointer to spatial audio panner

### MIDI Learn System

The MIDI learn system allows mapping MIDI CC messages to any parameter:

#### Architecture
- **`MidiLearnManager`**: Central manager for all MIDI mappings
- **`MidiLearnable`**: Mixin class for learnable components
- **`MidiLearnComponent`**: Helper components for UI integration

#### Usage Flow
1. Right-click on any learnable control
2. Select "MIDI Learn..." from context menu
3. Move MIDI CC control on your device
4. Mapping is automatically saved
5. Control now responds to MIDI CC

#### Features
- Save/load mappings to file
- Clear individual or all mappings
- Support for toggle (buttons) and continuous (sliders) parameters
- Visual feedback during learn mode
- Per-parameter unique IDs

#### Supported Parameters
- Track level/volume
- Track speed
- Track mute
- Transport controls (record, play, stop)
- Click synth trigger
- Sampler trigger
- Panner position (X/Y)

### Panner System

The panner system provides spatial audio distribution:

#### Panner Types

**StereoPanner**
- 2-channel output (left/right)
- Standard stereo panning
- Pan value: 0.0 (left) to 1.0 (right)

**QuadPanner**
- 4-channel output
- 2D panning across 4 speakers
- Pan X/Y coordinates (0.0 to 1.0)

**CLEATPanner**
- 16-channel output (4x4 grid)
- 2D panning with gain power control
- Channels arranged row-major:
  - Channels 0-3: Bottom row (left to right)
  - Channels 4-7: Second row
  - Channels 8-11: Third row
  - Channels 12-15: Top row
- Gain power factor: Controls gain distribution curve (0.1 to 10.0)
  - 1.0 = standard panning
  - < 1.0 = smoother transitions
  - > 1.0 = sharper focus

#### Panner Interface
All panners implement the `Panner` base interface:
- `process_block()`: Process audio block
- `get_num_input_channels()`: Returns 1 (mono input)
- `get_num_output_channels()`: Returns output channel count

#### UI Components
- **`Panner2DComponent`**: 2D panning control widget
- **`PathGeneratorButtons`**: Generate automated panning paths
- **`SinksWindow`**: Visualize channel levels and pan position

### ClickSynth

Click sound generator for metronome/click tracks:

#### Features
- Generates short sine wave bursts
- Configurable parameters:
  - Frequency: 100-5000 Hz (default 1000 Hz)
  - Duration: 1-100 ms (default 10 ms)
  - Amplitude: 0.0-1.0 (default 0.8)

#### Usage
- Per-track instances in `VampNetTrackEngine`
- Trigger via `triggerClick()` method
- Generate samples via `getNextSample(sampleRate)`
- Thread-safe atomic parameters

#### Integration
- Accessible via `VampNetTrackEngine::get_click_synth()`
- UI window (`ClickSynthWindow`) for control
- MIDI learnable trigger button
- Keyboard shortcut support (Space key)

### Sampler

Audio sample player for loading and triggering audio files:

#### Features
- Load audio files (WAV, AIFF, MP3, FLAC)
- Automatic stereo-to-mono conversion
- Thread-safe playback
- Per-track instances

#### Usage
- Load sample: `loadSample(juce::File)`
- Trigger playback: `trigger()`
- Generate samples: `getNextSample()`
- Check status: `isPlaying()`, `hasSample()`

#### Integration
- Accessible via `VampNetTrackEngine::get_sampler()`
- UI window (`SamplerWindow`) for control
- MIDI learnable trigger button
- Keyboard shortcut support (Space key)

### SinksWindow

Multi-channel level meter visualization:

#### Features
- 4x4 grid display for 16-channel output
- Real-time level meters with decay
- Pan position visualization (for CLEAT panner)
- Gain distribution display
- "Show Advanced" toggle for detailed info

#### Display Elements
- Channel level meters (peak detection)
- Pan position indicator
- Gain values per channel
- Phase information (for CLEAT panner)

#### Usage
- Accessible via "Sinks" button in frontends
- Updates every 50ms via timer callback
- Shows current pan position and gain distribution
- Useful for debugging spatial audio

### CLEAT Panner Plugin

VST3/AU plugin for spatial audio panning:

#### Features
- Mono input, 16-channel output
- 2D panning control (X/Y coordinates)
- Gain power parameter
- Real-time parameter smoothing

#### Parameters
- **Pan X**: Horizontal position (0.0 to 1.0)
- **Pan Y**: Vertical position (0.0 to 1.0)
- **Gain Power**: Gain distribution curve (0.1 to 10.0)

#### Build
- Built as separate plugin target
- Formats: VST3, AU, Standalone
- Located in `plugins/cleatpanner/`

---

## Building and Extending

### Adding a New Frontend

1. Create directory in `/apps/YourFrontend/`
2. Implement `Main.cpp` with application entry point
3. Implement `MainComponent.h/cpp` with constructor taking `int numTracks`
4. Implement `LooperTrack.h/cpp` for per-track UI
5. Add to `apps/CMakeLists.txt` as new target
6. Create `StartupDialog` if needed
7. Link against `flowerjuce` library

### Adding a New AI Integration

1. Extend `GradioClient` or create new client class in `libs/flowerjuce/GradioClient/`
2. Add methods for new API endpoints
3. Add UI controls in appropriate frontend (`apps/*/`)
4. Handle async requests in background thread
5. Load results via `VampNetTrackEngine::load_from_file()` or `LooperTrackEngine::load_from_file()`

### Adding Audio Effects

1. Create effect processor class in `libs/flowerjuce/DSP/`
2. Add to `LooperReadHead::process_block()` or create new processing stage
3. Add UI controls in `libs/flowerjuce/Components/`
4. Store parameters in `TrackState` (use atomic for thread safety)

### Adding a New Panner Type

1. Create new panner class inheriting from `Panner` interface
2. Implement `process_block()`, `get_num_input_channels()`, `get_num_output_channels()`
3. Add to `libs/flowerjuce/Panners/`
4. Update `CMakeLists.txt` to include new files
5. Add UI component if needed (e.g., `Panner2DComponent` for 2D panners)
6. Update frontends to support new panner type

---

## Code Style and Conventions

### Naming Conventions
- **Classes**: PascalCase (`MultiTrackLooperEngine`, `VampNetTrackEngine`)
- **Methods**: snake_case (`process_block()`, `get_track_engine()`)
- **Members**: m_snake_case prefix (`m_track_state`, `m_write_head`)
- **Local variables**: snake_case (`num_tracks`, `sample_rate`)
- **Constants**: snake_case with 'const' qualifier
- **Atomics**: Clear naming indicating shared state (`m_is_playing`, `m_dry_wet_mix`)

### File Organization
- One class per .h/.cpp pair
- Header includes in order: system, JUCE, local
- Forward declarations preferred over includes when possible

### JUCE Conventions
- Use `juce::` namespace prefix explicitly
- Inherit from JUCE base classes appropriately
- Use `JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR` macro

---

## Dependencies

### JUCE Modules Used
- `juce_core`: Foundation (strings, files, threads)
- `juce_audio_basics`: Audio buffers and utilities
- `juce_audio_devices`: Audio I/O and device management
- `juce_audio_formats`: Audio file reading/writing
- `juce_audio_processors`: Plugin infrastructure (minimal use)
- `juce_audio_utils`: High-level audio components
- `juce_events`: Message loop and timers
- `juce_graphics`: 2D graphics rendering
- `juce_gui_basics`: Standard UI components
- `juce_gui_extra`: Extended UI components
- `juce_data_structures`: Data management

### External Dependencies
- CMake 3.22+
- C++17 compiler
- Platform-specific audio APIs (CoreAudio, WASAPI, ALSA)

---

## Testing

### Manual Testing Checklist
- [ ] Record on single track, verify playback
- [ ] Record on multiple tracks simultaneously
- [ ] Sync tracks after different start times
- [ ] Change playback speed during playback
- [ ] Load audio file into track
- [ ] Switch audio devices
- [ ] Generate audio via Text2Sound
- [ ] Generate variation via VampNet
- [ ] Mute/unmute during playback
- [ ] Stop and restart playback
- [ ] Test WhAM frontend features (token viz, click synth, sampler)
- [ ] Test MIDI learn functionality
- [ ] Test panner types (Stereo, Quad, CLEAT)
- [ ] Test dry/wet mix in VampNet tracks
- [ ] Test CLEAT panner plugin (VST3/AU)
- [ ] Test keyboard shortcuts in WhAM

### Known Limitations
- Maximum buffer duration: 10 seconds per track
- 8 tracks maximum (configurable at compile time)
- No undo/redo functionality
- No project save/load
- Mono audio input (stereo/multichannel output supported via panners)
- MIDI mappings not persisted across sessions (save/load available but not automatic)

---

## Performance Considerations

### Real-Time Audio
- All processing must complete within buffer size time
- Target: < 5ms latency at 512 sample buffer
- No memory allocation in audio thread
- Minimal branching in hot paths

### Memory Usage
- Each track buffer: `sampleRate * maxDuration * sizeof(float)`
- 8 tracks at 48kHz, 10s = ~3.84 MB
- Waveform display caching

### CPU Optimization
- Variable-speed playback uses linear interpolation (fast)
- Consider sinc interpolation for higher quality (slower)
- SIMD opportunities in mixing and effects

---

## Future Roadmap

### Planned Features
- [ ] Stereo/multichannel input support
- [ ] Audio effects (reverb, delay, filters)
- [ ] MIDI synchronization (clock sync)
- [ ] Project save/load
- [ ] Undo/redo
- [ ] Export mixed output
- [ ] Improved waveform visualization
- [ ] Real-time input monitoring
- [ ] Overdub mode
- [ ] Automated panning paths
- [ ] More panner types (8-channel, custom layouts)
- [ ] Preset management for MIDI mappings

### Technical Debt
- Refactor atomic state management into cleaner abstraction
- Improve error handling in GradioClient
- Add unit tests for engine components
- Document thread safety guarantees more explicitly
- Consider moving UI state out of engine layer
- Consolidate duplicate code between frontends
- Standardize naming conventions (some methods still use camelCase)
- Add automated tests for panner system

---

## Troubleshooting

### Audio Issues
- **No sound**: Check audio device selection in settings
- **Crackling**: Increase buffer size in audio settings
- **Latency**: Decrease buffer size, use ASIO (Windows) or CoreAudio (macOS)

### Build Issues
- **JUCE not found**: Ensure JUCE submodule is initialized
- **Compiler errors**: Verify C++17 support
- **Linking errors**: Check CMakeLists.txt module list

### Runtime Issues
- **Gradio timeout**: Check network connection, increase timeout
- **File load failure**: Verify audio format supported (WAV, AIFF, MP3, FLAC)
- **Crash on startup**: Check audio device availability

---

## Resources

### JUCE Documentation
- [JUCE Tutorials](https://juce.com/learn/tutorials)
- [JUCE API Reference](https://docs.juce.com/)
- [JUCE Forum](https://forum.juce.com/)

### Audio Programming
- [The Audio Programming Book](http://www.audiosynth.com/)
- [Designing Audio Effect Plugins in C++](https://www.willpirkle.com/)

### Project-Specific
- See `README.md` for user documentation
- See `CMakeLists.txt` for build configuration
- See inline code comments for implementation details

---

## Contributing

When contributing to this project:

1. Follow existing code style
2. Maintain thread safety in audio code
3. Test on multiple platforms if possible
4. Update this documentation for architectural changes
5. Comment non-obvious design decisions
6. Keep commits focused and atomic

---

## License

See `JUCE/LICENSE.md` for JUCE framework licensing (GPL/Commercial).
Project-specific code license TBD.

