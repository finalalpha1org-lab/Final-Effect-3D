// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include "finaleffect/app/editor_controller.hpp"

#include <imgui.h>

#include <cstdint>
#include <string>
#include <string_view>

namespace finaleffect::ui {

enum class HubIcon : std::uint8_t {
    None = 0,
    Projects,
    Templates,
    Plus,
    Folder,
    Recovery,
    Settings,
    Info,
    Search,
    Pin,
    Grid,
    List,
    ArrowLeft,
    Check,
    Warning,
    Trash,
    Save,
};

enum class TagTone : std::uint8_t {
    Neutral = 0,
    Cyan,
    Amber,
    Emerald,
    Danger,
};

enum class ButtonVariant : std::uint8_t {
    Secondary = 0,
    PrimaryCyan,
    SolarAmber,
    Danger,
    Ghost,
};

namespace detail {
constexpr ImU32 packThemeRgba(
    std::uint8_t r,
    std::uint8_t g,
    std::uint8_t b,
    std::uint8_t a = 255u) noexcept {
    return (static_cast<ImU32>(a) << IM_COL32_A_SHIFT) |
           (static_cast<ImU32>(b) << IM_COL32_B_SHIFT) |
           (static_cast<ImU32>(g) << IM_COL32_G_SHIFT) |
           (static_cast<ImU32>(r) << IM_COL32_R_SHIFT);
}
} // namespace detail

// Stage 0 / Stage 1 Approved Design System for Native Desktop:
// Obsidian Slate surfaces + Prism Cyan (#27B6D9) primary + Solar Amber (#F29D38) secondary
// + Coral Emerald (#38C695) strictly reserved for semantic verified/healthy states.
struct ThemeColors {
    static constexpr ImU32 pack(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255u) noexcept {
        return detail::packThemeRgba(r, g, b, a);
    }

    static constexpr ImVec4 vec4(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255u) noexcept {
        constexpr float kInv = 1.0f / 255.0f;
        return ImVec4{
            static_cast<float>(r) * kInv,
            static_cast<float>(g) * kInv,
            static_cast<float>(b) * kInv,
            static_cast<float>(a) * kInv,
        };
    }

    // Obsidian Slate Surface Hierarchy
    static constexpr ImU32 kBgCanvas = detail::packThemeRgba(0x0F, 0x11, 0x15);
    static constexpr ImU32 kBgShell = detail::packThemeRgba(0x16, 0x19, 0x20);
    static constexpr ImU32 kBgPanel = detail::packThemeRgba(0x19, 0x1D, 0x24);
    static constexpr ImU32 kBgElevated = detail::packThemeRgba(0x1F, 0x24, 0x2D);
    static constexpr ImU32 kBgHover = detail::packThemeRgba(0x27, 0x2D, 0x38);
    static constexpr ImU32 kBgActive = detail::packThemeRgba(0x1B, 0x2D, 0x36);
    static constexpr ImU32 kBgInput = detail::packThemeRgba(0x14, 0x17, 0x1D);
    static constexpr ImU32 kBgDiagram = detail::packThemeRgba(0x11, 0x14, 0x1B);

    // Borders & Hairlines
    static constexpr ImU32 kBorderSubtle = detail::packThemeRgba(0x24, 0x29, 0x34);
    static constexpr ImU32 kBorderDefault = detail::packThemeRgba(0x2C, 0x33, 0x40);
    static constexpr ImU32 kBorderStrong = detail::packThemeRgba(0x3A, 0x43, 0x54);
    static constexpr ImU32 kBorderFocus = detail::packThemeRgba(0x27, 0xB6, 0xD9);

    // Accents & Semantic States
    static constexpr ImU32 kAccentCyan = detail::packThemeRgba(0x27, 0xB6, 0xD9);
    static constexpr ImU32 kAccentCyanHover = detail::packThemeRgba(0x3B, 0xC4, 0xE5);
    static constexpr ImU32 kAccentCyanActive = detail::packThemeRgba(0x1E, 0x9B, 0xBB);
    static constexpr ImU32 kAccentCyanMuted = detail::packThemeRgba(0x27, 0xB6, 0xD9, 0x28);

    static constexpr ImU32 kAccentAmber = detail::packThemeRgba(0xF2, 0x9D, 0x38);
    static constexpr ImU32 kAccentAmberHover = detail::packThemeRgba(0xF6, 0xAE, 0x55);
    static constexpr ImU32 kAccentAmberMuted = detail::packThemeRgba(0xF2, 0x9D, 0x38, 0x28);

