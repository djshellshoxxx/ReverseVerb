cmake_minimum_required(VERSION 3.22)
project(ReverseVerb VERSION 1.2.0)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

set(RV_COMMON_SOURCES
    Source/Common/Core.cpp
    Source/Common/Widgets.cpp)

set(RV_DEFS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_MODAL_LOOPS_PERMITTED=0)

# ---------------- Instrument ----------------
juce_add_plugin(ReverseVerb
    COMPANY_NAME "SheldonDavidson"
    PLUGIN_MANUFACTURER_CODE Shdv
    PLUGIN_CODE Rvrb
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "ReverseVerb"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon_small.png"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE)
juce_generate_juce_header(ReverseVerb)
target_sources(ReverseVerb PRIVATE
    Source/Instrument/PluginProcessor.cpp
    Source/Instrument/PluginEditor.cpp
    ${RV_COMMON_SOURCES})
target_compile_definitions(ReverseVerb PUBLIC ${RV_DEFS})
target_link_libraries(ReverseVerb
    PRIVATE juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# ---------------- Effect ----------------
juce_add_plugin(ReverseVerbFX
    COMPANY_NAME "SheldonDavidson"
    PLUGIN_MANUFACTURER_CODE Shdv
    PLUGIN_CODE Rvfx
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "ReverseVerb FX"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon_small.png"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    VST3_CATEGORIES Fx Reverb
    AU_MAIN_TYPE kAudioUnitType_MusicEffect
    COPY_PLUGIN_AFTER_BUILD TRUE)
juce_generate_juce_header(ReverseVerbFX)
target_sources(ReverseVerbFX PRIVATE
    Source/Effect/FxProcessor.cpp
    Source/Effect/FxEditor.cpp
    ${RV_COMMON_SOURCES})
target_compile_definitions(ReverseVerbFX PUBLIC ${RV_DEFS})
target_link_libraries(ReverseVerbFX
    PRIVATE juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
