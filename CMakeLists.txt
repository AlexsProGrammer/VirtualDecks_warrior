cmake_minimum_required(VERSION 3.25)

project(juceDjApp VERSION 0.0.1 LANGUAGES C CXX)

# Use modern C++ by default
set(CMAKE_CXX_STANDARD 17)

# Set Release as default build type if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Static linking configuration
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static libraries")

# JUCE static linking options
set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "Don't build JUCE Extras")
set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "Don't build JUCE Examples")

# Windows-specific static runtime settings
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# for clang-tidy (this enables finding system header files)
if(CMAKE_EXPORT_COMPILE_COMMANDS)
    set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES})
endif()

# Prefer SDL2 system includes; fall back to SDL1 if only available.
# Use target include directories later so ordering doesn't globally shadow SDL2.
# (We will attach these to the `juceDjApp` target after it's created.)

# Fetch JUCE, taglib and xwax the same way as the original repo so dependencies
# like utf8cpp are provided automatically.
include(FetchContent)

set(FETCHCONTENT_QUIET OFF)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG origin/master
    GIT_SHALLOW OFF
    FIND_PACKAGE_ARGS 7.0.3 GLOBAL)

FetchContent_Declare(
    taglib
    GIT_REPOSITORY https://github.com/taglib/taglib.git
    GIT_TAG origin/master
    GIT_SHALLOW ON)

FetchContent_Declare(
    xwax
    GIT_REPOSITORY https://github.com/xwax/xwax.git
    GIT_TAG origin/master
    GIT_SHALLOW ON)

FetchContent_MakeAvailable(JUCE taglib xwax)

juce_add_gui_app(juceDjApp)

# Generate the JUCE aggregated header for the top-level app target so
# `#include <JuceHeader.h>` resolves from sources under `src/`
juce_generate_juce_header(juceDjApp)

# Bundle project assets into JUCE BinaryData and expose to the app target
set(LoopStationBinaryData_SOURCES)
if(EXISTS "${CMAKE_SOURCE_DIR}/assets/logo.png")
    list(APPEND LoopStationBinaryData_SOURCES "${CMAKE_SOURCE_DIR}/assets/logo.png")
endif()
if(EXISTS "${CMAKE_SOURCE_DIR}/assets/horizontalKnob.svg")
    list(APPEND LoopStationBinaryData_SOURCES "${CMAKE_SOURCE_DIR}/assets/horizontalKnob.svg")
endif()
if(EXISTS "${CMAKE_SOURCE_DIR}/assets/verticalKnob.svg")
    list(APPEND LoopStationBinaryData_SOURCES "${CMAKE_SOURCE_DIR}/assets/verticalKnob.svg")
endif()
if(EXISTS "${CMAKE_SOURCE_DIR}/assets/playButton.svg")
    list(APPEND LoopStationBinaryData_SOURCES "${CMAKE_SOURCE_DIR}/assets/playButton.svg")
endif()
if(EXISTS "${CMAKE_SOURCE_DIR}/assets/playButtonHover.svg")
    list(APPEND LoopStationBinaryData_SOURCES "${CMAKE_SOURCE_DIR}/assets/playButtonHover.svg")
endif()
if(EXISTS "${CMAKE_SOURCE_DIR}/assets/loadButton.svg")
    list(APPEND LoopStationBinaryData_SOURCES "${CMAKE_SOURCE_DIR}/assets/loadButton.svg")
endif()
if(EXISTS "${CMAKE_SOURCE_DIR}/assets/loadButtonHover.svg")
    list(APPEND LoopStationBinaryData_SOURCES "${CMAKE_SOURCE_DIR}/assets/loadButtonHover.svg")
endif()
if(EXISTS "${CMAKE_SOURCE_DIR}/assets/pauseButton.svg")
    list(APPEND LoopStationBinaryData_SOURCES "${CMAKE_SOURCE_DIR}/assets/pauseButton.svg")
endif()
if(EXISTS "${CMAKE_SOURCE_DIR}/assets/pauseButtonHover.svg")
    list(APPEND LoopStationBinaryData_SOURCES "${CMAKE_SOURCE_DIR}/assets/pauseButtonHover.svg")
endif()

