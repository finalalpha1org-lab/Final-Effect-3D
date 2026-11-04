// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/project/binary_checksum.hpp"

#include <cstddef>

namespace finaleffect::project {

std::uint32_t crc32Ieee(std::span<const std::uint8_t> data) noexcept {
    std::uint32_t crc = 0xFFFFFFFFu;
    for (const std::uint8_t byte : data) {
        crc ^= static_cast<std::uint32_t>(byte);
        for (int bit = 0; bit < 8; ++bit) {
            const std::uint32_t mask = (crc & 1u) != 0u ? 0xEDB88320u : 0u;
            crc = (crc >> 1u) ^ mask;
        }
    }
    return ~crc;
}

} // namespace finaleffect::project
