// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "test_harness.hpp"

#include "finaleffect/math/constants.hpp"
#include "finaleffect/platform/file_system.hpp"
#include "finaleffect/platform/paths.hpp"
#include "finaleffect/project/binary_asset.hpp"
#include "finaleffect/project/binary_project.hpp"
#include "finaleffect/project/json.hpp"
#include "finaleffect/project/project.hpp"
#include "finaleffect/scene/primitives.hpp"

#include <filesystem>

namespace {

using namespace finaleffect;
using namespace finaleffect::project;

// Wraps a document tree given as JSON text into a valid .fe3d / .fe3a binary container so document
// validation can be exercised independently of the encoder.
std::vector<std::uint8_t> containerFromJsonText(std::string_view json_text) {
    BinaryProjectImage image{};
    image.document = parseJson(json_text).value();
    return BinaryProjectCodec::encode(image).value();
}

FE_TEST(Project, JsonParserEscapesAndErrorReporting) {
    const auto parsed = parseJson(R"({"title":"Final \"Effect\" \u0033D","ok":true,"nums":[-1, 2.5, 1e-3]})");
    FE_REQUIRE_TRUE(parsed.hasValue());
    FE_REQUIRE_TRUE(parsed.value().isObject());
    FE_EXPECT_EQ(parsed.value().findKey("title")->asString(), std::string("Final \"Effect\" 3D"));
    FE_EXPECT_EQ(parsed.value().findKey("ok")->asBool(), true);

    // Malformed syntax reports ParseError with line/column context.
    const auto bad = parseJson("{\n  \"a\": [1, 2, ]\n}");
    FE_REQUIRE_TRUE(bad.isError());
    FE_EXPECT_EQ(bad.error().code, core::ErrorCode::ParseError);
    FE_EXPECT_TRUE(bad.error().context.find("line 2") != std::string::npos);

    // Deep nesting guard prevents stack overflow.
    std::string nested(80, '[');
    nested += std::string(80, ']');
    FE_EXPECT_EQ(parseJson(nested, 32).error().code, core::ErrorCode::ParseError);
}

FE_TEST(Project, FullSceneRoundTripAndDeterministicOutput) {
    ProjectDocument original;
    original.metadata.project_name = "Stage 1 Showcase";
    original.metadata.author = "Final Effect Engineering";
    original.metadata.description = "Round-trip verification project";
    original.scene.setName("Main Composition");

    const auto cube_mesh = original.scene.registerMesh("HeroCubeMesh", scene::Primitives::createCube(1.5f)).value();
    const auto pivot = original.scene.createObject("Pivot").value();
    const auto cube = original.scene.createObject("HeroCube", pivot).value();
    const auto cam_obj = original.scene.createObject("RenderCamera").value();
    const auto light_obj = original.scene.createObject("KeyLight").value();

    math::Transform pivot_tf;
    pivot_tf.position = {1.0f, 2.0f, -3.5f};
    pivot_tf.rotation = math::Quat::fromAxisAngle(math::Vec3::unitY(), math::degToRad(30.0f));
    FE_REQUIRE_TRUE(original.scene.setObjectTransform(pivot, pivot_tf).hasValue());

    math::Transform cube_tf;
    cube_tf.position = {0.0f, 0.75f, 0.0f};
    cube_tf.scale = {1.0f, 1.25f, 1.0f};
    FE_REQUIRE_TRUE(original.scene.setObjectTransform(cube, cube_tf).hasValue());
    FE_REQUIRE_TRUE(original.scene.assignMesh(cube, cube_mesh).hasValue());

    scene::CameraComponent cam;
    cam.fov_y_radians = math::degToRad(55.0f);
    cam.near_plane = 0.2f;
    cam.far_plane = 500.0f;
    FE_REQUIRE_TRUE(original.scene.assignCamera(cam_obj, cam).hasValue());
    FE_REQUIRE_TRUE(original.scene.setActiveCamera(cam_obj).hasValue());

    scene::DirectionalLightComponent key_light{.color = {1.0f, 0.95f, 0.9f}, .intensity = 1.4f};
    FE_REQUIRE_TRUE(original.scene.assignDirectionalLight(light_obj, key_light).hasValue());

    // Save to disk and reload via LocalFileSystem.
    platform::LocalFileSystem fs;
    const auto temp_dir = platform::queryAppPaths("finaleffect3d_projtest").value().temp_directory;
    const auto file_path = temp_dir / "showcase.fe3d";
    std::error_code ec;
    std::filesystem::remove_all(temp_dir, ec);

    FE_REQUIRE_TRUE(ProjectSerializer::saveToFile(original, file_path, fs).hasValue());
    const auto loaded_res = ProjectSerializer::loadFromFile(file_path, fs);
    FE_REQUIRE_TRUE(loaded_res.hasValue());
    const auto& loaded = loaded_res.value();

    FE_EXPECT_EQ(loaded.metadata.project_name, original.metadata.project_name);
    FE_EXPECT_EQ(loaded.metadata.author, original.metadata.author);
    FE_EXPECT_EQ(loaded.scene.name(), std::string("Main Composition"));
    FE_EXPECT_EQ(loaded.scene.objectCount(), original.scene.objectCount());
    FE_EXPECT_EQ(loaded.scene.meshCount(), original.scene.meshCount());
    FE_EXPECT_EQ(loaded.scene.activeCamera(), cam_obj);

    const auto orig_world = original.scene.computeWorldMatrix(cube).value();
    const auto load_world = loaded.scene.computeWorldMatrix(cube).value();
    FE_EXPECT_TRUE(orig_world.approxEqual(load_world, 1e-4f));

    // Re-serializing produces byte-identical container output (deterministic encoding).
    const auto bytes_a = ProjectSerializer::serializeToBytes(original).value();
    const auto bytes_b = ProjectSerializer::serializeToBytes(loaded).value();
    FE_EXPECT_EQ(bytes_a, bytes_b);
    FE_EXPECT_TRUE(BinaryProjectCodec::isBinaryProjectImage(bytes_a));
    FE_EXPECT_EQ(fs.readBinaryFile(file_path).value(), bytes_a);

    std::filesystem::remove_all(temp_dir, ec);
}

