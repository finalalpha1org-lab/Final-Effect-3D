// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/scene/environment.hpp"
#include "finaleffect/math/constants.hpp"

#include <algorithm>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstring>
#include <mutex>
#include <sstream>
#include <unordered_map>

namespace finaleffect::scene {

std::uint16_t floatToHalfBits(float value) noexcept {
    if (!std::isfinite(value)) {
        return 0u;
    }
    const float clamped = std::clamp(value, -65504.0f, 65504.0f);
    const std::uint32_t bits = std::bit_cast<std::uint32_t>(clamped);
    const std::uint32_t sign = (bits >> 16u) & 0x8000u;
    const std::int32_t exp32 = static_cast<std::int32_t>((bits >> 23u) & 0xFFu) - 127 + 15;
    std::uint32_t mant = bits & 0x007FFFFFu;

    if (exp32 <= 0) {
        if (exp32 < -10) {
            return static_cast<std::uint16_t>(sign);
        }
        mant = (mant | 0x00800000u) >> static_cast<std::uint32_t>(1 - exp32);
        return static_cast<std::uint16_t>(sign | ((mant + 0x00001000u) >> 13u));
    }
    if (exp32 >= 31) {
        return static_cast<std::uint16_t>(sign | 0x7BFFu);
    }
    const std::uint32_t rounded = mant + 0x00001000u;
    if ((rounded & 0x00800000u) != 0u) {
        const std::int32_t next_exp = exp32 + 1;
        if (next_exp >= 31) {
            return static_cast<std::uint16_t>(sign | 0x7BFFu);
        }
        return static_cast<std::uint16_t>(sign | (static_cast<std::uint32_t>(next_exp) << 10u));
    }
    return static_cast<std::uint16_t>(
        sign | (static_cast<std::uint32_t>(exp32) << 10u) | (rounded >> 13u));
}

namespace {

constexpr float kMaxSafeRadiance = 65000.0f;

math::Vec3 sanitizeRadiance(const math::Vec3& c) noexcept {
    return math::Vec3{
        std::isfinite(c.x) ? std::clamp(c.x, 0.0f, kMaxSafeRadiance) : 0.0f,
        std::isfinite(c.y) ? std::clamp(c.y, 0.0f, kMaxSafeRadiance) : 0.0f,
        std::isfinite(c.z) ? std::clamp(c.z, 0.0f, kMaxSafeRadiance) : 0.0f,
    };
}

// Khronos OpenGL 3.3 / ES 3.0 Table 3.19 cubemap face texel direction for u,v in [-1, 1].
math::Vec3 cubemapFaceUvToDir(std::uint32_t face, float u, float v) noexcept {
    math::Vec3 d{};
    switch (face) {
        case 0: d = {+1.0f, -v, -u}; break; // +X
        case 1: d = {-1.0f, -v, +u}; break; // -X
        case 2: d = {+u, +1.0f, +v}; break; // +Y
        case 3: d = {+u, -1.0f, -v}; break; // -Y
        case 4: d = {+u, -v, +1.0f}; break; // +Z
        default: d = {-u, -v, -1.0f}; break; // -Z
    }
    return d.normalized();
}

void dirToCubemapFaceUv(const math::Vec3& dir, std::uint32_t& out_face, float& out_s, float& out_t) noexcept {
    const float ax = std::fabs(dir.x);
    const float ay = std::fabs(dir.y);
    const float az = std::fabs(dir.z);
    float sc = 0.0f;
    float tc = 0.0f;
    float ma = 1.0f;

    if (ax >= ay && ax >= az) {
        ma = std::max(ax, 1.0e-6f);
        if (dir.x >= 0.0f) {
            out_face = 0;
            sc = -dir.z;
            tc = -dir.y;
        } else {
            out_face = 1;
            sc = +dir.z;
            tc = -dir.y;
        }
    } else if (ay >= ax && ay >= az) {
        ma = std::max(ay, 1.0e-6f);
        if (dir.y >= 0.0f) {
            out_face = 2;
            sc = +dir.x;
            tc = +dir.z;
        } else {
            out_face = 3;
            sc = +dir.x;
            tc = -dir.z;
        }
    } else {
        ma = std::max(az, 1.0e-6f);
        if (dir.z >= 0.0f) {
            out_face = 4;
            sc = +dir.x;
            tc = -dir.y;
        } else {
            out_face = 5;
            sc = -dir.x;
            tc = -dir.y;
        }
    }
    out_s = std::clamp(0.5f * (sc / ma + 1.0f), 0.0f, 1.0f);
    out_t = std::clamp(0.5f * (tc / ma + 1.0f), 0.0f, 1.0f);
}

math::Vec3 sampleCubemapMipBilinear(
    const PrefilteredEnvCubemap::MipLevel& mip,
    std::uint32_t face,
    float s,
    float t) noexcept {
    if (mip.size == 0 || mip.linear_rgb.empty()) {
        return math::Vec3::zero();
    }
    const float sz = static_cast<float>(mip.size);
    const float fx = std::clamp(s * sz - 0.5f, 0.0f, sz - 1.0f);
    const float fy = std::clamp(t * sz - 0.5f, 0.0f, sz - 1.0f);

    const auto x0 = static_cast<std::uint32_t>(std::floor(fx));
    const auto y0 = static_cast<std::uint32_t>(std::floor(fy));
    const std::uint32_t x1 = std::min(x0 + 1u, mip.size - 1u);
    const std::uint32_t y1 = std::min(y0 + 1u, mip.size - 1u);
    const float tx = fx - static_cast<float>(x0);
    const float ty = fy - static_cast<float>(y0);

    const std::size_t base = static_cast<std::size_t>(face) * mip.size * mip.size;
    const math::Vec3& c00 = mip.linear_rgb[base + static_cast<std::size_t>(y0) * mip.size + x0];
    const math::Vec3& c10 = mip.linear_rgb[base + static_cast<std::size_t>(y0) * mip.size + x1];
    const math::Vec3& c01 = mip.linear_rgb[base + static_cast<std::size_t>(y1) * mip.size + x0];
    const math::Vec3& c11 = mip.linear_rgb[base + static_cast<std::size_t>(y1) * mip.size + x1];

    const math::Vec3 top = c00 * (1.0f - tx) + c10 * tx;
    const math::Vec3 bot = c01 * (1.0f - tx) + c11 * tx;
    return top * (1.0f - ty) + bot * ty;
}

float radicalInverseVdC(std::uint32_t bits) noexcept {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return static_cast<float>(bits) * 2.3283064365386963e-10f;
}

math::Vec3 importanceSampleGgx(
    float xi_1,
    float xi_2,
    const math::Vec3& normal,
    float roughness) noexcept {
    const float a = roughness * roughness;
    const float phi = math::kTwoPi * xi_1;
    const float cos_theta = std::sqrt((1.0f - xi_2) / std::max(1.0f + (a * a - 1.0f) * xi_2, 1.0e-6f));
    const float sin_theta = std::sqrt(std::max(0.0f, 1.0f - cos_theta * cos_theta));

    const math::Vec3 h_local{
        std::cos(phi) * sin_theta,
        std::sin(phi) * sin_theta,
        cos_theta,
    };

    const math::Vec3 up = (std::fabs(normal.z) < 0.999f) ? math::Vec3{0.0f, 0.0f, 1.0f}
                                                         : math::Vec3{1.0f, 0.0f, 0.0f};
    const math::Vec3 tangent = up.cross(normal).normalized();
    const math::Vec3 bitangent = normal.cross(tangent);
    return (tangent * h_local.x + bitangent * h_local.y + normal * h_local.z).normalized();
}

math::Vec3 rgbeToLinear(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t e, float inv_exposure) noexcept {
    if (e == 0u) {
        return math::Vec3::zero();
    }
    const float scale = std::ldexp(1.0f, static_cast<int>(e) - (128 + 8)) * inv_exposure;
    return sanitizeRadiance(math::Vec3{
        (static_cast<float>(r) + 0.5f) * scale,
        (static_cast<float>(g) + 0.5f) * scale,
        (static_cast<float>(b) + 0.5f) * scale,
    });
}

void linearToRgbe(const math::Vec3& c_raw, std::uint8_t out_rgbe[4]) noexcept {
    const math::Vec3 c = sanitizeRadiance(c_raw);
    const float v = std::max(c.x, std::max(c.y, c.z));
    if (v < 1.0e-32f) {
        out_rgbe[0] = 0;
        out_rgbe[1] = 0;
        out_rgbe[2] = 0;
        out_rgbe[3] = 0;
        return;
    }
    int exponent = 0;
    const float mantissa = std::frexp(v, &exponent) * 256.0f / v;
    const int biased = exponent + 128;
    if (biased <= 0) {
        out_rgbe[0] = out_rgbe[1] = out_rgbe[2] = out_rgbe[3] = 0;
        return;
    }
    if (biased >= 255) {
        out_rgbe[0] = out_rgbe[1] = out_rgbe[2] = 255;
        out_rgbe[3] = 254;
        return;
    }
    out_rgbe[0] = static_cast<std::uint8_t>(std::clamp(static_cast<int>(c.x * mantissa), 0, 255));
    out_rgbe[1] = static_cast<std::uint8_t>(std::clamp(static_cast<int>(c.y * mantissa), 0, 255));
    out_rgbe[2] = static_cast<std::uint8_t>(std::clamp(static_cast<int>(c.z * mantissa), 0, 255));
    out_rgbe[3] = static_cast<std::uint8_t>(biased);
}

std::uint64_t fnv1aHash64(std::span<const std::uint8_t> bytes) noexcept {
    std::uint64_t h = 14695981039346656037ull;
    for (const std::uint8_t b : bytes) {
        h ^= static_cast<std::uint64_t>(b);
        h *= 1099511628211ull;
    }
    return h;
}

core::Result<HdriImage> decodeRadianceHdr(
    std::span<const std::uint8_t> bytes,
    std::string_view debug_name) {
    std::size_t pos = 0;
    auto readLine = [&](std::string& line) -> bool {
        line.clear();
        if (pos >= bytes.size()) {
            return false;
        }
        while (pos < bytes.size()) {
            const char ch = static_cast<char>(bytes[pos++]);
            if (ch == '\n') {
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }
                return true;
            }
            line.push_back(ch);
            if (line.size() > 1024u) {
                return false;
            }
        }
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        return true;
    };

