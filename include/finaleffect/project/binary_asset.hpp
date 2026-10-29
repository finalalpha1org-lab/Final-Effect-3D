// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include "finaleffect/core/result.hpp"
#include "finaleffect/platform/file_system.hpp"
#include "finaleffect/scene/mesh.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace finaleffect::project {

inline constexpr std::uint16_t kBinaryAssetMajor = 1;
inline constexpr std::uint16_t kBinaryAssetMinor = 0;

[[nodiscard]] std::string formatContentHashHex(std::uint64_t hash);
[[nodiscard]] core::Result<std::uint64_t> parseContentHashHex(std::string_view hex);

// Builds a deduplicated, content-addressed .fe3dbin binary geometry container.
class BinaryAssetBuilder {
public:
    // Encodes and deduplicates a validated MeshData chunk; returns its 64-bit content hash.
    [[nodiscard]] core::Result<std::uint64_t> addMesh(const scene::MeshData& mesh);

    [[nodiscard]] std::size_t uniqueChunkCount() const noexcept { return chunks_.size(); }

    // Serializes header + payloads + directory with IEEE CRC-32 checksums.
    [[nodiscard]] std::vector<std::uint8_t> encode() const;

    [[nodiscard]] core::Status saveToFile(
        const std::filesystem::path& path,
        platform::IFileSystem& file_system) const;

private:
    struct EncodedChunk {
        std::uint64_t content_hash{0};
        std::uint32_t vertex_count{0};
        std::uint32_t index_count{0};
        std::uint32_t crc32{0};
        std::vector<std::uint8_t> payload;
    };

    std::vector<EncodedChunk> chunks_;
    std::unordered_map<std::uint64_t, std::size_t> hash_to_index_;
};

// Validated reader for .fe3dbin containers; verifies magic, version, bounds, and CRC-32 checksums.
class BinaryAssetArchive {
public:
    [[nodiscard]] static core::Result<BinaryAssetArchive> decode(const std::vector<std::uint8_t>& bytes);

    [[nodiscard]] static core::Result<BinaryAssetArchive> loadFromFile(
        const std::filesystem::path& path,
        const platform::IFileSystem& file_system);

    [[nodiscard]] bool containsChunk(std::uint64_t content_hash) const noexcept {
        return meshes_.contains(content_hash);
    }

    [[nodiscard]] std::size_t chunkCount() const noexcept { return meshes_.size(); }

    [[nodiscard]] core::Result<scene::MeshData> extractMesh(std::uint64_t content_hash) const;

private:
    std::unordered_map<std::uint64_t, scene::MeshData> meshes_;
};

} // namespace finaleffect::project
