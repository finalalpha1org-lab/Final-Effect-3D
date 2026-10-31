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
    std::optional<PointLightComponent> point_light;
    std::optional<SpotLightComponent> spot_light;
    std::optional<AreaLightComponent> area_light;

    [[nodiscard]] bool hasAnyLight() const noexcept {
        return directional_light.has_value() ||
               point_light.has_value() ||
               spot_light.has_value() ||
               area_light.has_value();
    }

    [[nodiscard]] std::optional<LightType> lightType() const noexcept {
        if (directional_light.has_value()) return LightType::Directional;
        if (point_light.has_value()) return LightType::Point;
        if (spot_light.has_value()) return LightType::Spot;
        if (area_light.has_value()) return LightType::Area;
        return std::nullopt;
    }

    [[nodiscard]] AnyLightComponent anyLightComponent() const {
        if (directional_light.has_value()) return *directional_light;
        if (point_light.has_value()) return *point_light;
        if (spot_light.has_value()) return *spot_light;
        if (area_light.has_value()) return *area_light;
        return std::monostate{};
    }

    [[nodiscard]] const LightLinkingSettings* lightLinking() const noexcept {
        if (directional_light.has_value()) return &directional_light->linking;
        if (point_light.has_value()) return &point_light->linking;
        if (spot_light.has_value()) return &spot_light->linking;
        if (area_light.has_value()) return &area_light->linking;
        return nullptr;
    }

    [[nodiscard]] LightLinkingSettings* lightLinkingMutable() noexcept {
        if (directional_light.has_value()) return &directional_light->linking;
        if (point_light.has_value()) return &point_light->linking;
        if (spot_light.has_value()) return &spot_light->linking;
        if (area_light.has_value()) return &area_light->linking;
        return nullptr;
    }
};

} // namespace finaleffect::scene
