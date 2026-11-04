// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/ui/native_home_navigation.hpp"
#include "finaleffect/app/project_hub_service.hpp"

#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <vector>

namespace finaleffect::ui {

namespace {

bool containsCaseInsensitive(std::string_view haystack, std::string_view needle) noexcept {
    if (needle.empty()) return true;
    if (haystack.size() < needle.size()) return false;
    for (std::size_t i = 0; i + needle.size() <= haystack.size(); ++i) {
        bool match = true;
        for (std::size_t j = 0; j < needle.size(); ++j) {
            const unsigned char a = static_cast<unsigned char>(haystack[i + j]);
            const unsigned char b = static_cast<unsigned char>(needle[j]);
            if (std::tolower(a) != std::tolower(b)) {
                match = false;
                break;
            }
        }
        if (match) return true;
    }
    return false;
}

bool isKeyJustPressedAnyOwner(ImGuiKey key) noexcept {
    if (key < ImGuiKey_NamedKey_BEGIN || key >= ImGuiKey_NamedKey_END) {
        return false;
    }
    const ImGuiKeyData& kd = ImGui::GetIO().KeysData[key - ImGuiKey_NamedKey_BEGIN];
    return kd.Down && kd.DownDuration == 0.0f;
}

} // namespace

const std::array<HomeHubViewDescriptor, 7>& allHomeHubViews() noexcept {
    static constexpr std::array<HomeHubViewDescriptor, 7> kViews = {{
        {HomeHubViewId::ProjectsStart, "projects", "Projects & Start", "Alt+1", HubIcon::Projects},
        {HomeHubViewId::Templates, "templates", "Templates", "Alt+2", HubIcon::Templates},
        {HomeHubViewId::NewProject, "new_project", "New Project", "Alt+3", HubIcon::Plus},
        {HomeHubViewId::OpenProject, "open_project", "Open Project", "Alt+4", HubIcon::Folder},
        {HomeHubViewId::Recovery, "recovery", "Recovery & Autosave", "Alt+5", HubIcon::Recovery},
        {HomeHubViewId::Preferences, "preferences", "Preferences", "Alt+6", HubIcon::Settings},
        {HomeHubViewId::About, "about", "About & System Info", "Alt+7", HubIcon::Info},
    }};
    return kViews;
}

std::string_view homeHubViewSlug(HomeHubViewId id) noexcept {
    for (const auto& v : allHomeHubViews()) {
        if (v.id == id) return v.slug;
    }
    return "projects";
}

std::string_view homeHubViewLabel(HomeHubViewId id) noexcept {
    for (const auto& v : allHomeHubViews()) {
        if (v.id == id) return v.label;
    }
    return "Projects & Start";
}

std::optional<HomeHubViewId> parseHomeHubViewSlug(std::string_view slug) noexcept {
    for (const auto& v : allHomeHubViews()) {
        if (v.slug == slug) return v.id;
    }
    return std::nullopt;
}

void NativeHomeNavigation::openCommandPalette() noexcept {
    palette_open_ = true;
    focus_palette_input_ = true;
    palette_query_.clear();
    palette_selected_idx_ = 0;
}

void NativeHomeNavigation::drawTopBar(
    const NativeHomeTheme& theme,
    float width,
    float height,
    const HubShellMetrics& metrics,
    HubNavigationActions& out_actions) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 bar_min = ImGui::GetCursorScreenPos();
    const ImVec2 bar_max(bar_min.x + width, bar_min.y + height);

    dl->AddRectFilled(bar_min, bar_max, ThemeColors::kBgShell);
    dl->AddLine(
        ImVec2(bar_min.x, bar_max.y - 1.0f),
        ImVec2(bar_max.x, bar_max.y - 1.0f),
        ThemeColors::kBorderSubtle,
        1.0f);

