# SPDX-License-Identifier: GPL-3.0-or-later
# Final Effect 3D - Explicit external dependency resolution.
#
# Core engine libraries and unit tests require ZERO external dependencies and
# ZERO network access.
#
# Desktop windowing/context creation optionally uses system-installed GLFW (>= 3.3)
# and OpenGL when FINALEFFECT_BUILD_RENDERER_DESKTOP=ON. If FINALEFFECT_FETCH_GLFW=ON
# is explicitly requested by a developer, GLFW 3.4 (zlib/libpng license) is fetched
# from its pinned upstream release tag.

include_guard(GLOBAL)

set(FINALEFFECT_HAS_GLFW OFF)
set(FINALEFFECT_HAS_SYSTEM_OPENGL OFF)

if(FINALEFFECT_BUILD_RENDERER_DESKTOP AND FINALEFFECT_PLATFORM_IS_DESKTOP)
    find_package(OpenGL QUIET)
    if(OpenGL_FOUND)
        set(FINALEFFECT_HAS_SYSTEM_OPENGL ON)
    endif()

    find_package(glfw3 3.3 QUIET CONFIG)
    if(NOT glfw3_FOUND)
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(GLFW3 QUIET IMPORTED_TARGET glfw3>=3.3)
            if(GLFW3_FOUND AND NOT TARGET glfw)
                add_library(glfw INTERFACE IMPORTED)
                target_link_libraries(glfw INTERFACE PkgConfig::GLFW3)
                set(glfw3_FOUND ON)
            endif()
        endif()
    endif()

    if(NOT glfw3_FOUND AND FINALEFFECT_FETCH_GLFW)
        include(FetchContent)
        message(STATUS "Final Effect 3D: Fetching pinned GLFW 3.4 from https://github.com/glfw/glfw.git")
        set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(
            glfw
            GIT_REPOSITORY https://github.com/glfw/glfw.git
            GIT_TAG        3.4
            GIT_SHALLOW    TRUE
        )
        FetchContent_MakeAvailable(glfw)
        set(glfw3_FOUND ON)
    endif()

    if(glfw3_FOUND AND FINALEFFECT_HAS_SYSTEM_OPENGL)
        set(FINALEFFECT_HAS_GLFW ON)
        message(STATUS "Final Effect 3D: GLFW + system OpenGL found; interactive OpenGL 3.3 window backend enabled.")
    else()
        if(FINALEFFECT_REQUIRE_GLFW)
            message(FATAL_ERROR
                "FINALEFFECT_REQUIRE_GLFW is ON, but GLFW (>= 3.3) and/or system OpenGL development "
                "packages were not found (OpenGL_FOUND=${OpenGL_FOUND}, glfw3_FOUND=${glfw3_FOUND}). "
                "Install system packages (e.g., libglfw3-dev and libgl1-mesa-dev on Debian/Ubuntu) "
                "or configure with -DFINALEFFECT_BUILD_RENDERER_DESKTOP=OFF for core-only mode.")
        endif()
        message(STATUS
            "Final Effect 3D: System GLFW/OpenGL dev packages not present "
            "(OpenGL_FOUND=${OpenGL_FOUND}, glfw3_FOUND=${glfw3_FOUND}). "
            "Building OpenGL 3.3 device module + headless/software desktop runner without GLFW windowing.")
    endif()
endif()
