cmake_minimum_required(VERSION 3.22)

project(myJV VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.9
    GIT_SHALLOW TRUE)

FetchContent_MakeAvailable(JUCE)

juce_add_plugin(myJV
    COMPANY_NAME "myJV"
    BUNDLE_ID "com.myjv.synth"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Myjv
    PLUGIN_CODE Mjv1
    FORMATS VST3 AU Standalone
    VERSION ${PROJECT_VERSION})

target_sources(myJV PRIVATE
    Source/Plugin/MyJVProcessor.cpp)

target_compile_features(myJV PRIVATE cxx_std_20)

target_compile_definitions(myJV
    PUBLIC
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(myJV
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
