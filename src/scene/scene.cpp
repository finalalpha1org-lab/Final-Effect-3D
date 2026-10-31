// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/scene/scene.hpp"
#include "finaleffect/math/constants.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace finaleffect::scene {

Scene::Scene(std::string scene_name)
    : name_(scene_name.empty() ? "Untitled Scene" : std::move(scene_name)) {}

void Scene::setName(std::string scene_name) {
    if (!scene_name.empty()) {
        name_ = std::move(scene_name);
    }
}

void Scene::clear() {
    meshes_.clear();
    mesh_index_.clear();
    materials_.clear();
    material_index_.clear();
    objects_.clear();
    object_index_.clear();
    active_camera_id_ = ObjectId::invalid();
    environment_ = EnvironmentLighting{};
    display_layers_.clear();
    timeline_ = TimelineSettings{};
    animation_tracks_.clear();
    next_object_id_ = 1;
    next_mesh_id_ = 1;
    next_material_id_ = 1;
}

core::Result<MeshId> Scene::registerMesh(std::string mesh_name, MeshData data) {
    const MeshId id{next_mesh_id_};
    auto status = registerMeshWithId(id, std::move(mesh_name), std::move(data));
    if (!status) {
        return status.error();
    }
    return core::Result<MeshId>::ok(id);
}

core::Status Scene::registerMeshWithId(MeshId id, std::string mesh_name, MeshData data) {
    if (!id.isValid()) {
        return core::Status::fail(core::ErrorCode::InvalidArgument, "MeshId must be non-zero");
    }
    if (mesh_name.empty()) {
        return core::Status::fail(core::ErrorCode::InvalidArgument, "Mesh name must not be empty");
    }
    if (mesh_index_.contains(id)) {
        return core::Status::fail(
            core::ErrorCode::AlreadyExists,
            "Duplicate MeshId registration",
            std::to_string(id.value));
    }

    auto valid = data.validate();
    if (!valid) {
        return valid;
    }

    const math::AABB bounds = data.computeBounds();
    const std::size_t index = meshes_.size();
    meshes_.push_back(MeshAsset{
        .id = id,
        .name = std::move(mesh_name),
        .data = std::move(data),
        .local_bounds = bounds,
        .revision = 1,
        .editable_mesh = std::nullopt,
    });
    mesh_index_.emplace(id, index);
    if (id.value >= next_mesh_id_) {
        next_mesh_id_ = id.value + 1;
    }
    return core::Status::ok();
}

core::Result<MeshId> Scene::registerEditableMesh(std::string mesh_name, EditableMesh editable) {
    const MeshId id{next_mesh_id_};
    auto status = registerEditableMeshWithId(id, std::move(mesh_name), std::move(editable));
    if (!status) {
        return status.error();
    }
    return core::Result<MeshId>::ok(id);
}

core::Status Scene::registerEditableMeshWithId(MeshId id, std::string mesh_name, EditableMesh editable) {
    if (!id.isValid()) {
        return core::Status::fail(core::ErrorCode::InvalidArgument, "MeshId must be non-zero");
    }
    if (mesh_name.empty()) {
        return core::Status::fail(core::ErrorCode::InvalidArgument, "Mesh name must not be empty");
    }
    if (mesh_index_.contains(id)) {
        return core::Status::fail(
            core::ErrorCode::AlreadyExists,
            "Duplicate MeshId registration",
            std::to_string(id.value));
    }

    auto valid_edit = editable.validateAndRebuild();
    if (!valid_edit) {
        return valid_edit;
    }
    for (const auto& f : editable.faces()) {
        if (f.material_id.isValid() && findMaterial(f.material_id) == nullptr) {
            return core::Status::fail(
                core::ErrorCode::NotFound,
                "Polygon face references unregistered MaterialId",
                std::to_string(f.material_id.value));
        }
    }
    MeshData triangulated = editable.toMeshData();
    auto valid_tri = triangulated.validate();
    if (!valid_tri) {
        return valid_tri;
    }

    const math::AABB bounds = triangulated.computeBounds();
    const std::size_t index = meshes_.size();
    meshes_.push_back(MeshAsset{
        .id = id,
        .name = std::move(mesh_name),
        .data = std::move(triangulated),
        .local_bounds = bounds,
        .revision = 1,
        .editable_mesh = std::move(editable),
    });
    mesh_index_.emplace(id, index);
    if (id.value >= next_mesh_id_) {
        next_mesh_id_ = id.value + 1;
    }
    return core::Status::ok();
}

core::Status Scene::ensureEditableMesh(MeshId id) {
    auto* asset = findMeshMutable(id);
    if (asset == nullptr) {
        return core::Status::fail(core::ErrorCode::NotFound, "MeshId not found");
    }
    if (asset->editable_mesh.has_value()) {
        return core::Status::ok();
    }
    auto conv = EditableMesh::fromMeshData(asset->data);
    if (!conv) {
        return conv.error();
    }
    asset->editable_mesh = std::move(conv.value());
    asset->data = asset->editable_mesh->toMeshData();
    asset->local_bounds = asset->data.computeBounds();
    ++asset->revision;
    return core::Status::ok();
}

