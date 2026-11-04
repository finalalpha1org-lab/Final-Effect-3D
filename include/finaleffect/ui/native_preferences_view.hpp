// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include "finaleffect/app/app_preferences.hpp"
#include "finaleffect/ui/native_home_theme.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace finaleffect::ui {

struct PreferencesViewActions {
    std::optional<app::AppPreferences> save_preferences{};
    bool reset_to_defaults{false};
};

// Stage 1.1 View F: Application Preferences View (Backed by AppPreferencesStore)
class NativePreferencesView {
public:
    NativePreferencesView() = default;

    void syncFromPreferences(const app::AppPreferences& prefs);

    [[nodiscard]] const app::AppPreferences& draft() const noexcept { return draft_; }
    void setDraftUiScalePercent(std::uint32_t scale_pct) noexcept;
    void setDraftNavigationProfile(app::NavigationProfile profile) noexcept;
    void setDraftDefaultAuthor(std::string author);
    void setDraftAutosaveIntervalSeconds(int seconds) noexcept { draft_interval_input_ = seconds; }

    [[nodiscard]] bool validateAndBuildSave(PreferencesViewActions& out_actions);
    void setStatusFeedback(std::string message, TagTone tone) {
        feedback_message_ = std::move(message);
        feedback_tone_ = tone;
    }
    [[nodiscard]] const std::string& feedbackMessage() const noexcept { return feedback_message_; }

    void render(
        const NativeHomeTheme& theme,
        const app::AppPreferences& committed_prefs,
        const std::filesystem::path& prefs_file_path,
        PreferencesViewActions& out_actions);

private:
    bool initialized_{false};
    app::AppPreferences draft_{};
    int draft_interval_input_{180};
    int draft_recent_limit_input_{20};
    std::string feedback_message_{};
    TagTone feedback_tone_{TagTone::Emerald};
};

} // namespace finaleffect::ui
