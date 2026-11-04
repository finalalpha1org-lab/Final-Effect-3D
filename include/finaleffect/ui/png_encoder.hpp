// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include "finaleffect/core/result.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace finaleffect::ui {

// Encodes a tightly packed top-down RGBA8 image buffer (width * height * 4 bytes)
// into a standard RFC-2083 compliant PNG byte stream without external dependencies.
[[nodiscard]] core::Result<std::vector<std::uint8_t>> encodeRgba8ToPng(
    std::uint32_t width,
    std::uint32_t height,
    std::span<const std::uint8_t> rgba_top_down);

} // namespace finaleffect::ui