    const float pad_x = std::round(theme.px(16.0f));
    const float logo_sz = std::round(theme.px(32.0f));
    const ImVec2 logo_pos(bar_min.x + pad_x, bar_min.y + (height - logo_sz) * 0.5f);
    theme.drawBrandLogo(dl, logo_pos, logo_sz);

    // Brand Title + Monospace Engine Badge
    const float text_x = logo_pos.x + logo_sz + std::round(theme.px(10.0f));
    theme.pushBoldFont();
    const char* brand_title = "Final Effect 3D";
    const ImVec2 title_sz = ImGui::CalcTextSize(brand_title);
    dl->AddText(
        ImVec2(text_x, bar_min.y + (height - title_sz.y * 1.95f) * 0.5f),
        ThemeColors::kTextPrimary,
        brand_title);
    theme.popFont();

    theme.pushMonoFont();
    const std::string badge_txt = "v" + metrics.engine_version + " · Native C++20 · " + metrics.graphics_backend;
    const ImVec2 badge_sz = ImGui::CalcTextSize(badge_txt.c_str());
    dl->AddText(
        ImVec2(text_x, bar_min.y + height * 0.5f + std::round(theme.px(1.0f))),
        ThemeColors::kTextMuted,
        badge_txt.c_str());
    theme.popFont();

    const float left_used = (text_x - bar_min.x) + std::max(title_sz.x, badge_sz.x) + std::round(theme.px(18.0f));

    // Measure right-side action controls first so center Command Palette trigger never overlaps at 1366x768 @ 125%
    const bool compact_bar = width < theme.px(1320.0f);
    const char* open_lbl = compact_bar ? "Open...##TopOpen" : "Open .fe3d...##TopOpen";
    const char* new_lbl = compact_bar ? "+ New##TopNew" : "+ New Project##TopNew";

    float right_est = theme.px(compact_bar ? 210.0f : 275.0f);
    if (metrics.has_active_studio_session) {
        right_est += theme.px(compact_bar ? 155.0f : 210.0f);
    }

    // Center Command Palette Search Trigger
    const float cmd_max_w = std::round(theme.px(380.0f));
    const float avail_center = width - left_used - right_est - pad_x * 2.0f;
    if (avail_center >= theme.px(170.0f)) {
        const float cmd_w = std::min(cmd_max_w, avail_center);
        const float cmd_h = std::round(theme.px(32.0f));
        const float cmd_x = std::max(
            bar_min.x + left_used,
            bar_min.x + (width - cmd_w) * 0.5f - (metrics.has_active_studio_session ? theme.px(50.0f) : 0.0f));
        const ImVec2 cmd_min(cmd_x, bar_min.y + (height - cmd_h) * 0.5f);
        ImGui::SetCursorScreenPos(cmd_min);
        const bool clicked_cmd = ImGui::InvisibleButton("##TopCommandPaletteBtn", ImVec2(cmd_w, cmd_h));
        const bool cmd_hov = ImGui::IsItemHovered();
        const ImVec2 cmd_max(cmd_min.x + cmd_w, cmd_min.y + cmd_h);
        const float rad = std::round(theme.px(6.0f));

        dl->AddRectFilled(cmd_min, cmd_max, cmd_hov ? ThemeColors::kBgElevated : ThemeColors::kBgInput, rad);
        dl->AddRect(
            cmd_min,
            cmd_max,
            cmd_hov ? ThemeColors::kBorderStrong : ThemeColors::kBorderDefault,
            rad,
            0,
            1.0f);
        theme.drawIcon(
            dl,
            HubIcon::Search,
            ImVec2(cmd_min.x + theme.px(14.0f), (cmd_min.y + cmd_max.y) * 0.5f),
            theme.px(5.5f),
            ThemeColors::kTextMuted);

        const char* hint = (cmd_w > theme.px(265.0f))
            ? "Search Hub commands, templates & views..."
            : "Search commands...";
        const ImVec2 h_sz = ImGui::CalcTextSize(hint);
        dl->AddText(
            ImVec2(cmd_min.x + theme.px(28.0f), (cmd_min.y + cmd_max.y - h_sz.y) * 0.5f),
            ThemeColors::kTextMuted,
            hint);

        if (cmd_w > theme.px(210.0f)) {
            theme.pushMonoFont();
            const char* kbd = "Ctrl+K";
            const ImVec2 k_sz = ImGui::CalcTextSize(kbd);
            const ImVec2 kp0(cmd_max.x - k_sz.x - theme.px(18.0f), (cmd_min.y + cmd_max.y - k_sz.y) * 0.5f - theme.px(2.0f));
            const ImVec2 kp1(cmd_max.x - theme.px(6.0f), (cmd_min.y + cmd_max.y + k_sz.y) * 0.5f + theme.px(2.0f));
            dl->AddRectFilled(kp0, kp1, ThemeColors::kBgElevated, std::round(theme.px(4.0f)));
            dl->AddRect(kp0, kp1, ThemeColors::kBorderDefault, std::round(theme.px(4.0f)), 0, 1.0f);
            dl->AddText(ImVec2(kp0.x + theme.px(6.0f), kp0.y + theme.px(2.0f)), ThemeColors::kTextSecondary, kbd);
            theme.popFont();
        }
        if (clicked_cmd) {
            openCommandPalette();
        }
    }

