// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/ui/native_project_open.hpp"
#include "finaleffect/project/json.hpp"

#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>
#include <cctype>
#include <cmath>

namespace finaleffect::ui {

namespace {

std::string trimPathText(std::string_view sv) {
    std::size_t b = 0;
    while (b < sv.size() && std::isspace(static_cast<unsigned char>(sv[b])) != 0) ++b;
    std::size_t e = sv.size();
    while (e > b && std::isspace(static_cast<unsigned char>(sv[e - 1u])) != 0) --e;
    return std::string(sv.substr(b, e - b));
}

struct BundleInspection {
    bool file_exists{false};
    bool references_companion_bin{false};
    bool companion_bin_exists{false};
    std::string companion_bin_name{};
};

BundleInspection inspectCandidateBundle(
    const std::filesystem::path& fe3d_path,
    const platform::IFileSystem& fs) {
    BundleInspection info{};
    if (fe3d_path.empty() || !fs.exists(fe3d_path)) {
        return info;
    }
    info.file_exists = true;
    if (auto txt = fs.readTextFile(fe3d_path); txt) {
        if (auto parsed = project::parseJson(txt.value()); parsed && parsed.value().isObject()) {
            if (const auto* arch = parsed.value().findKey("mesh_archive"); arch != nullptr && arch->isString()) {
                info.references_companion_bin = !arch->asString().empty();
                info.companion_bin_name = arch->asString();
                if (info.references_companion_bin) {
                    const auto bin_path = fe3d_path.parent_path() / info.companion_bin_name;
                    info.companion_bin_exists = fs.exists(bin_path);
                }
            }
        }
    }
    return info;
}

} // namespace

void NativeProjectOpen::invokeNativeOpenDialog(
    INativeFileDialog& file_dialog,
    const std::filesystem::path& initial_dir,
    ProjectOpenActions& out_actions) {
    const auto res = file_dialog.pickOpenProjectFile("Open Final Effect 3D Project (.fe3d)", initial_dir);
    switch (res.status) {
        case FileDialogStatus::Selected:
            if (!res.selected_path.empty()) {
                error_message_.clear();
                path_input_ = res.selected_path.generic_string();
                out_actions.open_project_path = res.selected_path;
            }
            break;
        case FileDialogStatus::Cancelled:
            // Silent, error-free cancellation as required by Stage 1.1 Section 4D.
            break;
        case FileDialogStatus::Unavailable:
            out_actions.request_in_app_file_picker = true;
            break;
        case FileDialogStatus::Error:
            error_message_ = res.error_message.empty()
                ? std::string("Failed to open operating-system file dialog.")
                : res.error_message;
            break;
    }
}

void NativeProjectOpen::render(
    const NativeHomeTheme& theme,
    const platform::IFileSystem& file_system,
    const app::RecentProjectsIndex& recent_index,
    INativeFileDialog& file_dialog,
    const std::filesystem::path& default_dir,
    ProjectOpenActions& out_actions) {
    theme.drawViewHeader(
        "OPEN EXISTING DOCUMENT",
        "Open Project",
        "Open a Final Effect 3D .fe3d project file (and automatic companion .fe3dbin binary mesh archive) from local disk. Invalid files report clear diagnostics without corrupting your active session.");

    ImGui::Spacing();
    if (theme.styledButton(
            "Browse Local Files...##BtnOpenTopBrowse",
            ButtonVariant::PrimaryCyan,
            ImVec2(0, 0),
            false,
            HubIcon::Folder)) {
        invokeNativeOpenDialog(file_dialog, default_dir, out_actions);
    }

    ImGui::Dummy(ImVec2(0.0f, theme.px(6.0f)));
    if (!error_message_.empty()) {
        theme.drawBanner(error_message_, TagTone::Danger);
    }

    // Main Native Picker Card + Direct Path Bar
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ThemeColors::vec4(0x19, 0x1D, 0x24));
    ImGui::BeginChild(
        "##OpenProjectPickerPanel",
        ImVec2(0.0f, 0.0f),
        ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);

    const float avail_w = ImGui::GetContentRegionAvail().x;
    const float hero_h = std::round(theme.px(138.0f));
    const ImVec2 h0 = ImGui::GetCursorScreenPos();
    const ImVec2 h1(h0.x + avail_w, h0.y + hero_h);
    const bool clicked_zone = ImGui::InvisibleButton("##NativeOpenZoneBtn", ImVec2(avail_w, hero_h));
    const bool zone_hov = ImGui::IsItemHovered();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float rad = std::round(theme.px(8.0f));
    dl->AddRectFilled(h0, h1, zone_hov ? ThemeColors::kBgElevated : ThemeColors::kBgInput, rad);
    dl->AddRect(
        h0,
        h1,
        zone_hov ? ThemeColors::kAccentCyan : ThemeColors::kBorderStrong,
        rad,
        0,
        zone_hov ? 1.5f : 1.0f);

    const float cx = (h0.x + h1.x) * 0.5f;
    theme.drawIcon(dl, HubIcon::Folder, ImVec2(cx, h0.y + theme.px(32.0f)), theme.px(13.0f), ThemeColors::kAccentCyan);

    theme.pushSubheadingFont();
    const char* z_title = "Select a .fe3d Project File from Disk";
    const ImVec2 zt_sz = ImGui::CalcTextSize(z_title);
    dl->AddText(ImVec2(cx - zt_sz.x * 0.5f, h0.y + theme.px(58.0f)), ThemeColors::kTextPrimary, z_title);
    theme.popFont();

