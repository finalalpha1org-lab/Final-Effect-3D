// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/app/editor_controller.hpp"
#include "finaleffect/math/constants.hpp"
#include "finaleffect/project/json.hpp"
#include "finaleffect/scene/primitives.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

namespace finaleffect::app {

namespace {

project::JsonValue vec3ToJson(const math::Vec3& v) {
    return project::JsonArray{
        project::JsonValue(static_cast<double>(v.x)),
        project::JsonValue(static_cast<double>(v.y)),
        project::JsonValue(static_cast<double>(v.z)),
    };
}

core::Result<std::uint64_t> parseJsonId(const project::JsonValue* v, std::string_view name) {
    if (v == nullptr || !v->isNumber()) {
        return core::Result<std::uint64_t>::fail(
            core::ErrorCode::InvalidArgument,
            "Expected numeric ID field",
            std::string(name));
    }
    const double d = v->asNumber();
    if (!std::isfinite(d) || d < 0.0 || d > 9007199254740991.0 || std::floor(d) != d) {
        return core::Result<std::uint64_t>::fail(
            core::ErrorCode::InvalidArgument,
            "ID field must be a non-negative integer",
            std::string(name));
    }
    return core::Result<std::uint64_t>::ok(static_cast<std::uint64_t>(d));
}

core::Result<float> parseJsonFloat(const project::JsonValue* v, std::string_view name) {
    if (v == nullptr || !v->isNumber()) {
        return core::Result<float>::fail(
            core::ErrorCode::InvalidArgument,
            "Expected numeric field",
            std::string(name));
    }
    const double d = v->asNumber();
    if (!std::isfinite(d) || std::fabs(d) > static_cast<double>(std::numeric_limits<float>::max())) {
        return core::Result<float>::fail(
            core::ErrorCode::InvalidArgument,
            "Numeric field out of 32-bit float range",
            std::string(name));
    }
    return core::Result<float>::ok(static_cast<float>(d));
}

core::Result<math::Vec3> parseJsonVec3(const project::JsonValue* v, std::string_view name) {
    if (v == nullptr || !v->isArray() || v->asArray().size() != 3) {
        return core::Result<math::Vec3>::fail(
            core::ErrorCode::InvalidArgument,
            "Expected 3-element numeric array",
            std::string(name));
    }
    const auto& arr = v->asArray();
    auto x = parseJsonFloat(&arr[0], name);
    auto y = parseJsonFloat(&arr[1], name);
    auto z = parseJsonFloat(&arr[2], name);
    if (!x) return x.error();
    if (!y) return y.error();
    if (!z) return z.error();
    return core::Result<math::Vec3>::ok(math::Vec3{x.value(), y.value(), z.value()});
}

} // namespace

EditorController::EditorController(StudioSession& session) : session_(session) {}

void EditorController::recordStatus(const core::Status& st, std::string_view ok_message) {
    ++state_revision_;
    if (st.hasValue()) {
        last_status_is_error_ = false;
        last_status_message_ = std::string(ok_message);
    } else {
        last_status_is_error_ = true;
        last_status_message_ = st.error().format();
    }
}

core::Status EditorController::selectObject(scene::ObjectId id) {
    if (id.isValid() && session_.document().scene.findObject(id) == nullptr) {
        auto st = core::Status::fail(core::ErrorCode::NotFound, "Selected object ID does not exist in scene");
        recordStatus(st, {});
        return st;
    }
    session_.setSelectedObject(id);
    if (id.isValid()) {
        const auto* obj = session_.document().scene.findObject(id);
        recordStatus(core::Status::ok(), "Selected '" + obj->name + "'");
    } else {
        recordStatus(core::Status::ok(), "Cleared selection");
    }
    return core::Status::ok();
}

std::optional<scene::RaycastHit> EditorController::pickViewportPixel(float x, float y) {
    auto hit = session_.pickAtScreenPosition(x, y);
    ++state_revision_;
    if (hit.has_value()) {
        if (const auto* obj = session_.document().scene.findObject(hit->object_id); obj != nullptr) {
            last_status_is_error_ = false;
            last_status_message_ = "Picked '" + obj->name + "'";
        }
    } else {
        last_status_is_error_ = false;
        last_status_message_ = "Cleared selection";
    }
    return hit;
}

core::Status EditorController::frameSelectionOrScene() {
    auto st = session_.frameSelectionOrScene();
    recordStatus(st, "Framed viewport camera");
    return st;
}

void EditorController::setRigAnimationPlaying(bool playing) noexcept {
    session_.setRigAnimationEnabled(playing);
    ++state_revision_;
    last_status_is_error_ = false;
    last_status_message_ = playing ? "Showcase rig animation resumed" : "Showcase rig animation paused";
}

core::Result<scene::MeshId> EditorController::ensurePresetMesh(ScenePreset preset) {
    auto& scene = session_.document().scene;

    std::string_view target_name;
    switch (preset) {
        case ScenePreset::Cube:
            target_name = "HeroCubeMesh";
            break;
        case ScenePreset::Pyramid:
            target_name = "AccentPyramidMesh";
            break;
        case ScenePreset::Plane:
            target_name = "StudioGroundMesh";
            break;
        default:
            return core::Result<scene::MeshId>::fail(core::ErrorCode::InvalidArgument, "Not a mesh preset");
    }

    for (const auto& existing : scene.meshes()) {
        if (existing.name == target_name) {
            return core::Result<scene::MeshId>::ok(existing.id);
        }
    }

    if (preset == ScenePreset::Cube) {
        return scene.registerMesh(
            std::string(target_name),
            scene::Primitives::createCube(1.25f, math::Vec3{0.86f, 0.48f, 0.22f}));
    }
    if (preset == ScenePreset::Pyramid) {
        return scene.registerMesh(
            std::string(target_name),
            scene::Primitives::createPyramid(0.95f, 1.25f, math::Vec3{0.24f, 0.68f, 0.88f}));
    }
    return scene.registerMesh(
        std::string(target_name),
        scene::Primitives::createCheckerPlane(4.0f, 4));
}

std::string EditorController::generateUniqueObjectName(std::string_view base_prefix) const {
    const auto& scene = session_.document().scene;
    if (scene.findObjectByName(base_prefix) == nullptr) {
        return std::string(base_prefix);
    }
    for (std::uint32_t idx = 1; idx < 10000; ++idx) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.*s.%03u", static_cast<int>(base_prefix.size()), base_prefix.data(), idx);
        if (scene.findObjectByName(buf) == nullptr) {
            return std::string(buf);
        }
    }
    return std::string(base_prefix) + "_copy";
}