    // Right-Side Topbar Actions
    const float btn_h = std::round(theme.px(32.0f));
    const float btn_y = bar_min.y + (height - btn_h) * 0.5f;

    theme.pushBoldFont();
    const float new_w = ImGui::CalcTextSize(compact_bar ? "+ New" : "+ New Project").x + theme.px(28.0f);
    const float open_w = ImGui::CalcTextSize(compact_bar ? "Open..." : "Open .fe3d...").x + theme.px(40.0f);
    const float resume_w = metrics.has_active_studio_session
        ? (ImGui::CalcTextSize(compact_bar ? "Return to Studio" : "Return to 3D Studio").x + theme.px(28.0f))
        : 0.0f;
    theme.popFont();

    const float gap = std::round(theme.px(8.0f));
    float cur_right = bar_max.x - pad_x;

    cur_right -= new_w;
    ImGui::SetCursorScreenPos(ImVec2(cur_right, btn_y));
    if (theme.styledButton(new_lbl, ButtonVariant::PrimaryCyan, ImVec2(new_w, btn_h))) {
        out_actions.navigate_to = HomeHubViewId::NewProject;
    }

    cur_right -= (gap + open_w);
    ImGui::SetCursorScreenPos(ImVec2(cur_right, btn_y));
    if (theme.styledButton(open_lbl, ButtonVariant::Secondary, ImVec2(open_w, btn_h), false, HubIcon::Folder)) {
        out_actions.trigger_open_dialog = true;
    }

    if (metrics.has_active_studio_session) {
        cur_right -= (gap + resume_w);
        ImGui::SetCursorScreenPos(ImVec2(cur_right, btn_y));
        const char* res_lbl = compact_bar ? "Return to Studio##TopResume" : "Return to 3D Studio##TopResume";
        if (theme.styledButton(res_lbl, ButtonVariant::SolarAmber, ImVec2(resume_w, btn_h))) {
            out_actions.resume_studio = true;
        }

        if (!compact_bar && (cur_right - bar_min.x) > left_used + theme.px(200.0f)) {
            std::string sess_tag = metrics.active_project_name.empty() ? "Untitled" : metrics.active_project_name;
            if (sess_tag.size() > 20u) {
                sess_tag = sess_tag.substr(0u, 18u) + "..";
            }
            sess_tag += metrics.studio_session_dirty ? " • Unsaved" : " • Saved";
            theme.pushMonoFont();
            const float pill_w = ImGui::CalcTextSize(sess_tag.c_str()).x + theme.px(18.0f);
            theme.popFont();
            if (cur_right - gap - pill_w > bar_min.x + left_used + theme.px(180.0f)) {
                ImGui::SetCursorScreenPos(ImVec2(cur_right - gap - pill_w, btn_y + theme.px(4.0f)));
                theme.drawTagPill(sess_tag, metrics.studio_session_dirty ? TagTone::Amber : TagTone::Emerald);
            }
        }
    }
}

