// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/ui/native_preferences_view.hpp"

#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>
#include <cctype>
#include <cmath>

namespace finaleffect::ui {

namespace {

std::string trimPrefText(std::string_view sv) {
    std::size_t b = 0;
    while (b < sv.size() && std::isspace(static_cast<unsigned char>(sv[b])) != 0) ++b;
    std::size_t e = sv.size();
    while (e > b && std::isspace(static_cast<unsigned char>(sv[e - 1u])) != 0) --e;
    return std::string(sv.substr(b, e - b));
}

bool preferencesEqual(const app::AppPreferences& a, const app::AppPreferences& b) noexcept {
    return a.ui_scale_percent == b.ui_scale_percent &&
           a.startup_environment == b.startup_environment &&
           a.default_author == b.default_author &&
           a.navigation_profile == b.navigation_profile &&
           a.recent_projects_limit == b.recent_projects_limit &&
           a.autosave_enabled == b.autosave_enabled &&
           a.autosave_interval_seconds == b.autosave_interval_seconds &&
           a.show_viewport_hud == b.show_viewport_hud;
}

} // namespace

void NativePreferencesView::syncFromPreferences(const app::AppPreferences& prefs) {
    draft_ = prefs;
    draft_interval_input_ = static_cast<int>(prefs.autosave_interval_seconds);
    draft_recent_limit_input_ = static_cast<int>(prefs.recent_projects_limit);
    initialized_ = true;
}

void NativePreferencesView::setDraftUiScalePercent(std::uint32_t scale_pct) noexcept {
    draft_.ui_scale_percent = scale_pct;
}

void NativePreferencesView::setDraftNavigationProfile(app::NavigationProfile profile) noexcept {
    draft_.navigation_profile = profile;
}

void NativePreferencesView::setDraftDefaultAuthor(std::string author) {
    draft_.default_author = std::move(author);
}

bool NativePreferencesView::validateAndBuildSave(PreferencesViewActions& out_actions) {
    const std::string clean_author = trimPrefText(draft_.default_author);
    if (clean_author.empty()) {
        feedback_message_ = "Default Project Author cannot be empty.";
        feedback_tone_ = TagTone::Danger;
        return false;
    }
    if (draft_.ui_scale_percent != 85u && draft_.ui_scale_percent != 100u &&
        draft_.ui_scale_percent != 115u && draft_.ui_scale_percent != 125u) {
        feedback_message_ = "Desktop UI Scale must be one of 85%, 100%, 115%, or 125%.";
        feedback_tone_ = TagTone::Danger;
        return false;
    }
    if (draft_interval_input_ < 60 || draft_interval_input_ > 900) {
        feedback_message_ = "Autosave Interval must be between 60 and 900 seconds.";
        feedback_tone_ = TagTone::Danger;
        return false;
    }
    if (draft_recent_limit_input_ < 10 || draft_recent_limit_input_ > 50) {
        feedback_message_ = "Recent Projects History Limit must be between 10 and 50 entries.";
        feedback_tone_ = TagTone::Danger;
        return false;
    }

    app::AppPreferences validated = draft_;
    validated.default_author = clean_author;
    validated.autosave_interval_seconds = static_cast<std::uint32_t>(draft_interval_input_);
    validated.recent_projects_limit = static_cast<std::uint32_t>(draft_recent_limit_input_);
    validated.sanitize();

    draft_ = validated;
    out_actions.save_preferences = validated;
    return true;
}

void NativePreferencesView::render(
    const NativeHomeTheme& theme,
    const app::AppPreferences& committed_prefs,
    const std::filesystem::path& prefs_file_path,
    PreferencesViewActions& out_actions) {
    if (!initialized_) {
        syncFromPreferences(committed_prefs);
    }

    theme.drawViewHeader(
        "APPLICATION CONFIGURATION",
        "Preferences",
        "Desktop application preferences persist independently of .fe3d scene files. Opening external projects never overwrites your UI scale, navigation profile, or autosave settings.");

    const bool has_unsaved_draft =
        !preferencesEqual(draft_, committed_prefs) ||
        draft_interval_input_ != static_cast<int>(committed_prefs.autosave_interval_seconds) ||
        draft_recent_limit_input_ != static_cast<int>(committed_prefs.recent_projects_limit);

    ImGui::Spacing();
    if (theme.styledButton(
            "Save Preferences##BtnSavePrefs",
            ButtonVariant::PrimaryCyan,
            ImVec2(0, 0),
            false,
            HubIcon::Save)) {
        (void)validateAndBuildSave(out_actions);
    }
    ImGui::SameLine();
    if (theme.styledButton("Reset to Defaults##BtnResetPrefs", ButtonVariant::Secondary)) {
        out_actions.reset_to_defaults = true;
    }
    if (has_unsaved_draft) {
        ImGui::SameLine();
        theme.drawTagPill("Modified — Click Save Preferences to Persist", TagTone::Amber);
    }

    ImGui::Dummy(ImVec2(0.0f, theme.px(6.0f)));
    if (!feedback_message_.empty()) {
        theme.drawBanner(feedback_message_, feedback_tone_);
    }

    const float ctrl_w = std::round(theme.px(290.0f));

    // Panel 1: Interface, Appearance & Startup
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ThemeColors::vec4(0x19, 0x1D, 0x24));
    ImGui::BeginChild(
        "##PrefsInterfacePanel",
        ImVec2(0.0f, 0.0f),
        ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);