    static constexpr ImU32 kSemanticEmerald = detail::packThemeRgba(0x38, 0xC6, 0x95);
    static constexpr ImU32 kSemanticEmeraldMuted = detail::packThemeRgba(0x38, 0xC6, 0x95, 0x26);

    static constexpr ImU32 kSemanticDanger = detail::packThemeRgba(0xE5, 0x53, 0x53);
    static constexpr ImU32 kSemanticDangerHover = detail::packThemeRgba(0xEE, 0x68, 0x68);
    static constexpr ImU32 kSemanticDangerMuted = detail::packThemeRgba(0xE5, 0x53, 0x53, 0x28);

    // Typography
    static constexpr ImU32 kTextPrimary = detail::packThemeRgba(0xEE, 0xF2, 0xF7);
    static constexpr ImU32 kTextDarkOnAccent = detail::packThemeRgba(0x0B, 0x10, 0x15);
    static constexpr ImU32 kTextSecondary = detail::packThemeRgba(0xA8, 0xB2, 0xC3);
    static constexpr ImU32 kTextMuted = detail::packThemeRgba(0x88, 0x93, 0xA7);
    static constexpr ImU32 kTextDisabled = detail::packThemeRgba(0x5B, 0x64, 0x75);
};

class NativeHomeTheme {
public:
    NativeHomeTheme() = default;

    // Applies Obsidian Slate style metrics & rebuilds anti-aliased fonts only when scale changes.
    // Returns true if the font atlas texture was rebuilt and needs GPU re-upload.
    bool syncScaleAndStyle(std::uint32_t ui_scale_percent, float dpi_content_scale);

    [[nodiscard]] float effectiveScale() const noexcept { return effective_scale_; }
    [[nodiscard]] float px(float base_px) const noexcept { return base_px * effective_scale_; }

    [[nodiscard]] ImFont* fontBody() const noexcept { return font_body_; }
    [[nodiscard]] ImFont* fontBold() const noexcept { return font_bold_; }
    [[nodiscard]] ImFont* fontHeading() const noexcept { return font_heading_; }
    [[nodiscard]] ImFont* fontSubheading() const noexcept { return font_subheading_; }
    [[nodiscard]] ImFont* fontMono() const noexcept { return font_mono_; }

    void pushBodyFont() const;
    void pushBoldFont() const;
    void pushHeadingFont() const;
    void pushSubheadingFont() const;
    void pushMonoFont() const;
    void popFont() const;

    // Vector Graphics & Styled Controls
    void drawBrandLogo(ImDrawList* dl, ImVec2 min_pos, float size_px) const;
    void drawIcon(ImDrawList* dl, HubIcon icon, ImVec2 center, float radius_px, ImU32 color) const;
    void drawTemplateDiagram(
        ImDrawList* dl,
        app::ProjectTemplate tmpl,
        ImVec2 rect_min,
        ImVec2 rect_max) const;

    void drawTagPill(std::string_view text, TagTone tone = TagTone::Neutral) const;
    void drawKeycap(std::string_view chord) const;
    void drawBanner(std::string_view message, TagTone tone) const;

    [[nodiscard]] bool styledButton(
        const char* label_and_id,
        ButtonVariant variant = ButtonVariant::Secondary,
        ImVec2 explicit_size = ImVec2(0.0f, 0.0f),
        bool disabled = false,
        HubIcon leading_icon = HubIcon::None) const;

    void drawViewHeader(
        std::string_view eyebrow,
        std::string_view title,
        std::string_view subtitle) const;

    [[nodiscard]] static std::string formatByteSize(std::uint64_t bytes);
    [[nodiscard]] static std::string formatTimestampUtc(std::uint64_t epoch_ms);

private:
    void rebuildFonts(float scale);
    void applyImGuiStyle(float scale) const;

    float effective_scale_{0.0f};
    std::uint32_t last_ui_scale_pct_{0};
    float last_dpi_scale_{0.0f};

    ImFont* font_body_{nullptr};
    ImFont* font_bold_{nullptr};
    ImFont* font_heading_{nullptr};
    ImFont* font_subheading_{nullptr};
    ImFont* font_mono_{nullptr};
    ImVector<ImWchar> glyph_ranges_{};
};

} // namespace finaleffect::ui
