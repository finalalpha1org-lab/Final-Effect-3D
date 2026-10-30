// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "test_harness.hpp"

#include "finaleffect/math/aabb.hpp"
#include "finaleffect/math/constants.hpp"
#include "finaleffect/math/mat4.hpp"
#include "finaleffect/math/quat.hpp"
#include "finaleffect/math/ray.hpp"
#include "finaleffect/math/transform.hpp"
#include "finaleffect/math/vec2.hpp"
#include "finaleffect/math/vec3.hpp"
#include "finaleffect/math/vec4.hpp"

#include <limits>

namespace {

using namespace finaleffect::math;

FE_TEST(Math, Vec2OperationsAndZeroNormalizeSafety) {
    const Vec2 a{3.0f, 4.0f};
    const Vec2 b{-1.0f, 2.0f};

    FE_EXPECT_NEAR(a.length(), 5.0f, 1e-6f);
    FE_EXPECT_NEAR(a.dot(b), 5.0f, 1e-6f);
    FE_EXPECT_TRUE((a + b).approxEqual(Vec2{2.0f, 6.0f}));
    FE_EXPECT_TRUE((a - b).approxEqual(Vec2{4.0f, 2.0f}));

    const Vec2 unit_a = a.normalized();
    FE_EXPECT_NEAR(unit_a.length(), 1.0f, 1e-6f);
    FE_EXPECT_TRUE(unit_a.approxEqual(Vec2{0.6f, 0.8f}));

    // Zero-length vector normalization must not produce NaN/Inf.
    const Vec2 zero_norm = Vec2::zero().normalized();
    FE_EXPECT_TRUE(zero_norm.isFinite());
    FE_EXPECT_TRUE(zero_norm.approxEqual(Vec2::zero()));

    FE_EXPECT_TRUE(Vec2::lerp(Vec2{0.0f, 10.0f}, Vec2{10.0f, 20.0f}, 0.25f).approxEqual(Vec2{2.5f, 12.5f}));
}

FE_TEST(Math, Vec3RightHandedBasisAndEdgeCases) {
    // Right-handed coordinate convention: +X x +Y = +Z, +Y x +Z = +X, +Z x +X = +Y.
    FE_EXPECT_TRUE(Vec3::unitX().cross(Vec3::unitY()).approxEqual(Vec3::unitZ()));
    FE_EXPECT_TRUE(Vec3::unitY().cross(Vec3::unitZ()).approxEqual(Vec3::unitX()));
    FE_EXPECT_TRUE(Vec3::unitZ().cross(Vec3::unitX()).approxEqual(Vec3::unitY()));
    FE_EXPECT_TRUE(Vec3::unitY().cross(Vec3::unitX()).approxEqual(-Vec3::unitZ()));

    const Vec3 v{1.0f, -2.0f, 2.0f};
    FE_EXPECT_NEAR(v.length(), 3.0f, 1e-6f);
    FE_EXPECT_NEAR(v.normalized().length(), 1.0f, 1e-6f);

    const Vec3 tiny{1e-12f, -1e-12f, 0.0f};
    const Vec3 tiny_norm = tiny.normalized();
    FE_EXPECT_TRUE(tiny_norm.isFinite());
    FE_EXPECT_TRUE(tiny_norm.approxEqual(Vec3::zero()));

    const Vec3 non_finite{std::numeric_limits<float>::infinity(), 0.0f, 0.0f};
    FE_EXPECT_FALSE(non_finite.isFinite());
}

FE_TEST(Math, QuaternionRotationsCompositionAndSlerp) {
    // +90 deg around +Z rotates +X -> +Y.
    const Quat qz90 = Quat::fromAxisAngle(Vec3::unitZ(), kHalfPi);
    FE_EXPECT_TRUE(qz90.rotate(Vec3::unitX()).approxEqual(Vec3::unitY(), 1e-5f));

    // +90 deg around +Y rotates +Z -> +X and -Z -> -X.
    const Quat qy90 = Quat::fromAxisAngle(Vec3::unitY(), kHalfPi);
    FE_EXPECT_TRUE(qy90.rotate(Vec3::unitZ()).approxEqual(Vec3::unitX(), 1e-5f));

    // Composition consistency: (qA * qB).rotate(v) == qA.rotate(qB.rotate(v)).
    const Quat q_a = Quat::fromAxisAngle(Vec3{1.0f, 2.0f, 3.0f}, degToRad(37.0f));
    const Quat q_b = Quat::fromAxisAngle(Vec3{-2.0f, 0.5f, 1.0f}, degToRad(63.0f));
    const Vec3 sample{3.5f, -1.25f, 0.75f};
    FE_EXPECT_TRUE((q_a * q_b).rotate(sample).approxEqual(q_a.rotate(q_b.rotate(sample)), 1e-5f));

    // Inverse restores original vector.
    const Vec3 restored = q_a.inverse().rotate(q_a.rotate(sample));
    FE_EXPECT_TRUE(restored.approxEqual(sample, 1e-5f));

    // Degenerate zero-axis returns identity safely.
    const Quat degenerate = Quat::fromAxisAngle(Vec3::zero(), 1.23f);
    FE_EXPECT_TRUE(degenerate.approxEqual(Quat::identity()));

    // Slerp midpoint between identity and +90 deg around Y is +45 deg around Y.
    const Quat mid = Quat::slerp(Quat::identity(), qy90, 0.5f);
    const Quat qy45 = Quat::fromAxisAngle(Vec3::unitY(), degToRad(45.0f));
    FE_EXPECT_TRUE(mid.sameOrientation(qy45, 1e-5f));

    // Antipodal equivalence: slerp between q and -q stays stationary without NaN.
    const Quat neg_qa{-q_a.x, -q_a.y, -q_a.z, -q_a.w};
    FE_EXPECT_TRUE(q_a.sameOrientation(neg_qa));
    const Quat stationary = Quat::slerp(q_a, neg_qa, 0.5f);
    FE_EXPECT_TRUE(stationary.isFinite());
    FE_EXPECT_TRUE(stationary.sameOrientation(q_a, 1e-5f));
}

FE_TEST(Math, Mat4TrsInversionAndSingularHandling) {
    const Vec3 t{4.0f, -3.0f, 12.5f};
    const Quat r = Quat::fromAxisAngle(Vec3{0.3f, 0.8f, -0.5f}, degToRad(52.0f));
    const Vec3 s{2.0f, 0.5f, 3.0f};

    const Mat4 m_trs = Mat4::trs(t, r, s);
    const Mat4 m_composed = Mat4::translation(t) * Mat4::fromQuat(r) * Mat4::scale(s);
    FE_EXPECT_TRUE(m_trs.approxEqual(m_composed, 1e-5f));

    // Matrix rotation matches quaternion rotation.
    const Vec3 p{1.5f, -2.0f, 0.25f};
    FE_EXPECT_TRUE(Mat4::fromQuat(r).transformPoint(p).approxEqual(r.rotate(p), 1e-5f));

    // Inversion recovers identity.
    const auto inv_opt = m_trs.inverted();
    FE_REQUIRE_TRUE(inv_opt.has_value());
    const Mat4 prod = m_trs * inv_opt.value();
    FE_EXPECT_TRUE(prod.approxEqual(Mat4::identity(), 1e-4f));

    // Singular matrix (zero scale axis) returns std::nullopt instead of NaN.
    const Mat4 singular = Mat4::scale(Vec3{1.0f, 0.0f, 1.0f});
    FE_EXPECT_NEAR(singular.determinant(), 0.0f, 1e-6f);
    FE_EXPECT_FALSE(singular.inverted().has_value());
}

FE_TEST(Math, LookAtAndPerspectiveDepthConventions) {
    const Mat4 view = Mat4::lookAtRH(Vec3{0.0f, 0.0f, 5.0f}, Vec3::zero(), Vec3::unitY());
    const Vec3 origin_in_view = view.transformPoint(Vec3::zero());
    FE_EXPECT_TRUE(origin_in_view.approxEqual(Vec3{0.0f, 0.0f, -5.0f}, 1e-5f));

    // Collinear up vector fallback must remain finite and orthonormal.
    const Mat4 top_down = Mat4::lookAtRH(Vec3{0.0f, 10.0f, 0.0f}, Vec3::zero(), Vec3::unitY());
    FE_EXPECT_TRUE(top_down.isFinite());
    FE_EXPECT_TRUE(top_down.inverted().has_value());

    const float z_near = 0.5f;
    const float z_far = 100.0f;

    // OpenGL / WebGL2 [-1, 1] NDC depth convention.
    const Mat4 proj_no = Mat4::perspectiveRH_NO(degToRad(60.0f), 16.0f / 9.0f, z_near, z_far);
    FE_EXPECT_NEAR(proj_no.transformPoint(Vec3{0.0f, 0.0f, -z_near}).z, -1.0f, 1e-5f);
    FE_EXPECT_NEAR(proj_no.transformPoint(Vec3{0.0f, 0.0f, -z_far}).z, 1.0f, 1e-5f);

    // Vulkan / Metal / WebGPU [0, 1] NDC depth convention.
    const Mat4 proj_zo = Mat4::perspectiveRH_ZO(degToRad(60.0f), 16.0f / 9.0f, z_near, z_far);
    FE_EXPECT_NEAR(proj_zo.transformPoint(Vec3{0.0f, 0.0f, -z_near}).z, 0.0f, 1e-5f);
    FE_EXPECT_NEAR(proj_zo.transformPoint(Vec3{0.0f, 0.0f, -z_far}).z, 1.0f, 1e-5f);
}

FE_TEST(Math, TransformHierarchyEquivalence) {
    Transform parent;
    parent.position = Vec3{10.0f, 2.0f, -4.0f};
    parent.rotation = Quat::fromAxisAngle(Vec3::unitY(), degToRad(90.0f));
    parent.scale = Vec3{2.0f, 2.0f, 2.0f}; // Uniform scale

    Transform child;
    child.position = Vec3{1.0f, 0.0f, 0.0f};
    child.rotation = Quat::fromAxisAngle(Vec3::unitX(), degToRad(45.0f));
    child.scale = Vec3{0.5f, 1.5f, 1.0f};

    const Transform combined = Transform::combineRigidOrUniform(parent, child);
    const Mat4 expected_matrix = parent.toMatrix() * child.toMatrix();

    const Vec3 local_pt{-1.0f, 2.0f, 3.0f};
    FE_EXPECT_TRUE(
        combined.transformPoint(local_pt).approxEqual(expected_matrix.transformPoint(local_pt), 1e-4f));
    FE_EXPECT_TRUE(parent.forward().approxEqual(Vec3{-1.0f, 0.0f, 0.0f}, 1e-5f));
}

FE_TEST(Math, RayAabbAndRayTriangleIntersections) {
    const AABB box = AABB::fromMinMax(Vec3{-1.0f, -2.0f, -3.0f}, Vec3{1.0f, 2.0f, 3.0f});
    FE_EXPECT_TRUE(box.isValid());

    const Ray hit_ray(Vec3{0.0f, 0.0f, 10.0f}, Vec3{0.0f, 0.0f, -1.0f});
    const auto hit_t = intersectRayAabb(hit_ray, box);
    FE_REQUIRE_TRUE(hit_t.has_value());
    FE_EXPECT_NEAR(hit_t.value(), 7.0f, 1e-5f);

    // Axis-parallel miss must return nullopt without division by zero.
    const Ray parallel_miss(Vec3{5.0f, 0.0f, 10.0f}, Vec3{0.0f, 0.0f, -1.0f});
    FE_EXPECT_FALSE(intersectRayAabb(parallel_miss, box).has_value());

    // Moller-Trumbore triangle intersection.
    const Vec3 v0{-1.0f, -1.0f, -4.0f};
    const Vec3 v1{1.0f, -1.0f, -4.0f};
    const Vec3 v2{0.0f, 1.0f, -4.0f};
    const Ray tri_ray(Vec3{0.0f, -0.25f, 0.0f}, Vec3{0.0f, 0.0f, -1.0f});
    const auto tri_t = intersectRayTriangle(tri_ray, v0, v1, v2, true);
    FE_REQUIRE_TRUE(tri_t.has_value());
    FE_EXPECT_NEAR(tri_t.value(), 4.0f, 1e-5f);

    // Backface-culled ray from behind must miss when backface_cull = true.
    const Ray behind_ray(Vec3{0.0f, -0.25f, -8.0f}, Vec3{0.0f, 0.0f, 1.0f});
    FE_EXPECT_FALSE(intersectRayTriangle(behind_ray, v0, v1, v2, true).has_value());
    FE_EXPECT_TRUE(intersectRayTriangle(behind_ray, v0, v1, v2, false).has_value());
}

FE_TEST(Math, QuaternionEulerXYZRoundTripAndSingularities) {
    const Vec3 angles_rad{degToRad(-24.0f), degToRad(37.5f), degToRad(112.0f)};
    const Quat q = Quat::fromEulerXYZ(angles_rad);
    const Vec3 extracted = q.toEulerXYZ();
    const Quat q_rebuilt = Quat::fromEulerXYZ(extracted);

    const Vec3 probe{1.2f, -0.7f, 2.5f};
    FE_EXPECT_TRUE(q.rotate(probe).approxEqual(q_rebuilt.rotate(probe), 1e-4f));

    // Gimbal lock at +90 deg Y pitch must reconstruct an equivalent rotation without NaN.
    const Vec3 gimbal_pos{degToRad(33.0f), degToRad(90.0f), degToRad(-18.0f)};
    const Quat q_gimbal = Quat::fromEulerXYZ(gimbal_pos);
    const Vec3 gimbal_extracted = q_gimbal.toEulerXYZ();
    FE_EXPECT_TRUE(gimbal_extracted.isFinite());
    const Quat q_gimbal_rebuilt = Quat::fromEulerXYZ(gimbal_extracted);
    FE_EXPECT_TRUE(q_gimbal.rotate(probe).approxEqual(q_gimbal_rebuilt.rotate(probe), 1e-4f));
}

} // namespace

int main(int argc, char** argv) {
    return finaleffect::testing::runAllTests(argc, argv);
}
