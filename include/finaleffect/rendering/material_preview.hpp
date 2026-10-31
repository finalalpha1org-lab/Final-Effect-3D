// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include "finaleffect/core/result.hpp"
#include "finaleffect/rendering/renderer.hpp"
#include "finaleffect/rendering/software_render_device.hpp"
#include "finaleffect/scene/material.hpp"
#include "finaleffect/scene/scene.hpp"

#include <cstdint>
#include <memory>
#include <vector>

namespace finaleffect::rendering {

// Real 3D UV-sphere studio material preview renderer (no static images).
// Renders an actual 3D sphere + checker backdrop through Scene + Renderer + Cook-Torrance GGX shading.
class MaterialPreviewRenderer {
public:
    MaterialPreviewRenderer();

    [[nodiscard]] core::Status initialize(std::uint32_t width = 96, std::uint32_t height = 96);

    // Renders the given PbrMaterial onto the 3D preview sphere and increments renderGeneration().
    [[nodiscard]] core::Status renderMaterial(const scene::PbrMaterial& material);

    [[nodiscard]] bool isInitialized() const noexcept { return initialized_; }
    [[nodiscard]] std::uint32_t width() const noexcept;
    [[nodiscard]] std::uint32_t height() const noexcept;
    [[nodiscard]] const std::vector<Rgba8>& rgbaPixels() const noexcept;
    [[nodiscard]] const std::uint8_t* rawRgbaBytes() const noexcept;
    [[nodiscard]] std::size_t rawByteSize() const noexcept;
    [[nodiscard]] Rgba8 pixelAt(std::uint32_t x, std::uint32_t y) const noexcept;
    [[nodiscard]] std::uint64_t renderGeneration() const noexcept { return generation_; }

private:
    scene::Scene preview_scene_{"MaterialPreviewStudio"};
    scene::ObjectId sphere_obj_id_{scene::ObjectId::invalid()};
    scene::MaterialId preview_mat_id_{scene::MaterialId::invalid()};
    SoftwareRenderDevice* software_device_{nullptr};
    std::unique_ptr<Renderer> renderer_;
    bool initialized_{false};
    std::uint64_t generation_{0};
};

} // namespace finaleffect::rendering
