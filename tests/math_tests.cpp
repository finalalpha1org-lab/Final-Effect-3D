// SPDX-License-Identifier: GPL-3.0-only
#include <cmath>

#include "TestFramework.hpp"
#include "finaleffect/math/Mat4.hpp"
#include "finaleffect/math/Quat.hpp"
#include "finaleffect/math/Transform.hpp"
#include "finaleffect/math/Vec2.hpp"
#include "finaleffect/math/Vec3.hpp"

namespace {
using namespace fe::math;
constexpr Scalar kTol = 1.0e-4F;
}  // namespace

FE3D_TEST(math, scalar_helpers) {
    FE3D_CHECK_NEAR(radians(180.0F), kPi, 1.0e-5F);
    FE3D_CHECK_NEAR(degrees(kHalfPi), 90.0F, 1.0e-4F);
    FE3D_CHECK_NEAR(clamp(5.0F, 0.0F, 1.0F), 1.0F, 0.0F);
    FE3D_CHECK(approxEqual(1.0F, 1.0F + 1.0e-7F));
    FE3D_CHECK(!approxEqual(1.0F, 1.01F));
    FE3D_CHECK(approxZero(1.0e-9F));
}

FE3D_TEST(math, vec2_and_vec3_operations) {
    const Vec2 a(3.0F, 4.0F);
    FE3D_CHECK_NEAR(a.length(), 5.0F, 1.0e-5F);
    FE3D_CHECK_NEAR(a.normalized().length(), 1.0F, 1.0e-5F);
    FE3D_CHECK_NEAR(dot(Vec2(1.0F, 0.0F), Vec2(0.0F, 1.0F)), 0.0F, 0.0F);
    FE3D_CHECK_NEAR(cross(Vec2(1.0F, 0.0F), Vec2(0.0F, 1.0F)), 1.0F, 0.0F);
    FE3D_CHECK(Vec2::zero().normalized() == Vec2::zero());

    const Vec3 x = Vec3::unitX();
    const Vec3 y = Vec3::unitY();
    FE3D_CHECK(cross(x, y).isApprox(Vec3::unitZ(), kTol));
    FE3D_CHECK_NEAR(dot(x, y), 0.0F, kTol);
    FE3D_CHECK_NEAR(Vec3(1.0F, 2.0F, 2.0F).length(), 3.0F, 1.0e-5F);
    FE3D_CHECK(Vec3::zero().normalized() == Vec3::zero());
    FE3D_CHECK(lerp(Vec3::zero(), Vec3::one(), 0.5F).isApprox(Vec3(0.5F, 0.5F, 0.5F), kTol));
    FE3D_CHECK(reflect(Vec3(1.0F, -1.0F, 0.0F), Vec3::unitY()).isApprox(Vec3(1.0F, 1.0F, 0.0F), kTol));
    FE3D_CHECK(min(Vec3(1.0F, 5.0F, 3.0F), Vec3(2.0F, 4.0F, 3.0F)).isApprox(Vec3(1.0F, 4.0F, 3.0F), kTol));
    FE3D_CHECK(Vec3::zero().isFinite());
    FE3D_CHECK(!Vec3(0.0F, std::nanf(""), 0.0F).isFinite());
}

FE3D_TEST(math, mat4_identity_multiply_and_inverse) {
    const Mat4 identity = Mat4::identity();
    FE3D_CHECK(identity.isApprox(Mat4(), 0.0F));

    const Mat4 translation = Mat4::translation(Vec3(1.0F, 2.0F, 3.0F));
    const Mat4 scaling = Mat4::scaling(Vec3(2.0F, 2.0F, 2.0F));
    const Mat4 combined = translation * scaling;

    const Vec3 point(1.0F, 0.0F, 0.0F);
    FE3D_CHECK(combined.transformPoint(point).isApprox(Vec3(3.0F, 2.0F, 3.0F), kTol));
    FE3D_CHECK(combined.transformVector(point).isApprox(Vec3(2.0F, 0.0F, 0.0F), kTol));
    FE3D_CHECK((translation * identity).isApprox(translation, 0.0F));

    // (A * B) * v == A * (B * v)
    const Mat4 rotation = Mat4::rotationAxisAngle(Vec3::unitY(), 0.7F);
    const Vec3 v(0.3F, -0.4F, 0.5F);
    const Vec3 left = (translation * rotation).transformPoint(v);
    const Vec3 right = translation.transformPoint(rotation.transformPoint(v));
    FE3D_CHECK(left.isApprox(right, kTol));

    Mat4 inverse;
    FE3D_REQUIRE(combined.tryInverse(inverse));
    FE3D_CHECK((combined * inverse).isApprox(Mat4::identity(), 1.0e-4F));
    FE3D_CHECK((inverse * combined).isApprox(Mat4::identity(), 1.0e-4F));

    Mat4 singularInverse;
    FE3D_CHECK(!Mat4::zero().tryInverse(singularInverse));
    FE3D_CHECK(Mat4::zero().transposed().isApprox(Mat4::zero(), 0.0F));
    FE3D_CHECK(Mat4::identity().isFinite());
}

