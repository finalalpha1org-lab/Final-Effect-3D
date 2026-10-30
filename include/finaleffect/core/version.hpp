// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include <compare>
#include <cstdint>
#include <string>
#include <string_view>

namespace finaleffect::core {

struct Version {
    std::uint32_t major{0};
    std::uint32_t minor{1};
    std::uint32_t patch{0};

    constexpr auto operator<=>(const Version&) const = default;

    [[nodiscard]] std::string toString() const;
};

inline constexpr Version kEngineVersion{0, 4, 0};
inline constexpr std::uint32_t kProjectSchemaMajor = 1;
inline constexpr std::uint32_t kProjectSchemaMinor = 2;

[[nodiscard]] std::string_view engineName() noexcept;
[[nodiscard]] std::string_view engineVersionString() noexcept;

} // namespace finaleffect::core
