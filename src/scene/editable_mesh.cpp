// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/scene/editable_mesh.hpp"
#include "finaleffect/math/constants.hpp"
#include "finaleffect/scene/mesh.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace finaleffect::scene {

namespace {

std::uint64_t packDirectedEdge(std::uint32_t u, std::uint32_t v) noexcept {
    return (static_cast<std::uint64_t>(u) << 32u) | static_cast<std::uint64_t>(v);
}

std::uint64_t packUndirectedEdge(std::uint32_t a, std::uint32_t b) noexcept {
    const std::uint32_t lo = std::min(a, b);
    const std::uint32_t hi = std::max(a, b);
    return packDirectedEdge(lo, hi);
}

math::Vec3 computeNewellNormalUnnormalized(
    const std::vector<EditVertex>& verts,
    const std::vector<FaceCorner>& corners) noexcept {
    math::Vec3 n{0.0f, 0.0f, 0.0f};
    const std::size_t count = corners.size();
    for (std::size_t i = 0; i < count; ++i) {
        const math::Vec3& cur = verts[corners[i].vertex].position;
        const math::Vec3& nxt = verts[corners[(i + 1) % count].vertex].position;
        n.x += (cur.y - nxt.y) * (cur.z + nxt.z);
        n.y += (cur.z - nxt.z) * (cur.x + nxt.x);
        n.z += (cur.x - nxt.x) * (cur.y + nxt.y);
    }
    return n;
}

float raySegmentDistance(
    const math::Ray& ray,
    const math::Vec3& seg_a,
    const math::Vec3& seg_b,
    float& out_ray_t) noexcept {
    const math::Vec3 u = ray.direction;
    const math::Vec3 v = seg_b - seg_a;
    const math::Vec3 w = ray.origin - seg_a;

    const float a = u.dot(u);
    const float b = u.dot(v);
    const float c = v.dot(v);
    const float d = u.dot(w);
    const float e = v.dot(w);
    const float denom = a * c - b * b;

    float sc = 0.0f;
    float tc = 0.0f;
    if (c < 1.0e-9f) {
        sc = (a > 1.0e-9f) ? std::max(0.0f, -d / a) : 0.0f;
        tc = 0.0f;
    } else if (denom < 1.0e-7f) {
        tc = std::clamp(e / c, 0.0f, 1.0f);
        sc = (a > 1.0e-9f) ? std::max(0.0f, (b * tc - d) / a) : 0.0f;
    } else {
        sc = (b * e - c * d) / denom;
        tc = (a * e - b * d) / denom;
        if (sc < 0.0f) {
            sc = 0.0f;
            tc = std::clamp(e / c, 0.0f, 1.0f);
        } else if (tc < 0.0f) {
            tc = 0.0f;
            sc = std::max(0.0f, -d / a);
        } else if (tc > 1.0f) {
            tc = 1.0f;
            sc = std::max(0.0f, (b - d) / a);
        }
    }

    out_ray_t = sc;
    return (w + u * sc - v * tc).length();
}

void appendOverlayBox(
    MeshData& mesh,
    const math::Vec3& a,
    const math::Vec3& b,
    float radius,
    const math::Vec3& color) {
    const math::Vec3 delta = b - a;
    const float len = delta.length();
    if (len < 1.0e-6f) {
        return;
    }
    const math::Vec3 axis = delta * (1.0f / len);
    const math::Vec3 ref = (std::fabs(axis.y) < 0.9f) ? math::Vec3{0.0f, 1.0f, 0.0f} : math::Vec3{1.0f, 0.0f, 0.0f};
    const math::Vec3 u = axis.cross(ref).normalized() * radius;
    const math::Vec3 v = axis.cross(u).normalized() * radius;

    const math::Vec3 a0 = a - u - v;
    const math::Vec3 a1 = a + u - v;
    const math::Vec3 a2 = a + u + v;
    const math::Vec3 a3 = a - u + v;

    const math::Vec3 b0 = b - u - v;
    const math::Vec3 b1 = b + u - v;
    const math::Vec3 b2 = b + u + v;
    const math::Vec3 b3 = b - u + v;

    auto addQuad = [&](const math::Vec3& p0, const math::Vec3& p1, const math::Vec3& p2, const math::Vec3& p3) {
        const math::Vec3 n = (p1 - p0).cross(p2 - p0).normalized();
        const auto start = static_cast<std::uint32_t>(mesh.vertices.size());
        mesh.vertices.push_back({p0, n, color, {0.0f, 0.0f}});
        mesh.vertices.push_back({p1, n, color, {1.0f, 0.0f}});
        mesh.vertices.push_back({p2, n, color, {1.0f, 1.0f}});
        mesh.vertices.push_back({p3, n, color, {0.0f, 1.0f}});
        mesh.indices.insert(mesh.indices.end(), {start + 0, start + 1, start + 2, start + 0, start + 2, start + 3});
    };

    addQuad(a0, a1, b1, b0);
    addQuad(a1, a2, b2, b1);
    addQuad(a2, a3, b3, b2);
    addQuad(a3, a0, b0, b3);
}

void appendVertexDiamond(
    MeshData& mesh,
    const math::Vec3& center,
    float radius,
    const math::Vec3& color) {
    const math::Vec3 px = center + math::Vec3{radius, 0.0f, 0.0f};
    const math::Vec3 nx = center - math::Vec3{radius, 0.0f, 0.0f};
    const math::Vec3 py = center + math::Vec3{0.0f, radius, 0.0f};
    const math::Vec3 ny = center - math::Vec3{0.0f, radius, 0.0f};
    const math::Vec3 pz = center + math::Vec3{0.0f, 0.0f, radius};
    const math::Vec3 nz = center - math::Vec3{0.0f, 0.0f, radius};

    auto addTri = [&](const math::Vec3& a, const math::Vec3& b, const math::Vec3& c) {
        const math::Vec3 n = (b - a).cross(c - a).normalized();
        const auto start = static_cast<std::uint32_t>(mesh.vertices.size());
        mesh.vertices.push_back({a, n, color, {0.0f, 0.0f}});
        mesh.vertices.push_back({b, n, color, {1.0f, 0.0f}});
        mesh.vertices.push_back({c, n, color, {0.5f, 1.0f}});
        mesh.indices.insert(mesh.indices.end(), {start + 0, start + 1, start + 2});
    };

    addTri(pz, px, py);
    addTri(px, nz, py);
    addTri(nz, nx, py);
    addTri(nx, pz, py);

    addTri(px, pz, ny);
    addTri(nz, px, ny);
    addTri(nx, nz, ny);
    addTri(pz, nx, ny);
}

void toggleOrSetImpl(std::vector<std::uint32_t>& vec, std::uint32_t idx, bool additive) {
    if (!additive) {
        vec.assign(1, idx);
        return;
    }
    const auto it = std::find(vec.begin(), vec.end(), idx);
    if (it != vec.end()) {
        vec.erase(it);
    } else {
        vec.push_back(idx);
    }
}

} // namespace

void MeshElementSelection::toggleOrSetVertex(std::uint32_t idx, bool additive) {
    toggleOrSetImpl(vertices, idx, additive);
}

void MeshElementSelection::toggleOrSetEdge(std::uint32_t idx, bool additive) {
    toggleOrSetImpl(edges, idx, additive);
}

void MeshElementSelection::toggleOrSetFace(std::uint32_t idx, bool additive) {
    toggleOrSetImpl(faces, idx, additive);
}

