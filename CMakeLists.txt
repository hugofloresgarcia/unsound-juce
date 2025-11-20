cmake_minimum_required(VERSION 3.22)

project(TapeLooper VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Git QUIET)

set(TAPELOOPER_GIT_BRANCH "unknown")
set(TAPELOOPER_GIT_COMMIT "unknown")
set(TAPELOOPER_GIT_TIMESTAMP "unknown")

if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE TAPELOOPER_GIT_BRANCH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE TAPELOOPER_GIT_BRANCH_RESULT
    )
    if(NOT TAPELOOPER_GIT_BRANCH_RESULT EQUAL 0 OR TAPELOOPER_GIT_BRANCH STREQUAL "")
        set(TAPELOOPER_GIT_BRANCH "unknown")
    endif()

    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE TAPELOOPER_GIT_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE TAPELOOPER_GIT_COMMIT_RESULT
    )
    if(NOT TAPELOOPER_GIT_COMMIT_RESULT EQUAL 0 OR TAPELOOPER_GIT_COMMIT STREQUAL "")
        set(TAPELOOPER_GIT_COMMIT "unknown")
    endif()

    execute_process(
        COMMAND ${GIT_EXECUTABLE} show -s --format=%ci HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE TAPELOOPER_GIT_TIMESTAMP
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE TAPELOOPER_GIT_TIMESTAMP_RESULT
    )
    if(NOT TAPELOOPER_GIT_TIMESTAMP_RESULT EQUAL 0 OR TAPELOOPER_GIT_TIMESTAMP STREQUAL "")
        set(TAPELOOPER_GIT_TIMESTAMP "unknown")
    endif()
endif()

add_compile_definitions(JUCE_MODAL_LOOPS_PERMITTED)

# Add JUCE
add_subdirectory(JUCE)

# Create the executable
set(APP_ICON_PATH "")
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/Assets/wham.png)
    set(APP_ICON_PATH ${CMAKE_CURRENT_SOURCE_DIR}/Assets/wham.png)
elseif(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon.png)
    set(APP_ICON_PATH ${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon.png)
endif()

if(APP_ICON_PATH)
    juce_add_gui_app(TapeLooper
        COMPANY_NAME "Unsound"
        PRODUCT_NAME "Tape Looper"
        VERSION ${PROJECT_VERSION}
        ICON_BIG ${APP_ICON_PATH}
        ICON_SMALL ${APP_ICON_PATH}
    )
else()
    juce_add_gui_app(TapeLooper
        COMPANY_NAME "Unsound"
        PRODUCT_NAME "Tape Looper"
        VERSION ${PROJECT_VERSION}
    )
endif()

# Engine source files
target_sources(TapeLooper PRIVATE
    Source/Engine/LooperTrackEngine.cpp
    Source/Engine/VampNetTrackEngine.cpp
    Source/Engine/TapeLoop.cpp
    Source/Engine/LooperWriteHead.cpp
    Source/Engine/LooperReadHead.cpp
)

# Panners source files
target_sources(TapeLooper PRIVATE
    Source/Panners/PanningUtils.cpp
    Source/Panners/StereoPanner.cpp
    Source/Panners/QuadPanner.cpp
    Source/Panners/CLEATPanner.cpp
    Source/Panners/Panner2DComponent.cpp
    Source/Panners/PannerTests.cpp
)

# Engine headers
target_sources(TapeLooper PRIVATE
    Source/Engine/LooperTrackEngine.h
    Source/Engine/MultiTrackLooperEngine.h
    Source/Engine/VampNetTrackEngine.h
    Source/Engine/TapeLoop.h
    Source/Engine/LooperWriteHead.h
    Source/Engine/LooperReadHead.h
    Source/Engine/OutputBus.h
)

# Panners headers
target_sources(TapeLooper PRIVATE
    Source/Panners/Panner.h
    Source/Panners/PanningUtils.h
    Source/Panners/StereoPanner.h
    Source/Panners/QuadPanner.h
    Source/Panners/CLEATPanner.h
    Source/Panners/Panner2DComponent.h
    Source/Panners/PannerTests.h
)

# GradioClient source files
target_sources(TapeLooper PRIVATE
    Source/GradioClient/GradioClient.cpp
)

# GradioClient headers
target_sources(TapeLooper PRIVATE
    Source/GradioClient/GradioClient.h
)

# Shared frontend components source files
target_sources(TapeLooper PRIVATE
    Source/Frontends/Shared/WaveformDisplay.cpp
    Source/Frontends/Shared/DualWaveformDisplay.cpp
    Source/Frontends/Shared/TransportControls.cpp
    Source/Frontends/Shared/ParameterKnobs.cpp
    Source/Frontends/Shared/LevelControl.cpp
    Source/Frontends/Shared/InputSelector.cpp
    Source/Frontends/Shared/OutputSelector.cpp
    Source/Frontends/Shared/GradioUtilities.cpp
    Source/Frontends/Shared/MidiLearnManager.cpp
    Source/Frontends/Shared/GitInfo.cpp
)

# Shared frontend components headers
target_sources(TapeLooper PRIVATE
    Source/Frontends/Shared/WaveformDisplay.h
    Source/Frontends/Shared/DualWaveformDisplay.h
    Source/Frontends/Shared/TransportControls.h
    Source/Frontends/Shared/ParameterKnobs.h
    Source/Frontends/Shared/LevelControl.h
    Source/Frontends/Shared/InputSelector.h
    Source/Frontends/Shared/OutputSelector.h
    Source/Frontends/Shared/GradioUtilities.h
    Source/Frontends/Shared/MidiLearnManager.h
    Source/Frontends/Shared/MidiLearnComponent.h
    Source/Frontends/Shared/GitInfo.h
)

# Basic frontend source files
target_sources(TapeLooper PRIVATE
    Source/Frontends/Basic/MainComponent.cpp
    Source/Frontends/Basic/LooperTrack.cpp
)

# Basic frontend headers
target_sources(TapeLooper PRIVATE
    Source/Frontends/Basic/MainComponent.h
    Source/Frontends/Basic/LooperTrack.h
)

# Text2Sound frontend source files
target_sources(TapeLooper PRIVATE
    Source/Frontends/Text2Sound/MainComponent.cpp
    Source/Frontends/Text2Sound/LooperTrack.cpp
)

# Text2Sound frontend headers
target_sources(TapeLooper PRIVATE
    Source/Frontends/Text2Sound/MainComponent.h
    Source/Frontends/Text2Sound/LooperTrack.h
)

# VampNet frontend source files
target_sources(TapeLooper PRIVATE
    Source/Frontends/VampNet/MainComponent.cpp
    Source/Frontends/VampNet/LooperTrack.cpp
    Source/Frontends/VampNet/ClickSynth.cpp
    Source/Frontends/VampNet/Sampler.cpp
    Source/Frontends/VampNet/SessionConfig.cpp
)

# VampNet frontend headers
target_sources(TapeLooper PRIVATE
    Source/Frontends/VampNet/MainComponent.h
    Source/Frontends/VampNet/LooperTrack.h
    Source/Frontends/VampNet/ClickSynth.h
    Source/Frontends/VampNet/Sampler.h
    Source/Frontends/VampNet/SessionConfig.h
)

# WhAM frontend source files
target_sources(TapeLooper PRIVATE
    Source/Frontends/WhAM/MainComponent.cpp
    Source/Frontends/WhAM/LooperTrack.cpp
    Source/Frontends/WhAM/Synths.cpp
    Source/Frontends/WhAM/SessionConfig.cpp
    Source/Frontends/WhAM/TokenVisualizer.cpp
    Source/Frontends/WhAM/ModelParamsPopup.cpp
)

# WhAM frontend headers
target_sources(TapeLooper PRIVATE
    Source/Frontends/WhAM/MainComponent.h
    Source/Frontends/WhAM/LooperTrack.h
    Source/Frontends/WhAM/Synths.h
    Source/Frontends/WhAM/SessionConfig.h
    Source/Frontends/WhAM/TokenVisualizer.h
    Source/Frontends/WhAM/ModelParamsPopup.h
)

# Application source files
target_sources(TapeLooper PRIVATE
    Source/Main.cpp
    Source/StartupDialog.cpp
)

# Application headers
target_sources(TapeLooper PRIVATE
    Source/StartupDialog.h
)

# Link JUCE modules
# Note: juce_add_gui_app automatically includes juce_application_manager
target_link_libraries(TapeLooper PRIVATE
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

# Embed binary resources
set(BINARY_DATA_SOURCES)

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon.png)
    list(APPEND BINARY_DATA_SOURCES Resources/icon.png)
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/Assets/wham.png)
    list(APPEND BINARY_DATA_SOURCES Assets/wham.png)