void NativeHomeNavigation::drawLeftRail(
    const NativeHomeTheme& theme,
    float width,
    float height,
    HomeHubViewId active_view,
    const HubShellMetrics& metrics,
    HubNavigationActions& out_actions) const {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 rail_min = ImGui::GetCursorScreenPos();
    const ImVec2 rail_max(rail_min.x + width, rail_min.y + height);

    dl->AddRectFilled(rail_min, rail_max, ThemeColors::kBgShell);
    dl->AddLine(
        ImVec2(rail_max.x - 1.0f, rail_min.y),
        ImVec2(rail_max.x - 1.0f, rail_max.y),
        ThemeColors::kBorderSubtle,
        1.0f);

    const float pad_x = std::round(theme.px(12.0f));
    float cur_y = rail_min.y + std::round(theme.px(14.0f));

    theme.pushMonoFont();
    dl->AddText(
        ImVec2(rail_min.x + pad_x + theme.px(4.0f), cur_y),
        ThemeColors::kTextMuted,
        "DESKTOP WORKSPACE");
    cur_y += ImGui::GetTextLineHeight() + std::round(theme.px(10.0f));
    theme.popFont();

    const float item_w = width - pad_x * 2.0f;
    const float item_h = std::round(theme.px(38.0f));
    const float item_gap = std::round(theme.px(4.0f));

    for (const auto& item : allHomeHubViews()) {
        if (item.id == HomeHubViewId::Preferences) {
            cur_y += std::round(theme.px(6.0f));
            dl->AddLine(
                ImVec2(rail_min.x + pad_x, cur_y),
                ImVec2(rail_max.x - pad_x, cur_y),
                ThemeColors::kBorderSubtle,
                1.0f);
            cur_y += std::round(theme.px(8.0f));
        }

        const ImVec2 p0(rail_min.x + pad_x, cur_y);
        const ImVec2 p1(p0.x + item_w, p0.y + item_h);
        ImGui::SetCursorScreenPos(p0);

        char btn_id[64]{};
        std::snprintf(btn_id, sizeof(btn_id), "##NavRail_%s", item.slug.data());
        const bool clicked = ImGui::InvisibleButton(btn_id, ImVec2(item_w, item_h));
        const bool hovered = ImGui::IsItemHovered();
        const bool focused = ImGui::IsItemFocused();
        const bool selected = (item.id == active_view);

        const float rad = std::round(theme.px(6.0f));
        if (selected) {
            dl->AddRectFilled(p0, p1, ThemeColors::kBgActive, rad);
            dl->AddRect(p0, p1, ThemeColors::pack(0x27, 0xB6, 0xD9, 0x68), rad, 0, 1.0f);
            dl->AddRectFilled(
                ImVec2(p0.x, p0.y + theme.px(7.0f)),
                ImVec2(p0.x + std::max(3.0f, theme.px(3.0f)), p1.y - theme.px(7.0f)),
                ThemeColors::kAccentCyan,
                2.0f);
        } else if (hovered) {
            dl->AddRectFilled(p0, p1, ThemeColors::kBgElevated, rad);
        }
        if (focused) {
            dl->AddRect(p0, p1, ThemeColors::kBorderFocus, rad, 0, 1.5f);
        }

        const ImU32 icon_col = selected ? ThemeColors::kAccentCyan
                                        : (hovered ? ThemeColors::kTextPrimary : ThemeColors::kTextSecondary);
        const ImU32 lbl_col = selected ? ThemeColors::kTextPrimary
                                       : (hovered ? ThemeColors::kTextPrimary : ThemeColors::kTextSecondary);

        theme.drawIcon(
            dl,
            item.icon,
            ImVec2(p0.x + theme.px(18.0f), (p0.y + p1.y) * 0.5f),
            theme.px(6.2f),
            icon_col);

        if (selected) {
            theme.pushBoldFont();
        } else {
            theme.pushBodyFont();
        }
        const ImVec2 lbl_sz = ImGui::CalcTextSize(item.label.data(), item.label.data() + item.label.size());
        dl->AddText(
            ImVec2(p0.x + theme.px(34.0f), (p0.y + p1.y - lbl_sz.y) * 0.5f),
            lbl_col,
            item.label.data(),
            item.label.data() + item.label.size());
        theme.popFont();

        // Live Count / Warning Badges on Projects & Recovery items, or Shortcut hint
        theme.pushMonoFont();
        std::string right_badge;
        ImU32 badge_col = ThemeColors::kTextMuted;
        if (item.id == HomeHubViewId::ProjectsStart && metrics.recent_count > 0) {
            right_badge = std::to_string(metrics.recent_count);
            badge_col = ThemeColors::kAccentCyan;
        } else if (item.id == HomeHubViewId::Recovery && metrics.recovery_count > 0) {
            right_badge = std::to_string(metrics.recovery_count);
            badge_col = metrics.recovery_unclean_warning ? ThemeColors::kAccentAmber : ThemeColors::kSemanticEmerald;
        } else if (width >= theme.px(225.0f)) {
            right_badge.assign(item.shortcut);
        }
        if (!right_badge.empty()) {
            const ImVec2 b_sz = ImGui::CalcTextSize(right_badge.c_str());
            if (p0.x + theme.px(38.0f) + lbl_sz.x < p1.x - b_sz.x - theme.px(10.0f)) {
                dl->AddText(
                    ImVec2(p1.x - b_sz.x - theme.px(10.0f), (p0.y + p1.y - b_sz.y) * 0.5f),
                    badge_col,
                    right_badge.c_str());
            }
        }
        theme.popFont();

        if (clicked) {
            out_actions.navigate_to = item.id;
        }

        cur_y += item_h + item_gap;
    }

    // Bottom Rail Calibration Card (only drawn when vertical height comfortably allows it)
    const float footer_h = std::round(theme.px(66.0f));
    const float footer_y = rail_max.y - footer_h - std::round(theme.px(12.0f));
    if (footer_y > cur_y + theme.px(12.0f)) {
        const ImVec2 f0(rail_min.x + pad_x, footer_y);
        const ImVec2 f1(rail_max.x - pad_x, footer_y + footer_h);
        const float rad = std::round(theme.px(6.0f));
        dl->AddRectFilled(f0, f1, ThemeColors::kBgPanel, rad);
        dl->AddRect(f0, f1, ThemeColors::kBorderSubtle, rad, 0, 1.0f);

        theme.pushMonoFont();
        dl->AddText(
            ImVec2(f0.x + theme.px(10.0f), f0.y + theme.px(10.0f)),
            ThemeColors::kAccentCyan,
            "CALIBRATED ENGINE");
        const std::string l1 = "Right-Handed +Y · Meters";
        const std::string l2 = "UI Scale: " + std::to_string(metrics.ui_scale_percent) + "%";
        dl->AddText(
            ImVec2(f0.x + theme.px(10.0f), f0.y + theme.px(28.0f)),
            ThemeColors::kTextSecondary,
            l1.c_str());
        dl->AddText(
            ImVec2(f0.x + theme.px(10.0f), f0.y + theme.px(44.0f)),
            ThemeColors::kTextMuted,
            l2.c_str());
        theme.popFont();
    }
}

