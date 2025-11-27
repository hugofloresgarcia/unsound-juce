# LayerCake app

juce_add_gui_app(LayerCakeApp
    COMPANY_NAME "Unsound"
    PRODUCT_NAME "LayerCake"
    VERSION 0.1.0
    BUNDLE_ID "com.unsound.layercake"
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "LayerCake needs microphone access for audio input"
    REQUIRES_FULL_SCREEN TRUE
    IPAD_SCREEN_ORIENTATIONS UIInterfaceOrientationLandscapeLeft UIInterfaceOrientationLandscapeRight
)

if(IOS)
    set_target_properties(LayerCakeApp PROPERTIES
        XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "2"
        XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "15.0"
        XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "Apple Development"
        XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Automatic"
    )
endif()

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
    KnobRecorderButton.cpp
    KnobRecorderButton.h
    # LFO components
    lfo/LfoAssignmentButton.cpp
    lfo/LfoAssignmentButton.h
    lfo/LayerCakeLfoWidget.cpp
    lfo/LayerCakeLfoWidget.h
    lfo/LfoDragHelpers.h
    lfo/LfoTriggerButton.cpp
    lfo/LfoTriggerButton.h
    lfo/LfoConnectionOverlay.cpp
    lfo/LfoConnectionOverlay.h
    # Keyboard components
    focus/FocusableTarget.h
    focus/FocusRegistry.h
    input/KeyboardCommandRouter.cpp
    input/KeyboardCommandRouter.h
    input/TapTempoController.h
    ui/StatusHUDComponent.cpp
    ui/StatusHUDComponent.h
    ui/CommandPaletteOverlay.cpp
    ui/CommandPaletteOverlay.h
    ui/HelpOverlay.cpp
    ui/HelpOverlay.h
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
