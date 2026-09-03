cmake_minimum_required(VERSION 3.22)
project(ReverseVerb VERSION 2.0.0)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(ReverseVerb
    COMPANY_NAME "SheldonDavidson"
    PLUGIN_MANUFACTURER_CODE Shdv
    PLUGIN_CODE Rvrb
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "ReverseVerb"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE)

juce_generate_juce_header(ReverseVerb)

target_sources(ReverseVerb PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(ReverseVerb PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(ReverseVerb
    PRIVATE juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

if(BUILD_TESTING)
    juce_add_console_app(ReverseVerbTests
        PRODUCT_NAME "ReverseVerb Tests")

    juce_generate_juce_header(ReverseVerbTests)

    target_sources(ReverseVerbTests PRIVATE
        Tests/TestMain.cpp)

    target_compile_definitions(ReverseVerbTests PRIVATE
        JUCE_UNIT_TESTS=1
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)

    target_link_libraries(ReverseVerbTests PRIVATE
        juce::juce_core
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

    add_test(NAME ReverseVerbTests COMMAND ReverseVerbTests)
endif()
