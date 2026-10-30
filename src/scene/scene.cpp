// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/scene/scene.hpp"

#include <algorithm>
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

    const std::size_t remove_idx = it->second;
    const std::size_t last_idx = objects_.size() - 1;
    if (remove_idx != last_idx) {
        objects_[remove_idx] = std::move(objects_[last_idx]);
        object_index_[objects_[remove_idx].id] = remove_idx;
    }
    objects_.pop_back();
    object_index_.erase(it);
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
    }
    obj->directional_light = light;
    return core::Status::ok();
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