core::Result<scene::ObjectId> EditorController::addPresetObject(
    ScenePreset preset,
    scene::ObjectId parent_id) {
    auto& scene = session_.document().scene;
    if (parent_id.isValid() && scene.findObject(parent_id) == nullptr) {
        return core::Result<scene::ObjectId>::fail(core::ErrorCode::NotFound, "Parent object not found");
    }

    std::string base_name = "Object";
    std::optional<scene::MeshId> mesh_id;
    std::optional<scene::CameraComponent> camera_comp;
    std::optional<scene::DirectionalLightComponent> light_comp;

    const float ring_angle = static_cast<float>(spawn_counter_++) * 1.15f;
    const float ring_radius = 1.35f;
    math::Transform init_tf = math::Transform::identity();
    init_tf.position = {
        std::cos(ring_angle) * ring_radius,
        0.75f,
        std::sin(ring_angle) * ring_radius,
    };

    switch (preset) {
        case ScenePreset::Cube: {
            base_name = "Cube";
            auto m = ensurePresetMesh(preset);
            if (!m) return m.error();
            mesh_id = m.value();
            break;
        }
        case ScenePreset::Pyramid: {
            base_name = "Pyramid";
            auto m = ensurePresetMesh(preset);
            if (!m) return m.error();
            mesh_id = m.value();
            break;
        }
        case ScenePreset::Plane: {
            base_name = "Plane";
            auto m = ensurePresetMesh(preset);
            if (!m) return m.error();
            mesh_id = m.value();
            init_tf.position = {0.0f, 0.0f, 0.0f};
            break;
        }
        case ScenePreset::EmptyPivot: {
            base_name = "Pivot";
            init_tf.position = {0.0f, 0.0f, 0.0f};
            break;
        }
        case ScenePreset::DirectionalLight: {
            base_name = "KeyLight";
            init_tf.position = {0.0f, 3.0f, 0.0f};
            init_tf.rotation = math::Quat::fromEulerXYZ(
                {math::degToRad(-45.0f), math::degToRad(-30.0f), 0.0f});
            light_comp = scene::DirectionalLightComponent{
                .color = {1.0f, 0.96f, 0.90f},
                .intensity = 1.1f,
            };
            break;
        }
        case ScenePreset::Camera: {
            base_name = "Camera";
            init_tf.position = {3.5f, 2.5f, 5.0f};
            init_tf.rotation = math::Quat::fromEulerXYZ(
                {math::degToRad(-22.0f), math::degToRad(32.0f), 0.0f});
            camera_comp = scene::CameraComponent{};
            break;
        }
    }

    const std::string unique_name = generateUniqueObjectName(base_name);
    auto create_cmd = std::make_unique<scene::CreateObjectCommand>(
        unique_name,
        parent_id,
        init_tf,
        mesh_id,
        camera_comp,
        light_comp);
    const auto* raw_cmd = create_cmd.get();

    auto st = session_.commandStack().execute(scene, std::move(create_cmd));
    if (!st) {
        recordStatus(st, {});
        return st.error();
    }

    const scene::ObjectId new_id = raw_cmd->createdObjectId();
    session_.setSelectedObject(new_id);
    recordStatus(core::Status::ok(), "Created '" + unique_name + "'");
    return core::Result<scene::ObjectId>::ok(new_id);
}

