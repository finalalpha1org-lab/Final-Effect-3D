// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/ui/native_about_view.hpp"

#include <cstdio>

namespace finaleffect::ui {

namespace {

std::string nonEmptyOrUnavailable(const std::string& value) {
    return value.empty() ? std::string("Unavailable") : value;
}

std::string uintOrUnavailable(std::uint32_t value, const char* suffix = "") {
    if (value == 0u) {
        return "Unavailable";
    }
    return std::to_string(value) + suffix;
}

std::string recoveryStateDisplay(app::RecoveryStorageState st) {
    switch (st) {
        case app::RecoveryStorageState::Available:
            return "Available (Persistent Ring Buffer)";
        case app::RecoveryStorageState::Unavailable:
            return "Unavailable";
        case app::RecoveryStorageState::Uninitialized:
            return "Uninitialized";
    }
    return "Unavailable";
}

} // namespace

void NativeAboutView::render(
    const NativeHomeTheme& theme,
    const AboutRuntimeContext& ctx) {
    const auto& info = ctx.system_info;

    theme.drawViewHeader(
        "DIAGNOSTICS & LIVE RUNTIME TELEMETRY",
        "About & System Information",
        "Live engine, OpenGL/GPU device, project serializer, and desktop persistence telemetry queried directly from the C++20 runtime. Missing hardware fields report 'Unavailable' honestly.");

    ImGui::Spacing();
    if (theme.styledButton(
            copied_feedback_
                ? "Copied Diagnostic JSON##BtnCopyDiag"
                : "Copy Diagnostic Report JSON##BtnCopyDiag",
            ButtonVariant::PrimaryCyan)) {
        if (auto json = app::serializeSystemInfoJson(info, true); json) {
            last_copied_json_ = json.value();
            ImGui::SetClipboardText(last_copied_json_.c_str());
            copied_feedback_ = true;
        }
    }

    ImGui::Dummy(ImVec2(0.0f, theme.px(6.0f)));
    if (copied_feedback_) {
        theme.drawBanner("Diagnostic JSON report copied to the system clipboard.", TagTone::Emerald);
    }

    auto renderKvSection = [&](const char* id,
                               const char* heading,
                               const std::string& badge_text,
                               TagTone badge_tone,
                               const auto& populate_fn) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ThemeColors::vec4(0x19, 0x1D, 0x24));
        ImGui::BeginChild(id, ImVec2(0.0f, 0.0f), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);
        theme.pushSubheadingFont();
        ImGui::TextUnformatted(heading);
        theme.popFont();
        if (!badge_text.empty()) {
            ImGui::SameLine();
            theme.drawTagPill(badge_text, badge_tone);
        }
        ImGui::Spacing();

        if (ImGui::BeginTable(id, 2, ImGuiTableFlags_SizingStretchSame)) {
            populate_fn();
            ImGui::EndTable();
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::Dummy(ImVec2(0.0f, theme.px(10.0f)));
    };

    auto kvPair = [&](const char* key, const std::string& val) {
        ImGui::TableNextColumn();
        ImGui::TextColored(ThemeColors::vec4(0x88, 0x93, 0xA7), "%s", key);
        theme.pushMonoFont();
        const bool is_unavail = (val == "Unavailable");
        ImGui::TextColored(
            is_unavail ? ThemeColors::vec4(0xF2, 0x9D, 0x38) : ThemeColors::vec4(0xEE, 0xF2, 0xF7),
            "%s",
            val.c_str());
        theme.popFont();
    };

    // Panel 1: Application & Engine Build
    std::string engine_badge = "v";
    engine_badge += nonEmptyOrUnavailable(info.engine_version);
    renderKvSection(
        "##AboutAppBuildPanel",
        "Application & C++20 Engine Build",
        engine_badge,
        TagTone::Cyan,
        [&] {
            ImGui::TableNextRow();
            kvPair("Application Name", nonEmptyOrUnavailable(info.application_name));
            kvPair("C++20 Core Engine Version", nonEmptyOrUnavailable(info.engine_version));
            ImGui::TableNextRow();
            kvPair("Build Configuration", nonEmptyOrUnavailable(info.build_configuration));
            kvPair("Host Operating System", nonEmptyOrUnavailable(info.platform_name));
            ImGui::TableNextRow();
            kvPair("Logical CPU Threads", uintOrUnavailable(info.logical_cpu_threads, " threads"));
            kvPair("Coordinate System", nonEmptyOrUnavailable(info.coordinate_system));
            ImGui::TableNextRow();
            kvPair("Open-Source License", nonEmptyOrUnavailable(info.license));
            char fb_buf[96]{};
            std::snprintf(
                fb_buf,
                sizeof(fb_buf),
                "%ux%u px (DPI %.2fx · UI %u%%)",
                ctx.framebuffer_width,
                ctx.framebuffer_height,
                static_cast<double>(ctx.content_scale),
                ctx.ui_scale_percent);
            kvPair("Active Framebuffer & Scale", std::string(fb_buf));
        });

    // Panel 2: Graphics API & GPU Device
    renderKvSection(
        "##AboutGpuPanel",
        "Graphics API & GPU Device",
        nonEmptyOrUnavailable(info.graphics_backend),
        TagTone::Emerald,
        [&] {
            ImGui::TableNextRow();
            kvPair("Graphics Backend", nonEmptyOrUnavailable(info.graphics_backend));
            kvPair("OpenGL / API Version", nonEmptyOrUnavailable(info.graphics_api_version));
            ImGui::TableNextRow();
            kvPair("Shading Language (GLSL)", nonEmptyOrUnavailable(info.shading_language_version));
            kvPair("GPU Vendor", nonEmptyOrUnavailable(info.graphics_vendor));
            ImGui::TableNextRow();
            kvPair("GPU Renderer", nonEmptyOrUnavailable(info.graphics_device));
            kvPair("Max 2D Texture Dimension", uintOrUnavailable(info.max_texture_dimension, " px"));
            ImGui::TableNextRow();
            kvPair("Max Viewport Dimension", uintOrUnavailable(info.max_viewport_dimension, " px"));
            kvPair("Native UI Layer", "Dear ImGui v1.91.8 · Obsidian Slate Theme");
        });

    // Panel 3: Project Format & Persistence Paths
    renderKvSection(
        "##AboutPersistencePanel",
        "Project Format & Desktop Persistence",
        ".fe3d + .fe3dbin",
        TagTone::Neutral,
        [&] {
            ImGui::TableNextRow();
            kvPair("Supported Project Formats", nonEmptyOrUnavailable(info.project_format));
            kvPair("Native File Dialog Backend", nonEmptyOrUnavailable(ctx.file_dialog_backend));
            ImGui::TableNextRow();
            kvPair("Preferences File Path", nonEmptyOrUnavailable(ctx.preferences_path.generic_string()));
            kvPair("Recovery Storage Status", recoveryStateDisplay(info.recovery_storage_state));
            ImGui::TableNextRow();
            kvPair("Recovery Ring Buffer Path", nonEmptyOrUnavailable(ctx.recovery_dir.generic_string()));
            kvPair("Cross-Platform Compatibility", "Native Desktop C++20 <-> Desktop Web (.fe3d v1.0-v1.3)");
        });
}

} // namespace finaleffect::ui
