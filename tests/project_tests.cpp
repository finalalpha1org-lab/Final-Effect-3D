// SPDX-License-Identifier: GPL-3.0-only
#include <string>
#include <vector>

#include "TestFramework.hpp"
#include "finaleffect/platform/FileSystem.hpp"
#include "finaleffect/platform/Paths.hpp"
#include "finaleffect/project/ProjectFile.hpp"
#include "finaleffect/scene/Scene.hpp"

namespace {
using fe::project::ProjectDocument;

std::string sampleProjectJson() {
    fe::scene::Scene scene;
    const fe::scene::ObjectId parent = scene.createObject("Root").value();
    (void)scene.createObject("Child", parent);
    scene.find(parent)->mesh = "cube";
    scene.find(parent)->localTransform.position = fe::math::Vec3(1.0F, 2.0F, 3.0F);
    const ProjectDocument document = ProjectDocument::fromScene(scene);
    const fe::core::Result<std::string> text = document.toJsonText();
    return text.ok() ? text.value() : std::string();
}
}  // namespace

FE3D_TEST(project, scene_round_trip_through_json) {
    fe::scene::Scene scene;
    const fe::scene::ObjectId ground = scene.createObject("Ground").value();
    const fe::scene::ObjectId box = scene.createObject("Box", ground).value();
    scene.find(ground)->mesh = "plane";
    scene.find(ground)->color = fe::math::Vec3(0.1F, 0.2F, 0.3F);
    scene.find(ground)->localTransform.scale = fe::math::Vec3(4.0F, 1.0F, 4.0F);
    scene.find(box)->mesh = "cube";
    scene.find(box)->visible = false;
    scene.find(box)->localTransform.position = fe::math::Vec3(-1.0F, 0.5F, 2.0F);
    scene.find(box)->localTransform.rotation = fe::math::Quat::fromEulerAngles(0.3F, -0.4F, 0.5F);

    const ProjectDocument document = ProjectDocument::fromScene(scene);
    const fe::core::Result<std::string> text = document.toJsonText();
    FE3D_REQUIRE(text.ok());

    const fe::core::Result<ProjectDocument> reparsed = ProjectDocument::fromJsonText(text.value());
    FE3D_REQUIRE(reparsed.ok());
    FE3D_CHECK_EQ(reparsed.value().formatVersion, fe::project::kProjectFormatVersion);
    FE3D_CHECK_EQ(reparsed.value().objects.size(), std::size_t(2));

    const fe::core::Result<fe::scene::Scene> rebuilt = reparsed.value().toScene();
    FE3D_REQUIRE(rebuilt.ok());
    FE3D_CHECK_EQ(rebuilt.value().objectCount(), std::size_t(2));

    const fe::scene::SceneObject* rebuiltGround = rebuilt.value().find(ground);
    const fe::scene::SceneObject* rebuiltBox = rebuilt.value().find(box);
    FE3D_REQUIRE(rebuiltGround != nullptr);
    FE3D_REQUIRE(rebuiltBox != nullptr);
    FE3D_CHECK_EQ(rebuiltGround->name, std::string("Ground"));
    FE3D_CHECK_EQ(rebuiltGround->mesh, std::string("plane"));
    FE3D_CHECK(rebuiltGround->color.isApprox(fe::math::Vec3(0.1F, 0.2F, 0.3F), 1.0e-6F));
    FE3D_CHECK(rebuiltGround->localTransform.scale.isApprox(fe::math::Vec3(4.0F, 1.0F, 4.0F), 1.0e-6F));
    FE3D_CHECK_EQ(rebuiltBox->parent, ground);
    FE3D_CHECK(!rebuiltBox->visible);
    FE3D_CHECK(rebuiltBox->localTransform.isApprox(scene.find(box)->localTransform, 1.0e-5F));
    FE3D_CHECK_EQ(rebuilt.value().find(ground)->children.size(), std::size_t(1));

    // Serializing twice produces identical bytes (deterministic output).
    const fe::core::Result<std::string> again = reparsed.value().toJsonText();
    FE3D_REQUIRE(again.ok());
    FE3D_CHECK_EQ(again.value(), reparsed.value().toJsonText().value());
}