FE_TEST(Project, RejectsUnsupportedVersionsAndTamperedData) {
    // 1. Unsupported major schema version (document tree inside a valid container).
    const char* future_major = R"({
        "format": "finaleffect.project",
        "schema_version": { "major": 99, "minor": 0 },
        "coordinate_system": "right-handed-y-up",
        "scene": { "name": "Future", "objects": [] }
    })";
    FE_EXPECT_EQ(
        ProjectSerializer::deserializeFromBytes(containerFromJsonText(future_major)).error().code,
        core::ErrorCode::UnsupportedVersion);

    // 2. Unsupported minor schema version (newer minor than reader supports).
    const char* future_minor = R"({
        "format": "finaleffect.project",
        "schema_version": { "major": 1, "minor": 5 },
        "coordinate_system": "right-handed-y-up",
        "scene": { "name": "FutureMinor", "objects": [] }
    })";
    FE_EXPECT_EQ(
        ProjectSerializer::deserializeFromBytes(containerFromJsonText(future_minor)).error().code,
        core::ErrorCode::UnsupportedVersion);

    // 3. Wrong format identifier inside a valid container.
    const char* wrong_magic = R"({
        "format": "other.app.project",
        "schema_version": { "major": 1, "minor": 0 },
        "coordinate_system": "right-handed-y-up",
        "scene": { "name": "Wrong", "objects": [] }
    })";
    FE_EXPECT_EQ(
        ProjectSerializer::deserializeFromBytes(containerFromJsonText(wrong_magic)).error().code,
        core::ErrorCode::ValidationError);

    // 4. Hierarchy cycle inside serialized objects (1 -> 2 -> 1).
    const char* cycle_project = R"({
        "format": "finaleffect.project",
        "schema_version": { "major": 1, "minor": 0 },
        "coordinate_system": "right-handed-y-up",
        "scene": {
            "name": "Cyclic",
            "objects": [
                {
                    "id": 1,
                    "name": "NodeA",
                    "parent_id": 2,
                    "local_transform": { "position": [0,0,0], "rotation": [0,0,0,1], "scale": [1,1,1] }
                },
                {
                    "id": 2,
                    "name": "NodeB",
                    "parent_id": 1,
                    "local_transform": { "position": [0,0,0], "rotation": [0,0,0,1], "scale": [1,1,1] }
                }
            ]
        }
    })";
    FE_EXPECT_EQ(
        ProjectSerializer::deserializeFromBytes(containerFromJsonText(cycle_project)).error().code,
        core::ErrorCode::HierarchyCycle);

    // 5. Dangling mesh reference on object.
    const char* dangling_mesh = R"({
        "format": "finaleffect.project",
        "schema_version": { "major": 1, "minor": 0 },
        "coordinate_system": "right-handed-y-up",
        "scene": {
            "name": "DanglingMesh",
            "objects": [
                {
                    "id": 1,
                    "name": "OrphanMeshObj",
                    "parent_id": 0,
                    "mesh_id": 404,
                    "local_transform": { "position": [0,0,0], "rotation": [0,0,0,1], "scale": [1,1,1] }
                }
            ]
        }
    })";
    FE_EXPECT_EQ(
        ProjectSerializer::deserializeFromBytes(containerFromJsonText(dangling_mesh)).error().code,
        core::ErrorCode::ValidationError);
}