core::Status Scene::updateMeshEditable(MeshId id, EditableMesh updated_editable) {
    auto* asset = findMeshMutable(id);
    if (asset == nullptr) {
        return core::Status::fail(core::ErrorCode::NotFound, "MeshId not found");
    }
    auto valid = updated_editable.validateAndRebuild();
    if (!valid) {
        return valid;
    }
    // Validate per-face MaterialId references against registered scene materials.
    for (const auto& f : updated_editable.faces()) {
        if (f.material_id.isValid() && findMaterial(f.material_id) == nullptr) {
            return core::Status::fail(
                core::ErrorCode::NotFound,
                "Polygon face references unregistered MaterialId",
                std::to_string(f.material_id.value));
        }
    }

    MeshData triangulated = updated_editable.toMeshData();
    auto tri_valid = triangulated.validate();
    if (!tri_valid) {
        return tri_valid;
    }

    asset->data = std::move(triangulated);
    asset->local_bounds = asset->data.computeBounds();
    asset->editable_mesh = std::move(updated_editable);
    ++asset->revision;
    return core::Status::ok();
}

const MeshAsset* Scene::findMesh(MeshId id) const noexcept {
    const auto it = mesh_index_.find(id);
    if (it == mesh_index_.end()) {
        return nullptr;
    }
    return &meshes_[it->second];
}

MeshAsset* Scene::findMeshMutable(MeshId id) noexcept {
    const auto it = mesh_index_.find(id);
    if (it == mesh_index_.end()) {
        return nullptr;
    }
    return &meshes_[it->second];
}

core::Result<MaterialId> Scene::createMaterial(PbrMaterial material) {
    const MaterialId id{next_material_id_};
    auto st = createMaterialWithId(id, std::move(material));
    if (!st) {
        return st.error();
    }
    return core::Result<MaterialId>::ok(id);
}

core::Status Scene::createMaterialWithId(MaterialId id, PbrMaterial material) {
    if (!id.isValid()) {
        return core::Status::fail(core::ErrorCode::InvalidArgument, "MaterialId must be non-zero");
    }
    if (material_index_.contains(id)) {
        return core::Status::fail(
            core::ErrorCode::AlreadyExists,
            "Duplicate MaterialId registration",
            std::to_string(id.value));
    }
    material.id = id;
    auto valid = material.validate();
    if (!valid) {
        return valid;
    }
    if (material.revision == 0) {
        material.revision = 1;
    }

    const std::size_t idx = materials_.size();
    materials_.push_back(std::move(material));
    material_index_.emplace(id, idx);
    if (id.value >= next_material_id_) {
        next_material_id_ = id.value + 1;
    }
    return core::Status::ok();
}

const PbrMaterial* Scene::findMaterial(MaterialId id) const noexcept {
    const auto it = material_index_.find(id);
    if (it == material_index_.end()) {
        return nullptr;
    }
    return &materials_[it->second];
}

PbrMaterial* Scene::findMaterialMutable(MaterialId id) noexcept {
    const auto it = material_index_.find(id);
    if (it == material_index_.end()) {
        return nullptr;
    }
    return &materials_[it->second];
}

const PbrMaterial* Scene::findMaterialByName(std::string_view material_name) const noexcept {
    for (const auto& mat : materials_) {
        if (mat.name == material_name) {
            return &mat;
        }
    }
    return nullptr;
}

core::Status Scene::updateMaterial(MaterialId id, const PbrMaterial& updated) {
    auto* mat = findMaterialMutable(id);
    if (mat == nullptr) {
        return core::Status::fail(core::ErrorCode::NotFound, "Material not found");
    }
    PbrMaterial candidate = updated;
    candidate.id = id;
    auto valid = candidate.validate();
    if (!valid) {
        return valid;
    }
    candidate.revision = mat->revision + 1;
    *mat = std::move(candidate);
    return core::Status::ok();
}

core::Status Scene::renameMaterial(MaterialId id, std::string new_name) {
    if (new_name.empty()) {
        return core::Status::fail(core::ErrorCode::InvalidArgument, "Material name must not be empty");
    }
    auto* mat = findMaterialMutable(id);
    if (mat == nullptr) {
        return core::Status::fail(core::ErrorCode::NotFound, "Material not found");
    }
    mat->name = std::move(new_name);
    ++mat->revision;
    return core::Status::ok();
}

