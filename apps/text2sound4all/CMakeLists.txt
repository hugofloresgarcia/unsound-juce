# Text2Sound4All app

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../Resources/icon.png)
    juce_add_gui_app(Text2Sound4AllApp
        COMPANY_NAME "Unsound"
        PRODUCT_NAME "Text2Sound4All Tape Looper"
        VERSION 1.0.0
        ICON_BIG ${CMAKE_CURRENT_SOURCE_DIR}/../Resources/icon.png
        ICON_SMALL ${CMAKE_CURRENT_SOURCE_DIR}/../Resources/icon.png
    )
else()
    juce_add_gui_app(Text2Sound4AllApp
        COMPANY_NAME "Unsound"
        PRODUCT_NAME "Text2Sound4All Tape Looper"
        VERSION 1.0.0
    )
endif()

# App source files
target_sources(Text2Sound4AllApp PRIVATE
    Main.cpp
    StartupDialog.cpp
    StartupDialog.h
    MainComponent.cpp
    MainComponent.h
    LooperTrack.cpp
    LooperTrack.h
    VizWindow.cpp
    VizWindow.h
)

# Link flowerjuce library
target_link_libraries(Text2Sound4AllApp PRIVATE
    flowerjuce
)

# Link JUCE modules
target_link_libraries(Text2Sound4AllApp PRIVATE
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

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../Resources/icon.png)
    list(APPEND BINARY_DATA_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../Resources/icon.png)
endif()

if(BINARY_DATA_SOURCES)
    juce_add_binary_data(Text2Sound4AllAppBinaryData
        SOURCES ${BINARY_DATA_SOURCES}
    )
    target_link_libraries(Text2Sound4AllApp PRIVATE Text2Sound4AllAppBinaryData)
endif()