FE_TEST(Project, BinaryAssetBundleDeduplicationCrcAndPathTraversalProtection) {
    // 1. Content-addressed deduplication in BinaryAssetBuilder + CRC-32 tampering detection.
    BinaryAssetBuilder builder;
    const auto cube_a = scene::Primitives::createCube(1.0f);
    const auto cube_b = scene::Primitives::createCube(1.0f); // identical geometry
    const auto plane = scene::Primitives::createCheckerPlane(2.0f, 2);

    const auto hash_a = builder.addMesh(cube_a).value();
    const auto hash_b = builder.addMesh(cube_b).value();
    const auto hash_plane = builder.addMesh(plane).value();

    FE_EXPECT_EQ(hash_a, hash_b);
    FE_EXPECT_TRUE(hash_a != hash_plane);
    FE_EXPECT_EQ(builder.uniqueChunkCount(), std::size_t{2}); // deduplicated!

    auto bytes = builder.encode();
    const auto decoded = BinaryAssetArchive::decode(bytes);
    FE_REQUIRE_TRUE(decoded.hasValue());
    FE_EXPECT_EQ(decoded.value().chunkCount(), std::size_t{2});
    FE_EXPECT_EQ(decoded.value().extractMesh(hash_a).value().vertices.size(), cube_a.vertices.size());

    // Flipping a single payload bit triggers CRC-32 validation failure.
    auto tampered = bytes;
    tampered[40] ^= 0x01u;
    FE_EXPECT_EQ(BinaryAssetArchive::decode(tampered).error().code, core::ErrorCode::ValidationError);

    // 2. Self-contained .fe3d container round-trip: geometry is embedded, never a sidecar file.
    ProjectDocument doc;
    doc.metadata.project_name = "Binary Bundle Scene";
    doc.scene.setName("Bundled");

    const auto m1 = doc.scene.registerMesh("CubePrimary", cube_a).value();
    const auto m2 = doc.scene.registerMesh("CubeDuplicateAsset", cube_b).value();
    const auto o1 = doc.scene.createObject("Obj1").value();
    const auto o2 = doc.scene.createObject("Obj2").value();
    FE_REQUIRE_TRUE(doc.scene.assignMesh(o1, m1).hasValue());
    FE_REQUIRE_TRUE(doc.scene.assignMesh(o2, m2).hasValue());

    platform::LocalFileSystem fs;
    const auto temp_dir = platform::queryAppPaths("finaleffect3d_bintest").value().temp_directory;
    std::error_code ec;
    std::filesystem::remove_all(temp_dir, ec);

    const auto project_path = temp_dir / "bundle.fe3d";
    const auto legacy_sidecar_path = temp_dir / "bundle.fe3dbin";

    FE_REQUIRE_TRUE(ProjectSerializer::saveToFile(doc, project_path, fs).hasValue());

    // Saving writes exactly one self-contained container; no .fe3dbin companion is created.
    FE_EXPECT_TRUE(fs.exists(project_path));
    FE_EXPECT_TRUE(!fs.exists(legacy_sidecar_path));

    // The container carries the embedded, deduplicated, CRC-checked geometry archive.
    const auto container_bytes = fs.readBinaryFile(project_path).value();
    FE_EXPECT_TRUE(BinaryProjectCodec::isBinaryProjectImage(container_bytes));
    const auto decoded_container = BinaryProjectCodec::decode(container_bytes);
    FE_REQUIRE_TRUE(decoded_container.hasValue());
    FE_EXPECT_TRUE(decoded_container.value().hasGeometry());
    const auto embedded_archive = BinaryAssetArchive::decode(decoded_container.value().geometry_archive);
    FE_REQUIRE_TRUE(embedded_archive.hasValue());
    FE_EXPECT_EQ(embedded_archive.value().chunkCount(), std::size_t{2}); // identical cubes deduplicated
    FE_EXPECT_TRUE(container_bytes.size() < 16u * 1024u); // packed binary geometry, not text

    const auto loaded_bundle = ProjectSerializer::loadFromFile(project_path, fs);
    FE_REQUIRE_TRUE(loaded_bundle.hasValue());
    FE_EXPECT_EQ(loaded_bundle.value().scene.meshCount(), std::size_t{2});
    FE_EXPECT_EQ(loaded_bundle.value().scene.findMesh(m1)->data.indices.size(), cube_a.indices.size());
    FE_EXPECT_EQ(loaded_bundle.value().scene.findMesh(m2)->data.vertices.size(), cube_b.vertices.size());

    // 3. A mesh that references embedded geometry is rejected cleanly when no geometry archive is
    //    present in the container; companion-file URIs are informational and never opened.
    const auto traversal_path = temp_dir / "traversal.fe3d";
    const char* malicious_json = R"({
        "format": "finaleffect.project",
        "schema_version": { "major": 1, "minor": 1 },
        "coordinate_system": "right-handed-y-up",
        "scene": {
            "name": "Malicious",
            "meshes": [
                {
                    "id": 1,
                    "name": "EvilMesh",
                    "binary_chunk": {
                        "uri": "../outside.fe3dbin",
                        "content_hash": "0123456789abcdef"
                    }
                }
            ],
            "objects": []
        }
    })";
    FE_REQUIRE_TRUE(fs.writeBinaryFile(traversal_path, containerFromJsonText(malicious_json)).hasValue());
    FE_EXPECT_EQ(
        ProjectSerializer::loadFromFile(traversal_path, fs).error().code,
        core::ErrorCode::ValidationError);

    // 4. Legacy JSON project text is refused outright: the format is binary-only.
    const auto legacy_path = temp_dir / "legacy.fe3d";
    FE_REQUIRE_TRUE(fs.writeTextFile(legacy_path, malicious_json).hasValue());
    FE_EXPECT_EQ(
        ProjectSerializer::loadFromFile(legacy_path, fs).error().code,
        core::ErrorCode::ValidationError);

    std::filesystem::remove_all(temp_dir, ec);
}

