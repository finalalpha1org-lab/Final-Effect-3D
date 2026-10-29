# Final Effect 3D - engine libraries.
# Dependency direction (never reversed):
#   core <- math <- scene <- rendering <- engine
#   core <- platform, core <- project (project <- scene, platform)

add_library(fe3d_core
    core/Version.cpp
    core/Error.cpp
    core/Log.cpp
    core/Json.cpp)
target_include_directories(fe3d_core PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(fe3d_core PUBLIC Threads::Threads)
fe3d_apply_warnings(fe3d_core)
add_library(fe3d::core ALIAS fe3d_core)

add_library(fe3d_math
    math/Quat.cpp
    math/Mat4.cpp
    math/Transform.cpp)
target_link_libraries(fe3d_math PUBLIC fe3d::core)
fe3d_apply_warnings(fe3d_math)
add_library(fe3d::math ALIAS fe3d_math)

add_library(fe3d_scene
    scene/Scene.cpp)
target_link_libraries(fe3d_scene PUBLIC fe3d::math)
fe3d_apply_warnings(fe3d_scene)
add_library(fe3d::scene ALIAS fe3d_scene)

if(WIN32)
    set(FE3D_PLATFORM_PATHS_SOURCE platform/windows/WindowsPaths.cpp)
else()
    set(FE3D_PLATFORM_PATHS_SOURCE platform/posix/PosixPaths.cpp)
endif()

add_library(fe3d_platform
    platform/generic/StdFileSystem.cpp
    platform/generic/Clock.cpp
    platform/generic/NullWindow.cpp
    ${FE3D_PLATFORM_PATHS_SOURCE})
target_link_libraries(fe3d_platform PUBLIC fe3d::core)
fe3d_apply_warnings(fe3d_platform)
add_library(fe3d::platform ALIAS fe3d_platform)

if(FE3D_BUILD_SOFTWARE_RENDERER)
    set(FE3D_SOFTWARE_RENDERER_SOURCE rendering/SoftwareRenderer.cpp)
else()
    set(FE3D_SOFTWARE_RENDERER_SOURCE "")
endif()

add_library(fe3d_rendering
    rendering/Renderer.cpp
    rendering/MeshLibrary.cpp
    rendering/RenderViewBuilder.cpp
    rendering/ImageIO.cpp
    ${FE3D_SOFTWARE_RENDERER_SOURCE})
target_link_libraries(fe3d_rendering PUBLIC fe3d::scene fe3d::math)
fe3d_apply_warnings(fe3d_rendering)
add_library(fe3d::rendering ALIAS fe3d_rendering)

if(FE3D_ENABLE_OPENGL_RENDERER)
    fe3d_resolve_glfw()
    add_library(fe3d_rendering_opengl
        rendering/gl/OpenGLRenderer.cpp
        platform/glfw/GlfwWindow.cpp)
    target_link_libraries(fe3d_rendering_opengl PUBLIC fe3d::rendering fe3d::platform)
    if(TARGET glfw)
        target_link_libraries(fe3d_rendering_opengl PUBLIC glfw)
    elseif(TARGET glfw3)
        target_link_libraries(fe3d_rendering_opengl PUBLIC glfw3)
    else()
        message(FATAL_ERROR "GLFW target not available after fe3d_resolve_glfw()")
    endif()
    fe3d_apply_warnings(fe3d_rendering_opengl)
    add_library(fe3d::rendering_opengl ALIAS fe3d_rendering_opengl)
    message(STATUS "  OpenGL renderer     : enabled (GLFW)")
endif()

add_library(fe3d_project
    project/ProjectFile.cpp)
target_link_libraries(fe3d_project PUBLIC fe3d::core fe3d::math fe3d::scene fe3d::platform)
fe3d_apply_warnings(fe3d_project)
add_library(fe3d::project ALIAS fe3d_project)

add_library(fe3d_engine
    engine/Engine.cpp
    engine/Application.cpp
    engine/DemoScene.cpp)
target_link_libraries(fe3d_engine PUBLIC fe3d::core fe3d::math fe3d::scene fe3d::platform fe3d::rendering fe3d::project)
fe3d_apply_warnings(fe3d_engine)
add_library(fe3d::engine ALIAS fe3d_engine)
