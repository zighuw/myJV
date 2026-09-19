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

add_library(myJV_core STATIC
    Source/Params/Params.cpp
    Source/Model/Model.cpp
    Source/Engine/Engine.cpp
    Source/DSP/DSP.cpp
    Source/IO/IO.cpp)

target_include_directories(myJV_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Source)

set_target_properties(myJV_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_compile_features(myJV_core PRIVATE cxx_std_20)

target_link_libraries(myJV_core
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

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
        myJV_core
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
