// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/scene/commands.hpp"
#include "finaleffect/scene/editable_mesh.hpp"
#include "finaleffect/scene/material.hpp"
#include "finaleffect/scene/scene.hpp"
#include "test_harness.hpp"

#include <limits>
#include <memory>

namespace {

using finaleffect::math::Vec3;
using finaleffect::scene::AssignObjectMaterialCommand;
using finaleffect::scene::CommandStack;
using finaleffect::scene::CreateMaterialCommand;
using finaleffect::scene::DeleteMaterialCommand;
using finaleffect::scene::EditableMesh;
using finaleffect::scene::MaterialId;
using finaleffect::scene::ModifyMeshCommand;
using finaleffect::scene::PbrMaterial;
using finaleffect::scene::Scene;
using finaleffect::scene::UpdateMaterialCommand;

FE_TEST(Material, MaterialValidationAndSanitization) {
    PbrMaterial valid{
        .id = MaterialId{1},
        .name = "BrushedGold",
        .base_color = {1.0f, 0.78f, 0.34f},
        .metallic = 1.0f,
        .roughness = 0.22f,
        .ambient_occlusion = 1.0f,
        .emissive_color = {0.0f, 0.0f, 0.0f},
        .emissive_strength = 0.0f,
        .opacity = 1.0f,
    };
    FE_EXPECT_TRUE(valid.validate().hasValue());

    // Out-of-range values fail validation
    PbrMaterial bad_metallic = valid;
    bad_metallic.metallic = 1.4f;
    FE_EXPECT_TRUE(!bad_metallic.validate().hasValue());

    PbrMaterial bad_rough = valid;
    bad_rough.roughness = -0.1f;
    FE_EXPECT_TRUE(!bad_rough.validate().hasValue());

    PbrMaterial bad_nan = valid;
    bad_nan.opacity = std::numeric_limits<float>::quiet_NaN();
    FE_EXPECT_TRUE(!bad_nan.validate().hasValue());

    // Sanitized copy clamps roughness to >= 0.04 (anti-firefly GGX guard) and bounds [0, 1]
    PbrMaterial raw = valid;
    raw.name = "";
    raw.metallic = 2.0f;
    raw.roughness = 0.0f;
    raw.opacity = -0.5f;
    const PbrMaterial clean = raw.sanitized();
    FE_EXPECT_TRUE(clean.validate().hasValue());
    FE_EXPECT_EQ(clean.name, std::string("Untitled Material"));
    FE_EXPECT_NEAR(clean.metallic, 1.0f, 1.0e-6f);
    FE_EXPECT_NEAR(clean.roughness, 0.04f, 1.0e-6f);
    FE_EXPECT_NEAR(clean.opacity, 0.0f, 1.0e-6f);
}

FE_TEST(Material, SceneMaterialLifecycleMultiMaterialSubmeshesAndSafeDeletion) {
    Scene scene("MaterialScene");

    const auto copper_id = scene.createMaterial(PbrMaterial{
        .name = "PolishedCopper",
        .base_color = {0.95f, 0.64f, 0.54f},
        .metallic = 1.0f,
        .roughness = 0.18f,
    }).value();

    const auto glass_id = scene.duplicateMaterial(copper_id, "FrostedGlass").value();
    PbrMaterial glass = *scene.findMaterial(glass_id);
    glass.base_color = {0.86f, 0.94f, 0.99f};
    glass.metallic = 0.0f;
    glass.roughness = 0.15f;
    glass.opacity = 0.45f;
    FE_EXPECT_TRUE(scene.updateMaterial(glass_id, glass).hasValue());
    FE_EXPECT_TRUE(scene.renameMaterial(copper_id, "AgedCopper").hasValue());
    FE_EXPECT_EQ(scene.findMaterial(copper_id)->name, std::string("AgedCopper"));
    FE_EXPECT_EQ(scene.materialCount(), static_cast<std::size_t>(2));

    // Create editable cube and assign object-level copper + per-face glass on top/bottom faces
    const auto mesh_id = scene.registerEditableMesh("CubeMesh", EditableMesh::createCube(2.0f)).value();
    const auto obj_id = scene.createObject("ShowcaseCube").value();
    FE_EXPECT_TRUE(scene.assignMesh(obj_id, mesh_id).hasValue());
    FE_EXPECT_TRUE(scene.assignMaterial(obj_id, copper_id).hasValue());

    EditableMesh edited = scene.findMesh(mesh_id)->editable_mesh.value();
    const std::uint32_t glass_faces[2] = {4, 5}; // +Y top, -Y bottom
    FE_EXPECT_TRUE(edited.assignMaterialToFaces(glass_faces, glass_id).hasValue());
    FE_EXPECT_TRUE(scene.updateMeshEditable(mesh_id, std::move(edited)).hasValue());

    // Triangulated mesh now has 2 deterministic submesh slices: MaterialId{0} (4 faces = 24 indices) and glass_id (2 faces = 12 indices)
    const auto* mesh_asset = scene.findMesh(mesh_id);
    FE_EXPECT_EQ(mesh_asset->data.submeshes.size(), static_cast<std::size_t>(2));
    FE_EXPECT_EQ(mesh_asset->data.submeshes[0].material_id, MaterialId::invalid());
    FE_EXPECT_EQ(mesh_asset->data.submeshes[0].index_count, 24u);
    FE_EXPECT_EQ(mesh_asset->data.submeshes[1].material_id, glass_id);
    FE_EXPECT_EQ(mesh_asset->data.submeshes[1].index_count, 12u);

    // Safe deletion via CommandStack detaches object & per-face references and restores them on undo!
    CommandStack stack;
    FE_EXPECT_TRUE(stack.execute(scene, std::make_unique<DeleteMaterialCommand>(glass_id)).hasValue());
    FE_EXPECT_EQ(scene.materialCount(), static_cast<std::size_t>(1));
    FE_EXPECT_EQ(scene.findMesh(mesh_id)->data.submeshes.size(), static_cast<std::size_t>(1));

    // Delete object-assigned copper material as well -> object material_id safely becomes nullopt
    FE_EXPECT_TRUE(stack.execute(scene, std::make_unique<DeleteMaterialCommand>(copper_id)).hasValue());
    FE_EXPECT_EQ(scene.materialCount(), static_cast<std::size_t>(0));
    FE_EXPECT_TRUE(!scene.findObject(obj_id)->material_id.has_value());

    // Undo both deletions -> restores materials, object assignment, AND per-face submesh assignments!
    FE_EXPECT_TRUE(stack.undo(scene).hasValue());
    FE_EXPECT_TRUE(scene.findObject(obj_id)->material_id.has_value());
    FE_EXPECT_EQ(scene.findObject(obj_id)->material_id.value(), copper_id);

    FE_EXPECT_TRUE(stack.undo(scene).hasValue());
    FE_EXPECT_EQ(scene.materialCount(), static_cast<std::size_t>(2));
    FE_EXPECT_EQ(scene.findMesh(mesh_id)->data.submeshes.size(), static_cast<std::size_t>(2));
}

FE_TEST(Material, MaterialCommandsUndoRedoAndSliderCoalescing) {
    Scene scene("MatCmdScene");
    CommandStack stack;

    auto create_cmd = std::make_unique<CreateMaterialCommand>(PbrMaterial{
        .name = "Cobalt",
        .base_color = {0.15f, 0.35f, 0.85f},
        .metallic = 0.8f,
        .roughness = 0.35f,
    });
    auto* create_ptr = create_cmd.get();
    FE_EXPECT_TRUE(stack.execute(scene, std::move(create_cmd)).hasValue());
    const MaterialId mat_id = create_ptr->createdMaterialId();
    FE_EXPECT_TRUE(mat_id.isValid());

    const auto obj_id = scene.createObject("SphereObj").value();
    FE_EXPECT_TRUE(stack.execute(scene, std::make_unique<AssignObjectMaterialCommand>(obj_id, mat_id)).hasValue());
    FE_EXPECT_EQ(scene.findObject(obj_id)->material_id.value(), mat_id);

    // Coalesced roughness slider drag
    for (float r : {0.30f, 0.20f, 0.12f}) {
        PbrMaterial m = *scene.findMaterial(mat_id);
        m.roughness = r;
        FE_EXPECT_TRUE(stack.execute(
            scene,
            std::make_unique<UpdateMaterialCommand>(mat_id, m, true, 7)).hasValue());
    }
    FE_EXPECT_EQ(stack.undoCount(), static_cast<std::size_t>(3)); // Create + Assign + 1 coalesced Update
    FE_EXPECT_NEAR(scene.findMaterial(mat_id)->roughness, 0.12f, 1.0e-5f);

    // Undo coalesced slider drag -> reverts directly to 0.35
    FE_EXPECT_TRUE(stack.undo(scene).hasValue());
    FE_EXPECT_NEAR(scene.findMaterial(mat_id)->roughness, 0.35f, 1.0e-5f);

    // Redo -> back to 0.12
    FE_EXPECT_TRUE(stack.redo(scene).hasValue());
    FE_EXPECT_NEAR(scene.findMaterial(mat_id)->roughness, 0.12f, 1.0e-5f);
}

} // namespace

int main(int argc, char** argv) {
    return finaleffect::testing::runAllTests(argc, argv);
}
