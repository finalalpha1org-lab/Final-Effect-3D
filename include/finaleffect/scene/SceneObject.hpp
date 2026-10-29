// Final Effect 3D - a single scene object.
// SPDX-License-Identifier: GPL-3.0-only
//
// Ownership rules:
//   * Scene owns every SceneObject. Application code never owns scene objects.
//   * SceneObject handles are non-owning and are invalidated by any structural
//     change (creation, destruction, reparenting, load, clear).
//   * References between objects use ObjectId values, never pointers.
#pragma once

#include <string>
#include <vector>

#include "finaleffect/math/Transform.hpp"
#include "finaleffect/scene/ObjectId.hpp"

namespace fe::scene {

struct SceneObject {
    ObjectId id = kInvalidObjectId;
    std::string name;

    /// Transform relative to the parent object (or to world space for roots).
    math::Transform localTransform;

    /// kInvalidObjectId for root objects.
    ObjectId parent = kInvalidObjectId;

    /// Maintained by Scene (rebuilt on load). Not authoritative in project files,
    /// which store only the parent reference.
    std::vector<ObjectId> children;

    /// Name of the primitive mesh to draw, empty when the object has no geometry.
    /// Stage 1 ships built-in primitives (see rendering::MeshLibrary).
    std::string mesh;

    /// Base colour used by the reference renderers (linear RGB, 0..1).
    math::Vec3 color{0.82F, 0.82F, 0.86F};

    bool visible = true;
};

}  // namespace fe::scene
