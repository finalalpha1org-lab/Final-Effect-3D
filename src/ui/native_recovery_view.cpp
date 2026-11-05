// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/ui/native_recovery_view.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace finaleffect::ui {

void NativeRecoveryView::render(
    const NativeHomeTheme& theme,
    const app::RecoveryService& recovery_service,
    const app::AppPreferences& prefs,
    RecoveryViewActions& out_actions) {
    const bool available = (recovery_service.storageState() == app::RecoveryStorageState::Available);
    const auto& raw_snaps = recovery_service.snapshots();

    // Present newest checkpoints first
    std::vector<app::RecoverySnapshotRecord> ordered = raw_snaps;
    std::sort(
        ordered.begin(),
        ordered.end(),
        [](const app::RecoverySnapshotRecord& a, const app::RecoverySnapshotRecord& b) {
            return a.captured_epoch_ms > b.captured_epoch_ms;
        });

    theme.drawViewHeader(
        "CRASH PROTECTION & AUTOSAVE RING BUFFER",
        "Recovery & Autosave",
        "Genuine .fe3d recovery checkpoints captured in a bounded disk ring buffer when unsaved Studio edits occur.");

    ImGui::Spacing();
    if (available) {
        if (theme.styledButton(
                "Capture Current Session Checkpoint##BtnManualRecovery",
                ButtonVariant::Secondary,
                ImVec2(0, 0),
                false,
                HubIcon::Save)) {
            out_actions.capture_manual_checkpoint = true;
        }
        if (!ordered.empty()) {
            ImGui::SameLine();
            if (theme.styledButton(
                    "Restore Latest Checkpoint##BtnRestoreLatest",
                    ButtonVariant::PrimaryCyan,
                    ImVec2(0, 0),
                    false,
                    HubIcon::Recovery)) {
                out_actions.restore_snapshot_id = std::string("latest");
            }
            ImGui::SameLine();
            if (theme.styledButton(
                    "Clear All Checkpoints...##BtnClearAllRec",
                    ButtonVariant::Danger,
                    ImVec2(0, 0),
                    false,
                    HubIcon::Trash)) {
                confirm_clear_all_open_ = true;
            }
        }
    }

    ImGui::Dummy(ImVec2(0.0f, theme.px(6.0f)));
    if (!banner_message_.empty()) {
        theme.drawBanner(banner_message_, banner_tone_);
    }

    if (recovery_service.hadUncleanPreviousSession() && !ordered.empty()) {
        theme.drawBanner(
            "Previous Studio session exited without a clean shutdown marker. " +
                std::to_string(ordered.size()) + " genuine recovery checkpoint(s) can be restored below.",
            TagTone::Amber);
    }

    // Subsystem Status Panel
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ThemeColors::vec4(0x19, 0x1D, 0x24));
    ImGui::BeginChild(
        "##RecoverySubsystemStatusPanel",
        ImVec2(0.0f, 0.0f),
        ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);

    theme.pushSubheadingFont();
    ImGui::TextUnformatted("Autosave Subsystem Status");
    theme.popFont();
    ImGui::SameLine();
    theme.drawTagPill(
        available ? "Storage Available" : "Storage Unavailable",
        available ? TagTone::Emerald : TagTone::Amber);
    ImGui::Spacing();

    if (ImGui::BeginTable("##RecoveryStatusKV", 2, ImGuiTableFlags_SizingStretchSame)) {
        auto kvCell = [&](const char* key, const std::string& val) {
            ImGui::TableNextColumn();
            ImGui::TextColored(ThemeColors::vec4(0x88, 0x93, 0xA7), "%s", key);
            theme.pushMonoFont();
            ImGui::TextUnformatted(val.c_str());
            theme.popFont();
        };
        ImGui::TableNextRow();
        kvCell("Periodic Autosave", prefs.autosave_enabled ? "Enabled" : "Disabled in Preferences");
        kvCell("Autosave Interval", std::to_string(prefs.autosave_interval_seconds) + "s (dirty-session gated)");
        ImGui::TableNextRow();
        kvCell(
            "Ring Buffer Occupancy",
            std::to_string(ordered.size()) + " / " + std::to_string(app::RecoveryService::kMaxRingSlots) + " slots");
        kvCell(
            "Previous Session Exit",
            recovery_service.hadUncleanPreviousSession() ? "Unclean termination detected" : "Clean");
        ImGui::EndTable();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0.0f, theme.px(10.0f)));

    // Honest Unavailable vs Empty vs Populated States
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ThemeColors::vec4(0x19, 0x1D, 0x24));
    ImGui::BeginChild(
        "##RecoverySnapshotsBody",
        ImVec2(0.0f, 0.0f),
        ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);

    if (!available) {
        theme.drawTagPill("CAPABILITY UNAVAILABLE", TagTone::Amber);
        ImGui::Spacing();
        theme.pushSubheadingFont();
        ImGui::TextUnformatted("Persistent Recovery Storage Is Unavailable");
        theme.popFont();
        const std::string reason = recovery_service.unavailableReason().empty()
            ? std::string("Recovery directory could not be initialized or is read-only.")
            : recovery_service.unavailableReason();
        ImGui::TextColored(ThemeColors::vec4(0xA8, 0xB2, 0xC3), "%s", reason.c_str());
        ImGui::TextColored(
            ThemeColors::vec4(0x88, 0x93, 0xA7),
            "Save scenes explicitly to .fe3d files on disk to preserve work.");
    } else if (ordered.empty()) {
        theme.drawTagPill("SUBSYSTEM READY · 0 CHECKPOINTS", TagTone::Emerald);
        ImGui::Spacing();
        theme.pushSubheadingFont();
        ImGui::TextUnformatted("No Recoverable Checkpoints Recorded");
        theme.popFont();
        ImGui::TextColored(
            ThemeColors::vec4(0xA8, 0xB2, 0xC3),
            "Recovery storage is active and healthy, and there are currently zero unsaved crash or autosave checkpoints waiting for recovery.");
    } else {
        theme.pushSubheadingFont();
        ImGui::TextUnformatted("Recorded Recovery Checkpoints");
        theme.popFont();
        ImGui::Spacing();

        for (std::size_t idx = 0; idx < ordered.size(); ++idx) {
            const auto& snap = ordered[idx];
            ImGui::PushID(snap.id.c_str());
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ThemeColors::vec4(0x1F, 0x24, 0x2D));
            ImGui::BeginChild("##RecoveryCardRow", ImVec2(0.0f, std::round(theme.px(88.0f))), ImGuiChildFlags_Borders);

            theme.pushBoldFont();
            ImGui::TextUnformatted(snap.project_name.empty() ? "Untitled Project" : snap.project_name.c_str());
            theme.popFont();
            ImGui::SameLine();
            theme.drawTagPill("Slot #" + std::to_string(snap.slot_index), TagTone::Cyan);
            if (idx == 0u) {
                ImGui::SameLine();
                theme.drawTagPill("Latest", TagTone::Emerald);
            }
            if (snap.unclean_session) {
                ImGui::SameLine();
                theme.drawTagPill("Unclean Exit", TagTone::Amber);
            }

            const float btns_w = theme.px(250.0f);
            if (ImGui::GetWindowWidth() > btns_w + theme.px(220.0f)) {
                ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - btns_w);
            }
            if (theme.styledButton(
                    "Restore into Studio##BtnRestoreSnap",
                    ButtonVariant::PrimaryCyan,
                    ImVec2(theme.px(156.0f), theme.px(28.0f)))) {
                out_actions.restore_snapshot_id = snap.id;
            }
            ImGui::SameLine(0.0f, theme.px(6.0f));
            if (theme.styledButton(
                    "Discard##BtnDiscardSnap",
                    ButtonVariant::Danger,
                    ImVec2(theme.px(86.0f), theme.px(28.0f)))) {
                out_actions.discard_snapshot_id = snap.id;
            }

            theme.pushMonoFont();
            const std::string detail =
                snap.reason + " · " + std::to_string(snap.object_count) + " objects · " +
                NativeHomeTheme::formatByteSize(snap.byte_size) + " · " +
                NativeHomeTheme::formatTimestampUtc(snap.captured_epoch_ms);
            ImGui::TextColored(ThemeColors::vec4(0xA8, 0xB2, 0xC3), "%s", detail.c_str());
            const std::string orig =
                snap.origin_path.empty() ? std::string("Origin: Unsaved in-memory Studio document") : ("Origin: " + snap.origin_path);
            ImGui::TextColored(ThemeColors::vec4(0x88, 0x93, 0xA7), "%s", orig.c_str());
            theme.popFont();

            ImGui::EndChild();
            ImGui::PopStyleColor();
            ImGui::PopID();
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();

    // Clear All Recovery Checkpoints Confirmation Modal
    if (confirm_clear_all_open_) {
        const ImVec2 disp = ImGui::GetIO().DisplaySize;
        const float mw = std::min(disp.x - theme.px(40.0f), std::round(theme.px(460.0f)));
        const float mh = std::round(theme.px(200.0f));
        ImGui::SetNextWindowPos(ImVec2((disp.x - mw) * 0.5f, (disp.y - mh) * 0.5f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(mw, mh), ImGuiCond_Always);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ThemeColors::vec4(0x1B, 0x20, 0x29));
        ImGui::PushStyleColor(ImGuiCol_Border, ThemeColors::vec4(0xE5, 0x53, 0x53));
        if (ImGui::Begin(
                "##ConfirmClearRecoveryModal",
                nullptr,
                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                    ImGuiWindowFlags_NoSavedSettings)) {
            theme.drawTagPill("CONFIRM DESTRUCTIVE ACTION", TagTone::Danger);
            ImGui::Spacing();
            theme.pushSubheadingFont();
            ImGui::TextUnformatted("Clear All Recovery Checkpoints?");
            theme.popFont();
            ImGui::TextWrapped(
                "This permanently deletes all %zu autosave recovery snapshot(s) from the recovery ring buffer. Saved .fe3d project files on disk are not affected.",
                ordered.size());
            ImGui::Dummy(ImVec2(0.0f, theme.px(12.0f)));
            if (theme.styledButton("Cancel##CancelClearRec", ButtonVariant::Secondary)) {
                cancelClearAll();
            }
            ImGui::SameLine(0.0f, theme.px(10.0f));
            if (theme.styledButton("Clear All Checkpoints##ConfirmClearRec", ButtonVariant::Danger)) {
                confirmClearAll(out_actions);
            }
        }
        ImGui::End();
        ImGui::PopStyleColor(2);
    }
}

} // namespace finaleffect::ui
