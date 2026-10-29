// Final Effect 3D - stable scene object identifiers.
// SPDX-License-Identifier: GPL-3.0-only
//
// Identifiers are opaque 64-bit values that stay stable across save/load cycles
// and never get reused inside a scene, so a stale reference is always detectable.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace fe::scene {

struct ObjectId {
    std::uint64_t value = 0;

    constexpr bool isValid() const noexcept { return value != 0; }
    constexpr bool operator==(const ObjectId& rhs) const noexcept { return value == rhs.value; }
    constexpr bool operator!=(const ObjectId& rhs) const noexcept { return value != rhs.value; }
    constexpr bool operator<(const ObjectId& rhs) const noexcept { return value < rhs.value; }
};

inline constexpr ObjectId kInvalidObjectId{0};

std::string toString(ObjectId id);

struct ObjectIdHash {
    std::size_t operator()(const ObjectId& id) const noexcept {
        return static_cast<std::size_t>(id.value ^ (id.value >> 32U));
    }
};

}  // namespace fe::scene
