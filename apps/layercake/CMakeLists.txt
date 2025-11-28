# LayerCake Project

# --- Core Sources ---
set(LAYERCAKE_CORE_SOURCES
    core/LayerCakeProcessor.cpp
    core/LayerCakeProcessor.h
    core/LayerCakeComponent.cpp
    core/LayerCakeComponent.h
    core/LayerCakeSettings.h
    core/LayerCakeDisplay.cpp
    core/LayerCakeDisplay.h
    core/LayerCakeLookAndFeel.cpp
    core/LayerCakeLookAndFeel.h
    core/LayerCakeLibraryManager.cpp
    core/LayerCakeLibraryManager.h
    core/LibraryBrowserWindow.cpp
    core/LibraryBrowserWindow.h
    core/LayerCakeKnob.cpp
    core/LayerCakeKnob.h
    core/KnobRecorderButton.cpp
    core/KnobRecorderButton.h
    
    # LFO
    core/lfo/LfoAssignmentButton.cpp
    core/lfo/LfoAssignmentButton.h
    core/lfo/LayerCakeLfoWidget.cpp
    core/lfo/LayerCakeLfoWidget.h
    core/lfo/LfoDragHelpers.h
    core/lfo/LfoTriggerButton.cpp
    core/lfo/LfoTriggerButton.h
    core/lfo/LfoConnectionOverlay.cpp
    core/lfo/LfoConnectionOverlay.h
    
    # Focus/Input/UI
    core/focus/FocusableTarget.h
    core/focus/FocusRegistry.h
    core/input/KeyboardCommandRouter.cpp
    core/input/KeyboardCommandRouter.h
    core/input/TapTempoController.h
    core/ui/StatusHUDComponent.cpp
    core/ui/StatusHUDComponent.h
    core/ui/CommandPaletteOverlay.cpp
    core/ui/CommandPaletteOverlay.h
    core/ui/HelpOverlay.cpp
    core/ui/HelpOverlay.h
)

# Create a static library for core logic
add_library(LayerCakeCore STATIC ${LAYERCAKE_CORE_SOURCES})
target_link_libraries(LayerCakeCore PUBLIC
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
target_include_directories(LayerCakeCore PUBLIC core)

# --- Standalone App ---
juce_add_gui_app(LayerCakeApp
    COMPANY_NAME "Unsound"
    PRODUCT_NAME "LayerCake"
    VERSION 0.1.0
    BUNDLE_ID "com.unsound.layercake"
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "LayerCake needs microphone access for audio input"
    REQUIRES_FULL_SCREEN TRUE
)

target_sources(LayerCakeApp PRIVATE
    standalone/Main.cpp
    standalone/StandaloneSettings.h
)

target_link_libraries(LayerCakeApp PRIVATE LayerCakeCore)

if(IOS)
    set_target_properties(LayerCakeApp PROPERTIES
        XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "2"
        XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "15.0"
        XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "Apple Development"
        XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Automatic"
    )
endif()

# --- Plugin ---
juce_add_plugin(LayerCakePlugin
    COMPANY_NAME "Unsound"
    PLUGIN_NAME "LayerCake"
    PLUGIN_CODE "LyrC"
    PLUGIN_MANUFACTURER_CODE "Unsd"
    VERSION 0.1.0
    FORMATS VST3 AU
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    VST3_CATEGORIES Fx
)

target_compile_definitions(LayerCakePlugin PUBLIC
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_FORCE_USE_LEGACY_PARAM_IDS=1
    JUCE_IGNORE_VST3_MISMATCHED_PARAMETER_ID_WARNING=1
)

target_sources(LayerCakePlugin PRIVATE
    plugin/Main.cpp
)

target_link_libraries(LayerCakePlugin PRIVATE LayerCakeCore)

