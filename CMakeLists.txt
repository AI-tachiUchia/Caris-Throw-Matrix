cmake_minimum_required(VERSION 3.22)
project(CarisThrowMatrix VERSION 0.2.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Quick Linux/macOS DSP-only build that never needs JUCE, Xcode, or a network.
option(CARIS_DSP_ONLY "Build only the independent DSP tests" OFF)
if(CARIS_DSP_ONLY)
    enable_testing()
    add_executable(caris_throw_dsp_tests Tests/dsp_tests.cpp Source/ThrowDSP.cpp)
    target_include_directories(caris_throw_dsp_tests PRIVATE Source)
    add_test(NAME native_dsp COMMAND caris_throw_dsp_tests)
    add_executable(caris_throw_demo Tests/render_demo.cpp Source/ThrowDSP.cpp)
    target_include_directories(caris_throw_demo PRIVATE Source)
    return()
endif()

# Supply -DJUCE_DIR=/path/to/JUCE to build offline. Otherwise download JUCE.
if(DEFINED JUCE_DIR AND EXISTS "${JUCE_DIR}/CMakeLists.txt")
    add_subdirectory("${JUCE_DIR}" juce)
else()
    include(FetchContent)
    FetchContent_Declare(juce GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.15 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(juce)
endif()

# Logic Pro uses AU; VST3 supports compatible DAWs.
if(APPLE)
    set(CARIS_FORMATS AU VST3)
else()
    set(CARIS_FORMATS VST3)
endif()

juce_add_plugin(CarisThrowMatrix
    COMPANY_NAME "Caris"
    BUNDLE_ID "me.caris.throwmatrix"
    PLUGIN_MANUFACTURER_CODE Cari
    PLUGIN_CODE ThMx
    PRODUCT_NAME "Caris Throw Matrix"
    FORMATS ${CARIS_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

juce_generate_juce_header(CarisThrowMatrix)

target_sources(CarisThrowMatrix PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/ThrowDSP.cpp
)
# This project has never shipped a VST2 variant. Disable VST2 replacement
# so JUCE does not reject VST3 builds due to incompatible automation IDs.
# Keep this setting stable after shipping releases.
target_compile_definitions(CarisThrowMatrix PUBLIC
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:CarisThrowMatrix,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="${PROJECT_VERSION}"
)
target_link_libraries(CarisThrowMatrix PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_extra
    PUBLIC juce::juce_recommended_config_flags
           juce::juce_recommended_lto_flags
           juce::juce_recommended_warning_flags
)

option(CARIS_BUILD_DSP_TESTS "Build headless DSP checks" ON)
if(CARIS_BUILD_DSP_TESTS)
    enable_testing()
    add_executable(caris_throw_dsp_tests Tests/dsp_tests.cpp Source/ThrowDSP.cpp)
    target_include_directories(caris_throw_dsp_tests PRIVATE Source)
    add_test(NAME native_dsp COMMAND caris_throw_dsp_tests)
endif()
