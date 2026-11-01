cmake_minimum_required(VERSION 3.22)
project(RectifyDistortion VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)

# Baixa o JUCE automaticamente (precisa de internet e git na primeira vez)
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(RectifyDistortion
    COMPANY_NAME "SeuNome"
    PLUGIN_MANUFACTURER_CODE Demo
    PLUGIN_CODE Rect
    FORMATS VST3 Standalone
    PRODUCT_NAME "Rectify Distortion"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE)

target_sources(RectifyDistortion PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h)

target_compile_definitions(RectifyDistortion PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(RectifyDistortion
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
