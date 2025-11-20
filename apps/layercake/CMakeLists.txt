# LayerCake app

juce_add_gui_app(LayerCakeApp
    COMPANY_NAME "Unsound"
    PRODUCT_NAME "LayerCake"
    VERSION 0.1.0
)

target_sources(LayerCakeApp PRIVATE
    Main.cpp
    MainComponent.cpp
    MainComponent.h
    LayerCakeDisplay.cpp
    LayerCakeDisplay.h
    LayerCakeLookAndFeel.cpp
    LayerCakeLookAndFeel.h
        LayerCakeLibraryManager.cpp
        LayerCakeLibraryManager.h
    LibraryBrowserWindow.cpp
    LibraryBrowserWindow.h
    LayerCakeKnob.cpp
    LayerCakeKnob.h
)

target_link_libraries(LayerCakeApp PRIVATE
    flowerjuce
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


