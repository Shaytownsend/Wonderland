include(FetchContent)
FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.16.1
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(Catch2)

file(GLOB FV_TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
juce_add_console_app(FreddyVerbTests PRODUCT_NAME "FreddyVerbTests")
target_sources(FreddyVerbTests PRIVATE ${FV_TEST_SOURCES})
target_compile_definitions(FreddyVerbTests PRIVATE
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
    FV_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
    FV_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(FreddyVerbTests PRIVATE FreddyVerb Catch2::Catch2 juce::juce_recommended_config_flags)

include(${catch2_SOURCE_DIR}/extras/Catch.cmake)
catch_discover_tests(FreddyVerbTests DISCOVERY_MODE PRE_TEST)
