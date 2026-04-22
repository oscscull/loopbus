cmake_minimum_required(VERSION 3.22)
project(LoopBus VERSION 0.1.0)

# JUCE is included as a git submodule in the libs/ directory.
# If you cloned without --recursive, run:
#   git submodule update --init --recursive
add_subdirectory(libs/JUCE)

juce_add_plugin(LoopBus
    COMPANY_NAME "LoopBus"
    PLUGIN_MANUFACTURER_CODE Lpbs
    PLUGIN_CODE Lpb1
    FORMATS VST3
    PRODUCT_NAME "LoopBus"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

target_sources(LoopBus PRIVATE
    src/PluginProcessor.cpp
    src/PluginProcessor.h
    src/LoopBus.h
)

target_compile_features(LoopBus PRIVATE cxx_std_17)

target_compile_definitions(LoopBus PUBLIC
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)

target_link_libraries(LoopBus
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

juce_generate_juce_header(LoopBus)