void NativeHomeNavigation::drawStatusStrip(
    const NativeHomeTheme& theme,
    float width,
    float height,
    const HubShellMetrics& metrics) const {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    const ImVec2 p1(p0.x + width, p0.y + height);

    dl->AddRectFilled(p0, p1, ThemeColors::kBgShell);
    dl->AddLine(p0, ImVec2(p1.x, p0.y), ThemeColors::kBorderSubtle, 1.0f);

    const float pad_x = std::round(theme.px(16.0f));
    const float mid_y = (p0.y + p1.y) * 0.5f;
    const ImU32 dot_col = metrics.recovery_available ? ThemeColors::kSemanticEmerald : ThemeColors::kAccentAmber;
    dl->AddCircleFilled(ImVec2(p0.x + pad_x + theme.px(4.0f), mid_y), theme.px(3.5f), dot_col);

    theme.pushMonoFont();
    const std::string left_msg = metrics.graphics_backend + "  ·  Recovery Storage: " +
                                 (metrics.recovery_available ? "Available" : "Unavailable") +
                                 "  ·  .fe3d v1.3 / .fe3dbin";
    const ImVec2 l_sz = ImGui::CalcTextSize(left_msg.c_str());
    dl->AddText(
        ImVec2(p0.x + pad_x + theme.px(14.0f), mid_y - l_sz.y * 0.5f),
        ThemeColors::kTextSecondary,
        left_msg.c_str());

    const char* shortcuts = "Alt+1..7 Views  ·  Ctrl+N New  ·  Ctrl+O Open  ·  Ctrl+K Palette";
    const ImVec2 r_sz = ImGui::CalcTextSize(shortcuts);
    if (p0.x + pad_x + theme.px(24.0f) + l_sz.x < p1.x - pad_x - r_sz.x) {
        dl->AddText(
            ImVec2(p1.x - pad_x - r_sz.x, mid_y - r_sz.y * 0.5f),
            ThemeColors::kTextMuted,
            shortcuts);
    }
    theme.popFont();
}