void MeshElementSelection::sanitizeForCounts(
    std::size_t vert_count,
    std::size_t edge_count,
    std::size_t face_count) {
    std::erase_if(vertices, [&](std::uint32_t i) { return static_cast<std::size_t>(i) >= vert_count; });
    std::erase_if(edges, [&](std::uint32_t i) { return static_cast<std::size_t>(i) >= edge_count; });
    std::erase_if(faces, [&](std::uint32_t i) { return static_cast<std::size_t>(i) >= face_count; });
}

EditableMesh::EditableMesh(std::vector<EditVertex> vertices, std::vector<EditFace> faces)
    : vertices_(std::move(vertices)), faces_(std::move(faces)) {
    rebuildEdgesAndNormals();
}

core::Status EditableMesh::validateAndRebuild() {
    if (vertices_.size() < 3) {
        return core::Status::fail(
            core::ErrorCode::ValidationError,
            "EditableMesh requires at least 3 vertices");
    }
    if (faces_.empty()) {
        return core::Status::fail(
            core::ErrorCode::ValidationError,
            "EditableMesh requires at least 1 polygon face");
    }
    for (const auto& v : vertices_) {
        if (!v.position.isFinite() || !v.color.isFinite()) {
            return core::Status::fail(
                core::ErrorCode::ValidationError,
                "EditableMesh contains non-finite vertex attributes");
        }
    }

    std::unordered_set<std::uint64_t> directed_half_edges;
    std::unordered_map<std::uint64_t, std::size_t> undirected_incidence;

    for (std::size_t fi = 0; fi < faces_.size(); ++fi) {
        const auto& f = faces_[fi];
        if (f.corners.size() < 3) {
            return core::Status::fail(
                core::ErrorCode::ValidationError,
                "Polygon face has fewer than 3 corners");
        }

        std::unordered_set<std::uint32_t> seen_in_face;
        for (std::size_t ci = 0; ci < f.corners.size(); ++ci) {
            const std::uint32_t u = f.corners[ci].vertex;
            const std::uint32_t v = f.corners[(ci + 1) % f.corners.size()].vertex;
            if (static_cast<std::size_t>(u) >= vertices_.size()) {
                return core::Status::fail(
                    core::ErrorCode::ValidationError,
                    "Polygon face corner vertex index exceeds vertex array bounds");
            }
            if (!f.corners[ci].uv.isFinite()) {
                return core::Status::fail(
                    core::ErrorCode::ValidationError,
                    "Polygon face corner UV is non-finite");
            }
            if (!seen_in_face.insert(u).second) {
                return core::Status::fail(
                    core::ErrorCode::ValidationError,
                    "Polygon face contains duplicate/self-pinching vertex index");
            }

            const std::uint64_t dir_key = packDirectedEdge(u, v);
            if (!directed_half_edges.insert(dir_key).second) {
                return core::Status::fail(
                    core::ErrorCode::ValidationError,
                    "Non-manifold or inconsistently wound duplicate half-edge detected");
            }

            const std::uint64_t undir_key = packUndirectedEdge(u, v);
            if (++undirected_incidence[undir_key] > 2) {
                return core::Status::fail(
                    core::ErrorCode::ValidationError,
                    "Non-manifold edge shared by more than 2 faces");
            }
        }

        const math::Vec3 newell = computeNewellNormalUnnormalized(vertices_, f.corners);
        if (newell.lengthSquared() <= 1.0e-12f) {
            return core::Status::fail(
                core::ErrorCode::ValidationError,
                "Degenerate zero-area or collinear polygon face");
        }
    }

    rebuildEdgesAndNormals();
    return core::Status::ok();
}

void EditableMesh::rebuildEdgesAndNormals() noexcept {
    for (auto& v : vertices_) {
        v.normal = {0.0f, 0.0f, 0.0f};
    }

    std::map<std::uint64_t, EditEdge> edge_map;

    for (std::size_t fi = 0; fi < faces_.size(); ++fi) {
        auto& f = faces_[fi];
        if (f.corners.size() < 3) {
            continue;
        }
        const math::Vec3 newell = computeNewellNormalUnnormalized(vertices_, f.corners);
        const float area_len = newell.length();
        f.normal = (area_len > 1.0e-7f) ? (newell * (1.0f / area_len)) : math::Vec3{0.0f, 1.0f, 0.0f};

        for (std::size_t ci = 0; ci < f.corners.size(); ++ci) {
            const std::uint32_t u = f.corners[ci].vertex;
            const std::uint32_t v = f.corners[(ci + 1) % f.corners.size()].vertex;
            if (static_cast<std::size_t>(u) < vertices_.size()) {
                vertices_[u].normal += newell; // Area-weighted accumulation
            }
            const std::uint64_t key = packUndirectedEdge(u, v);
            auto [it, inserted] = edge_map.try_emplace(
                key,
                EditEdge{.v0 = std::min(u, v), .v1 = std::max(u, v), .adjacent_faces = {}});
            (void)inserted;
            it->second.adjacent_faces.push_back(static_cast<std::uint32_t>(fi));
        }
    }

    for (auto& v : vertices_) {
        if (v.normal.lengthSquared() > 1.0e-10f) {
            v.normal = v.normal.normalized();
        } else {
            v.normal = {0.0f, 1.0f, 0.0f};
        }
    }

    edges_.clear();
    edges_.reserve(edge_map.size());
    for (auto& [key, edge] : edge_map) {
        (void)key;
        edges_.push_back(std::move(edge));
    }
}

void EditableMesh::pruneIsolatedVertices() {
    std::vector<bool> used(vertices_.size(), false);
    for (const auto& f : faces_) {
        for (const auto& c : f.corners) {
            if (static_cast<std::size_t>(c.vertex) < used.size()) {
                used[c.vertex] = true;
            }
        }
    }

    std::vector<std::uint32_t> remap(vertices_.size(), 0);
    std::vector<EditVertex> compacted;
    compacted.reserve(vertices_.size());
    for (std::size_t i = 0; i < vertices_.size(); ++i) {
        if (used[i]) {
            remap[i] = static_cast<std::uint32_t>(compacted.size());
            compacted.push_back(vertices_[i]);
        }
    }

    for (auto& f : faces_) {
        for (auto& c : f.corners) {
            c.vertex = remap[c.vertex];
        }
    }
    vertices_ = std::move(compacted);
}

