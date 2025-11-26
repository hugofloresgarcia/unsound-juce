
cmake_minimum_required(VERSION 3.22)

project(Tests)

# Define the LfoTests executable
add_executable(LfoTests LfoTests.cpp)

# Link against flowerjuce and JUCE modules
target_link_libraries(LfoTests PRIVATE
    flowerjuce
    juce::juce_core
    juce::juce_events
    juce::juce_data_structures
    juce::juce_audio_basics
    juce::juce_audio_formats
)

# Define the PannerTests executable
add_executable(PannerTests PannerTests.cpp)

# Link against flowerjuce and JUCE modules
target_link_libraries(PannerTests PRIVATE
    flowerjuce
    juce::juce_core
    juce::juce_events
    juce::juce_data_structures
    juce::juce_audio_basics
    juce::juce_audio_formats
)

# Enable C++17
target_compile_features(LfoTests PRIVATE cxx_std_17)
target_compile_features(PannerTests PRIVATE cxx_std_17)

# Include directories
target_include_directories(LfoTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/libs
)

target_include_directories(PannerTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/libs
)
