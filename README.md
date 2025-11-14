# Tape Looper (README SLOP)

<div align="center">

**A powerful multitrack audio looper with AI-powered sound generation**

[![JUCE](https://img.shields.io/badge/JUCE-8.0+-blue.svg)](https://juce.com/)
[![CMake](https://img.shields.io/badge/CMake-3.22+-green.svg)](https://cmake.org/)
[![C++](https://img.shields.io/badge/C++-17-orange.svg)](https://isocpp.org/)
[![License](https://img.shields.io/badge/license-GPL%2FCommercial-red.svg)](JUCE/LICENSE.md)

</div>

---

## Overview

Tape Looper is a standalone JUCE application that brings the creative power of multitrack loop recording to your desktop. Inspired by classic tape loopers and modern looping pedals, it combines intuitive real-time recording with cutting-edge AI audio generation capabilities.

### Three Powerful Frontends

**🎵 Basic** - Clean, focused looping experience
- Essential looping controls
- Perfect for live performance and jamming
- Lightweight and responsive

**🤖 Text2Sound** - AI-powered audio generation
- Generate sounds from text descriptions
- Powered by Gradio integration
- Create unique soundscapes instantly

**🎸 VampNet** - Neural audio variations
- Transform existing loops with AI
- Generate variations and remixes
- Experimental sound design tools

---

## Features

### Core Looping
- **8 Independent Tracks** (configurable)
  - Record, play/pause, stop controls
  - Variable-speed playback (0.25x - 4.0x)
  - Individual mute buttons
  - Load audio files directly
  
- **Real-Time Performance**
  - Low-latency audio processing
  - Simultaneous multi-track recording
  - Live speed manipulation
  
- **Track Synchronization**
  - One-click sync all tracks
  - Aligned playback across loops
  - Perfect for layering

### AI Integration
- **Text-to-Sound Generation**
  - Natural language audio synthesis
  - Gradio API integration
  - Direct-to-track loading

- **VampNet Processing**
  - Neural audio variation generation
  - Creative sound transformation
  - Experimental AI workflows

### Professional Audio
- **Flexible I/O**
  - Support for all major audio interfaces
  - Configurable sample rates
  - Low-latency monitoring

- **File Support**
  - WAV, AIFF, MP3, FLAC import
  - Load existing loops
  - Export individual tracks

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
git clone https://github.com/yourusername/unsound-juce.git
cd unsound-juce
```

2. **Initialize JUCE submodule**
```bash
git submodule update --init --recursive
```

Or manually clone JUCE:
```bash
git clone https://github.com/juce-framework/JUCE.git JUCE
```

3. **Build the project**

**macOS/Linux:**
```bash
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

**macOS with Xcode:**
```bash
mkdir build && cd build
cmake -G Xcode ..
open TapeLooper.xcodeproj
```

**Windows:**
```bash
mkdir build && cd build
cmake -G "Visual Studio 17 2022" ..
cmake --build . --config Release
```

4. **Run**
- macOS: `build/TapeLooper_artefacts/Release/Tape Looper.app`
- Linux: `build/TapeLooper_artefacts/Release/Tape\ Looper`
- Windows: `build\TapeLooper_artefacts\Release\Tape Looper.exe`

---

## Usage Guide

### Getting Started

1. **Launch & Configure**
   - On startup, select your desired frontend
   - Choose number of tracks (1-8)
   - Configure audio input/output devices

2. **Basic Recording**
   - Click **Record** on any track to start recording
   - Play or sing into your microphone
   - Click **Record** again to stop and start playback
   - Adjust **Speed** slider to change playback rate

3. **Multi-Track Layering**
   - Record on multiple tracks sequentially
   - Each track loops independently
   - Use **Sync All** to align all tracks
   - Use **Mute** to control individual track output

4. **Loading Audio**
   - Click **Load** on any track
   - Select an audio file (WAV, MP3, AIFF, FLAC)
   - File will loop at original speed
   - Adjust speed as needed

### Text2Sound Frontend

1. **Configure Gradio**
   - Click **Gradio Settings**
   - Enter your Gradio space URL
   - Default: `https://opensound-ezaudio-controlnet.hf.space/`

2. **Generate Audio**
   - Enter text description in prompt field
   - Click **Generate**
   - Wait for AI processing (may take 30-60 seconds)
   - Audio automatically loads into track

3. **Tips**
   - Be specific in descriptions
   - Try musical terms (e.g., "warm synth pad")
   - Experiment with instrument names
   - Describe tempo and mood

### VampNet Frontend

1. **Setup**
   - Configure Gradio endpoint for VampNet
   - Record or load base audio on a track

2. **Generate Variations**
   - Select source track
   - Adjust variation parameters
   - Click **Generate Variation**
   - AI creates new version based on original

---

## Architecture

Tape Looper uses a clean, modular architecture:

```
Application Layer (Main.cpp, StartupDialog)
         ↓
Frontend Layer (Basic / Text2Sound / VampNet)
         ↓
Shared UI Components (Waveforms, Controls)
         ↓
Engine Layer (MultiTrackLooperEngine)
         ↓
Track Processing (LooperTrackEngine, Read/Write Heads)
         ↓
Audio Buffer (TapeLoop)
```

### Key Components

- **MultiTrackLooperEngine**: Coordinates all tracks and audio I/O
- **LooperTrackEngine**: Processes individual track audio
- **TapeLoop**: Thread-safe audio buffer storage
- **GradioClient**: HTTP client for AI service integration

For detailed architecture documentation, see **[DEVELOPER.md](DEVELOPER.md)**.

---

## Configuration

### Audio Settings

Access via **Audio Settings** button:
- **Input Device**: Select microphone or audio interface
- **Output Device**: Select speakers or headphones
- **Sample Rate**: 44.1kHz, 48kHz, 96kHz (device dependent)
- **Buffer Size**: Trade-off between latency and stability
  - Smaller = lower latency (64-256 samples)
  - Larger = more stable (512-2048 samples)

### Performance Tips

- Use ASIO drivers on Windows for best latency
- Use CoreAudio on macOS (automatic)
- Use JACK or ALSA on Linux
- Close other audio applications
- Increase buffer size if experiencing dropouts

---

## Keyboard Shortcuts

*Coming soon - keyboard shortcuts are planned for future releases*

---

## Troubleshooting

### No Audio Input/Output
- Check audio device selection in settings
- Verify device is not in use by another application
- On macOS: Check System Preferences → Security & Privacy → Microphone
- On Linux: Check PulseAudio/JACK configuration

### Crackling or Dropouts
- Increase buffer size in audio settings
- Close background applications
- Check CPU usage
- Disable WiFi/Bluetooth if possible

### Gradio Connection Failed
- Verify internet connection
- Check Gradio URL is correct
- Ensure space is online (HuggingFace spaces can sleep)
- Try again after a few minutes

### Build Errors
- Ensure JUCE submodule is initialized: `git submodule update --init`
- Verify CMake version: `cmake --version`
- Check compiler supports C++17
- See [DEVELOPER.md](DEVELOPER.md) for detailed build instructions

---

## Development

Want to contribute or customize Tape Looper?

📚 **Read the [Developer Documentation](DEVELOPER.md)** for:
- Detailed architecture overview
- Code organization and style guide
- How to add new frontends
- How to extend AI integrations
- Threading and performance considerations
- API reference

### Quick Links for Developers

- [Architecture Overview](DEVELOPER.md#architecture-overview)
- [Directory Structure](DEVELOPER.md#directory-structure)
- [Adding a New Frontend](DEVELOPER.md#adding-a-new-frontend)
- [Thread Safety Guidelines](DEVELOPER.md#threading-model)

---

## Roadmap

### Near Term
- [ ] Stereo/multichannel track support
- [ ] Audio effects (reverb, delay, EQ)
- [ ] Keyboard shortcuts
- [ ] Project save/load
- [ ] Export mixed output

### Future
- [ ] VST3 plugin version
- [ ] MIDI synchronization
- [ ] Undo/redo functionality
- [ ] Advanced waveform editing
- [ ] Cloud collaboration features

See [DEVELOPER.md](DEVELOPER.md#future-roadmap) for complete roadmap.

---

## Credits

### Built With
- [JUCE](https://juce.com/) - Cross-platform audio framework
- [CMake](https://cmake.org/) - Build system
- [Gradio](https://gradio.app/) - AI service integration

### Research & Inspiration
- Unsound Research Project
- Modern looping pedals (Boss RC series)
- Tape delay aesthetics

---

## License

This project uses the JUCE framework, which is licensed under GPL v3 or a commercial license. See [JUCE/LICENSE.md](JUCE/LICENSE.md) for details.

Project-specific code license: TBD

---

## Support

- 🐛 **Bug Reports**: Open an issue on GitHub
- 💡 **Feature Requests**: Open an issue with [Feature Request] tag
- 📖 **Documentation**: See [DEVELOPER.md](DEVELOPER.md)
- 💬 **Discussions**: GitHub Discussions

---

<div align="center">

**Made with ❤️ for musicians, sound designers, and audio experimenters**

[Report Bug](../../issues) · [Request Feature](../../issues) · [Documentation](DEVELOPER.md)

</div>

