// Final Effect 3D - scene container and hierarchy.
// SPDX-License-Identifier: GPL-3.0-only
//
// The scene is the authoritative, portable description of what a project
// contains. It stores no GPU resources and no platform objects.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "finaleffect/core/Error.hpp"
#include "finaleffect/scene/SceneObject.hpp"

namespace fe::scene {

class Scene {
public:
    Scene() = default;
    Scene(const Scene&) = default;
    Scene(Scene&&) noexcept = default;
    Scene& operator=(const Scene&) = default;
    Scene& operator=(Scene&&) noexcept = default;
    ~Scene() = default;

    /// Creates an object. The parent must exist when it is not kInvalidObjectId.
    /// The returned identifier is never reused by this scene.
    core::Result<ObjectId> createObject(std::string name, ObjectId parent = kInvalidObjectId);

    /// Removes an object and all of its descendants.
    core::Status destroyObject(ObjectId id);

    /// Reparents an object. Rejects unknown ids, self-parenting and cycles.
    core::Status setParent(ObjectId child, ObjectId parent);

    core::Status setName(ObjectId id, std::string name);

    SceneObject* find(ObjectId id) noexcept;
    const SceneObject* find(ObjectId id) const noexcept;
    bool contains(ObjectId id) const noexcept;

    std::size_t objectCount() const noexcept { return objects_.size(); }
    const std::vector<SceneObject>& objects() const noexcept { return objects_; }

    /// Depth-first list of root objects in creation order.
    std::vector<ObjectId> roots() const;

    /// World matrix of an object, or the identity when the id is unknown.
    math::Mat4 worldMatrix(ObjectId id) const;

    /// World transform of an object (decomposed from the world matrix; hierarchy
    /// chains mixing non-uniform scale and rotation are approximated, exactly as
    /// in every TRS-based tool).
    math::Transform worldTransform(ObjectId id) const;

    /// Number of parents between the object and the scene root (0 for roots).
    std::size_t depthOf(ObjectId id) const;

    bool isAncestorOf(ObjectId candidateAncestor, ObjectId id) const;

    /// Highest identifier handed out so far (0 for an empty scene).
    std::uint64_t maxAssignedId() const noexcept { return nextIdValue_ - 1U; }

    void reserve(std::size_t objectCount);

    /// Removes every object and restarts identifier allocation.
    void clear() noexcept;

    /// Checks identifier uniqueness, parent references, cycles, finite transforms and
    /// non-empty names. Returns the first problem found.
    core::Status validate() const;

    /// Bulk construction used by the project loader. Validates the whole set
    /// before committing: on failure the caller keeps its previous scene.
    static core::Result<Scene> fromObjects(std::vector<SceneObject> objects);

private:
    void rebuildIndex();
    void rebuildChildren();
    void appendWithDescendants(ObjectId id, std::vector<ObjectId>& out) const;

    core::Error cycleError(ObjectId child, ObjectId parent) const;

    std::vector<SceneObject> objects_;
    std::unordered_map<ObjectId, std::size_t, ObjectIdHash> index_;
    std::uint64_t nextIdValue_ = 1U;
};

}  // namespace fe::scene