MeshData EditableMesh::toMeshData() const {
    MeshData out;
    if (vertices_.empty() || faces_.empty()) {
        return out;
    }

    // Group faces deterministically by MaterialId so multi-material meshes emit clean SubmeshRange slices.
    std::vector<std::size_t> face_order(faces_.size());
    for (std::size_t i = 0; i < faces_.size(); ++i) {
        face_order[i] = i;
    }
    std::stable_sort(face_order.begin(), face_order.end(), [&](std::size_t a, std::size_t b) {
        return faces_[a].material_id.value < faces_[b].material_id.value;
    });

    out.vertices.reserve(faces_.size() * 4u);
    out.indices.reserve(faces_.size() * 6u);
    out.triangle_face_ids.reserve(faces_.size() * 2u);

    std::optional<MaterialId> current_mat;
    std::uint32_t submesh_start = 0;

    for (const std::size_t fi : face_order) {
        const auto& f = faces_[fi];
        if (f.corners.size() < 3) {
            continue;
        }

        if (!current_mat.has_value() || current_mat.value() != f.material_id) {
            if (current_mat.has_value()) {
                const auto cur_end = static_cast<std::uint32_t>(out.indices.size());
                if (cur_end > submesh_start) {
                    out.submeshes.push_back(SubmeshRange{
                        .index_offset = submesh_start,
                        .index_count = cur_end - submesh_start,
                        .material_id = current_mat.value(),
                    });
                }
            }
            current_mat = f.material_id;
            submesh_start = static_cast<std::uint32_t>(out.indices.size());
        }

        const auto base_v = static_cast<std::uint32_t>(out.vertices.size());
        for (const auto& corner : f.corners) {
            const auto& ev = vertices_[corner.vertex];
            const math::Vec3 n = f.smooth_shading ? ev.normal : f.normal;
            const math::Vec3 col = ev.color.cwiseProduct(f.color);
            out.vertices.push_back(Vertex{
                .position = ev.position,
                .normal = n,
                .color = col,
                .uv = corner.uv,
            });
        }

        for (std::size_t k = 1; k + 1 < f.corners.size(); ++k) {
            out.indices.push_back(base_v);
            out.indices.push_back(base_v + static_cast<std::uint32_t>(k));
            out.indices.push_back(base_v + static_cast<std::uint32_t>(k + 1));
            out.triangle_face_ids.push_back(static_cast<std::uint32_t>(fi));
        }
    }

    if (current_mat.has_value()) {
        const auto cur_end = static_cast<std::uint32_t>(out.indices.size());
        if (cur_end > submesh_start) {
            out.submeshes.push_back(SubmeshRange{
                .index_offset = submesh_start,
                .index_count = cur_end - submesh_start,
                .material_id = current_mat.value(),
            });
        }
    }

    return out;
}

core::Result<EditableMesh> EditableMesh::fromMeshData(const MeshData& tri_mesh) {
    auto val_st = tri_mesh.validate();
    if (!val_st) {
        return val_st.error();
    }

    // Weld vertices sharing identical 3D positions (quantized to 1e-4 grid) while avoiding pinched faces.
    struct QuantizedPos {
        std::int64_t x{0};
        std::int64_t y{0};
        std::int64_t z{0};
        auto operator<=>(const QuantizedPos&) const noexcept = default;
    };

    std::map<QuantizedPos, std::uint32_t> pos_to_welded;
    std::vector<EditVertex> welded_verts;
    std::vector<std::uint32_t> v_remap(tri_mesh.vertices.size(), 0);

    for (std::size_t i = 0; i < tri_mesh.vertices.size(); ++i) {
        const auto& v = tri_mesh.vertices[i];
        const QuantizedPos qp{
            static_cast<std::int64_t>(std::llround(v.position.x * 10000.0f)),
            static_cast<std::int64_t>(std::llround(v.position.y * 10000.0f)),
            static_cast<std::int64_t>(std::llround(v.position.z * 10000.0f)),
        };
        auto [it, inserted] = pos_to_welded.try_emplace(qp, static_cast<std::uint32_t>(welded_verts.size()));
        if (inserted) {
            welded_verts.push_back(EditVertex{
                .position = v.position,
                .normal = v.normal,
                .color = {1.0f, 1.0f, 1.0f},
            });
        }
        v_remap[i] = it->second;
    }

    std::vector<EditFace> faces;
    const std::size_t tri_count = tri_mesh.indices.size() / 3u;
    faces.reserve(tri_count);

    std::unordered_set<std::uint64_t> used_half_edges;
    for (std::size_t t = 0; t < tri_count; ++t) {
        const std::uint32_t i0 = tri_mesh.indices[t * 3u + 0];
        const std::uint32_t i1 = tri_mesh.indices[t * 3u + 1];
        const std::uint32_t i2 = tri_mesh.indices[t * 3u + 2];

        std::uint32_t w0 = v_remap[i0];
        std::uint32_t w1 = v_remap[i1];
        std::uint32_t w2 = v_remap[i2];
        if (w0 == w1 || w1 == w2 || w2 == w0) {
            continue;
        }

        // Guard against duplicate directed half-edges in double-sided triangle soups.
        if (used_half_edges.contains(packDirectedEdge(w0, w1)) ||
            used_half_edges.contains(packDirectedEdge(w1, w2)) ||
            used_half_edges.contains(packDirectedEdge(w2, w0))) {
            w0 = static_cast<std::uint32_t>(welded_verts.size());
            welded_verts.push_back({tri_mesh.vertices[i0].position, tri_mesh.vertices[i0].normal, {1.0f, 1.0f, 1.0f}});
            w1 = static_cast<std::uint32_t>(welded_verts.size());
            welded_verts.push_back({tri_mesh.vertices[i1].position, tri_mesh.vertices[i1].normal, {1.0f, 1.0f, 1.0f}});
            w2 = static_cast<std::uint32_t>(welded_verts.size());
            welded_verts.push_back({tri_mesh.vertices[i2].position, tri_mesh.vertices[i2].normal, {1.0f, 1.0f, 1.0f}});
        }

        used_half_edges.insert(packDirectedEdge(w0, w1));
        used_half_edges.insert(packDirectedEdge(w1, w2));
        used_half_edges.insert(packDirectedEdge(w2, w0));

        EditFace f;
        f.corners = {
            FaceCorner{w0, tri_mesh.vertices[i0].uv},
            FaceCorner{w1, tri_mesh.vertices[i1].uv},
            FaceCorner{w2, tri_mesh.vertices[i2].uv},
        };
        f.color = (tri_mesh.vertices[i0].color + tri_mesh.vertices[i1].color + tri_mesh.vertices[i2].color) * (1.0f / 3.0f);
        faces.push_back(std::move(f));
    }

    EditableMesh result(std::move(welded_verts), std::move(faces));
    auto st = result.validateAndRebuild();
    if (!st) {
        return st.error();
    }
    return core::Result<EditableMesh>::ok(std::move(result));
}

EditableMesh EditableMesh::createCube(float side_length, const math::Vec3& base_color) {
    const float h = 0.5f * std::max(side_length, 1.0e-4f);
    std::vector<EditVertex> verts{
        {{-h, -h,  h}, {}, {1.0f, 1.0f, 1.0f}}, // 0: front-bottom-left
        {{ h, -h,  h}, {}, {1.0f, 1.0f, 1.0f}}, // 1: front-bottom-right
        {{ h,  h,  h}, {}, {1.0f, 1.0f, 1.0f}}, // 2: front-top-right
        {{-h,  h,  h}, {}, {1.0f, 1.0f, 1.0f}}, // 3: front-top-left
        {{-h, -h, -h}, {}, {1.0f, 1.0f, 1.0f}}, // 4: back-bottom-left
        {{ h, -h, -h}, {}, {1.0f, 1.0f, 1.0f}}, // 5: back-bottom-right
        {{ h,  h, -h}, {}, {1.0f, 1.0f, 1.0f}}, // 6: back-top-right
        {{-h,  h, -h}, {}, {1.0f, 1.0f, 1.0f}}, // 7: back-top-left
    };

    auto makeQuad = [&](std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t d, float shade) {
        EditFace f;
        f.corners = {
            {a, {0.0f, 0.0f}},
            {b, {1.0f, 0.0f}},
            {c, {1.0f, 1.0f}},
            {d, {0.0f, 1.0f}},
        };
        f.color = {
            std::clamp(base_color.x * shade, 0.0f, 1.0f),
            std::clamp(base_color.y * shade, 0.0f, 1.0f),
            std::clamp(base_color.z * shade, 0.0f, 1.0f),
        };
        return f;
    };

    std::vector<EditFace> faces{
        makeQuad(0, 1, 2, 3, 1.00f), // +Z front
        makeQuad(5, 4, 7, 6, 0.92f), // -Z back
        makeQuad(1, 5, 6, 2, 0.96f), // +X right
        makeQuad(4, 0, 3, 7, 0.88f), // -X left
        makeQuad(3, 2, 6, 7, 1.04f), // +Y top
        makeQuad(4, 5, 1, 0, 0.82f), // -Y bottom
    };
    return EditableMesh(std::move(verts), std::move(faces));
}