    std::string first_line;
    if (!readLine(first_line) ||
        (!first_line.starts_with("#?RADIANCE") &&
         !first_line.starts_with("#?RGBE") &&
         !first_line.starts_with("FORMAT=32-bit_rle_rgbe"))) {
        return core::Result<HdriImage>::fail(
            core::ErrorCode::ParseError,
            "Invalid Radiance .hdr header magic",
            std::string(debug_name));
    }

    float exposure = 1.0f;
    bool saw_blank_separator = false;
    std::string line;
    if (first_line.starts_with("FORMAT=")) {
        // Header started directly with FORMAT=; continue until blank line.
    }
    while (readLine(line)) {
        if (line.empty()) {
            saw_blank_separator = true;
            break;
        }
        if (line.starts_with("FORMAT=")) {
            const std::string_view fmt = std::string_view(line).substr(7);
            if (fmt != "32-bit_rle_rgbe" && fmt != "32-bit_rle_xyze") {
                return core::Result<HdriImage>::fail(
                    core::ErrorCode::UnsupportedVersion,
                    "Unsupported Radiance HDR format: " + std::string(fmt),
                    std::string(debug_name));
            }
        } else if (line.starts_with("EXPOSURE=")) {
            try {
                const float exp_val = std::stof(line.substr(9));
                if (std::isfinite(exp_val) && exp_val > 1.0e-6f) {
                    exposure *= exp_val;
                }
            } catch (...) {
            }
        }
    }

