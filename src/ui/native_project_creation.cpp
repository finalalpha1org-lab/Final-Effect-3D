// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/ui/native_project_creation.hpp"

#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>
#include <cctype>
#include <cmath>

namespace finaleffect::ui {

namespace {

std::string trimAscii(std::string_view sv) {
    std::size_t b = 0;
    while (b < sv.size() && std::isspace(static_cast<unsigned char>(sv[b])) != 0) ++b;
    std::size_t e = sv.size();
    while (e > b && std::isspace(static_cast<unsigned char>(sv[e - 1u])) != 0) --e;
    return std::string(sv.substr(b, e - b));
}

std::string slugifyFileName(std::string_view name) {
    std::string slug;
    slug.reserve(name.size());
    bool prev_underscore = false;
    for (const char ch : name) {
        const auto uch = static_cast<unsigned char>(ch);
        if (std::isalnum(uch) != 0) {
            slug.push_back(static_cast<char>(std::tolower(uch)));
            prev_underscore = false;
        } else if (ch == '-' || ch == '_' || ch == ' ' || ch == '.') {
            if (!slug.empty() && !prev_underscore) {
                slug.push_back('_');
                prev_underscore = true;
            }
        }
    }
    while (!slug.empty() && slug.back() == '_') {
        slug.pop_back();
    }
    if (slug.empty()) {
        slug = "project";
    }
    return slug + ".fe3d";
}

bool hasIllegalFileNameChars(std::string_view file_name) noexcept {
    constexpr std::string_view kForbidden = "<>:\"/\\|?*";
    for (const char ch : file_name) {
        if (static_cast<unsigned char>(ch) < 32u || kForbidden.find(ch) != std::string_view::npos) {
            return true;
        }
    }
    return false;
}

float fpsFromIndex(int idx) noexcept {
    switch (idx) {
        case 1: return 25.0f;
        case 2: return 30.0f;
        case 3: return 60.0f;
        default: return 24.0f;
    }
}

} // namespace

void NativeProjectCreation::initializeDefaults(
    const app::AppPreferences& prefs,
    const std::filesystem::path& default_projects_dir) {
    if (destination_dir_.empty()) {
        destination_dir_ = default_projects_dir.generic_string();
    }
    if (!prefs.default_author.empty()) {
        author_ = prefs.default_author;
    }
}

std::filesystem::path NativeProjectCreation::resolvedTargetPath() const {
    const std::string clean_dir = trimAscii(destination_dir_);
    const std::string clean_explicit = trimAscii(explicit_file_name_);
    std::string leaf;
    if (!clean_explicit.empty()) {
        leaf = clean_explicit;
        if (!leaf.ends_with(".fe3d")) {
            leaf += ".fe3d";
        }
    } else {
        leaf = slugifyFileName(trimAscii(project_name_));
    }
    if (clean_dir.empty()) {
        return std::filesystem::path(leaf);
    }
    return (std::filesystem::path(clean_dir) / leaf).lexically_normal();
}

bool NativeProjectCreation::requestSubmit(
    const platform::IFileSystem& file_system,
    ProjectCreationActions& out_actions) {
    error_message_.clear();
    const std::string clean_name = trimAscii(project_name_);
    const std::string clean_dir = trimAscii(destination_dir_);
    const std::string clean_explicit = trimAscii(explicit_file_name_);

    if (clean_name.empty()) {
        error_message_ = "Project Name cannot be empty.";
        return false;
    }
    if (clean_dir.empty()) {
        error_message_ = "Destination Directory cannot be empty.";
        return false;
    }
    if (!clean_explicit.empty() && hasIllegalFileNameChars(clean_explicit)) {
        error_message_ =
            "Explicit File Name contains unsupported path or wildcard characters (< > : \" / \\ | ? *).";
        return false;
    }

    const std::filesystem::path target = resolvedTargetPath();
    if (file_system.exists(target)) {
        overwrite_modal_open_ = true;
        return false;
    }

    out_actions.create_project = NewProjectCreateSpec{
        .project_name = clean_name,
        .author = trimAscii(author_).empty() ? std::string("Final Effect 3D") : trimAscii(author_),
        .description = trimAscii(description_),
        .template_id = template_id_,
        .target_path = target,
        .fps = fpsFromIndex(fps_index_),
        .allow_overwrite = false,
    };
    return true;
}

void NativeProjectCreation::confirmPendingOverwrite(ProjectCreationActions& out_actions) {
    overwrite_modal_open_ = false;
    error_message_.clear();
    const std::string clean_name = trimAscii(project_name_);
    if (clean_name.empty()) {
        error_message_ = "Project Name cannot be empty.";
        return;
    }
    out_actions.create_project = NewProjectCreateSpec{
        .project_name = clean_name,
        .author = trimAscii(author_).empty() ? std::string("Final Effect 3D") : trimAscii(author_),
        .description = trimAscii(description_),
        .template_id = template_id_,
        .target_path = resolvedTargetPath(),
        .fps = fpsFromIndex(fps_index_),
        .allow_overwrite = true,
    };
}

void NativeProjectCreation::render(
    const NativeHomeTheme& theme,
    const platform::IFileSystem& file_system,
    INativeFileDialog& file_dialog,
    ProjectCreationActions& out_actions) {
    theme.drawViewHeader(
        "NEW .FE3D SCENE DOCUMENT",
        "Create New Project",
        "Configure project identity, destination folder, C++20 scene template, and overwrite safety before entering 3D Studio.");

    ImGui::Spacing();
    if (theme.styledButton(
            "Back to Projects##BtnNewBack",
            ButtonVariant::Secondary,
            ImVec2(0, 0),
            false,
            HubIcon::ArrowLeft)) {
        out_actions.navigate_to = HomeHubViewId::ProjectsStart;
    }
    ImGui::SameLine();
    if (theme.styledButton(
            "Create Project & Open Studio##BtnNewSubmitTop",
            ButtonVariant::PrimaryCyan,
            ImVec2(0, 0),
            false,
            HubIcon::Plus)) {
        (void)requestSubmit(file_system, out_actions);
    }

    ImGui::Dummy(ImVec2(0.0f, theme.px(6.0f)));

    if (!error_message_.empty()) {
        theme.drawBanner(error_message_, TagTone::Danger);
    }

    const std::filesystem::path resolved_path = resolvedTargetPath();
    const bool conflict_exists = !trimAscii(destination_dir_).empty() && file_system.exists(resolved_path);
    if (conflict_exists) {
        theme.drawBanner(
            "A file already exists at '" + resolved_path.generic_string() +
                "'. Creating this project will prompt for explicit overwrite confirmation before replacing it.",
            TagTone::Amber);
    }

    // Panel 1: Project Identity & Destination Location
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ThemeColors::vec4(0x19, 0x1D, 0x24));
    ImGui::BeginChild(
        "##NewProjectIdentityPanel",
        ImVec2(0.0f, 0.0f),
        ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);