EditableMesh EditableMesh::createPyramid(float base_length, float height, const math::Vec3& base_color) {
    const float h = 0.5f * std::max(base_length, 1.0e-4f);
    const float top_y = 0.5f * std::max(height, 1.0e-4f);
    const float bot_y = -top_y;

    std::vector<EditVertex> verts{
        {{-h, bot_y,  h}, {}, {1.0f, 1.0f, 1.0f}}, // 0: bl
        {{ h, bot_y,  h}, {}, {1.0f, 1.0f, 1.0f}}, // 1: br
        {{ h, bot_y, -h}, {}, {1.0f, 1.0f, 1.0f}}, // 2: fr
        {{-h, bot_y, -h}, {}, {1.0f, 1.0f, 1.0f}}, // 3: fl
        {{0.0f, top_y, 0.0f}, {}, {1.0f, 1.0f, 1.0f}}, // 4: apex
    };

    auto makeTri = [&](std::uint32_t a, std::uint32_t b, std::uint32_t c) {
        EditFace f;
        f.corners = {{a, {0.0f, 0.0f}}, {b, {1.0f, 0.0f}}, {c, {0.5f, 1.0f}}};
        f.color = base_color;
        return f;
    };

    EditFace base_quad;
    base_quad.corners = {{3, {0.0f, 0.0f}}, {2, {1.0f, 0.0f}}, {1, {1.0f, 1.0f}}, {0, {0.0f, 1.0f}}};
    base_quad.color = base_color * 0.85f;

    std::vector<EditFace> faces{
        makeTri(0, 1, 4),
        makeTri(1, 2, 4),
        makeTri(2, 3, 4),
        makeTri(3, 0, 4),
        std::move(base_quad),
    };
    return EditableMesh(std::move(verts), std::move(faces));
}

EditableMesh EditableMesh::createGridPlane(
    float half_extent,
    std::uint32_t subdivisions,
    const math::Vec3& color_a,
    const math::Vec3& color_b) {
    const float ext = std::max(half_extent, 0.1f);
    const std::uint32_t cells = std::max(subdivisions, 1u);
    const std::uint32_t stride = cells + 1u;
    const float step = (2.0f * ext) / static_cast<float>(cells);

    std::vector<EditVertex> verts;
    verts.reserve(static_cast<std::size_t>(stride) * stride);
    for (std::uint32_t gz = 0; gz <= cells; ++gz) {
        for (std::uint32_t gx = 0; gx <= cells; ++gx) {
            verts.push_back(EditVertex{
                .position = {-ext + static_cast<float>(gx) * step, 0.0f, -ext + static_cast<float>(gz) * step},
                .normal = {0.0f, 1.0f, 0.0f},
                .color = {1.0f, 1.0f, 1.0f},
            });
        }
    }

    std::vector<EditFace> faces;
    faces.reserve(static_cast<std::size_t>(cells) * cells);
    for (std::uint32_t gz = 0; gz < cells; ++gz) {
        for (std::uint32_t gx = 0; gx < cells; ++gx) {
            const std::uint32_t v00 = gz * stride + gx;
            const std::uint32_t v10 = gz * stride + (gx + 1u);
            const std::uint32_t v01 = (gz + 1u) * stride + gx;
            const std::uint32_t v11 = (gz + 1u) * stride + (gx + 1u);

            EditFace f;
            // Outward +Y normal (CCW looking from +Y down): v01 -> v11 -> v10 -> v00.
            f.corners = {
                {v01, {0.0f, 0.0f}},
                {v11, {1.0f, 0.0f}},
                {v10, {1.0f, 1.0f}},
                {v00, {0.0f, 1.0f}},
            };
            f.color = ((gx + gz) % 2u == 0u) ? color_a : color_b;
            faces.push_back(std::move(f));
        }
    }
    return EditableMesh(std::move(verts), std::move(faces));
}

EditableMesh EditableMesh::createUvSphere(
    float radius,
    std::uint32_t rings,
    std::uint32_t sectors,
    const math::Vec3& base_color) {
    const float r = std::max(radius, 0.05f);
    const std::uint32_t ring_count = std::max(rings, 4u);
    const std::uint32_t sec_count = std::max(sectors, 6u);

    std::vector<EditVertex> verts;
    verts.push_back(EditVertex{{0.0f, r, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}}); // 0: north pole

    for (std::uint32_t i = 1; i < ring_count; ++i) {
        const float phi = (static_cast<float>(i) / static_cast<float>(ring_count)) * math::kPi;
        const float y = std::cos(phi) * r;
        const float ring_r = std::sin(phi) * r;
        for (std::uint32_t j = 0; j < sec_count; ++j) {
            const float theta = (static_cast<float>(j) / static_cast<float>(sec_count)) * math::kTwoPi;
            const math::Vec3 pos{std::sin(theta) * ring_r, y, std::cos(theta) * ring_r};
            verts.push_back(EditVertex{pos, pos.normalized(), {1.0f, 1.0f, 1.0f}});
        }
    }
    const auto south_idx = static_cast<std::uint32_t>(verts.size());
    verts.push_back(EditVertex{{0.0f, -r, 0.0f}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}});

    std::vector<EditFace> faces;
    // North polar fan
    for (std::uint32_t j = 0; j < sec_count; ++j) {
        const std::uint32_t next_j = (j + 1u) % sec_count;
        EditFace f;
        f.corners = {{0u, {0.5f, 1.0f}}, {1u + j, {0.0f, 0.0f}}, {1u + next_j, {1.0f, 0.0f}}};
        f.color = base_color;
        f.smooth_shading = true;
        faces.push_back(std::move(f));
    }

    // Middle latitude quads
    for (std::uint32_t i = 0; i + 2u < ring_count; ++i) {
        const std::uint32_t row0 = 1u + i * sec_count;
        const std::uint32_t row1 = row0 + sec_count;
        for (std::uint32_t j = 0; j < sec_count; ++j) {
            const std::uint32_t next_j = (j + 1u) % sec_count;
            EditFace f;
            f.corners = {
                {row0 + j, {0.0f, 1.0f}},
                {row1 + j, {0.0f, 0.0f}},
                {row1 + next_j, {1.0f, 0.0f}},
                {row0 + next_j, {1.0f, 1.0f}},
            };
            f.color = base_color;
            f.smooth_shading = true;
            faces.push_back(std::move(f));
        }
    }

    // South polar fan
    const std::uint32_t last_row = 1u + (ring_count - 2u) * sec_count;
    for (std::uint32_t j = 0; j < sec_count; ++j) {
        const std::uint32_t next_j = (j + 1u) % sec_count;
        EditFace f;
        f.corners = {
            {last_row + j, {0.0f, 1.0f}},
            {south_idx, {0.5f, 0.0f}},
            {last_row + next_j, {1.0f, 1.0f}},
        };
        f.color = base_color;
        f.smooth_shading = true;
        faces.push_back(std::move(f));
    }

    return EditableMesh(std::move(verts), std::move(faces));
}

