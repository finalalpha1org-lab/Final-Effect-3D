// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/ui/native_template_browser.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace finaleffect::ui {

void NativeTemplateBrowser::render(
    const NativeHomeTheme& theme,
    TemplateBrowserActions& out_actions) {
    const auto& all_templates = app::TemplateCatalog::all();

    std::vector<std::string> categories;
    categories.emplace_back("All");
    for (const auto& t : all_templates) {
        const std::string cat(t.category);
        if (std::find(categories.begin(), categories.end(), cat) == categories.end()) {
            categories.push_back(cat);
        }
    }

    theme.drawViewHeader(
        "VERIFIED C++20 ENGINE PRESETS",
        "Project Templates",
        "Every preset maps directly to a compiled finaleffect::app::ProjectTemplate initializer in C++20 and produces a calibrated .fe3d v1.3 scene document.");

    ImGui::Spacing();
    // Category Filter Pill Row
    for (std::size_t i = 0; i < categories.size(); ++i) {
        if (i > 0u) {
            ImGui::SameLine(0.0f, theme.px(6.0f));
        }
        const bool active = (categories[i] == category_filter_);
        const std::string lbl =
            (i == 0u ? "All Categories" : categories[i]) + "##CatBtn_" + std::to_string(i);
        if (theme.styledButton(lbl.c_str(), active ? ButtonVariant::PrimaryCyan : ButtonVariant::Secondary)) {
            category_filter_ = categories[i];
        }
    }

    ImGui::Dummy(ImVec2(0.0f, theme.px(8.0f)));

    std::vector<const app::TemplateDescriptor*> visible;
    for (const auto& t : all_templates) {
        if (category_filter_ == "All" || category_filter_ == t.category) {
            visible.push_back(&t);
        }
    }

    const float avail_w = ImGui::GetContentRegionAvail().x;
    const int cols = (avail_w >= theme.px(740.0f)) ? 2 : 1;
    const float gap = std::round(theme.px(14.0f));
    const float card_w = (avail_w - gap * static_cast<float>(cols - 1)) / static_cast<float>(cols);
    const float card_h = std::round(theme.px(320.0f));

    for (std::size_t idx = 0; idx < visible.size(); ++idx) {
        if (cols > 1 && (idx % static_cast<std::size_t>(cols)) != 0u) {
            ImGui::SameLine(0.0f, gap);
        }
        const auto& tmpl = *visible[idx];
        const bool is_selected = (tmpl.id == selected_template_);

        ImGui::PushID(tmpl.slug.data());
        ImGui::PushStyleColor(
            ImGuiCol_ChildBg,
            is_selected ? ThemeColors::vec4(0x1B, 0x24, 0x2F) : ThemeColors::vec4(0x19, 0x1D, 0x24));
        ImGui::PushStyleColor(
            ImGuiCol_Border,
            is_selected ? ThemeColors::vec4(0x27, 0xB6, 0xD9) : ThemeColors::vec4(0x2C, 0x33, 0x40));

        ImGui::BeginChild("##TemplateCard", ImVec2(card_w, card_h), ImGuiChildFlags_Borders);
        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            selected_template_ = tmpl.id;
        }

        // Vector Isometric Wireframe Preview
        const float diag_w = ImGui::GetContentRegionAvail().x;
        const float diag_h = std::round(theme.px(116.0f));
        const ImVec2 d0 = ImGui::GetCursorScreenPos();
        const ImVec2 d1(d0.x + diag_w, d0.y + diag_h);
        ImGui::Dummy(ImVec2(diag_w, diag_h));
        theme.drawTemplateDiagram(ImGui::GetWindowDrawList(), tmpl.id, d0, d1);

        ImGui::Spacing();
        theme.pushSubheadingFont();
        ImGui::TextUnformatted(tmpl.title.data(), tmpl.title.data() + tmpl.title.size());
        theme.popFont();
        ImGui::SameLine();
        theme.drawTagPill(tmpl.category, TagTone::Cyan);

        ImGui::PushStyleColor(ImGuiCol_Text, ThemeColors::vec4(0xA8, 0xB2, 0xC3));
        ImGui::TextWrapped("%.*s", static_cast<int>(tmpl.summary.size()), tmpl.summary.data());
        ImGui::PopStyleColor();

        ImGui::Spacing();
        theme.drawTagPill(std::to_string(tmpl.expected_objects) + " Objects", TagTone::Neutral);
        ImGui::SameLine(0.0f, theme.px(6.0f));
        theme.drawTagPill(std::to_string(tmpl.expected_meshes) + " Editable Meshes", TagTone::Neutral);
        ImGui::SameLine(0.0f, theme.px(6.0f));
        theme.drawTagPill("Right-Handed +Y", TagTone::Neutral);

        // Anchor buttons cleanly near bottom of card
        const float btn_y = card_h - std::round(theme.px(60.0f));
        if (ImGui::GetCursorPosY() < btn_y) {
            ImGui::SetCursorPosY(btn_y);
        }
        ImGui::Separator();
        ImGui::Spacing();
        if (theme.styledButton("Configure Name & Path...##CfgTmplBtn", ButtonVariant::Secondary)) {
            selected_template_ = tmpl.id;
            out_actions.configure_new_with_template = tmpl.id;
        }
        ImGui::SameLine(0.0f, theme.px(8.0f));
        if (theme.styledButton("Create & Enter Studio##QuickTmplBtn", ButtonVariant::PrimaryCyan)) {
            selected_template_ = tmpl.id;
            out_actions.quick_create_template = tmpl.id;
        }

        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopID();
    }
}

} // namespace finaleffect::ui
