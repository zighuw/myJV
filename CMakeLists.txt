cmake_minimum_required(VERSION 3.22)

project(myJV VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.9
    GIT_SHALLOW TRUE)

FetchContent_MakeAvailable(JUCE)

set(MYJV_CORE_SOURCES
    Source/Engine/SynthEngine.cpp
    Source/IO/LibraryIndex.cpp
    Source/IO/SampleImporter.cpp
    Source/Model/SampleLibrary.cpp
    Source/Params/ParameterLayout.cpp
    Source/Plugin/MyJVCrashHandler.cpp
    Source/Plugin/MyJVLog.cpp)

set(MYJV_SOURCE_HEADERS
    Source/DSP/Biquad.h
    Source/DSP/Booster.h
    Source/DSP/DeterministicRandom.h
    Source/DSP/Envelope.h
    Source/DSP/FXM.h
    Source/DSP/LFO.h
    Source/DSP/RingMod.h
    Source/DSP/SVF.h
    Source/DSP/SamplePlayer.h
    Source/DSP/Smoother.h
    Source/Engine/AssetReclaimer.h
    Source/Engine/ModulationMatrix.h
    Source/Engine/Note.h
    Source/Engine/SynthEngine.h
    Source/Engine/TonePairProcessor.h
    Source/Engine/ToneVoice.h
    Source/Engine/VoiceManager.h
    Source/IO/LibraryIndex.h
    Source/IO/PatchSerializer.h
    Source/IO/SampleImporter.h
    Source/Model/Patch.h
    Source/Model/Sample.h
    Source/Model/SampleLibrary.h
    Source/Model/Tone.h
    Source/Model/ZoneSet.h
    Source/Params/Calibration.h
    Source/Params/ParamSnapshot.h
    Source/Params/ParameterIDs.h
    Source/Plugin/MyJVCrashHandler.h
    Source/Plugin/MyJVLog.h
    Source/Plugin/MyJVProcessor.h)

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
    ${MYJV_CORE_SOURCES}
    ${MYJV_SOURCE_HEADERS}
    Source/Plugin/MyJVProcessor.cpp)

target_include_directories(myJV PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)

target_compile_features(myJV PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(myJV PRIVATE /utf-8)
endif()

target_compile_definitions(myJV
    PUBLIC
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)

target_link_libraries(myJV
    PRIVATE
        juce::juce_audio_utils
        juce::juce_cryptography
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

include(CTest)

if(BUILD_TESTING)
    FetchContent_Declare(Catch2
        URL https://codeload.github.com/catchorg/Catch2/tar.gz/refs/tags/v3.16.0
        URL_HASH SHA256=0957CAE5821B17CE07F0833AAA52B5137643A8382203221F363A8303C109AF34
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)

    FetchContent_MakeAvailable(Catch2)

    add_executable(myJV_tests
        Tests/diagnostics_tests.cpp
        Tests/engine_tests.cpp
        Tests/importer_tests.cpp
        Tests/library_tests.cpp
        Tests/model_tests.cpp
        Tests/param_tests.cpp
        Tests/test_main.cpp
        ${MYJV_CORE_SOURCES}
        ${MYJV_SOURCE_HEADERS})

    target_include_directories(myJV_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)

    target_compile_features(myJV_tests PRIVATE cxx_std_20)

    if(MSVC)
        target_compile_options(myJV_tests PRIVATE /utf-8)
    endif()

    target_compile_definitions(myJV_tests
        PRIVATE
            MYJV_REPO_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_MODAL_LOOPS_PERMITTED=1)

    target_link_libraries(myJV_tests
        PRIVATE
            Catch2::Catch2
            juce::juce_audio_formats
            juce::juce_audio_processors
            juce::juce_cryptography
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)

    add_test(NAME myJV_tests COMMAND myJV_tests)
endif()