core::Result<scene::ObjectId> EditorController::duplicateSelectedObject() {
    const scene::ObjectId sel = session_.selectedObject();
    if (!sel.isValid()) {
        auto err = core::Status::fail(core::ErrorCode::InvalidState, "Select an object before duplicating");
        recordStatus(err, {});
        return err.error();
    }
    const auto* src = session_.document().scene.findObject(sel);
    if (src == nullptr) {
        auto err = core::Status::fail(core::ErrorCode::NotFound, "Selected object no longer exists");
        recordStatus(err, {});
        return err.error();
    }

    math::Transform dup_tf = src->local_transform;
    dup_tf.position += math::Vec3{0.65f, 0.0f, 0.65f};
    const std::string dup_name = generateUniqueObjectName(src->name);

    auto cmd = std::make_unique<scene::CreateObjectCommand>(
        dup_name,
        src->parent_id,
        dup_tf,
        src->mesh_id,
        src->camera,
        src->directional_light);
    const auto* raw = cmd.get();

    auto st = session_.commandStack().execute(session_.document().scene, std::move(cmd));
    if (!st) {
        recordStatus(st, {});
        return st.error();
    }

    const scene::ObjectId new_id = raw->createdObjectId();
    session_.setSelectedObject(new_id);
    recordStatus(core::Status::ok(), "Duplicated '" + dup_name + "'");
    return core::Result<scene::ObjectId>::ok(new_id);
}

core::Status EditorController::deleteSelectedObject(scene::RemovalMode mode) {
    const scene::ObjectId sel = session_.selectedObject();
    if (!sel.isValid()) {
        auto st = core::Status::fail(core::ErrorCode::InvalidState, "No object selected to delete");
        recordStatus(st, {});
        return st;
    }
    if (sel == session_.document().scene.activeCamera()) {
        auto st = core::Status::fail(
            core::ErrorCode::InvalidState,
            "Cannot delete the active viewport camera; switch active camera first");
        recordStatus(st, {});
        return st;
    }
    const auto* obj = session_.document().scene.findObject(sel);
    if (obj == nullptr) {
        session_.setSelectedObject({});
        auto st = core::Status::fail(core::ErrorCode::NotFound, "Selected object not found");
        recordStatus(st, {});
        return st;
    }

    const std::string deleted_name = obj->name;
    auto st = session_.commandStack().execute(
        session_.document().scene,
        std::make_unique<scene::DeleteObjectCommand>(sel, mode));
    if (st) {
        session_.setSelectedObject({});
    }
    recordStatus(st, "Deleted '" + deleted_name + "'");
    return st;
}

core::Status EditorController::renameObject(scene::ObjectId id, std::string new_name) {
    auto st = session_.commandStack().execute(
        session_.document().scene,
        std::make_unique<scene::RenameObjectCommand>(id, std::move(new_name)));
    recordStatus(st, "Renamed object");
    return st;
}

core::Status EditorController::setObjectVisibility(scene::ObjectId id, bool visible) {
    auto st = session_.commandStack().execute(
        session_.document().scene,
        std::make_unique<scene::SetVisibilityCommand>(id, visible));
    recordStatus(st, visible ? "Shown object" : "Hidden object");
    return st;
}

core::Status EditorController::reparentObject(scene::ObjectId child_id, scene::ObjectId new_parent_id) {
    auto st = session_.commandStack().execute(
        session_.document().scene,
        std::make_unique<scene::ReparentObjectCommand>(child_id, new_parent_id));
    recordStatus(st, "Updated object parent hierarchy");
    return st;
}

core::Status EditorController::setObjectTransformEulerDegrees(
    scene::ObjectId id,
    const math::Vec3& position,
    const math::Vec3& euler_degrees,
    const math::Vec3& scale,
    bool allow_coalesce) {
    math::Transform tf;
    tf.position = position;
    tf.rotation = math::Quat::fromEulerXYZ({
        math::degToRad(euler_degrees.x),
        math::degToRad(euler_degrees.y),
        math::degToRad(euler_degrees.z),
    });
    tf.scale = scale;

    auto st = session_.commandStack().execute(
        session_.document().scene,
        std::make_unique<scene::SetTransformCommand>(id, tf, allow_coalesce));
    if (st && id == session_.document().scene.activeCamera()) {
        session_.cameraController().syncFromCameraTransform(tf, session_.cameraController().target());
    }
    recordStatus(st, "Updated transform");
    return st;
}

core::Status EditorController::resetObjectTransform(scene::ObjectId id) {
    auto st = session_.commandStack().execute(
        session_.document().scene,
        std::make_unique<scene::SetTransformCommand>(id, math::Transform::identity(), false));
    recordStatus(st, "Reset transform to identity");
    return st;
}

