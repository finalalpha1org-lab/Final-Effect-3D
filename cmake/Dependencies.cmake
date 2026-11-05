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

set(FINALEFFECT_HAS_NATIVE_UI OFF)
set(FINALEFFECT_HAS_IMGUI_OPENGL_GLFW OFF)

if(FINALEFFECT_BUILD_NATIVE_UI AND FINALEFFECT_PLATFORM_IS_DESKTOP)
    include(FetchContent)
    message(STATUS "Final Effect 3D: Fetching pinned Dear ImGui v1.91.8 for Native Desktop Home Hub")
    FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG        v1.91.8
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(imgui)

    set(FINALEFFECT_IMGUI_SOURCES
        "${imgui_SOURCE_DIR}/imgui.cpp"
        "${imgui_SOURCE_DIR}/imgui_draw.cpp"
        "${imgui_SOURCE_DIR}/imgui_tables.cpp"
        "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
        "${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp"
    )

    if(FINALEFFECT_HAS_GLFW)
        list(APPEND FINALEFFECT_IMGUI_SOURCES
            "${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp"
            "${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp"
        )
        set(FINALEFFECT_HAS_IMGUI_OPENGL_GLFW ON)
    endif()

    add_library(finaleffect_thirdparty_imgui STATIC ${FINALEFFECT_IMGUI_SOURCES})
    set_target_properties(finaleffect_thirdparty_imgui PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        POSITION_INDEPENDENT_CODE ON
    )
    target_include_directories(finaleffect_thirdparty_imgui SYSTEM
        PUBLIC
            "${imgui_SOURCE_DIR}"
            "${imgui_SOURCE_DIR}/backends"
            "${imgui_SOURCE_DIR}/misc/cpp"
    )
    if(MSVC)
        target_compile_options(finaleffect_thirdparty_imgui PRIVATE /w /EHsc /utf-8)
        target_compile_definitions(finaleffect_thirdparty_imgui PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        target_compile_options(finaleffect_thirdparty_imgui PRIVATE -w)
    endif()

    if(FINALEFFECT_HAS_GLFW)
        target_link_libraries(finaleffect_thirdparty_imgui PUBLIC glfw OpenGL::GL)
    endif()

    # Generate embedded TrueType font byte arrays from ImGui's bundled SIL OFL / Apache-2.0 fonts
    # so NativeHomeTheme always renders crisp anti-aliased typography across Windows, macOS, and Linux
    # without external runtime file-path dependencies or binary blobs in Git.
    set(FINALEFFECT_GENERATED_FONT_HEADER "${CMAKE_BINARY_DIR}/generated/finaleffect_embedded_fonts.hpp")
    if(NOT EXISTS "${FINALEFFECT_GENERATED_FONT_HEADER}")
        file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated")
        file(READ "${imgui_SOURCE_DIR}/misc/fonts/Karla-Regular.ttf" _fe_karla_hex HEX)
        string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," _fe_karla_bytes "${_fe_karla_hex}")
        file(READ "${imgui_SOURCE_DIR}/misc/fonts/Roboto-Medium.ttf" _fe_roboto_hex HEX)
        string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," _fe_roboto_bytes "${_fe_roboto_hex}")
        file(READ "${imgui_SOURCE_DIR}/misc/fonts/Cousine-Regular.ttf" _fe_cousine_hex HEX)
        string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," _fe_cousine_bytes "${_fe_cousine_hex}")

        file(WRITE "${FINALEFFECT_GENERATED_FONT_HEADER}"
"// Auto-generated at CMake configure time from Dear ImGui misc/fonts (Karla SIL OFL, Roboto & Cousine Apache-2.0).\n"
"#pragma once\n"
"#include <cstddef>\n\n"
"namespace finaleffect::ui::embedded_fonts {\n"
"inline constexpr unsigned char kKarlaRegularTtf[] = { ${_fe_karla_bytes} };\n"
"inline constexpr std::size_t kKarlaRegularTtfSize = sizeof(kKarlaRegularTtf);\n"
"inline constexpr unsigned char kRobotoMediumTtf[] = { ${_fe_roboto_bytes} };\n"
"inline constexpr std::size_t kRobotoMediumTtfSize = sizeof(kRobotoMediumTtf);\n"
"inline constexpr unsigned char kCousineRegularTtf[] = { ${_fe_cousine_bytes} };\n"
"inline constexpr std::size_t kCousineRegularTtfSize = sizeof(kCousineRegularTtf);\n"
"} // namespace finaleffect::ui::embedded_fonts\n")
        unset(_fe_karla_hex)
        unset(_fe_karla_bytes)
        unset(_fe_roboto_hex)
        unset(_fe_roboto_bytes)
        unset(_fe_cousine_hex)
        unset(_fe_cousine_bytes)
    endif()

    target_include_directories(finaleffect_thirdparty_imgui SYSTEM PUBLIC "${CMAKE_BINARY_DIR}/generated")
    set(FINALEFFECT_HAS_NATIVE_UI ON)
endif()
