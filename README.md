# Neural Tape Looper 

A JUCE-based multitrack audio looper application with support for AI-powered sound generation, spatial audio panning, and MIDI control.

---

## Features

- **Multiple Frontends**: Choose from Basic, Text2Sound, VampNet, or WhAM interfaces
- **Multitrack Looping**: Up to 8 independent looper tracks (configurable)
- **Real-time Recording & Playback**: Low-latency audio processing
- **Variable-speed Playback**: 0.25x to 4.0x speed control
- **AI Integration**: Text-to-sound and VampNet audio generation via Gradio
- **Spatial Audio**: Multiple panner types (Stereo, Quad, CLEAT 16-channel)
- **MIDI Learn**: Map MIDI CC controls to any parameter
- **Click Synth & Sampler**: Built-in click generation and sample playback
- **Token Visualization**: Visualize VampNet model tokens in real-time
- **Cross-platform**: macOS, Windows, and Linux support

---

## Quick Start

### Installation

#### Prerequisites

- **CMake** 3.22 or higher
- **C++17** compatible compiler
  - macOS: Xcode 12+
  - Windows: Visual Studio 2019+
  - Linux: GCC 9+ or Clang 10+
- **JUCE Framework** (included as submodule)

#### Setup

1. **Clone the repository**
```bash
git clone https://github.com/hugofloresgarcia/unsound-juce.git
cd unsound-juce
```

2. **Initialize JUCE submodule**
```bash
git submodule update --init --recursive
```

3. **Build the project**

**macOS/Linux:**
```bash
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

**Windows:**
```bash
mkdir build && cd build
cmake -G "Visual Studio 17 2022" ..
cmake --build . --config Release
```

4. **Run**
- macOS: `build/apps/basic/Release/Basic.app` (or `text2sound`, `vampnet`, `wham`)
- Linux: `build/apps/basic/Release/Basic` (or `text2sound`, `vampnet`, `wham`)
- Windows: `build\apps\basic\Release\Basic.exe` (or `text2sound`, `vampnet`, `wham`)

---

## Available Applications

### Basic
Minimal looper interface with essential recording and playback controls.

### Text2Sound
Extended interface with AI text-to-sound generation. Enter text prompts to generate audio.

### VampNet
VampNet audio variation generation. Uses existing audio as input to generate variations.

### WhAM
Advanced frontend featuring:
- Token visualization for VampNet models
- Click synth for metronome/click tracks
- Sampler for loading and triggering audio samples
- Full MIDI learn support
- Keyboard shortcuts for track selection and recording

---

## Plugins

### CLEAT Panner
A VST3/AU plugin for spatial audio panning across 16 channels (4x4 grid). Supports:
- 2D panning (X/Y coordinates)
- Gain power control for panning curves
- Real-time channel level visualization

Build location: `build/plugins/cleatpanner/Release/`

---

## Documentation

See `DEVELOPER.md` for detailed architecture documentation and development guide.

---
