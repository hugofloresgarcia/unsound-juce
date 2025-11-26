
cmake_minimum_required(VERSION 3.22)

project(LfoTests)

# Define the test executable
add_executable(LfoTests LfoTests.cpp)

# Link against flowerjuce and JUCE modules
target_link_libraries(LfoTests PRIVATE
    flowerjuce
    juce::juce_core
    juce::juce_events
    juce::juce_data_structures
)

# Enable C++17
target_compile_features(LfoTests PRIVATE cxx_std_17)

# Include directories
target_include_directories(LfoTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/libs
)