    if (!saw_blank_separator) {
        return core::Result<HdriImage>::fail(
            core::ErrorCode::ParseError,
            "Malformed Radiance .hdr header (missing blank separator line)",
            std::string(debug_name));
    }

    std::string res_line;
    if (!readLine(res_line)) {
        return core::Result<HdriImage>::fail(
            core::ErrorCode::ParseError,
            "Missing Radiance .hdr resolution line",
            std::string(debug_name));
    }

    // Parse standard "-Y <height> +X <width>" (also accept "+Y <height> +X <width>").
    std::istringstream iss(res_line);
    std::string y_tag, x_tag;
    int h_signed = 0;
    int w_signed = 0;
    if (!(iss >> y_tag >> h_signed >> x_tag >> w_signed) ||
        (y_tag != "-Y" && y_tag != "+Y") ||
        (x_tag != "+X" && x_tag != "-X") ||
        w_signed <= 0 || h_signed <= 0) {
        return core::Result<HdriImage>::fail(
            core::ErrorCode::ParseError,
            "Unsupported or invalid Radiance .hdr resolution specification",
            std::string(debug_name));
    }

    const auto width = static_cast<std::uint32_t>(w_signed);
    const auto height = static_cast<std::uint32_t>(h_signed);
    if (width > HdriImage::kMaxWidth || height > HdriImage::kMaxHeight) {
        return core::Result<HdriImage>::fail(
            core::ErrorCode::ValidationError,
            "HDRI dimensions exceed maximum supported resolution (4096x2048)",
            std::string(debug_name));
    }

    const bool flip_y = (y_tag == "+Y");
    const bool flip_x = (x_tag == "-X");
    const float inv_exposure = 1.0f / std::max(exposure, 1.0e-6f);

    HdriImage img;
    img.width = width;
    img.height = height;
    img.pixels.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));

    std::vector<std::uint8_t> scanline(static_cast<std::size_t>(width) * 4u, 0u);

    for (std::uint32_t y = 0; y < height; ++y) {
        if (pos + 4u > bytes.size()) {
            return core::Result<HdriImage>::fail(
                core::ErrorCode::ParseError,
                "Truncated Radiance .hdr scanline data",
                std::string(debug_name));
        }

        const std::uint8_t b0 = bytes[pos];
        const std::uint8_t b1 = bytes[pos + 1];
        const std::uint8_t b2 = bytes[pos + 2];
        const std::uint8_t b3 = bytes[pos + 3];

        const bool is_adaptive_rle =
            (width >= 8u && width <= 0x7FFFu && b0 == 2u && b1 == 2u && (b2 & 0x80u) == 0u);

        if (is_adaptive_rle) {
            const std::uint32_t encoded_w = (static_cast<std::uint32_t>(b2) << 8u) | static_cast<std::uint32_t>(b3);
            if (encoded_w != width) {
                return core::Result<HdriImage>::fail(
                    core::ErrorCode::ParseError,
                    "Radiance .hdr RLE scanline width mismatch",
                    std::string(debug_name));
            }
            pos += 4u;

            for (std::uint32_t ch = 0; ch < 4u; ++ch) {
                std::uint32_t x = 0;
                while (x < width) {
                    if (pos >= bytes.size()) {
                        return core::Result<HdriImage>::fail(
                            core::ErrorCode::ParseError,
                            "Truncated Radiance .hdr RLE run",
                            std::string(debug_name));
                    }
                    const std::uint8_t code = bytes[pos++];
                    if (code > 128u) {
                        const std::uint32_t run_len = static_cast<std::uint32_t>(code - 128u);
                        if (pos >= bytes.size() || x + run_len > width) {
                            return core::Result<HdriImage>::fail(
                                core::ErrorCode::ParseError,
                                "Corrupt Radiance .hdr RLE repeat length",
                                std::string(debug_name));
                        }
                        const std::uint8_t val = bytes[pos++];
                        for (std::uint32_t k = 0; k < run_len; ++k) {
                            scanline[static_cast<std::size_t>(x++) * 4u + ch] = val;
                        }
                    } else {
                        const std::uint32_t lit_len = static_cast<std::uint32_t>(code);
                        if (lit_len == 0u || pos + lit_len > bytes.size() || x + lit_len > width) {
                            return core::Result<HdriImage>::fail(
                                core::ErrorCode::ParseError,
                                "Corrupt Radiance .hdr RLE literal length",
                                std::string(debug_name));
                        }
                        for (std::uint32_t k = 0; k < lit_len; ++k) {
                            scanline[static_cast<std::size_t>(x++) * 4u + ch] = bytes[pos++];
                        }
                    }
                }
            }
        } else {
            const std::size_t row_bytes = static_cast<std::size_t>(width) * 4u;
            if (pos + row_bytes > bytes.size()) {
                return core::Result<HdriImage>::fail(
                    core::ErrorCode::ParseError,
                    "Truncated uncompressed Radiance .hdr scanline",
                    std::string(debug_name));
            }
            std::memcpy(scanline.data(), bytes.data() + pos, row_bytes);
            pos += row_bytes;
        }

        const std::uint32_t dst_y = flip_y ? (height - 1u - y) : y;
        const std::size_t dst_row = static_cast<std::size_t>(dst_y) * static_cast<std::size_t>(width);
        for (std::uint32_t x = 0; x < width; ++x) {
            const std::uint32_t dst_x = flip_x ? (width - 1u - x) : x;
            const std::size_t s = static_cast<std::size_t>(x) * 4u;
            img.pixels[dst_row + dst_x] =
                rgbeToLinear(scanline[s + 0], scanline[s + 1], scanline[s + 2], scanline[s + 3], inv_exposure);
        }
    }

    return core::Result<HdriImage>::ok(std::move(img));
}

