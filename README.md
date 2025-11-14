# Neural Tape Looper 

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
- macOS: `build/TapeLooper_artefacts/Release/Tape Looper.app`
- Linux: `build/TapeLooper_artefacts/Release/Tape\ Looper`
- Windows: `build\TapeLooper_artefacts\Release\Tape Looper.exe`

---
