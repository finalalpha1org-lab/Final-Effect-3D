// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/ui/png_encoder.hpp"

#include <algorithm>
#include <array>
#include <cstddef>

namespace finaleffect::ui {

namespace {

void appendBigEndianU32(std::vector<std::uint8_t>& out, std::uint32_t v) {
    out.push_back(static_cast<std::uint8_t>((v >> 24u) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((v >> 16u) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((v >> 8u) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>(v & 0xFFu));
}

std::uint32_t computeCrc32(std::span<const std::uint8_t> bytes) noexcept {
    static const auto kTable = [] {
        std::array<std::uint32_t, 256> tbl{};
        for (std::uint32_t n = 0; n < 256u; ++n) {
            std::uint32_t c = n;
            for (int k = 0; k < 8; ++k) {
                c = (c & 1u) != 0u ? (0xEDB88320u ^ (c >> 1u)) : (c >> 1u);
            }
            tbl[n] = c;
        }
        return tbl;
    }();

    std::uint32_t crc = 0xFFFFFFFFu;
    for (const std::uint8_t b : bytes) {
        crc = kTable[(crc ^ b) & 0xFFu] ^ (crc >> 8u);
    }
    return crc ^ 0xFFFFFFFFu;
}

std::uint32_t computeAdler32(std::span<const std::uint8_t> bytes) noexcept {
    constexpr std::uint32_t kModAdler = 65521u;
    std::uint32_t a = 1u;
    std::uint32_t b = 0u;
    std::size_t idx = 0;
    while (idx < bytes.size()) {
        const std::size_t chunk = std::min<std::size_t>(bytes.size() - idx, 5552u);
        for (std::size_t i = 0; i < chunk; ++i) {
            a += bytes[idx + i];
            b += a;
        }
        a %= kModAdler;
        b %= kModAdler;
        idx += chunk;
    }
    return (b << 16u) | a;
}

void appendPngChunk(
    std::vector<std::uint8_t>& png,
    const std::array<std::uint8_t, 4>& type,
    std::span<const std::uint8_t> payload) {
    appendBigEndianU32(png, static_cast<std::uint32_t>(payload.size()));
    const std::size_t type_offset = png.size();
    png.insert(png.end(), type.begin(), type.end());
    png.insert(png.end(), payload.begin(), payload.end());
    const std::uint32_t crc = computeCrc32(
        std::span<const std::uint8_t>(png.data() + type_offset, 4u + payload.size()));
    appendBigEndianU32(png, crc);
}

} // namespace

core::Result<std::vector<std::uint8_t>> encodeRgba8ToPng(
    std::uint32_t width,
    std::uint32_t height,
    std::span<const std::uint8_t> rgba_top_down) {
    if (width == 0 || height == 0) {
        return core::Error(
            core::ErrorCode::InvalidArgument,
            "PNG dimensions must be non-zero");
    }
    const std::size_t row_bytes = static_cast<std::size_t>(width) * 4u;
    const std::size_t expected_size = row_bytes * static_cast<std::size_t>(height);
    if (rgba_top_down.size() < expected_size) {
        return core::Error(
            core::ErrorCode::InvalidArgument,
            "RGBA8 pixel buffer is smaller than width * height * 4");
    }

    // Build filtered scanlines (filter byte 0 = None per row).
    std::vector<std::uint8_t> raw_scanlines;
    raw_scanlines.reserve(static_cast<std::size_t>(height) * (1u + row_bytes));
    for (std::uint32_t y = 0; y < height; ++y) {
        raw_scanlines.push_back(0u);
        const auto* row_ptr = rgba_top_down.data() + static_cast<std::size_t>(y) * row_bytes;
        raw_scanlines.insert(raw_scanlines.end(), row_ptr, row_ptr + row_bytes);
    }

    // Wrap raw scanlines inside a valid RFC-1950 zlib stream using stored (BTYPE=00) deflate blocks.
    std::vector<std::uint8_t> zlib_stream;
    zlib_stream.reserve(raw_scanlines.size() + (raw_scanlines.size() / 65535u + 2u) * 5u + 6u);
    zlib_stream.push_back(0x78u);
    zlib_stream.push_back(0x01u);

    std::size_t pos = 0;
    while (pos < raw_scanlines.size()) {
        const std::size_t remain = raw_scanlines.size() - pos;
        const std::uint16_t block_len = static_cast<std::uint16_t>(std::min<std::size_t>(remain, 65535u));
        const bool is_final = (pos + block_len) >= raw_scanlines.size();
        zlib_stream.push_back(is_final ? 0x01u : 0x00u);
        zlib_stream.push_back(static_cast<std::uint8_t>(block_len & 0xFFu));
        zlib_stream.push_back(static_cast<std::uint8_t>((block_len >> 8u) & 0xFFu));
        const std::uint16_t nlen = static_cast<std::uint16_t>(~block_len);
        zlib_stream.push_back(static_cast<std::uint8_t>(nlen & 0xFFu));
        zlib_stream.push_back(static_cast<std::uint8_t>((nlen >> 8u) & 0xFFu));
        zlib_stream.insert(
            zlib_stream.end(),
            raw_scanlines.data() + pos,
            raw_scanlines.data() + pos + block_len);
        pos += block_len;
    }
    appendBigEndianU32(zlib_stream, computeAdler32(raw_scanlines));

    std::vector<std::uint8_t> png;
    png.reserve(zlib_stream.size() + 64u);
    constexpr std::array<std::uint8_t, 8> kPngSig = {
        0x89u, 0x50u, 0x4Eu, 0x47u, 0x0Du, 0x0Au, 0x1Au, 0x0Au};
    png.insert(png.end(), kPngSig.begin(), kPngSig.end());

    std::vector<std::uint8_t> ihdr;
    ihdr.reserve(13u);
    appendBigEndianU32(ihdr, width);
    appendBigEndianU32(ihdr, height);
    ihdr.push_back(8u); // Bit depth: 8 bits per channel
    ihdr.push_back(6u); // Color type: Truecolor with alpha (RGBA)
    ihdr.push_back(0u); // Compression: Deflate
    ihdr.push_back(0u); // Filter: Adaptive
    ihdr.push_back(0u); // Interlace: None
    appendPngChunk(png, {'I', 'H', 'D', 'R'}, ihdr);
    appendPngChunk(png, {'I', 'D', 'A', 'T'}, zlib_stream);
    appendPngChunk(png, {'I', 'E', 'N', 'D'}, {});

    return png;
}

} // namespace finaleffect::ui