core::Result<MaterialId> Scene::duplicateMaterial(MaterialId source_id, std::string duplicate_name) {
    const auto* src = findMaterial(source_id);
    if (src == nullptr) {
        return core::Result<MaterialId>::fail(core::ErrorCode::NotFound, "Source material not found");
    }
    PbrMaterial copy = *src;
    copy.name = duplicate_name.empty() ? (src->name + " Copy") : std::move(duplicate_name);
    copy.revision = 1;
    return createMaterial(std::move(copy));
}

core::Status Scene::removeMaterial(MaterialId id) {
    const auto it = material_index_.find(id);
    if (it == material_index_.end()) {
        return core::Status::fail(core::ErrorCode::NotFound, "Cannot remove non-existent material");
    }

    // Safely detach all object-level and per-face references so no dangling MaterialId remains.
    for (auto& obj : objects_) {
        if (obj.material_id.has_value() && obj.material_id.value() == id) {
            obj.material_id = std::nullopt;
        }
    }
    for (auto& mesh : meshes_) {
        if (mesh.editable_mesh.has_value()) {
            bool had_ref = false;
            for (const auto& f : mesh.editable_mesh->faces()) {
                if (f.material_id == id) {
                    had_ref = true;
                    break;
                }
            }
            if (had_ref) {
                mesh.editable_mesh->clearMaterialReferences(id);
                mesh.data = mesh.editable_mesh->toMeshData();
                ++mesh.revision;
            }
        } else {
            bool had_submesh_ref = false;
            for (auto& sm : mesh.data.submeshes) {
                if (sm.material_id == id) {
                    sm.material_id = MaterialId::invalid();
                    had_submesh_ref = true;
                }
            }
            if (had_submesh_ref) {
                ++mesh.revision;
            }
        }
    }

    const std::size_t remove_idx = it->second;
    const std::size_t last_idx = materials_.size() - 1;
    if (remove_idx != last_idx) {
        materials_[remove_idx] = std::move(materials_[last_idx]);
        material_index_[materials_[remove_idx].id] = remove_idx;
    }
    materials_.pop_back();
    material_index_.erase(it);
    return core::Status::ok();
}

core::Result<ObjectId> Scene::createObject(std::string object_name, ObjectId parent_id) {
    const ObjectId id{next_object_id_};
    auto status = createObjectWithId(id, std::move(object_name), parent_id);
    if (!status) {
        return status.error();
    }
    return core::Result<ObjectId>::ok(id);
}

core::Status Scene::createObjectWithId(ObjectId id, std::string object_name, ObjectId parent_id) {
    if (!id.isValid()) {
        return core::Status::fail(core::ErrorCode::InvalidArgument, "ObjectId must be non-zero");
    }
    if (object_name.empty()) {
        return core::Status::fail(core::ErrorCode::InvalidArgument, "Object name must not be empty");
    }
    if (object_index_.contains(id)) {
        return core::Status::fail(
            core::ErrorCode::AlreadyExists,
            "Duplicate ObjectId in scene",
            std::to_string(id.value));
    }
    if (parent_id.isValid() && !object_index_.contains(parent_id)) {
        return core::Status::fail(
            core::ErrorCode::NotFound,
            "Parent object does not exist in scene",
            std::to_string(parent_id.value));
    }

    const std::size_t idx = objects_.size();
    objects_.push_back(SceneObject{
        .id = id,
        .name = std::move(object_name),
        .visible = true,
        .local_transform = math::Transform::identity(),
        .parent_id = parent_id,
        .children = {},
        .mesh_id = std::nullopt,
        .material_id = std::nullopt,
        .camera = std::nullopt,
        .directional_light = std::nullopt,
        .point_light = std::nullopt,
        .spot_light = std::nullopt,
        .area_light = std::nullopt,
    });
    object_index_.emplace(id, idx);

    if (parent_id.isValid()) {
        auto* parent_obj = findObjectMutable(parent_id);
        parent_obj->children.push_back(id);
    }

    if (id.value >= next_object_id_) {
        next_object_id_ = id.value + 1;
    }
    return core::Status::ok();
}

const SceneObject* Scene::findObject(ObjectId id) const noexcept {
    const auto it = object_index_.find(id);
    if (it == object_index_.end()) {
        return nullptr;
    }
    return &objects_[it->second];
}

SceneObject* Scene::findObjectMutable(ObjectId id) noexcept {
    const auto it = object_index_.find(id);
    if (it == object_index_.end()) {
        return nullptr;
    }
    return &objects_[it->second];
}

const SceneObject* Scene::findObjectByName(std::string_view object_name) const noexcept {
    for (const auto& obj : objects_) {
        if (obj.name == object_name) {
            return &obj;
        }
    }
    return nullptr;
}

bool Scene::wouldCreateCycle(ObjectId child_id, ObjectId candidate_parent_id) const noexcept {
    ObjectId cursor = candidate_parent_id;
    std::size_t guard = 0;
    const std::size_t max_steps = objects_.size() + 1;
    while (cursor.isValid() && guard < max_steps) {
        if (cursor == child_id) {
            return true;
        }
        const auto* node = findObject(cursor);
        if (node == nullptr) {
            break;
        }
        cursor = node->parent_id;
        ++guard;
    }
    return false;
}