FE_TEST(Project, Stage4EditableMeshMaterialsEditorStateAndWorkspaceRoundTrip) {
    ProjectDocument doc;
    doc.metadata.project_name = "Stage 4 Studio Project";
    doc.scene.setName("SculptAndShade");

    const auto titanium = doc.scene.createMaterial(scene::PbrMaterial{
        .name = "Titanium",
        .base_color = {0.72f, 0.75f, 0.79f},
        .metallic = 1.0f,
        .roughness = 0.24f,
        .ambient_occlusion = 0.95f,
    }).value();

    const auto neon = doc.scene.createMaterial(scene::PbrMaterial{
        .name = "NeonCyan",
        .base_color = {0.10f, 0.85f, 0.95f},
        .metallic = 0.0f,
        .roughness = 0.18f,
        .ambient_occlusion = 1.0f,
        .emissive_color = {0.10f, 0.90f, 1.0f},
        .emissive_strength = 2.5f,
        .opacity = 0.80f,
    }).value();

    scene::EditableMesh cube = scene::EditableMesh::createCube(2.0f);
    const std::uint32_t top_face = 4;
    FE_REQUIRE_TRUE(cube.extrudeFaces({&top_face, 1}, 0.8f).hasValue());
    FE_REQUIRE_TRUE(cube.assignMaterialToFaces({&top_face, 1}, neon).hasValue());

    const auto mesh_id = doc.scene.registerEditableMesh("ExtrudedHero", std::move(cube)).value();
    const auto obj_id = doc.scene.createObject("HeroAsset").value();
    FE_REQUIRE_TRUE(doc.scene.assignMesh(obj_id, mesh_id).hasValue());
    FE_REQUIRE_TRUE(doc.scene.assignMaterial(obj_id, titanium).hasValue());

    doc.editor_state.edit_mode = scene::EditMode::Face;
    doc.editor_state.gizmo_tool = "rotate";
    doc.editor_state.gizmo_space = "local";
    doc.editor_state.selected_object_id = obj_id;
    doc.editor_state.active_material_id = neon;
    doc.editor_state.mesh_selection.faces = {4u};

    doc.workspace.layout_name = "Shading";
    doc.workspace.focused_panel = WorkspacePanelId::MaterialEditor;
    doc.workspace.left_dock_width_ratio = 0.21f;
    doc.workspace.panels[static_cast<std::size_t>(WorkspacePanelId::Timeline)].visible = true;

    const auto container_a = ProjectSerializer::serializeToBytes(doc).value();
    const auto loaded_res = ProjectSerializer::deserializeFromBytes(container_a);
    FE_REQUIRE_TRUE(loaded_res.hasValue());
    const auto& loaded = loaded_res.value();

    FE_EXPECT_EQ(loaded.scene.materialCount(), std::size_t{2});
    FE_EXPECT_TRUE(loaded.scene.findMaterial(neon)->approxEqualProperties(*doc.scene.findMaterial(neon)));
    FE_EXPECT_EQ(loaded.scene.findObject(obj_id)->material_id.value(), titanium);

    const auto* loaded_mesh = loaded.scene.findMesh(mesh_id);
    FE_REQUIRE_TRUE(loaded_mesh != nullptr && loaded_mesh->editable_mesh.has_value());
    FE_EXPECT_EQ(loaded_mesh->editable_mesh->vertexCount(), std::size_t{12});
    FE_EXPECT_EQ(loaded_mesh->editable_mesh->faceCount(), std::size_t{10});
    FE_EXPECT_EQ(loaded_mesh->editable_mesh->faces()[4].material_id, neon);
    FE_EXPECT_EQ(loaded_mesh->data.submeshes.size(), std::size_t{2});

    FE_EXPECT_EQ(loaded.editor_state.edit_mode, scene::EditMode::Face);
    FE_EXPECT_EQ(loaded.editor_state.gizmo_tool, std::string("rotate"));
    FE_EXPECT_EQ(loaded.editor_state.gizmo_space, std::string("local"));
    FE_EXPECT_EQ(loaded.editor_state.selected_object_id, obj_id);
    FE_EXPECT_EQ(loaded.editor_state.active_material_id, neon);
    FE_EXPECT_EQ(loaded.editor_state.mesh_selection.faces.size(), std::size_t{1});

    FE_EXPECT_EQ(loaded.workspace.layout_name, std::string("Shading"));
    FE_EXPECT_EQ(loaded.workspace.focused_panel, WorkspacePanelId::MaterialEditor);
    FE_EXPECT_TRUE(loaded.workspace.panels[static_cast<std::size_t>(WorkspacePanelId::Timeline)].visible);

    const auto container_b = ProjectSerializer::serializeToBytes(loaded).value();
    FE_EXPECT_EQ(container_a, container_b);
}