core::Status EditorController::setCameraComponent(
    scene::ObjectId id,
    const scene::CameraComponent& camera) {
    auto st = session_.commandStack().execute(
        session_.document().scene,
        std::make_unique<scene::SetCameraComponentCommand>(id, camera));
    recordStatus(st, "Updated camera settings");
    return st;
}

core::Status EditorController::setActiveCamera(scene::ObjectId id) {
    auto st = session_.document().scene.setActiveCamera(id);
    if (st) {
        if (const auto* cam_obj = session_.document().scene.findObject(id); cam_obj != nullptr) {
            session_.cameraController().syncFromCameraTransform(
                cam_obj->local_transform,
                session_.cameraController().target());
        }
    }
    recordStatus(st, "Switched active viewport camera");
    return st;
}

core::Status EditorController::setDirectionalLightComponent(
    scene::ObjectId id,
    const scene::DirectionalLightComponent& light,
    bool allow_coalesce) {
    auto st = session_.commandStack().execute(
        session_.document().scene,
        std::make_unique<scene::SetDirectionalLightCommand>(id, light, allow_coalesce));
    recordStatus(st, "Updated directional light");
    return st;
}

core::Status EditorController::updateProjectMetadata(
    std::string project_name,
    std::string author,
    std::string scene_name) {
    if (project_name.empty() || scene_name.empty()) {
        auto st = core::Status::fail(
            core::ErrorCode::InvalidArgument,
            "Project name and scene name must be non-empty");
        recordStatus(st, {});
        return st;
    }
    session_.document().metadata.project_name = std::move(project_name);
    session_.document().metadata.author = std::move(author);
    session_.document().scene.setName(std::move(scene_name));
    recordStatus(core::Status::ok(), "Updated project metadata");
    return core::Status::ok();
}

core::Status EditorController::undo() {
    if (!session_.commandStack().canUndo()) {
        auto st = core::Status::fail(core::ErrorCode::InvalidState, "Nothing to undo");
        recordStatus(st, {});
        return st;
    }
    const std::string desc(session_.commandStack().nextUndoDescription());
    auto st = session_.commandStack().undo(session_.document().scene);
    if (st) {
        if (session_.selectedObject().isValid() &&
            session_.document().scene.findObject(session_.selectedObject()) == nullptr) {
            session_.setSelectedObject({});
        }
        const auto active_cam = session_.document().scene.activeCamera();
        if (active_cam.isValid()) {
            if (const auto* cam_obj = session_.document().scene.findObject(active_cam); cam_obj != nullptr) {
                session_.cameraController().syncFromCameraTransform(
                    cam_obj->local_transform,
                    session_.cameraController().target());
            }
        }
    }
    recordStatus(st, "Undid: " + desc);
    return st;
}

core::Status EditorController::redo() {
    if (!session_.commandStack().canRedo()) {
        auto st = core::Status::fail(core::ErrorCode::InvalidState, "Nothing to redo");
        recordStatus(st, {});
        return st;
    }
    const std::string desc(session_.commandStack().nextRedoDescription());
    auto st = session_.commandStack().redo(session_.document().scene);
    if (st) {
        if (session_.selectedObject().isValid() &&
            session_.document().scene.findObject(session_.selectedObject()) == nullptr) {
            session_.setSelectedObject({});
        }
        const auto active_cam = session_.document().scene.activeCamera();
        if (active_cam.isValid()) {
            if (const auto* cam_obj = session_.document().scene.findObject(active_cam); cam_obj != nullptr) {
                session_.cameraController().syncFromCameraTransform(
                    cam_obj->local_transform,
                    session_.cameraController().target());
            }
        }
    }
    recordStatus(st, "Redid: " + desc);
    return st;
}

core::Status EditorController::newStarterProject() {
    auto st = session_.resetStarterProject();
    spawn_counter_ = 0;
    recordStatus(st, "Reset to starter 3D showcase project");
    return st;
}

core::Result<std::string> EditorController::exportInlineProjectJson() const {
    return project::ProjectSerializer::serializeToString(session_.document(), true);
}

core::Status EditorController::importInlineProjectJson(std::string_view json_text) {
    auto loaded = project::ProjectSerializer::deserializeFromString(json_text);
    if (!loaded) {
        core::Status st = loaded.error();
        recordStatus(st, {});
        return st;
    }
    if (session_.renderer().isInitialized()) {
        session_.renderer().releaseSceneResources();
    }
    session_.document() = std::move(loaded).value();
    session_.commandStack().clear();
    session_.setSelectedObject({});
    session_.setRigAnimationEnabled(false);

    const auto active_cam = session_.document().scene.activeCamera();
    if (active_cam.isValid()) {
        if (const auto* cam_obj = session_.document().scene.findObject(active_cam); cam_obj != nullptr) {
            session_.cameraController().syncFromCameraTransform(cam_obj->local_transform, math::Vec3::zero());
        }
    }
    recordStatus(core::Status::ok(), "Loaded project '" + session_.document().metadata.project_name + "'");
    return core::Status::ok();
}

