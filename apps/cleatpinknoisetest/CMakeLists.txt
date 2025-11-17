# CLEAT Pink Noise Test app

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../Resources/icon.png)
    juce_add_gui_app(CLEATPinkNoiseTestApp
        COMPANY_NAME "Unsound"
        PRODUCT_NAME "CLEAT Pink Noise Test"
        VERSION 1.0.0
        ICON_BIG ${CMAKE_CURRENT_SOURCE_DIR}/../Resources/icon.png
        ICON_SMALL ${CMAKE_CURRENT_SOURCE_DIR}/../Resources/icon.png
    )
else()
    juce_add_gui_app(CLEATPinkNoiseTestApp
        COMPANY_NAME "Unsound"
        PRODUCT_NAME "CLEAT Pink Noise Test"
        VERSION 1.0.0
    )
endif()

# App source files
target_sources(CLEATPinkNoiseTestApp PRIVATE
    Main.cpp
    MainComponent.cpp
    MainComponent.h
)

# Link flowerjuce library (for CustomLookAndFeel)
target_link_libraries(CLEATPinkNoiseTestApp PRIVATE
    flowerjuce
)

# Link JUCE modules
target_link_libraries(CLEATPinkNoiseTestApp PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
)

# Embed binary resources
set(BINARY_DATA_SOURCES)

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../Resources/icon.png)
    list(APPEND BINARY_DATA_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../Resources/icon.png)
endif()

if(BINARY_DATA_SOURCES)
    juce_add_binary_data(CLEATPinkNoiseTestAppBinaryData
        SOURCES ${BINARY_DATA_SOURCES}
    )
    target_link_libraries(CLEATPinkNoiseTestApp PRIVATE CLEATPinkNoiseTestAppBinaryData)
endif()

