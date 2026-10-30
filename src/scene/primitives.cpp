// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/scene/primitives.hpp"
#include "finaleffect/scene/editable_mesh.hpp"

#include <algorithm>
#include <array>

namespace finaleffect::scene {

MeshData Primitives::createCube(float side_length, const math::Vec3& base_color) {
    const float h = 0.5f * std::max(side_length, 1.0e-4f);
    MeshData mesh;
    mesh.vertices.reserve(24);
    mesh.indices.reserve(36);

    auto appendFace = [&](const math::Vec3& p0,
                          const math::Vec3& p1,
                          const math::Vec3& p2,
                          const math::Vec3& p3,
                          const math::Vec3& normal,
                          float shade) {
        const auto start = static_cast<std::uint32_t>(mesh.vertices.size());
        const math::Vec3 c = base_color * shade;
        mesh.vertices.push_back(Vertex{p0, normal, c, {0.0f, 0.0f}});
        mesh.vertices.push_back(Vertex{p1, normal, c, {1.0f, 0.0f}});
        mesh.vertices.push_back(Vertex{p2, normal, c, {1.0f, 1.0f}});
        mesh.vertices.push_back(Vertex{p3, normal, c, {0.0f, 1.0f}});

        // Counter-clockwise winding when viewed from outside (right-handed).
        mesh.indices.insert(mesh.indices.end(), {start + 0, start + 1, start + 2, start + 0, start + 2, start + 3});
    };

    // +Z front face
    appendFace({-h, -h, h}, {h, -h, h}, {h, h, h}, {-h, h, h}, {0.0f, 0.0f, 1.0f}, 1.00f);
    // -Z back face
    appendFace({h, -h, -h}, {-h, -h, -h}, {-h, h, -h}, {h, h, -h}, {0.0f, 0.0f, -1.0f}, 0.92f);
    // +X right face
    appendFace({h, -h, h}, {h, -h, -h}, {h, h, -h}, {h, h, h}, {1.0f, 0.0f, 0.0f}, 0.96f);
    // -X left face
    appendFace({-h, -h, -h}, {-h, -h, h}, {-h, h, h}, {-h, h, -h}, {-1.0f, 0.0f, 0.0f}, 0.88f);
    // +Y top face
    appendFace({-h, h, h}, {h, h, h}, {h, h, -h}, {-h, h, -h}, {0.0f, 1.0f, 0.0f}, 1.05f);
    // -Y bottom face
    appendFace({-h, -h, -h}, {h, -h, -h}, {h, -h, h}, {-h, -h, h}, {0.0f, -1.0f, 0.0f}, 0.80f);

    return mesh;
}

MeshData Primitives::createPyramid(float base_length, float height, const math::Vec3& base_color) {
    const float h = 0.5f * std::max(base_length, 1.0e-4f);
    const float top_y = std::max(height, 1.0e-4f) * 0.5f;
    const float bot_y = -top_y;

    const math::Vec3 apex{0.0f, top_y, 0.0f};
    const math::Vec3 bl{-h, bot_y, h};
    const math::Vec3 br{h, bot_y, h};
    const math::Vec3 fr{h, bot_y, -h};
    const math::Vec3 fl{-h, bot_y, -h};

    MeshData mesh;
    mesh.vertices.reserve(16);
    mesh.indices.reserve(18);

    auto appendTri = [&](const math::Vec3& a, const math::Vec3& b, const math::Vec3& c) {
        const math::Vec3 n = (b - a).cross(c - a).normalized();
        const auto start = static_cast<std::uint32_t>(mesh.vertices.size());
        mesh.vertices.push_back(Vertex{a, n, base_color, {0.0f, 0.0f}});
        mesh.vertices.push_back(Vertex{b, n, base_color, {1.0f, 0.0f}});
        mesh.vertices.push_back(Vertex{c, n, base_color, {0.5f, 1.0f}});
        mesh.indices.insert(mesh.indices.end(), {start + 0, start + 1, start + 2});
    };

    appendTri(bl, br, apex);
    appendTri(br, fr, apex);
    appendTri(fr, fl, apex);
    appendTri(fl, bl, apex);

    // Bottom quad (-Y normal)
    const auto base_start = static_cast<std::uint32_t>(mesh.vertices.size());
    const math::Vec3 down{0.0f, -1.0f, 0.0f};
    mesh.vertices.push_back(Vertex{fl, down, base_color * 0.85f, {0.0f, 0.0f}});
    mesh.vertices.push_back(Vertex{fr, down, base_color * 0.85f, {1.0f, 0.0f}});
    mesh.vertices.push_back(Vertex{br, down, base_color * 0.85f, {1.0f, 1.0f}});
    mesh.vertices.push_back(Vertex{bl, down, base_color * 0.85f, {0.0f, 1.0f}});
    mesh.indices.insert(
        mesh.indices.end(),
        {base_start + 0, base_start + 1, base_start + 2, base_start + 0, base_start + 2, base_start + 3});

    return mesh;
}

MeshData Primitives::createCheckerPlane(
    float half_extent,
    std::uint32_t subdivisions,
    const math::Vec3& color_a,
    const math::Vec3& color_b) {
    const float ext = std::max(half_extent, 0.1f);
    const std::uint32_t cells = std::max(subdivisions, 1u);
    const float step = (2.0f * ext) / static_cast<float>(cells);

    MeshData mesh;
    mesh.vertices.reserve(static_cast<std::size_t>(cells) * cells * 4);
    mesh.indices.reserve(static_cast<std::size_t>(cells) * cells * 6);

    const math::Vec3 up{0.0f, 1.0f, 0.0f};

    for (std::uint32_t gz = 0; gz < cells; ++gz) {
        for (std::uint32_t gx = 0; gx < cells; ++gx) {
            const float x0 = -ext + static_cast<float>(gx) * step;
            const float x1 = x0 + step;
            const float z0 = -ext + static_cast<float>(gz) * step;
            const float z1 = z0 + step;

            const math::Vec3 c = ((gx + gz) % 2 == 0) ? color_a : color_b;
            const auto start = static_cast<std::uint32_t>(mesh.vertices.size());

            // CCW when viewed from +Y looking down (-Y): (x0,z1) -> (x1,z1) -> (x1,z0) -> (x0,z0).
            mesh.vertices.push_back(Vertex{{x0, 0.0f, z1}, up, c, {0.0f, 0.0f}});
            mesh.vertices.push_back(Vertex{{x1, 0.0f, z1}, up, c, {1.0f, 0.0f}});
            mesh.vertices.push_back(Vertex{{x1, 0.0f, z0}, up, c, {1.0f, 1.0f}});
            mesh.vertices.push_back(Vertex{{x0, 0.0f, z0}, up, c, {0.0f, 1.0f}});

            mesh.indices.insert(
                mesh.indices.end(),
                {start + 0, start + 1, start + 2, start + 0, start + 2, start + 3});
        }
    }

    return mesh;
}

MeshData Primitives::createUvSphere(
    float radius,
    std::uint32_t rings,
    std::uint32_t sectors,
    const math::Vec3& base_color) {
    return EditableMesh::createUvSphere(radius, rings, sectors, base_color).toMeshData();
}

} // namespace finaleffect::scene
