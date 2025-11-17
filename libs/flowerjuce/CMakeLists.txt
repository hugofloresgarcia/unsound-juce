# flowerjuce shared library

add_library(flowerjuce STATIC)

# Engine source files
target_sources(flowerjuce PRIVATE
    Engine/LooperTrackEngine.cpp
    Engine/VampNetTrackEngine.cpp
    Engine/TapeLoop.cpp
    Engine/LooperWriteHead.cpp
    Engine/LooperReadHead.cpp
)

# Engine headers
target_sources(flowerjuce PRIVATE
    Engine/LooperTrackEngine.h
    Engine/MultiTrackLooperEngine.h
    Engine/VampNetTrackEngine.h
    Engine/TapeLoop.h
    Engine/LooperWriteHead.h
    Engine/LooperReadHead.h
    Engine/OutputBus.h
)

# DSP source files
target_sources(flowerjuce PRIVATE
    DSP/OnsetDetector.cpp
)

# DSP headers
target_sources(flowerjuce PRIVATE
    DSP/OnsetDetector.h
)

# GradioClient source files
target_sources(flowerjuce PRIVATE
    GradioClient/GradioClient.cpp
)

# GradioClient headers
target_sources(flowerjuce PRIVATE
    GradioClient/GradioClient.h
)

# Panners source files
target_sources(flowerjuce PRIVATE
    Panners/PanningUtils.cpp
    Panners/StereoPanner.cpp
    Panners/QuadPanner.cpp
    Panners/CLEATPanner.cpp
    Panners/Panner2DComponent.cpp
    Panners/PathGeneratorButtons.cpp
    Panners/PannerTests.cpp
)

# Panners headers
target_sources(flowerjuce PRIVATE
    Panners/Panner.h
    Panners/PanningUtils.h
    Panners/StereoPanner.h
    Panners/QuadPanner.h
    Panners/CLEATPanner.h
    Panners/Panner2DComponent.h
    Panners/PathGeneratorButtons.h
    Panners/PannerTests.h
)

# Components source files (renamed from Shared)
target_sources(flowerjuce PRIVATE
    Components/WaveformDisplay.cpp
    Components/DualWaveformDisplay.cpp
    Components/TransportControls.cpp
    Components/ParameterKnobs.cpp
    Components/LevelControl.cpp
    Components/InputSelector.cpp
    Components/OutputSelector.cpp
    Components/GradioUtilities.cpp
    Components/MidiLearnManager.cpp
    Components/ConfigManager.cpp
    Components/VariationSelector.cpp
)

# Components headers
target_sources(flowerjuce PRIVATE
    Components/WaveformDisplay.h
    Components/DualWaveformDisplay.h
    Components/TransportControls.h
    Components/ParameterKnobs.h
    Components/LevelControl.h
    Components/InputSelector.h
    Components/OutputSelector.h
    Components/GradioUtilities.h
    Components/MidiLearnManager.h
    Components/MidiLearnComponent.h
    Components/ConfigManager.h
    Components/VariationSelector.h
    Components/SettingsDialog.h
    Components/ModelParameterDialog.h
    Components/AudioInfoDisplay.h
)

# CustomLookAndFeel header
target_sources(flowerjuce PRIVATE
    CustomLookAndFeel.h
)

# ClickSynth and Sampler (needed by VampNetTrackEngine)
target_sources(flowerjuce PRIVATE
    ClickSynth.cpp
    ClickSynth.h
    Sampler.cpp
    Sampler.h
)

# Set include directories - this allows includes like <flowerjuce/Engine/...>
# From libs/flowerjuce/, we need to go up to root, then include libs/ so flowerjuce/ resolves correctly
get_filename_component(PROJECT_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../.. ABSOLUTE)
target_include_directories(flowerjuce PUBLIC
    ${PROJECT_ROOT}/libs
)

# Link JUCE modules
target_link_libraries(flowerjuce PUBLIC
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_data_structures
    juce::juce_dsp
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
)

