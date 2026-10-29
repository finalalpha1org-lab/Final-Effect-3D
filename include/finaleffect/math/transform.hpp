// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include "finaleffect/math/mat4.hpp"
#include "finaleffect/math/quat.hpp"
#include "finaleffect/math/vec3.hpp"

namespace finaleffect::math {

struct Transform {
    Vec3 position{0.0f, 0.0f, 0.0f};
    Quat rotation{Quat::identity()};
    Vec3 scale{1.0f, 1.0f, 1.0f};

    [[nodiscard]] static constexpr Transform identity() noexcept {
        return Transform{};
    }

    [[nodiscard]] Mat4 toMatrix() const noexcept;

    [[nodiscard]] Vec3 right() const noexcept;
    [[nodiscard]] Vec3 up() const noexcept;
    [[nodiscard]] Vec3 forward() const noexcept;

    [[nodiscard]] Vec3 transformPoint(const Vec3& local_point) const noexcept;
    [[nodiscard]] Vec3 transformDirection(const Vec3& local_direction) const noexcept;

    // Composes parent and local transforms when parent has uniform or general TRS.
    // Note: Under non-uniform parent scale + child rotation, exact world transform is
    // always represented by parent.toMatrix() * local.toMatrix() (used by Scene::computeWorldMatrix).
    [[nodiscard]] static Transform combineRigidOrUniform(const Transform& parent, const Transform& local) noexcept;

    [[nodiscard]] bool isValid() const noexcept;
    [[nodiscard]] bool approxEqual(const Transform& rhs, float epsilon = kDefaultEpsilon) const noexcept;
};

} // namespace finaleffect::math