void Scene::detachFromParent(ObjectId child_id) noexcept {
    auto* child = findObjectMutable(child_id);
    if (child == nullptr || !child->parent_id.isValid()) {
        return;
    }
    if (auto* old_parent = findObjectMutable(child->parent_id)) {
        auto& siblings = old_parent->children;
        siblings.erase(std::remove(siblings.begin(), siblings.end(), child_id), siblings.end());
    }
    child->parent_id = ObjectId::invalid();
}

void Scene::eraseSingleObject(ObjectId id) noexcept {
    const auto it = object_index_.find(id);
    if (it == object_index_.end()) {
        return;
    }

    if (active_camera_id_ == id) {
        active_camera_id_ = ObjectId::invalid();
    }

    for (auto& remaining : objects_) {
        if (auto* link = remaining.lightLinkingMutable(); link != nullptr) {
            link->removeObjectReference(id);
        }
    }

    const std::size_t remove_idx = it->second;
    const std::size_t last_idx = objects_.size() - 1;
    if (remove_idx != last_idx) {
        objects_[remove_idx] = std::move(objects_[last_idx]);
        object_index_[objects_[remove_idx].id] = remove_idx;
    }
    objects_.pop_back();
    object_index_.erase(it);

    for (auto& layer : display_layers_) {
        std::erase(layer.members, id);
    }
    std::erase_if(animation_tracks_, [id](const ObjectAnimationTrack& tr) { return tr.object_id == id; });
}

core::Status Scene::setParent(ObjectId child_id, ObjectId new_parent_id) {
    if ( findObject(child_id) == nullptr) {
        return core::Status::fail(core::ErrorCode::NotFound, "Child object not found");
    }
    if (new_parent_id.isValid() && findObject(new_parent_id) == nullptr) {
        return core::Status::fail(core::ErrorCode::NotFound, "New parent object not found");
    }
    if (wouldCreateCycle(child_id, new_parent_id)) {
        return core::Status::fail(
            core::ErrorCode::HierarchyCycle,
            "Reparenting would introduce a cycle in the scene hierarchy");
    }

    const auto* current_child = findObject(child_id);
    if (current_child->parent_id == new_parent_id) {
        return core::Status::ok();
    }

    detachFromParent(child_id);
    if (new_parent_id.isValid()) {
        auto* child_mut = findObjectMutable(child_id);
        auto* parent_mut = findObjectMutable(new_parent_id);
        child_mut->parent_id = new_parent_id;
        parent_mut->children.push_back(child_id);
    }
    return core::Status::ok();
}

core::Status Scene::removeObject(ObjectId id, RemovalMode mode) {
    auto* target = findObjectMutable(id);
    if (target == nullptr) {
        return core::Status::fail(core::ErrorCode::NotFound, "Cannot remove non-existent object");
    }

    if (mode == RemovalMode::ReparentChildren) {
        const ObjectId grandparent = target->parent_id;
        const std::vector<ObjectId> children_copy = target->children;
        for (const ObjectId child_id : children_copy) {
            auto status = setParent(child_id, grandparent);
            if (!status) {
                return status;
            }
        }
        detachFromParent(id);
        eraseSingleObject(id);
        return core::Status::ok();
    }

    // Recursive removal: gather full subtree in post-order using explicit stack (no unbounded recursion).
    detachFromParent(id);
    std::vector<ObjectId> to_visit{id};
    std::vector<ObjectId> subtree;
    while (!to_visit.empty()) {
        const ObjectId current = to_visit.back();
        to_visit.pop_back();
        subtree.push_back(current);
        if (const auto* node = findObject(current)) {
            for (const ObjectId ch : node->children) {
                to_visit.push_back(ch);
            }
        }
    }

    for (const ObjectId dead_id : subtree) {
        eraseSingleObject(dead_id);
    }
    return core::Status::ok();
}

core::Status Scene::setObjectTransform(ObjectId id, const math::Transform& local_transform) {
    auto* obj = findObjectMutable(id);
    if (obj == nullptr) {
        return core::Status::fail(core::ErrorCode::NotFound, "Scene object not found");
    }
    if (!local_transform.isValid()) {
        return core::Status::fail(
            core::ErrorCode::ValidationError,
            "Transform contains non-finite values or zero-length quaternion");
    }
    obj->local_transform = local_transform;
    return core::Status::ok();
}

core::Status Scene::setObjectVisible(ObjectId id, bool visible) {
    auto* obj = findObjectMutable(id);
    if (obj == nullptr) {
        return core::Status::fail(core::ErrorCode::NotFound, "Scene object not found");
    }
    obj->visible = visible;
    return core::Status::ok();
}

