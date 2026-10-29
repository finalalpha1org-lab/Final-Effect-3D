# Final Effect 3D - applications.
# The CLI is dependency free. The desktop host requires a window backend and,
# for the windowed path, the OpenGL renderer.

if(FE3D_BUILD_CLI)
    add_executable(fe3d_tool cli/main.cpp)
    target_link_libraries(fe3d_tool PRIVATE fe3d::engine)
    fe3d_apply_warnings(fe3d_tool)
    set_target_properties(fe3d_tool PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        FOLDER "apps")
endif()

if(FE3D_BUILD_DESKTOP_APP)
    add_executable(fe3d_desktop desktop/main.cpp)
    target_link_libraries(fe3d_desktop PRIVATE fe3d::engine fe3d::rendering_opengl)
    fe3d_apply_warnings(fe3d_desktop)
    set_target_properties(fe3d_desktop PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        FOLDER "apps")
endif()
