// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/ui/imgui_software_rasterizer.hpp"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace finaleffect::ui {

namespace {

struct UnpackedRgba {
    float r{0.0f};
    float g{0.0f};
    float b{0.0f};
    float a{0.0f};
};

UnpackedRgba unpackImCol(ImU32 col) noexcept {
    constexpr float kInv255 = 1.0f / 255.0f;
    return UnpackedRgba{
        .r = static_cast<float>((col >> IM_COL32_R_SHIFT) & 0xFFu) * kInv255,
        .g = static_cast<float>((col >> IM_COL32_G_SHIFT) & 0xFFu) * kInv255,
        .b = static_cast<float>((col >> IM_COL32_B_SHIFT) & 0xFFu) * kInv255,
        .a = static_cast<float>((col >> IM_COL32_A_SHIFT) & 0xFFu) * kInv255,
    };
}

UnpackedRgba sampleAtlasBilinear(const FontAtlasRgbaView& atlas, float u, float v) noexcept {
    if (atlas.pixels == nullptr || atlas.width <= 0 || atlas.height <= 0) {
        return UnpackedRgba{1.0f, 1.0f, 1.0f, 1.0f};
    }
    const float fx = std::clamp(u, 0.0f, 1.0f) * static_cast<float>(atlas.width - 1);
    const float fy = std::clamp(v, 0.0f, 1.0f) * static_cast<float>(atlas.height - 1);
    const int x0 = static_cast<int>(fx);
    const int y0 = static_cast<int>(fy);
    const int x1 = std::min(x0 + 1, atlas.width - 1);
    const int y1 = std::min(y0 + 1, atlas.height - 1);
    const float tx = fx - static_cast<float>(x0);
    const float ty = fy - static_cast<float>(y0);

    auto fetch = [&](int px, int py) noexcept -> UnpackedRgba {
        const std::size_t off =
            (static_cast<std::size_t>(py) * static_cast<std::size_t>(atlas.width) +
             static_cast<std::size_t>(px)) *
            4u;
        constexpr float kInv255 = 1.0f / 255.0f;
        return UnpackedRgba{
            .r = static_cast<float>(atlas.pixels[off + 0u]) * kInv255,
            .g = static_cast<float>(atlas.pixels[off + 1u]) * kInv255,
            .b = static_cast<float>(atlas.pixels[off + 2u]) * kInv255,
            .a = static_cast<float>(atlas.pixels[off + 3u]) * kInv255,
        };
    };

    const UnpackedRgba c00 = fetch(x0, y0);
    const UnpackedRgba c10 = fetch(x1, y0);
    const UnpackedRgba c01 = fetch(x0, y1);
    const UnpackedRgba c11 = fetch(x1, y1);

    const float w00 = (1.0f - tx) * (1.0f - ty);
    const float w10 = tx * (1.0f - ty);
    const float w01 = (1.0f - tx) * ty;
    const float w11 = tx * ty;
    return UnpackedRgba{
        .r = c00.r * w00 + c10.r * w10 + c01.r * w01 + c11.r * w11,
        .g = c00.g * w00 + c10.g * w10 + c01.g * w01 + c11.g * w11,
        .b = c00.b * w00 + c10.b * w10 + c01.b * w01 + c11.b * w11,
        .a = c00.a * w00 + c10.a * w10 + c01.a * w01 + c11.a * w11,
    };
}

} // namespace