core::Result<HdriImage> decodePfmHdr(
    std::span<const std::uint8_t> bytes,
    std::string_view debug_name) {
    std::size_t pos = 0;
    auto skipCommentsAndWs = [&]() {
        while (pos < bytes.size()) {
            const char c = static_cast<char>(bytes[pos]);
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                ++pos;
            } else if (c == '#') {
                while (pos < bytes.size() && static_cast<char>(bytes[pos]) != '\n') {
                    ++pos;
                }
            } else {
                break;
            }
        }
    };
    auto nextToken = [&](std::string& tok) -> bool {
        skipCommentsAndWs();
        tok.clear();
        while (pos < bytes.size()) {
            const char c = static_cast<char>(bytes[pos]);
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                break;
            }
            tok.push_back(c);
            ++pos;
            if (tok.size() > 128u) {
                return false;
            }
        }
        return !tok.empty();
    };

    std::string magic, w_str, h_str, scale_str;
    if (!nextToken(magic) || (magic != "PF" && magic != "Pf") ||
        !nextToken(w_str) || !nextToken(h_str) || !nextToken(scale_str)) {
        return core::Result<HdriImage>::fail(
            core::ErrorCode::ParseError,
            "Invalid Portable FloatMap (.pfm) header",
            std::string(debug_name));
    }
    // Consume exactly one single whitespace byte separating header from binary float payload.
    if (pos < bytes.size() && static_cast<char>(bytes[pos]) == '\r') {
        ++pos;
    }
    if (pos >= bytes.size() ||
        (static_cast<char>(bytes[pos]) != '\n' && static_cast<char>(bytes[pos]) != ' ')) {
        return core::Result<HdriImage>::fail(
            core::ErrorCode::ParseError,
            "Missing single-byte whitespace delimiter after .pfm scale factor",
            std::string(debug_name));
    }
    ++pos;

    int w_signed = 0;
    int h_signed = 0;
    float scale_factor = 0.0f;
    try {
        w_signed = std::stoi(w_str);
        h_signed = std::stoi(h_str);
        scale_factor = std::stof(scale_str);
    } catch (...) {
        return core::Result<HdriImage>::fail(
            core::ErrorCode::ParseError,
            "Malformed numeric header fields in .pfm",
            std::string(debug_name));
    }

    if (w_signed <= 0 || h_signed <= 0 || !std::isfinite(scale_factor) || std::fabs(scale_factor) <= 1.0e-8f) {
        return core::Result<HdriImage>::fail(
            core::ErrorCode::ValidationError,
            "Invalid .pfm dimensions or zero scale factor",
            std::string(debug_name));
    }

    const auto width = static_cast<std::uint32_t>(w_signed);
    const auto height = static_cast<std::uint32_t>(h_signed);
    if (width > HdriImage::kMaxWidth || height > HdriImage::kMaxHeight) {
        return core::Result<HdriImage>::fail(
            core::ErrorCode::ValidationError,
            ".pfm dimensions exceed maximum supported resolution (4096x2048)",
            std::string(debug_name));
    }

    const std::size_t channels = (magic == "PF") ? 3u : 1u;
    const std::size_t float_count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * channels;
    const std::size_t needed_bytes = float_count * sizeof(float);
    if (pos + needed_bytes > bytes.size()) {
        return core::Result<HdriImage>::fail(
            core::ErrorCode::ParseError,
            "Truncated .pfm binary float payload",
            std::string(debug_name));
    }

    const bool file_big_endian = (scale_factor > 0.0f);
    const float abs_scale = std::fabs(scale_factor);
    const bool host_big_endian = (std::endian::native == std::endian::big);
    const bool need_swap = (file_big_endian != host_big_endian);

    auto readFloat = [&](std::size_t index) noexcept -> float {
        std::uint32_t raw = 0;
        std::memcpy(&raw, bytes.data() + pos + index * 4u, 4u);
        if (need_swap) {
            raw = ((raw & 0x000000FFu) << 24u) |
                  ((raw & 0x0000FF00u) << 8u) |
                  ((raw & 0x00FF0000u) >> 8u) |
                  ((raw & 0xFF000000u) >> 24u);
        }
        return std::bit_cast<float>(raw) * abs_scale;
    };

    HdriImage img;
    img.width = width;
    img.height = height;
    img.pixels.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));

    // PFM stores rows bottom-to-top; flip into top-to-bottom equirectangular convention.
    for (std::uint32_t row_pfm = 0; row_pfm < height; ++row_pfm) {
        const std::uint32_t dst_y = height - 1u - row_pfm;
        const std::size_t dst_row = static_cast<std::size_t>(dst_y) * width;
        const std::size_t src_row = static_cast<std::size_t>(row_pfm) * width * channels;
        for (std::uint32_t x = 0; x < width; ++x) {
            const std::size_t base = src_row + static_cast<std::size_t>(x) * channels;
            if (channels == 3u) {
                img.pixels[dst_row + x] = sanitizeRadiance(math::Vec3{
                    readFloat(base + 0u),
                    readFloat(base + 1u),
                    readFloat(base + 2u),
                });
            } else {
                const float g = readFloat(base);
                img.pixels[dst_row + x] = sanitizeRadiance(math::Vec3{g, g, g});
            }
        }
    }

    return core::Result<HdriImage>::ok(std::move(img));
}

