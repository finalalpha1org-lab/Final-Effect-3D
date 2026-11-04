// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include <cstdint>
#include <span>

namespace finaleffect::project {

// CRC-32 (IEEE 802.3, reflected, polynomial 0xEDB88320) used for the .fe3d / .fe3a project
// container (directory and per-chunk payload protection). BinaryAssetArchive keeps its own
// table-driven routine so existing .fe3dbin geometry bytes stay byte-compatible.
[[nodiscard]] std::uint32_t crc32Ieee(std::span<const std::uint8_t> data) noexcept;

} // namespace finaleffect::project