core::Status Scene::setObjectName(ObjectId id, std::string new_name) {
    if (new_name.empty()) {
        return core::Status::fail(core::ErrorCode::InvalidArgument, "Scene object name must not be empty");
    }
    auto* obj = findObjectMutable(id);
    if (obj == nullptr) {
        return core::Status::fail(core::ErrorCode::NotFound, "Scene object not found");
    }
    obj->name = std::move(new_name);
    return core::Status::ok();
}

core::Status Scene::setObjectLocked(ObjectId id, bool locked) {
    auto* obj = findObjectMutable(id);
    if (obj == nullptr) {
        return core::Status::fail(core::ErrorCode::NotFound, "Scene object not found");
    }
    obj->locked = locked;
    return core::Status::ok();
}

core::Status Scene::setObjectChannelLocks(ObjectId id, std::uint16_t mask) {
    auto* obj = findObjectMutable(id);
    if (obj == nullptr) {
        return core::Status::fail(core::ErrorCode::NotFound, "Scene object not found");
    }
    obj->locked_channels_mask = mask;
    return core::Status::ok();
}

core::Status Scene::setObjectPivotOffset(ObjectId id, const math::Vec3& pivot_offset) {
    if (!pivot_offset.isFinite()) {
        return core::Status::fail(core::ErrorCode::ValidationError, "Pivot offset must be finite");
    }
    auto* obj = findObjectMutable(id);
    if (obj == nullptr) {
        return core::Status::fail(core::ErrorCode::NotFound, "Scene object not found");
    }
    obj->pivot_offset = pivot_offset;
    return core::Status::ok();
}

core::Status Scene::setObjectCustomAttributes(
    ObjectId id,
    std::vector<CustomAttribute> attributes) {
    for (const auto& attr : attributes) {
        if (attr.name.empty() || !std::isfinite(attr.value)) {
            return core::Status::fail(
                core::ErrorCode::ValidationError,
                "Custom attribute requires a non-empty name and finite value");
        }
    }
    auto* obj = findObjectMutable(id);
    if (obj == nullptr) {
        return core::Status::fail(core::ErrorCode::NotFound, "Scene object not found");
    }
    obj->custom_attributes = std::move(attributes);
    return core::Status::ok();
}

core::Status Scene::setDisplayLayers(std::vector<DisplayLayer> layers) {
    for (auto& layer : layers) {
        if (layer.name.empty()) {
            return core::Status::fail(core::ErrorCode::ValidationError, "Display layer name must not be empty");
        }
        std::erase_if(layer.members, [this](ObjectId oid) { return findObject(oid) == nullptr; });
    }
    display_layers_ = std::move(layers);
    return core::Status::ok();
}

const DisplayLayer* Scene::findObjectDisplayLayer(ObjectId id) const noexcept {
    if (!id.isValid() || display_layers_.empty()) {
        return nullptr;
    }
    const SceneObject* cursor = findObject(id);
    std::size_t guard = 0;
    const std::size_t max_depth = objects_.size() + 1;
    while (cursor != nullptr && ++guard <= max_depth) {
        for (const auto& layer : display_layers_) {
            if (std::find(layer.members.begin(), layer.members.end(), cursor->id) != layer.members.end()) {
                return &layer;
            }
        }
        if (!cursor->parent_id.isValid()) {
            break;
        }
        cursor = findObject(cursor->parent_id);
    }
    return nullptr;
}

void Scene::setTimeline(const TimelineSettings& settings) noexcept {
    TimelineSettings s = settings;
    if (!std::isfinite(s.start_frame)) s.start_frame = 1.0f;
    if (!std::isfinite(s.end_frame) || s.end_frame <= s.start_frame) s.end_frame = s.start_frame + 239.0f;
    s.playback_start = std::clamp(s.playback_start, s.start_frame, s.end_frame - 1.0f);
    s.playback_end = std::clamp(s.playback_end, s.playback_start + 1.0f, s.end_frame);
    s.current_frame = std::clamp(s.current_frame, s.start_frame, s.end_frame);
    s.fps = std::clamp(std::isfinite(s.fps) ? s.fps : 24.0f, 1.0f, 240.0f);
    timeline_ = s;
}

const ObjectAnimationTrack* Scene::findAnimationTrack(ObjectId id) const noexcept {
    for (const auto& tr : animation_tracks_) {
        if (tr.object_id == id) {
            return &tr;
        }
    }
    return nullptr;
}

core::Status Scene::setAnimationTracks(std::vector<ObjectAnimationTrack> tracks) {
    std::erase_if(tracks, [this](const ObjectAnimationTrack& tr) {
        return !tr.object_id.isValid() || findObject(tr.object_id) == nullptr || tr.keyframes.empty();
    });
    for (auto& tr : tracks) {
        std::sort(tr.keyframes.begin(), tr.keyframes.end(), [](const TransformKeyframe& a, const TransformKeyframe& b) {
            return a.frame < b.frame;
        });
    }
    animation_tracks_ = std::move(tracks);
    return core::Status::ok();
}