FE3D_TEST(math, mat4_projection_and_view) {
    const Mat4 perspective = Mat4::perspective(radians(60.0F), 1.5F, 0.1F, 100.0F);
    const Vec3 inFront(0.0F, 0.0F, -1.0F);
    const Vec3 projected = perspective.transformPoint(inFront);
    FE3D_CHECK(projected.isFinite());

    // Degenerate arguments must not produce NaN matrices.
    FE3D_CHECK(Mat4::perspective(0.0F, 1.0F, 0.1F, 1.0F).isApprox(Mat4::identity(), 0.0F));
    FE3D_CHECK(Mat4::perspective(1.0F, 0.0F, 0.1F, 1.0F).isApprox(Mat4::identity(), 0.0F));
    FE3D_CHECK(Mat4::perspective(1.0F, 1.0F, 0.0F, 1.0F).isApprox(Mat4::identity(), 0.0F));

    const Mat4 view = Mat4::lookAt(Vec3(0.0F, 0.0F, 5.0F), Vec3::zero(), Vec3::unitY());
    const Vec3 cameraSpaceOrigin = view.transformPoint(Vec3::zero());
    FE3D_CHECK(cameraSpaceOrigin.isApprox(Vec3(0.0F, 0.0F, -5.0F), kTol));
    FE3D_CHECK(Mat4::lookAt(Vec3::zero(), Vec3::zero(), Vec3::unitY())
                   .isApprox(Mat4::identity(), 0.0F));

    const Mat4 orthographic = Mat4::orthographic(-1.0F, 1.0F, -1.0F, 1.0F, 0.1F, 10.0F);
    FE3D_CHECK(orthographic.isFinite());
    FE3D_CHECK(orthographic.transformPoint(Vec3::zero()).isApprox(Vec3(0.0F, 0.0F, -1.0202F), 1.0e-3F));
    FE3D_CHECK(Mat4::orthographic(0.0F, 0.0F, -1.0F, 1.0F, 0.1F, 10.0F).isApprox(Mat4::identity(), 0.0F));
    // The OpenGL-style depth mapping sends z=0 of the view volume to -(f+n)/(f-n).
    FE3D_CHECK_NEAR(orthographic.transformPoint(Vec3::zero()).z, -1.0202F, 1.0e-3F);
}

FE3D_TEST(math, quaternion_rotation_matches_matrix) {
    const Vec3 axis = Vec3(1.0F, 2.0F, -0.5F).normalized();
    const Quat quat = Quat::fromAxisAngle(axis, 1.1F);
    FE3D_CHECK_NEAR(quat.length(), 1.0F, 1.0e-5F);

    const Vec3 value(0.7F, -0.2F, 0.4F);
    const Vec3 rotatedByQuat = quat.rotate(value);
    const Vec3 rotatedByMatrix = Mat4::rotation(quat).transformVector(value);
    FE3D_CHECK(rotatedByQuat.isApprox(rotatedByMatrix, kTol));
    FE3D_CHECK_NEAR(rotatedByQuat.length(), value.length(), 1.0e-4F);
    FE3D_CHECK((quat * quat.inverse()).isApprox(Quat::identity(), kTol));
    FE3D_CHECK(quat.rotate(axis * 3.0F).isApprox(axis * 3.0F, 1.0e-4F));

    // Composition order matches matrix multiplication order.
    const Quat a = Quat::fromAxisAngle(Vec3::unitY(), 0.5F);
    const Quat b = Quat::fromAxisAngle(Vec3::unitX(), -0.9F);
    const Vec3 composed = (a * b).rotate(value);
    const Vec3 stepwise = a.rotate(b.rotate(value));
    FE3D_CHECK(composed.isApprox(stepwise, kTol));

    // q and -q are the same rotation.
    FE3D_CHECK(quat.isApprox(Quat(-quat.x, -quat.y, -quat.z, -quat.w), kTol));
}