FE3D_TEST(project, rejects_malformed_and_unsupported_files) {
    const auto expectCode = [](const std::string& text, fe::core::ErrorCode code, const char* label) {
        const fe::core::Result<ProjectDocument> parsed = ProjectDocument::fromJsonText(text);
        FE3D_CHECK_MSG(!parsed.ok(), std::string("should be rejected: ") + label);
        if (parsed.ok()) {
            return;
        }
        FE3D_CHECK_MSG(parsed.error().code() == code,
                       std::string(label) + " -> " + parsed.error().toString());
    };

    expectCode("", fe::core::ErrorCode::ParseError, "empty");
    expectCode("{", fe::core::ErrorCode::ParseError, "truncated");
    expectCode("[]", fe::core::ErrorCode::ValidationFailed, "array instead of object");
    expectCode(R"({"formatVersion": 1})", fe::core::ErrorCode::ValidationFailed, "no format id");
    expectCode(R"({"format": "blender-project", "formatVersion": 1})", fe::core::ErrorCode::ValidationFailed,
               "foreign format");
    expectCode(R"({"format": "finaleffect3d-project"})", fe::core::ErrorCode::ValidationFailed,
               "missing version");
    expectCode(R"({"format": "finaleffect3d-project", "formatVersion": 99})",
               fe::core::ErrorCode::UnsupportedVersion, "newer format");
    expectCode(R"({"format": "finaleffect3d-project", "formatVersion": 0})",
               fe::core::ErrorCode::UnsupportedVersion, "too old format");
    expectCode(R"({"format": "finaleffect3d-project", "formatVersion": 1})",
               fe::core::ErrorCode::ValidationFailed, "no scene");
    expectCode(R"({"format": "finaleffect3d-project", "formatVersion": 1, "scene": {}})",
               fe::core::ErrorCode::ValidationFailed, "scene without objects");
    expectCode(R"({"format": "finaleffect3d-project", "formatVersion": 1, "scene": {"objects": {}}})",
               fe::core::ErrorCode::ValidationFailed, "objects not an array");
    expectCode(R"({"format":"finaleffect3d-project","formatVersion":1,"scene":{"objects":[{"id":1}]}})",
               fe::core::ErrorCode::ValidationFailed, "object without a name");
    expectCode(
        R"({"format":"finaleffect3d-project","formatVersion":1,"scene":{"objects":[{"id":1,"name":"A","parent":2}]}})",
        fe::core::ErrorCode::ValidationFailed, "dangling parent");
    expectCode(
        R"({"format":"finaleffect3d-project","formatVersion":1,"scene":{"objects":[{"id":1,"name":"A"},{"id":1,"name":"B"}]}})",
        fe::core::ErrorCode::ValidationFailed, "duplicate ids");
    expectCode(
        R"({"format":"finaleffect3d-project","formatVersion":1,"scene":{"objects":[{"id":1,"name":"A","parent":2},{"id":2,"name":"B","parent":1}]}})",
        fe::core::ErrorCode::ValidationFailed, "cycle");
    expectCode(
        R"({"format":"finaleffect3d-project","formatVersion":1,"scene":{"objects":[{"id":1,"name":"A","transform":{"position":[0,null,0]}}]}})",
        fe::core::ErrorCode::ValidationFailed, "non-finite transform component");
    expectCode(
        R"({"format":"finaleffect3d-project","formatVersion":1,"scene":{"objects":[{"id":1,"name":"A","transform":{"rotation":[0,0,1]}}]}})",
        fe::core::ErrorCode::ValidationFailed, "rotation with three components");
}

