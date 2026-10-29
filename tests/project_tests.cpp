// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "test_harness.hpp"

#include "finaleffect/math/constants.hpp"
#include "finaleffect/platform/file_system.hpp"
#include "finaleffect/platform/paths.hpp"
#include "finaleffect/project/json.hpp"
#include "finaleffect/project/project.hpp"
#include "finaleffect/scene/primitives.hpp"

#include <filesystem>

namespace {

using namespace finaleffect;
using namespace finaleffect::project;

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

    // Re-serializing produces identical JSON text.
    const auto text_a = ProjectSerializer::serializeToString(original).value();
    const auto text_b = ProjectSerializer::serializeToString(loaded).value();
    FE_EXPECT_EQ(text_a, text_b);

    std::filesystem::remove_all(temp_dir, ec);
}

FE_TEST(Project, RejectsUnsupportedVersionsAndTamperedData) {
    // 1. Unsupported major schema version.
    const char* future_major = R"({
        "format": "finaleffect.project",
        "schema_version": { "major": 99, "minor": 0 },
        "coordinate_system": "right-handed-y-up",
        "scene": { "name": "Future", "objects": [] }
    })";
    FE_EXPECT_EQ(
        ProjectSerializer::deserializeFromString(future_major).error().code,
        core::ErrorCode::UnsupportedVersion);

    // 2. Unsupported minor schema version (newer minor than reader supports).
    const char* future_minor = R"({
        "format": "finaleffect.project",
        "schema_version": { "major": 1, "minor": 5 },
        "coordinate_system": "right-handed-y-up",
        "scene": { "name": "FutureMinor", "objects": [] }
    })";
    FE_EXPECT_EQ(
        ProjectSerializer::deserializeFromString(future_minor).error().code,
        core::ErrorCode::UnsupportedVersion);

    // 3. Wrong format identifier.
    const char* wrong_magic = R"({
        "format": "other.app.project",
        "schema_version": { "major": 1, "minor": 0 },
        "coordinate_system": "right-handed-y-up",
        "scene": { "name": "Wrong", "objects": [] }
    })";
    FE_EXPECT_EQ(
        ProjectSerializer::deserializeFromString(wrong_magic).error().code,
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
        ProjectSerializer::deserializeFromString(cycle_project).error().code,
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
        ProjectSerializer::deserializeFromString(dangling_mesh).error().code,
        core::ErrorCode::ValidationError);
}

} // namespace

int main(int argc, char** argv) {
    return finaleffect::testing::runAllTests(argc, argv);
}
