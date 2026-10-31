# Third-party dependencies. All are fetched at configure time with pinned tags.
# For offline/local builds point FETCHCONTENT_SOURCE_DIR_<NAME> at existing clones, e.g.
#   -DFETCHCONTENT_SOURCE_DIR_JUCE=/path/to/JUCE
include(FetchContent)
set(FETCHCONTENT_QUIET ON)

FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.15
    GIT_SHALLOW    TRUE)

FetchContent_Declare(clap_juce_extensions
    GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
    GIT_TAG        7adee3a1bd4684d4caa5601100e364abccff4b4d
    GIT_SUBMODULES_RECURSE TRUE)

FetchContent_Declare(r8brain
    GIT_REPOSITORY https://github.com/avaneev/r8brain-free-src.git
    GIT_TAG        version-6.5
    GIT_SHALLOW    TRUE)

FetchContent_Declare(pffft
    GIT_REPOSITORY https://github.com/marton78/pffft.git
    GIT_TAG        b3344191adc040a0f4c8553a168ab38b3ef04367)

FetchContent_MakeAvailable(JUCE)
FetchContent_MakeAvailable(clap_juce_extensions)

# r8brain and pffft are compiled directly (we only need a few files).
FetchContent_GetProperties(r8brain)
if(NOT r8brain_POPULATED)
    FetchContent_Populate(r8brain)
endif()
FetchContent_GetProperties(pffft)
if(NOT pffft_POPULATED)
    FetchContent_Populate(pffft)
endif()

add_library(fv_r8brain STATIC ${r8brain_SOURCE_DIR}/r8bbase.cpp)
target_include_directories(fv_r8brain PUBLIC ${r8brain_SOURCE_DIR})
target_compile_definitions(fv_r8brain PUBLIC R8B_PFFFT_DOUBLE=0)
set_target_properties(fv_r8brain PROPERTIES POSITION_INDEPENDENT_CODE ON)

# ---------------------------------------------------------------------------
# FFT backend for the convolution engine:
#   macOS   -> Apple vDSP (Accelerate), double precision
#   Windows -> Intel IPP when FV_USE_IPP and found, otherwise PFFFT (double)
#   Linux   -> PFFFT (double)
# ---------------------------------------------------------------------------
add_library(fv_fft INTERFACE)
if(APPLE)
    target_compile_definitions(fv_fft INTERFACE FV_FFT_VDSP=1)
    target_link_libraries(fv_fft INTERFACE "-framework Accelerate")
else()
    set(FV_IPP_FOUND FALSE)
    if(FV_USE_IPP)
        find_package(IPP QUIET)
        if(IPP_FOUND)
            set(FV_IPP_FOUND TRUE)
            target_compile_definitions(fv_fft INTERFACE FV_FFT_IPP=1)
            target_link_libraries(fv_fft INTERFACE IPP::ipps IPP::ippcore IPP::ippvm)
        else()
            message(WARNING "FV_USE_IPP set but IPP not found - falling back to PFFFT")
        endif()
    endif()
    if(NOT FV_IPP_FOUND)
        add_library(fv_pffft STATIC
            ${pffft_SOURCE_DIR}/src/pffft.c
            ${pffft_SOURCE_DIR}/src/pffft_double.c
            ${pffft_SOURCE_DIR}/src/pffft_common.c)
        target_include_directories(fv_pffft PUBLIC ${pffft_SOURCE_DIR}/include ${pffft_SOURCE_DIR}/include/pffft)
        target_include_directories(fv_pffft PRIVATE ${pffft_SOURCE_DIR}/src)
        set_target_properties(fv_pffft PROPERTIES POSITION_INDEPENDENT_CODE ON C_STANDARD 99)
        target_compile_definitions(fv_pffft PUBLIC PFFFT_STATIC_DEFINE PRIVATE _USE_MATH_DEFINES PFFFT_SCALVEC_ENABLED=1)
        if(MSVC)
            target_compile_options(fv_pffft PRIVATE /arch:AVX)
        elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
            target_compile_options(fv_pffft PRIVATE -O3 -mavx)
        else()
            target_compile_options(fv_pffft PRIVATE -O3)
        endif()
        if(NOT MSVC)
            target_link_libraries(fv_pffft PUBLIC m)
        endif()
        target_compile_definitions(fv_fft INTERFACE FV_FFT_PFFFT=1)
        target_link_libraries(fv_fft INTERFACE fv_pffft)
    endif()
endif()
