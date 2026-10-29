// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include "finaleffect/core/result.hpp"
#include "finaleffect/math/vec2.hpp"
#include "finaleffect/math/vec3.hpp"
#include "finaleffect/scene/object_id.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace finaleffect::scene {

struct Vertex {
    math::Vec3 position{};
    math::Vec3 normal{0.0f, 1.0f, 0.0f};
    math::Vec3 color{1.0f, 1.0f, 1.0f};
    math::Vec2 uv{};

    [[nodiscard]] bool isFinite() const noexcept {
        return position.isFinite() && normal.isFinite() && color.isFinite() && uv.isFinite();
    }
};

struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;

    [[nodiscard]] core::Status validate() const {
        if (vertices.empty()) {
            return core::Status::fail(core::ErrorCode::ValidationError, "MeshData must contain at least one vertex");
        }
        if (indices.empty() || (indices.size() % 3 != 0)) {
            return core::Status::fail(
                core::ErrorCode::ValidationError,
                "MeshData index count must be non-zero and a multiple of 3");
        }
        for (const auto& v : vertices) {
            if (!v.isFinite()) {
                return core::Status::fail(
                    core::ErrorCode::ValidationError,
                    "MeshData contains non-finite vertex attributes");
            }
        }
        const auto vertex_count = vertices.size();
        for (const std::uint32_t idx : indices) {
            if (static_cast<std::size_t>(idx) >= vertex_count) {
                return core::Status::fail(
                    core::ErrorCode::ValidationError,
                    "MeshData triangle index exceeds vertex array bounds");
            }
        }
        return core::Status::ok();
    }

    [[nodiscard]] std::size_t byteSize() const noexcept {
        return vertices.size() * sizeof(Vertex) + indices.size() * sizeof(std::uint32_t);
    }
};

struct MeshAsset {
    MeshId id{MeshId::invalid()};
    std::string name;
    MeshData data;
    std::uint64_t revision{1};
};

} // namespace finaleffect::scene
