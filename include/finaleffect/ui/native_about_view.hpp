// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include "finaleffect/app/project_hub_service.hpp"
#include "finaleffect/ui/native_file_dialog.hpp"
#include "finaleffect/ui/native_home_theme.hpp"

#include <cstdint>
#include <filesystem>
#include <string>

namespace finaleffect::ui {

struct AboutRuntimeContext {
    app::SystemInfoSummary system_info{};
    std::uint32_t framebuffer_width{0};
    std::uint32_t framebuffer_height{0};
    float content_scale{1.0f};
    std::uint32_t ui_scale_percent{100};
    std::string file_dialog_backend{};
    std::filesystem::path preferences_path{};
    std::filesystem::path recovery_dir{};
};

// Stage 1.1 View G: About & System Information View (Honest Live Telemetry)
class NativeAboutView {
public:
    NativeAboutView() = default;

    void render(
        const NativeHomeTheme& theme,
        const AboutRuntimeContext& ctx);

    [[nodiscard]] const std::string& lastCopiedJson() const noexcept { return last_copied_json_; }

private:
    bool copied_feedback_{false};
    std::string last_copied_json_{};
};

} // namespace finaleffect::ui
