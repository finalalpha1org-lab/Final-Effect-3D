# Third-party dependency resolution for Final Effect 3D.
#
# Policy (see docs/THIRD_PARTY.md):
#   * The portable engine core depends on nothing but the C++ standard library.
#   * Optional dependencies are only located, never silently downloaded.
#   * FE3D_FETCH_GLFW=ON is the single, explicit, pinned download path and is
#     only consulted when no system installation is found.

set(FE3D_GLFW_MIN_VERSION "3.3" CACHE STRING "Minimum acceptable GLFW version")

function(fe3d_resolve_glfw)
    if(TARGET glfw OR TARGET glfw3)
        message(STATUS "GLFW: provided by an enclosing project")
        return()
    endif()

    find_package(glfw3 ${FE3D_GLFW_MIN_VERSION} QUIET)
    if(TARGET glfw)
        message(STATUS "GLFW: system installation found")
        return()
    endif()

    if(NOT FE3D_FETCH_GLFW)
        message(FATAL_ERROR
            "GLFW ${FE3D_GLFW_MIN_VERSION}+ was not found.\n"
            "Install it through your package manager (for example\n"
            "  Debian/Ubuntu : sudo apt-get install libglfw3-dev libgl1-mesa-dev\n"
            "  macOS         : brew install glfw\n"
            "  vcpkg         : vcpkg install glfw3\n"
            "or configure with -DFE3D_FETCH_GLFW=ON to download the pinned release\n"
            "documented in docs/THIRD_PARTY.md (GLFW 3.4, zlib licence).")
    endif()

    include(FetchContent)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(glfw)
    message(STATUS "GLFW: fetched pinned release 3.4 (zlib licence)")
endfunction()

find_package(Threads REQUIRED)