std::vector<std::uint32_t> EditableMesh::collectSelectedVertexIndices(
    EditMode mode,
    const MeshElementSelection& selection) const {
    std::set<std::uint32_t> unique_verts;
    if (mode == EditMode::Vertex) {
        for (const std::uint32_t vi : selection.vertices) {
            if (static_cast<std::size_t>(vi) < vertices_.size()) {
                unique_verts.insert(vi);
            }
        }
    } else if (mode == EditMode::Edge) {
        for (const std::uint32_t ei : selection.edges) {
            if (static_cast<std::size_t>(ei) < edges_.size()) {
                unique_verts.insert(edges_[ei].v0);
                unique_verts.insert(edges_[ei].v1);
            }
        }
    } else if (mode == EditMode::Face) {
        for (const std::uint32_t fi : selection.faces) {
            if (static_cast<std::size_t>(fi) < faces_.size()) {
                for (const auto& c : faces_[fi].corners) {
                    unique_verts.insert(c.vertex);
                }
            }
        }
    }
    return {unique_verts.begin(), unique_verts.end()};
}

std::optional<math::Vec3> EditableMesh::computeSelectionCentroid(
    EditMode mode,
    const MeshElementSelection& selection) const {
    const auto verts = collectSelectedVertexIndices(mode, selection);
    if (verts.empty()) {
        return std::nullopt;
    }
    math::Vec3 sum{0.0f, 0.0f, 0.0f};
    for (const std::uint32_t vi : verts) {
        sum += vertices_[vi].position;
    }
    return sum * (1.0f / static_cast<float>(verts.size()));
}

core::Status EditableMesh::transformVertices(
    std::span<const std::uint32_t> vertex_indices,
    const math::Mat4& local_delta_matrix) {
    if (vertex_indices.empty()) {
        return core::Status::fail(core::ErrorCode::InvalidArgument, "No vertices selected for transformation");
    }

    EditableMesh candidate = *this;
    std::unordered_set<std::uint32_t> seen;
    for (const std::uint32_t vi : vertex_indices) {
        if (static_cast<std::size_t>(vi) >= candidate.vertices_.size()) {
            return core::Status::fail(core::ErrorCode::InvalidArgument, "Vertex index out of bounds");
        }
        if (seen.insert(vi).second) {
            candidate.vertices_[vi].position = local_delta_matrix.transformPoint(candidate.vertices_[vi].position);
        }
    }

    auto st = candidate.validateAndRebuild();
    if (!st) {
        return st;
    }
    *this = std::move(candidate);
    return core::Status::ok();
}

core::Status EditableMesh::translateVertices(
    std::span<const std::uint32_t> vertex_indices,
    const math::Vec3& local_delta) {
    if (!local_delta.isFinite()) {
        return core::Status::fail(core::ErrorCode::InvalidArgument, "Vertex translation delta must be finite");
    }
    return transformVertices(vertex_indices, math::Mat4::translation(local_delta));
}

core::Status EditableMesh::translateEdges(
    std::span<const std::uint32_t> edge_indices,
    const math::Vec3& local_delta) {
    if (edge_indices.empty()) {
        return core::Status::fail(core::ErrorCode::InvalidArgument, "No edges selected for translation");
    }
    std::set<std::uint32_t> verts;
    for (const std::uint32_t ei : edge_indices) {
        if (static_cast<std::size_t>(ei) >= edges_.size()) {
            return core::Status::fail(core::ErrorCode::InvalidArgument, "Edge index out of bounds");
        }
        verts.insert(edges_[ei].v0);
        verts.insert(edges_[ei].v1);
    }
    const std::vector<std::uint32_t> vert_list(verts.begin(), verts.end());
    return translateVertices(vert_list, local_delta);
}

core::Status EditableMesh::translateFaces(
    std::span<const std::uint32_t> face_indices,
    const math::Vec3& local_delta) {
    if (face_indices.empty()) {
        return core::Status::fail(core::ErrorCode::InvalidArgument, "No faces selected for translation");
    }
    std::set<std::uint32_t> verts;
    for (const std::uint32_t fi : face_indices) {
        if (static_cast<std::size_t>(fi) >= faces_.size()) {
            return core::Status::fail(core::ErrorCode::InvalidArgument, "Face index out of bounds");
        }
        for (const auto& c : faces_[fi].corners) {
            verts.insert(c.vertex);
        }
    }
    const std::vector<std::uint32_t> vert_list(verts.begin(), verts.end());
    return translateVertices(vert_list, local_delta);
}

core::Result<std::vector<std::uint32_t>> EditableMesh::extrudeFaces(
    std::span<const std::uint32_t> face_indices,
    float distance) {
    if (face_indices.empty()) {
        return core::Result<std::vector<std::uint32_t>>::fail(
            core::ErrorCode::InvalidArgument, "Select at least one face to extrude");
    }
    if (!std::isfinite(distance) || std::fabs(distance) < 1.0e-5f) {
        return core::Result<std::vector<std::uint32_t>>::fail(
            core::ErrorCode::InvalidArgument, "Extrusion distance must be finite and non-zero");
    }

    std::set<std::uint32_t> selected_set;
    for (const std::uint32_t fi : face_indices) {
        if (static_cast<std::size_t>(fi) >= faces_.size()) {
            return core::Result<std::vector<std::uint32_t>>::fail(
                core::ErrorCode::InvalidArgument, "Extrude face index out of bounds");
        }
        selected_set.insert(fi);
    }

    EditableMesh candidate = *this;

    // Collect directed half-edges of selected faces to locate the region boundary.
    std::unordered_set<std::uint64_t> region_half_edges;
    math::Vec3 avg_normal{0.0f, 0.0f, 0.0f};
    std::set<std::uint32_t> region_verts;

    for (const std::uint32_t fi : selected_set) {
        const auto& f = candidate.faces_[fi];
        avg_normal += f.normal;
        for (std::size_t ci = 0; ci < f.corners.size(); ++ci) {
            const std::uint32_t u = f.corners[ci].vertex;
            const std::uint32_t v = f.corners[(ci + 1) % f.corners.size()].vertex;
            region_verts.insert(u);
            region_half_edges.insert(packDirectedEdge(u, v));
        }
    }

    struct BoundaryHalfEdge {
        std::uint32_t u;
        std::uint32_t v;
        math::Vec3 color;
        MaterialId material_id;
    };
    std::vector<BoundaryHalfEdge> boundary;
    for (const std::uint32_t fi : selected_set) {
        const auto& f = candidate.faces_[fi];
        for (std::size_t ci = 0; ci < f.corners.size(); ++ci) {
            const std::uint32_t u = f.corners[ci].vertex;
            const std::uint32_t v = f.corners[(ci + 1) % f.corners.size()].vertex;
            if (!region_half_edges.contains(packDirectedEdge(v, u))) {
                boundary.push_back(BoundaryHalfEdge{u, v, f.color, f.material_id});
            }
        }
    }

    if (boundary.empty()) {
        return core::Result<std::vector<std::uint32_t>>::fail(
            core::ErrorCode::ValidationError,
            "Cannot extrude a closed shell with no boundary edges; select a subset of faces");
    }

    const math::Vec3 extrude_dir = (avg_normal.lengthSquared() > 1.0e-8f)
                                       ? avg_normal.normalized()
                                       : candidate.faces_[*selected_set.begin()].normal;

    // Duplicate vertices belonging to the extruded region and offset along `extrude_dir * distance`.
    std::unordered_map<std::uint32_t, std::uint32_t> cap_vert_map;
    for (const std::uint32_t old_v : region_verts) {
        const auto new_idx = static_cast<std::uint32_t>(candidate.vertices_.size());
        EditVertex dup = candidate.vertices_[old_v];
        dup.position += extrude_dir * distance;
        candidate.vertices_.push_back(dup);
        cap_vert_map.emplace(old_v, new_idx);
    }

    // Rewire selected cap faces to the newly offset cap vertices.
    for (const std::uint32_t fi : selected_set) {
        for (auto& corner : candidate.faces_[fi].corners) {
            corner.vertex = cap_vert_map.at(corner.vertex);
        }
    }

    // Stitch outward-wound side wall quads along every boundary half-edge (u -> v).
    for (const auto& bh : boundary) {
        EditFace wall;
        wall.corners = {
            {bh.u, {0.0f, 0.0f}},
            {bh.v, {1.0f, 0.0f}},
            {cap_vert_map.at(bh.v), {1.0f, 1.0f}},
            {cap_vert_map.at(bh.u), {0.0f, 1.0f}},
        };
        wall.color = bh.color * 0.94f;
        wall.material_id = bh.material_id;
        candidate.faces_.push_back(std::move(wall));
    }

    // Note: if multiple adjacent faces were extruded together, interior vertices of the old region
    // that have no unselected faces attached become isolated; prune them cleanly!
    // Because pruning only removes unreferenced old vertices while cap faces keep their face indices,
    // `selected_set` face indices remain unchanged!
    candidate.pruneIsolatedVertices();

    auto st = candidate.validateAndRebuild();
    if (!st) {
        return st.error();
    }

    *this = std::move(candidate);
    return core::Result<std::vector<std::uint32_t>>::ok(
        std::vector<std::uint32_t>(selected_set.begin(), selected_set.end()));
}

