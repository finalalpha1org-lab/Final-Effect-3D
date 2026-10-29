// SPDX-License-Identifier: GPL-3.0-only
#include "finaleffect/rendering/MeshLibrary.hpp"

#include <cmath>
#include <utility>

namespace fe::rendering {
namespace {

constexpr math::Scalar kPiScalar = 3.14159265358979323846F;

void addQuadFace(MeshData& mesh, const math::Vec3& a, const math::Vec3& b, const math::Vec3& c,
                 const math::Vec3& d, const math::Vec3& normal) {
    const auto base = static_cast<std::uint32_t>(mesh.vertices.size());
    mesh.vertices.push_back(Vertex{a, normal});
    mesh.vertices.push_back(Vertex{b, normal});
    mesh.vertices.push_back(Vertex{c, normal});
    mesh.vertices.push_back(Vertex{d, normal});
    mesh.indices.push_back(base + 0U);
    mesh.indices.push_back(base + 1U);
    mesh.indices.push_back(base + 2U);
    mesh.indices.push_back(base + 0U);
    mesh.indices.push_back(base + 2U);
    mesh.indices.push_back(base + 3U);
}

}  // namespace

MeshLibrary::MeshLibrary() {
    add(makeCube(1.0F));
    add(makePlane(2.0F, 4));
    add(makeSphere(0.5F, 24, 12));
}

void MeshLibrary::add(MeshData mesh) {
    meshes_.push_back(std::move(mesh));
}

const MeshData* MeshLibrary::find(std::string_view name) const noexcept {
    for (const MeshData& mesh : meshes_) {
        if (mesh.name == name) {
            return &mesh;
        }
    }
    return nullptr;
}

std::vector<std::string> MeshLibrary::names() const {
    std::vector<std::string> result;
    result.reserve(meshes_.size());
    for (const MeshData& mesh : meshes_) {
        result.push_back(mesh.name);
    }
    return result;
}

MeshData MeshLibrary::makeCube(math::Scalar size) {
    const math::Scalar h = size * 0.5F;
    MeshData mesh;
    mesh.name = "cube";
    addQuadFace(mesh, math::Vec3(-h, -h, h), math::Vec3(h, -h, h), math::Vec3(h, h, h), math::Vec3(-h, h, h),
                math::Vec3(0.0F, 0.0F, 1.0F));
    addQuadFace(mesh, math::Vec3(h, -h, -h), math::Vec3(-h, -h, -h), math::Vec3(-h, h, -h), math::Vec3(h, h, -h),
                math::Vec3(0.0F, 0.0F, -1.0F));
    addQuadFace(mesh, math::Vec3(h, -h, h), math::Vec3(h, -h, -h), math::Vec3(h, h, -h), math::Vec3(h, h, h),
                math::Vec3(1.0F, 0.0F, 0.0F));
    addQuadFace(mesh, math::Vec3(-h, -h, -h), math::Vec3(-h, -h, h), math::Vec3(-h, h, h), math::Vec3(-h, h, -h),
                math::Vec3(-1.0F, 0.0F, 0.0F));
    addQuadFace(mesh, math::Vec3(-h, h, h), math::Vec3(h, h, h), math::Vec3(h, h, -h), math::Vec3(-h, h, -h),
                math::Vec3(0.0F, 1.0F, 0.0F));
    addQuadFace(mesh, math::Vec3(-h, -h, -h), math::Vec3(h, -h, -h), math::Vec3(h, -h, h), math::Vec3(-h, -h, h),
                math::Vec3(0.0F, -1.0F, 0.0F));
    return mesh;
}

MeshData MeshLibrary::makePlane(math::Scalar size, int subdivisions) {
    const int cells = subdivisions < 1 ? 1 : subdivisions;
    const math::Scalar half = size * 0.5F;
    const math::Scalar step = size / static_cast<math::Scalar>(cells);

    MeshData mesh;
    mesh.name = "plane";
    for (int row = 0; row <= cells; ++row) {
        for (int column = 0; column <= cells; ++column) {
            const math::Scalar x = -half + step * static_cast<math::Scalar>(column);
            const math::Scalar z = -half + step * static_cast<math::Scalar>(row);
            mesh.vertices.push_back(Vertex{math::Vec3(x, 0.0F, z), math::Vec3(0.0F, 1.0F, 0.0F)});
        }
    }
    const auto rowStride = static_cast<std::uint32_t>(cells + 1);
    for (int row = 0; row < cells; ++row) {
        for (int column = 0; column < cells; ++column) {
            const std::uint32_t topLeft = static_cast<std::uint32_t>(row) * rowStride +
                                          static_cast<std::uint32_t>(column);
            const std::uint32_t topRight = topLeft + 1U;
            const std::uint32_t bottomLeft = topLeft + rowStride;
            const std::uint32_t bottomRight = bottomLeft + 1U;
            mesh.indices.push_back(topLeft);
            mesh.indices.push_back(bottomLeft);
            mesh.indices.push_back(topRight);
            mesh.indices.push_back(topRight);
            mesh.indices.push_back(bottomLeft);
            mesh.indices.push_back(bottomRight);
        }
    }
    return mesh;
}

MeshData MeshLibrary::makeSphere(math::Scalar radius, int segments, int rings) {
    const int safeSegments = segments < 3 ? 3 : segments;
    const int safeRings = rings < 2 ? 2 : rings;

    MeshData mesh;
    mesh.name = "sphere";
    for (int ring = 0; ring <= safeRings; ++ring) {
        const math::Scalar v = static_cast<math::Scalar>(ring) / static_cast<math::Scalar>(safeRings);
        const math::Scalar phi = v * kPiScalar;
        const math::Scalar sinPhi = std::sin(phi);
        const math::Scalar cosPhi = std::cos(phi);
        for (int segment = 0; segment <= safeSegments; ++segment) {
            const math::Scalar u = static_cast<math::Scalar>(segment) / static_cast<math::Scalar>(safeSegments);
            const math::Scalar theta = u * 2.0F * kPiScalar;
            const math::Vec3 normal(sinPhi * std::cos(theta), cosPhi, sinPhi * std::sin(theta));
            mesh.vertices.push_back(Vertex{normal * radius, normal});
        }
    }
    const auto stride = static_cast<std::uint32_t>(safeSegments + 1);
    for (int ring = 0; ring < safeRings; ++ring) {
        for (int segment = 0; segment < safeSegments; ++segment) {
            const std::uint32_t a = static_cast<std::uint32_t>(ring) * stride + static_cast<std::uint32_t>(segment);
            const std::uint32_t b = a + stride;
            mesh.indices.push_back(a);
            mesh.indices.push_back(b);
            mesh.indices.push_back(a + 1U);
            mesh.indices.push_back(a + 1U);
            mesh.indices.push_back(b);
            mesh.indices.push_back(b + 1U);
        }
    }
    return mesh;
}

}  // namespace fe::rendering
