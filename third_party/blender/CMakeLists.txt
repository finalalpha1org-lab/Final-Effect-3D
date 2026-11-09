# SPDX-License-Identifier: GPL-3.0-or-later
# Vendored subset of Blender's blenlib math library (upstream license: GPL-2.0-or-later).
# Provenance, patched files and build notes: see README.md in this directory.

add_library(finaleffect_blenlib STATIC
    blenlib/intern/math_base.cc
    blenlib/intern/math_basis_types.cc
    blenlib/intern/math_half.cc
    blenlib/intern/math_interp.cc
    blenlib/intern/math_matrix.cc
    blenlib/intern/math_matrix_c.cc
    blenlib/intern/math_rotation.cc
    blenlib/intern/math_rotation_c.cc
    blenlib/intern/math_vector.cc
    compat/blenlib_assert.cc
    compat/blenlib_geom_support.cc
    compat/blenlib_matrix_support.cc
)
add_library(finaleffect::blenlib ALIAS finaleffect_blenlib)

# Blender 5.x blenlib requires C++20 (std::numbers, std::bit_cast, concepts/requires).
# PUBLIC so that consumers including the blenlib headers also compile as C++20.
target_compile_features(finaleffect_blenlib PUBLIC cxx_std_20)

set_target_properties(finaleffect_blenlib PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Blender's own configuration switch: builds the blenlib math library without Eigen,
# OpenMP or the Blender task scheduler.
target_compile_definitions(finaleffect_blenlib PRIVATE MATH_STANDALONE)

target_include_directories(finaleffect_blenlib SYSTEM PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/blenlib
    ${CMAKE_CURRENT_SOURCE_DIR}/compat
)

# Vendored third-party sources keep their own warning policy. The project-wide strict set
# (-Wconversion, -Wsign-conversion, -Wshadow, -Wpedantic, -Wold-style-cast, ...) is aimed at
# first-party code; Blender's sources are warning-clean with -Wall -Wextra, which is what
# upstream itself uses.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang|IntelLLVM")
    target_compile_options(finaleffect_blenlib PRIVATE -Wall -Wextra)
endif()
