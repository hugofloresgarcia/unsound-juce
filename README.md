# Tape Looper - JUCE Multitrack Looper

A standalone JUCE application implementing a multitrack "tape" looper with variable-speed playback.

## Features

- 4 independent looper tracks (expandable)
- Each track has:
  - Record toggle button
  - Play/Pause button
  - Stop button
  - Mute button
  - Variable-speed playback slider (0.25x to 4.0x)
- Sync all tracks button to align playback
- Audio device selection menu

## Building

### Prerequisites

- CMake 3.22 or higher
- C++17 compatible compiler
- JUCE framework

### Setup JUCE

You need to add JUCE as a submodule or download it:

```bash
# Option 1: Add JUCE as a git submodule
git submodule add https://github.com/juce-framework/JUCE.git JUCE

# Option 2: Clone JUCE manually
git clone https://github.com/juce-framework/JUCE.git JUCE
```

### Build

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

On macOS, you can also use Xcode:
```bash
cmake -G Xcode ..
open TapeLooper.xcodeproj
```

## Usage

1. Launch the application
2. Click "Audio Settings" to configure your audio input/output devices
3. Click "Record" on a track to start recording
4. Click "Record" again to stop recording and start playback
5. Use the speed slider to change playback speed/pitch
6. Use "Sync All" to synchronize all playing tracks

## Architecture

- `LooperEngine`: Handles audio processing, recording, and playback
- `LooperTrack`: UI component for a single looper track
- `MainComponent`: Main window containing all tracks and controls

