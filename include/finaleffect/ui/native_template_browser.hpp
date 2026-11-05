// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include "finaleffect/app/project_hub_service.hpp"
#include "finaleffect/ui/native_home_theme.hpp"

#include <optional>
#include <string>

namespace finaleffect::ui {

struct TemplateBrowserActions {
    std::optional<app::ProjectTemplate> configure_new_with_template{};
    std::optional<app::ProjectTemplate> quick_create_template{};
};

// Stage 1.1 View B: Verified C++20 Project Templates Browser
class NativeTemplateBrowser {
public:
    NativeTemplateBrowser() = default;

    void setCategoryFilter(std::string category) { category_filter_ = std::move(category); }
    [[nodiscard]] const std::string& categoryFilter() const noexcept { return category_filter_; }

    void setSelectedTemplate(app::ProjectTemplate tmpl) noexcept { selected_template_ = tmpl; }
    [[nodiscard]] app::ProjectTemplate selectedTemplate() const noexcept { return selected_template_; }

    void render(
        const NativeHomeTheme& theme,
        TemplateBrowserActions& out_actions);

private:
    std::string category_filter_{"All"};
    app::ProjectTemplate selected_template_{app::ProjectTemplate::StarterShowcase};
};

} // namespace finaleffect::ui
