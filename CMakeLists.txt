cmake_minimum_required(VERSION 3.22)
project(ReverseVerb VERSION 1.0.0)

option(RV_BUILD_TESTS "Build the ReverseVerb unit tests" ON)
option(RV_SANITIZE "Build tests with AddressSanitizer + UBSan (GCC/Clang). Disables LTO to keep the build small." OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
FetchContent_Declare(clap-juce-extensions
    GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
    GIT_TAG main
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE clap-juce-extensions)

if(APPLE)
    set(RV_FORMATS ${RV_FORMATS})
else()
    set(RV_FORMATS VST3 Standalone)
endif()

juce_add_plugin(ReverseVerb
    COMPANY_NAME "SheldonDavidson"
    PLUGIN_MANUFACTURER_CODE Shdv
    PLUGIN_CODE Rvrb
    FORMATS ${RV_FORMATS}
    PRODUCT_NAME "ReverseVerb"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)

clap_juce_extensions_plugin(TARGET ReverseVerb
    CLAP_ID "com.circuitdriftlabs.reverseverb"
    CLAP_FEATURES instrument "audio-effect" reverb)

file(GLOB RV_FACTORY_PRESETS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Resources/presets/*.rvpreset")
juce_add_binary_data(ReverseVerbPresets SOURCES ${RV_FACTORY_PRESETS})

juce_generate_juce_header(ReverseVerb)

target_sources(ReverseVerb PRIVATE
    Source/BatchExporter.cpp
    Source/Params.cpp
    Source/PresetManager.cpp
    Source/UndoHistory.cpp
    Source/PluginEditor.cpp
    Source/PluginProcessor.cpp
    Source/Voices.cpp
    Source/dsp/RenderEngine.cpp
    Source/ui/DiffusionShape.cpp
    Source/ui/ExportDialog.cpp
    Source/ui/DragOutPad.cpp
    Source/ui/HelpOverlay.cpp
    Source/ui/LevelMeter.cpp
    Source/ui/PresetBar.cpp
    Source/ui/RVLookAndFeel.cpp
    Source/ui/TensionBox.cpp
    Source/ui/Theme.cpp
    Source/ui/WaveformDisplay.cpp)

target_compile_definitions(ReverseVerb PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

# LTO + AddressSanitizer makes the CI sanitizer build too large for a small runner; skip LTO there.
if(RV_SANITIZE)
    set(RV_LTO_TARGET "")
else()
    set(RV_LTO_TARGET juce::juce_recommended_lto_flags)
endif()

target_link_libraries(ReverseVerb
    PRIVATE ReverseVerbPresets juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags ${RV_LTO_TARGET} juce::juce_recommended_warning_flags)

if(RV_BUILD_TESTS)
    enable_testing()
    juce_add_console_app(ReverseVerbTests PRODUCT_NAME "ReverseVerbTests")
    juce_generate_juce_header(ReverseVerbTests)
    target_sources(ReverseVerbTests PRIVATE
        Tests/BatchTests.cpp
        Tests/PresetTests.cpp
        Tests/RenderTests.cpp
        Tests/UndoTests.cpp
        Tests/UiTests.cpp
        Tests/VoiceTests.cpp
        Tests/TestMain.cpp)
    target_compile_definitions(ReverseVerbTests PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_MODAL_LOOPS_PERMITTED=1
        RV_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/Tests/data")
    target_link_libraries(ReverseVerbTests PRIVATE
        ReverseVerb
        ReverseVerbPresets
        juce::juce_audio_utils
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
    if(RV_SANITIZE AND NOT MSVC)
        target_compile_options(ReverseVerbTests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(ReverseVerbTests PRIVATE -fsanitize=address,undefined)
        target_compile_options(ReverseVerb PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(ReverseVerb PUBLIC -fsanitize=address,undefined)
    endif()
    add_test(NAME ReverseVerbTests COMMAND ReverseVerbTests)
endif()