SphericalHarmonics9 projectEquirectangularToSh9(const HdriImage& img) noexcept {
    SphericalHarmonics9 sh{};
    if (!img.isValid()) {
        return sh;
    }

    // Subsample huge equirectangular maps to at most 256x128 for fast (<1ms) exact solid-angle quadrature.
    const std::uint32_t sample_w = std::min<std::uint32_t>(img.width, 256u);
    const std::uint32_t sample_h = std::min<std::uint32_t>(img.height, 128u);
    const float d_phi = math::kTwoPi / static_cast<float>(sample_w);
    const float d_theta = math::kPi / static_cast<float>(sample_h);

    std::array<math::Vec3, 9> l_lm{};
    float weight_sum = 0.0f;

    for (std::uint32_t y = 0; y < sample_h; ++y) {
        const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(sample_h);
        const float theta = v * math::kPi;
        const float sin_theta = std::sin(theta);
        const float cos_theta = std::cos(theta);
        const float dw = sin_theta * d_theta * d_phi;

        for (std::uint32_t x = 0; x < sample_w; ++x) {
            const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(sample_w);
            const float phi = (u - 0.5f) * math::kTwoPi;
            // Right-handed Y-up equirectangular convention:
            // u = 0.5 -> +Z (phi = 0), u = 0.75 -> +X (phi = +pi/2), v = 0 -> +Y, v = 1 -> -Y.
            const math::Vec3 dir{
                sin_theta * std::sin(phi),
                cos_theta,
                sin_theta * std::cos(phi),
            };
            const math::Vec3 rad = img.sampleEquirectangular(dir);
            weight_sum += dw;

            const float y00  = 0.282095f;
            const float y1_1 = 0.488603f * dir.y;
            const float y10  = 0.488603f * dir.z;
            const float y11  = 0.488603f * dir.x;
            const float y2_2 = 1.092548f * dir.x * dir.y;
            const float y2_1 = 1.092548f * dir.y * dir.z;
            const float y20  = 0.315392f * (3.0f * dir.z * dir.z - 1.0f);
            const float y21  = 1.092548f * dir.x * dir.z;
            const float y22  = 0.546274f * (dir.x * dir.x - dir.y * dir.y);

            l_lm[0] += rad * (y00 * dw);
            l_lm[1] += rad * (y1_1 * dw);
            l_lm[2] += rad * (y10 * dw);
            l_lm[3] += rad * (y11 * dw);
            l_lm[4] += rad * (y2_2 * dw);
            l_lm[5] += rad * (y2_1 * dw);
            l_lm[6] += rad * (y20 * dw);
            l_lm[7] += rad * (y21 * dw);
            l_lm[8] += rad * (y22 * dw);
        }
    }

    const float norm_fix = (weight_sum > 1.0e-5f) ? ((4.0f * math::kPi) / weight_sum) : 1.0f;
    // Pre-convolve with normalized Lambertian cosine lobe (A_l / pi) so evaluateIrradiance
    // returns diffuse irradiance / pi directly (ready to multiply by kd * albedo)!
    constexpr float kA0OverPi = 1.0f;          // pi / pi
    constexpr float kA1OverPi = 2.0f / 3.0f;   // (2*pi/3) / pi
    constexpr float kA2OverPi = 0.25f;         // (pi/4) / pi

    sh.coeffs[0] = l_lm[0] * (norm_fix * kA0OverPi * 0.282095f);
    sh.coeffs[1] = l_lm[1] * (norm_fix * kA1OverPi * 0.488603f);
    sh.coeffs[2] = l_lm[2] * (norm_fix * kA1OverPi * 0.488603f);
    sh.coeffs[3] = l_lm[3] * (norm_fix * kA1OverPi * 0.488603f);
    sh.coeffs[4] = l_lm[4] * (norm_fix * kA2OverPi * 1.092548f);
    sh.coeffs[5] = l_lm[5] * (norm_fix * kA2OverPi * 1.092548f);
    sh.coeffs[6] = l_lm[6] * (norm_fix * kA2OverPi * 0.315392f);
    sh.coeffs[7] = l_lm[7] * (norm_fix * kA2OverPi * 1.092548f);
    sh.coeffs[8] = l_lm[8] * (norm_fix * kA2OverPi * 0.546274f);
    return sh;
}

