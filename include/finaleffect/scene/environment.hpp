// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include "finaleffect/core/result.hpp"
#include "finaleffect/math/vec3.hpp"
#include "finaleffect/platform/file_system.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace finaleffect::scene {

// Order-2 (9-coefficient) Ramamoorthi-Hanrahan Spherical Harmonics diffuse irradiance probe.
struct SphericalHarmonics9 {
    std::array<math::Vec3, 9> coeffs{};

    [[nodiscard]] math::Vec3 evaluateIrradiance(const math::Vec3& unit_normal) const noexcept;
};

// Precomputed 6-face GGX importance-sampled roughness cubemap pyramid (+X, -X, +Y, -Y, +Z, -Z).
// Stored in both linear float3 (for CPU software rendering & unit verification) and packed
// IEEE-754 binary16 RGBA16F (for seamless trilinear GPU textureLod sampling in OpenGL 3.3 & WebGL 2.0).
struct PrefilteredEnvCubemap {
    static constexpr std::uint32_t kBaseSize = 64;
    static constexpr std::uint32_t kMipCount = 5; // 64, 32, 16, 8, 4

    struct MipLevel {
        std::uint32_t size{0};
        // 6 faces contiguous: face * (size * size) + y * size + x.
        std::vector<math::Vec3> linear_rgb;
        // 6 faces contiguous RGBA16F half-floats: 4 * uint16_t per texel.
        std::vector<std::uint16_t> rgba16f_half;
    };

    std::array<MipLevel, kMipCount> mips{};

    [[nodiscard]] bool isValid() const noexcept {
        return mips[0].size == kBaseSize && !mips[0].linear_rgb.empty();
    }

    [[nodiscard]] std::size_t gpuByteSize() const noexcept {
        std::size_t bytes = 0;
        for (const auto& mip : mips) {
            bytes += mip.rgba16f_half.size() * sizeof(std::uint16_t);
        }
        return bytes;
    }

    // Trilinear CPU sampler matching OpenGL/WebGL2 `textureLod(samplerCube, dir, lod)`.
    [[nodiscard]] math::Vec3 sampleLod(const math::Vec3& direction, float lod) const noexcept;
};

// Decoded linear-space equirectangular HDR image (.hdr Radiance RGBE or .pfm Portable FloatMap).
struct HdriImage {
    static constexpr std::uint32_t kMaxWidth = 4096;
    static constexpr std::uint32_t kMaxHeight = 2048;

    std::uint32_t width{0};
    std::uint32_t height{0};
    // Row-major top-to-bottom linear RGB radiance values >= 0.
    std::vector<math::Vec3> pixels;

    [[nodiscard]] bool isValid() const noexcept {
        return width > 0 && height > 0 &&
               pixels.size() == static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    }

    [[nodiscard]] math::Vec3 sampleEquirectangular(const math::Vec3& unit_dir) const noexcept;

    [[nodiscard]] static core::Result<HdriImage> decodeFromMemory(
        std::span<const std::uint8_t> bytes,
        std::string_view debug_name = "memory.hdr");

    [[nodiscard]] static core::Result<HdriImage> loadFromFile(
        const std::filesystem::path& path,
        const platform::IFileSystem& file_system);

    // Encodes a valid HdriImage to uncompressed Radiance .hdr (RGBE) bytes for portable embedding & tests.
    [[nodiscard]] core::Result<std::vector<std::uint8_t>> encodeRadianceHdr() const;
};

// Immutable prefiltered HDRI + IBL resource shared safely across undo snapshots and Scene copies.
struct HdriEnvironmentAsset {
    std::string name;
    std::uint32_t source_width{0};
    std::uint32_t source_height{0};
    std::uint64_t content_hash{0};
    SphericalHarmonics9 irradiance_sh{};
    PrefilteredEnvCubemap prefiltered_cubemap{};
    // Compact Radiance RGBE proxy (<= 128x64) retained so portable inline .fe3d saves preserve live IBL.
    std::vector<std::uint8_t> portable_hdr_bytes;

    [[nodiscard]] static core::Result<std::shared_ptr<const HdriEnvironmentAsset>> buildFromImage(
        const HdriImage& image,
        std::string asset_name = "Environment.hdr");

    [[nodiscard]] static core::Result<std::shared_ptr<const HdriEnvironmentAsset>> loadFromMemory(
        std::span<const std::uint8_t> bytes,
        std::string asset_name = "Environment.hdr");

    [[nodiscard]] static core::Result<std::shared_ptr<const HdriEnvironmentAsset>> loadFromFile(
        const std::filesystem::path& path,
        const platform::IFileSystem& file_system);
};

// Scene-level environment & Image-Based Lighting (IBL) configuration.
struct EnvironmentLighting {
    bool enabled{true};
    float intensity{1.0f};
    float rotation_radians{0.0f};
    float diffuse_ibl_scale{1.0f};
    float specular_ibl_scale{1.0f};
    math::Vec3 fallback_zenith_color{0.243f, 0.270f, 0.324f};
    math::Vec3 fallback_horizon_color{0.117f, 0.130f, 0.156f};

    std::string source_path;
    std::shared_ptr<const HdriEnvironmentAsset> hdri_asset;
    std::uint64_t revision{1};

    [[nodiscard]] bool hasHdri() const noexcept {
        return hdri_asset != nullptr && hdri_asset->prefiltered_cubemap.isValid();
    }

    [[nodiscard]] core::Status validate() const {
        if (!std::isfinite(intensity) || !std::isfinite(rotation_radians) ||
            !std::isfinite(diffuse_ibl_scale) || !std::isfinite(specular_ibl_scale) ||
            !fallback_zenith_color.isFinite() || !fallback_horizon_color.isFinite()) {
            return core::Status::fail(
                core::ErrorCode::ValidationError,
                "EnvironmentLighting parameters must be finite");
        }
        if (intensity < 0.0f || intensity > 1000.0f) {
            return core::Status::fail(
                core::ErrorCode::ValidationError,
                "EnvironmentLighting intensity must be in [0, 1000]");
        }
        if (diffuse_ibl_scale < 0.0f || diffuse_ibl_scale > 16.0f ||
            specular_ibl_scale < 0.0f || specular_ibl_scale > 16.0f) {
            return core::Status::fail(
                core::ErrorCode::ValidationError,
                "EnvironmentLighting IBL scales must be in [0, 16]");
        }
        return core::Status::ok();
    }
};

// IEEE-754 binary32 -> binary16 conversion utility for RGBA16F GPU textures.
[[nodiscard]] std::uint16_t floatToHalfBits(float value) noexcept;

} // namespace finaleffect::scene