core::Status EditableMesh::deleteFaces(std::span<const std::uint32_t> face_indices) {
    if (face_indices.empty()) {
        return core::Status::fail(core::ErrorCode::InvalidArgument, "No faces selected to delete");
    }

    std::vector<bool> drop(faces_.size(), false);
    for (const std::uint32_t fi : face_indices) {
        if (static_cast<std::size_t>(fi) >= faces_.size()) {
            return core::Status::fail(core::ErrorCode::InvalidArgument, "Face index out of bounds");
        }
        drop[fi] = true;
    }

    EditableMesh candidate = *this;
    std::vector<EditFace> kept;
    kept.reserve(faces_.size());
    for (std::size_t i = 0; i < candidate.faces_.size(); ++i) {
        if (!drop[i]) {
            kept.push_back(std::move(candidate.faces_[i]));
        }
    }
    if (kept.empty()) {
        return core::Status::fail(
            core::ErrorCode::ValidationError,
            "Cannot delete all faces of a mesh; delete the scene object instead");
    }

    candidate.faces_ = std::move(kept);
    candidate.pruneIsolatedVertices();

    auto st = candidate.validateAndRebuild();
    if (!st) {
        return st;
    }
    *this = std::move(candidate);
    return core::Status::ok();
}

core::Status EditableMesh::deleteVertices(std::span<const std::uint32_t> vertex_indices) {
    if (vertex_indices.empty()) {
        return core::Status::fail(core::ErrorCode::InvalidArgument, "No vertices selected to delete");
    }

    std::unordered_set<std::uint32_t> killed;
    for (const std::uint32_t vi : vertex_indices) {
        if (static_cast<std::size_t>(vi) >= vertices_.size()) {
            return core::Status::fail(core::ErrorCode::InvalidArgument, "Vertex index out of bounds");
        }
        killed.insert(vi);
    }

    EditableMesh candidate = *this;
    std::erase_if(candidate.faces_, [&](const EditFace& f) {
        for (const auto& c : f.corners) {
            if (killed.contains(c.vertex)) {
                return true;
            }
        }
        return false;
    });

    if (candidate.faces_.empty()) {
        return core::Status::fail(
            core::ErrorCode::ValidationError,
            "Deleting selected vertices would leave zero valid polygon faces");
    }

    candidate.pruneIsolatedVertices();
    auto st = candidate.validateAndRebuild();
    if (!st) {
        return st;
    }
    *this = std::move(candidate);
    return core::Status::ok();
}

core::Status EditableMesh::deleteEdges(std::span<const std::uint32_t> edge_indices) {
    if (edge_indices.empty()) {
        return core::Status::fail(core::ErrorCode::InvalidArgument, "No edges selected to delete");
    }

    EditableMesh candidate = *this;

    // Single interior edge shared by two faces: attempt topological edge dissolve (polygon merge) first!
    if (edge_indices.size() == 1) {
        const std::uint32_t ei = edge_indices[0];
        if (static_cast<std::size_t>(ei) >= candidate.edges_.size()) {
            return core::Status::fail(core::ErrorCode::InvalidArgument, "Edge index out of bounds");
        }
        const auto& edge = candidate.edges_[ei];
        if (edge.adjacent_faces.size() == 2) {
            const std::uint32_t fa_idx = edge.adjacent_faces[0];
            const std::uint32_t fb_idx = edge.adjacent_faces[1];
            const auto& fa = candidate.faces_[fa_idx];
            const auto& fb = candidate.faces_[fb_idx];

            // Find directed half-edge (u -> v) in fa and (v -> u) in fb.
            std::optional<std::size_t> pos_a;
            std::optional<std::size_t> pos_b;
            for (std::size_t i = 0; i < fa.corners.size(); ++i) {
                const std::uint32_t u = fa.corners[i].vertex;
                const std::uint32_t v = fa.corners[(i + 1) % fa.corners.size()].vertex;
                if ((u == edge.v0 && v == edge.v1) || (u == edge.v1 && v == edge.v0)) {
                    pos_a = i;
                    for (std::size_t j = 0; j < fb.corners.size(); ++j) {
                        const std::uint32_t bu = fb.corners[j].vertex;
                        const std::uint32_t bv = fb.corners[(j + 1) % fb.corners.size()].vertex;
                        if (bu == v && bv == u) {
                            pos_b = j;
                            break;
                        }
                    }
                    break;
                }
            }

            if (pos_a.has_value() && pos_b.has_value()) {
                EditFace merged;
                merged.color = (fa.color + fb.color) * 0.5f;
                merged.material_id = fa.material_id;
                merged.smooth_shading = fa.smooth_shading;

                // Walk fa from (pos_a + 1) .. pos_a, splicing fb's exterior corners between u and v.
                const std::size_t na = fa.corners.size();
                const std::size_t nb = fb.corners.size();
                for (std::size_t step = 1; step <= na - 1; ++step) {
                    merged.corners.push_back(fa.corners[(pos_a.value() + step) % na]);
                }
                for (std::size_t step = 1; step <= nb - 1; ++step) {
                    merged.corners.push_back(fb.corners[(pos_b.value() + step) % nb]);
                }

                EditableMesh dissolved = candidate;
                const std::uint32_t hi = std::max(fa_idx, fb_idx);
                const std::uint32_t lo = std::min(fa_idx, fb_idx);
                dissolved.faces_.erase(dissolved.faces_.begin() + static_cast<std::ptrdiff_t>(hi));
                dissolved.faces_[lo] = std::move(merged);
                dissolved.pruneIsolatedVertices();
                if (dissolved.validateAndRebuild().hasValue()) {
                    *this = std::move(dissolved);
                    return core::Status::ok();
                }
            }
        }
    }

    // General edge deletion: remove incident faces of selected edges if valid geometry remains.
    std::set<std::uint32_t> incident_faces;
    for (const std::uint32_t ei : edge_indices) {
        if (static_cast<std::size_t>(ei) >= edges_.size()) {
            return core::Status::fail(core::ErrorCode::InvalidArgument, "Edge index out of bounds");
        }
        for (const std::uint32_t fi : edges_[ei].adjacent_faces) {
            incident_faces.insert(fi);
        }
    }
    const std::vector<std::uint32_t> faces_to_remove(incident_faces.begin(), incident_faces.end());
    return deleteFaces(faces_to_remove);
}

