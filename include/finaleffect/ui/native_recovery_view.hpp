// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include "finaleffect/app/app_preferences.hpp"
#include "finaleffect/app/project_hub_service.hpp"
#include "finaleffect/ui/native_home_theme.hpp"

#include <optional>
#include <string>

namespace finaleffect::ui {

struct RecoveryViewActions {
    std::optional<std::string> restore_snapshot_id{};
    std::optional<std::string> discard_snapshot_id{};
    bool capture_manual_checkpoint{false};
    bool clear_all_snapshots{false};
};

// Stage 1.1 View E: Recovery & Autosave View (Zero Fake Checkpoints)
class NativeRecoveryView {
public:
    NativeRecoveryView() = default;

    void setStatusBanner(std::string message, TagTone tone) {
        banner_message_ = std::move(message);
        banner_tone_ = tone;
    }
    void clearBanner() noexcept { banner_message_.clear(); }
    [[nodiscard]] const std::string& bannerMessage() const noexcept { return banner_message_; }

    void requestClearAllConfirmation() noexcept { confirm_clear_all_open_ = true; }
    [[nodiscard]] bool isClearAllConfirmOpen() const noexcept { return confirm_clear_all_open_; }
    void confirmClearAll(RecoveryViewActions& out_actions) noexcept {
        confirm_clear_all_open_ = false;
        out_actions.clear_all_snapshots = true;
    }
    void cancelClearAll() noexcept { confirm_clear_all_open_ = false; }

    void render(
        const NativeHomeTheme& theme,
        const app::RecoveryService& recovery_service,
        const app::AppPreferences& prefs,
        RecoveryViewActions& out_actions);

private:
    std::string banner_message_{};
    TagTone banner_tone_{TagTone::Neutral};
    bool confirm_clear_all_open_{false};
};

} // namespace finaleffect::ui
