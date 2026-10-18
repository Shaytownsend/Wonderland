cmake_minimum_required(VERSION 3.22)

# Must be set before project(): universal binary, macOS 12+.
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build architectures")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS version")
endif()

project(BLDSPlayer VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

#-------------------------------------------------------------------------------
# JUCE, pinned to a release tag.
#-------------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.15
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE)
FetchContent_MakeAvailable(JUCE)

#-------------------------------------------------------------------------------
# Embedded BLDS files. Only the six exact names are accepted; any that are
# present are embedded, missing ones are reported as "No BLDS file" at runtime.
# A generated manifest is always embedded so BinaryData is never empty.
#-------------------------------------------------------------------------------
set(BLDS_RATES 44100 48000 88200 96000 176400 192000)
set(BLDS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/blds_files")
set(BLDS_EMBEDDED_FILES "")
set(BLDS_MANIFEST_TEXT "BLDS Player embedded files:\n")

foreach(rate IN LISTS BLDS_RATES)
    set(f "${BLDS_DIR}/BLDS_${rate}.wav")
    # Re-run configure if a file appears/disappears/changes.
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${BLDS_DIR}")
    if(EXISTS "${f}")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${f}")
        list(APPEND BLDS_EMBEDDED_FILES "${f}")
        file(SHA256 "${f}" sha)
        string(APPEND BLDS_MANIFEST_TEXT "BLDS_${rate}.wav sha256=${sha}\n")
        message(STATUS "BLDS: embedding BLDS_${rate}.wav")
    else()
        string(APPEND BLDS_MANIFEST_TEXT "BLDS_${rate}.wav MISSING\n")
        message(WARNING "BLDS: blds_files/BLDS_${rate}.wav not found - ${rate} Hz will report 'No BLDS file'")
    endif()
endforeach()

set(BLDS_MANIFEST "${CMAKE_CURRENT_BINARY_DIR}/blds_manifest.txt")
file(WRITE "${BLDS_MANIFEST}" "${BLDS_MANIFEST_TEXT}")

juce_add_binary_data(BldsBinaryData
    HEADER_NAME BinaryData.h
    NAMESPACE   BinaryData
    SOURCES     ${BLDS_EMBEDDED_FILES} "${BLDS_MANIFEST}")

if(APPLE)
    set(BLDS_FORMATS AU VST3)
else()
    set(BLDS_FORMATS VST3)   # lets the test target build on Linux CI too
endif()

option(BLDS_ENABLE_LTO "Link-time optimisation for release builds" ON)

set(BLDS_SOURCES
    Source/BldsWav.cpp
    Source/BldsLibrary.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

#-------------------------------------------------------------------------------
# Helper that creates one plugin variant from the shared sources.
#-------------------------------------------------------------------------------
function(blds_add_variant target product code is_effect)
    if(is_effect)
        set(main_type kAudioUnitType_Effect)
        set(vst3_cat Fx Generator)
        set(synth FALSE)
    else()
        set(main_type kAudioUnitType_MusicDevice)
        set(vst3_cat Instrument Generator)
        set(synth TRUE)
    endif()

    juce_add_plugin(${target}
        PRODUCT_NAME                "${product}"
        COMPANY_NAME                "Lnce"
        BUNDLE_ID                   "com.lnce.${target}"
        PLUGIN_MANUFACTURER_CODE    Lnce
        PLUGIN_CODE                 ${code}
        FORMATS                     ${BLDS_FORMATS}
        IS_SYNTH                    ${synth}
        NEEDS_MIDI_INPUT            FALSE
        NEEDS_MIDI_OUTPUT           FALSE
        IS_MIDI_EFFECT              FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        AU_MAIN_TYPE                ${main_type}
        VST3_CATEGORIES             ${vst3_cat}
        HARDENED_RUNTIME_ENABLED    TRUE
        COPY_PLUGIN_AFTER_BUILD     FALSE)

    target_sources(${target} PRIVATE ${BLDS_SOURCES})
    target_include_directories(${target} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/Source")

    target_compile_definitions(${target}
        PUBLIC
            BLDS_EFFECT=$<BOOL:${is_effect}>
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_VST3_CAN_REPLACE_VST2=0)

    target_link_libraries(${target}
        PRIVATE
            BldsBinaryData
        PUBLIC
            juce::juce_audio_utils
            juce::juce_audio_processors
            juce::juce_gui_basics
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)

    if(BLDS_ENABLE_LTO)
        target_link_libraries(${target} PUBLIC juce::juce_recommended_lto_flags)
    endif()
endfunction()

blds_add_variant(BLDSPlayer   "BLDS Player"    Blds FALSE)
blds_add_variant(BLDSPlayerFX "BLDS Player FX" Blde TRUE)

#-------------------------------------------------------------------------------
# Command-line verification (no GUI, no host). One binary per variant, each
# linked against that variant's real shared plugin code.
#-------------------------------------------------------------------------------
enable_testing()

foreach(variant BLDSPlayer BLDSPlayerFX)
    add_executable(${variant}Tests Tests/BldsTests.cpp)
    target_link_libraries(${variant}Tests PRIVATE ${variant} BldsBinaryData
                          juce::juce_recommended_config_flags
                          juce::juce_recommended_warning_flags)
    add_test(NAME ${variant}Tests COMMAND ${variant}Tests)
endforeach()
