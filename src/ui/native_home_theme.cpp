// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/ui/native_home_theme.hpp"
#include "finaleffect_embedded_fonts.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>

namespace finaleffect::ui {

namespace {

std::string_view visibleButtonText(const char* label_and_id) noexcept {
    if (label_and_id == nullptr) {
        return {};
    }
    const std::string_view sv(label_and_id);
    const std::size_t hash_pos = sv.find("##");
    return (hash_pos == std::string_view::npos) ? sv : sv.substr(0u, hash_pos);
}

} // namespace

bool NativeHomeTheme::syncScaleAndStyle(std::uint32_t ui_scale_percent, float dpi_content_scale) {
    const std::uint32_t clamped_pct = std::clamp<std::uint32_t>(ui_scale_percent, 85u, 150u);
    const float safe_dpi = std::clamp(dpi_content_scale, 0.75f, 3.0f);
    const float target_scale = (static_cast<float>(clamped_pct) / 100.0f) * safe_dpi;

    ImGuiIO& io = ImGui::GetIO();
    const bool fonts_missing = (font_body_ == nullptr) || (io.Fonts == nullptr) || (io.Fonts->Fonts.Size == 0);
    const bool scale_changed =
        (clamped_pct != last_ui_scale_pct_) || (std::fabs(safe_dpi - last_dpi_scale_) > 0.01f);

    if (!fonts_missing && !scale_changed) {
        return false;
    }

    last_ui_scale_pct_ = clamped_pct;
    last_dpi_scale_ = safe_dpi;
    effective_scale_ = target_scale;

    rebuildFonts(target_scale);
    applyImGuiStyle(target_scale);
    return true;
}

void NativeHomeTheme::rebuildFonts(float scale) {
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();
    glyph_ranges_.clear();

    ImFontGlyphRangesBuilder builder;
    builder.AddRanges(io.Fonts->GetGlyphRangesDefault());
    builder.AddText("•–—…→←↑↓×✓★");
    builder.BuildRanges(&glyph_ranges_);

    auto addEmbeddedFont = [&](const unsigned char* data, std::size_t size, float base_px) -> ImFont* {
        ImFontConfig cfg{};
        cfg.FontDataOwnedByAtlas = false;
        cfg.OversampleH = 3;
        cfg.OversampleV = 2;
        cfg.PixelSnapH = true;
        const float px_size = std::round( std::max(11.0f, base_px * scale) );
        return io.Fonts->AddFontFromMemoryTTF(
            const_cast<unsigned char*>(data),
            static_cast<int>(size),
            px_size,
            &cfg,
            glyph_ranges_.Data);
    };

    font_body_ = addEmbeddedFont(
        embedded_fonts::kKarlaRegularTtf,
        embedded_fonts::kKarlaRegularTtfSize,
        15.0f);
    font_bold_ = addEmbeddedFont(
        embedded_fonts::kRobotoMediumTtf,
        embedded_fonts::kRobotoMediumTtfSize,
        14.5f);
    font_subheading_ = addEmbeddedFont(
        embedded_fonts::kRobotoMediumTtf,
        embedded_fonts::kRobotoMediumTtfSize,
        16.5f);
    font_heading_ = addEmbeddedFont(
        embedded_fonts::kRobotoMediumTtf,
        embedded_fonts::kRobotoMediumTtfSize,
        23.0f);
    font_mono_ = addEmbeddedFont(
        embedded_fonts::kCousineRegularTtf,
        embedded_fonts::kCousineRegularTtfSize,
        13.0f);

    if (font_body_ == nullptr) {
        font_body_ = io.Fonts->AddFontDefault();
    }
    if (font_bold_ == nullptr) font_bold_ = font_body_;
    if (font_subheading_ == nullptr) font_subheading_ = font_bold_;
    if (font_heading_ == nullptr) font_heading_ = font_bold_;
    if (font_mono_ == nullptr) font_mono_ = font_body_;

    io.FontDefault = font_body_;
    unsigned char* tex_pixels = nullptr;
    int tex_w = 0;
    int tex_h = 0;
    io.Fonts->GetTexDataAsRGBA32(&tex_pixels, &tex_w, &tex_h);
}

void NativeHomeTheme::applyImGuiStyle(float scale) const {
    ImGuiStyle& style = ImGui::GetStyle();
    style = ImGuiStyle{};

    style.WindowPadding = ImVec2(std::round(16.0f * scale), std::round(14.0f * scale));
    style.FramePadding = ImVec2(std::round(10.0f * scale), std::round(7.0f * scale));
    style.CellPadding = ImVec2(std::round(8.0f * scale), std::round(6.0f * scale));
    style.ItemSpacing = ImVec2(std::round(10.0f * scale), std::round(9.0f * scale));
    style.ItemInnerSpacing = ImVec2(std::round(7.0f * scale), std::round(5.0f * scale));
    style.ScrollbarSize = std::round(11.0f * scale);
    style.GrabMinSize = std::round(10.0f * scale);

    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;

    style.WindowRounding = std::round(8.0f * scale);
    style.ChildRounding = std::round(8.0f * scale);
    style.FrameRounding = std::round(6.0f * scale);
    style.PopupRounding = std::round(8.0f * scale);
    style.ScrollbarRounding = std::round(6.0f * scale);
    style.GrabRounding = std::round(5.0f * scale);
    style.TabRounding = std::round(6.0f * scale);
    style.AntiAliasedLines = true;
    style.AntiAliasedFill = true;

    ImVec4* c = style.Colors;
    c[ImGuiCol_Text] = ThemeColors::vec4(0xEE, 0xF2, 0xF7);
    c[ImGuiCol_TextDisabled] = ThemeColors::vec4(0x6E, 0x78, 0x8C);
    c[ImGuiCol_WindowBg] = ThemeColors::vec4(0x0F, 0x11, 0x15);
    c[ImGuiCol_ChildBg] = ThemeColors::vec4(0x19, 0x1D, 0x24);
    c[ImGuiCol_PopupBg] = ThemeColors::vec4(0x1B, 0x20, 0x29);
    c[ImGuiCol_Border] = ThemeColors::vec4(0x2C, 0x33, 0x40);
    c[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

    c[ImGuiCol_FrameBg] = ThemeColors::vec4(0x14, 0x17, 0x1D);
    c[ImGuiCol_FrameBgHovered] = ThemeColors::vec4(0x1F, 0x24, 0x2E);
    c[ImGuiCol_FrameBgActive] = ThemeColors::vec4(0x18, 0x22, 0x2C);

    c[ImGuiCol_TitleBg] = ThemeColors::vec4(0x16, 0x19, 0x20);
    c[ImGuiCol_TitleBgActive] = ThemeColors::vec4(0x19, 0x1E, 0x27);
    c[ImGuiCol_TitleBgCollapsed] = ThemeColors::vec4(0x16, 0x19, 0x20);
    c[ImGuiCol_MenuBarBg] = ThemeColors::vec4(0x16, 0x19, 0x20);

    c[ImGuiCol_ScrollbarBg] = ThemeColors::vec4(0x12, 0x15, 0x1B);
    c[ImGuiCol_ScrollbarGrab] = ThemeColors::vec4(0x2D, 0x35, 0x44);
    c[ImGuiCol_ScrollbarGrabHovered] = ThemeColors::vec4(0x3B, 0x45, 0x58);
    c[ImGuiCol_ScrollbarGrabActive] = ThemeColors::vec4(0x27, 0xB6, 0xD9);

    c[ImGuiCol_CheckMark] = ThemeColors::vec4(0x27, 0xB6, 0xD9);
    c[ImGuiCol_SliderGrab] = ThemeColors::vec4(0x27, 0xB6, 0xD9);
    c[ImGuiCol_SliderGrabActive] = ThemeColors::vec4(0x3B, 0xC4, 0xE5);

    c[ImGuiCol_Button] = ThemeColors::vec4(0x1F, 0x24, 0x2D);
    c[ImGuiCol_ButtonHovered] = ThemeColors::vec4(0x27, 0x2D, 0x38);
    c[ImGuiCol_ButtonActive] = ThemeColors::vec4(0x1B, 0x2D, 0x36);

    c[ImGuiCol_Header] = ThemeColors::vec4(0x1B, 0x2D, 0x36);
    c[ImGuiCol_HeaderHovered] = ThemeColors::vec4(0x24, 0x38, 0x44);
    c[ImGuiCol_HeaderActive] = ThemeColors::vec4(0x27, 0xB6, 0xD9, 0x55);

    c[ImGuiCol_Separator] = ThemeColors::vec4(0x24, 0x29, 0x34);
    c[ImGuiCol_SeparatorHovered] = ThemeColors::vec4(0x27, 0xB6, 0xD9);
    c[ImGuiCol_SeparatorActive] = ThemeColors::vec4(0x27, 0xB6, 0xD9);

    c[ImGuiCol_TextSelectedBg] = ThemeColors::vec4(0x27, 0xB6, 0xD9, 0x48);
    c[ImGuiCol_NavHighlight] = ThemeColors::vec4(0x27, 0xB6, 0xD9);
    c[ImGuiCol_ModalWindowDimBg] = ThemeColors::vec4(0x07, 0x09, 0x0D, 0xBA);
}

void NativeHomeTheme::pushBodyFont() const {
    ImGui::PushFont(font_body_ != nullptr ? font_body_ : ImGui::GetFont());
}
void NativeHomeTheme::pushBoldFont() const {
    ImGui::PushFont(font_bold_ != nullptr ? font_bold_ : ImGui::GetFont());
}
void NativeHomeTheme::pushHeadingFont() const {
    ImGui::PushFont(font_heading_ != nullptr ? font_heading_ : ImGui::GetFont());
}
void NativeHomeTheme::pushSubheadingFont() const {
    ImGui::PushFont(font_subheading_ != nullptr ? font_subheading_ : ImGui::GetFont());
}
void NativeHomeTheme::pushMonoFont() const {
    ImGui::PushFont(font_mono_ != nullptr ? font_mono_ : ImGui::GetFont());
}
void NativeHomeTheme::popFont() const {
    ImGui::PopFont();
}

void NativeHomeTheme::drawBrandLogo(ImDrawList* dl, ImVec2 min_pos, float size_px) const {
    if (dl == nullptr) return;
    const ImVec2 max_pos(min_pos.x + size_px, min_pos.y + size_px);
    const float r = std::round(6.0f * effective_scale_);
    dl->AddRectFilled(min_pos, max_pos, ThemeColors::kBgElevated, r);
    dl->AddRect(min_pos, max_pos, ThemeColors::kBorderStrong, r, 0, 1.2f);

    const float cx = min_pos.x + size_px * 0.5f;
    const float cy = min_pos.y + size_px * 0.5f;
    const float rad = size_px * 0.34f;

    // Isometric Prism Cyan hexagon + Solar Amber core facet
    const ImVec2 top(cx, cy - rad);
    const ImVec2 tr(cx + rad * 0.866f, cy - rad * 0.48f);
    const ImVec2 br(cx + rad * 0.866f, cy + rad * 0.48f);
    const ImVec2 bot(cx, cy + rad);
    const ImVec2 bl(cx - rad * 0.866f, cy + rad * 0.48f);
    const ImVec2 tl(cx - rad * 0.866f, cy - rad * 0.48f);
    const ImVec2 mid(cx, cy + rad * 0.04f);

    const ImVec2 top_face[4] = {top, tr, mid, tl};
    dl->AddConvexPolyFilled(top_face, 4, ThemeColors::pack(0x27, 0xB6, 0xD9, 0x50));
    const ImVec2 right_face[4] = {mid, tr, br, bot};
    dl->AddConvexPolyFilled(right_face, 4, ThemeColors::pack(0xF2, 0x9D, 0x38, 0x52));

    const ImVec2 hex[6] = {top, tr, br, bot, bl, tl};
    dl->AddPolyline(hex, 6, ThemeColors::kAccentCyan, ImDrawFlags_Closed, 1.6f * effective_scale_);
    dl->AddLine(tl, mid, ThemeColors::kAccentCyan, 1.3f * effective_scale_);
    dl->AddLine(tr, mid, ThemeColors::kAccentAmber, 1.3f * effective_scale_);
    dl->AddLine(bot, mid, ThemeColors::kAccentCyan, 1.3f * effective_scale_);
}

void NativeHomeTheme::drawIcon(
    ImDrawList* dl,
    HubIcon icon,
    ImVec2 c,
    float r,
    ImU32 col) const {
    if (dl == nullptr || icon == HubIcon::None) return;
    const float t = std::max(1.25f, 1.45f * effective_scale_);

    switch (icon) {
        case HubIcon::Projects: {
            const float g = r * 0.22f;
            dl->AddRect(ImVec2(c.x - r, c.y - r), ImVec2(c.x - g, c.y - g), col, 1.5f, 0, t);
            dl->AddRect(ImVec2(c.x + g, c.y - r), ImVec2(c.x + r, c.y - g), col, 1.5f, 0, t);
            dl->AddRect(ImVec2(c.x - r, c.y + g), ImVec2(c.x - g, c.y + r), col, 1.5f, 0, t);
            dl->AddRect(ImVec2(c.x + g, c.y + g), ImVec2(c.x + r, c.y + r), col, 1.5f, 0, t);
            break;
        }
        case HubIcon::Templates: {
            const ImVec2 top(c.x, c.y - r);
            const ImVec2 rr(c.x + r * 0.9f, c.y - r * 0.42f);
            const ImVec2 br(c.x + r * 0.9f, c.y + r * 0.48f);
            const ImVec2 bot(c.x, c.y + r);
            const ImVec2 bl(c.x - r * 0.9f, c.y + r * 0.48f);
            const ImVec2 ll(c.x - r * 0.9f, c.y - r * 0.42f);
            const ImVec2 poly[6] = {top, rr, br, bot, bl, ll};
            dl->AddPolyline(poly, 6, col, ImDrawFlags_Closed, t);
            dl->AddLine(ll, c, col, t);
            dl->AddLine(rr, c, col, t);
            dl->AddLine(bot, c, col, t);
            break;
        }
        case HubIcon::Plus: {
            dl->AddLine(ImVec2(c.x - r * 0.85f, c.y), ImVec2(c.x + r * 0.85f, c.y), col, t * 1.15f);
            dl->AddLine(ImVec2(c.x, c.y - r * 0.85f), ImVec2(c.x, c.y + r * 0.85f), col, t * 1.15f);
            break;
        }
        case HubIcon::Folder: {
            const ImVec2 p0(c.x - r, c.y - r * 0.72f);
            const ImVec2 p1(c.x - r * 0.18f, c.y - r * 0.72f);
            const ImVec2 p2(c.x + r * 0.08f, c.y - r * 0.40f);
            const ImVec2 p3(c.x + r, c.y - r * 0.40f);
            const ImVec2 p4(c.x + r, c.y + r * 0.78f);
            const ImVec2 p5(c.x - r, c.y + r * 0.78f);
            const ImVec2 pts[6] = {p0, p1, p2, p3, p4, p5};
            dl->AddPolyline(pts, 6, col, ImDrawFlags_Closed, t);
            break;
        }
        case HubIcon::Recovery: {
            dl->PathArcTo(c, r * 0.88f, 0.55f, 5.45f, 18);
            dl->PathStroke(col, 0, t);
            dl->AddLine(c, ImVec2(c.x, c.y - r * 0.48f), col, t);
            dl->AddLine(c, ImVec2(c.x + r * 0.40f, c.y + r * 0.22f), col, t);
            break;
        }
        case HubIcon::Settings: {
            dl->AddLine(ImVec2(c.x - r, c.y - r * 0.55f), ImVec2(c.x + r, c.y - r * 0.55f), col, t);
            dl->AddCircleFilled(ImVec2(c.x - r * 0.30f, c.y - r * 0.55f), r * 0.24f, col);
            dl->AddLine(ImVec2(c.x - r, c.y + r * 0.05f), ImVec2(c.x + r, c.y + r * 0.05f), col, t);
            dl->AddCircleFilled(ImVec2(c.x + r * 0.35f, c.y + r * 0.05f), r * 0.24f, col);
            dl->AddLine(ImVec2(c.x - r, c.y + r * 0.65f), ImVec2(c.x + r, c.y + r * 0.65f), col, t);
            dl->AddCircleFilled(ImVec2(c.x - r * 0.15f, c.y + r * 0.65f), r * 0.24f, col);
            break;
        }
        case HubIcon::Info: {
            dl->AddCircle(c, r * 0.92f, col, 18, t);
            dl->AddCircleFilled(ImVec2(c.x, c.y - r * 0.42f), t * 0.75f, col);
            dl->AddLine(ImVec2(c.x, c.y - r * 0.08f), ImVec2(c.x, c.y + r * 0.52f), col, t);
            break;
        }
        case HubIcon::Search: {
            const ImVec2 lc(c.x - r * 0.16f, c.y - r * 0.16f);
            dl->AddCircle(lc, r * 0.64f, col, 16, t);
            dl->AddLine(
                ImVec2(lc.x + r * 0.46f, lc.y + r * 0.46f),
                ImVec2(c.x + r * 0.88f, c.y + r * 0.88f),
                col,
                t * 1.15f);
            break;
        }
        case HubIcon::Pin: {
            dl->AddCircle(ImVec2(c.x, c.y - r * 0.32f), r * 0.46f, col, 14, t);
            dl->AddLine(ImVec2(c.x, c.y + r * 0.14f), ImVec2(c.x, c.y + r * 0.92f), col, t);
            break;
        }
        case HubIcon::Grid: {
            const float g = r * 0.20f;
            dl->AddRectFilled(ImVec2(c.x - r, c.y - r), ImVec2(c.x - g, c.y - g), col, 1.0f);
            dl->AddRectFilled(ImVec2(c.x + g, c.y - r), ImVec2(c.x + r, c.y - g), col, 1.0f);
            dl->AddRectFilled(ImVec2(c.x - r, c.y + g), ImVec2(c.x - g, c.y + r), col, 1.0f);
            dl->AddRectFilled(ImVec2(c.x + g, c.y + g), ImVec2(c.x + r, c.y + r), col, 1.0f);
            break;
        }
        case HubIcon::List: {
            for (int i = -1; i <= 1; ++i) {
                const float y = c.y + static_cast<float>(i) * r * 0.62f;
                dl->AddCircleFilled(ImVec2(c.x - r * 0.78f, y), t * 0.72f, col);
                dl->AddLine(ImVec2(c.x - r * 0.36f, y), ImVec2(c.x + r * 0.90f, y), col, t);
            }
            break;
        }
        case HubIcon::ArrowLeft: {
            dl->AddLine(ImVec2(c.x + r * 0.85f, c.y), ImVec2(c.x - r * 0.75f, c.y), col, t);
            dl->AddLine(ImVec2(c.x - r * 0.75f, c.y), ImVec2(c.x - r * 0.15f, c.y - r * 0.60f), col, t);
            dl->AddLine(ImVec2(c.x - r * 0.75f, c.y), ImVec2(c.x - r * 0.15f, c.y + r * 0.60f), col, t);
            break;
        }
        case HubIcon::Check: {
            dl->AddLine(ImVec2(c.x - r * 0.78f, c.y + r * 0.05f), ImVec2(c.x - r * 0.20f, c.y + r * 0.65f), col, t * 1.2f);
            dl->AddLine(ImVec2(c.x - r * 0.20f, c.y + r * 0.65f), ImVec2(c.x + r * 0.85f, c.y - r * 0.55f), col, t * 1.2f);
            break;
        }
        case HubIcon::Warning: {
            const ImVec2 p0(c.x, c.y - r * 0.88f);
            const ImVec2 p1(c.x + r * 0.92f, c.y + r * 0.80f);
            const ImVec2 p2(c.x - r * 0.92f, c.y + r * 0.80f);
            const ImVec2 tri[3] = {p0, p1, p2};
            dl->AddPolyline(tri, 3, col, ImDrawFlags_Closed, t);
            dl->AddLine(ImVec2(c.x, c.y - r * 0.28f), ImVec2(c.x, c.y + r * 0.28f), col, t);
            dl->AddCircleFilled(ImVec2(c.x, c.y + r * 0.54f), t * 0.7f, col);
            break;
        }
        case HubIcon::Trash: {
            dl->AddLine(ImVec2(c.x - r * 0.85f, c.y - r * 0.52f), ImVec2(c.x + r * 0.85f, c.y - r * 0.52f), col, t);
            dl->AddRect(ImVec2(c.x - r * 0.62f, c.y - r * 0.52f), ImVec2(c.x + r * 0.62f, c.y + r * 0.85f), col, 1.5f, 0, t);
            break;
        }
        case HubIcon::Save: {
            dl->AddRect(ImVec2(c.x - r * 0.82f, c.y - r * 0.82f), ImVec2(c.x + r * 0.82f, c.y + r * 0.82f), col, 1.5f, 0, t);
            dl->AddRect(ImVec2(c.x - r * 0.45f, c.y - r * 0.82f), ImVec2(c.x + r * 0.45f, c.y - r * 0.22f), col, 0.0f, 0, t);
            break;
        }
        case HubIcon::None:
            break;
    }
}

void NativeHomeTheme::drawTemplateDiagram(
    ImDrawList* dl,
    app::ProjectTemplate tmpl,
    ImVec2 rect_min,
    ImVec2 rect_max) const {
    if (dl == nullptr) return;
    const float r = std::round(6.0f * effective_scale_);
    dl->AddRectFilled(rect_min, rect_max, ThemeColors::kBgDiagram, r);
    dl->AddRect(rect_min, rect_max, ThemeColors::kBorderSubtle, r, 0, 1.0f);

    dl->PushClipRect(rect_min, rect_max, true);
    const float w = rect_max.x - rect_min.x;
    const float h = rect_max.y - rect_min.y;
    const float cx = rect_min.x + w * 0.5f;
    const float cy = rect_min.y + h * 0.56f;
    const float s = std::min(w * 0.34f, h * 0.44f);
    const float lw = std::max(1.2f, 1.4f * effective_scale_);

    // Calibrated isometric ground grid on every template diagram
    const ImU32 grid_col = ThemeColors::pack(0x2A, 0x33, 0x43, 0xB8);
    for (int i = -3; i <= 3; ++i) {
        const float u = static_cast<float>(i) / 3.0f;
        const ImVec2 a0(cx + (u - 1.0f) * s * 1.15f, cy + (u + 1.0f) * s * 0.34f);
        const ImVec2 a1(cx + (u + 1.0f) * s * 1.15f, cy + (u - 1.0f) * s * 0.34f);
        dl->AddLine(a0, a1, grid_col, 1.0f);

        const ImVec2 b0(cx + (-1.0f - u) * s * 1.15f, cy + (-1.0f + u) * s * 0.34f);
        const ImVec2 b1(cx + (1.0f - u) * s * 1.15f, cy + (1.0f + u) * s * 0.34f);
        dl->AddLine(b0, b1, grid_col, 1.0f);
    }

    auto drawIsoBox = [&](ImVec2 base_c, float bw, float bh, ImU32 stroke, ImU32 fill) {
        const ImVec2 top(base_c.x, base_c.y - bh - bw * 0.45f);
        const ImVec2 tr(base_c.x + bw * 0.86f, base_c.y - bh);
        const ImVec2 tl(base_c.x - bw * 0.86f, base_c.y - bh);
        const ImVec2 mid(base_c.x, base_c.y - bh + bw * 0.45f);
        const ImVec2 br(base_c.x + bw * 0.86f, base_c.y);
        const ImVec2 bl(base_c.x - bw * 0.86f, base_c.y);
        const ImVec2 bot(base_c.x, base_c.y + bw * 0.45f);

        const ImVec2 top_f[4] = {top, tr, mid, tl};
        dl->AddConvexPolyFilled(top_f, 4, fill);
        const ImVec2 hull[6] = {top, tr, br, bot, bl, tl};
        dl->AddPolyline(hull, 6, stroke, ImDrawFlags_Closed, lw);
        dl->AddLine(tl, mid, stroke, lw);
        dl->AddLine(tr, mid, stroke, lw);
        dl->AddLine(bot, mid, stroke, lw);
    };

    switch (tmpl) {
        case app::ProjectTemplate::StarterShowcase: {
            // HeroCube (Prism Cyan) + AccentPyramid (Solar Amber) + SunLight ray
            drawIsoBox(
                ImVec2(cx - s * 0.36f, cy + s * 0.06f),
                s * 0.46f,
                s * 0.56f,
                ThemeColors::kAccentCyan,
                ThemeColors::pack(0x27, 0xB6, 0xD9, 0x26));

            const ImVec2 pc(cx + s * 0.52f, cy + s * 0.14f);
            const ImVec2 apex(pc.x, pc.y - s * 0.72f);
            const ImVec2 p0(pc.x - s * 0.36f, pc.y);
            const ImVec2 p1(pc.x, pc.y + s * 0.20f);
            const ImVec2 p2(pc.x + s * 0.36f, pc.y - s * 0.02f);
            const ImVec2 tri_fill[3] = {apex, p2, p1};
            dl->AddConvexPolyFilled(tri_fill, 3, ThemeColors::pack(0xF2, 0x9D, 0x38, 0x28));
            dl->AddLine(apex, p0, ThemeColors::kAccentAmber, lw);
            dl->AddLine(apex, p1, ThemeColors::kAccentAmber, lw);
            dl->AddLine(apex, p2, ThemeColors::kAccentAmber, lw);
            dl->AddLine(p0, p1, ThemeColors::kAccentAmber, lw);
            dl->AddLine(p1, p2, ThemeColors::kAccentAmber, lw);

            const ImVec2 sun(rect_max.x - s * 0.65f, rect_min.y + s * 0.42f);
            dl->AddCircle(sun, s * 0.14f, ThemeColors::kAccentAmber, 14, lw);
            dl->AddLine(sun, ImVec2(cx, cy - s * 0.15f), ThemeColors::pack(0xF2, 0x9D, 0x38, 0x70), 1.0f);
            break;
        }
        case app::ProjectTemplate::MinimalCube: {
            drawIsoBox(
                ImVec2(cx, cy + s * 0.10f),
                s * 0.60f,
                s * 0.72f,
                ThemeColors::kAccentCyan,
                ThemeColors::pack(0x27, 0xB6, 0xD9, 0x28));
            break;
        }
        case app::ProjectTemplate::EmptyStage: {
            // Calibrated right-handed coordinate triad (+X red/amber, +Y emerald, +Z cyan)
            const ImVec2 org(cx, cy);
            dl->AddLine(org, ImVec2(cx + s * 0.78f, cy + s * 0.24f), ThemeColors::kAccentAmber, lw * 1.2f);
            dl->AddLine(org, ImVec2(cx, cy - s * 0.85f), ThemeColors::kSemanticEmerald, lw * 1.2f);
            dl->AddLine(org, ImVec2(cx - s * 0.78f, cy + s * 0.24f), ThemeColors::kAccentCyan, lw * 1.2f);
            dl->AddCircleFilled(org, 3.0f * effective_scale_, ThemeColors::kTextPrimary);
            break;
        }
        case app::ProjectTemplate::ShaderBall: {
            // Pedestal + PBR Shader Ball wireframe sphere with specular highlight arc
            drawIsoBox(
                ImVec2(cx, cy + s * 0.30f),
                s * 0.56f,
                s * 0.16f,
                ThemeColors::kBorderStrong,
                ThemeColors::pack(0x27, 0x2D, 0x38, 0x70));
            const ImVec2 sc(cx, cy - s * 0.22f);
            const float sr = s * 0.58f;
            dl->AddCircleFilled(sc, sr, ThemeColors::pack(0x27, 0xB6, 0xD9, 0x22), 28);
            dl->AddCircle(sc, sr, ThemeColors::kAccentCyan, 28, lw);
            ImVec2 lat_pts[24]{};
            ImVec2 lon_pts[24]{};
            for (int k = 0; k < 24; ++k) {
                const float ang = (static_cast<float>(k) / 24.0f) * 6.2831853f;
                const float ca = std::cos(ang);
                const float sa = std::sin(ang);
                lat_pts[k] = ImVec2(sc.x + ca * sr, sc.y + sa * sr * 0.36f);
                lon_pts[k] = ImVec2(sc.x + ca * sr * 0.42f, sc.y + sa * sr);
            }
            dl->AddPolyline(lat_pts, 24, ThemeColors::pack(0x27, 0xB6, 0xD9, 0x95), ImDrawFlags_Closed, 1.1f);
            dl->AddPolyline(lon_pts, 24, ThemeColors::pack(0x27, 0xB6, 0xD9, 0x95), ImDrawFlags_Closed, 1.1f);
            dl->PathArcTo(sc, sr * 0.72f, -2.35f, -1.05f, 14);
            dl->PathStroke(ThemeColors::kAccentAmber, 0, lw * 1.25f);
            break;
        }
    }

    dl->PopClipRect();
}

void NativeHomeTheme::drawTagPill(std::string_view text, TagTone tone) const {
    pushMonoFont();
    const ImVec2 txt_sz = ImGui::CalcTextSize(text.data(), text.data() + text.size());
    const float pad_x = std::round(7.5f * effective_scale_);
    const float pad_y = std::round(3.0f * effective_scale_);
    const ImVec2 box_sz(txt_sz.x + pad_x * 2.0f, txt_sz.y + pad_y * 2.0f);

    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    const ImVec2 p1(p0.x + box_sz.x, p0.y + box_sz.y);
    ImGui::Dummy(box_sz);

    ImU32 bg = ThemeColors::kBgElevated;
    ImU32 border = ThemeColors::kBorderDefault;
    ImU32 fg = ThemeColors::kTextSecondary;
    switch (tone) {
        case TagTone::Cyan:
            bg = ThemeColors::kAccentCyanMuted;
            border = ThemeColors::pack(0x27, 0xB6, 0xD9, 0x70);
            fg = ThemeColors::kAccentCyan;
            break;
        case TagTone::Amber:
            bg = ThemeColors::kAccentAmberMuted;
            border = ThemeColors::pack(0xF2, 0x9D, 0x38, 0x70);
            fg = ThemeColors::kAccentAmber;
            break;
        case TagTone::Emerald:
            bg = ThemeColors::kSemanticEmeraldMuted;
            border = ThemeColors::pack(0x38, 0xC6, 0x95, 0x70);
            fg = ThemeColors::kSemanticEmerald;
            break;
        case TagTone::Danger:
            bg = ThemeColors::kSemanticDangerMuted;
            border = ThemeColors::pack(0xE5, 0x53, 0x53, 0x70);
            fg = ThemeColors::kSemanticDanger;
            break;
        case TagTone::Neutral:
            break;
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float rad = std::round(4.0f * effective_scale_);
    dl->AddRectFilled(p0, p1, bg, rad);
    dl->AddRect(p0, p1, border, rad, 0, 1.0f);
    dl->AddText(ImVec2(p0.x + pad_x, p0.y + pad_y), fg, text.data(), text.data() + text.size());
    popFont();
}

void NativeHomeTheme::drawKeycap(std::string_view chord) const {
    pushMonoFont();
    const ImVec2 txt_sz = ImGui::CalcTextSize(chord.data(), chord.data() + chord.size());
    const float pad_x = std::round(6.0f * effective_scale_);
    const float pad_y = std::round(2.0f * effective_scale_);
    const ImVec2 box_sz(txt_sz.x + pad_x * 2.0f, txt_sz.y + pad_y * 2.0f);
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    const ImVec2 p1(p0.x + box_sz.x, p0.y + box_sz.y);
    ImGui::Dummy(box_sz);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float rad = std::round(4.0f * effective_scale_);
    dl->AddRectFilled(p0, p1, ThemeColors::kBgInput, rad);
    dl->AddRect(p0, p1, ThemeColors::kBorderStrong, rad, 0, 1.0f);
    dl->AddText(ImVec2(p0.x + pad_x, p0.y + pad_y), ThemeColors::kTextMuted, chord.data(), chord.data() + chord.size());
    popFont();
}

void NativeHomeTheme::drawBanner(std::string_view message, TagTone tone) const {
    if (message.empty()) return;
    const float avail_w = ImGui::GetContentRegionAvail().x;
    const float pad_x = std::round(14.0f * effective_scale_);
    const float pad_y = std::round(10.0f * effective_scale_);
    const float icon_space = std::round(24.0f * effective_scale_);
    const float wrap_w = std::max(120.0f, avail_w - pad_x * 2.0f - icon_space);

    const ImVec2 txt_sz = ImGui::CalcTextSize(message.data(), message.data() + message.size(), false, wrap_w);
    const float h = std::max(std::round(38.0f * effective_scale_), txt_sz.y + pad_y * 2.0f);

    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    const ImVec2 p1(p0.x + avail_w, p0.y + h);
    ImGui::Dummy(ImVec2(avail_w, h));

    ImU32 bg = ThemeColors::kBgElevated;
    ImU32 border = ThemeColors::kBorderDefault;
    ImU32 accent = ThemeColors::kAccentCyan;
    HubIcon ic = HubIcon::Info;
    if (tone == TagTone::Danger) {
        bg = ThemeColors::pack(0x2B, 0x15, 0x18);
        border = ThemeColors::pack(0xE5, 0x53, 0x53, 0x95);
        accent = ThemeColors::kSemanticDanger;
        ic = HubIcon::Warning;
    } else if (tone == TagTone::Amber) {
        bg = ThemeColors::pack(0x2A, 0x20, 0x13);
        border = ThemeColors::pack(0xF2, 0x9D, 0x38, 0x95);
        accent = ThemeColors::kAccentAmber;
        ic = HubIcon::Warning;
    } else if (tone == TagTone::Emerald) {
        bg = ThemeColors::pack(0x13, 0x28, 0x22);
        border = ThemeColors::pack(0x38, 0xC6, 0x95, 0x95);
        accent = ThemeColors::kSemanticEmerald;
        ic = HubIcon::Check;
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float rad = std::round(6.0f * effective_scale_);
    dl->AddRectFilled(p0, p1, bg, rad);
    dl->AddRect(p0, p1, border, rad, 0, 1.0f);
    drawIcon(dl, ic, ImVec2(p0.x + pad_x + 7.0f * effective_scale_, p0.y + h * 0.5f), 6.5f * effective_scale_, accent);
    dl->AddText(
        ImGui::GetFont(),
        ImGui::GetFontSize(),
        ImVec2(p0.x + pad_x + icon_space, p0.y + (h - txt_sz.y) * 0.5f),
        ThemeColors::kTextPrimary,
        message.data(),
        message.data() + message.size(),
        wrap_w);
    ImGui::Spacing();
}

bool NativeHomeTheme::styledButton(
    const char* label_and_id,
    ButtonVariant variant,
    ImVec2 explicit_size,
    bool disabled,
    HubIcon leading_icon) const {
    pushBoldFont();
    const std::string_view label = visibleButtonText(label_and_id);
    const ImVec2 txt_sz = ImGui::CalcTextSize(label.data(), label.data() + label.size());
    const float icon_w = (leading_icon != HubIcon::None) ? std::round(18.0f * effective_scale_) : 0.0f;
    const float pad_x = std::round(13.0f * effective_scale_);
    const float pad_y = std::round(7.5f * effective_scale_);

    const ImVec2 sz(
        explicit_size.x > 0.0f ? explicit_size.x : (txt_sz.x + icon_w + pad_x * 2.0f),
        explicit_size.y > 0.0f ? explicit_size.y : std::max(std::round(32.0f * effective_scale_), txt_sz.y + pad_y * 2.0f));

    if (disabled) {
        ImGui::BeginDisabled(true);
    }
    const bool pressed = ImGui::InvisibleButton(label_and_id, sz);
    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();
    const bool focused = ImGui::IsItemFocused();
    if (disabled) {
        ImGui::EndDisabled();
    }

    const ImVec2 p0 = ImGui::GetItemRectMin();
    const ImVec2 p1 = ImGui::GetItemRectMax();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImU32 bg = ThemeColors::kBgElevated;
    ImU32 border = ThemeColors::kBorderDefault;
    ImU32 fg = ThemeColors::kTextPrimary;

    switch (variant) {
        case ButtonVariant::PrimaryCyan:
            bg = active ? ThemeColors::kAccentCyanActive
                        : (hovered ? ThemeColors::kAccentCyanHover : ThemeColors::kAccentCyan);
            border = ThemeColors::kAccentCyanHover;
            fg = ThemeColors::kTextDarkOnAccent;
            break;
        case ButtonVariant::SolarAmber:
            bg = hovered ? ThemeColors::kAccentAmberHover : ThemeColors::kAccentAmber;
            border = ThemeColors::kAccentAmberHover;
            fg = ThemeColors::kTextDarkOnAccent;
            break;
        case ButtonVariant::Danger:
            bg = hovered ? ThemeColors::pack(0x3A, 0x1A, 0x1E) : ThemeColors::pack(0x2B, 0x16, 0x19);
            border = ThemeColors::kSemanticDanger;
            fg = ThemeColors::kSemanticDangerHover;
            break;
        case ButtonVariant::Ghost:
            bg = hovered ? ThemeColors::kBgHover : ThemeColors::pack(0, 0, 0, 0);
            border = hovered ? ThemeColors::kBorderDefault : ThemeColors::pack(0, 0, 0, 0);
            fg = hovered ? ThemeColors::kTextPrimary : ThemeColors::kTextSecondary;
            break;
        case ButtonVariant::Secondary:
            bg = active ? ThemeColors::kBgActive : (hovered ? ThemeColors::kBgHover : ThemeColors::kBgElevated);
            border = hovered ? ThemeColors::kBorderStrong : ThemeColors::kBorderDefault;
            fg = ThemeColors::kTextPrimary;
            break;
    }

    if (disabled) {
        bg = ThemeColors::pack(0x18, 0x1B, 0x22);
        border = ThemeColors::kBorderSubtle;
        fg = ThemeColors::kTextDisabled;
    }

    const float rad = std::round(6.0f * effective_scale_);
    if ((bg >> IM_COL32_A_SHIFT) > 0u) {
        dl->AddRectFilled(p0, p1, bg, rad);
    }
    if ((border >> IM_COL32_A_SHIFT) > 0u) {
        dl->AddRect(p0, p1, border, rad, 0, 1.0f);
    }
    if (focused && !disabled) {
        dl->AddRect(
            ImVec2(p0.x - 1.0f, p0.y - 1.0f),
            ImVec2(p1.x + 1.0f, p1.y + 1.0f),
            ThemeColors::kBorderFocus,
            rad + 1.0f,
            0,
            1.5f);
    }

    const float content_w = icon_w + txt_sz.x;
    float cur_x = p0.x + (sz.x - content_w) * 0.5f;
    const float mid_y = (p0.y + p1.y) * 0.5f;
    if (leading_icon != HubIcon::None) {
        drawIcon(dl, leading_icon, ImVec2(cur_x + 6.0f * effective_scale_, mid_y), 5.5f * effective_scale_, fg);
        cur_x += icon_w;
    }
    dl->AddText(ImVec2(cur_x, mid_y - txt_sz.y * 0.5f), fg, label.data(), label.data() + label.size());

    popFont();
    return !disabled && pressed;
}

void NativeHomeTheme::drawViewHeader(
    std::string_view eyebrow,
    std::string_view title,
    std::string_view subtitle) const {
    pushMonoFont();
    ImGui::PushStyleColor(ImGuiCol_Text, ThemeColors::vec4(0x27, 0xB6, 0xD9));
    ImGui::TextUnformatted(eyebrow.data(), eyebrow.data() + eyebrow.size());
    ImGui::PopStyleColor();
    popFont();

    pushHeadingFont();
    ImGui::TextUnformatted(title.data(), title.data() + title.size());
    popFont();

    if (!subtitle.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ThemeColors::vec4(0xA8, 0xB2, 0xC3));
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextUnformatted(subtitle.data(), subtitle.data() + subtitle.size());
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
    }
}

std::string NativeHomeTheme::formatByteSize(std::uint64_t bytes) {
    char buf[64]{};
    if (bytes < 1024ULL) {
        std::snprintf(buf, sizeof(buf), "%llu B", static_cast<unsigned long long>(bytes));
    } else if (bytes < 1024ULL * 1024ULL) {
        std::snprintf(buf, sizeof(buf), "%.1f KB", static_cast<double>(bytes) / 1024.0);
    } else {
        std::snprintf(buf, sizeof(buf), "%.2f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
    }
    return std::string(buf);
}

std::string NativeHomeTheme::formatTimestampUtc(std::uint64_t epoch_ms) {
    if (epoch_ms == 0ULL) {
        return "Just now";
    }
    const std::time_t secs = static_cast<std::time_t>(epoch_ms / 1000ULL);
    std::tm utc_tm{};
#if defined(_WIN32)
    if (gmtime_s(&utc_tm, &secs) != 0) {
        return "Recorded";
    }
#else
    if (gmtime_r(&secs, &utc_tm) == nullptr) {
        return "Recorded";
    }
#endif
    char buf[48]{};
    if (std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M UTC", &utc_tm) == 0) {
        return "Recorded";
    }
    return std::string(buf);
}

} // namespace finaleffect::ui
