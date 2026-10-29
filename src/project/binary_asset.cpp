// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors
//
// Final Effect 3D Binary Asset Container (.fe3dbin v1.0) Layout (Little-Endian):
// - Bytes 0..7   : Magic "FE3DBIN\0"
// - Bytes 8..9   : uint16 major_version (1)
// - Bytes 10..11 : uint16 minor_version (0)
// - Bytes 12..15 : uint32 chunk_count
// - Bytes 16..23 : uint64 directory_offset
// - Bytes 24..27 : uint32 directory_crc32
// - Bytes 28..31 : uint32 reserved (0)
// - Payload region: packed mesh chunks (each vertex = 11 IEEE-754 float32s = 44 bytes, followed by uint32 indices)
// - Directory region: chunk_count * 40-byte entries:
//     uint64 content_hash | uint32 chunk_type (1=Mesh) | uint32 vertex_count |
//     uint32 index_count  | uint32 crc32               | uint64 byte_offset  | uint64 byte_length

#include "finaleffect/project/binary_asset.hpp"

#include <array>
#include <bit>
#include <cstdio>
#include <cstring>
#include <limits>

namespace finaleffect::project {

namespace {

constexpr std::array<std::uint8_t, 8> kMagicBytes{'F', 'E', '3', 'D', 'B', 'I', 'N', '\0'};
constexpr std::size_t kHeaderSize = 32;
constexpr std::size_t kDirectoryEntrySize = 40;
constexpr std::uint32_t kChunkTypeTriangleMesh = 1;
constexpr std::size_t kVertexEncodedBytes = 11 * sizeof(std::uint32_t); // 44 bytes

void appendU16LE(std::vector<std::uint8_t>& out, std::uint16_t v) {
    out.push_back(static_cast<std::uint8_t>(v & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((v >> 8u) & 0xFFu));
}

void appendU32LE(std::vector<std::uint8_t>& out, std::uint32_t v) {
    out.push_back(static_cast<std::uint8_t>(v & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((v >> 8u) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((v >> 16u) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((v >> 24u) & 0xFFu));
}

void appendU64LE(std::vector<std::uint8_t>& out, std::uint64_t v) {
    for (unsigned int shift = 0; shift < 64u; shift += 8u) {
        out.push_back(static_cast<std::uint8_t>((v >> shift) & 0xFFu));
    }
}

void appendF32LE(std::vector<std::uint8_t>& out, float f) {
    // Normalize -0.0f to +0.0f so identical geometry produces identical content hashes.
    const float normalized = (f == 0.0f) ? 0.0f : f;
    appendU32LE(out, std::bit_cast<std::uint32_t>(normalized));
}

std::uint16_t readU16LE(const std::uint8_t* p) noexcept {
    return static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(p[0]) |
        static_cast<std::uint16_t>(static_cast<std::uint16_t>(p[1]) << 8u));
}

std::uint32_t readU32LE(const std::uint8_t* p) noexcept {
    return static_cast<std::uint32_t>(p[0]) |
           (static_cast<std::uint32_t>(p[1]) << 8u) |
           (static_cast<std::uint32_t>(p[2]) << 16u) |
           (static_cast<std::uint32_t>(p[3]) << 24u);
}

std::uint64_t readU64LE(const std::uint8_t* p) noexcept {
    std::uint64_t val = 0;
    for (unsigned int i = 0; i < 8u; ++i) {
        val |= static_cast<std::uint64_t>(p[i]) << (i * 8u);
    }
    return val;
}

float readF32LE(const std::uint8_t* p) noexcept {
    return std::bit_cast<float>(readU32LE(p));
}

std::uint64_t computeFnv1a64(const std::uint8_t* data, std::size_t len) noexcept {
    std::uint64_t hash = 14695981039346656037ull;
    for (std::size_t i = 0; i < len; ++i) {
        hash ^= static_cast<std::uint64_t>(data[i]);
        hash *= 1099511628211ull;
    }
    return hash;
}

std::uint32_t computeCrc32(const std::uint8_t* data, std::size_t len) noexcept {
    std::uint32_t crc = 0xFFFFFFFFu;
    for (std::size_t i = 0; i < len; ++i) {
        crc ^= static_cast<std::uint32_t>(data[i]);
        for (int bit = 0; bit < 8; ++bit) {
            const std::uint32_t mask = (crc & 1u) != 0u ? 0xEDB88320u : 0u;
            crc = (crc >> 1u) ^ mask;
        }
    }
    return ~crc;
}

} // namespace

std::string formatContentHashHex(std::uint64_t hash) {
    char buf[17];
    std::snprintf(buf, sizeof(buf), "%016llx", static_cast<unsigned long long>(hash));
    return std::string(buf);
}

core::Result<std::uint64_t> parseContentHashHex(std::string_view hex) {
    if (hex.size() != 16) {
        return core::Result<std::uint64_t>::fail(
            core::ErrorCode::ValidationError,
            "Content hash must be an exact 16-character hexadecimal string");
    }
    std::uint64_t val = 0;
    for (const char ch : hex) {
        val <<= 4u;
        if (ch >= '0' && ch <= '9') {
            val |= static_cast<std::uint64_t>(ch - '0');
        } else if (ch >= 'a' && ch <= 'f') {
            val |= static_cast<std::uint64_t>(10 + (ch - 'a'));
        } else if (ch >= 'A' && ch <= 'F') {
            val |= static_cast<std::uint64_t>(10 + (ch - 'A'));
        } else {
            return core::Result<std::uint64_t>::fail(
                core::ErrorCode::ValidationError,
                "Invalid hexadecimal character in content hash");
        }
    }
    return core::Result<std::uint64_t>::ok(val);
}

core::Result<std::uint64_t> BinaryAssetBuilder::addMesh(const scene::MeshData& mesh) {
    auto valid = mesh.validate();
    if (!valid) {
        return valid.error();
    }
    if (mesh.vertices.size() > std::numeric_limits<std::uint32_t>::max() ||
        mesh.indices.size() > std::numeric_limits<std::uint32_t>::max()) {
        return core::Result<std::uint64_t>::fail(
            core::ErrorCode::InvalidArgument,
            "Mesh exceeds uint32 vertex/index limits for .fe3dbin");
    }

    const std::size_t payload_bytes =
        mesh.vertices.size() * kVertexEncodedBytes + mesh.indices.size() * sizeof(std::uint32_t);
    std::vector<std::uint8_t> payload;
    payload.reserve(payload_bytes);

    for (const auto& v : mesh.vertices) {
        appendF32LE(payload, v.position.x);
        appendF32LE(payload, v.position.y);
        appendF32LE(payload, v.position.z);
        appendF32LE(payload, v.normal.x);
        appendF32LE(payload, v.normal.y);
        appendF32LE(payload, v.normal.z);
        appendF32LE(payload, v.color.x);
        appendF32LE(payload, v.color.y);
        appendF32LE(payload, v.color.z);
        appendF32LE(payload, v.uv.x);
        appendF32LE(payload, v.uv.y);
    }
    for (const std::uint32_t idx : mesh.indices) {
        appendU32LE(payload, idx);
    }

    const std::uint64_t hash = computeFnv1a64(payload.data(), payload.size());
    if (const auto existing = hash_to_index_.find(hash); existing != hash_to_index_.end()) {
        if (chunks_[existing->second].payload == payload) {
            return core::Result<std::uint64_t>::ok(hash);
        }
    }

    const std::uint32_t crc = computeCrc32(payload.data(), payload.size());
    const std::size_t chunk_idx = chunks_.size();
    chunks_.push_back(EncodedChunk{
        .content_hash = hash,
        .vertex_count = static_cast<std::uint32_t>(mesh.vertices.size()),
        .index_count = static_cast<std::uint32_t>(mesh.indices.size()),
        .crc32 = crc,
        .payload = std::move(payload),
    });
    hash_to_index_.emplace(hash, chunk_idx);
    return core::Result<std::uint64_t>::ok(hash);
}

std::vector<std::uint8_t> BinaryAssetBuilder::encode() const {
    std::size_t total_payload_size = 0;
    for (const auto& c : chunks_) {
        total_payload_size += c.payload.size();
    }

    const std::uint64_t dir_offset = kHeaderSize + total_payload_size;
    std::vector<std::uint8_t> directory;
    directory.reserve(chunks_.size() * kDirectoryEntrySize);

    std::uint64_t running_offset = kHeaderSize;
    for (const auto& c : chunks_) {
        appendU64LE(directory, c.content_hash);
        appendU32LE(directory, kChunkTypeTriangleMesh);
        appendU32LE(directory, c.vertex_count);
        appendU32LE(directory, c.index_count);
        appendU32LE(directory, c.crc32);
        appendU64LE(directory, running_offset);
        appendU64LE(directory, static_cast<std::uint64_t>(c.payload.size()));
        running_offset += c.payload.size();
    }

    const std::uint32_t dir_crc =
        directory.empty() ? 0u : computeCrc32(directory.data(), directory.size());

    std::vector<std::uint8_t> header;
    header.reserve(kHeaderSize);
    for (const std::uint8_t b : kMagicBytes) {
        header.push_back(b);
    }
    appendU16LE(header, kBinaryAssetMajor);
    appendU16LE(header, kBinaryAssetMinor);
    appendU32LE(header, static_cast<std::uint32_t>(chunks_.size()));
    appendU64LE(header, dir_offset);
    appendU32LE(header, dir_crc);
    appendU32LE(header, 0u); // reserved

    std::vector<std::uint8_t> out(header.size() + total_payload_size + directory.size(), 0u);
    std::size_t write_offset = 0;
    std::memcpy(out.data() + write_offset, header.data(), header.size());
    write_offset += header.size();

    for (const auto& c : chunks_) {
        if (!c.payload.empty()) {
            std::memcpy(out.data() + write_offset, c.payload.data(), c.payload.size());
            write_offset += c.payload.size();
        }
    }
    if (!directory.empty()) {
        std::memcpy(out.data() + write_offset, directory.data(), directory.size());
    }
    return out;
}

core::Status BinaryAssetBuilder::saveToFile(
    const std::filesystem::path& path,
    platform::IFileSystem& file_system) const {
    return file_system.writeBinaryFile(path, encode());
}

core::Result<BinaryAssetArchive> BinaryAssetArchive::decode(const std::vector<std::uint8_t>& bytes) {
    if (bytes.size() < kHeaderSize) {
        return core::Result<BinaryAssetArchive>::fail(
            core::ErrorCode::ParseError,
            "Binary asset file is truncated (smaller than 32-byte header)");
    }

    if (std::memcmp(bytes.data(), kMagicBytes.data(), kMagicBytes.size()) != 0) {
        return core::Result<BinaryAssetArchive>::fail(
            core::ErrorCode::ValidationError,
            "Invalid .fe3dbin magic header");
    }

    const std::uint16_t major = readU16LE(bytes.data() + 8);
    const std::uint16_t minor = readU16LE(bytes.data() + 10);
    if (major != kBinaryAssetMajor || minor > kBinaryAssetMinor) {
        return core::Result<BinaryAssetArchive>::fail(
            core::ErrorCode::UnsupportedVersion,
            "Unsupported .fe3dbin version " + std::to_string(major) + "." + std::to_string(minor));
    }

    const std::uint32_t chunk_count = readU32LE(bytes.data() + 12);
    const std::uint64_t dir_offset = readU64LE(bytes.data() + 16);
    const std::uint32_t expected_dir_crc = readU32LE(bytes.data() + 24);

    const std::uint64_t dir_bytes = static_cast<std::uint64_t>(chunk_count) * kDirectoryEntrySize;
    if (dir_offset < kHeaderSize || dir_offset > bytes.size() || dir_bytes > (bytes.size() - dir_offset)) {
        return core::Result<BinaryAssetArchive>::fail(
            core::ErrorCode::ValidationError,
            "Corrupted .fe3dbin directory offset or chunk count");
    }

    const std::uint32_t actual_dir_crc =
        (dir_bytes == 0)
            ? 0u
            : computeCrc32(bytes.data() + static_cast<std::size_t>(dir_offset), static_cast<std::size_t>(dir_bytes));
    if (actual_dir_crc != expected_dir_crc) {
        return core::Result<BinaryAssetArchive>::fail(
            core::ErrorCode::ValidationError,
            "CRC-32 checksum mismatch in .fe3dbin chunk directory");
    }

    BinaryAssetArchive archive;
    archive.meshes_.reserve(chunk_count);

    for (std::uint32_t i = 0; i < chunk_count; ++i) {
        const std::uint8_t* entry =
            bytes.data() + static_cast<std::size_t>(dir_offset) + static_cast<std::size_t>(i) * kDirectoryEntrySize;

        const std::uint64_t content_hash = readU64LE(entry + 0);
        const std::uint32_t chunk_type = readU32LE(entry + 8);
        const std::uint32_t vertex_count = readU32LE(entry + 12);
        const std::uint32_t index_count = readU32LE(entry + 16);
        const std::uint32_t expected_crc = readU32LE(entry + 20);
        const std::uint64_t byte_offset = readU64LE(entry + 24);
        const std::uint64_t byte_length = readU64LE(entry + 32);

        if (chunk_type != kChunkTypeTriangleMesh) {
            return core::Result<BinaryAssetArchive>::fail(
                core::ErrorCode::ValidationError,
                "Unknown chunk type in .fe3dbin directory");
        }

        if (byte_offset < kHeaderSize || byte_offset > dir_offset || byte_length > (dir_offset - byte_offset)) {
            return core::Result<BinaryAssetArchive>::fail(
                core::ErrorCode::ValidationError,
                "Chunk byte range lies outside .fe3dbin payload bounds");
        }

        const std::uint64_t expected_len =
            static_cast<std::uint64_t>(vertex_count) * kVertexEncodedBytes +
            static_cast<std::uint64_t>(index_count) * sizeof(std::uint32_t);
        if (byte_length != expected_len) {
            return core::Result<BinaryAssetArchive>::fail(
                core::ErrorCode::ValidationError,
                "Chunk byte_length does not match vertex_count and index_count");
        }

        const std::uint8_t* payload_ptr = bytes.data() + static_cast<std::size_t>(byte_offset);
        const auto payload_size = static_cast<std::size_t>(byte_length);

        if (computeCrc32(payload_ptr, payload_size) != expected_crc) {
            return core::Result<BinaryAssetArchive>::fail(
                core::ErrorCode::ValidationError,
                "CRC-32 checksum mismatch in .fe3dbin mesh payload");
        }
        if (computeFnv1a64(payload_ptr, payload_size) != content_hash) {
            return core::Result<BinaryAssetArchive>::fail(
                core::ErrorCode::ValidationError,
                "Content hash mismatch in .fe3dbin mesh payload");
        }

        scene::MeshData mesh;
        mesh.vertices.resize(vertex_count);
        mesh.indices.resize(index_count);

        const std::uint8_t* cursor = payload_ptr;
        for (std::uint32_t vi = 0; vi < vertex_count; ++vi) {
            scene::Vertex v{};
            v.position.x = readF32LE(cursor + 0);
            v.position.y = readF32LE(cursor + 4);
            v.position.z = readF32LE(cursor + 8);
            v.normal.x = readF32LE(cursor + 12);
            v.normal.y = readF32LE(cursor + 16);
            v.normal.z = readF32LE(cursor + 20);
            v.color.x = readF32LE(cursor + 24);
            v.color.y = readF32LE(cursor + 28);
            v.color.z = readF32LE(cursor + 32);
            v.uv.x = readF32LE(cursor + 36);
            v.uv.y = readF32LE(cursor + 40);
            mesh.vertices[vi] = v;
            cursor += kVertexEncodedBytes;
        }
        for (std::uint32_t ii = 0; ii < index_count; ++ii) {
            mesh.indices[ii] = readU32LE(cursor);
            cursor += sizeof(std::uint32_t);
        }

        auto valid = mesh.validate();
        if (!valid) {
            return valid.error();
        }
        archive.meshes_.emplace(content_hash, std::move(mesh));
    }

    return core::Result<BinaryAssetArchive>::ok(std::move(archive));
}

core::Result<BinaryAssetArchive> BinaryAssetArchive::loadFromFile(
    const std::filesystem::path& path,
    const platform::IFileSystem& file_system) {
    auto bytes_res = file_system.readBinaryFile(path);
    if (!bytes_res) {
        return bytes_res.error();
    }
    return decode(bytes_res.value());
}

core::Result<scene::MeshData> BinaryAssetArchive::extractMesh(std::uint64_t content_hash) const {
    const auto it = meshes_.find(content_hash);
    if (it == meshes_.end()) {
        return core::Result<scene::MeshData>::fail(
            core::ErrorCode::NotFound,
            "Requested mesh content hash not found in .fe3dbin archive",
            formatContentHashHex(content_hash));
    }
    return core::Result<scene::MeshData>::ok(it->second);
}

} // namespace finaleffect::project
