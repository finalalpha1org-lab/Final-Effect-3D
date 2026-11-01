// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include "finaleffect/core/result.hpp"
#include "finaleffect/math/aabb.hpp"
#include "finaleffect/math/mat4.hpp"
#include "finaleffect/math/ray.hpp"
#include "finaleffect/math/vec2.hpp"
#include "finaleffect/math/vec3.hpp"
#include "finaleffect/scene/object_id.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace finaleffect::scene {

struct MeshData;

enum class EditMode : std::uint8_t {
    Object = 0,
    Vertex = 1,
    Edge = 2,
    Face = 3,
};

struct EditVertex {
    math::Vec3 position{};
    math::Vec3 normal{0.0f, 1.0f, 0.0f};
    math::Vec3 color{1.0f, 1.0f, 1.0f};
};

struct EditEdge {
    std::uint32_t v0{0};
    std::uint32_t v1{0}; // Canonical v0 < v1
    std::vector<std::uint32_t> adjacent_faces;

    [[nodiscard]] bool isBoundary() const noexcept { return adjacent_faces.size() == 1; }
};

struct FaceCorner {
    std::uint32_t vertex{0};
    math::Vec2 uv{0.0f, 0.0f};
};

struct EditFace {
    std::vector<FaceCorner> corners;
    math::Vec3 normal{0.0f, 1.0f, 0.0f};
    math::Vec3 color{1.0f, 1.0f, 1.0f};
    MaterialId material_id{MaterialId::invalid()};
    bool smooth_shading{false};
};

struct MeshElementSelection {
    std::vector<std::uint32_t> vertices;
    std::vector<std::uint32_t> edges;
    std::vector<std::uint32_t> faces;

    void clear() noexcept {
        vertices.clear();
        edges.clear();
        faces.clear();
    }

    [[nodiscard]] bool empty() const noexcept {
        return vertices.empty() && edges.empty() && faces.empty();
    }

    void toggleOrSetVertex(std::uint32_t idx, bool additive);
    void toggleOrSetEdge(std::uint32_t idx, bool additive);
    void toggleOrSetFace(std::uint32_t idx, bool additive);
    void sanitizeForCounts(std::size_t vert_count, std::size_t edge_count, std::size_t face_count);
};

// Manifold-validated polygon mesh representation supporting triangles, quads, and n-gons,
// topological edge adjacency, per-face materials & UVs, and non-destructive triangulation.
class EditableMesh {
public:
    EditableMesh() = default;
    EditableMesh(std::vector<EditVertex> vertices, std::vector<EditFace> faces);

    [[nodiscard]] const std::vector<EditVertex>& vertices() const noexcept { return vertices_; }
    [[nodiscard]] const std::vector<EditEdge>& edges() const noexcept { return edges_; }
    [[nodiscard]] const std::vector<EditFace>& faces() const noexcept { return faces_; }

    [[nodiscard]] std::size_t vertexCount() const noexcept { return vertices_.size(); }
    [[nodiscard]] std::size_t edgeCount() const noexcept { return edges_.size(); }
    [[nodiscard]] std::size_t faceCount() const noexcept { return faces_.size(); }

    // Validates polygon loops, vertex bounds, finite positions, non-zero face areas,
    // and 2-manifold half-edge orientation consistency, then rebuilds edges & normals.
    [[nodiscard]] core::Status validateAndRebuild();

    // Triangulates polygons into renderable MeshData with SubmeshRange material slices
    // and triangle-to-polygon face mapping.
    [[nodiscard]] MeshData toMeshData() const;

    // Reconstructs a welded polygon EditableMesh from an arbitrary triangle MeshData
    // (used when loading legacy .fe3d v1.0 / v1.1 triangle meshes).
    [[nodiscard]] static core::Result<EditableMesh> fromMeshData(const MeshData& tri_mesh);

