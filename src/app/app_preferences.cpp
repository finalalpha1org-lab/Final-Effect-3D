// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/app/app_preferences.hpp"
#include "finaleffect/project/json.hpp"

#include <algorithm>
#include <cmath>

namespace finaleffect::app {

namespace {

std::uint32_t snapUiScalePercent(std::uint32_t raw) noexcept {
    constexpr std::uint32_t kAllowed[] = {85u, 100u, 115u, 125u};
    std::uint32_t best = 100u;
    std::uint32_t best_diff = 1000u;
    for (const std::uint32_t candidate : kAllowed) {
        const std::uint32_t diff = (raw > candidate) ? (raw - candidate) : (candidate - raw);
        if (diff < best_diff) {
            best_diff = diff;
            best = candidate;
        }
    }
    return best;
}

std::string trimAsciiWhitespace(std::string_view sv) {
    std::size_t first = 0;
    while (first < sv.size() && (sv[first] == ' ' || sv[first] == '\t' || sv[first] == '\r' || sv[first] == '\n')) {
        ++first;
    }
    std::size_t last = sv.size();
    while (last > first && (sv[last - 1] == ' ' || sv[last - 1] == '\t' || sv[last - 1] == '\r' || sv[last - 1] == '\n')) {
        --last;
    }
    return std::string(sv.substr(first, last - first));
}

} // namespace

std::string_view navigationProfileSlug(NavigationProfile profile) noexcept {
    switch (profile) {
        case NavigationProfile::Blender: return "blender";
        case NavigationProfile::Maya:    return "maya";
        case NavigationProfile::Fe3dHybrid:
        default:
            return "fe3d_hybrid";
    }
}

std::optional<NavigationProfile> parseNavigationProfileSlug(std::string_view slug) noexcept {
    if (slug == "fe3d_hybrid" || slug == "hybrid") return NavigationProfile::Fe3dHybrid;
    if (slug == "blender") return NavigationProfile::Blender;
    if (slug == "maya") return NavigationProfile::Maya;
    return std::nullopt;
}

std::string_view startupEnvironmentSlug(StartupEnvironment env) noexcept {
    switch (env) {
        case StartupEnvironment::DefaultScene: return "default_scene";
        case StartupEnvironment::HomeHub:
        default:
            return "home_hub";
    }
}

std::optional<StartupEnvironment> parseStartupEnvironmentSlug(std::string_view slug) noexcept {
    if (slug == "home_hub" || slug == "home" || slug == "hub") return StartupEnvironment::HomeHub;
    if (slug == "default_scene" || slug == "studio") return StartupEnvironment::DefaultScene;
    return std::nullopt;
}

void AppPreferences::sanitize() noexcept {
    ui_scale_percent = snapUiScalePercent(ui_scale_percent);
    recent_projects_limit = std::clamp<std::uint32_t>(recent_projects_limit, 5u, 50u);
    autosave_interval_seconds = std::clamp<std::uint32_t>(autosave_interval_seconds, 30u, 900u);
    default_author = trimAsciiWhitespace(default_author);
    if (default_author.empty()) {
        default_author = "Final Effect 3D";
    }
    if (default_author.size() > 128u) {
        default_author.resize(128u);
    }

    std::vector<std::pair<std::string, std::string>> cleaned;
    cleaned.reserve(keymap_overrides.size());
    for (const auto& [action_id, combo] : keymap_overrides) {
        const std::string k = trimAsciiWhitespace(action_id);
        const std::string v = trimAsciiWhitespace(combo);
        if (k.empty() || v.empty()) {
            continue;
        }
        auto existing = std::find_if(cleaned.begin(), cleaned.end(), [&](const auto& item) {
            return item.first == k;
        });
        if (existing != cleaned.end()) {
            existing->second = v;
        } else {
            cleaned.emplace_back(k, v);
        }
    }
    std::sort(cleaned.begin(), cleaned.end(), [](const auto& a, const auto& b) {
        return a.first < b.first;
    });
    keymap_overrides = std::move(cleaned);
}

core::Result<std::string> AppPreferencesStore::serializeToString(
    const AppPreferences& prefs_in,
    bool pretty) {
    AppPreferences prefs = prefs_in;
    prefs.sanitize();

    project::JsonValue root{project::JsonObject{}};
    root.setKey("format", std::string(kFormatIdentifier));
    root.setKey("version", static_cast<std::uint64_t>(kSchemaVersion));
    root.setKey("ui_scale_percent", static_cast<std::uint64_t>(prefs.ui_scale_percent));
    root.setKey("startup_environment", std::string(startupEnvironmentSlug(prefs.startup_environment)));
    root.setKey("default_author", prefs.default_author);
    root.setKey("navigation_profile", std::string(navigationProfileSlug(prefs.navigation_profile)));
    root.setKey("recent_projects_limit", static_cast<std::uint64_t>(prefs.recent_projects_limit));
    root.setKey("autosave_enabled", prefs.autosave_enabled);
    root.setKey("autosave_interval_seconds", static_cast<std::uint64_t>(prefs.autosave_interval_seconds));
    root.setKey("show_viewport_hud", prefs.show_viewport_hud);

    project::JsonValue km{project::JsonObject{}};
    for (const auto& [action_id, combo] : prefs.keymap_overrides) {
        km.setKey(action_id, combo);
    }
    root.setKey("keymap_overrides", std::move(km));

    return project::writeJson(root, pretty);
}

core::Result<AppPreferences> AppPreferencesStore::deserializeFromString(std::string_view json_text) {
    auto parsed = project::parseJson(json_text);
    if (!parsed) {
        return parsed.error();
    }
    const auto& root = parsed.value();
    if (!root.isObject()) {
        return core::Result<AppPreferences>::fail(
            core::ErrorCode::ParseError,
            "Preferences JSON root must be an object");
    }

    if (const auto* fmt = root.findKey("format"); fmt != nullptr) {
        if (!fmt->isString() || fmt->asString() != kFormatIdentifier) {
            return core::Result<AppPreferences>::fail(
                core::ErrorCode::ValidationError,
                "Unrecognized preferences format identifier");
        }
    }

    AppPreferences prefs{};
    if (const auto* v = root.findKey("ui_scale_percent"); v != nullptr && v->isNumber() && std::isfinite(v->asNumber())) {
        const double num = std::max(50.0, std::min(200.0, v->asNumber()));
        prefs.ui_scale_percent = static_cast<std::uint32_t>(std::lround(num));
    }
    if (const auto* v = root.findKey("startup_environment"); v != nullptr && v->isString()) {
        if (const auto parsed_env = parseStartupEnvironmentSlug(v->asString()); parsed_env.has_value()) {
            prefs.startup_environment = parsed_env.value();
        }
    }
    if (const auto* v = root.findKey("default_author"); v != nullptr && v->isString()) {
        prefs.default_author = v->asString();
    }
    if (const auto* v = root.findKey("navigation_profile"); v != nullptr && v->isString()) {
        if (const auto parsed_nav = parseNavigationProfileSlug(v->asString()); parsed_nav.has_value()) {
            prefs.navigation_profile = parsed_nav.value();
        }
    }
    if (const auto* v = root.findKey("recent_projects_limit"); v != nullptr && v->isNumber() && std::isfinite(v->asNumber())) {
        const double num = std::max(1.0, std::min(200.0, v->asNumber()));
        prefs.recent_projects_limit = static_cast<std::uint32_t>(std::lround(num));
    }
    if (const auto* v = root.findKey("autosave_enabled"); v != nullptr && v->isBool()) {
        prefs.autosave_enabled = v->asBool();
    }
    if (const auto* v = root.findKey("autosave_interval_seconds"); v != nullptr && v->isNumber() && std::isfinite(v->asNumber())) {
        const double num = std::max(10.0, std::min(3600.0, v->asNumber()));
        prefs.autosave_interval_seconds = static_cast<std::uint32_t>(std::lround(num));
    }
    if (const auto* v = root.findKey("show_viewport_hud"); v != nullptr && v->isBool()) {
        prefs.show_viewport_hud = v->asBool();
    }
    if (const auto* km = root.findKey("keymap_overrides"); km != nullptr && km->isObject()) {
        for (const auto& [action_id, combo_val] : km->asObject()) {
            if (combo_val.isString()) {
                prefs.keymap_overrides.emplace_back(action_id, combo_val.asString());
            }
        }
    }

    prefs.sanitize();
    return core::Result<AppPreferences>::ok(std::move(prefs));
}

core::Result<AppPreferences> AppPreferencesStore::loadOrDefault(
    const std::filesystem::path& path,
    const platform::IFileSystem& file_system) {
    if (path.empty() || !file_system.exists(path)) {
        return core::Result<AppPreferences>::ok(AppPreferences{});
    }
    auto text_res = file_system.readTextFile(path);
    if (!text_res) {
        return text_res.error();
    }
    return deserializeFromString(text_res.value());
}

core::Status AppPreferencesStore::saveToFile(
    const AppPreferences& prefs,
    const std::filesystem::path& path,
    platform::IFileSystem& file_system) {
    if (path.empty()) {
        return core::Status::fail(core::ErrorCode::InvalidArgument, "Preferences file path cannot be empty");
    }
    auto serialized = serializeToString(prefs, true);
    if (!serialized) {
        return serialized.error();
    }
    return file_system.writeTextFile(path, serialized.value());
}

} // namespace finaleffect::app
