cmake_minimum_required(VERSION 3.22)

project(AtlantaMachine VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.6
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(AtlantaMachine
    COMPANY_NAME                "GOLDMINE AUDIO"
    PRODUCT_NAME                "ATLANTA MACHINE"
    PLUGIN_MANUFACTURER_CODE    Gmau
    PLUGIN_CODE                 Atmc
    FORMATS                     VST3
    VST3_CATEGORIES             Instrument Sampler
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD     FALSE
)

target_sources(AtlantaMachine PRIVATE
    Source/Pad.cpp
    Source/SampleVoice.cpp
    Source/AudioEngine.cpp
    Source/Effects.cpp
    Source/Sequencer.cpp
    Source/MidiManager.cpp
    Source/FactoryKits.cpp
    Source/DrumKitManager.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

target_include_directories(AtlantaMachine PRIVATE Source)

target_compile_definitions(AtlantaMachine PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_MODAL_LOOPS_PERMITTED=0
)

if(MSVC)
    target_compile_options(AtlantaMachine PRIVATE /utf-8)
endif()

target_link_libraries(AtlantaMachine
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_formats
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