void rasterizeImDrawDataToRgba8(
    const ImDrawData* draw_data,
    const FontAtlasRgbaView& font_atlas,
    std::uint32_t fb_width,
    std::uint32_t fb_height,
    std::vector<std::uint8_t>& inout_rgba_top_down) {
    if (draw_data == nullptr || fb_width == 0 || fb_height == 0) {
        return;
    }
    const std::size_t needed = static_cast<std::size_t>(fb_width) * static_cast<std::size_t>(fb_height) * 4u;
    if (inout_rgba_top_down.size() < needed) {
        inout_rgba_top_down.assign(needed, 0u);
    }

    const ImVec2 clip_off = draw_data->DisplayPos;
    const ImVec2 clip_scale = draw_data->FramebufferScale;

    for (int n = 0; n < draw_data->CmdListsCount; ++n) {
        const ImDrawList* cmd_list = draw_data->CmdLists[n];
        if (cmd_list == nullptr) continue;
        const ImDrawVert* vtx_buf = cmd_list->VtxBuffer.Data;
        const ImDrawIdx* idx_buf = cmd_list->IdxBuffer.Data;

        for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; ++cmd_i) {
            const ImDrawCmd& pcmd = cmd_list->CmdBuffer[cmd_i];
            if (pcmd.UserCallback != nullptr) {
                continue;
            }

            const float clip_min_x = (pcmd.ClipRect.x - clip_off.x) * clip_scale.x;
            const float clip_min_y = (pcmd.ClipRect.y - clip_off.y) * clip_scale.y;
            const float clip_max_x = (pcmd.ClipRect.z - clip_off.x) * clip_scale.x;
            const float clip_max_y = (pcmd.ClipRect.w - clip_off.y) * clip_scale.y;
            if (clip_max_x <= clip_min_x || clip_max_y <= clip_min_y) {
                continue;
            }

            const int sc_x0 = std::max(0, static_cast<int>(std::floor(clip_min_x)));
            const int sc_y0 = std::max(0, static_cast<int>(std::floor(clip_min_y)));
            const int sc_x1 = std::min(static_cast<int>(fb_width), static_cast<int>(std::ceil(clip_max_x)));
            const int sc_y1 = std::min(static_cast<int>(fb_height), static_cast<int>(std::ceil(clip_max_y)));
            if (sc_x1 <= sc_x0 || sc_y1 <= sc_y0) {
                continue;
            }

            for (unsigned int elem = 0; elem + 2u < pcmd.ElemCount; elem += 3u) {
                const ImDrawVert& v0 = vtx_buf[idx_buf[pcmd.IdxOffset + elem + 0u] + pcmd.VtxOffset];
                const ImDrawVert& v1 = vtx_buf[idx_buf[pcmd.IdxOffset + elem + 1u] + pcmd.VtxOffset];
                const ImDrawVert& v2 = vtx_buf[idx_buf[pcmd.IdxOffset + elem + 2u] + pcmd.VtxOffset];

                const float p0x = (v0.pos.x - clip_off.x) * clip_scale.x;
                const float p0y = (v0.pos.y - clip_off.y) * clip_scale.y;
                const float p1x = (v1.pos.x - clip_off.x) * clip_scale.x;
                const float p1y = (v1.pos.y - clip_off.y) * clip_scale.y;
                const float p2x = (v2.pos.x - clip_off.x) * clip_scale.x;
                const float p2y = (v2.pos.y - clip_off.y) * clip_scale.y;

                const float area = (p1x - p0x) * (p2y - p0y) - (p1y - p0y) * (p2x - p0x);
                if (std::fabs(area) < 1.0e-5f) {
                    continue;
                }
                const float inv_area = 1.0f / area;

                const int min_x = std::max(sc_x0, static_cast<int>(std::floor(std::min({p0x, p1x, p2x}))));
                const int min_y = std::max(sc_y0, static_cast<int>(std::floor(std::min({p0y, p1y, p2y}))));
                const int max_x = std::min(sc_x1, static_cast<int>(std::ceil(std::max({p0x, p1x, p2x}))));
                const int max_y = std::min(sc_y1, static_cast<int>(std::ceil(std::max({p0y, p1y, p2y}))));
                if (max_x <= min_x || max_y <= min_y) {
                    continue;
                }

                const UnpackedRgba c0 = unpackImCol(v0.col);
                const UnpackedRgba c1 = unpackImCol(v1.col);
                const UnpackedRgba c2 = unpackImCol(v2.col);

                const bool solid_uv =
                    std::fabs(v0.uv.x - v1.uv.x) < 1.0e-5f &&
                    std::fabs(v0.uv.y - v1.uv.y) < 1.0e-5f &&
                    std::fabs(v0.uv.x - v2.uv.x) < 1.0e-5f &&
                    std::fabs(v0.uv.y - v2.uv.y) < 1.0e-5f;
                const UnpackedRgba const_tex =
                    solid_uv ? sampleAtlasBilinear(font_atlas, v0.uv.x, v0.uv.y) : UnpackedRgba{};

                for (int py = min_y; py < max_y; ++py) {
                    const float sy = static_cast<float>(py) + 0.5f;
                    for (int px = min_x; px < max_x; ++px) {
                        const float sx = static_cast<float>(px) + 0.5f;
                        const float w0 = ((p1x - sx) * (p2y - sy) - (p1y - sy) * (p2x - sx)) * inv_area;
                        const float w1 = ((p2x - sx) * (p0y - sy) - (p2y - sy) * (p0x - sx)) * inv_area;
                        const float w2 = 1.0f - w0 - w1;
                        if (w0 < -1.0e-4f || w1 < -1.0e-4f || w2 < -1.0e-4f) {
                            continue;
                        }

                        const UnpackedRgba tex = solid_uv
                            ? const_tex
                            : sampleAtlasBilinear(
                                  font_atlas,
                                  w0 * v0.uv.x + w1 * v1.uv.x + w2 * v2.uv.x,
                                  w0 * v0.uv.y + w1 * v1.uv.y + w2 * v2.uv.y);

                        const float src_a = std::clamp((w0 * c0.a + w1 * c1.a + w2 * c2.a) * tex.a, 0.0f, 1.0f);
                        if (src_a <= (1.0f / 512.0f)) {
                            continue;
                        }
                        const float src_r = std::clamp((w0 * c0.r + w1 * c1.r + w2 * c2.r) * tex.r, 0.0f, 1.0f);
                        const float src_g = std::clamp((w0 * c0.g + w1 * c1.g + w2 * c2.g) * tex.g, 0.0f, 1.0f);
                        const float src_b = std::clamp((w0 * c0.b + w1 * c1.b + w2 * c2.b) * tex.b, 0.0f, 1.0f);

                        const std::size_t off =
                            (static_cast<std::size_t>(py) * static_cast<std::size_t>(fb_width) +
                             static_cast<std::size_t>(px)) *
                            4u;
                        constexpr float kInv255 = 1.0f / 255.0f;
                        const float dst_r = static_cast<float>(inout_rgba_top_down[off + 0u]) * kInv255;
                        const float dst_g = static_cast<float>(inout_rgba_top_down[off + 1u]) * kInv255;
                        const float dst_b = static_cast<float>(inout_rgba_top_down[off + 2u]) * kInv255;
                        const float inv_a = 1.0f - src_a;

                        inout_rgba_top_down[off + 0u] =
                            static_cast<std::uint8_t>(std::clamp((src_r * src_a + dst_r * inv_a) * 255.0f + 0.5f, 0.0f, 255.0f));
                        inout_rgba_top_down[off + 1u] =
                            static_cast<std::uint8_t>(std::clamp((src_g * src_a + dst_g * inv_a) * 255.0f + 0.5f, 0.0f, 255.0f));
                        inout_rgba_top_down[off + 2u] =
                            static_cast<std::uint8_t>(std::clamp((src_b * src_a + dst_b * inv_a) * 255.0f + 0.5f, 0.0f, 255.0f));
                        inout_rgba_top_down[off + 3u] = 255u;
                    }
                }
            }
        }
    }
}

} // namespace finaleffect::ui