    // Canonical manifold polygon primitives.
    [[nodiscard]] static EditableMesh createCube(
        float side_length = 1.0f,
        const math::Vec3& base_color = {0.86f, 0.88f, 0.92f});
    [[nodiscard]] static EditableMesh createPyramid(
        float base_length = 1.0f,
        float height = 1.2f,
        const math::Vec3& base_color = {0.24f, 0.68f, 0.88f});
    [[nodiscard]] static EditableMesh createGridPlane(
        float half_extent = 4.0f,
        std::uint32_t subdivisions = 4,
        const math::Vec3& color_a = {0.26f, 0.29f, 0.34f},
        const math::Vec3& color_b = {0.18f, 0.20f, 0.24f});
    [[nodiscard]] static EditableMesh createUvSphere(
        float radius = 0.75f,
        std::uint32_t rings = 16,
        std::uint32_t sectors = 24,
        const math::Vec3& base_color = {0.92f, 0.92f, 0.94f});

    // Collects unique control vertex indices referenced by current selection in `mode`.
    [[nodiscard]] std::vector<std::uint32_t> collectSelectedVertexIndices(
        EditMode mode,
        const MeshElementSelection& selection) const;

    // Computes local-space centroid of selected vertices/edges/faces.
    [[nodiscard]] std::optional<math::Vec3> computeSelectionCentroid(
        EditMode mode,
        const MeshElementSelection& selection) const;

    // -------------------------------------------------------------------------
    // Mesh Editing Operations (all preserve manifold validity or fail cleanly)
    // -------------------------------------------------------------------------
    [[nodiscard]] core::Status transformVertices(
        std::span<const std::uint32_t> vertex_indices,
        const math::Mat4& local_delta_matrix);

    [[nodiscard]] core::Status translateVertices(
        std::span<const std::uint32_t> vertex_indices,
        const math::Vec3& local_delta);

    [[nodiscard]] core::Status translateEdges(
        std::span<const std::uint32_t> edge_indices,
        const math::Vec3& local_delta);

    [[nodiscard]] core::Status translateFaces(
        std::span<const std::uint32_t> face_indices,
        const math::Vec3& local_delta);

    // Extrudes selected faces along their outward normal by `distance` and returns the cap face indices.
    [[nodiscard]] core::Result<std::vector<std::uint32_t>> extrudeFaces(
        std::span<const std::uint32_t> face_indices,
        float distance);

    [[nodiscard]] core::Status deleteFaces(std::span<const std::uint32_t> face_indices);
    [[nodiscard]] core::Status deleteVertices(std::span<const std::uint32_t> vertex_indices);
    [[nodiscard]] core::Status deleteEdges(std::span<const std::uint32_t> edge_indices);

    // Creates a new polygon face capping a closed boundary loop (filtered to `selected_vertices` if >= 3).
    [[nodiscard]] core::Result<std::uint32_t> fillBoundaryLoop(
        std::span<const std::uint32_t> selected_vertices = {});

    [[nodiscard]] core::Status assignMaterialToFaces(
        std::span<const std::uint32_t> face_indices,
        MaterialId material_id);

    void clearMaterialReferences(MaterialId removed_material_id) noexcept;

    // -------------------------------------------------------------------------
    // Viewport Raycast Picking & 3D Edit Overlay Mesh Generation
    // -------------------------------------------------------------------------
    [[nodiscard]] std::optional<std::uint32_t> pickVertex(
        const MeshData& triangulated,
        const math::Mat4& world_matrix,
        const math::Ray& world_ray,
        float world_pick_radius) const;

    [[nodiscard]] std::optional<std::uint32_t> pickEdge(
        const MeshData& triangulated,
        const math::Mat4& world_matrix,
        const math::Ray& world_ray,
        float world_pick_radius) const;

    [[nodiscard]] std::optional<std::uint32_t> pickFace(
        const MeshData& triangulated,
        const math::Mat4& world_matrix,
        const math::Ray& world_ray) const;

    [[nodiscard]] MeshData buildEditOverlayMesh(
        EditMode mode,
        const MeshElementSelection& selection,
        const math::Mat4& world_matrix,
        float world_gizmo_scale) const;

private:
    void rebuildEdgesAndNormals() noexcept;
    void pruneIsolatedVertices();

    std::vector<EditVertex> vertices_;
    std::vector<EditEdge> edges_;
    std::vector<EditFace> faces_;
};

} // namespace finaleffect::scene
