# SPDX-License-Identifier: GPL-3.0-or-later
# Final Effect 3D - Stage 1 Engine Foundation & Cross-Platform Build Architecture

cmake_minimum_required(VERSION 3.22)

project(FinalEffect3D
    VERSION 0.5.0
    DESCRIPTION "Final Effect 3D - Open-Source Creative 3D Engine & Studio Foundation"
    LANGUAGES CXX
)

# Enforce out-of-source builds to keep repository working tree clean.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR
        "In-source builds are not permitted in Final Effect 3D. "
        "Please configure an out-of-source build directory, for example:\n"
        "  cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# ------------------------------------------------------------------------------
# Project Build Options
# ------------------------------------------------------------------------------
option(FINALEFFECT_BUILD_TESTS "Build Final Effect 3D automated unit and integration tests" ON)
option(FINALEFFECT_BUILD_RENDERER_DESKTOP "Build desktop rendering backend and desktop host application" ON)
option(FINALEFFECT_BUILD_RENDERER_WEBGL2 "Build WebGL 2.0 / OpenGL ES 3.0 render device adapter" ON)
option(FINALEFFECT_FETCH_GLFW "Fetch pinned GLFW 3.4 from GitHub if system GLFW is not installed" OFF)
option(FINALEFFECT_REQUIRE_GLFW "Fail configuration if GLFW + system OpenGL are unavailable when desktop renderer is enabled" OFF)

include(PlatformDetection)
include(CompilerWarnings)
include(Dependencies)

if(NOT FINALEFFECT_PLATFORM_IS_WEB)
    find_package(Threads REQUIRED)
endif()