    const char* z_sub =
        "Supports inline JSON .fe3d (schema v1.0-v1.3) and deduplicated .fe3d + .fe3dbin binary mesh archives.";
    const ImVec2 zs_sz = ImGui::CalcTextSize(z_sub);
    dl->AddText(
        ImVec2(std::max(h0.x + theme.px(12.0f), cx - zs_sz.x * 0.5f), h0.y + theme.px(84.0f)),
        ThemeColors::kTextSecondary,
        z_sub);

    theme.pushMonoFont();
    const std::string portal_txt = "Dialog Backend: " + std::string(file_dialog.backendLabel());
    const ImVec2 zp_sz = ImGui::CalcTextSize(portal_txt.c_str());
    dl->AddText(ImVec2(cx - zp_sz.x * 0.5f, h0.y + theme.px(108.0f)), ThemeColors::kAccentCyan, portal_txt.c_str());
    theme.popFont();

    if (clicked_zone) {
        invokeNativeOpenDialog(file_dialog, default_dir, out_actions);
    }

    ImGui::Spacing();
    theme.pushBoldFont();
    ImGui::TextUnformatted("Or Open Directly by File Path (.fe3d)");
    theme.popFont();

    const float open_btn_w = std::round(theme.px(136.0f));
    ImGui::SetNextItemWidth(std::max(theme.px(160.0f), avail_w - open_btn_w - theme.px(10.0f)));
    const bool submitted_enter = ImGui::InputTextWithHint(
        "##OpenDirectPathInput",
        "/path/to/scene.fe3d",
        &path_input_,
        ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine(0.0f, theme.px(10.0f));
    const std::string clean_path = trimPathText(path_input_);
    if (theme.styledButton(
            "Open File##BtnOpenDirectPath",
            ButtonVariant::PrimaryCyan,
            ImVec2(open_btn_w, 0.0f),
            clean_path.empty()) ||
        (submitted_enter && !clean_path.empty())) {
        error_message_.clear();
        out_actions.open_project_path = std::filesystem::path(clean_path);
    }

    if (!clean_path.empty()) {
        const BundleInspection insp = inspectCandidateBundle(std::filesystem::path(clean_path), file_system);
        ImGui::Spacing();
        if (!insp.file_exists) {
            theme.drawTagPill("Path Not Found on Disk", TagTone::Amber);
        } else if (insp.references_companion_bin) {
            if (insp.companion_bin_exists) {
                theme.drawTagPill(
                    "Companion Bundle Verified: " + insp.companion_bin_name,
                    TagTone::Emerald);
            } else {
                theme.drawTagPill(
                    "Missing Companion Archive: " + insp.companion_bin_name,
                    TagTone::Danger);
            }
        } else {
            theme.drawTagPill("Inline .fe3d Document Found", TagTone::Emerald);
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0.0f, theme.px(10.0f)));

    // Quick Reopen Recent Projects List
    const auto recents = recent_index.query({}, app::RecentSortMode::LastOpenedDesc, false);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ThemeColors::vec4(0x19, 0x1D, 0x24));
    ImGui::BeginChild(
        "##OpenProjectQuickRecents",
        ImVec2(0.0f, 0.0f),
        ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);

    theme.pushSubheadingFont();
    ImGui::TextUnformatted("Quick Reopen Recent Projects");
    theme.popFont();
    ImGui::SameLine();
    theme.drawTagPill(std::to_string(std::min<std::size_t>(recents.size(), 8u)) + " shown", TagTone::Neutral);
    ImGui::Spacing();

    if (recents.empty()) {
        ImGui::TextColored(
            ThemeColors::vec4(0x88, 0x93, 0xA7),
            "No recent projects have been recorded in this desktop profile yet.");
    } else {
        const std::size_t limit = std::min<std::size_t>(recents.size(), 8u);
        for (std::size_t i = 0; i < limit; ++i) {
            const auto& r = recents[i];
            ImGui::PushID(r.id.c_str());
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ThemeColors::vec4(0x1F, 0x24, 0x2D));
            ImGui::BeginChild("##QuickReopenItem", ImVec2(0.0f, std::round(theme.px(66.0f))), ImGuiChildFlags_Borders);

            theme.pushBoldFont();
            ImGui::TextUnformatted(r.project_name.c_str());
            theme.popFont();
            ImGui::SameLine();
            theme.drawTagPill(r.exists_on_disk ? "On Disk" : "Missing", r.exists_on_disk ? TagTone::Emerald : TagTone::Amber);

            const float rbtn_w = theme.px(92.0f);
            if (ImGui::GetWindowWidth() > rbtn_w + theme.px(200.0f)) {
                ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - rbtn_w);
            }
            if (theme.styledButton(
                    "Reopen##QuickReopenBtn",
                    ButtonVariant::PrimaryCyan,
                    ImVec2(rbtn_w, theme.px(28.0f)),
                    !r.exists_on_disk)) {
                error_message_.clear();
                out_actions.open_project_path = std::filesystem::path(r.file_path);
            }

            theme.pushMonoFont();
            ImGui::TextColored(ThemeColors::vec4(0xA8, 0xB2, 0xC3), "%s", r.file_path.c_str());
            theme.popFont();

            ImGui::EndChild();
            ImGui::PopStyleColor();
            ImGui::PopID();
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

} // namespace finaleffect::ui
