cmake_minimum_required(VERSION 3.22)
project(TensionRise VERSION 1.1.0)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(TensionRise
    COMPANY_NAME "Tellus"
    FORMATS VST3
    PRODUCT_NAME "Tension Rise"
    PLUGIN_MANUFACTURER_CODE Tlls
    PLUGIN_CODE Tnsr
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)

# Logo embutida no plugin (acessível como BinaryData::logo_png)
juce_add_binary_data(TellusAssets SOURCES logo.png)

target_sources(TensionRise PRIVATE
    PluginProcessor.cpp
    PluginProcessor.h
    PluginEditor.cpp
    PluginEditor.h)

target_compile_definitions(TensionRise PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(TensionRise
    PRIVATE
        TellusAssets
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