# ------------------------------------------------------------------------------
# Platform Abstraction Module (finaleffect_platform)
# Selects OS-isolated implementation file in CMake without #ifdef clutter.
# ------------------------------------------------------------------------------
set(FINALEFFECT_PLATFORM_PATHS_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/platform_paths_generic.cpp")
if(FINALEFFECT_PLATFORM_NAME STREQUAL "Linux")
    set(FINALEFFECT_PLATFORM_PATHS_SRC "${CMAKE_CURRENT_SOURCE_DIR}/platform/linux/platform_paths_linux.cpp")
elseif(FINALEFFECT_PLATFORM_NAME STREQUAL "Windows")
    set(FINALEFFECT_PLATFORM_PATHS_SRC "${CMAKE_CURRENT_SOURCE_DIR}/platform/windows/platform_paths_windows.cpp")
elseif(FINALEFFECT_PLATFORM_NAME STREQUAL "macOS")
    set(FINALEFFECT_PLATFORM_PATHS_SRC "${CMAKE_CURRENT_SOURCE_DIR}/platform/macos/platform_paths_macos.cpp")
endif()

add_library(finaleffect_platform STATIC
    src/platform/local_file_system.cpp
    src/platform/steady_clock.cpp
    src/platform/headless_window.cpp
    "${FINALEFFECT_PLATFORM_PATHS_SRC}"
)
add_library(finaleffect::platform ALIAS finaleffect_platform)
target_include_directories(finaleffect_platform
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
finaleffect_apply_compiler_settings(finaleffect_platform)

# ------------------------------------------------------------------------------
# Core Engine Module (finaleffect_core)
# ------------------------------------------------------------------------------
add_library(finaleffect_core STATIC
    src/core/version.cpp
    src/core/logger.cpp
    src/core/engine.cpp
)
add_library(finaleffect::core ALIAS finaleffect_core)
target_include_directories(finaleffect_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(finaleffect_core
    PUBLIC
        finaleffect::platform
)
if(NOT FINALEFFECT_PLATFORM_IS_WEB)
    target_link_libraries(finaleffect_core PUBLIC Threads::Threads)
endif()
finaleffect_apply_compiler_settings(finaleffect_core)

# ------------------------------------------------------------------------------
# Math Module (finaleffect_math)
# ------------------------------------------------------------------------------
add_library(finaleffect_math STATIC
    src/math/quat.cpp
    src/math/mat4.cpp
    src/math/transform.cpp
    src/math/geometry.cpp
)
add_library(finaleffect::math ALIAS finaleffect_math)
target_include_directories(finaleffect_math
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
finaleffect_apply_compiler_settings(finaleffect_math)

# ------------------------------------------------------------------------------
# Scene Module (finaleffect_scene)
# ------------------------------------------------------------------------------
add_library(finaleffect_scene STATIC
    src/scene/editable_mesh.cpp
    src/scene/environment.cpp
    src/scene/primitives.cpp
    src/scene/scene.cpp
    src/scene/picking.cpp
    src/scene/commands.cpp
)
add_library(finaleffect::scene ALIAS finaleffect_scene)
target_include_directories(finaleffect_scene
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(finaleffect_scene
    PUBLIC
        finaleffect::core
        finaleffect::math
        finaleffect::platform
)
finaleffect_apply_compiler_settings(finaleffect_scene)

# ------------------------------------------------------------------------------
# Project Serialization Module (finaleffect_project)
# ------------------------------------------------------------------------------
add_library(finaleffect_project STATIC
    src/project/json.cpp
    src/project/binary_asset.cpp
    src/project/project_serializer.cpp
)
add_library(finaleffect::project ALIAS finaleffect_project)
target_include_directories(finaleffect_project
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(finaleffect_project
    PUBLIC
        finaleffect::scene
        finaleffect::core
        finaleffect::platform
)
finaleffect_apply_compiler_settings(finaleffect_project)

# ------------------------------------------------------------------------------
# Portable Rendering Abstraction Module (finaleffect_rendering)
# Graphics-API-agnostic renderer + Null & CPU Reference Software devices.
# ------------------------------------------------------------------------------
add_library(finaleffect_rendering STATIC
    src/rendering/editor_grid.cpp
    src/rendering/lighting_system.cpp
    src/rendering/null_render_device.cpp
    src/rendering/software_render_device.cpp
    src/rendering/renderer.cpp
    src/rendering/material_preview.cpp
)
add_library(finaleffect::rendering ALIAS finaleffect_rendering)
target_include_directories(finaleffect_rendering
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(finaleffect_rendering
    PUBLIC
        finaleffect::scene
        finaleffect::math
        finaleffect::core
)
finaleffect_apply_compiler_settings(finaleffect_rendering)

# ------------------------------------------------------------------------------
# Portable Application Session Module (finaleffect_app)
# ------------------------------------------------------------------------------
add_library(finaleffect_app STATIC
    src/app/orbit_camera_controller.cpp
    src/app/starter_scene.cpp
    src/app/transform_gizmo.cpp
    src/app/viewport_scene_helpers.cpp
    src/app/workspace_manager.cpp
    src/app/studio_session.cpp
    src/app/editor_controller.cpp
    src/app/editor_controller_json.cpp
)
add_library(finaleffect::app ALIAS finaleffect_app)
target_include_directories(finaleffect_app
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(finaleffect_app
    PUBLIC
        finaleffect::rendering
        finaleffect::project
        finaleffect::scene
        finaleffect::core
        finaleffect::platform
)
finaleffect_apply_compiler_settings(finaleffect_app)

# ------------------------------------------------------------------------------
# Optional Desktop Rendering & Host Targets (FINALEFFECT_BUILD_RENDERER_DESKTOP)
# ------------------------------------------------------------------------------
if(FINALEFFECT_BUILD_RENDERER_DESKTOP AND FINALEFFECT_PLATFORM_IS_DESKTOP)
    add_library(finaleffect_rendering_opengl33 STATIC
        src/rendering/opengl33_render_device.cpp
    )
    add_library(finaleffect::rendering_opengl33 ALIAS finaleffect_rendering_opengl33)
    target_include_directories(finaleffect_rendering_opengl33
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    )
    target_link_libraries(finaleffect_rendering_opengl33
        PUBLIC
            finaleffect::rendering
            finaleffect::platform
    )
    finaleffect_apply_compiler_settings(finaleffect_rendering_opengl33)

    add_executable(finaleffect_desktop
        apps/desktop/main.cpp
    )
    target_link_libraries(finaleffect_desktop
        PRIVATE
            finaleffect::app
            finaleffect::rendering_opengl33
    )
    finaleffect_apply_compiler_settings(finaleffect_desktop)

    if(FINALEFFECT_HAS_GLFW)
        add_library(finaleffect_window_glfw STATIC
            src/platform/glfw_window.cpp
        )
        target_include_directories(finaleffect_window_glfw
            PUBLIC
                $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                $<INSTALL_INTERFACE:include>
        )
        target_link_libraries(finaleffect_window_glfw
            PUBLIC
                finaleffect::platform
            PRIVATE
                glfw
                OpenGL::GL
        )
        finaleffect_apply_compiler_settings(finaleffect_window_glfw)

        target_compile_definitions(finaleffect_desktop PRIVATE FINALEFFECT_HAS_GLFW_WINDOW=1)
        target_link_libraries(finaleffect_desktop PRIVATE finaleffect_window_glfw)
    endif()
endif()

# ------------------------------------------------------------------------------
# WebGL 2.0 / OpenGL ES 3.0 Adapter & Emscripten Browser Prototype Target
# ------------------------------------------------------------------------------
if(FINALEFFECT_BUILD_RENDERER_WEBGL2 OR FINALEFFECT_PLATFORM_IS_WEB)
    add_library(finaleffect_rendering_webgl2 STATIC
        src/rendering/webgl2_render_device.cpp
    )
    add_library(finaleffect::rendering_webgl2 ALIAS finaleffect_rendering_webgl2)
    target_include_directories(finaleffect_rendering_webgl2
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    )
    target_link_libraries(finaleffect_rendering_webgl2
        PUBLIC
            finaleffect::rendering
            finaleffect::platform
    )
    finaleffect_apply_compiler_settings(finaleffect_rendering_webgl2)
endif()

if(FINALEFFECT_PLATFORM_IS_WEB)
    add_executable(finaleffect_web
        apps/web/main.cpp
    )
    target_link_libraries(finaleffect_web
        PRIVATE
            finaleffect::app
            finaleffect::rendering_webgl2
    )
    finaleffect_apply_compiler_settings(finaleffect_web)
    target_link_options(finaleffect_web
        PRIVATE
            "-sUSE_WEBGL2=1"
            "-sMIN_WEBGL_VERSION=2"
            "-sMAX_WEBGL_VERSION=2"
            "-sALLOW_MEMORY_GROWTH=1"
            "-sINITIAL_MEMORY=33554432"
            "-sSTACK_SIZE=1048576"
            "-sFILESYSTEM=1"
            "-sEXPORTED_FUNCTIONS=['_main','_malloc','_free','_fe3d_is_ready','_fe3d_get_boot_stage','_fe3d_get_boot_message','_fe3d_get_init_error','_fe3d_get_state_json','_fe3d_dispatch_action','_fe3d_export_inline_project','_fe3d_save_bundle_to_vfs','_fe3d_load_file_from_vfs','_fe3d_state_revision','_fe3d_material_preview_ptr','_fe3d_material_preview_width','_fe3d_material_preview_height','_fe3d_material_preview_generation','_fe3d_export_workspace_json','_fe3d_hit_test_gizmo','_fe3d_touch_tap','_fe3d_pointer_event','_fe3d_camera_orbit','_fe3d_editor_camera_orbit','_fe3d_camera_pan','_fe3d_camera_zoom','_fe3d_render_final_frame','_fe3d_load_hdri_bytes']"
            "-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap','UTF8ToString','stringToNewUTF8','FS','HEAPU8']"
    )
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/web/index.html"
        "${CMAKE_CURRENT_BINARY_DIR}/index.html"
        COPYONLY
    )
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/web/editor.css"
        "${CMAKE_CURRENT_BINARY_DIR}/editor.css"
        COPYONLY
    )
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/web/studios.js"
        "${CMAKE_CURRENT_BINARY_DIR}/studios.js"
        COPYONLY
    )
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/web/editor.js"
        "${CMAKE_CURRENT_BINARY_DIR}/editor.js"
        COPYONLY
    )
endif()

# ------------------------------------------------------------------------------
# Automated Tests
# ------------------------------------------------------------------------------
if(FINALEFFECT_BUILD_TESTS)
    enable_testing()

    add_executable(finaleffect_core_tests tests/core_tests.cpp)
    target_link_libraries(finaleffect_core_tests PRIVATE finaleffect::core)
    finaleffect_apply_compiler_settings(finaleffect_core_tests)
    add_test(NAME finaleffect.core COMMAND finaleffect_core_tests)

    add_executable(finaleffect_platform_tests tests/platform_tests.cpp)
    target_link_libraries(finaleffect_platform_tests PRIVATE finaleffect::platform finaleffect::core)
    finaleffect_apply_compiler_settings(finaleffect_platform_tests)
    add_test(NAME finaleffect.platform COMMAND finaleffect_platform_tests)

    add_executable(finaleffect_math_tests tests/math_tests.cpp)
    target_link_libraries(finaleffect_math_tests PRIVATE finaleffect::math)
    finaleffect_apply_compiler_settings(finaleffect_math_tests)
    add_test(NAME finaleffect.math COMMAND finaleffect_math_tests)

    add_executable(finaleffect_scene_tests tests/scene_tests.cpp)
    target_link_libraries(finaleffect_scene_tests PRIVATE finaleffect::scene)
    finaleffect_apply_compiler_settings(finaleffect_scene_tests)
    add_test(NAME finaleffect.scene COMMAND finaleffect_scene_tests)

    add_executable(finaleffect_command_tests tests/command_tests.cpp)
    target_link_libraries(finaleffect_command_tests PRIVATE finaleffect::scene)
    finaleffect_apply_compiler_settings(finaleffect_command_tests)
    add_test(NAME finaleffect.commands COMMAND finaleffect_command_tests)

    add_executable(finaleffect_project_tests tests/project_tests.cpp)
    target_link_libraries(finaleffect_project_tests PRIVATE finaleffect::project)
    finaleffect_apply_compiler_settings(finaleffect_project_tests)
    add_test(NAME finaleffect.project COMMAND finaleffect_project_tests)

    add_executable(finaleffect_rendering_tests tests/rendering_tests.cpp)
    target_link_libraries(finaleffect_rendering_tests PRIVATE finaleffect::app finaleffect::rendering)
    finaleffect_apply_compiler_settings(finaleffect_rendering_tests)
    add_test(NAME finaleffect.rendering COMMAND finaleffect_rendering_tests)

    add_executable(finaleffect_viewport_tests tests/viewport_tests.cpp)
    target_link_libraries(finaleffect_viewport_tests PRIVATE finaleffect::app finaleffect::rendering)
    finaleffect_apply_compiler_settings(finaleffect_viewport_tests)
    add_test(NAME finaleffect.viewport COMMAND finaleffect_viewport_tests)

    add_executable(finaleffect_editor_tests tests/editor_tests.cpp)
    target_link_libraries(finaleffect_editor_tests PRIVATE finaleffect::app finaleffect::rendering)
    finaleffect_apply_compiler_settings(finaleffect_editor_tests)
    add_test(NAME finaleffect.editor COMMAND finaleffect_editor_tests)

    add_executable(finaleffect_gizmo_tests tests/gizmo_tests.cpp)
    target_link_libraries(finaleffect_gizmo_tests PRIVATE finaleffect::app finaleffect::rendering)
    finaleffect_apply_compiler_settings(finaleffect_gizmo_tests)
    add_test(NAME finaleffect.gizmo COMMAND finaleffect_gizmo_tests)

    add_executable(finaleffect_mesh_edit_tests tests/mesh_edit_tests.cpp)
    target_link_libraries(finaleffect_mesh_edit_tests PRIVATE finaleffect::scene)
    finaleffect_apply_compiler_settings(finaleffect_mesh_edit_tests)
    add_test(NAME finaleffect.mesh_edit COMMAND finaleffect_mesh_edit_tests)

    add_executable(finaleffect_material_tests tests/material_tests.cpp)
    target_link_libraries(finaleffect_material_tests PRIVATE finaleffect::scene)
    finaleffect_apply_compiler_settings(finaleffect_material_tests)
    add_test(NAME finaleffect.material COMMAND finaleffect_material_tests)

    add_executable(finaleffect_workspace_tests tests/workspace_tests.cpp)
    target_link_libraries(finaleffect_workspace_tests PRIVATE finaleffect::app)
    finaleffect_apply_compiler_settings(finaleffect_workspace_tests)
    add_test(NAME finaleffect.workspace COMMAND finaleffect_workspace_tests)

    add_executable(finaleffect_camera_light_viewport_tests tests/camera_light_viewport_tests.cpp)
    target_link_libraries(finaleffect_camera_light_viewport_tests PRIVATE finaleffect::app finaleffect::rendering)
    finaleffect_apply_compiler_settings(finaleffect_camera_light_viewport_tests)
    add_test(NAME finaleffect.camera_light_viewport COMMAND finaleffect_camera_light_viewport_tests)

    add_executable(finaleffect_editor_grid_tests tests/editor_grid_tests.cpp)
    target_link_libraries(finaleffect_editor_grid_tests PRIVATE finaleffect::app finaleffect::rendering)
    finaleffect_apply_compiler_settings(finaleffect_editor_grid_tests)
    add_test(NAME finaleffect.editor_grid COMMAND finaleffect_editor_grid_tests)

    add_executable(finaleffect_lighting_engine_tests tests/lighting_engine_tests.cpp)
    target_link_libraries(finaleffect_lighting_engine_tests PRIVATE finaleffect::app finaleffect::rendering)
    finaleffect_apply_compiler_settings(finaleffect_lighting_engine_tests)
    add_test(NAME finaleffect.lighting_engine COMMAND finaleffect_lighting_engine_tests)

    if(FINALEFFECT_BUILD_RENDERER_DESKTOP AND FINALEFFECT_PLATFORM_IS_DESKTOP)
        add_executable(finaleffect_opengl33_tests tests/opengl33_device_tests.cpp)
        target_link_libraries(finaleffect_opengl33_tests PRIVATE finaleffect::rendering_opengl33)
        finaleffect_apply_compiler_settings(finaleffect_opengl33_tests)
        add_test(NAME finaleffect.rendering_opengl33 COMMAND finaleffect_opengl33_tests)
    endif()

    if(FINALEFFECT_BUILD_RENDERER_WEBGL2 AND NOT FINALEFFECT_PLATFORM_IS_WEB)
        add_executable(finaleffect_webgl2_tests tests/webgl2_device_tests.cpp)
        target_link_libraries(finaleffect_webgl2_tests PRIVATE finaleffect::rendering_webgl2)
        finaleffect_apply_compiler_settings(finaleffect_webgl2_tests)
        add_test(NAME finaleffect.rendering_webgl2 COMMAND finaleffect_webgl2_tests)
    endif()
endif()
