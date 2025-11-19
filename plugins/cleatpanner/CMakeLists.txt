cmake_minimum_required(VERSION 3.22)

project(CLEATPannerPlugin VERSION 1.0.0)

# Add the plugin using JUCE's plugin helper
juce_add_plugin(CLEATPannerPlugin
    COMPANY_NAME "Unsound"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    PLUGIN_MANUFACTURER_CODE Uns
    PLUGIN_CODE ClPn
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "CLEAT Panner")

# Add source files
target_sources(CLEATPannerPlugin
    PRIVATE
        PluginProcessor.cpp
        PluginEditor.cpp)

# Compile definitions
target_compile_definitions(CLEATPannerPlugin
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

# Link libraries
target_link_libraries(CLEATPannerPlugin
    PRIVATE
        flowerjuce
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)