PrefilteredEnvCubemap buildPrefilteredCubemap(const HdriImage& img) {
    PrefilteredEnvCubemap cube{};
    if (!img.isValid()) {
        return cube;
    }

    // Mip 0: 64x64 sharp reflection cubemap (2x2 stratified supersample per texel).
    {
        auto& mip0 = cube.mips[0];
        mip0.size = PrefilteredEnvCubemap::kBaseSize;
        const std::size_t texel_count = 6u * mip0.size * mip0.size;
        mip0.linear_rgb.resize(texel_count);
        mip0.rgba16f_half.resize(texel_count * 4u);

        const float inv_s = 1.0f / static_cast<float>(mip0.size);
        for (std::uint32_t face = 0; face < 6u; ++face) {
            for (std::uint32_t y = 0; y < mip0.size; ++y) {
                for (std::uint32_t x = 0; x < mip0.size; ++x) {
                    math::Vec3 acc{};
                    for (int sy = 0; sy < 2; ++sy) {
                        for (int sx = 0; sx < 2; ++sx) {
                            const float u = 2.0f * ((static_cast<float>(x) + (sx == 0 ? 0.25f : 0.75f)) * inv_s) - 1.0f;
                            const float v = 2.0f * ((static_cast<float>(y) + (sy == 0 ? 0.25f : 0.75f)) * inv_s) - 1.0f;
                            acc += img.sampleEquirectangular(cubemapFaceUvToDir(face, u, v));
                        }
                    }
                    const math::Vec3 c = sanitizeRadiance(acc * 0.25f);
                    const std::size_t idx = (static_cast<std::size_t>(face) * mip0.size + y) * mip0.size + x;
                    mip0.linear_rgb[idx] = c;
                    mip0.rgba16f_half[idx * 4u + 0u] = floatToHalfBits(c.x);
                    mip0.rgba16f_half[idx * 4u + 1u] = floatToHalfBits(c.y);
                    mip0.rgba16f_half[idx * 4u + 2u] = floatToHalfBits(c.z);
                    mip0.rgba16f_half[idx * 4u + 3u] = floatToHalfBits(1.0f);
                }
            }
        }
    }

    // Mips 1..4: GGX importance-sampled specular prefiltered levels (roughness = 0.25, 0.50, 0.75, 1.00)
    // sampling from Mip 0 cubemap for fast, alias-free convolution.
    constexpr std::uint32_t kGgxSamples = 20u;
    for (std::uint32_t level = 1; level < PrefilteredEnvCubemap::kMipCount; ++level) {
        auto& mip = cube.mips[level];
        mip.size = PrefilteredEnvCubemap::kBaseSize >> level;
        const std::size_t texel_count = 6u * mip.size * mip.size;
        mip.linear_rgb.resize(texel_count);
        mip.rgba16f_half.resize(texel_count * 4u);

        const float roughness = static_cast<float>(level) / static_cast<float>(PrefilteredEnvCubemap::kMipCount - 1u);
        const float inv_s = 1.0f / static_cast<float>(mip.size);

        for (std::uint32_t face = 0; face < 6u; ++face) {
            for (std::uint32_t y = 0; y < mip.size; ++y) {
                for (std::uint32_t x = 0; x < mip.size; ++x) {
                    const float u = 2.0f * ((static_cast<float>(x) + 0.5f) * inv_s) - 1.0f;
                    const float v = 2.0f * ((static_cast<float>(y) + 0.5f) * inv_s) - 1.0f;
                    const math::Vec3 n = cubemapFaceUvToDir(face, u, v);
                    const math::Vec3 v_dir = n;

                    math::Vec3 prefiltered{};
                    float total_weight = 0.0f;

                    for (std::uint32_t si = 0; si < kGgxSamples; ++si) {
                        const float xi_1 = static_cast<float>(si) / static_cast<float>(kGgxSamples);
                        const float xi_2 = radicalInverseVdC(si);
                        const math::Vec3 h = importanceSampleGgx(xi_1, xi_2, n, roughness);
                        const math::Vec3 l = (h * (2.0f * v_dir.dot(h)) - v_dir).normalized();
                        const float n_dot_l = std::max(n.dot(l), 0.0f);
                        if (n_dot_l > 0.0f) {
                            std::uint32_t s_face = 0;
                            float s_u = 0.5f;
                            float s_v = 0.5f;
                            dirToCubemapFaceUv(l, s_face, s_u, s_v);
                            // Clamp fireflies slightly during wide-lobe convolution for smooth mips.
                            const math::Vec3 sample_col = sampleCubemapMipBilinear(cube.mips[0], s_face, s_u, s_v);
                            prefiltered += sample_col * n_dot_l;
                            total_weight += n_dot_l;
                        }
                    }

                    const math::Vec3 c = (total_weight > 1.0e-5f)
                                             ? sanitizeRadiance(prefiltered * (1.0f / total_weight))
                                             : math::Vec3::zero();
                    const std::size_t idx = (static_cast<std::size_t>(face) * mip.size + y) * mip.size + x;
                    mip.linear_rgb[idx] = c;
                    mip.rgba16f_half[idx * 4u + 0u] = floatToHalfBits(c.x);
                    mip.rgba16f_half[idx * 4u + 1u] = floatToHalfBits(c.y);
                    mip.rgba16f_half[idx * 4u + 2u] = floatToHalfBits(c.z);
                    mip.rgba16f_half[idx * 4u + 3u] = floatToHalfBits(1.0f);
                }
            }
        }
    }

    return cube;
}