FE3D_TEST(math, quaternion_edge_cases) {
    const Quat degenerate = Quat(0.0F, 0.0F, 0.0F, 0.0F);
    FE3D_CHECK(degenerate.normalized().isApprox(Quat::identity(), kTol));
    FE3D_CHECK(degenerate.inverse().isApprox(Quat::identity(), kTol));
    FE3D_CHECK(Quat::fromAxisAngle(Vec3::zero(), 1.0F).isApprox(Quat::identity(), kTol));
    FE3D_CHECK(Quat::identity().rotate(Vec3(1.0F, 2.0F, 3.0F)).isApprox(Vec3(1.0F, 2.0F, 3.0F), kTol));

    Vec3 axis;
    Scalar angle = 0.0F;
    const Quat source = Quat::fromAxisAngle(Vec3::unitZ(), 0.75F);
    source.toAxisAngle(axis, angle);
    FE3D_CHECK_NEAR(angle, 0.75F, 1.0e-4F);
    FE3D_CHECK(axis.isApprox(Vec3::unitZ(), 1.0e-4F));

    Quat::identity().toAxisAngle(axis, angle);
    FE3D_CHECK_NEAR(angle, 0.0F, 1.0e-4F);

    const Quat start = Quat::fromAxisAngle(Vec3::unitY(), 0.0F);
    const Quat end = Quat::fromAxisAngle(Vec3::unitY(), kPi * 0.5F);
    const Quat half = Quat::slerp(start, end, 0.5F);
    FE3D_CHECK(half.rotate(Vec3::unitX()).isApprox(
        Quat::fromAxisAngle(Vec3::unitY(), kPi * 0.25F).rotate(Vec3::unitX()), 1.0e-3F));
    FE3D_CHECK(Quat::slerp(start, start, 0.5F).isApprox(start, kTol));
    FE3D_CHECK(Quat::slerp(start, end, 0.0F).isApprox(start, kTol));
    FE3D_CHECK(Quat::slerp(start, end, 1.0F).isApprox(end, 1.0e-3F));
}

FE3D_TEST(math, transform_composition_and_inverse) {
    Transform transform;
    transform.position = Vec3(1.0F, 2.0F, 3.0F);
    transform.rotation = Quat::fromEulerAngles(0.4F, -0.25F, 0.1F);
    transform.scale = Vec3(2.0F, 0.5F, 1.5F);

    const Mat4 matrix = transform.matrix();
    const Mat4 expected = Mat4::translation(transform.position) * Mat4::rotation(transform.rotation) *
                          Mat4::scaling(transform.scale);
    FE3D_CHECK(matrix.isApprox(expected, kTol));

    const Vec3 point(0.25F, -0.5F, 0.75F);
    FE3D_CHECK(transform.transformPoint(point).isApprox(matrix.transformPoint(point), kTol));
    FE3D_CHECK(transform.transformVector(point).isApprox(matrix.transformVector(point), kTol));

    Mat4 inverseMatrix;
    FE3D_REQUIRE(matrix.tryInverse(inverseMatrix));

    // A uniform scale inverts exactly: (T R S)^-1 factorises as R^-1 S^-1 T^-1.
    Transform uniform = transform;
    uniform.scale = Vec3(2.0F, 2.0F, 2.0F);
    const Transform uniformInverse = uniform.inverse();
    FE3D_CHECK(uniformInverse.transformPoint(uniform.transformPoint(point)).isApprox(point, 1.0e-4F));
    Mat4 uniformMatrix;
    FE3D_REQUIRE(uniform.matrix().tryInverse(uniformMatrix));
    FE3D_CHECK(uniformMatrix.isApprox(uniformInverse.matrix(), 1.0e-4F));

    // So does a non-uniform scale without rotation.
    Transform scaledOnly;
    scaledOnly.position = Vec3(-1.0F, 4.0F, 0.5F);
    scaledOnly.scale = Vec3(2.0F, 0.5F, 4.0F);
    Mat4 scaledOnlyMatrix;
    FE3D_REQUIRE(scaledOnly.matrix().tryInverse(scaledOnlyMatrix));
    FE3D_CHECK(scaledOnlyMatrix.isApprox(scaledOnly.inverse().matrix(), 1.0e-4F));

    // With rotation *and* non-uniform scale the linear part of the inverse is not
    // a TRS transform, so only the translation is exact (see Transform::inverse).
    const Transform inverse = transform.inverse();
    FE3D_CHECK(inverse.position.isApprox(inverseMatrix.translation(), 1.0e-4F));
}

FE3D_TEST(math, transform_decompose_round_trip) {
    Transform original;
    original.position = Vec3(-2.0F, 0.5F, 4.0F);
    original.rotation = Quat::fromEulerAngles(-0.8F, 0.3F, 1.2F);
    original.scale = Vec3(1.5F, 2.0F, 0.75F);

    const Transform decomposed = Transform::fromMatrix(original.matrix());
    FE3D_CHECK(decomposed.isApprox(original, 1.0e-4F));

    Transform reflected;
    reflected.scale = Vec3(1.0F, 1.0F, -1.0F);
    reflected.rotation = Quat::fromAxisAngle(Vec3::unitY(), 0.35F);
    const Transform reflectedBack = Transform::fromMatrix(reflected.matrix());
    FE3D_CHECK(reflectedBack.matrix().isApprox(reflected.matrix(), 1.0e-4F));

    Transform degenerate;
    degenerate.scale = Vec3::zero();
    FE3D_CHECK(Transform::fromMatrix(degenerate.matrix()).rotation.isApprox(Quat::identity(), kTol));
}