    theme.pushSubheadingFont();
    ImGui::TextUnformatted("Interface, Appearance & Startup");
    theme.popFont();
    ImGui::SameLine();
    theme.drawTagPill("Obsidian Slate · Prism Cyan · Solar Amber", TagTone::Cyan);
    ImGui::Spacing();

    // UI Scale
    theme.pushBoldFont();
    ImGui::TextUnformatted("Desktop UI Scale");
    theme.popFont();
    ImGui::TextColored(
        ThemeColors::vec4(0x88, 0x93, 0xA7),
        "Scales native typography, panels, cards, and navigation density across standard and HiDPI displays.");
    const std::uint32_t scales[4] = {85u, 100u, 115u, 125u};
    const char* scale_names[4] = {
        "85% (Compact)##Scale85",
        "100% (Default)##Scale100",
        "115% (Comfortable)##Scale115",
        "125% (Large Display)##Scale125"};
    for (int i = 0; i < 4; ++i) {
        if (i > 0) ImGui::SameLine(0.0f, theme.px(8.0f));
        if (theme.styledButton(
                scale_names[i],
                draft_.ui_scale_percent == scales[i] ? ButtonVariant::PrimaryCyan : ButtonVariant::Secondary)) {
            draft_.ui_scale_percent = scales[i];
            // Apply live immediately and persist so scaling feels instant!
            (void)validateAndBuildSave(out_actions);
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Startup Environment
    theme.pushBoldFont();
    ImGui::TextUnformatted("Default Startup Environment");
    theme.popFont();
    ImGui::SetNextItemWidth(ctrl_w);
    int startup_idx = (draft_.startup_environment == app::StartupEnvironment::DefaultScene) ? 1 : 0;
    const char* startup_items[] = {"Project Hub (Home)", "Direct 3D Studio (Starter Scene)"};
    if (ImGui::Combo("##PrefStartupCombo", &startup_idx, startup_items, 2)) {
        draft_.startup_environment = (startup_idx == 1)
            ? app::StartupEnvironment::DefaultScene
            : app::StartupEnvironment::HomeHub;
    }

    ImGui::Spacing();
    ImGui::Checkbox("Show 3D Viewport Heads-Up Display (HUD) inside Studio", &draft_.show_viewport_hud);

    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0.0f, theme.px(10.0f)));

    // Panel 2: Navigation & Authoring Defaults
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ThemeColors::vec4(0x19, 0x1D, 0x24));
    ImGui::BeginChild(
        "##PrefsNavigationPanel",
        ImVec2(0.0f, 0.0f),
        ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);

    theme.pushSubheadingFont();
    ImGui::TextUnformatted("Navigation & Authoring Defaults");
    theme.popFont();
    ImGui::Spacing();

    theme.pushBoldFont();
    ImGui::TextUnformatted("Viewport Navigation Profile");
    theme.popFont();
    ImGui::SetNextItemWidth(ctrl_w + theme.px(90.0f));
    int nav_idx = static_cast<int>(draft_.navigation_profile);
    const char* nav_items[] = {
        "Final Effect Hybrid (MMB + Alt-Mouse)",
        "Blender Compatible (MMB Orbit / Shift+MMB Pan)",
        "Industry Alt-Navigation (Alt+LMB / Alt+MMB / Alt+RMB)",
    };
    if (ImGui::Combo("##PrefNavProfileCombo", &nav_idx, nav_items, 3)) {
        draft_.navigation_profile = static_cast<app::NavigationProfile>(nav_idx);
    }

    ImGui::Spacing();
    theme.pushBoldFont();
    ImGui::TextUnformatted("Default Project Author *");
    theme.popFont();
    ImGui::SetNextItemWidth(ctrl_w);
    ImGui::InputText("##PrefDefaultAuthorInput", &draft_.default_author);

    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0.0f, theme.px(10.0f)));

    // Panel 3: Autosave & Recent History
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ThemeColors::vec4(0x19, 0x1D, 0x24));
    ImGui::BeginChild(
        "##PrefsAutosavePanel",
        ImVec2(0.0f, 0.0f),
        ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);

    theme.pushSubheadingFont();
    ImGui::TextUnformatted("Autosave & Recent History");
    theme.popFont();
    ImGui::Spacing();

    ImGui::Checkbox("Enable Periodic Dirty-Session Recovery Autosave", &draft_.autosave_enabled);
    ImGui::Spacing();

    theme.pushBoldFont();
    ImGui::TextUnformatted("Autosave Interval (Seconds, Valid Range: 60 - 900)");
    theme.popFont();
    ImGui::SetNextItemWidth(theme.px(200.0f));
    ImGui::InputInt("##PrefAutosaveSeconds", &draft_interval_input_, 30, 60);

    ImGui::Spacing();
    theme.pushBoldFont();
    ImGui::TextUnformatted("Recent Projects History Limit (Valid Range: 10 - 50)");
    theme.popFont();
    ImGui::SetNextItemWidth(theme.px(200.0f));
    ImGui::InputInt("##PrefRecentLimit", &draft_recent_limit_input_, 5, 10);

    ImGui::Spacing();
    theme.pushMonoFont();
    ImGui::TextColored(
        ThemeColors::vec4(0x88, 0x93, 0xA7),
        "Persisted Config File: %s",
        prefs_file_path.generic_string().c_str());
    theme.popFont();

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

} // namespace finaleffect::ui