HdriImage downsampleForPortableEmbed(const HdriImage& src) {
    if (src.width <= 128u && src.height <= 64u) {
        return src;
    }
    HdriImage dst;
    dst.width = std::min<std::uint32_t>(src.width, 128u);
    dst.height = std::min<std::uint32_t>(src.height, 64u);
    dst.pixels.resize(static_cast<std::size_t>(dst.width) * dst.height);
    for (std::uint32_t y = 0; y < dst.height; ++y) {
        const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(dst.height);
        const float theta = v * math::kPi;
        const float sin_t = std::sin(theta);
        const float cos_t = std::cos(theta);
        for (std::uint32_t x = 0; x < dst.width; ++x) {
            const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(dst.width);
            const float phi = (u - 0.5f) * math::kTwoPi;
            const math::Vec3 dir{sin_t * std::sin(phi), cos_t, sin_t * std::cos(phi)};
            dst.pixels[static_cast<std::size_t>(y) * dst.width + x] = src.sampleEquirectangular(dir);
        }
    }
    return dst;
}

struct CachedAssetSlot {
    std::weak_ptr<const HdriEnvironmentAsset> asset;
};

std::mutex g_hdri_cache_mutex;
std::unordered_map<std::uint64_t, CachedAssetSlot> g_hdri_cache;

} // namespace

math::Vec3 SphericalHarmonics9::evaluateIrradiance(const math::Vec3& unit_normal) const noexcept {
    const math::Vec3 n = unit_normal.normalized();
    const math::Vec3 raw =
        coeffs[0] +
        coeffs[1] * n.y +
        coeffs[2] * n.z +
        coeffs[3] * n.x +
        coeffs[4] * (n.x * n.y) +
        coeffs[5] * (n.y * n.z) +
        coeffs[6] * (3.0f * n.z * n.z - 1.0f) +
        coeffs[7] * (n.x * n.z) +
        coeffs[8] * (n.x * n.x - n.y * n.y);
    return math::Vec3{
        std::max(0.0f, raw.x),
        std::max(0.0f, raw.y),
        std::max(0.0f, raw.z),
    };
}

math::Vec3 PrefilteredEnvCubemap::sampleLod(const math::Vec3& direction, float lod) const noexcept {
    if (!isValid() || direction.lengthSquared() <= math::kStrictEpsilon) {
        return math::Vec3::zero();
    }
    std::uint32_t face = 0;
    float s = 0.5f;
    float t = 0.5f;
    dirToCubemapFaceUv(direction.normalized(), face, s, t);

    const float clamped_lod = std::clamp(lod, 0.0f, static_cast<float>(kMipCount - 1u));
    const auto lod0 = static_cast<std::uint32_t>(std::floor(clamped_lod));
    const std::uint32_t lod1 = std::min(lod0 + 1u, kMipCount - 1u);
    const float frac = clamped_lod - static_cast<float>(lod0);

    const math::Vec3 c0 = sampleCubemapMipBilinear(mips[lod0], face, s, t);
    if (lod0 == lod1 || frac <= 1.0e-5f) {
        return c0;
    }
    const math::Vec3 c1 = sampleCubemapMipBilinear(mips[lod1], face, s, t);
    return c0 * (1.0f - frac) + c1 * frac;
}

math::Vec3 HdriImage::sampleEquirectangular(const math::Vec3& unit_dir) const noexcept {
    if (!isValid()) {
        return math::Vec3::zero();
    }
    const math::Vec3 d = unit_dir.normalized();
    const float phi = std::atan2(d.x, d.z); // [-pi, pi]
    const float theta = std::acos(std::clamp(d.y, -1.0f, 1.0f)); // [0, pi]

    float u = phi * (1.0f / math::kTwoPi) + 0.5f;
    u = u - std::floor(u);
    const float v = std::clamp(theta * (1.0f / math::kPi), 0.0f, 1.0f);

    const float fx = u * static_cast<float>(width) - 0.5f;
    const float fy = std::clamp(v * static_cast<float>(height) - 0.5f, 0.0f, static_cast<float>(height - 1u));

    const int x0_raw = static_cast<int>(std::floor(fx));
    const float tx = fx - static_cast<float>(x0_raw);
    const auto w_i = static_cast<int>(width);
    const auto x0 = static_cast<std::uint32_t>(((x0_raw % w_i) + w_i) % w_i);
    const auto x1 = static_cast<std::uint32_t>(((x0_raw + 1) % w_i + w_i) % w_i);

    const auto y0 = static_cast<std::uint32_t>(std::floor(fy));
    const std::uint32_t y1 = std::min(y0 + 1u, height - 1u);
    const float ty = fy - static_cast<float>(y0);

    const math::Vec3& c00 = pixels[static_cast<std::size_t>(y0) * width + x0];
    const math::Vec3& c10 = pixels[static_cast<std::size_t>(y0) * width + x1];
    const math::Vec3& c01 = pixels[static_cast<std::size_t>(y1) * width + x0];
    const math::Vec3& c11 = pixels[static_cast<std::size_t>(y1) * width + x1];

    const math::Vec3 top = c00 * (1.0f - tx) + c10 * tx;
    const math::Vec3 bot = c01 * (1.0f - tx) + c11 * tx;
    return top * (1.0f - ty) + bot * ty;
}