core::Status EditorController::saveBundleProject(
    const std::filesystem::path& fe3d_path,
    platform::IFileSystem& file_system) {
    auto st = session_.saveProjectBundleToFile(fe3d_path, file_system);
    recordStatus(st, "Exported .fe3d + .fe3dbin binary bundle");
    return st;
}

core::Status EditorController::loadProjectFile(
    const std::filesystem::path& fe3d_path,
    const platform::IFileSystem& file_system) {
    auto st = session_.loadProjectFromFile(fe3d_path, file_system);
    if (st) {
        session_.setRigAnimationEnabled(false);
    }
    recordStatus(st, "Loaded project '" + session_.document().metadata.project_name + "'");
    return st;
}

std::string EditorController::serializeStateJson() const {
    using project::JsonArray;
    using project::JsonObject;
    using project::JsonValue;

    const auto& doc = session_.document();
    const auto& scene = doc.scene;
    const auto& stack = session_.commandStack();
    const auto stats = session_.collectResourceStats();
    const auto& caps = session_.renderer().device().capabilities();

    JsonValue root{JsonObject{}};
    root.setKey("revision", state_revision_);

    JsonValue proj{JsonObject{}};
    proj.setKey("project_name", doc.metadata.project_name);
    proj.setKey("author", doc.metadata.author);
    proj.setKey("description", doc.metadata.description);
    proj.setKey("engine_version", doc.metadata.engine_version);
    proj.setKey("scene_name", scene.name());
    root.setKey("project", std::move(proj));

    JsonValue vp{JsonObject{}};
    vp.setKey("backend_name", caps.backend_name);
    vp.setKey("device_name", caps.device_name);
    vp.setKey("rig_playing", session_.isRigAnimationEnabled());
    vp.setKey("active_camera_id", scene.activeCamera().value);
    vp.setKey("selected_object_id", session_.selectedObject().value);
    vp.setKey("camera_target", vec3ToJson(session_.cameraController().target()));
    vp.setKey("camera_distance", static_cast<double>(session_.cameraController().distance()));
    vp.setKey("camera_yaw_deg", static_cast<double>(math::radToDeg(session_.cameraController().yawRadians())));
    vp.setKey("camera_pitch_deg", static_cast<double>(math::radToDeg(session_.cameraController().pitchRadians())));
    root.setKey("viewport", std::move(vp));

    JsonValue hist{JsonObject{}};
    hist.setKey("can_undo", stack.canUndo());
    hist.setKey("can_redo", stack.canRedo());
    hist.setKey("undo_count", static_cast<std::uint64_t>(stack.undoCount()));
    hist.setKey("redo_count", static_cast<std::uint64_t>(stack.redoCount()));
    hist.setKey("next_undo", std::string(stack.nextUndoDescription()));
    hist.setKey("next_redo", std::string(stack.nextRedoDescription()));
    hist.setKey("memory_bytes", static_cast<std::uint64_t>(stack.totalMemoryBytes()));
    root.setKey("history", std::move(hist));

    JsonValue st_json{JsonObject{}};
    st_json.setKey("object_count", static_cast<std::uint64_t>(stats.scene_object_count));
    st_json.setKey("mesh_count", static_cast<std::uint64_t>(stats.mesh_asset_count));
    st_json.setKey("total_vertices", static_cast<std::uint64_t>(stats.total_cpu_vertices));
    st_json.setKey("total_triangles", static_cast<std::uint64_t>(stats.total_cpu_indices / 3u));
    st_json.setKey("cpu_mesh_bytes", static_cast<std::uint64_t>(stats.cpu_mesh_bytes));
    st_json.setKey("gpu_buffer_count", static_cast<std::uint64_t>(stats.gpu_mesh_buffer_count));
    st_json.setKey("gpu_mesh_bytes", static_cast<std::uint64_t>(stats.gpu_mesh_bytes));
    root.setKey("stats", std::move(st_json));

    JsonValue status_obj{JsonObject{}};
    status_obj.setKey("level", last_status_is_error_ ? "error" : "info");
    status_obj.setKey("message", last_status_message_);
    root.setKey("status", std::move(status_obj));

    // Build deterministic hierarchical Outliner tree in pre-order (sorted by ObjectId among siblings).
    std::vector<const scene::SceneObject*> roots;
    for (const auto& obj : scene.objects()) {
        if (!obj.parent_id.isValid() || scene.findObject(obj.parent_id) == nullptr) {
            roots.push_back(&obj);
        }
    }
    std::sort(roots.begin(), roots.end(), [](const auto* a, const auto* b) { return a->id < b->id; });

    JsonArray outliner_arr;
    outliner_arr.reserve(scene.objectCount());

    auto appendSubtree = [&](auto& self, const scene::SceneObject* node, std::uint32_t depth) -> void {
        JsonValue item{JsonObject{}};
        item.setKey("id", node->id.value);
        item.setKey("name", node->name);
        item.setKey("visible", node->visible);
        item.setKey("parent_id", node->parent_id.value);
        item.setKey("depth", static_cast<std::uint64_t>(depth));
        item.setKey("has_mesh", node->mesh_id.has_value());
        item.setKey("has_camera", node->camera.has_value());
        item.setKey("has_light", node->directional_light.has_value());
        item.setKey("is_active_camera", node->id == scene.activeCamera());
        outliner_arr.push_back(std::move(item));

        std::vector<const scene::SceneObject*> kids;
        kids.reserve(node->children.size());
        for (const auto cid : node->children) {
            if (const auto* child_ptr = scene.findObject(cid); child_ptr != nullptr) {
                kids.push_back(child_ptr);
            }
        }
        std::sort(kids.begin(), kids.end(), [](const auto* a, const auto* b) { return a->id < b->id; });
        for (const auto* child_ptr : kids) {
            self(self, child_ptr, depth + 1u);
        }
    };

    for (const auto* r : roots) {
        appendSubtree(appendSubtree, r, 0u);
    }
    root.setKey("objects", std::move(outliner_arr));

    // Populate selected object Inspector payload when valid.
    if (session_.selectedObject().isValid()) {
        if (const auto* sel_obj = scene.findObject(session_.selectedObject()); sel_obj != nullptr) {
            JsonValue insp{JsonObject{}};
            insp.setKey("id", sel_obj->id.value);
            insp.setKey("name", sel_obj->name);
            insp.setKey("visible", sel_obj->visible);
            insp.setKey("parent_id", sel_obj->parent_id.value);
            insp.setKey("position", vec3ToJson(sel_obj->local_transform.position));

            const math::Vec3 euler_rad = sel_obj->local_transform.rotation.toEulerXYZ();
            insp.setKey("rotation_deg", vec3ToJson(math::Vec3{
                math::radToDeg(euler_rad.x),
                math::radToDeg(euler_rad.y),
                math::radToDeg(euler_rad.z),
            }));
            insp.setKey("scale", vec3ToJson(sel_obj->local_transform.scale));

            if (sel_obj->mesh_id.has_value()) {
                if (const auto* mesh_asset = scene.findMesh(sel_obj->mesh_id.value()); mesh_asset != nullptr) {
                    JsonValue m_json{JsonObject{}};
                    m_json.setKey("id", mesh_asset->id.value);
                    m_json.setKey("name", mesh_asset->name);
                    m_json.setKey("vertex_count", static_cast<std::uint64_t>(mesh_asset->data.vertices.size()));
                    m_json.setKey(
                        "triangle_count",
                        static_cast<std::uint64_t>(mesh_asset->data.indices.size() / 3u));
                    m_json.setKey("bounds_size", vec3ToJson(mesh_asset->local_bounds.size()));
                    insp.setKey("mesh", std::move(m_json));
                }
            }

            if (sel_obj->camera.has_value()) {
                const auto& cam = sel_obj->camera.value();
                JsonValue c_json{JsonObject{}};
                c_json.setKey(
                    "projection",
                    cam.projection == scene::ProjectionType::Perspective ? "perspective" : "orthographic");
                c_json.setKey("fov_deg", static_cast<double>(math::radToDeg(cam.fov_y_radians)));
                c_json.setKey("ortho_height", static_cast<double>(cam.orthographic_height));
                c_json.setKey("near_plane", static_cast<double>(cam.near_plane));
                c_json.setKey("far_plane", static_cast<double>(cam.far_plane));
                c_json.setKey("is_active", sel_obj->id == scene.activeCamera());
                insp.setKey("camera", std::move(c_json));
            }

            if (sel_obj->directional_light.has_value()) {
                const auto& lgt = sel_obj->directional_light.value();
                JsonValue l_json{JsonObject{}};
                l_json.setKey("color", vec3ToJson(lgt.color));
                l_json.setKey("intensity", static_cast<double>(lgt.intensity));
                insp.setKey("light", std::move(l_json));
            }

            root.setKey("inspector", std::move(insp));
        } else {
            root.setKey("inspector", JsonValue{});
        }
    } else {
        root.setKey("inspector", JsonValue{});
    }

    auto serialized = project::writeJson(root, false);
    return serialized.hasValue() ? serialized.value() : "{}";
}