core::Result<std::uint32_t> EditableMesh::fillBoundaryLoop(
    std::span<const std::uint32_t> selected_vertices) {
    std::unordered_set<std::uint32_t> filter_verts(selected_vertices.begin(), selected_vertices.end());
    const bool use_filter = filter_verts.size() >= 3;

    // Collect existing directed half-edges across all faces.
    std::unordered_set<std::uint64_t> existing_half_edges;
    for (const auto& f : faces_) {
        for (std::size_t ci = 0; ci < f.corners.size(); ++ci) {
            existing_half_edges.insert(
                packDirectedEdge(f.corners[ci].vertex, f.corners[(ci + 1) % f.corners.size()].vertex));
        }
    }

    // Every boundary half-edge (u -> v) in an existing face requires its opposite (v -> u) in the new cap face!
    std::unordered_map<std::uint32_t, std::vector<std::uint32_t>> next_boundary;
    for (const auto& f : faces_) {
        for (std::size_t ci = 0; ci < f.corners.size(); ++ci) {
            const std::uint32_t u = f.corners[ci].vertex;
            const std::uint32_t v = f.corners[(ci + 1) % f.corners.size()].vertex;
            if (!existing_half_edges.contains(packDirectedEdge(v, u))) {
                if (!use_filter || (filter_verts.contains(u) && filter_verts.contains(v))) {
                    next_boundary[v].push_back(u);
                }
            }
        }
    }

    if (next_boundary.empty()) {
        return core::Result<std::uint32_t>::fail(
            core::ErrorCode::ValidationError,
            "No open boundary loop found to fill");
    }

    // Trace a simple closed cycle along `next_boundary`.
    std::vector<std::uint32_t> best_loop;
    for (const auto& [start_v, outs] : next_boundary) {
        (void)outs;
        std::vector<std::uint32_t> path{start_v};
        std::unordered_set<std::uint32_t> visited{start_v};
        std::uint32_t cur = start_v;

        while (true) {
            const auto it = next_boundary.find(cur);
            if (it == next_boundary.end() || it->second.empty()) {
                break;
            }
            std::optional<std::uint32_t> chosen;
            for (const std::uint32_t cand : it->second) {
                if (cand == start_v && path.size() >= 3) {
                    chosen = cand;
                    break;
                }
                if (!visited.contains(cand)) {
                    chosen = cand;
                    break;
                }
            }
            if (!chosen.has_value()) {
                break;
            }
            if (chosen.value() == start_v) {
                best_loop = std::move(path);
                break;
            }
            visited.insert(chosen.value());
            path.push_back(chosen.value());
            cur = chosen.value();
        }
        if (!best_loop.empty()) {
            break;
        }
    }

    if (best_loop.size() < 3) {
        return core::Result<std::uint32_t>::fail(
            core::ErrorCode::ValidationError,
            "Selected boundary does not form a closed polygon loop of >= 3 vertices");
    }

    EditableMesh candidate = *this;
    EditFace cap;
    cap.color = candidate.faces_.front().color;
    cap.material_id = candidate.faces_.front().material_id;

    // Project planar UVs in the new polygon's tangent plane.
    math::Vec3 centroid{0.0f, 0.0f, 0.0f};
    for (const std::uint32_t vi : best_loop) {
        centroid += candidate.vertices_[vi].position;
    }
    centroid *= (1.0f / static_cast<float>(best_loop.size()));

    const math::Vec3 e0 = (candidate.vertices_[best_loop[1]].position - candidate.vertices_[best_loop[0]].position).normalized();
    cap.corners.reserve(best_loop.size());
    for (const std::uint32_t vi : best_loop) {
        cap.corners.push_back(FaceCorner{.vertex = vi, .uv = {0.0f, 0.0f}});
    }
    const math::Vec3 n_raw = computeNewellNormalUnnormalized(candidate.vertices_, cap.corners);
    const math::Vec3 n_unit = (n_raw.lengthSquared() > 1.0e-10f) ? n_raw.normalized() : math::Vec3{0.0f, 1.0f, 0.0f};
    const math::Vec3 bitangent = n_unit.cross(e0).normalized();

    for (std::size_t i = 0; i < cap.corners.size(); ++i) {
        const math::Vec3 rel = candidate.vertices_[cap.corners[i].vertex].position - centroid;
        cap.corners[i].uv = {0.5f + rel.dot(e0) * 0.5f, 0.5f + rel.dot(bitangent) * 0.5f};
    }

    const auto new_face_idx = static_cast<std::uint32_t>(candidate.faces_.size());
    candidate.faces_.push_back(std::move(cap));

    auto st = candidate.validateAndRebuild();
    if (!st) {
        return st.error();
    }
    *this = std::move(candidate);
    return core::Result<std::uint32_t>::ok(new_face_idx);
}

core::Status EditableMesh::assignMaterialToFaces(
    std::span<const std::uint32_t> face_indices,
    MaterialId material_id) {
    if (face_indices.empty()) {
        return core::Status::fail(core::ErrorCode::InvalidArgument, "No faces selected for material assignment");
    }
    for (const std::uint32_t fi : face_indices) {
        if (static_cast<std::size_t>(fi) >= faces_.size()) {
            return core::Status::fail(core::ErrorCode::InvalidArgument, "Face index out of bounds");
        }
    }
    for (const std::uint32_t fi : face_indices) {
        faces_[fi].material_id = material_id;
    }
    return core::Status::ok();
}

void EditableMesh::clearMaterialReferences(MaterialId removed_material_id) noexcept {
    if (!removed_material_id.isValid()) {
        return;
    }
    for (auto& f : faces_) {
        if (f.material_id == removed_material_id) {
            f.material_id = MaterialId::invalid();
        }
    }
}

namespace {

std::optional<float> raycastMeshSurfaceDistance(
    const MeshData& triangulated,
    const math::Mat4& world_matrix,
    const math::Ray& world_ray,
    std::size_t* out_tri_idx = nullptr) {
    const auto inv_world = world_matrix.inverted();
    if (!inv_world.has_value()) {
        return std::nullopt;
    }
    const math::Vec3 local_orig = inv_world->transformPoint(world_ray.origin);
    const math::Vec3 local_dir_raw = inv_world->transformDirection(world_ray.direction);
    if (local_dir_raw.lengthSquared() <= 1.0e-12f) {
        return std::nullopt;
    }
    const math::Ray local_ray(local_orig, local_dir_raw.normalized());

    float best_world_t = std::numeric_limits<float>::max();
    bool hit_any = false;
    const std::size_t tri_count = triangulated.indices.size() / 3u;

    for (std::size_t t = 0; t < tri_count; ++t) {
        const auto& v0 = triangulated.vertices[triangulated.indices[t * 3u + 0]].position;
        const auto& v1 = triangulated.vertices[triangulated.indices[t * 3u + 1]].position;
        const auto& v2 = triangulated.vertices[triangulated.indices[t * 3u + 2]].position;

        const auto local_t = math::intersectRayTriangle(local_ray, v0, v1, v2, true);
        if (!local_t.has_value()) {
            continue;
        }
        const math::Vec3 world_hit = world_matrix.transformPoint(local_ray.at(local_t.value()));
        const float wt = (world_hit - world_ray.origin).dot(world_ray.direction);
        if (wt >= 0.0f && wt < best_world_t) {
            best_world_t = wt;
            hit_any = true;
            if (out_tri_idx != nullptr) {
                *out_tri_idx = t;
            }
        }
    }
    return hit_any ? std::optional<float>(best_world_t) : std::nullopt;
}

} // namespace

