// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors
//
// Exercises the vendored Blender blenlib math subset in third_party/blender.

#include "test_harness.hpp"

#include "BLI_math_euler.hh"
#include "BLI_math_matrix.hh"
#include "BLI_math_quaternion.hh"
#include "BLI_math_rotation.hh"
#include "BLI_math_vector.hh"

#include <numbers>

namespace {

using blender::float3;
using blender::float4x4;

FE_TEST(BlenderBlenlib, VectorOperations) {
    const float3 a{1.0f, 2.0f, 3.0f};
    const float3 b{4.0f, -5.0f, 6.0f};

    FE_EXPECT_NEAR(blender::math::dot(a, b), 12.0f, 1e-6f);

    const float3 c = blender::math::cross(a, b);
    FE_EXPECT_NEAR(c.x, 27.0f, 1e-6f);
    FE_EXPECT_NEAR(c.y, 6.0f, 1e-6f);
    FE_EXPECT_NEAR(c.z, -13.0f, 1e-6f);

    FE_EXPECT_NEAR(blender::math::length(blender::math::normalize(a)), 1.0f, 1e-6f);
}

FE_TEST(BlenderBlenlib, EulerQuaternionMatrixRotation) {
    const float quarter_turn = std::numbers::pi_v<float> / 2.0f;
    const blender::math::EulerXYZ euler{0.0f, 0.0f, quarter_turn};
    const blender::math::Quaternion quat = blender::math::to_quaternion(euler);
    const float4x4 rot = blender::math::from_rotation<float4x4>(quat);

    const float3 rotated = blender::math::transform_point(rot, float3{1.0f, 0.0f, 0.0f});
    FE_EXPECT_NEAR(rotated.x, 0.0f, 1e-5f);
    FE_EXPECT_NEAR(rotated.y, 1.0f, 1e-5f);
    FE_EXPECT_NEAR(rotated.z, 0.0f, 1e-5f);

    const blender::math::EulerXYZ back = blender::math::to_euler(quat);
    FE_EXPECT_NEAR(back.z, quarter_turn, 1e-5f);
}

FE_TEST(BlenderBlenlib, InverseDeterminantAndRoundTrip) {
    const blender::math::Quaternion quat = blender::math::to_quaternion(
        blender::math::EulerXYZ{0.3f, -0.7f, 1.1f});
    const float4x4 rot = blender::math::from_rotation<float4x4>(quat);

    bool inverted = false;
    const float4x4 inv = blender::math::invert(rot, inverted);
    FE_EXPECT_TRUE(inverted);

    const float3 point{0.4f, -1.2f, 2.5f};
    const float3 round_trip = blender::math::transform_point(
        inv, blender::math::transform_point(rot, point));
    FE_EXPECT_NEAR(round_trip.x, point.x, 1e-4f);
    FE_EXPECT_NEAR(round_trip.y, point.y, 1e-4f);
    FE_EXPECT_NEAR(round_trip.z, point.z, 1e-4f);

    // A pure rotation has determinant one and a non-negative basis.
    FE_EXPECT_NEAR(blender::math::determinant(rot), 1.0f, 1e-5f);
    FE_EXPECT_TRUE(!blender::math::is_negative(rot));
}

FE_TEST(BlenderBlenlib, CApiRotationAndTriangleNormal) {
    const float quarter_turn = std::numbers::pi_v<float> / 2.0f;
    const float axis[3] = {0.0f, 0.0f, 1.0f};
    float matrix[3][3];
    blender::axis_angle_normalized_to_mat3(matrix, axis, quarter_turn);

    float vec[3] = {1.0f, 0.0f, 0.0f};
    blender::mul_m3_v3(matrix, vec);
    FE_EXPECT_NEAR(vec[0], 0.0f, 1e-5f);
    FE_EXPECT_NEAR(vec[1], 1.0f, 1e-5f);
    FE_EXPECT_NEAR(vec[2], 0.0f, 1e-5f);

    const float v1[3] = {0.0f, 0.0f, 0.0f};
    const float v2[3] = {1.0f, 0.0f, 0.0f};
    const float v3[3] = {0.0f, 1.0f, 0.0f};
    float normal[3];
    const float area = blender::normal_tri_v3(normal, v1, v2, v3);
    FE_EXPECT_NEAR(normal[0], 0.0f, 1e-5f);
    FE_EXPECT_NEAR(normal[1], 0.0f, 1e-5f);
    FE_EXPECT_NEAR(normal[2], 1.0f, 1e-5f);
    FE_EXPECT_NEAR(area, 1.0f, 1e-5f);
}

}  // namespace