FE3D_TEST(project, tolerates_unknown_keys_and_optional_sections) {
    const std::string text = R"({
        "format": "finaleffect3d-project",
        "formatVersion": 1,
        "revision": 7,
        "futureKey": {"anything": [1, 2, 3]},
        "application": {"name": "Final Effect 3D", "id": "finaleffect3d", "version": "0.9.9"},
        "settings": {"author": "Test", "units": "meters", "rendererHint": "cpu", "unknownSetting": true},
        "scene": {"objects": [{"id": 4, "name": "Solo", "mesh": "sphere", "unknownObjectKey": 12}]}
    })";
    const fe::core::Result<ProjectDocument> parsed = ProjectDocument::fromJsonText(text);
    FE3D_REQUIRE(parsed.ok());
    FE3D_CHECK_EQ(parsed.value().revision, std::uint64_t(7));
    FE3D_CHECK_EQ(parsed.value().applicationVersion, std::string("0.9.9"));
    FE3D_CHECK_EQ(parsed.value().settings.author, std::string("Test"));
    FE3D_CHECK_EQ(parsed.value().settings.rendererHint, std::string("cpu"));
    FE3D_REQUIRE(parsed.value().objects.size() == 1);
    FE3D_CHECK_EQ(parsed.value().objects[0].mesh, std::string("sphere"));
    FE3D_CHECK(parsed.value().objects[0].visible);
}

FE3D_TEST(project, saving_rejects_invalid_documents) {
    ProjectDocument document;
    fe::scene::SceneObject object;
    object.id = fe::scene::ObjectId{1};
    object.name = "Broken";
    object.color = fe::math::Vec3(0.0F, std::nanf(""), 0.0F);
    document.objects.push_back(object);
    FE3D_CHECK(!document.toJsonText().ok());

    ProjectDocument nameless;
    fe::scene::SceneObject unnamed;
    unnamed.id = fe::scene::ObjectId{2};
    unnamed.name = "";
    nameless.objects.push_back(unnamed);
    FE3D_CHECK(!nameless.toJsonText().ok());

    ProjectDocument noId;
    fe::scene::SceneObject unset;
    unset.name = "Unset";
    noId.objects.push_back(unset);
    FE3D_CHECK(!noId.toJsonText().ok());
}

FE3D_TEST(project, file_round_trip_and_error_reporting) {
    fe::platform::StdFileSystem fileSystem;
    const std::string directory = "fe3d-test-project";
    FE3D_REQUIRE(fileSystem.createDirectories(directory).ok());
    const std::string path = fe::platform::joinPath(directory, "sample.fe3d");

    const std::string text = sampleProjectJson();
    FE3D_REQUIRE(!text.empty());
    const fe::core::Result<ProjectDocument> document = ProjectDocument::fromJsonText(text);
    FE3D_REQUIRE(document.ok());

    FE3D_REQUIRE(fe::project::saveProjectFile(fileSystem, path, document.value()).ok());
    FE3D_CHECK(fileSystem.exists(path));
    FE3D_CHECK(!fileSystem.exists(path + ".tmp"));

    const fe::core::Result<ProjectDocument> loaded = fe::project::loadProjectFile(fileSystem, path);
    FE3D_REQUIRE(loaded.ok());
    FE3D_CHECK_EQ(loaded.value().objects.size(), std::size_t(2));

    const fe::core::Result<ProjectDocument> missing =
        fe::project::loadProjectFile(fileSystem, fe::platform::joinPath(directory, "does-not-exist.fe3d"));
    FE3D_CHECK(!missing.ok());
    FE3D_CHECK(missing.error().code() == fe::core::ErrorCode::IoFailure);

    const std::string brokenPath = fe::platform::joinPath(directory, "broken.fe3d");
    FE3D_REQUIRE(fileSystem.writeTextFile(brokenPath, "{ this is not json }").ok());
    const fe::core::Result<ProjectDocument> broken = fe::project::loadProjectFile(fileSystem, brokenPath);
    FE3D_CHECK(!broken.ok());
    FE3D_CHECK(broken.error().code() == fe::core::ErrorCode::ParseError);
    FE3D_CHECK(broken.error().message().find("broken.fe3d") != std::string::npos);

    FE3D_CHECK(fileSystem.removeFile(path).ok());
    FE3D_CHECK(fileSystem.removeFile(brokenPath).ok());
    FE3D_REQUIRE(fileSystem.createDirectories(directory).ok());
}