endif()

set(WHAM_STACK_ASSETS
    Assets/Stack/WHAM-Stack-1-black.png
    Assets/Stack/WHAM-Stack-1-color.png
    Assets/Stack/WHAM-Stack-2-black.png
    Assets/Stack/WHAM-Stack-2-color.png
    Assets/Stack/WHAM-Stack-3-black.png
    Assets/Stack/WHAM-Stack-3-color.png
    Assets/Stack/WHAM-Stack-4-black.png
    Assets/Stack/WHAM-Stack-4-color.png
)

set(MISSING_STACK_ASSETS)
foreach(stackFile IN LISTS WHAM_STACK_ASSETS)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${stackFile})
        list(APPEND BINARY_DATA_SOURCES ${stackFile})
    else()
        list(APPEND MISSING_STACK_ASSETS ${stackFile})
    endif()
endforeach()

if(MISSING_STACK_ASSETS)
    list(JOIN MISSING_STACK_ASSETS ", " _WHAM_STACK_WARNING_LIST)
    message(WARNING "Missing WhAM stack asset(s): ${_WHAM_STACK_WARNING_LIST}. The Viz cube stack will fall back to text placeholders.")
endif()

if(BINARY_DATA_SOURCES)
    juce_add_binary_data(TapeLooperBinaryData
        SOURCES ${BINARY_DATA_SOURCES}
    )
    target_link_libraries(TapeLooper PRIVATE TapeLooperBinaryData)
endif()

# Test runner executable
juce_add_console_app(PannerTestsRunner
    COMPANY_NAME "Unsound"
    PRODUCT_NAME "Panner Tests Runner"
    VERSION ${PROJECT_VERSION}
)

target_sources(PannerTestsRunner PRIVATE
    Source/Panners/RunPannerTests.cpp
    Source/Panners/PannerTests.cpp
    Source/Panners/PanningUtils.cpp
    Source/Panners/StereoPanner.cpp
    Source/Panners/QuadPanner.cpp
    Source/Panners/CLEATPanner.cpp
)

target_sources(PannerTestsRunner PRIVATE
    Source/Panners/PannerTests.h
    Source/Panners/Panner.h
    Source/Panners/PanningUtils.h
    Source/Panners/StereoPanner.h
    Source/Panners/QuadPanner.h
    Source/Panners/CLEATPanner.h
)

target_link_libraries(PannerTestsRunner PRIVATE
    juce::juce_core
    juce::juce_dsp
    juce::juce_graphics
    juce::juce_gui_basics
)