void NativeHomeNavigation::drawCommandPalette(
    const NativeHomeTheme& theme,
    const HubShellMetrics& metrics,
    HubNavigationActions& out_actions) {
    if (!palette_open_) {
        return;
    }

    struct PaletteEntry {
        std::string title;
        std::string category;
        std::string shortcut;
        std::optional<HomeHubViewId> view_id{};
        std::optional<app::ProjectTemplate> tmpl{};
        bool open_dialog{false};
        bool resume_studio{false};
    };

    std::vector<PaletteEntry> entries;
    entries.reserve(14u);
    if (metrics.has_active_studio_session) {
        entries.push_back(PaletteEntry{
            .title = "Return to Active 3D Studio Session",
            .category = "Session",
            .shortcut = "F2",
            .resume_studio = true,
        });
    }
    for (const auto& v : allHomeHubViews()) {
        entries.push_back(PaletteEntry{
            .title = "Go to " + std::string(v.label),
            .category = "Navigation",
            .shortcut = std::string(v.shortcut),
            .view_id = v.id,
        });
    }
    entries.push_back(PaletteEntry{
        .title = "Open Existing .fe3d Project File...",
        .category = "File",
        .shortcut = "Ctrl+O",
        .open_dialog = true,
    });
    for (const auto& t : app::TemplateCatalog::all()) {
        entries.push_back(PaletteEntry{
            .title = "Create from Template: " + std::string(t.title),
            .category = "C++20 Template",
            .shortcut = "",
            .tmpl = t.id,
        });
    }

    std::vector<std::size_t> filtered;
    for (std::size_t i = 0; i < entries.size(); ++i) {
        if (containsCaseInsensitive(entries[i].title, palette_query_) ||
            containsCaseInsensitive(entries[i].category, palette_query_)) {
            filtered.push_back(i);
        }
    }
    if (palette_selected_idx_ >= filtered.size()) {
        palette_selected_idx_ = 0;
    }

    const ImVec2 disp = ImGui::GetIO().DisplaySize;
    const float modal_w = std::min(disp.x - theme.px(48.0f), std::round(theme.px(560.0f)));
    const float modal_h = std::min(disp.y - theme.px(80.0f), std::round(theme.px(380.0f)));
    ImGui::SetNextWindowPos(ImVec2((disp.x - modal_w) * 0.5f, disp.y * 0.14f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(modal_w, modal_h), ImGuiCond_Always);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ThemeColors::vec4(0x18, 0x1C, 0x24));
    ImGui::PushStyleColor(ImGuiCol_Border, ThemeColors::vec4(0x27, 0xB6, 0xD9));
    if (ImGui::Begin(
            "##HubCommandPaletteWindow",
            nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoSavedSettings)) {
        theme.pushBoldFont();
        ImGui::TextUnformatted("Hub Command Palette");
        theme.popFont();
        ImGui::SameLine(modal_w - theme.px(76.0f));
        if (theme.styledButton("Esc##ClosePalette", ButtonVariant::Ghost, ImVec2(theme.px(60.0f), theme.px(24.0f)))) {
            palette_open_ = false;
        }

        ImGui::SetNextItemWidth(-1.0f);
        if (focus_palette_input_) {
            ImGui::SetKeyboardFocusHere();
            focus_palette_input_ = false;
        }
        if (ImGui::InputTextWithHint(
                "##PaletteSearchInput",
                "Type a view, C++ template, or Hub action...",
                &palette_query_)) {
            palette_selected_idx_ = 0;
        }

        if (isKeyJustPressedAnyOwner(ImGuiKey_Escape)) {
            palette_open_ = false;
        } else if (!filtered.empty() && isKeyJustPressedAnyOwner(ImGuiKey_DownArrow)) {
            palette_selected_idx_ = (palette_selected_idx_ + 1u) % filtered.size();
        } else if (!filtered.empty() && isKeyJustPressedAnyOwner(ImGuiKey_UpArrow)) {
            palette_selected_idx_ = (palette_selected_idx_ == 0u) ? (filtered.size() - 1u) : (palette_selected_idx_ - 1u);
        }

        bool activate_selected = !filtered.empty() && isKeyJustPressedAnyOwner(ImGuiKey_Enter);
        ImGui::Spacing();
        ImGui::BeginChild("##PaletteResults", ImVec2(0.0f, 0.0f), true);
        for (std::size_t row = 0; row < filtered.size(); ++row) {
            const auto& entry = entries[filtered[row]];
            const bool is_sel = (row == palette_selected_idx_);
            ImGui::PushID(static_cast<int>(row));
            if (ImGui::Selectable(entry.title.c_str(), is_sel, 0, ImVec2(0.0f, theme.px(26.0f)))) {
                palette_selected_idx_ = row;
                activate_selected = true;
            }
            if (!entry.shortcut.empty()) {
                theme.pushMonoFont();
                const ImVec2 sc_sz = ImGui::CalcTextSize(entry.shortcut.c_str());
                ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - sc_sz.x - theme.px(8.0f));
                ImGui::TextColored(ThemeColors::vec4(0x88, 0x93, 0xA7), "%s", entry.shortcut.c_str());
                theme.popFont();
            }
            ImGui::PopID();
        }
        if (filtered.empty()) {
            ImGui::TextColored(ThemeColors::vec4(0x88, 0x93, 0xA7), "No matching Hub commands or templates.");
        }
        ImGui::EndChild();

        if (activate_selected && palette_selected_idx_ < filtered.size()) {
            const auto& chosen = entries[filtered[palette_selected_idx_]];
            if (chosen.view_id.has_value()) out_actions.navigate_to = chosen.view_id;
            if (chosen.tmpl.has_value()) out_actions.quick_create_template = chosen.tmpl;
            if (chosen.open_dialog) out_actions.trigger_open_dialog = true;
            if (chosen.resume_studio) out_actions.resume_studio = true;
            palette_open_ = false;
        }
    }
    ImGui::End();
    ImGui::PopStyleColor(2);
}

} // namespace finaleffect::ui