core::Result<HdriImage> HdriImage::decodeFromMemory(
    std::span<const std::uint8_t> bytes,
    std::string_view debug_name) {
    if (bytes.size() < 10u) {
        return core::Result<HdriImage>::fail(
            core::ErrorCode::ParseError,
            "HDRI file is empty or too short",
            std::string(debug_name));
    }
    if ((bytes[0] == 'P' && (bytes[1] == 'F' || bytes[1] == 'f'))) {
        return decodePfmHdr(bytes, debug_name);
    }
    if (bytes[0] == '#' || bytes[0] == 'F') {
        return decodeRadianceHdr(bytes, debug_name);
    }
    return core::Result<HdriImage>::fail(
        core::ErrorCode::ParseError,
        "Unsupported HDRI file format (expected Radiance .hdr or Portable FloatMap .pfm)",
        std::string(debug_name));
}

core::Result<HdriImage> HdriImage::loadFromFile(
    const std::filesystem::path& path,
    const platform::IFileSystem& file_system) {
    auto bytes_res = file_system.readBinaryFile(path);
    if (!bytes_res) {
        return bytes_res.error();
    }
    return decodeFromMemory(bytes_res.value(), path.string());
}

core::Result<std::vector<std::uint8_t>> HdriImage::encodeRadianceHdr() const {
    if (!isValid()) {
        return core::Result<std::vector<std::uint8_t>>::fail(
            core::ErrorCode::ValidationError,
            "Cannot encode invalid HdriImage");
    }
    std::string header = "#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y " +
                         std::to_string(height) + " +X " + std::to_string(width) + "\n";
    std::vector<std::uint8_t> out;
    out.reserve(header.size() + pixels.size() * 4u);
    out.insert(out.end(), header.begin(), header.end());

    for (const auto& px : pixels) {
        std::uint8_t rgbe[4]{};
        linearToRgbe(px, rgbe);
        // Guard against accidental adaptive RLE sentinel (2, 2) on pixel 0 of a scanline when writing flat RGBE.
        if (rgbe[0] == 2u && rgbe[1] == 2u) {
            rgbe[1] = 3u;
        }
        out.insert(out.end(), rgbe, rgbe + 4);
    }
    return core::Result<std::vector<std::uint8_t>>::ok(std::move(out));
}

core::Result<std::shared_ptr<const HdriEnvironmentAsset>> HdriEnvironmentAsset::buildFromImage(
    const HdriImage& image,
    std::string asset_name) {
    if (!image.isValid()) {
        return core::Result<std::shared_ptr<const HdriEnvironmentAsset>>::fail(
            core::ErrorCode::ValidationError,
            "Invalid HdriImage passed to HdriEnvironmentAsset::buildFromImage");
    }

    auto asset = std::make_shared<HdriEnvironmentAsset>();
    asset->name = asset_name.empty() ? "Environment.hdr" : std::move(asset_name);
    asset->source_width = image.width;
    asset->source_height = image.height;
    asset->irradiance_sh = projectEquirectangularToSh9(image);
    asset->prefiltered_cubemap = buildPrefilteredCubemap(image);

    const HdriImage compact = downsampleForPortableEmbed(image);
    if (auto encoded = compact.encodeRadianceHdr(); encoded.hasValue()) {
        asset->portable_hdr_bytes = std::move(encoded).value();
        asset->content_hash = fnv1aHash64(asset->portable_hdr_bytes);
    }
    return core::Result<std::shared_ptr<const HdriEnvironmentAsset>>::ok(std::move(asset));
}

core::Result<std::shared_ptr<const HdriEnvironmentAsset>> HdriEnvironmentAsset::loadFromMemory(
    std::span<const std::uint8_t> bytes,
    std::string asset_name) {
    const std::uint64_t hash = fnv1aHash64(bytes);
    {
        std::scoped_lock lock(g_hdri_cache_mutex);
        if (const auto it = g_hdri_cache.find(hash); it != g_hdri_cache.end()) {
            if (auto existing = it->second.asset.lock()) {
                return core::Result<std::shared_ptr<const HdriEnvironmentAsset>>::ok(std::move(existing));
            }
        }
    }

    auto decoded = HdriImage::decodeFromMemory(bytes, asset_name);
    if (!decoded) {
        return decoded.error();
    }
    auto built_res = buildFromImage(decoded.value(), std::move(asset_name));
    if (!built_res) {
        return built_res.error();
    }
    auto mutable_copy = std::make_shared<HdriEnvironmentAsset>(*built_res.value());
    mutable_copy->content_hash = hash;
    std::shared_ptr<const HdriEnvironmentAsset> shared_const = std::move(mutable_copy);

    {
        std::scoped_lock lock(g_hdri_cache_mutex);
        if (g_hdri_cache.size() > 64u) {
            for (auto it = g_hdri_cache.begin(); it != g_hdri_cache.end();) {
                if (it->second.asset.expired()) {
                    it = g_hdri_cache.erase(it);
                } else {
                    ++it;
                }
            }
        }
        g_hdri_cache[hash] = CachedAssetSlot{shared_const};
    }
    return core::Result<std::shared_ptr<const HdriEnvironmentAsset>>::ok(std::move(shared_const));
}

core::Result<std::shared_ptr<const HdriEnvironmentAsset>> HdriEnvironmentAsset::loadFromFile(
    const std::filesystem::path& path,
    const platform::IFileSystem& file_system) {
    auto bytes_res = file_system.readBinaryFile(path);
    if (!bytes_res) {
        return bytes_res.error();
    }
    std::string name = path.filename().string();
    if (name.empty()) {
        name = path.string();
    }
    return loadFromMemory(bytes_res.value(), std::move(name));
}

} // namespace finaleffect::scene