FE_TEST(Project, BackwardCompatibilityV10V11AndLosslessUnknownFieldPreservation) {
    const char* legacy_v10_with_extensions = R"({
        "format": "finaleffect.project",
        "schema_version": { "major": 1, "minor": 0 },
        "coordinate_system": "right-handed-y-up",
        "metadata": {
            "project_name": "Legacy V1.0 Asset",
            "studio_pipeline_tag": "archive-2026"
        },
        "scene": {
            "name": "LegacyScene",
            "custom_environment_note": "hdri-placeholder",
            "objects": [
                {
                    "id": 1,
                    "name": "EmptyPivot",
                    "parent_id": 0,
                    "local_transform": { "position": [1, 2, 3], "rotation": [0, 0, 0, 1], "scale": [1, 1, 1] },
                    "user_tag": "hero_anchor"
                }
            ]
        },
        "custom_plugin_data": { "version": 3, "enabled": true }
    })";

    const auto loaded_res = ProjectSerializer::deserializeFromBytes(containerFromJsonText(legacy_v10_with_extensions));
    FE_REQUIRE_TRUE(loaded_res.hasValue());
    const auto& loaded = loaded_res.value();

    FE_EXPECT_EQ(loaded.metadata.project_name, std::string("Legacy V1.0 Asset"));
    FE_EXPECT_EQ(loaded.scene.objectCount(), std::size_t{1});
    // Unknown extension fields are explicitly flagged in load_warnings AND preserved on re-save!
    FE_EXPECT_TRUE(loaded.load_warnings.size() >= std::size_t{4});

    const auto saved_container = ProjectSerializer::serializeToBytes(loaded).value();
    const auto reparsed = BinaryProjectCodec::decode(saved_container).value().document;
    FE_REQUIRE_TRUE(reparsed.findKey("custom_plugin_data") != nullptr);
    FE_EXPECT_EQ(reparsed.findKey("custom_plugin_data")->findKey("enabled")->asBool(), true);
    FE_EXPECT_EQ(
        reparsed.findKey("metadata")->findKey("studio_pipeline_tag")->asString(),
        std::string("archive-2026"));
    FE_EXPECT_EQ(
        reparsed.findKey("scene")->findKey("custom_environment_note")->asString(),
        std::string("hdri-placeholder"));
    FE_EXPECT_EQ(
        reparsed.findKey("scene")->findKey("objects")->asArray()[0].findKey("user_tag")->asString(),
        std::string("hero_anchor"));
}

} // namespace

int main(int argc, char** argv) {
    return finaleffect::testing::runAllTests(argc, argv);
}
