// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "test_harness.hpp"

#include "finaleffect/math/constants.hpp"
#include "finaleffect/scene/commands.hpp"
#include "finaleffect/scene/primitives.hpp"

#include <memory>

namespace {

using namespace finaleffect;
using namespace finaleffect::scene;

FE_TEST(Commands, CreateTransformUndoRedoPreservesStableObjectIds) {
    Scene scene;
    const auto cube_mesh = scene.registerMesh("Cube", Primitives::createCube()).value();

    CommandStack stack;
    math::Transform init_tf;
    init_tf.position = {1.0f, 2.0f, 3.0f};

    auto create_cmd = std::make_unique<CreateObjectCommand>("Block", ObjectId::invalid(), init_tf, cube_mesh);
    auto* create_ptr = create_cmd.get();
    FE_REQUIRE_TRUE(stack.execute(scene, std::move(create_cmd)).hasValue());

    const ObjectId block_id = create_ptr->createdObjectId();
    FE_REQUIRE_TRUE(block_id.isValid());
    FE_REQUIRE_TRUE(scene.findObject(block_id) != nullptr);

    // Coalesced transform drag updates merge into a single undo entry.
    for (int i = 1; i <= 5; ++i) {
        math::Transform drag_tf = init_tf;
        drag_tf.position.x = 1.0f + static_cast<float>(i);
        FE_REQUIRE_TRUE(
            stack.execute(scene, std::make_unique<SetTransformCommand>(block_id, drag_tf, true)).hasValue());
    }
    FE_EXPECT_EQ(stack.undoCount(), static_cast<std::size_t>(2)); // Create + 1 coalesced SetTransform
    FE_EXPECT_NEAR(scene.findObject(block_id)->local_transform.position.x, 6.0f, 1e-5f);

    // Undo transform drag -> back to x = 1.0.
    FE_REQUIRE_TRUE(stack.undo(scene).hasValue());
    FE_EXPECT_NEAR(scene.findObject(block_id)->local_transform.position.x, 1.0f, 1e-5f);

    // Undo creation -> object removed.
    FE_REQUIRE_TRUE(stack.undo(scene).hasValue());
    FE_EXPECT_TRUE(scene.findObject(block_id) == nullptr);

    // Redo creation + Redo transform -> exact same ObjectId and final x = 6.0!
    FE_REQUIRE_TRUE(stack.redo(scene).hasValue());
    FE_REQUIRE_TRUE(stack.redo(scene).hasValue());
    FE_REQUIRE_TRUE(scene.findObject(block_id) != nullptr);
    FE_EXPECT_NEAR(scene.findObject(block_id)->local_transform.position.x, 6.0f, 1e-5f);
}

FE_TEST(Commands, DeleteSubtreeUndoRestoresHierarchyComponentsAndActiveCamera) {
    Scene scene;
    const auto mesh_id = scene.registerMesh("Pyramid", Primitives::createPyramid()).value();

    const auto root = scene.createObject("RigRoot").value();
    const auto child_mesh = scene.createObject("MeshChild", root).value();
    const auto child_cam = scene.createObject("CamChild", child_mesh).value();

    FE_REQUIRE_TRUE(scene.assignMesh(child_mesh, mesh_id).hasValue());
    FE_REQUIRE_TRUE(scene.assignCamera(child_cam, CameraComponent{}).hasValue());
    FE_REQUIRE_TRUE(scene.setActiveCamera(child_cam).hasValue());

    CommandStack stack;
    FE_REQUIRE_TRUE(stack.execute(scene, std::make_unique<DeleteObjectCommand>(root, RemovalMode::Recursive)).hasValue());
    FE_EXPECT_EQ(scene.objectCount(), static_cast<std::size_t>(0));
    FE_EXPECT_FALSE(scene.activeCamera().isValid());

    // Undo restores entire 3-node hierarchy, mesh reference, CameraComponent, and activeCamera.
    FE_REQUIRE_TRUE(stack.undo(scene).hasValue());
    FE_EXPECT_EQ(scene.objectCount(), static_cast<std::size_t>(3));
    FE_EXPECT_EQ(scene.activeCamera(), child_cam);
    FE_REQUIRE_TRUE(scene.findObject(child_cam) != nullptr);
    FE_EXPECT_EQ(scene.findObject(child_cam)->parent_id, child_mesh);
    FE_EXPECT_EQ(scene.findObject(child_mesh)->mesh_id.value(), mesh_id);
}

FE_TEST(Commands, CompoundTransactionRollsBackOnFailureAndMemoryCapEvictsOldest) {
    Scene scene;
    const auto a = scene.createObject("NodeA").value();
    const auto b = scene.createObject("NodeB", a).value();

    math::Transform moved;
    moved.position = {42.0f, 0.0f, 0.0f};

    // Transaction: Step 1 moves NodeA to x=42; Step 2 tries illegal cycle reparenting NodeA -> NodeB.
    auto txn = std::make_unique<CompoundTransactionCommand>("MoveAndIllegalReparent");
    txn->addStep(std::make_unique<SetTransformCommand>(a, moved));
    txn->addStep(std::make_unique<ReparentObjectCommand>(a, b)); // Fails with HierarchyCycle!

    CommandStack stack(CommandStackLimits{.max_entries = 3, .max_memory_bytes = 512});
    const auto st = stack.execute(scene, std::move(txn));
    FE_REQUIRE_TRUE(st.isError());
    FE_EXPECT_EQ(st.error().code, core::ErrorCode::HierarchyCycle);

    // Step 1 must have been rolled back automatically: NodeA remains at x = 0.0!
    FE_EXPECT_NEAR(scene.findObject(a)->local_transform.position.x, 0.0f, 1e-6f);
    FE_EXPECT_EQ(stack.undoCount(), static_cast<std::size_t>(0));

    // Verify max_entries cap evicts oldest entries cleanly.
    for (int i = 1; i <= 6; ++i) {
        math::Transform tf;
        tf.position.y = static_cast<float>(i);
        FE_REQUIRE_TRUE(stack.execute(scene, std::make_unique<SetTransformCommand>(a, tf, false)).hasValue());
    }
    FE_EXPECT_TRUE(stack.undoCount() <= 3);
    FE_EXPECT_TRUE(stack.totalMemoryBytes() <= 512);
}

FE_TEST(Commands, RenameVisibilityCameraAndLightUndoRedo) {
    Scene scene;
    CommandStack stack;

    CameraComponent init_cam{};
    init_cam.fov_y_radians = math::degToRad(50.0f);
    DirectionalLightComponent init_light{
        .color = {1.0f, 0.9f, 0.8f},
        .intensity = 1.2f,
    };

    auto create_cmd = std::make_unique<CreateObjectCommand>(
        "RiggedCameraLight",
        ObjectId::invalid(),
        math::Transform::identity(),
        std::nullopt,
        init_cam,
        init_light);
    const auto* raw_create = create_cmd.get();
    FE_REQUIRE_TRUE(stack.execute(scene, std::move(create_cmd)).hasValue());

    const ObjectId obj_id = raw_create->createdObjectId();
    FE_REQUIRE_TRUE(obj_id.isValid());
    FE_EXPECT_EQ(scene.activeCamera(), obj_id);

    // Rename + Undo + Redo.
    FE_REQUIRE_TRUE(stack.execute(scene, std::make_unique<RenameObjectCommand>(obj_id, "MainCam")).hasValue());
    FE_EXPECT_EQ(scene.findObject(obj_id)->name, std::string("MainCam"));
    FE_REQUIRE_TRUE(stack.undo(scene).hasValue());
    FE_EXPECT_EQ(scene.findObject(obj_id)->name, std::string("RiggedCameraLight"));
    FE_REQUIRE_TRUE(stack.redo(scene).hasValue());
    FE_EXPECT_EQ(scene.findObject(obj_id)->name, std::string("MainCam"));

    // Visibility + Undo.
    FE_REQUIRE_TRUE(stack.execute(scene, std::make_unique<SetVisibilityCommand>(obj_id, false)).hasValue());
    FE_EXPECT_FALSE(scene.findObject(obj_id)->visible);
    FE_REQUIRE_TRUE(stack.undo(scene).hasValue());
    FE_EXPECT_TRUE(scene.findObject(obj_id)->visible);

    // Camera component update + Undo.
    CameraComponent wide_cam = init_cam;
    wide_cam.fov_y_radians = math::degToRad(85.0f);
    FE_REQUIRE_TRUE(stack.execute(scene, std::make_unique<SetCameraComponentCommand>(obj_id, wide_cam)).hasValue());
    FE_EXPECT_NEAR(scene.findObject(obj_id)->camera->fov_y_radians, math::degToRad(85.0f), 1e-5f);
    FE_REQUIRE_TRUE(stack.undo(scene).hasValue());
    FE_EXPECT_NEAR(scene.findObject(obj_id)->camera->fov_y_radians, math::degToRad(50.0f), 1e-5f);

    // Coalescing directional light updates + single Undo back to 1.2f.
    DirectionalLightComponent l1{.color = {0.5f, 0.6f, 1.0f}, .intensity = 2.0f};
    DirectionalLightComponent l2{.color = {0.4f, 0.7f, 1.0f}, .intensity = 3.5f};
    const std::size_t undo_before = stack.undoCount();
    FE_REQUIRE_TRUE(stack.execute(scene, std::make_unique<SetDirectionalLightCommand>(obj_id, l1, true)).hasValue());
    FE_REQUIRE_TRUE(stack.execute(scene, std::make_unique<SetDirectionalLightCommand>(obj_id, l2, true)).hasValue());
    FE_EXPECT_EQ(stack.undoCount(), undo_before + 1);
    FE_EXPECT_NEAR(scene.findObject(obj_id)->directional_light->intensity, 3.5f, 1e-5f);
    FE_REQUIRE_TRUE(stack.undo(scene).hasValue());
    FE_EXPECT_NEAR(scene.findObject(obj_id)->directional_light->intensity, 1.2f, 1e-5f);
}

} // namespace

int main(int argc, char** argv) {
    return finaleffect::testing::runAllTests(argc, argv);
}
