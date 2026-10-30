// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>

namespace finaleffect::scene {

struct ObjectId {
    std::uint64_t value{0};

    [[nodiscard]] static constexpr ObjectId invalid() noexcept { return ObjectId{0}; }
    [[nodiscard]] constexpr bool isValid() const noexcept { return value != 0; }
    constexpr auto operator<=>(const ObjectId&) const noexcept = default;
};

struct MeshId {
    std::uint64_t value{0};

    [[nodiscard]] static constexpr MeshId invalid() noexcept { return MeshId{0}; }
    [[nodiscard]] constexpr bool isValid() const noexcept { return value != 0; }
    constexpr auto operator<=>(const MeshId&) const noexcept = default;
};

struct MaterialId {
    std::uint64_t value{0};

    [[nodiscard]] static constexpr MaterialId invalid() noexcept { return MaterialId{0}; }
    [[nodiscard]] constexpr bool isValid() const noexcept { return value != 0; }
    constexpr auto operator<=>(const MaterialId&) const noexcept = default;
};

} // namespace finaleffect::scene

namespace std {

template <>
struct hash<finaleffect::scene::ObjectId> {
    std::size_t operator()(const finaleffect::scene::ObjectId& id) const noexcept {
        return std::hash<std::uint64_t>{}(id.value);
    }
};

template <>
struct hash<finaleffect::scene::MeshId> {
    std::size_t operator()(const finaleffect::scene::MeshId& id) const noexcept {
        return std::hash<std::uint64_t>{}(id.value);
    }
};

template <>
struct hash<finaleffect::scene::MaterialId> {
    std::size_t operator()(const finaleffect::scene::MaterialId& id) const noexcept {
        return std::hash<std::uint64_t>{}(id.value);
    }
};

} // namespace std