std::optional<std::uint32_t> EditableMesh::pickFace(
    const MeshData& triangulated,
    const math::Mat4& world_matrix,
    const math::Ray& world_ray) const {
    if (!world_ray.isValid()) {
        return std::nullopt;
    }
    std::size_t tri_idx = 0;
    const auto surf_t = raycastMeshSurfaceDistance(triangulated, world_matrix, world_ray, &tri_idx);
    if (!surf_t.has_value()) {
        return std::nullopt;
    }
    if (tri_idx < triangulated.triangle_face_ids.size()) {
        const std::uint32_t fi = triangulated.triangle_face_ids[tri_idx];
        if (static_cast<std::size_t>(fi) < faces_.size()) {
            return fi;
        }
    }
    return std::nullopt;
}

std::optional<std::uint32_t> EditableMesh::pickEdge(
    const MeshData& triangulated,
    const math::Mat4& world_matrix,
    const math::Ray& world_ray,
    float world_pick_radius) const {
    if (!world_ray.isValid() || edges_.empty()) {
        return std::nullopt;
    }
    const auto surf_t = raycastMeshSurfaceDistance(triangulated, world_matrix, world_ray);
    const float max_occluded_t = surf_t.has_value()
                                     ? (surf_t.value() + world_pick_radius * 2.5f)
                                     : std::numeric_limits<float>::max();

    float best_dist = std::max(1.0e-4f, world_pick_radius);
    std::optional<std::uint32_t> best_edge;

    for (std::size_t ei = 0; ei < edges_.size(); ++ei) {
        const math::Vec3 wa = world_matrix.transformPoint(vertices_[edges_[ei].v0].position);
        const math::Vec3 wb = world_matrix.transformPoint(vertices_[edges_[ei].v1].position);
        float ray_t = 0.0f;
        const float dist = raySegmentDistance(world_ray, wa, wb, ray_t);
        if (dist <= best_dist && ray_t <= max_occluded_t) {
            best_dist = dist;
            best_edge = static_cast<std::uint32_t>(ei);
        }
    }
    return best_edge;
}

std::optional<std::uint32_t> EditableMesh::pickVertex(
    const MeshData& triangulated,
    const math::Mat4& world_matrix,
    const math::Ray& world_ray,
    float world_pick_radius) const {
    if (!world_ray.isValid() || vertices_.empty()) {
        return std::nullopt;
    }
    const auto surf_t = raycastMeshSurfaceDistance(triangulated, world_matrix, world_ray);
    const float max_occluded_t = surf_t.has_value()
                                     ? (surf_t.value() + world_pick_radius * 2.5f)
                                     : std::numeric_limits<float>::max();

    float best_dist = std::max(1.0e-4f, world_pick_radius);
    std::optional<std::uint32_t> best_vert;

    for (std::size_t vi = 0; vi < vertices_.size(); ++vi) {
        const math::Vec3 wp = world_matrix.transformPoint(vertices_[vi].position);
        float ray_t = 0.0f;
        const float dist = raySegmentDistance(world_ray, wp, wp, ray_t);
        if (dist <= best_dist && ray_t <= max_occluded_t) {
            best_dist = dist;
            best_vert = static_cast<std::uint32_t>(vi);
        }
    }
    return best_vert;
}

MeshData EditableMesh::buildEditOverlayMesh(
    EditMode mode,
    const MeshElementSelection& selection,
    const math::Mat4& world_matrix,
    float world_gizmo_scale) const {
    MeshData overlay;
    if (mode == EditMode::Object || vertices_.empty()) {
        return overlay;
    }

    const float s = std::max(0.05f, world_gizmo_scale);
    const float wire_r = 0.0085f * s;
    const float sel_wire_r = 0.015f * s;
    const float vert_r = 0.026f * s;
    const float sel_vert_r = 0.038f * s;

    const std::unordered_set<std::uint32_t> sel_verts(selection.vertices.begin(), selection.vertices.end());
    const std::unordered_set<std::uint32_t> sel_edges(selection.edges.begin(), selection.edges.end());
    const std::unordered_set<std::uint32_t> sel_faces(selection.faces.begin(), selection.faces.end());

    // 1. Wireframe cage along all topological edges.
    for (std::size_t ei = 0; ei < edges_.size(); ++ei) {
        const bool is_sel = (mode == EditMode::Edge) && sel_edges.contains(static_cast<std::uint32_t>(ei));
        const math::Vec3 wa = world_matrix.transformPoint(vertices_[edges_[ei].v0].position);
        const math::Vec3 wb = world_matrix.transformPoint(vertices_[edges_[ei].v1].position);
        const math::Vec3 col = is_sel ? math::Vec3{1.0f, 0.78f, 0.20f} : math::Vec3{0.36f, 0.48f, 0.64f};
        appendOverlayBox(overlay, wa, wb, is_sel ? sel_wire_r : wire_r, col);
    }

    // 2. Vertex markers in Vertex mode.
    if (mode == EditMode::Vertex) {
        for (std::size_t vi = 0; vi < vertices_.size(); ++vi) {
            const bool is_sel = sel_verts.contains(static_cast<std::uint32_t>(vi));
            const math::Vec3 wp = world_matrix.transformPoint(vertices_[vi].position);
            const math::Vec3 col = is_sel ? math::Vec3{1.0f, 0.84f, 0.24f} : math::Vec3{0.35f, 0.78f, 0.98f};
            appendVertexDiamond(overlay, wp, is_sel ? sel_vert_r : vert_r, col);
        }
    }

    // 3. Selected polygon face highlights in Face mode.
    if (mode == EditMode::Face) {
        for (const std::uint32_t fi : selection.faces) {
            if (static_cast<std::size_t>(fi) >= faces_.size()) {
                continue;
            }
            const auto& f = faces_[fi];
            if (f.corners.size() < 3) {
                continue;
            }
            const math::Vec3 wn = world_matrix.transformDirection(f.normal).normalized();
            const math::Vec3 offset = wn * (0.008f * s);
            const auto base_idx = static_cast<std::uint32_t>(overlay.vertices.size());
            for (const auto& c : f.corners) {
                const math::Vec3 wp = world_matrix.transformPoint(vertices_[c.vertex].position) + offset;
                overlay.vertices.push_back(Vertex{
                    .position = wp,
                    .normal = wn,
                    .color = {1.0f, 0.74f, 0.22f},
                    .uv = c.uv,
                });
            }
            for (std::size_t k = 1; k + 1 < f.corners.size(); ++k) {
                overlay.indices.push_back(base_idx);
                overlay.indices.push_back(base_idx + static_cast<std::uint32_t>(k));
                overlay.indices.push_back(base_idx + static_cast<std::uint32_t>(k + 1));
            }
        }
    }
    (void)sel_faces;

    return overlay;
}

} // namespace finaleffect::scene
