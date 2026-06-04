cmake_minimum_required(VERSION 3.22)

project(NorbyAutotune VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.12
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(JUCE)

juce_add_plugin(NorbyAutotune
    COMPANY_NAME "VoxTune"
    BUNDLE_ID "com.voxtune.norbyautotune"
    PLUGIN_MANUFACTURER_CODE VoxT
    PLUGIN_CODE NAut
    FORMATS VST3 Standalone
    PRODUCT_NAME "Norby Autotune"
    VERSION "${PROJECT_VERSION}"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

juce_generate_juce_header(NorbyAutotune)

target_sources(NorbyAutotune
    PRIVATE
        Source/AutotuneEngine.cpp
        Source/FormantShifter.cpp
        Source/GraphicalMode.cpp
        Source/MidiController.cpp
        Source/PitchDetector.cpp
        Source/PitchShifter.cpp
        Source/PluginEditor.cpp
        Source/PluginProcessor.cpp
        Source/ScaleQuantizer.cpp
)

target_compile_definitions(NorbyAutotune
    PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(NorbyAutotune
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_plugin_client
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_dsp
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

