cmake_minimum_required(VERSION 3.22)

project(TapeLooper VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_definitions(JUCE_MODAL_LOOPS_PERMITTED)

# Add JUCE
add_subdirectory(JUCE)

# Create the executable
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon.png)
    juce_add_gui_app(TapeLooper
        COMPANY_NAME "Unsound"
        PRODUCT_NAME "Tape Looper"
        VERSION ${PROJECT_VERSION}
        ICON_BIG ${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon.png
        ICON_SMALL ${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon.png
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
    Source/Engine/MultiTrackLooperEngine.cpp
    Source/Engine/TapeLoop.cpp
    Source/Engine/LooperWriteHead.cpp
    Source/Engine/LooperReadHead.cpp
)

# Engine headers
target_sources(TapeLooper PRIVATE
    Source/Engine/LooperTrackEngine.h
    Source/Engine/MultiTrackLooperEngine.h
    Source/Engine/TapeLoop.h
    Source/Engine/LooperWriteHead.h
    Source/Engine/LooperReadHead.h
    Source/Engine/OutputBus.h
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
    Source/Frontends/Shared/TransportControls.cpp
    Source/Frontends/Shared/ParameterKnobs.cpp
    Source/Frontends/Shared/LevelControl.cpp
    Source/Frontends/Shared/OutputSelector.cpp
    Source/Frontends/Shared/GradioUtilities.cpp
)

# Shared frontend components headers
target_sources(TapeLooper PRIVATE
    Source/Frontends/Shared/WaveformDisplay.h
    Source/Frontends/Shared/TransportControls.h
    Source/Frontends/Shared/ParameterKnobs.h
    Source/Frontends/Shared/LevelControl.h
    Source/Frontends/Shared/OutputSelector.h
    Source/Frontends/Shared/GradioUtilities.h
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
)

# VampNet frontend headers
target_sources(TapeLooper PRIVATE
    Source/Frontends/VampNet/MainComponent.h
    Source/Frontends/VampNet/LooperTrack.h
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
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
)

# Copy resources (if icon exists)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon.png)
    juce_add_binary_data(TapeLooperBinaryData
        SOURCES
            Resources/icon.png
    )
    target_link_libraries(TapeLooper PRIVATE TapeLooperBinaryData)
endif()

