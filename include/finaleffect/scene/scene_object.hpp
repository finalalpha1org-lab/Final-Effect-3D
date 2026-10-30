// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include "finaleffect/math/transform.hpp"
#include "finaleffect/scene/camera.hpp"
#include "finaleffect/scene/light.hpp"
#include "finaleffect/scene/object_id.hpp"

#include <optional>
#include <string>
#include <vector>

namespace finaleffect::scene {

struct SceneObject {
    ObjectId id{ObjectId::invalid()};
    std::string name;
    bool visible{true};
    math::Transform local_transform{};
    ObjectId parent_id{ObjectId::invalid()};
    std::vector<ObjectId> children;

    std::optional<MeshId> mesh_id;
    std::optional<MaterialId> material_id;
    std::optional<CameraComponent> camera;
    std::optional<DirectionalLightComponent> directional_light;
};

} // namespace finaleffect::scene