    theme.pushSubheadingFont();
    ImGui::TextUnformatted("Project Identity & Save Location");
    theme.popFont();
    ImGui::SameLine();
    theme.drawTagPill(file_dialog.backendLabel(), TagTone::Cyan);
    ImGui::Spacing();

    const float avail_w = ImGui::GetContentRegionAvail().x;
    const bool two_col = avail_w >= theme.px(680.0f);
    const float col_gap = std::round(theme.px(16.0f));
    const float col_w = two_col ? ((avail_w - col_gap) * 0.5f) : avail_w;

    ImGui::BeginGroup();
    theme.pushBoldFont();
    ImGui::TextUnformatted("Project Name *");
    theme.popFont();
    ImGui::SetNextItemWidth(col_w);
    if (ImGui::InputTextWithHint("##NewProjNameInput", "e.g. Hero Prop LookDev", &project_name_)) {
        error_message_.clear();
    }
    ImGui::TextColored(ThemeColors::vec4(0x88, 0x93, 0xA7), "Stored in metadata.project_name inside .fe3d.");
    ImGui::EndGroup();

    if (two_col) {
        ImGui::SameLine(0.0f, col_gap);
    } else {
        ImGui::Spacing();
    }

    ImGui::BeginGroup();
    theme.pushBoldFont();
    ImGui::TextUnformatted("Optional Explicit File Name (.fe3d)");
    theme.popFont();
    ImGui::SetNextItemWidth(col_w);
    if (ImGui::InputTextWithHint(
            "##NewProjExplicitFileInput",
            "Auto-derived from Project Name when blank",
            &explicit_file_name_)) {
        error_message_.clear();
    }
    ImGui::TextColored(ThemeColors::vec4(0x88, 0x93, 0xA7), "Automatic .fe3d extension enforced.");
    ImGui::EndGroup();

