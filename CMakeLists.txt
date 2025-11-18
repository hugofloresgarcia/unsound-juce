cmake_minimum_required(VERSION 3.22)

project(TapeLooper VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_definitions(JUCE_MODAL_LOOPS_PERMITTED)

# Add JUCE
add_subdirectory(JUCE)

# Add libraries
add_subdirectory(libs)

# Add apps
add_subdirectory(apps)

# Add plugins
add_subdirectory(plugins)

# Test runner executable
juce_add_console_app(PannerTestsRunner
    COMPANY_NAME "Unsound"
    PRODUCT_NAME "Panner Tests Runner"
    VERSION ${PROJECT_VERSION}
)

target_sources(PannerTestsRunner PRIVATE
    libs/flowerjuce/Panners/RunPannerTests.cpp
    libs/flowerjuce/Panners/PannerTests.cpp
    libs/flowerjuce/Panners/PannerTests.h
    libs/flowerjuce/Panners/PanningUtils.cpp
    libs/flowerjuce/Panners/PanningUtils.h
    libs/flowerjuce/Panners/StereoPanner.cpp
    libs/flowerjuce/Panners/StereoPanner.h
    libs/flowerjuce/Panners/QuadPanner.cpp
    libs/flowerjuce/Panners/QuadPanner.h
    libs/flowerjuce/Panners/CLEATPanner.cpp
    libs/flowerjuce/Panners/CLEATPanner.h
    libs/flowerjuce/Panners/Panner.h
)

target_link_libraries(PannerTestsRunner PRIVATE
    flowerjuce
    juce::juce_core
    juce::juce_dsp
    juce::juce_graphics
    juce::juce_gui_basics
)
