// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors
//
// Final Effect 3D Binary Project Container (.fe3d / .fe3a, v1.0):
//
// Both extensions use this identical container; .fe3d is the project document and
// .fe3a is the same document used as a portable archive. It replaces the former
// JSON text project format - no JSON text is stored on disk.
//
// Layout (little-endian):
// - Bytes 0..7   : magic  'F' 'E' '3' 'D' 'P' 'R' 'J' 0x1A
// - Bytes 8..9   : uint16 major_version (1)
// - Bytes 10..11 : uint16 minor_version (0)
// - Bytes 12..15 : uint32 chunk_count (1..kBinaryProjectMaxChunks)
// - Bytes 16..23 : uint64 directory_offset
// - Bytes 24..27 : uint32 directory_crc32 (CRC-32 IEEE over the directory bytes)
// - Bytes 28..31 : uint32 flags (bit0 = embedded geometry archive present)
// - Payload region (starts at byte 32): chunks, each padded to an 8-byte boundary
// - Directory region: chunk_count * 40-byte entries:
//     uint64 content_hash (FNV-1a-64 of the payload) | uint32 chunk_type |
//     uint32 reserved | uint64 byte_offset | uint64 byte_length |
//     uint32 crc32 (CRC-32 IEEE of the payload) | uint32 reserved
//
// Chunk type 1 (DocumentNodeStream) payload:
//     uint32 symbol_count, symbol_count * (uint32 byte_length + UTF-8 bytes)
//     (interned object keys, first-seen order) followed by one root node.
//   Node encoding: uint8 tag, then payload by tag
//     0x00 null | 0x01 false | 0x02 true | 0x03 float64 number
//     0x04 string: uint32 byte_length + UTF-8 bytes
//     0x05 array:  uint32 count + count nodes
//     0x06 object: uint32 count + count * (uint32 symbol_index + node)
// Chunk type 2 (GeometryArchive) payload: one embedded .fe3dbin container image.

#pragma once

#include "finaleffect/core/result.hpp"
#include "finaleffect/project/json.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace finaleffect::project {

inline constexpr std::array<std::uint8_t, 8> kBinaryProjectMagic{'F', 'E', '3', 'D', 'P', 'R', 'J', 0x1A};
inline constexpr std::uint16_t kBinaryProjectMajor = 1;
inline constexpr std::uint16_t kBinaryProjectMinor = 0;
inline constexpr std::size_t kBinaryProjectHeaderSize = 32;
inline constexpr std::size_t kBinaryProjectDirectoryEntrySize = 40;
inline constexpr std::size_t kBinaryProjectMaxDepth = 128;
inline constexpr std::size_t kBinaryProjectMaxSymbols = 65536;
inline constexpr std::size_t kBinaryProjectMaxChunks = 64;

// A decoded container image: the document tree plus optional embedded geometry archive.
struct BinaryProjectImage {
    JsonValue document{};
    std::vector<std::uint8_t> geometry_archive{};

    [[nodiscard]] bool hasGeometry() const noexcept { return !geometry_archive.empty(); }
};

// Encodes/decodes the .fe3d / .fe3a binary project container.
class BinaryProjectCodec {
public:
    // True when the buffer carries the container magic (used to reject legacy JSON projects).
    [[nodiscard]] static bool isBinaryProjectImage(std::span<const std::uint8_t> bytes) noexcept;

    [[nodiscard]] static core::Result<std::vector<std::uint8_t>> encode(const BinaryProjectImage& image);

    [[nodiscard]] static core::Result<BinaryProjectImage> decode(std::span<const std::uint8_t> bytes);
};

} // namespace finaleffect::project