void Scene::evaluateAnimationAtFrame(float frame) {
    if (!std::isfinite(frame)) {
        return;
    }
    timeline_.current_frame = std::clamp(frame, timeline_.start_frame, timeline_.end_frame);
    const float f = timeline_.current_frame;
    for (const auto& tr : animation_tracks_) {
        if (tr.keyframes.empty()) {
            continue;
        }
        auto* obj = findObjectMutable(tr.object_id);
        if (obj == nullptr) {
            continue;
        }
        TransformKeyframe eval = tr.keyframes.front();
        if (f <= tr.keyframes.front().frame) {
            eval = tr.keyframes.front();
        } else if (f >= tr.keyframes.back().frame) {
            eval = tr.keyframes.back();
        } else {
            for (std::size_t i = 0; i + 1 < tr.keyframes.size(); ++i) {
                const auto& k0 = tr.keyframes[i];
                const auto& k1 = tr.keyframes[i + 1];
                if (f >= k0.frame && f <= k1.frame) {
                    const float span = std::max(1.0e-5f, k1.frame - k0.frame);
                    const float t = std::clamp((f - k0.frame) / span, 0.0f, 1.0f);
                    eval.frame = f;
                    eval.position = k0.position + (k1.position - k0.position) * t;
                    eval.rotation_deg = k0.rotation_deg + (k1.rotation_deg - k0.rotation_deg) * t;
                    eval.scale = k0.scale + (k1.scale - k0.scale) * t;
                    eval.visible = k0.visible;
                    break;
                }
            }
        }
        math::Transform next_tf;
        next_tf.position = eval.position;
        next_tf.rotation = math::Quat::fromEulerXYZ(math::Vec3{
            math::degToRad(eval.rotation_deg.x),
            math::degToRad(eval.rotation_deg.y),
            math::degToRad(eval.rotation_deg.z)});
        next_tf.scale = {
            std::fabs(eval.scale.x) < 1.0e-4f ? 1.0e-4f : eval.scale.x,
            std::fabs(eval.scale.y) < 1.0e-4f ? 1.0e-4f : eval.scale.y,
            std::fabs(eval.scale.z) < 1.0e-4f ? 1.0e-4f : eval.scale.z,
        };
        if (next_tf.isValid()) {
            obj->local_transform = next_tf;
        }
        obj->visible = eval.visible;
    }
}

core::Status Scene::assignMesh(ObjectId id, std::optional<MeshId> mesh_id) {
    auto* obj = findObjectMutable(id);
    if (obj == nullptr) {
        return core::Status::fail(core::ErrorCode::NotFound, "Scene object not found");
    }
    if (mesh_id.has_value() && findMesh(mesh_id.value()) == nullptr) {
        return core::Status::fail(
            core::ErrorCode::NotFound,
            "Referenced MeshId is not registered in this Scene",
            std::to_string(mesh_id->value));
    }
    obj->mesh_id = mesh_id;
    return core::Status::ok();
}

core::Status Scene::assignMaterial(ObjectId id, std::optional<MaterialId> material_id) {
    auto* obj = findObjectMutable(id);
    if (obj == nullptr) {
        return core::Status::fail(core::ErrorCode::NotFound, "Scene object not found");
    }
    if (material_id.has_value() && (!material_id->isValid() || findMaterial(material_id.value()) == nullptr)) {
        return core::Status::fail(
            core::ErrorCode::NotFound,
            "Referenced MaterialId is not registered in this Scene",
            std::to_string(material_id->value));
    }
    obj->material_id = material_id;
    return core::Status::ok();
}

core::Status Scene::assignCamera(ObjectId id, std::optional<CameraComponent> camera) {
    auto* obj = findObjectMutable(id);
    if (obj == nullptr) {
        return core::Status::fail(core::ErrorCode::NotFound, "Scene object not found");
    }
    if (camera.has_value()) {
        auto valid = camera->validate();
        if (!valid) {
            return valid;
        }
    } else if (active_camera_id_ == id) {
        active_camera_id_ = ObjectId::invalid();
    }
    obj->camera = camera;
    return core::Status::ok();
}

core::Status Scene::assignDirectionalLight(
    ObjectId id,
    std::optional<DirectionalLightComponent> light) {
    auto* obj = findObjectMutable(id);
    if (obj == nullptr) {
        return core::Status::fail(core::ErrorCode::NotFound, "Scene object not found");
    }
    if (light.has_value()) {
        auto valid = light->validate();
        if (!valid) {
            return valid;
        }
        light->linking.normalize();
        obj->point_light = std::nullopt;
        obj->spot_light = std::nullopt;
        obj->area_light = std::nullopt;
    }
    obj->directional_light = std::move(light);
    return core::Status::ok();
}

