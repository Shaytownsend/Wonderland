cmake_minimum_required(VERSION 3.22)

project(FreddyVerb VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "")
    # Universal binary is set by CI with -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
endif()

option(FV_BUILD_TESTS     "Build the Catch2 test suite"                ON)
option(FV_BUILD_SNAPSHOTS "Build the offscreen UI snapshot tool"       ON)
option(FV_BUILD_IR_FORGE  "Build the offline IR generator (tools/ir_forge)" ON)
option(FV_USE_IPP         "Use Intel IPP for FFT on Windows/Linux when available" OFF)
set(FV_AAX_SDK_PATH "" CACHE PATH "Path to the AAX SDK (AAX is skipped if empty)")

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_C_COMPILER_LAUNCHER   "${CCACHE_PROGRAM}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
endif()

include(cmake/Dependencies.cmake)

# ----------------------------------------------------------------------------
# Plugin formats
# ----------------------------------------------------------------------------
set(FV_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND FV_FORMATS AU)
endif()
if(UNIX AND NOT APPLE)
    list(APPEND FV_FORMATS LV2)
endif()
if(FV_AAX_SDK_PATH AND EXISTS "${FV_AAX_SDK_PATH}")
    juce_set_aax_sdk_path("${FV_AAX_SDK_PATH}")
    list(APPEND FV_FORMATS AAX)
    message(STATUS "FreddyVerb: AAX enabled (${FV_AAX_SDK_PATH})")
else()
    message(STATUS "FreddyVerb: AAX SDK not provided - AAX format skipped (see BLOCKERS.md)")
endif()

juce_add_plugin(FreddyVerb
    COMPANY_NAME                "Freddy Audio"
    COMPANY_WEBSITE             "https://example.invalid/freddyverb"
    BUNDLE_ID                   "com.freddyaudio.freddyverb"
    PLUGIN_MANUFACTURER_CODE    Frdy
    PLUGIN_CODE                 FrVb
    PRODUCT_NAME                "Freddy Verb"
    FORMATS                     ${FV_FORMATS}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx Reverb
    AU_MAIN_TYPE                kAudioUnitType_Effect
    AAX_CATEGORY                AAX_ePlugInCategory_Reverb
    LV2URI                      "https://example.invalid/freddyverb"
    MICROPHONE_PERMISSION_ENABLED TRUE)

clap_juce_extensions_plugin(TARGET FreddyVerb
    CLAP_ID "com.freddyaudio.freddyverb"
    CLAP_FEATURES audio-effect reverb stereo mono)

juce_generate_juce_header(FreddyVerb)

file(GLOB_RECURSE FV_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/*.h)
target_sources(FreddyVerb PRIVATE ${FV_SOURCES})
target_include_directories(FreddyVerb PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Source)

# ----------------------------------------------------------------------------
# Binary resources: fonts, factory IRs, presets, photoreal skin layers
# ----------------------------------------------------------------------------
file(GLOB FV_FONT_FILES   CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Resources/fonts/*.ttf)
file(GLOB FV_IR_FILES     CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Resources/irs/*.flac
                                            ${CMAKE_CURRENT_SOURCE_DIR}/Resources/irs/*.json)
file(GLOB FV_PRESET_FILES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Resources/presets/*.fvpreset)
juce_add_binary_data(FreddyVerbData
    HEADER_NAME FreddyBinaryData.h
    NAMESPACE   FreddyBinaryData
    SOURCES     ${FV_FONT_FILES} ${FV_IR_FILES} ${FV_PRESET_FILES})

# Skins are big, so each photoreal skin gets its own binary-data library and the
# editor only decodes the active one (see ui/themes/photoreal/SkinAssets.cpp).
set(FV_SKIN_LIBS "")
file(GLOB FV_SKIN_DIRS LIST_DIRECTORIES true ${CMAKE_CURRENT_SOURCE_DIR}/Resources/skins/*)
foreach(dir ${FV_SKIN_DIRS})
    if(IS_DIRECTORY ${dir})
        get_filename_component(skinName ${dir} NAME)
        file(GLOB skinFiles CONFIGURE_DEPENDS ${dir}/*.png ${dir}/*.jpg ${dir}/*.json)
        if(skinFiles)
            juce_add_binary_data(FreddySkin_${skinName}
                HEADER_NAME FreddySkin_${skinName}.h
                NAMESPACE   FreddySkin_${skinName}
                SOURCES     ${skinFiles})
            list(APPEND FV_SKIN_LIBS FreddySkin_${skinName})
            target_compile_definitions(FreddyVerb PUBLIC FV_HAS_SKIN_${skinName}=1)
        endif()
    endif()
endforeach()

target_compile_definitions(FreddyVerb
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0
        JUCE_MODAL_LOOPS_PERMITTED=1
        JUCE_STRICT_REFCOUNTEDPOINTER=1
        DONT_SET_USING_JUCE_NAMESPACE=1)

target_link_libraries(FreddyVerb
    PRIVATE
        FreddyVerbData
        ${FV_SKIN_LIBS}
    PUBLIC
        fv_fft
        fv_r8brain
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_opengl
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(MSVC)
    target_compile_options(FreddyVerb PRIVATE /MP)
endif()

# ----------------------------------------------------------------------------
# Tools and tests
# ----------------------------------------------------------------------------
if(FV_BUILD_SNAPSHOTS)
    juce_add_console_app(FreddyVerb_Snapshots PRODUCT_NAME "FreddyVerb_Snapshots")
    target_sources(FreddyVerb_Snapshots PRIVATE tools/snapshots/SnapshotMain.cpp)
    target_compile_definitions(FreddyVerb_Snapshots PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(FreddyVerb_Snapshots PRIVATE FreddyVerb juce::juce_recommended_config_flags)
endif()

if(FV_BUILD_IR_FORGE)
    add_subdirectory(tools/ir_forge)
endif()

if(FV_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
