// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/ui/native_project_browser.hpp"

#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

namespace finaleffect::ui {

void NativeProjectBrowser::render(
    const NativeHomeTheme& theme,
    app::RecentProjectsIndex& recent_index,
    const app::RecoveryService& recovery_service,
    const HubShellMetrics& shell_metrics,
    ProjectBrowserActions& out_actions) {
    // View Header Row
    const float avail_w = ImGui::GetContentRegionAvail().x;
    ImGui::BeginGroup();
    theme.drawViewHeader(
        "STAGE 1 NATIVE PROJECT HUB",
        "Projects & Start",
        "Create calibrated 3D scenes from C++20 templates, open portable .fe3d documents, or resume recent desktop work.");
    ImGui::EndGroup();

    ImGui::Spacing();
    // Primary Action Row
    if (shell_metrics.has_active_studio_session) {
        std::string res_lbl = "Resume '" +
                              (shell_metrics.active_project_name.empty() ? std::string("Untitled") : shell_metrics.active_project_name) +
                              "'" + (shell_metrics.studio_session_dirty ? " • Unsaved##ProjResume" : "##ProjResume");
        if (theme.styledButton(res_lbl.c_str(), ButtonVariant::SolarAmber)) {
            out_actions.resume_studio = true;
        }
        ImGui::SameLine();
    }
    if (theme.styledButton("Open .fe3d...##ProjOpenBtn", ButtonVariant::Secondary, ImVec2(0, 0), false, HubIcon::Folder)) {
        out_actions.trigger_open_dialog = true;
    }
    ImGui::SameLine();
    if (theme.styledButton("New Project...##ProjNewBtn", ButtonVariant::PrimaryCyan, ImVec2(0, 0), false, HubIcon::Plus)) {
        out_actions.navigate_to = HomeHubViewId::NewProject;
    }
    if (!recovery_service.snapshots().empty()) {
        ImGui::SameLine();
        const std::string rec_btn =
            "Recovery (" + std::to_string(recovery_service.snapshots().size()) + ")##ProjRecBtn";
        if (theme.styledButton(
                rec_btn.c_str(),
                recovery_service.hadUncleanPreviousSession() ? ButtonVariant::SolarAmber : ButtonVariant::Secondary,
                ImVec2(0, 0),
                false,
                HubIcon::Recovery)) {
            out_actions.navigate_to = HomeHubViewId::Recovery;
        }
    }

    ImGui::Dummy(ImVec2(0.0f, theme.px(6.0f)));

    // Unclean Recovery Notice Banner if previous session exited uncleanly
    if (recovery_service.hadUncleanPreviousSession() && !recovery_service.snapshots().empty()) {
        const std::string msg =
            "Previous Studio session terminated without a clean shutdown marker. " +
            std::to_string(recovery_service.snapshots().size()) +
            " genuine autosave checkpoint(s) are ready in Recovery & Autosave.";
        theme.drawBanner(msg, TagTone::Amber);
    }

    // 3 Quick-Start Action Cards
    const int quick_cols = (avail_w >= theme.px(780.0f)) ? 3 : 1;
    const float gap = std::round(theme.px(12.0f));
    const float card_w = (avail_w - gap * static_cast<float>(quick_cols - 1)) / static_cast<float>(quick_cols);
    const float card_h = std::round(theme.px(108.0f));

    struct QuickCardSpec {
        const char* id;
        const char* title;
        const char* tag;
        TagTone tag_tone;
        const char* desc;
    };
    const QuickCardSpec specs[3] = {
        {"##QuickStarterCard",
         "Quick Starter Showcase",
         "Instant",
         TagTone::Cyan,
         "Initialize the C++20 Starter Showcase rig (HeroCube, AccentPyramid, MainCamera, SunLight) and enter 3D Studio."},
        {"##QuickConfigNewCard",
         "Configure New Project",
         "Validated",
         TagTone::Neutral,
         "Set project name, destination folder, scene units, frame rate, and overwrite protection before creating."},
        {"##QuickTemplatesCard",
         "Explore 4 C++ Templates",
         "Presets",
         TagTone::Neutral,
         "Inspect Starter Showcase, Minimal Cube, Empty Modeling Stage, and Shader Ball Studio presets."},
    };

    for (int i = 0; i < 3; ++i) {
        if (quick_cols > 1 && i > 0) {
            ImGui::SameLine(0.0f, gap);
        }
        const ImVec2 p0 = ImGui::GetCursorScreenPos();
        const ImVec2 p1(p0.x + card_w, p0.y + card_h);
        const bool clicked = ImGui::InvisibleButton(specs[i].id, ImVec2(card_w, card_h));
        const bool hov = ImGui::IsItemHovered();
        const bool foc = ImGui::IsItemFocused();

        ImDrawList* dl = ImGui::GetWindowDrawList();
        const float rad = std::round(theme.px(8.0f));
        dl->AddRectFilled(p0, p1, hov ? ThemeColors::kBgElevated : ThemeColors::kBgPanel, rad);
        dl->AddRect(
            p0,
            p1,
            foc ? ThemeColors::kBorderFocus : (hov ? ThemeColors::kAccentCyan : ThemeColors::kBorderDefault),
            rad,
            0,
            foc ? 1.5f : 1.0f);

        const float cpad = std::round(theme.px(14.0f));
        theme.pushBoldFont();
        dl->AddText(ImVec2(p0.x + cpad, p0.y + cpad), ThemeColors::kTextPrimary, specs[i].title);
        theme.popFont();

        theme.pushMonoFont();
        const ImVec2 t_sz = ImGui::CalcTextSize(specs[i].tag);
        const ImVec2 tp0(p1.x - cpad - t_sz.x - theme.px(12.0f), p0.y + cpad - theme.px(2.0f));
        const ImVec2 tp1(p1.x - cpad, tp0.y + t_sz.y + theme.px(5.0f));
        dl->AddRectFilled(
            tp0,
            tp1,
            specs[i].tag_tone == TagTone::Cyan ? ThemeColors::kAccentCyanMuted : ThemeColors::kBgElevated,
            std::round(theme.px(4.0f)));
        dl->AddRect(
            tp0,
            tp1,
            specs[i].tag_tone == TagTone::Cyan ? ThemeColors::kAccentCyan : ThemeColors::kBorderDefault,
            std::round(theme.px(4.0f)),
            0,
            1.0f);
        dl->AddText(
            ImVec2(tp0.x + theme.px(6.0f), tp0.y + theme.px(2.5f)),
            specs[i].tag_tone == TagTone::Cyan ? ThemeColors::kAccentCyan : ThemeColors::kTextSecondary,
            specs[i].tag);
        theme.popFont();

        dl->AddText(
            theme.fontBody(),
            theme.fontBody()->FontSize,
            ImVec2(p0.x + cpad, p0.y + cpad + theme.px(26.0f)),
            ThemeColors::kTextSecondary,
            specs[i].desc,
            nullptr,
            card_w - cpad * 2.0f);

        if (clicked) {
            if (i == 0) {
                out_actions.quick_create_template = app::ProjectTemplate::StarterShowcase;
            } else if (i == 1) {
                out_actions.navigate_to = HomeHubViewId::NewProject;
            } else {
                out_actions.navigate_to = HomeHubViewId::Templates;
            }
        }
        if (quick_cols == 1) {
            ImGui::Dummy(ImVec2(0.0f, theme.px(4.0f)));
        }
    }

    ImGui::Dummy(ImVec2(0.0f, theme.px(10.0f)));

    // Recent Projects Panel
    const std::size_t total_recorded = recent_index.entries().size();
    std::vector<app::RecentProjectEntry> filtered = recent_index.query(
        search_query_,
        sort_mode_,
        disk_filter_ == RecentDiskFilter::PinnedOnly);
    if (disk_filter_ == RecentDiskFilter::OnDiskOnly) {
        std::erase_if(filtered, [](const app::RecentProjectEntry& e) { return !e.exists_on_disk; });
    }

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ThemeColors::vec4(0x19, 0x1D, 0x24));
    ImGui::BeginChild("##RecentProjectsSection", ImVec2(0.0f, 0.0f), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);