core::Status Scene::assignPointLight(
    ObjectId id,
    std::optional<PointLightComponent> light) {
    auto* obj = findObjectMutable(id);
    if (obj == nullptr) {
        return core::Status::fail(core::ErrorCode::NotFound, "Scene object not found");
    }
    if (light.has_value()) {
        auto valid = light->validate();
        if (!valid) {
            return valid;
        }
        light->linking.normalize();
        obj->directional_light = std::nullopt;
        obj->spot_light = std::nullopt;
        obj->area_light = std::nullopt;
    }
    obj->point_light = std::move(light);
    return core::Status::ok();
}

core::Status Scene::assignSpotLight(
    ObjectId id,
    std::optional<SpotLightComponent> light) {
    auto* obj = findObjectMutable(id);
    if (obj == nullptr) {
        return core::Status::fail(core::ErrorCode::NotFound, "Scene object not found");
    }
    if (light.has_value()) {
        auto valid = light->validate();
        if (!valid) {
            return valid;
        }
        light->linking.normalize();
        obj->directional_light = std::nullopt;
        obj->point_light = std::nullopt;
        obj->area_light = std::nullopt;
    }
    obj->spot_light = std::move(light);
    return core::Status::ok();
}

core::Status Scene::assignAreaLight(
    ObjectId id,
    std::optional<AreaLightComponent> light) {
    auto* obj = findObjectMutable(id);
    if (obj == nullptr) {
        return core::Status::fail(core::ErrorCode::NotFound, "Scene object not found");
    }
    if (light.has_value()) {
        auto valid = light->validate();
        if (!valid) {
            return valid;
        }
        light->linking.normalize();
        obj->directional_light = std::nullopt;
        obj->point_light = std::nullopt;
        obj->spot_light = std::nullopt;
    }
    obj->area_light = std::move(light);
    return core::Status::ok();
}

core::Status Scene::assignAnyLight(ObjectId id, const AnyLightComponent& light) {
    auto* obj = findObjectMutable(id);
    if (obj == nullptr) {
        return core::Status::fail(core::ErrorCode::NotFound, "Scene object not found");
    }
    return std::visit(
        [&](const auto& l) -> core::Status {
            using T = std::decay_t<decltype(l)>;
            if constexpr (std::is_same_v<T, std::monostate>) {
                obj->directional_light = std::nullopt;
                obj->point_light = std::nullopt;
                obj->spot_light = std::nullopt;
                obj->area_light = std::nullopt;
                return core::Status::ok();
            } else if constexpr (std::is_same_v<T, DirectionalLightComponent>) {
                return assignDirectionalLight(id, l);
            } else if constexpr (std::is_same_v<T, PointLightComponent>) {
                return assignPointLight(id, l);
            } else if constexpr (std::is_same_v<T, SpotLightComponent>) {
                return assignSpotLight(id, l);
            } else if constexpr (std::is_same_v<T, AreaLightComponent>) {
                return assignAreaLight(id, l);
            }
        },
        light);
}

bool Scene::doesLightAffectObject(
    const LightLinkingSettings& linking,
    ObjectId target_id) const noexcept {
    if (linking.mode == LightLinkMode::AllObjects) {
        return true;
    }
    if (!target_id.isValid()) {
        return linking.mode == LightLinkMode::ExcludeList;
    }

    bool matched = linking.containsDirect(target_id);
    if (!matched && linking.include_descendants) {
        ObjectId cur = target_id;
        std::size_t guard = 0;
        const std::size_t max_depth = objects_.size() + 1;
        while (cur.isValid() && guard++ < max_depth) {
            const auto* obj = findObject(cur);
            if (obj == nullptr || !obj->parent_id.isValid()) {
                break;
            }
            cur = obj->parent_id;
            if (linking.containsDirect(cur)) {
                matched = true;
                break;
            }
        }
    }

    return (linking.mode == LightLinkMode::IncludeOnly) ? matched : !matched;
}

core::Status Scene::setEnvironment(EnvironmentLighting updated) {
    auto valid = updated.validate();
    if (!valid) {
        return valid;
    }
    updated.revision = environment_.revision + 1u;
    environment_ = std::move(updated);
    return core::Status::ok();
}

core::Status Scene::loadEnvironmentHdriFromMemory(
    std::span<const std::uint8_t> bytes,
    std::string source_name) {
    auto asset_res = HdriEnvironmentAsset::loadFromMemory(bytes, source_name);
    if (!asset_res) {
        return asset_res.error();
    }
    EnvironmentLighting next = environment_;
    next.hdri_asset = std::move(asset_res).value();
    next.source_path = std::move(source_name);
    next.enabled = true;
    return setEnvironment(std::move(next));
}

