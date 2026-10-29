# Final Effect 3D - unit and integration tests.
# A single dependency-free test executable is registered with CTest once per suite,
# so a failure names the subsystem that broke.

add_executable(fe3d_tests
    test_main.cpp
    TestRunner.cpp
    core_tests.cpp
    math_tests.cpp
    scene_tests.cpp
    project_tests.cpp
    rendering_tests.cpp
    engine_tests.cpp)
target_link_libraries(fe3d_tests PRIVATE fe3d::engine)
fe3d_apply_warnings(fe3d_tests)
set_target_properties(fe3d_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    FOLDER "tests")

foreach(suite IN ITEMS core math scene project rendering engine)
    add_test(NAME fe3d.${suite} COMMAND fe3d_tests --suite ${suite})
    set_tests_properties(fe3d.${suite} PROPERTIES TIMEOUT 180)
endforeach()

add_test(NAME fe3d.all COMMAND fe3d_tests)
set_tests_properties(fe3d.all PROPERTIES TIMEOUT 300)

# CTest runs the tools with their output files inside the build tree.
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/testing")

if(FE3D_BUILD_CLI)
    add_test(NAME fe3d.cli.info COMMAND fe3d_tool info)
    add_test(NAME fe3d.cli.render COMMAND fe3d_tool render --width 160 --height 120 --out
             "${CMAKE_BINARY_DIR}/testing/fe3d-cli-frame.ppm")
    set_tests_properties(fe3d.cli.info fe3d.cli.render PROPERTIES TIMEOUT 180)
endif()

if(FE3D_BUILD_DESKTOP_APP)
    add_test(NAME fe3d.desktop.headless COMMAND fe3d_desktop --headless --frames 2 --width 160 --height 120
             --screenshot "${CMAKE_BINARY_DIR}/testing/fe3d-desktop-frame.ppm")
    set_tests_properties(fe3d.desktop.headless PROPERTIES TIMEOUT 180)
endif()
