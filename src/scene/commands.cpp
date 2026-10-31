// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/scene/commands.hpp"

#include <algorithm>
#include <utility>

namespace finaleffect::scene {

// -----------------------------------------------------------------------------
// SetTransformCommand
// -----------------------------------------------------------------------------
SetTransformCommand::SetTransformCommand(
    ObjectId object_id,
    const math::Transform& new_transform,
    bool allow_coalesce,
    std::uint64_t coalesce_group)
    : object_id_(object_id),
      new_transform_(new_transform),
      allow_coalesce_(allow_coalesce),
      coalesce_group_(coalesce_group) {}

core::Status SetTransformCommand::execute(Scene& scene) {
    const auto* obj = scene.findObject(object_id_);
    if (obj == nullptr) {
        return core::Status::fail(core::ErrorCode::NotFound, "SetTransformCommand target object not found");
    }
    if (!captured_previous_) {
        previous_transform_ = obj->local_transform;
        captured_previous_ = true;
    }
    return scene.setObjectTransform(object_id_, new_transform_);
}

core::Status SetTransformCommand::undo(Scene& scene) {
    if (!captured_previous_) {
        return core::Status::fail(core::ErrorCode::InvalidState, "SetTransformCommand was never executed");
    }
    return scene.setObjectTransform(object_id_, previous_transform_);
}

bool SetTransformCommand::tryCoalesceWith(const ISceneCommand& incoming) noexcept {
    if (!allow_coalesce_) {
        return false;
    }
    const auto* next_tf = dynamic_cast<const SetTransformCommand*>(&incoming);
    if (next_tf == nullptr || !next_tf->allow_coalesce_ ||
        next_tf->object_id_ != object_id_ ||
        next_tf->coalesce_group_ != coalesce_group_) {
        return false;
    }
    new_transform_ = next_tf->new_transform_;
    return true;
}

// -----------------------------------------------------------------------------
// CreateObjectCommand
// -----------------------------------------------------------------------------
CreateObjectCommand::CreateObjectCommand(
    std::string object_name,
    ObjectId parent_id,
    math::Transform initial_transform,
    std::optional<MeshId> mesh_id,
    std::optional<CameraComponent> camera,
    std::optional<DirectionalLightComponent> directional_light,
    std::optional<MaterialId> material_id)
    : object_name_(std::move(object_name)),
      parent_id_(parent_id),
      initial_transform_(initial_transform),
      mesh_id_(mesh_id),
      camera_(camera),
      light_component_(directional_light.has_value()
                           ? AnyLightComponent{*directional_light}
                           : AnyLightComponent{std::monostate{}}),
      material_id_(material_id) {}

CreateObjectCommand::CreateObjectCommand(
    std::string object_name,
    ObjectId parent_id,
    math::Transform initial_transform,
    AnyLightComponent light_component)
    : object_name_(std::move(object_name)),
      parent_id_(parent_id),
      initial_transform_(initial_transform),
      light_component_(std::move(light_component)) {}

std::size_t CreateObjectCommand::estimatedBytes() const noexcept {
    return sizeof(*this) + object_name_.capacity();
}

core::Status CreateObjectCommand::execute(Scene& scene) {
    if (!initial_transform_.isValid()) {
        return core::Status::fail(core::ErrorCode::ValidationError, "CreateObjectCommand initial transform is invalid");
    }
    if (mesh_id_.has_value() && scene.findMesh(mesh_id_.value()) == nullptr) {
        return core::Status::fail(core::ErrorCode::NotFound, "CreateObjectCommand referenced unknown MeshId");
    }

    if (!created_id_.isValid()) {
        auto created = scene.createObject(object_name_, parent_id_);
        if (!created) {
            return created.error();
        }
        created_id_ = created.value();
    } else {
        auto st = scene.createObjectWithId(created_id_, object_name_, parent_id_);
        if (!st) {
            return st;
        }
    }

    auto tf_st = scene.setObjectTransform(created_id_, initial_transform_);
    if (!tf_st) {
        (void)scene.removeObject(created_id_, RemovalMode::Recursive);
        return tf_st;
    }

    if (mesh_id_.has_value()) {
        auto m_st = scene.assignMesh(created_id_, mesh_id_);
        if (!m_st) {
            (void)scene.removeObject(created_id_, RemovalMode::Recursive);
            return m_st;
        }
    }

    if (material_id_.has_value()) {
        auto mat_st = scene.assignMaterial(created_id_, material_id_);
        if (!mat_st) {
            (void)scene.removeObject(created_id_, RemovalMode::Recursive);
            return mat_st;
        }
    }

    if (camera_.has_value()) {
        auto c_st = scene.assignCamera(created_id_, camera_);
        if (!c_st) {
            (void)scene.removeObject(created_id_, RemovalMode::Recursive);
            return c_st;
        }
        if (!scene.activeCamera().isValid()) {
            (void)scene.setActiveCamera(created_id_);
        }
    }

    if (!std::holds_alternative<std::monostate>(light_component_)) {
        auto l_st = scene.assignAnyLight(created_id_, light_component_);
        if (!l_st) {
            (void)scene.removeObject(created_id_, RemovalMode::Recursive);
            return l_st;
        }
    }
    return core::Status::ok();
}

core::Status CreateObjectCommand::undo(Scene& scene) {
    if (!created_id_.isValid()) {
        return core::Status::fail(core::ErrorCode::InvalidState, "CreateObjectCommand was never executed");
    }
    return scene.removeObject(created_id_, RemovalMode::Recursive);
}

// -----------------------------------------------------------------------------
// DeleteObjectCommand
// -----------------------------------------------------------------------------
DeleteObjectCommand::DeleteObjectCommand(ObjectId target_id, RemovalMode mode)
    : target_id_(target_id), mode_(mode) {}

std::size_t DeleteObjectCommand::estimatedBytes() const noexcept {
    std::size_t bytes = sizeof(*this) + reparented_children_.capacity() * sizeof(ObjectId);
    for (const auto& snap : deleted_snapshots_) {
        bytes += sizeof(ObjectSnapshot) + snap.name.capacity();
    }
    return bytes;
}

core::Status DeleteObjectCommand::execute(Scene& scene) {
    const auto* target = scene.findObject(target_id_);
    if (target == nullptr) {
        return core::Status::fail(core::ErrorCode::NotFound, "DeleteObjectCommand target object not found");
    }

    deleted_snapshots_.clear();
    reparented_children_.clear();
    removed_light_links_.clear();
    previous_active_camera_ = scene.activeCamera();

    auto makeSnapshot = [](const SceneObject& obj) -> ObjectSnapshot {
        return ObjectSnapshot{
            .id = obj.id,
            .name = obj.name,
            .visible = obj.visible,
            .local_transform = obj.local_transform,
            .parent_id = obj.parent_id,
            .mesh_id = obj.mesh_id,
            .material_id = obj.material_id,
            .camera = obj.camera,
            .light = obj.anyLightComponent(),
        };
    };

    if (mode_ == RemovalMode::ReparentChildren) {
        reparented_children_ = target->children;
        deleted_snapshots_.push_back(makeSnapshot(*target));
    } else {
        // Pre-order BFS/DFS snapshot so parents always precede children during undo restoration.
        std::vector<ObjectId> stack{target_id_};
        while (!stack.empty()) {
            const ObjectId curr_id = stack.back();
            stack.pop_back();
            const auto* node = scene.findObject(curr_id);
            if (node == nullptr) {
                continue;
            }
            deleted_snapshots_.push_back(makeSnapshot(*node));
            for (auto it = node->children.rbegin(); it != node->children.rend(); ++it) {
                stack.push_back(*it);
            }
        }
    }

    // Capture any LightLinkingSettings references pointing to soon-to-be-deleted objects so Undo restores them.
    for (const auto& obj : scene.objects()) {
        const auto* link = obj.lightLinking();
        if (link == nullptr || link->linked_objects.empty()) {
            continue;
        }
        for (const auto& snap : deleted_snapshots_) {
            if (link->containsDirect(snap.id)) {
                removed_light_links_.push_back(RemovedLightLinkRef{
                    .light_object_id = obj.id,
                    .linked_target_id = snap.id,
                });
            }
        }
    }

    return scene.removeObject(target_id_, mode_);
}

core::Status DeleteObjectCommand::undo(Scene& scene) {
    if (deleted_snapshots_.empty()) {
        return core::Status::fail(core::ErrorCode::InvalidState, "DeleteObjectCommand has no captured snapshot");
    }

    for (const auto& snap : deleted_snapshots_) {
        auto st = scene.createObjectWithId(snap.id, snap.name, snap.parent_id);
        if (!st) return st;

        if (auto vis_st = scene.setObjectVisible(snap.id, snap.visible); !vis_st) return vis_st;
        if (auto tf_st = scene.setObjectTransform(snap.id, snap.local_transform); !tf_st) return tf_st;
        if (snap.mesh_id.has_value()) {
            if (auto m_st = scene.assignMesh(snap.id, snap.mesh_id); !m_st) return m_st;
        }
        if (snap.material_id.has_value() && scene.findMaterial(snap.material_id.value()) != nullptr) {
            if (auto mat_st = scene.assignMaterial(snap.id, snap.material_id); !mat_st) return mat_st;
        }
        if (snap.camera.has_value()) {
            if (auto c_st = scene.assignCamera(snap.id, snap.camera); !c_st) return c_st;
        }
        if (!std::holds_alternative<std::monostate>(snap.light)) {
            if (auto l_st = scene.assignAnyLight(snap.id, snap.light); !l_st) return l_st;
        }
    }

    // Restore external light-linking references that pointed to the restored objects.
    for (const auto& ref : removed_light_links_) {
        const auto* light_obj = scene.findObject(ref.light_object_id);
        if (light_obj == nullptr || scene.findObject(ref.linked_target_id) == nullptr) {
            continue;
        }
        AnyLightComponent updated = light_obj->anyLightComponent();
        std::visit(
            [&](auto& l) {
                using T = std::decay_t<decltype(l)>;
                if constexpr (!std::is_same_v<T, std::monostate>) {
                    l.linking.linked_objects.push_back(ref.linked_target_id);
                    l.linking.normalize();
                }
            },
            updated);
        (void)scene.assignAnyLight(ref.light_object_id, updated);
    }

    if (mode_ == RemovalMode::ReparentChildren) {
        for (const ObjectId child_id : reparented_children_) {
            if (scene.findObject(child_id) != nullptr) {
                auto st = scene.setParent(child_id, target_id_);
                if (!st) return st;
            }
        }
    }

    if (previous_active_camera_.isValid() && scene.findObject(previous_active_camera_) != nullptr) {
        auto cam_st = scene.setActiveCamera(previous_active_camera_);
        if (!cam_st) return cam_st;
    }

    return core::Status::ok();
}

// -----------------------------------------------------------------------------
// ReparentObjectCommand
// -----------------------------------------------------------------------------
ReparentObjectCommand::ReparentObjectCommand(ObjectId child_id, ObjectId new_parent_id)
    : child_id_(child_id), new_parent_id_(new_parent_id) {}

core::Status ReparentObjectCommand::execute(Scene& scene) {
    const auto* child = scene.findObject(child_id_);
    if (child == nullptr) {
        return core::Status::fail(core::ErrorCode::NotFound, "ReparentObjectCommand child object not found");
    }
    if (!captured_previous_) {
        previous_parent_id_ = child->parent_id;
        captured_previous_ = true;
    }
    return scene.setParent(child_id_, new_parent_id_);
}

core::Status ReparentObjectCommand::undo(Scene& scene) {
    if (!captured_previous_) {
        return core::Status::fail(core::ErrorCode::InvalidState, "ReparentObjectCommand was never executed");
    }
    return scene.setParent(child_id_, previous_parent_id_);
}

// -----------------------------------------------------------------------------
// RenameObjectCommand
// -----------------------------------------------------------------------------
RenameObjectCommand::RenameObjectCommand(ObjectId object_id, std::string new_name)
    : object_id_(object_id), new_name_(std::move(new_name)) {}

std::size_t RenameObjectCommand::estimatedBytes() const noexcept {
    return sizeof(*this) + new_name_.capacity() + previous_name_.capacity();
}

core::Status RenameObjectCommand::execute(Scene& scene) {
    const auto* obj = scene.findObject(object_id_);
    if (obj == nullptr) {
        return core::Status::fail(core::ErrorCode::NotFound, "RenameObjectCommand target object not found");
    }
    if (!captured_previous_) {
        previous_name_ = obj->name;
        captured_previous_ = true;
    }
    return scene.setObjectName(object_id_, new_name_);
}

core::Status RenameObjectCommand::undo(Scene& scene) {
    if (!captured_previous_) {
        return core::Status::fail(core::ErrorCode::InvalidState, "RenameObjectCommand was never executed");
    }
    return scene.setObjectName(object_id_, previous_name_);
}

// -----------------------------------------------------------------------------
// SetVisibilityCommand
// -----------------------------------------------------------------------------
SetVisibilityCommand::SetVisibilityCommand(ObjectId object_id, bool visible)
    : object_id_(object_id), new_visible_(visible) {}

core::Status SetVisibilityCommand::execute(Scene& scene) {
    const auto* obj = scene.findObject(object_id_);
    if (obj == nullptr) {
        return core::Status::fail(core::ErrorCode::NotFound, "SetVisibilityCommand target object not found");
    }
    if (!captured_previous_) {
        previous_visible_ = obj->visible;
        captured_previous_ = true;
    }
    return scene.setObjectVisible(object_id_, new_visible_);
}

core::Status SetVisibilityCommand::undo(Scene& scene) {
    if (!captured_previous_) {
        return core::Status::fail(core::ErrorCode::InvalidState, "SetVisibilityCommand was never executed");
    }
    return scene.setObjectVisible(object_id_, previous_visible_);
}

// -----------------------------------------------------------------------------
// SetCameraComponentCommand
// -----------------------------------------------------------------------------
SetCameraComponentCommand::SetCameraComponentCommand(
    ObjectId object_id,
    std::optional<CameraComponent> camera)
    : object_id_(object_id), new_camera_(camera) {}

core::Status SetCameraComponentCommand::execute(Scene& scene) {
    const auto* obj = scene.findObject(object_id_);
    if (obj == nullptr) {
        return core::Status::fail(core::ErrorCode::NotFound, "SetCameraComponentCommand target object not found");
    }
    if (!captured_previous_) {
        previous_camera_ = obj->camera;
        previous_active_camera_ = scene.activeCamera();
        captured_previous_ = true;
    }
    return scene.assignCamera(object_id_, new_camera_);
}

core::Status SetCameraComponentCommand::undo(Scene& scene) {
    if (!captured_previous_) {
        return core::Status::fail(core::ErrorCode::InvalidState, "SetCameraComponentCommand was never executed");
    }
    auto st = scene.assignCamera(object_id_, previous_camera_);
    if (!st) {
        return st;
    }
    if (previous_active_camera_.isValid() && scene.findObject(previous_active_camera_) != nullptr) {
        return scene.setActiveCamera(previous_active_camera_);
    }
    return core::Status::ok();
}

// -----------------------------------------------------------------------------
// SetDirectionalLightCommand
// -----------------------------------------------------------------------------
SetDirectionalLightCommand::SetDirectionalLightCommand(
    ObjectId object_id,
    std::optional<DirectionalLightComponent> light,
    bool allow_coalesce)
    : object_id_(object_id), new_light_(light), allow_coalesce_(allow_coalesce) {}

core::Status SetDirectionalLightCommand::execute(Scene& scene) {
    const auto* obj = scene.findObject(object_id_);
    if (obj == nullptr) {
        return core::Status::fail(core::ErrorCode::NotFound, "SetDirectionalLightCommand target object not found");
    }
    if (!captured_previous_) {
        previous_light_ = obj->directional_light;
        captured_previous_ = true;
    }
    return scene.assignDirectionalLight(object_id_, new_light_);
}

core::Status SetDirectionalLightCommand::undo(Scene& scene) {
    if (!captured_previous_) {
        return core::Status::fail(core::ErrorCode::InvalidState, "SetDirectionalLightCommand was never executed");
    }
    return scene.assignDirectionalLight(object_id_, previous_light_);
}

bool SetDirectionalLightCommand::tryCoalesceWith(const ISceneCommand& incoming) noexcept {
    if (!allow_coalesce_) {
        return false;
    }
    const auto* next_light = dynamic_cast<const SetDirectionalLightCommand*>(&incoming);
    if (next_light == nullptr || !next_light->allow_coalesce_ || next_light->object_id_ != object_id_) {
        return false;
    }
    new_light_ = next_light->new_light_;
    return true;
}

// -----------------------------------------------------------------------------
// SetLightComponentCommand
// -----------------------------------------------------------------------------
SetLightComponentCommand::SetLightComponentCommand(
    ObjectId object_id,
    AnyLightComponent light,
    bool allow_coalesce,
    std::string label)
    : object_id_(object_id),
      new_light_(std::move(light)),
      label_(label.empty() ? "Edit Light" : std::move(label)),
      allow_coalesce_(allow_coalesce) {}

std::size_t SetLightComponentCommand::estimatedBytes() const noexcept {
    return sizeof(*this) + label_.capacity();
}

core::Status SetLightComponentCommand::execute(Scene& scene) {
    const auto* obj = scene.findObject(object_id_);
    if (obj == nullptr) {
        return core::Status::fail(core::ErrorCode::NotFound, "SetLightComponentCommand target object not found");
    }
    if (!captured_previous_) {
        previous_light_ = obj->anyLightComponent();
        captured_previous_ = true;
    }
    return scene.assignAnyLight(object_id_, new_light_);
}

core::Status SetLightComponentCommand::undo(Scene& scene) {
    if (!captured_previous_) {
        return core::Status::fail(core::ErrorCode::InvalidState, "SetLightComponentCommand was never executed");
    }
    return scene.assignAnyLight(object_id_, previous_light_);
}

bool SetLightComponentCommand::tryCoalesceWith(const ISceneCommand& incoming) noexcept {
    if (!allow_coalesce_) {
        return false;
    }
    const auto* next_cmd = dynamic_cast<const SetLightComponentCommand*>(&incoming);
    if (next_cmd == nullptr || !next_cmd->allow_coalesce_ ||
        next_cmd->object_id_ != object_id_ ||
        next_cmd->new_light_.index() != new_light_.index()) {
        return false;
    }
    new_light_ = next_cmd->new_light_;
    return true;
}

// -----------------------------------------------------------------------------
// SetEnvironmentLightingCommand
// -----------------------------------------------------------------------------
SetEnvironmentLightingCommand::SetEnvironmentLightingCommand(
    EnvironmentLighting new_environment,
    bool allow_coalesce,
    std::string label)
    : new_environment_(std::move(new_environment)),
      label_(label.empty() ? "Edit Environment" : std::move(label)),
      allow_coalesce_(allow_coalesce) {}

std::size_t SetEnvironmentLightingCommand::estimatedBytes() const noexcept {
    return sizeof(*this) + label_.capacity() +
           new_environment_.source_path.capacity() +
           previous_environment_.source_path.capacity();
}

core::Status SetEnvironmentLightingCommand::execute(Scene& scene) {
    if (!captured_previous_) {
        previous_environment_ = scene.environment();
        captured_previous_ = true;
    }
    return scene.setEnvironment(new_environment_);
}

core::Status SetEnvironmentLightingCommand::undo(Scene& scene) {
    if (!captured_previous_) {
        return core::Status::fail(
            core::ErrorCode::InvalidState,
            "SetEnvironmentLightingCommand was never executed");
    }
    return scene.setEnvironment(previous_environment_);
}

bool SetEnvironmentLightingCommand::tryCoalesceWith(const ISceneCommand& incoming) noexcept {
    if (!allow_coalesce_) {
        return false;
    }
    const auto* next_cmd = dynamic_cast<const SetEnvironmentLightingCommand*>(&incoming);
    if (next_cmd == nullptr || !next_cmd->allow_coalesce_ ||
        next_cmd->new_environment_.hdri_asset != new_environment_.hdri_asset) {
        return false;
    }
    new_environment_ = next_cmd->new_environment_;
    return true;
}

// -----------------------------------------------------------------------------
// ModifyMeshCommand
// -----------------------------------------------------------------------------
ModifyMeshCommand::ModifyMeshCommand(
    MeshId mesh_id,
    EditableMesh new_mesh,
    std::string label,
    bool allow_coalesce,
    std::uint64_t coalesce_group)
    : mesh_id_(mesh_id),
      new_mesh_(std::move(new_mesh)),
      label_(label.empty() ? "Edit Mesh" : std::move(label)),
      allow_coalesce_(allow_coalesce),
      coalesce_group_(coalesce_group) {}

std::size_t ModifyMeshCommand::estimatedBytes() const noexcept {
    const std::size_t new_b = new_mesh_.vertexCount() * sizeof(EditVertex) +
                              new_mesh_.faceCount() * sizeof(EditFace);
    const std::size_t old_b = previous_mesh_.vertexCount() * sizeof(EditVertex) +
                              previous_mesh_.faceCount() * sizeof(EditFace);
    return sizeof(*this) + label_.capacity() + new_b + old_b;
}

core::Status ModifyMeshCommand::execute(Scene& scene) {
    auto ensure_st = scene.ensureEditableMesh(mesh_id_);
    if (!ensure_st) {
        return ensure_st;
    }
    const auto* asset = scene.findMesh(mesh_id_);
    if (asset == nullptr || !asset->editable_mesh.has_value()) {
        return core::Status::fail(core::ErrorCode::NotFound, "ModifyMeshCommand target editable mesh not found");
    }
    if (!captured_previous_) {
        previous_mesh_ = asset->editable_mesh.value();
        captured_previous_ = true;
    }
    return scene.updateMeshEditable(mesh_id_, new_mesh_);
}

core::Status ModifyMeshCommand::undo(Scene& scene) {
    if (!captured_previous_) {
        return core::Status::fail(core::ErrorCode::InvalidState, "ModifyMeshCommand was never executed");
    }
    return scene.updateMeshEditable(mesh_id_, previous_mesh_);
}

bool ModifyMeshCommand::tryCoalesceWith(const ISceneCommand& incoming) noexcept {
    if (!allow_coalesce_) {
        return false;
    }
    const auto* next_m = dynamic_cast<const ModifyMeshCommand*>(&incoming);
    if (next_m == nullptr || !next_m->allow_coalesce_ ||
        next_m->mesh_id_ != mesh_id_ ||
        next_m->coalesce_group_ != coalesce_group_) {
        return false;
    }
    new_mesh_ = next_m->new_mesh_;
    return true;
}

// -----------------------------------------------------------------------------
// CreateMaterialCommand
// -----------------------------------------------------------------------------
CreateMaterialCommand::CreateMaterialCommand(PbrMaterial material, std::string label)
    : material_(std::move(material)),
      label_(label.empty() ? "Create Material" : std::move(label)) {}

std::size_t CreateMaterialCommand::estimatedBytes() const noexcept {
    return sizeof(*this) + material_.name.capacity() + label_.capacity();
}

core::Status CreateMaterialCommand::execute(Scene& scene) {
    if (!created_id_.isValid()) {
        auto res = scene.createMaterial(material_);
        if (!res) {
            return res.error();
        }
        created_id_ = res.value();
        return core::Status::ok();
    }
    return scene.createMaterialWithId(created_id_, material_);
}

core::Status CreateMaterialCommand::undo(Scene& scene) {
    if (!created_id_.isValid()) {
        return core::Status::fail(core::ErrorCode::InvalidState, "CreateMaterialCommand was never executed");
    }
    return scene.removeMaterial(created_id_);
}

// -----------------------------------------------------------------------------
// UpdateMaterialCommand
// -----------------------------------------------------------------------------
UpdateMaterialCommand::UpdateMaterialCommand(
    MaterialId material_id,
    PbrMaterial updated_material,
    bool allow_coalesce,
    std::uint64_t coalesce_group)
    : material_id_(material_id),
      updated_material_(std::move(updated_material)),
      allow_coalesce_(allow_coalesce),
      coalesce_group_(coalesce_group) {}

std::size_t UpdateMaterialCommand::estimatedBytes() const noexcept {
    return sizeof(*this) + updated_material_.name.capacity() + previous_material_.name.capacity();
}

core::Status UpdateMaterialCommand::execute(Scene& scene) {
    const auto* existing = scene.findMaterial(material_id_);
    if (existing == nullptr) {
        return core::Status::fail(core::ErrorCode::NotFound, "UpdateMaterialCommand target material not found");
    }
    if (!captured_previous_) {
        previous_material_ = *existing;
        captured_previous_ = true;
    }
    return scene.updateMaterial(material_id_, updated_material_);
}

core::Status UpdateMaterialCommand::undo(Scene& scene) {
    if (!captured_previous_) {
        return core::Status::fail(core::ErrorCode::InvalidState, "UpdateMaterialCommand was never executed");
    }
    return scene.updateMaterial(material_id_, previous_material_);
}

bool UpdateMaterialCommand::tryCoalesceWith(const ISceneCommand& incoming) noexcept {
    if (!allow_coalesce_) {
        return false;
    }
    const auto* next_mat = dynamic_cast<const UpdateMaterialCommand*>(&incoming);
    if (next_mat == nullptr || !next_mat->allow_coalesce_ ||
        next_mat->material_id_ != material_id_ ||
        next_mat->coalesce_group_ != coalesce_group_) {
        return false;
    }
    updated_material_ = next_mat->updated_material_;
    return true;
}

// -----------------------------------------------------------------------------
// DeleteMaterialCommand
// -----------------------------------------------------------------------------
DeleteMaterialCommand::DeleteMaterialCommand(MaterialId material_id)
    : material_id_(material_id) {}

std::size_t DeleteMaterialCommand::estimatedBytes() const noexcept {
    return sizeof(*this) + snapshot_.name.capacity() +
           affected_objects_.capacity() * sizeof(ObjectId) +
           affected_faces_.capacity() * sizeof(FaceMaterialRef);
}

core::Status DeleteMaterialCommand::execute(Scene& scene) {
    const auto* mat = scene.findMaterial(material_id_);
    if (mat == nullptr) {
        return core::Status::fail(core::ErrorCode::NotFound, "DeleteMaterialCommand target material not found");
    }

    snapshot_ = *mat;
    affected_objects_.clear();
    affected_faces_.clear();

    for (const auto& obj : scene.objects()) {
        if (obj.material_id.has_value() && obj.material_id.value() == material_id_) {
            affected_objects_.push_back(obj.id);
        }
    }
    for (const auto& mesh : scene.meshes()) {
        if (mesh.editable_mesh.has_value()) {
            const auto& faces = mesh.editable_mesh->faces();
            for (std::size_t fi = 0; fi < faces.size(); ++fi) {
                if (faces[fi].material_id == material_id_) {
                    affected_faces_.push_back(FaceMaterialRef{
                        .mesh_id = mesh.id,
                        .face_index = static_cast<std::uint32_t>(fi),
                    });
                }
            }
        }
    }
    captured_ = true;
    return scene.removeMaterial(material_id_);
}

core::Status DeleteMaterialCommand::undo(Scene& scene) {
    if (!captured_) {
        return core::Status::fail(core::ErrorCode::InvalidState, "DeleteMaterialCommand was never executed");
    }
    auto st = scene.createMaterialWithId(material_id_, snapshot_);
    if (!st) {
        return st;
    }
    for (const ObjectId obj_id : affected_objects_) {
        if (scene.findObject(obj_id) != nullptr) {
            (void)scene.assignMaterial(obj_id, material_id_);
        }
    }
    // Restore per-face material assignments grouped by MeshId.
    for (const auto& ref : affected_faces_) {
        const auto* mesh_asset = scene.findMesh(ref.mesh_id);
        if (mesh_asset != nullptr && mesh_asset->editable_mesh.has_value()) {
            EditableMesh restored = mesh_asset->editable_mesh.value();
            const std::uint32_t f_idx = ref.face_index;
            if (restored.assignMaterialToFaces(std::span<const std::uint32_t>(&f_idx, 1), material_id_).hasValue()) {
                (void)scene.updateMeshEditable(ref.mesh_id, std::move(restored));
            }
        }
    }
    return core::Status::ok();
}

// -----------------------------------------------------------------------------
// AssignObjectMaterialCommand
// -----------------------------------------------------------------------------
AssignObjectMaterialCommand::AssignObjectMaterialCommand(
    ObjectId object_id,
    std::optional<MaterialId> material_id)
    : object_id_(object_id), new_material_id_(material_id) {}

core::Status AssignObjectMaterialCommand::execute(Scene& scene) {
    const auto* obj = scene.findObject(object_id_);
    if (obj == nullptr) {
        return core::Status::fail(core::ErrorCode::NotFound, "AssignObjectMaterialCommand target object not found");
    }
    if (!captured_previous_) {
        previous_material_id_ = obj->material_id;
        captured_previous_ = true;
    }
    return scene.assignMaterial(object_id_, new_material_id_);
}

core::Status AssignObjectMaterialCommand::undo(Scene& scene) {
    if (!captured_previous_) {
        return core::Status::fail(core::ErrorCode::InvalidState, "AssignObjectMaterialCommand was never executed");
    }
    if (previous_material_id_.has_value() && scene.findMaterial(previous_material_id_.value()) == nullptr) {
        return scene.assignMaterial(object_id_, std::nullopt);
    }
    return scene.assignMaterial(object_id_, previous_material_id_);
}

// -----------------------------------------------------------------------------
// CompoundTransactionCommand
// -----------------------------------------------------------------------------
CompoundTransactionCommand::CompoundTransactionCommand(
    std::string label,
    std::vector<std::unique_ptr<ISceneCommand>> steps)
    : label_(label.empty() ? "Transaction" : std::move(label)),
      steps_(std::move(steps)) {}

void CompoundTransactionCommand::addStep(std::unique_ptr<ISceneCommand> step) {
    if (step) {
        steps_.push_back(std::move(step));
    }
}

std::size_t CompoundTransactionCommand::estimatedBytes() const noexcept {
    std::size_t total = sizeof(*this) + label_.capacity();
    for (const auto& step : steps_) {
        total += step ? step->estimatedBytes() : 0u;
    }
    return total;
}

core::Status CompoundTransactionCommand::execute(Scene& scene) {
    if (steps_.empty()) {
        return core::Status::fail(core::ErrorCode::InvalidArgument, "CompoundTransactionCommand contains no steps");
    }

    for (std::size_t i = 0; i < steps_.size(); ++i) {
        auto st = steps_[i]->execute(scene);
        if (!st) {
            // Roll back already-applied steps in reverse order.
            for (std::size_t rev = i; rev > 0; --rev) {
                (void)steps_[rev - 1]->undo(scene);
            }
            return st;
        }
    }
    return core::Status::ok();
}

core::Status CompoundTransactionCommand::undo(Scene& scene) {
    for (std::size_t rev = steps_.size(); rev > 0; --rev) {
        auto st = steps_[rev - 1]->undo(scene);
        if (!st) {
            return st;
        }
    }
    return core::Status::ok();
}

// -----------------------------------------------------------------------------
// CommandStack
// -----------------------------------------------------------------------------
CommandStack::CommandStack(CommandStackLimits limits) : limits_(limits) {
    if (limits_.max_entries == 0) {
        limits_.max_entries = 1;
    }
    if (limits_.max_memory_bytes == 0) {
        limits_.max_memory_bytes = 4096;
    }
}

core::Status CommandStack::execute(Scene& scene, std::unique_ptr<ISceneCommand> command) {
    if (!command) {
        return core::Status::fail(core::ErrorCode::InvalidArgument, "Cannot execute null ISceneCommand");
    }

    auto st = command->execute(scene);
    if (!st) {
        return st;
    }

    // Any new committed edit invalidates the redo branch.
    redo_stack_.clear();
    redo_bytes_ = 0;

    if (!undo_stack_.empty() && undo_stack_.back()->tryCoalesceWith(*command)) {
        enforceLimits();
        return core::Status::ok();
    }

    const std::size_t cmd_bytes = command->estimatedBytes();
    undo_bytes_ += cmd_bytes;
    undo_stack_.push_back(std::move(command));
    enforceLimits();
    return core::Status::ok();
}

core::Status CommandStack::undo(Scene& scene) {
    if (undo_stack_.empty()) {
        return core::Status::fail(core::ErrorCode::InvalidState, "Nothing to undo");
    }

    auto cmd = std::move(undo_stack_.back());
    undo_stack_.pop_back();
    const std::size_t cmd_bytes = cmd->estimatedBytes();
    undo_bytes_ = (undo_bytes_ >= cmd_bytes) ? (undo_bytes_ - cmd_bytes) : 0u;

    auto st = cmd->undo(scene);
    if (!st) {
        return st;
    }

    redo_bytes_ += cmd_bytes;
    redo_stack_.push_back(std::move(cmd));
    return core::Status::ok();
}

core::Status CommandStack::redo(Scene& scene) {
    if (redo_stack_.empty()) {
        return core::Status::fail(core::ErrorCode::InvalidState, "Nothing to redo");
    }

    auto cmd = std::move(redo_stack_.back());
    redo_stack_.pop_back();
    const std::size_t cmd_bytes = cmd->estimatedBytes();
    redo_bytes_ = (redo_bytes_ >= cmd_bytes) ? (redo_bytes_ - cmd_bytes) : 0u;

    auto st = cmd->execute(scene);
    if (!st) {
        return st;
    }

    undo_bytes_ += cmd_bytes;
    undo_stack_.push_back(std::move(cmd));
    enforceLimits();
    return core::Status::ok();
}

void CommandStack::clear() noexcept {
    undo_stack_.clear();
    redo_stack_.clear();
    undo_bytes_ = 0;
    redo_bytes_ = 0;
}

std::string_view CommandStack::nextUndoDescription() const noexcept {
    return undo_stack_.empty() ? std::string_view{} : undo_stack_.back()->description();
}

std::string_view CommandStack::nextRedoDescription() const noexcept {
    return redo_stack_.empty() ? std::string_view{} : redo_stack_.back()->description();
}

void CommandStack::enforceLimits() noexcept {
    while (undo_stack_.size() > limits_.max_entries ||
           (undo_stack_.size() > 1 && undo_bytes_ > limits_.max_memory_bytes)) {
        const std::size_t front_bytes = undo_stack_.front()->estimatedBytes();
        undo_bytes_ = (undo_bytes_ >= front_bytes) ? (undo_bytes_ - front_bytes) : 0u;
        undo_stack_.pop_front();
    }
}

} // namespace finaleffect::scene