core::Status EditorController::executeActionJson(std::string_view action_json) {
    auto parsed = project::parseJson(action_json);
    if (!parsed) {
        recordStatus(core::Status(parsed.error()), {});
        return parsed.error();
    }
    const auto& cmd = parsed.value();
    if (!cmd.isObject()) {
        auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Editor action JSON must be an object");
        recordStatus(st, {});
        return st;
    }

    const auto* action_val = cmd.findKey("action");
    if (action_val == nullptr || !action_val->isString()) {
        auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Missing string 'action' key");
        recordStatus(st, {});
        return st;
    }
    const std::string& action = action_val->asString();

    if (action == "select_object") {
        auto id_res = parseJsonId(cmd.findKey("id"), "id");
        if (!id_res) {
            recordStatus(core::Status(id_res.error()), {});
            return id_res.error();
        }
        return selectObject(scene::ObjectId{id_res.value()});
    }

    if (action == "add_preset") {
        const auto* preset_val = cmd.findKey("preset");
        if (preset_val == nullptr || !preset_val->isString()) {
            auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Missing string 'preset'");
            recordStatus(st, {});
            return st;
        }
        ScenePreset preset = ScenePreset::Cube;
        const std::string& p_str = preset_val->asString();
        if (p_str == "cube") preset = ScenePreset::Cube;
        else if (p_str == "pyramid") preset = ScenePreset::Pyramid;
        else if (p_str == "plane") preset = ScenePreset::Plane;
        else if (p_str == "pivot") preset = ScenePreset::EmptyPivot;
        else if (p_str == "light") preset = ScenePreset::DirectionalLight;
        else if (p_str == "camera") preset = ScenePreset::Camera;
        else {
            auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Unknown preset: " + p_str);
            recordStatus(st, {});
            return st;
        }

        scene::ObjectId parent_id{};
        if (const auto* pv = cmd.findKey("parent_id"); pv != nullptr && pv->isNumber()) {
            auto pid = parseJsonId(pv, "parent_id");
            if (!pid) {
                recordStatus(core::Status(pid.error()), {});
                return pid.error();
            }
            parent_id = scene::ObjectId{pid.value()};
        }
        auto res = addPresetObject(preset, parent_id);
        return res.hasValue() ? core::Status::ok() : core::Status(res.error());
    }

    if (action == "duplicate_selected") {
        auto res = duplicateSelectedObject();
        return res.hasValue() ? core::Status::ok() : core::Status(res.error());
    }

    if (action == "delete_selected") {
        return deleteSelectedObject(scene::RemovalMode::Recursive);
    }

    if (action == "rename_object") {
        auto id_res = parseJsonId(cmd.findKey("id"), "id");
        if (!id_res) {
            recordStatus(core::Status(id_res.error()), {});
            return id_res.error();
        }
        const auto* nv = cmd.findKey("name");
        if (nv == nullptr || !nv->isString()) {
            auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Missing string 'name'");
            recordStatus(st, {});
            return st;
        }
        return renameObject(scene::ObjectId{id_res.value()}, nv->asString());
    }

    if (action == "set_visibility") {
        auto id_res = parseJsonId(cmd.findKey("id"), "id");
        if (!id_res) {
            recordStatus(core::Status(id_res.error()), {});
            return id_res.error();
        }
        const auto* vv = cmd.findKey("visible");
        if (vv == nullptr || !vv->isBool()) {
            auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Missing boolean 'visible'");
            recordStatus(st, {});
            return st;
        }
        return setObjectVisibility(scene::ObjectId{id_res.value()}, vv->asBool());
    }

    if (action == "reparent_object") {
        auto cid = parseJsonId(cmd.findKey("child_id"), "child_id");
        auto pid = parseJsonId(cmd.findKey("parent_id"), "parent_id");
        if (!cid) {
            recordStatus(core::Status(cid.error()), {});
            return cid.error();
        }
        if (!pid) {
            recordStatus(core::Status(pid.error()), {});
            return pid.error();
        }
        return reparentObject(scene::ObjectId{cid.value()}, scene::ObjectId{pid.value()});
    }

    if (action == "set_transform") {
        auto id_res = parseJsonId(cmd.findKey("id"), "id");
        auto pos_res = parseJsonVec3(cmd.findKey("position"), "position");
        auto rot_res = parseJsonVec3(cmd.findKey("rotation_deg"), "rotation_deg");
        auto scl_res = parseJsonVec3(cmd.findKey("scale"), "scale");
        if (!id_res) { recordStatus(core::Status(id_res.error()), {}); return id_res.error(); }
        if (!pos_res) { recordStatus(core::Status(pos_res.error()), {}); return pos_res.error(); }
        if (!rot_res) { recordStatus(core::Status(rot_res.error()), {}); return rot_res.error(); }
        if (!scl_res) { recordStatus(core::Status(scl_res.error()), {}); return scl_res.error(); }

        bool coalesce = false;
        if (const auto* cv = cmd.findKey("coalesce"); cv != nullptr && cv->isBool()) {
            coalesce = cv->asBool();
        }
        return setObjectTransformEulerDegrees(
            scene::ObjectId{id_res.value()},
            pos_res.value(),
            rot_res.value(),
            scl_res.value(),
            coalesce);
    }

    if (action == "reset_transform") {
        auto id_res = parseJsonId(cmd.findKey("id"), "id");
        if (!id_res) {
            recordStatus(core::Status(id_res.error()), {});
            return id_res.error();
        }
        return resetObjectTransform(scene::ObjectId{id_res.value()});
    }

    if (action == "set_camera") {
        auto id_res = parseJsonId(cmd.findKey("id"), "id");
        auto fov_res = parseJsonFloat(cmd.findKey("fov_deg"), "fov_deg");
        auto oh_res = parseJsonFloat(cmd.findKey("ortho_height"), "ortho_height");
        auto np_res = parseJsonFloat(cmd.findKey("near_plane"), "near_plane");
        auto fp_res = parseJsonFloat(cmd.findKey("far_plane"), "far_plane");
        if (!id_res) { recordStatus(core::Status(id_res.error()), {}); return id_res.error(); }
        if (!fov_res) { recordStatus(core::Status(fov_res.error()), {}); return fov_res.error(); }
        if (!oh_res) { recordStatus(core::Status(oh_res.error()), {}); return oh_res.error(); }
        if (!np_res) { recordStatus(core::Status(np_res.error()), {}); return np_res.error(); }
        if (!fp_res) { recordStatus(core::Status(fp_res.error()), {}); return fp_res.error(); }

        scene::CameraComponent cam{};
        if (const auto* pv = cmd.findKey("projection"); pv != nullptr && pv->isString()) {
            cam.projection = (pv->asString() == "orthographic")
                                 ? scene::ProjectionType::Orthographic
                                 : scene::ProjectionType::Perspective;
        }
        cam.fov_y_radians = math::degToRad(fov_res.value());
        cam.orthographic_height = oh_res.value();
        cam.near_plane = np_res.value();
        cam.far_plane = fp_res.value();
        return setCameraComponent(scene::ObjectId{id_res.value()}, cam);
    }

    if (action == "set_active_camera") {
        auto id_res = parseJsonId(cmd.findKey("id"), "id");
        if (!id_res) {
            recordStatus(core::Status(id_res.error()), {});
            return id_res.error();
        }
        return setActiveCamera(scene::ObjectId{id_res.value()});
    }

    if (action == "set_light") {
        auto id_res = parseJsonId(cmd.findKey("id"), "id");
        auto col_res = parseJsonVec3(cmd.findKey("color"), "color");
        auto int_res = parseJsonFloat(cmd.findKey("intensity"), "intensity");
        if (!id_res) { recordStatus(core::Status(id_res.error()), {}); return id_res.error(); }
        if (!col_res) { recordStatus(core::Status(col_res.error()), {}); return col_res.error(); }
        if (!int_res) { recordStatus(core::Status(int_res.error()), {}); return int_res.error(); }

        bool coalesce = false;
        if (const auto* cv = cmd.findKey("coalesce"); cv != nullptr && cv->isBool()) {
            coalesce = cv->asBool();
        }
        scene::DirectionalLightComponent lgt{
            .color = col_res.value(),
            .intensity = int_res.value(),
        };
        return setDirectionalLightComponent(scene::ObjectId{id_res.value()}, lgt, coalesce);
    }

    if (action == "set_metadata") {
        const auto* pn = cmd.findKey("project_name");
        const auto* au = cmd.findKey("author");
        const auto* sn = cmd.findKey("scene_name");
        if (pn == nullptr || !pn->isString() || au == nullptr || !au->isString() || sn == nullptr || !sn->isString()) {
            auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Metadata fields must be strings");
            recordStatus(st, {});
            return st;
        }
        return updateProjectMetadata(pn->asString(), au->asString(), sn->asString());
    }

    if (action == "frame_selection") {
        return frameSelectionOrScene();
    }

    if (action == "set_rig_playing") {
        const auto* pv = cmd.findKey("playing");
        if (pv == nullptr || !pv->isBool()) {
            auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Missing boolean 'playing'");
            recordStatus(st, {});
            return st;
        }
        setRigAnimationPlaying(pv->asBool());
        return core::Status::ok();
    }

    if (action == "undo") {
        return undo();
    }

    if (action == "redo") {
        return redo();
    }

    if (action == "new_starter_project") {
        return newStarterProject();
    }

    if (action == "load_inline_json") {
        const auto* jv = cmd.findKey("json_text");
        if (jv == nullptr || !jv->isString()) {
            auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Missing string 'json_text'");
            recordStatus(st, {});
            return st;
        }
        return importInlineProjectJson(jv->asString());
    }

    auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Unknown editor action: " + action);
    recordStatus(st, {});
    return st;
}

} // namespace finaleffect::app
