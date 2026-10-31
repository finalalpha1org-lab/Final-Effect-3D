// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include "finaleffect/math/vec3.hpp"
#include "finaleffect/scene/mesh.hpp"

#include <cstdint>

namespace finaleffect::scene {

class Primitives {
public:
    [[nodiscard]] static MeshData createCube(
        float side_length = 1.0f,
        const math::Vec3& base_color = math::Vec3{0.85f, 0.46f, 0.20f});

    [[nodiscard]] static MeshData createPyramid(
        float base_length = 1.0f,
        float height = 1.25f,
        const math::Vec3& base_color = math::Vec3{0.24f, 0.68f, 0.88f});

    [[nodiscard]] static MeshData createCheckerPlane(
        float half_extent = 4.0f,
        std::uint32_t subdivisions = 4,
        const math::Vec3& color_a = math::Vec3{0.22f, 0.24f, 0.27f},
        const math::Vec3& color_b = math::Vec3{0.31f, 0.34f, 0.38f});

    [[nodiscard]] static MeshData createUvSphere(
        float radius = 0.75f,
        std::uint32_t rings = 16,
        std::uint32_t sectors = 24,
        const math::Vec3& base_color = math::Vec3{0.82f, 0.85f, 0.90f});

    [[nodiscard]] static MeshData createCylinder(
        float radius = 0.5f,
        float height = 1.2f,
        std::uint32_t sectors = 20,
        const math::Vec3& base_color = math::Vec3{0.84f, 0.86f, 0.90f});

    [[nodiscard]] static MeshData createCone(
        float radius = 0.6f,
        float height = 1.2f,
        std::uint32_t sectors = 20,
        const math::Vec3& base_color = math::Vec3{0.84f, 0.86f, 0.90f});

    [[nodiscard]] static MeshData createTorus(
        float major_radius = 0.65f,
        float minor_radius = 0.22f,
        std::uint32_t major_segments = 24,
        std::uint32_t minor_segments = 12,
        const math::Vec3& base_color = math::Vec3{0.84f, 0.86f, 0.90f});
};

} // namespace finaleffect::scene