    ImGui::Spacing();
    theme.pushBoldFont();
    ImGui::TextUnformatted("Destination Directory *");
    theme.popFont();
    const float browse_btn_w = std::round(theme.px(150.0f));
    ImGui::SetNextItemWidth(std::max(theme.px(160.0f), avail_w - browse_btn_w - theme.px(10.0f)));
    if (ImGui::InputText("##NewProjDestDirInput", &destination_dir_)) {
        error_message_.clear();
    }
    ImGui::SameLine(0.0f, theme.px(10.0f));
    if (theme.styledButton(
            "Browse Folder...##BtnBrowseNewDir",
            ButtonVariant::Secondary,
            ImVec2(browse_btn_w, 0.0f),
            false,
            HubIcon::Folder)) {
        const auto res = file_dialog.pickDirectory(
            "Select Destination Folder for New Project",
            std::filesystem::path(destination_dir_));
        if (res.status == FileDialogStatus::Selected && !res.selected_path.empty()) {
            destination_dir_ = res.selected_path.generic_string();
            error_message_.clear();
        } else if (res.status == FileDialogStatus::Unavailable) {
            out_actions.request_in_app_folder_picker = true;
        } else if (res.status == FileDialogStatus::Error) {
            error_message_ = res.error_message;
        }
    }

    theme.pushMonoFont();
    ImGui::TextColored(
        conflict_exists ? ThemeColors::vec4(0xF2, 0x9D, 0x38) : ThemeColors::vec4(0x27, 0xB6, 0xD9),
        "Resolved Target Path: %s",
        resolved_path.generic_string().c_str());
    theme.popFont();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Author, Units, FPS Row
    const float third_w = two_col ? ((avail_w - col_gap * 2.0f) / 3.0f) : avail_w;
    ImGui::BeginGroup();
    theme.pushBoldFont();
    ImGui::TextUnformatted("Author");
    theme.popFont();
    ImGui::SetNextItemWidth(third_w);
    ImGui::InputText("##NewProjAuthorInput", &author_);
    ImGui::EndGroup();

    if (two_col) ImGui::SameLine(0.0f, col_gap);
    ImGui::BeginGroup();
    theme.pushBoldFont();
    ImGui::TextUnformatted("Scene Linear Units");
    theme.popFont();
    ImGui::SetNextItemWidth(third_w);
    const char* unit_items[] = {"Meters (m)", "Centimeters (cm)", "Millimeters (mm)"};
    ImGui::Combo("##NewProjUnitsCombo", &units_index_, unit_items, 3);
    ImGui::EndGroup();

    if (two_col) ImGui::SameLine(0.0f, col_gap);
    ImGui::BeginGroup();
    theme.pushBoldFont();
    ImGui::TextUnformatted("Timeline Frame Rate");
    theme.popFont();
    ImGui::SetNextItemWidth(third_w);
    const char* fps_items[] = {"24 FPS (Film)", "25 FPS (PAL)", "30 FPS (NTSC / Realtime)", "60 FPS (Interactive)"};
    ImGui::Combo("##NewProjFpsCombo", &fps_index_, fps_items, 4);
    ImGui::EndGroup();

