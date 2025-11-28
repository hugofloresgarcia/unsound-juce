cmake_minimum_required(VERSION 3.22)

# Version is defined here as the single source of truth
# This version is used by:
# - All CMake targets (via ${PROJECT_VERSION})
# - Build scripts (extracted via grep/sed)
# - GitHub releases (via build script)
project(TapeLooper VERSION 0.0.1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_definitions(JUCE_MODAL_LOOPS_PERMITTED)

# Add JUCE
add_subdirectory(JUCE)

# Add libraries
add_subdirectory(third_party)
add_subdirectory(libs)

# Add apps
add_subdirectory(apps)

# Add tests (skip for iOS builds)
if(NOT IOS)
    add_subdirectory(tests)
    
    # Build AudioPluginHost for testing plugins
    add_subdirectory(JUCE/extras/AudioPluginHost)
endif()

# Add plugins
# add_subdirectory(plugins)