    // Panel Header
    theme.pushSubheadingFont();
    ImGui::TextUnformatted("Recent Projects");
    theme.popFont();
    ImGui::SameLine();
    theme.drawTagPill(std::to_string(total_recorded) + " recorded", TagTone::Neutral);

    if (total_recorded > 0) {
        const float right_btn_w = theme.px(124.0f);
        if (ImGui::GetContentRegionAvail().x > right_btn_w + theme.px(16.0f)) {
            ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - right_btn_w);
        }
        if (theme.styledButton("Clear Unpinned##BtnClearUnpinned", ButtonVariant::Ghost, ImVec2(right_btn_w, theme.px(28.0f)))) {
            if (recent_index.clearUnpinned() > 0) {
                out_actions.recent_index_mutated = true;
            }
        }
    }

    ImGui::Spacing();

    // Filter, Search, Sort & Layout Toolbar
    const float search_w = std::clamp(avail_w * 0.36f, theme.px(180.0f), theme.px(380.0f));
    ImGui::SetNextItemWidth(search_w);
    ImGui::InputTextWithHint(
        "##RecentSearchBox",
        "Search by project name, path, scene, or author...",
        &search_query_);

    ImGui::SameLine();
    ImGui::SetNextItemWidth(theme.px(150.0f));
    const char* filter_labels[] = {"All Projects", "Pinned Only", "On Disk Only"};
    int cur_filter = static_cast<int>(disk_filter_);
    if (ImGui::Combo("##RecentDiskFilter", &cur_filter, filter_labels, 3)) {
        disk_filter_ = static_cast<RecentDiskFilter>(cur_filter);
    }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(theme.px(162.0f));
    const char* sort_labels[] = {"Sort: Last Opened", "Sort: Name (A-Z)", "Sort: File Size"};
    int cur_sort = static_cast<int>(sort_mode_);
    if (ImGui::Combo("##RecentSortMode", &cur_sort, sort_labels, 3)) {
        sort_mode_ = static_cast<app::RecentSortMode>(cur_sort);
    }

    ImGui::SameLine();
    if (theme.styledButton(
            "List##LayoutListBtn",
            layout_mode_ == ProjectLayoutMode::List ? ButtonVariant::PrimaryCyan : ButtonVariant::Secondary,
            ImVec2(theme.px(66.0f), theme.px(30.0f)),
            false,
            HubIcon::List)) {
        layout_mode_ = ProjectLayoutMode::List;
    }
    ImGui::SameLine(0.0f, theme.px(4.0f));
    if (theme.styledButton(
            "Grid##LayoutGridBtn",
            layout_mode_ == ProjectLayoutMode::Grid ? ButtonVariant::PrimaryCyan : ButtonVariant::Secondary,
            ImVec2(theme.px(68.0f), theme.px(30.0f)),
            false,
            HubIcon::Grid)) {
        layout_mode_ = ProjectLayoutMode::Grid;
    }

    ImGui::Dummy(ImVec2(0.0f, theme.px(6.0f)));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0.0f, theme.px(6.0f)));

    // Honest Empty States vs Populated Recent Project Cards
    if (total_recorded == 0) {
        ImGui::Dummy(ImVec2(0.0f, theme.px(8.0f)));
        theme.drawTagPill("CLEAN PROFILE · 0 RECENTS", TagTone::Cyan);
        ImGui::Spacing();
        theme.pushSubheadingFont();
        ImGui::TextUnformatted("No Recent Projects Recorded Yet");
        theme.popFont();
        ImGui::PushStyleColor(ImGuiCol_Text, ThemeColors::vec4(0xA8, 0xB2, 0xC3));
        ImGui::TextWrapped(
            "Projects you create or open from disk appear here automatically with verified scene counts and real file paths — never placeholder entries.");
        ImGui::PopStyleColor();
        ImGui::Spacing();
        if (theme.styledButton("Create New Project##EmptyCreate", ButtonVariant::PrimaryCyan, ImVec2(0, 0), false, HubIcon::Plus)) {
            out_actions.navigate_to = HomeHubViewId::NewProject;
        }
        ImGui::SameLine();
        if (theme.styledButton("Open Existing .fe3d...##EmptyOpen", ButtonVariant::Secondary, ImVec2(0, 0), false, HubIcon::Folder)) {
            out_actions.trigger_open_dialog = true;
        }
        ImGui::Dummy(ImVec2(0.0f, theme.px(8.0f)));
    } else if (filtered.empty()) {
        theme.drawTagPill("0 MATCHES", TagTone::Amber);
        ImGui::Spacing();
        ImGui::TextColored(
            ThemeColors::vec4(0xA8, 0xB2, 0xC3),
            "No recorded projects match the active search or filter criteria.");
        if (theme.styledButton("Reset Search & Filters##BtnResetSearch", ButtonVariant::Secondary)) {
            search_query_.clear();
            disk_filter_ = RecentDiskFilter::All;
        }
    } else if (layout_mode_ == ProjectLayoutMode::List) {
        for (std::size_t idx = 0; idx < filtered.size(); ++idx) {
            const auto& entry = filtered[idx];
            ImGui::PushID(entry.id.c_str());
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ThemeColors::vec4(0x1F, 0x24, 0x2D));
            ImGui::BeginChild(
                "##RecentRowCard",
                ImVec2(0.0f, std::round(theme.px(94.0f))),
                ImGuiChildFlags_Borders,
                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

            theme.pushBoldFont();
            ImGui::TextUnformatted(entry.project_name.c_str());
            theme.popFont();
            if (entry.pinned) {
                ImGui::SameLine();
                theme.drawTagPill("Pinned", TagTone::Amber);
            }
            ImGui::SameLine();
            theme.drawTagPill(
                entry.exists_on_disk ? "On Disk" : "Missing on Disk",
                entry.exists_on_disk ? TagTone::Emerald : TagTone::Amber);
            ImGui::SameLine();
            theme.drawTagPill(entry.format_label, TagTone::Neutral);

            // Action Buttons on Right
            const float actions_w = theme.px(252.0f);
            if (ImGui::GetWindowWidth() > actions_w + theme.px(220.0f)) {
                ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - actions_w);
            }
            if (theme.styledButton(
                    entry.pinned ? "Unpin##PinBtn" : "Pin##PinBtn",
                    entry.pinned ? ButtonVariant::SolarAmber : ButtonVariant::Ghost,
                    ImVec2(theme.px(68.0f), theme.px(28.0f)),
                    false,
                    HubIcon::Pin)) {
                if (recent_index.setPinned(entry.id, !entry.pinned)) {
                    out_actions.recent_index_mutated = true;
                }
            }
            ImGui::SameLine(0.0f, theme.px(6.0f));
            if (theme.styledButton(
                    "Open##ReopenBtn",
                    ButtonVariant::PrimaryCyan,
                    ImVec2(theme.px(78.0f), theme.px(28.0f)),
                    !entry.exists_on_disk)) {
                out_actions.reopen_project_path = std::filesystem::path(entry.file_path);
            }
            ImGui::SameLine(0.0f, theme.px(6.0f));
            if (theme.styledButton(
                    "Remove##RemoveHistoryBtn",
                    ButtonVariant::Ghost,
                    ImVec2(theme.px(92.0f), theme.px(28.0f)),
                    false,
                    HubIcon::Trash)) {
                if (recent_index.removeById(entry.id)) {
                    out_actions.recent_index_mutated = true;
                }
            }

            theme.pushMonoFont();
            ImGui::TextColored(ThemeColors::vec4(0xA8, 0xB2, 0xC3), "%s", entry.file_path.c_str());
            const std::string stats =
                std::to_string(entry.object_count) + " objects · " +
                std::to_string(entry.mesh_count) + " meshes · " +
                NativeHomeTheme::formatByteSize(entry.byte_size) + " · " +
                NativeHomeTheme::formatTimestampUtc(entry.last_opened_epoch_ms);
            ImGui::TextColored(ThemeColors::vec4(0x88, 0x93, 0xA7), "%s", stats.c_str());
            theme.popFont();

            ImGui::EndChild();
            ImGui::PopStyleColor();
            ImGui::PopID();
        }
    } else {
        // Responsive 2-column or 3-column Grid Mode
        const float inner_w = ImGui::GetContentRegionAvail().x;
        const int gcols = (inner_w >= theme.px(980.0f)) ? 3 : ((inner_w >= theme.px(600.0f)) ? 2 : 1);
        const float ggap = std::round(theme.px(12.0f));
        const float gw = (inner_w - ggap * static_cast<float>(gcols - 1)) / static_cast<float>(gcols);
        const float gh = std::round(theme.px(158.0f));

        for (std::size_t idx = 0; idx < filtered.size(); ++idx) {
            if (gcols > 1 && (idx % static_cast<std::size_t>(gcols)) != 0u) {
                ImGui::SameLine(0.0f, ggap);
            }
            const auto& entry = filtered[idx];
            ImGui::PushID(entry.id.c_str());
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ThemeColors::vec4(0x1F, 0x24, 0x2D));
            ImGui::BeginChild("##RecentGridCard", ImVec2(gw, gh), ImGuiChildFlags_Borders);

            theme.pushBoldFont();
            ImGui::TextUnformatted(entry.project_name.c_str());
            theme.popFont();
            if (entry.pinned) {
                ImGui::SameLine();
                theme.drawTagPill("Pinned", TagTone::Amber);
            }
            ImGui::SameLine();
            theme.drawTagPill(
                entry.exists_on_disk ? "On Disk" : "Missing",
                entry.exists_on_disk ? TagTone::Emerald : TagTone::Amber);

            theme.pushMonoFont();
            ImGui::TextColored(ThemeColors::vec4(0xA8, 0xB2, 0xC3), "%s", entry.file_path.c_str());
            const std::string meta =
                std::to_string(entry.object_count) + " obj · " +
                std::to_string(entry.mesh_count) + " mesh · " +
                NativeHomeTheme::formatByteSize(entry.byte_size);
            ImGui::TextColored(ThemeColors::vec4(0x88, 0x93, 0xA7), "%s", meta.c_str());
            ImGui::TextColored(
                ThemeColors::vec4(0x88, 0x93, 0xA7),
                "%s",
                NativeHomeTheme::formatTimestampUtc(entry.last_opened_epoch_ms).c_str());
            theme.popFont();

            ImGui::Spacing();
            if (theme.styledButton(
                    "Open##GridOpen",
                    ButtonVariant::PrimaryCyan,
                    ImVec2(theme.px(74.0f), theme.px(28.0f)),
                    !entry.exists_on_disk)) {
                out_actions.reopen_project_path = std::filesystem::path(entry.file_path);
            }
            ImGui::SameLine(0.0f, theme.px(6.0f));
            if (theme.styledButton(
                    entry.pinned ? "Unpin##GridPin" : "Pin##GridPin",
                    entry.pinned ? ButtonVariant::SolarAmber : ButtonVariant::Secondary,
                    ImVec2(theme.px(68.0f), theme.px(28.0f)))) {
                if (recent_index.setPinned(entry.id, !entry.pinned)) {
                    out_actions.recent_index_mutated = true;
                }
            }
            ImGui::SameLine(0.0f, theme.px(6.0f));
            if (theme.styledButton(
                    "Remove##GridRem",
                    ButtonVariant::Ghost,
                    ImVec2(theme.px(78.0f), theme.px(28.0f)))) {
                if (recent_index.removeById(entry.id)) {
                    out_actions.recent_index_mutated = true;
                }
            }

            ImGui::EndChild();
            ImGui::PopStyleColor();
            ImGui::PopID();
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

} // namespace finaleffect::ui