core::Status Scene::loadEnvironmentHdriFromFile(
    const std::filesystem::path& path,
    const platform::IFileSystem& file_system) {
    auto asset_res = HdriEnvironmentAsset::loadFromFile(path, file_system);
    if (!asset_res) {
        return asset_res.error();
    }
    EnvironmentLighting next = environment_;
    next.hdri_asset = std::move(asset_res).value();
    next.source_path = path.generic_string();
    next.enabled = true;
    return setEnvironment(std::move(next));
}

void Scene::clearEnvironmentHdri() noexcept {
    environment_.hdri_asset.reset();
    environment_.source_path.clear();
    ++environment_.revision;
}

core::Status Scene::setActiveCamera(ObjectId camera_object_id) {
    if (!camera_object_id.isValid()) {
        active_camera_id_ = ObjectId::invalid();
        return core::Status::ok();
    }
    const auto* obj = findObject(camera_object_id);
    if (obj == nullptr) {
        return core::Status::fail(core::ErrorCode::NotFound, "Active camera object does not exist");
    }
    if (!obj->camera.has_value()) {
        return core::Status::fail(
            core::ErrorCode::ValidationError,
            "Active camera object does not have a CameraComponent");
    }
    active_camera_id_ = camera_object_id;
    return core::Status::ok();
}

core::Result<math::Mat4> Scene::computeWorldMatrix(ObjectId id) const {
    const auto* leaf = findObject(id);
    if (leaf == nullptr) {
        return core::Result<math::Mat4>::fail(core::ErrorCode::NotFound, "Scene object not found");
    }

    // Collect ancestor chain [leaf, parent, ..., root] then multiply root -> leaf.
    std::vector<const SceneObject*> chain;
    chain.reserve(8);
    const SceneObject* cursor = leaf;
    std::size_t guard = 0;
    const std::size_t max_depth = objects_.size() + 1;

    while (cursor != nullptr) {
        if (++guard > max_depth) {
            return core::Result<math::Mat4>::fail(
                core::ErrorCode::HierarchyCycle,
                "Detected cycle while computing world matrix");
        }
        chain.push_back(cursor);
        if (!cursor->parent_id.isValid()) {
            break;
        }
        cursor = findObject(cursor->parent_id);
        if (cursor == nullptr) {
            return core::Result<math::Mat4>::fail(
                core::ErrorCode::ValidationError,
                "Broken parent link in scene graph");
        }
    }

    math::Mat4 world = math::Mat4::identity();
    for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
        world = world * (*it)->local_transform.toMatrix();
    }
    return core::Result<math::Mat4>::ok(world);
}

core::Result<math::AABB> Scene::computeObjectWorldBounds(ObjectId id) const {
    const auto* obj = findObject(id);
    if (obj == nullptr) {
        return core::Result<math::AABB>::fail(core::ErrorCode::NotFound, "Scene object not found");
    }
    auto world_res = computeWorldMatrix(id);
    if (!world_res) {
        return world_res.error();
    }

    if (obj->mesh_id.has_value()) {
        if (const auto* mesh = findMesh(obj->mesh_id.value()); mesh != nullptr && mesh->local_bounds.isValid()) {
            return core::Result<math::AABB>::ok(mesh->local_bounds.transformed(world_res.value()));
        }
    }

    if (obj->area_light.has_value()) {
        const float hw = std::max(0.35f, 0.5f * obj->area_light->width);
        const float hh = (obj->area_light->shape == AreaLightShape::Disk)
                             ? hw
                             : std::max(0.35f, 0.5f * obj->area_light->height);
        const math::AABB area_local = math::AABB::fromMinMax(
            math::Vec3{-hw, -hh, -0.75f},
            math::Vec3{hw, hh, 0.25f});
        return core::Result<math::AABB>::ok(area_local.transformed(world_res.value()));
    }

    if (obj->camera.has_value() || obj->hasAnyLight()) {
        const math::AABB helper_local = math::AABB::fromMinMax(
            math::Vec3{-0.45f, -0.45f, -0.85f},
            math::Vec3{0.45f, 0.45f, 0.45f});
        return core::Result<math::AABB>::ok(helper_local.transformed(world_res.value()));
    }

    const math::Vec3 origin = world_res.value().transformPoint(math::Vec3::zero());
    const math::Vec3 half_pad{0.25f, 0.25f, 0.25f};
    return core::Result<math::AABB>::ok(math::AABB::fromMinMax(origin - half_pad, origin + half_pad));
}

core::ResourceStats Scene::collectResourceStats() const noexcept {
    core::ResourceStats stats;
    stats.scene_object_count = objects_.size();
    stats.mesh_asset_count = meshes_.size();
    stats.material_count = materials_.size();
    for (const auto& mesh : meshes_) {
        stats.total_cpu_vertices += mesh.data.vertices.size();
        stats.total_cpu_indices += mesh.data.indices.size();
        stats.cpu_mesh_bytes += mesh.data.byteSize();
    }
    return stats;
}

} // namespace finaleffect::scene