if(LoopStationBinaryData_SOURCES)
    juce_add_binary_data(LoopStationBinaryData SOURCES ${LoopStationBinaryData_SOURCES})
    target_link_libraries(juceDjApp PRIVATE LoopStationBinaryData)
    target_include_directories(juceDjApp PRIVATE "${CMAKE_BINARY_DIR}/LoopStationBinaryData_artefacts/JuceLibraryCode")
endif()

# We want all source code in the src directory
file(GLOB_RECURSE SourceFiles CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h")

# No, we don't want our source buried in extra nested folders
set_target_properties("${PROJECT_NAME}" PROPERTIES FOLDER "")

# The IDE/source tree should look like the source tree
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src PREFIX "Source" FILES ${SourceFiles})
target_sources("${PROJECT_NAME}" PRIVATE ${SourceFiles})

target_link_libraries(juceDjApp PRIVATE
    juce::juce_analytics
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_core
    juce::juce_data_structures
    juce::juce_graphics
    juce::juce_gui_basics
    # juce::juce_gui_extra  # disabled to avoid optional WebKit dependency on this host
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
    tag)

if(taglib_POPULATED)
    get_property(tag_dirs DIRECTORY "${taglib_SOURCE_DIR}/taglib" PROPERTY INCLUDE_DIRECTORIES)
    target_include_directories(juceDjApp PRIVATE ${tag_dirs} "${taglib_SOURCE_DIR}/taglib")
endif()

# Prefer SDL2 headers for xwax; fall back to SDL1 if SDL2 is not available.
if(EXISTS "/usr/include/SDL2")
    target_include_directories(juceDjApp PRIVATE "/usr/include/SDL2")
elseif(EXISTS "/usr/include/SDL")
    target_include_directories(juceDjApp PRIVATE "/usr/include/SDL")
endif()

# Add xwax source/include directories so its local headers (tests/*.h, etc.) resolve.

# Detect GTK (for JUCE GUI on Linux) via pkg-config and add include/link flags.
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(GTK3 QUIET gtk+-3.0)
endif()

if(GTK3_INCLUDE_DIRS)
    target_include_directories(juceDjApp PRIVATE ${GTK3_INCLUDE_DIRS})
endif()

if(GTK3_LIBRARIES)
    target_link_libraries(juceDjApp PRIVATE ${GTK3_LIBRARIES})
endif()

if(GTK3_CFLAGS_OTHER)
    target_compile_options(juceDjApp PRIVATE ${GTK3_CFLAGS_OTHER})
endif()

# Detect WebKit2 (used by juce_gui_extra) and add include/link flags.
if(PkgConfig_FOUND)
    pkg_check_modules(WEBKIT2 QUIET webkit2gtk-4.1)
endif()

if(WEBKIT2_INCLUDE_DIRS)
    target_include_directories(juceDjApp PRIVATE ${WEBKIT2_INCLUDE_DIRS})
endif()

if(WEBKIT2_LIBRARIES)
    target_link_libraries(juceDjApp PRIVATE ${WEBKIT2_LIBRARIES})
endif()

if(WEBKIT2_CFLAGS_OTHER)
    target_compile_options(juceDjApp PRIVATE ${WEBKIT2_CFLAGS_OTHER})
endif()

# Link libcurl if available (used by JUCE network/curl symbols)
find_package(CURL QUIET)
if(CURL_FOUND)
    target_link_libraries(juceDjApp PRIVATE CURL::libcurl)
endif()

# Enable JACK audio backend so JUCE can enumerate PipeWire/JACK sinks.
# On modern Linux, pipewire-jack provides this and exposes individual device
# sinks (Headphones, Speakers, etc.) that are invisible through raw ALSA.
if(PkgConfig_FOUND)
    pkg_check_modules(JACK QUIET jack)
endif()
if(JACK_FOUND)
    message(STATUS "JACK found (${JACK_VERSION}); enabling JUCE_JACK backend")
    target_compile_definitions(juceDjApp PRIVATE JUCE_JACK=1)
    target_include_directories(juceDjApp PRIVATE ${JACK_INCLUDE_DIRS})
    target_link_directories(juceDjApp PRIVATE ${JACK_LIBRARY_DIRS})
    target_link_libraries(juceDjApp PRIVATE ${JACK_LIBRARIES})
else()
    message(STATUS "JACK not found; headphone output will use ALSA only")
endif()
