cmake_minimum_required(VERSION 3.25)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include (PamplejuceVersion)
include (CPM)
include (PamplejuceMacOS)
include (JUCEDefaults)

set(PROJECT_NAME "TheQueen")
set(PRODUCT_NAME "The Queen")
set(COMPANY_NAME "Lance Shipp")
set(BUNDLE_ID "com.lanceshipp.thequeen")

# AU only exists on macOS; Linux builds (CI containers) produce VST3 + CLAP + Standalone
if (APPLE)
    set(FORMATS AU VST3 Standalone)
else ()
    set(FORMATS VST3 Standalone)
endif ()

set(CMAKE_POLICY_DEFAULT_CMP0141        NEW      CACHE STRING "" FORCE)
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded CACHE STRING "" FORCE)

project(${PROJECT_NAME} VERSION ${CURRENT_VERSION})

include(PamplejuceLog)
include(PamplejuceFunctions)

add_subdirectory(JUCE)
add_subdirectory(modules/clap-juce-extensions EXCLUDE_FROM_ALL)

juce_add_plugin("${PROJECT_NAME}"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/packaging/icon.png"
    COMPANY_NAME "${COMPANY_NAME}"
    BUNDLE_ID "${BUNDLE_ID}"
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE Lshp
    PLUGIN_CODE Quen
    FORMATS "${FORMATS}"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    VST3_CATEGORIES Instrument Synth
    PRODUCT_NAME "${PRODUCT_NAME}")

add_library(SharedCode INTERFACE)

# CLAP is the 4th format (clap-juce-extensions wraps the JUCE plugin)
clap_juce_extensions_plugin(TARGET "${PROJECT_NAME}"
    CLAP_ID "${BUNDLE_ID}"
    CLAP_FEATURES instrument synthesizer)

pamplejuce_shared_code_defaults()

file(GLOB_RECURSE SourceFiles CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/source/*.h")
target_sources(SharedCode INTERFACE ${SourceFiles})
target_include_directories(SharedCode INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/source")

pamplejuce_add_assets()
pamplejuce_xcode_prettify()

target_compile_definitions(SharedCode
    INTERFACE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    CMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}"
    VERSION="${CURRENT_VERSION}"
    PRODUCT_NAME_WITHOUT_VERSION="The Queen"
    QUEEN_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

# Pamplejuce links modules to SharedCode with INTERFACE visibility (juce_dsp included)
target_link_libraries(SharedCode
    INTERFACE
    Assets
    juce_audio_utils
    juce_audio_processors
    juce_dsp
    juce_gui_basics
    juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

target_link_libraries("${PROJECT_NAME}" PRIVATE SharedCode)

pamplejuce_add_tests()