    ImGui::Spacing();
    theme.pushBoldFont();
    ImGui::TextUnformatted("Project Description (Optional)");
    theme.popFont();
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##NewProjDescInput", "Optional scene notes stored in metadata.description...", &description_);

    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0.0f, theme.px(10.0f)));

    // Panel 2: Visual C++20 Template Selection
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ThemeColors::vec4(0x19, 0x1D, 0x24));
    ImGui::BeginChild(
        "##NewProjectTemplatePanel",
        ImVec2(0.0f, 0.0f),
        ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);

    theme.pushSubheadingFont();
    ImGui::TextUnformatted("Starting C++20 Scene Template");
    theme.popFont();
    ImGui::Spacing();

    const auto& templates = app::TemplateCatalog::all();
    const float inner_w = ImGui::GetContentRegionAvail().x;
    const int tcols = (inner_w >= theme.px(920.0f)) ? 4 : ((inner_w >= theme.px(560.0f)) ? 2 : 1);
    const float tgap = std::round(theme.px(10.0f));
    const float tw = (inner_w - tgap * static_cast<float>(tcols - 1)) / static_cast<float>(tcols);
    const float th = std::round(theme.px(164.0f));

    for (std::size_t i = 0; i < templates.size(); ++i) {
        if (tcols > 1 && (i % static_cast<std::size_t>(tcols)) != 0u) {
            ImGui::SameLine(0.0f, tgap);
        }
        const auto& t = templates[i];
        const bool chosen = (t.id == template_id_);
        ImGui::PushID(t.slug.data());
        const ImVec2 p0 = ImGui::GetCursorScreenPos();
        const ImVec2 p1(p0.x + tw, p0.y + th);
        if (ImGui::InvisibleButton("##SelectTmplCard", ImVec2(tw, th))) {
            template_id_ = t.id;
        }
        const bool hov = ImGui::IsItemHovered();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const float rad = std::round(theme.px(6.0f));
        dl->AddRectFilled(
            p0,
            p1,
            chosen ? ThemeColors::kBgActive : (hov ? ThemeColors::kBgElevated : ThemeColors::kBgInput),
            rad);
        dl->AddRect(
            p0,
            p1,
            chosen ? ThemeColors::kAccentCyan : (hov ? ThemeColors::kBorderStrong : ThemeColors::kBorderDefault),
            rad,
            0,
            chosen ? 1.8f : 1.0f);

        const float pad = std::round(theme.px(10.0f));
        theme.drawTemplateDiagram(dl, t.id, ImVec2(p0.x + pad, p0.y + pad), ImVec2(p1.x - pad, p0.y + pad + theme.px(82.0f)));
        theme.pushBoldFont();
        dl->AddText(
            ImVec2(p0.x + pad, p0.y + pad + theme.px(90.0f)),
            chosen ? ThemeColors::kAccentCyan : ThemeColors::kTextPrimary,
            t.title.data(),
            t.title.data() + t.title.size());
        theme.popFont();
        theme.pushMonoFont();
        const std::string spec = std::to_string(t.expected_objects) + " obj · " + std::to_string(t.expected_meshes) + " mesh";
        dl->AddText(ImVec2(p0.x + pad, p0.y + pad + theme.px(112.0f)), ThemeColors::kTextSecondary, spec.c_str());
        if (chosen) {
            dl->AddText(ImVec2(p0.x + pad, p0.y + pad + theme.px(129.0f)), ThemeColors::kAccentCyan, "Selected Preset");
        }
        theme.popFont();
        ImGui::PopID();
    }

    ImGui::Spacing();
    if (theme.styledButton(
            "Create Project & Enter 3D Studio##BtnNewSubmitBottom",
            ButtonVariant::PrimaryCyan,
            ImVec2(0, 0),
            false,
            HubIcon::Plus)) {
        (void)requestSubmit(file_system, out_actions);
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();

    // Overwrite Protection Modal
    if (overwrite_modal_open_) {
        const ImVec2 disp = ImGui::GetIO().DisplaySize;
        const float mw = std::min(disp.x - theme.px(40.0f), std::round(theme.px(480.0f)));
        const float mh = std::round(theme.px(210.0f));
        ImGui::SetNextWindowPos(ImVec2((disp.x - mw) * 0.5f, (disp.y - mh) * 0.5f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(mw, mh), ImGuiCond_Always);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ThemeColors::vec4(0x1B, 0x20, 0x29));
        ImGui::PushStyleColor(ImGuiCol_Border, ThemeColors::vec4(0xF2, 0x9D, 0x38));
        if (ImGui::Begin(
                "##OverwriteConfirmModal",
                nullptr,
                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                    ImGuiWindowFlags_NoSavedSettings)) {
            theme.drawTagPill("OVERWRITE PROTECTION", TagTone::Amber);
            ImGui::Spacing();
            theme.pushSubheadingFont();
            ImGui::TextUnformatted("Replace Existing Project File?");
            theme.popFont();
            ImGui::TextWrapped(
                "A file already exists at '%s'. Confirming will overwrite it with the newly initialized C++20 project.",
                resolved_path.generic_string().c_str());
            ImGui::Dummy(ImVec2(0.0f, theme.px(12.0f)));
            if (theme.styledButton("Cancel##CancelOverwriteBtn", ButtonVariant::Secondary)) {
                cancelPendingOverwrite();
            }
            ImGui::SameLine(0.0f, theme.px(10.0f));
            if (theme.styledButton("Overwrite & Create Project##ConfirmOverwriteBtn", ButtonVariant::SolarAmber)) {
                confirmPendingOverwrite(out_actions);
            }
        }
        ImGui::End();
        ImGui::PopStyleColor(2);
    }
}

} // namespace finaleffect::ui
