// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include <cstdint>
#include <span>
#include <vector>

struct ImDrawData;

namespace finaleffect::ui {

struct FontAtlasRgbaView {
    const std::uint8_t* pixels{nullptr};
    int width{0};
    int height{0};
};

// CPU reference rasterizer for Dear ImGui draw lists.
// Used strictly in headless automated tests (--headless / SoftwareRenderDevice) so native UI
// layout, typography, colors, and clipping are verified across CI matrices without an X server.
// Interactive desktop execution uses OpenGL 3.3 Core (ImGui_ImplOpenGL3).
void rasterizeImDrawDataToRgba8(
    const ImDrawData* draw_data,
    const FontAtlasRgbaView& font_atlas,
    std::uint32_t fb_width,
    std::uint32_t fb_height,
    std::vector<std::uint8_t>& inout_rgba_top_down);

} // namespace finaleffect::ui
