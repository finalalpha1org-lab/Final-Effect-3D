// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include "finaleffect/core/resource_stats.hpp"
#include "finaleffect/core/result.hpp"
#include "finaleffect/math/mat4.hpp"
#include "finaleffect/math/transform.hpp"
#include "finaleffect/scene/editable_mesh.hpp"
#include "finaleffect/scene/material.hpp"
#include "finaleffect/scene/mesh.hpp"
#include "finaleffect/scene/object_id.hpp"
#include "finaleffect/scene/scene_object.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace finaleffect::scene {

enum class RemovalMode : std::uint8_t {
    Recursive = 0,
    ReparentChildren = 1
};

// Single-owner scene graph and CPU mesh asset registry.
// Enforces acyclic parent-child invariants, valid transforms, and valid asset references.
class Scene {
public:
    explicit Scene(std::string scene_name = "Untitled Scene");

    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    void setName(std::string scene_name);

    void clear();

    // Mesh asset management (shared across multiple SceneObject instances without copying vertices).
    [[nodiscard]] core::Result<MeshId> registerMesh(std::string mesh_name, MeshData data);
    [[nodiscard]] core::Status registerMeshWithId(MeshId id, std::string mesh_name, MeshData data);
    [[nodiscard]] core::Result<MeshId> registerEditableMesh(std::string mesh_name, EditableMesh editable);
    [[nodiscard]] core::Status registerEditableMeshWithId(MeshId id, std::string mesh_name, EditableMesh editable);
    [[nodiscard]] core::Status ensureEditableMesh(MeshId id);
    [[nodiscard]] core::Status updateMeshEditable(MeshId id, EditableMesh updated_editable);
    [[nodiscard]] const MeshAsset* findMesh(MeshId id) const noexcept;
    [[nodiscard]] const std::vector<MeshAsset>& meshes() const noexcept { return meshes_; }
    [[nodiscard]] std::size_t meshCount() const noexcept { return meshes_.size(); }

    // Metallic-roughness PBR material asset management (stable IDs, safe deletion).
    [[nodiscard]] core::Result<MaterialId> createMaterial(PbrMaterial material);
    [[nodiscard]] core::Status createMaterialWithId(MaterialId id, PbrMaterial material);
    [[nodiscard]] const PbrMaterial* findMaterial(MaterialId id) const noexcept;
    [[nodiscard]] const PbrMaterial* findMaterialByName(std::string_view material_name) const noexcept;
    [[nodiscard]] const std::vector<PbrMaterial>& materials() const noexcept { return materials_; }
    [[nodiscard]] std::size_t materialCount() const noexcept { return materials_.size(); }
    [[nodiscard]] core::Status updateMaterial(MaterialId id, const PbrMaterial& updated);
    [[nodiscard]] core::Status renameMaterial(MaterialId id, std::string new_name);
    [[nodiscard]] core::Result<MaterialId> duplicateMaterial(MaterialId source_id, std::string duplicate_name = {});
    [[nodiscard]] core::Status removeMaterial(MaterialId id);

    // Scene object creation, lookup, and removal.
    [[nodiscard]] core::Result<ObjectId> createObject(
        std::string object_name,
        ObjectId parent_id = ObjectId::invalid());
    [[nodiscard]] core::Status createObjectWithId(
        ObjectId id,
        std::string object_name,
        ObjectId parent_id = ObjectId::invalid());

    [[nodiscard]] const SceneObject* findObject(ObjectId id) const noexcept;
    [[nodiscard]] const SceneObject* findObjectByName(std::string_view object_name) const noexcept;
    [[nodiscard]] const std::vector<SceneObject>& objects() const noexcept { return objects_; }
    [[nodiscard]] std::size_t objectCount() const noexcept { return objects_.size(); }

    [[nodiscard]] core::Status removeObject(ObjectId id, RemovalMode mode = RemovalMode::Recursive);

    // Hierarchy and transform operations.
    [[nodiscard]] core::Status setParent(ObjectId child_id, ObjectId new_parent_id);
    [[nodiscard]] core::Status setObjectTransform(ObjectId id, const math::Transform& local_transform);
    [[nodiscard]] core::Status setObjectVisible(ObjectId id, bool visible);
    [[nodiscard]] core::Status setObjectName(ObjectId id, std::string new_name);

    // Typed components.
    [[nodiscard]] core::Status assignMesh(ObjectId id, std::optional<MeshId> mesh_id);
    [[nodiscard]] core::Status assignMaterial(ObjectId id, std::optional<MaterialId> material_id);
    [[nodiscard]] core::Status assignCamera(ObjectId id, std::optional<CameraComponent> camera);
    [[nodiscard]] core::Status assignDirectionalLight(
        ObjectId id,
        std::optional<DirectionalLightComponent> light);

    [[nodiscard]] core::Status setActiveCamera(ObjectId camera_object_id);
    [[nodiscard]] ObjectId activeCamera() const noexcept { return active_camera_id_; }

    // Evaluates exact hierarchical world matrix: Root * ... * Parent * Local.
    [[nodiscard]] core::Result<math::Mat4> computeWorldMatrix(ObjectId id) const;

    // Computes conservative world-space AABB for an object (uses mesh bounds or a unit point AABB).
    [[nodiscard]] core::Result<math::AABB> computeObjectWorldBounds(ObjectId id) const;

    // CPU-side resource and memory telemetry.
    [[nodiscard]] core::ResourceStats collectResourceStats() const noexcept;

private:
    [[nodiscard]] SceneObject* findObjectMutable(ObjectId id) noexcept;
    [[nodiscard]] MeshAsset* findMeshMutable(MeshId id) noexcept;
    [[nodiscard]] PbrMaterial* findMaterialMutable(MaterialId id) noexcept;
    [[nodiscard]] bool wouldCreateCycle(ObjectId child_id, ObjectId candidate_parent_id) const noexcept;
    void detachFromParent(ObjectId child_id) noexcept;
    void eraseSingleObject(ObjectId id) noexcept;

    std::string name_;
    std::uint64_t next_object_id_{1};
    std::uint64_t next_mesh_id_{1};
    std::uint64_t next_material_id_{1};

    std::vector<MeshAsset> meshes_;
    std::unordered_map<MeshId, std::size_t> mesh_index_;

    std::vector<PbrMaterial> materials_;
    std::unordered_map<MaterialId, std::size_t> material_index_;

    std::vector<SceneObject> objects_;
    std::unordered_map<ObjectId, std::size_t> object_index_;

    ObjectId active_camera_id_{ObjectId::invalid()};
};

} // namespace finaleffect::scene
