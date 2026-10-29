// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "test_harness.hpp"

#include "finaleffect/math/constants.hpp"
#include "finaleffect/scene/picking.hpp"
#include "finaleffect/scene/primitives.hpp"
#include "finaleffect/scene/scene.hpp"

#include <limits>

namespace {

using namespace finaleffect;
using namespace finaleffect::scene;

FE_TEST(Scene, ObjectCreationLookupAndSharedMeshes) {
    Scene scene("Studio Scene");
    FE_EXPECT_EQ(scene.name(), std::string("Studio Scene"));

    const auto cube_mesh_res = scene.registerMesh("UnitCube", Primitives::createCube(1.0f));
    FE_REQUIRE_TRUE(cube_mesh_res.hasValue());
    const MeshId cube_id = cube_mesh_res.value();

    const auto a_res = scene.createObject("CubeInstanceA");
    const auto b_res = scene.createObject("CubeInstanceB");
    FE_REQUIRE_TRUE(a_res.hasValue());
    FE_REQUIRE_TRUE(b_res.hasValue());
    FE_EXPECT_TRUE(a_res.value() != b_res.value());

    FE_REQUIRE_TRUE(scene.assignMesh(a_res.value(), cube_id).hasValue());
    FE_REQUIRE_TRUE(scene.assignMesh(b_res.value(), cube_id).hasValue());

    // Dangling mesh ID must be rejected.
    FE_EXPECT_EQ(scene.assignMesh(a_res.value(), MeshId{9999}).error().code, core::ErrorCode::NotFound);

    const auto* found = scene.findObjectByName("CubeInstanceB");
    FE_REQUIRE_TRUE(found != nullptr);
    FE_EXPECT_EQ(found->id, b_res.value());
    FE_REQUIRE_TRUE(found->mesh_id.has_value());
    FE_EXPECT_EQ(found->mesh_id.value(), cube_id);

    // Two object instances sharing one mesh asset only allocate CPU vertex storage once.
    const auto stats = scene.collectResourceStats();
    FE_EXPECT_EQ(stats.scene_object_count, static_cast<std::size_t>(2));
    FE_EXPECT_EQ(stats.mesh_asset_count, static_cast<std::size_t>(1));
    FE_EXPECT_EQ(stats.total_cpu_vertices, static_cast<std::size_t>(24));
    FE_EXPECT_EQ(stats.total_cpu_indices, static_cast<std::size_t>(36));
}

FE_TEST(Scene, HierarchyWorldMatrixAndCycleRejection) {
    Scene scene;

    const auto root_id = scene.createObject("Root").value();
    const auto arm_id = scene.createObject("Arm", root_id).value();
    const auto tip_id = scene.createObject("Tip", arm_id).value();

    math::Transform root_tf;
    root_tf.position = {10.0f, 0.0f, 0.0f};
    root_tf.rotation = math::Quat::fromAxisAngle(math::Vec3::unitY(), math::kHalfPi);
    FE_REQUIRE_TRUE(scene.setObjectTransform(root_id, root_tf).hasValue());

    math::Transform arm_tf;
    arm_tf.position = {0.0f, 2.0f, -3.0f};
    FE_REQUIRE_TRUE(scene.setObjectTransform(arm_id, arm_tf).hasValue());

    math::Transform tip_tf;
    tip_tf.position = {1.0f, 0.0f, 0.0f};
    FE_REQUIRE_TRUE(scene.setObjectTransform(tip_id, tip_tf).hasValue());

    const auto tip_world_res = scene.computeWorldMatrix(tip_id);
    FE_REQUIRE_TRUE(tip_world_res.hasValue());

    // Local (0,0,0) of Tip -> Arm space (1, 0, 0) -> Root space (1, 2, -3)
    // Rotated +90 deg around Y: (1, 2, -3) -> (-3, 2, -1), plus root translation (10, 0, 0) = (7, 2, -1).
    const math::Vec3 tip_origin = tip_world_res.value().transformPoint(math::Vec3::zero());
    FE_EXPECT_TRUE(tip_origin.approxEqual(math::Vec3{7.0f, 2.0f, -1.0f}, 1e-4f));

    // Self-parenting and ancestor cycles must be rejected without mutating hierarchy.
    FE_EXPECT_EQ(scene.setParent(root_id, root_id).error().code, core::ErrorCode::HierarchyCycle);
    FE_EXPECT_EQ(scene.setParent(root_id, tip_id).error().code, core::ErrorCode::HierarchyCycle);
    FE_EXPECT_EQ(scene.setParent(arm_id, tip_id).error().code, core::ErrorCode::HierarchyCycle);

    // Non-finite transform must be rejected.
    math::Transform bad_tf;
    bad_tf.position = {std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f};
    FE_EXPECT_EQ(scene.setObjectTransform(tip_id, bad_tf).error().code, core::ErrorCode::ValidationError);
}

FE_TEST(Scene, RemovalModesAndCameraInvariants) {
    Scene scene;
    const auto parent = scene.createObject("Parent").value();
    const auto mid = scene.createObject("Middle", parent).value();
    const auto cam_obj = scene.createObject("CameraChild", mid).value();

    // Setting active camera before CameraComponent is attached must fail.
    FE_EXPECT_EQ(scene.setActiveCamera(cam_obj).error().code, core::ErrorCode::ValidationError);

    CameraComponent cam;
    cam.fov_y_radians = math::degToRad(60.0f);
    FE_REQUIRE_TRUE(scene.assignCamera(cam_obj, cam).hasValue());
    FE_REQUIRE_TRUE(scene.setActiveCamera(cam_obj).hasValue());
    FE_EXPECT_EQ(scene.activeCamera(), cam_obj);

    // ReparentChildren removal of Middle promotes CameraChild under Parent and keeps active camera valid.
    FE_REQUIRE_TRUE(scene.removeObject(mid, RemovalMode::ReparentChildren).hasValue());
    FE_EXPECT_TRUE(scene.findObject(mid) == nullptr);
    FE_REQUIRE_TRUE(scene.findObject(cam_obj) != nullptr);
    FE_EXPECT_EQ(scene.findObject(cam_obj)->parent_id, parent);
    FE_EXPECT_EQ(scene.activeCamera(), cam_obj);

    // Recursive removal of Parent destroys CameraChild and clears activeCamera automatically.
    FE_REQUIRE_TRUE(scene.removeObject(parent, RemovalMode::Recursive).hasValue());
    FE_EXPECT_EQ(scene.objectCount(), static_cast<std::size_t>(0));
    FE_EXPECT_FALSE(scene.activeCamera().isValid());
}

FE_TEST(Scene, MeshValidationRejectsOutOfBoundsIndices) {
    MeshData invalid_mesh;
    invalid_mesh.vertices.push_back(Vertex{});
    invalid_mesh.vertices.push_back(Vertex{});
    invalid_mesh.vertices.push_back(Vertex{});
    invalid_mesh.indices = {0, 1, 5}; // index 5 out of range

    Scene scene;
    const auto res = scene.registerMesh("Broken", std::move(invalid_mesh));
    FE_REQUIRE_TRUE(res.isError());
    FE_EXPECT_EQ(res.error().code, core::ErrorCode::ValidationError);
}

FE_TEST(Scene, RaycastPickingFindsClosestVisibleHierarchicalMesh) {
    Scene scene;
    const auto cube_mesh = scene.registerMesh("Cube", Primitives::createCube(2.0f)).value();

    const auto pivot = scene.createObject("Pivot").value();
    math::Transform pivot_tf;
    pivot_tf.position = {0.0f, 0.0f, -5.0f};
    FE_REQUIRE_TRUE(scene.setObjectTransform(pivot, pivot_tf).hasValue());

    const auto far_cube = scene.createObject("FarCube", pivot).value();
    FE_REQUIRE_TRUE(scene.assignMesh(far_cube, cube_mesh).hasValue());

    const auto near_cube = scene.createObject("NearCube").value();
    math::Transform near_tf;
    near_tf.position = {0.0f, 0.0f, -2.0f};
    near_tf.scale = {0.5f, 2.0f, 0.5f}; // Non-uniform scale: half-depth along Z is 0.5 -> front face at z = -1.5.
    FE_REQUIRE_TRUE(scene.setObjectTransform(near_cube, near_tf).hasValue());
    FE_REQUIRE_TRUE(scene.assignMesh(near_cube, cube_mesh).hasValue());

    const math::Ray ray(math::Vec3{0.0f, 0.0f, 3.0f}, math::Vec3{0.0f, 0.0f, -1.0f});
    const auto hit = raycastScene(scene, ray);
    FE_REQUIRE_TRUE(hit.has_value());
    FE_EXPECT_EQ(hit->object_id, near_cube);
    FE_EXPECT_NEAR(hit->distance, 4.5f, 1e-4f); // From z=+3 to z=-1.5 is 4.5 units.
    FE_EXPECT_TRUE(hit->world_normal.approxEqual(math::Vec3{0.0f, 0.0f, 1.0f}, 1e-4f));

    // Hiding NearCube hits FarCube (center at z=-5, half-size=1 -> front face at z=-4, distance=7).
    FE_REQUIRE_TRUE(scene.setObjectVisible(near_cube, false).hasValue());
    const auto far_hit = raycastScene(scene, ray);
    FE_REQUIRE_TRUE(far_hit.has_value());
    FE_EXPECT_EQ(far_hit->object_id, far_cube);
    FE_EXPECT_NEAR(far_hit->distance, 7.0f, 1e-4f);
}

} // namespace

int main(int argc, char** argv) {
    return finaleffect::testing::runAllTests(argc, argv);
}
