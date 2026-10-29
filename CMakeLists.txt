cmake_minimum_required(VERSION 3.20)

project(FinalEffect3D
    VERSION 0.1.0
    DESCRIPTION "Final Effect 3D - open-source creative engine (Stage 1 foundation)"
    HOMEPAGE_URL "https://github.com/finalalpha1org-lab/Final-Effect-3D"
    LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Language standard
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20 CACHE STRING "C++ standard used for all Final Effect 3D targets")
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------------------
# Build options
#
# The portable core, the CLI and the tests never need a GPU, a window, a
# network connection or a third-party library. Everything that does is opt-in.
# ---------------------------------------------------------------------------
option(FE3D_BUILD_TESTS              "Build the unit-test suite"                                  ON)
option(FE3D_BUILD_CLI                "Build the headless fe3d_tool command-line application"      ON)
option(FE3D_BUILD_SOFTWARE_RENDERER  "Build the dependency-free CPU reference renderer"           ON)
option(FE3D_BUILD_DESKTOP_APP        "Build the windowed desktop application"                     OFF)
option(FE3D_ENABLE_OPENGL_RENDERER   "Build the OpenGL 3.3 Core desktop renderer (needs GLFW)"    OFF)
option(FE3D_FETCH_GLFW               "Download the pinned GLFW release if no system GLFW exists"  OFF)
option(FE3D_WARNINGS_AS_ERRORS       "Treat compiler warnings as errors"                          OFF)

if(FE3D_BUILD_DESKTOP_APP AND NOT FE3D_ENABLE_OPENGL_RENDERER)
    message(FATAL_ERROR
        "FE3D_BUILD_DESKTOP_APP requires FE3D_ENABLE_OPENGL_RENDERER=ON: "
        "the desktop host has no windowing backend of its own yet.")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerWarnings)
include(Fe3DDependencies)

add_subdirectory(src)
add_subdirectory(apps)

if(FE3D_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ---------------------------------------------------------------------------
# Configuration summary
# ---------------------------------------------------------------------------
message(STATUS "")
message(STATUS "Final Effect 3D ${PROJECT_VERSION}  (${CMAKE_BUILD_TYPE})")
message(STATUS "  compiler            : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  C++ standard        : ${CMAKE_CXX_STANDARD}")
message(STATUS "  tests               : ${FE3D_BUILD_TESTS}")
message(STATUS "  CLI tool            : ${FE3D_BUILD_CLI}")
message(STATUS "  software renderer   : ${FE3D_BUILD_SOFTWARE_RENDERER}")
message(STATUS "  OpenGL renderer     : ${FE3D_ENABLE_OPENGL_RENDERER}")
message(STATUS "  desktop application : ${FE3D_BUILD_DESKTOP_APP}")
message(STATUS "")
