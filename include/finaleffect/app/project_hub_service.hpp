// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include "finaleffect/app/app_preferences.hpp"
#include "finaleffect/app/editor_controller.hpp"
#include "finaleffect/core/result.hpp"
#include "finaleffect/platform/file_system.hpp"
#include "finaleffect/project/project.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace finaleffect::app {

struct TemplateDescriptor {
    ProjectTemplate id{ProjectTemplate::StarterShowcase};
    std::string_view slug;
    std::string_view title;
    std::string_view category;
    std::string_view summary;
    std::uint32_t expected_objects{0};
    std::uint32_t expected_meshes{0};
};

class TemplateCatalog {
public:
    [[nodiscard]] static const std::vector<TemplateDescriptor>& all() noexcept;
    [[nodiscard]] static std::optional<ProjectTemplate> parseSlug(std::string_view slug) noexcept;
    [[nodiscard]] static std::string_view canonicalSlug(ProjectTemplate tmpl) noexcept;
    [[nodiscard]] static const TemplateDescriptor& describe(ProjectTemplate tmpl) noexcept;
};

enum class RecentSortMode : std::uint8_t {
    LastOpenedDesc = 0,
    NameAsc = 1,
    ByteSizeDesc = 2,
};

struct RecentProjectEntry {
    std::string id;
    std::string project_name;
    std::string author;
    std::string scene_name;
    std::string file_path;
    std::string format_label{".fe3d v1.3"};
    std::uint64_t byte_size{0};
    std::uint32_t object_count{0};
    std::uint32_t mesh_count{0};
    std::uint32_t material_count{0};
    std::uint64_t last_opened_epoch_ms{0};
    bool pinned{false};
    bool exists_on_disk{true};
};

class RecentProjectsIndex {
public:
    static constexpr std::string_view kFormatIdentifier = "finaleffect.recent_projects";

    [[nodiscard]] core::Status loadFromFile(
        const std::filesystem::path& index_path,
        const platform::IFileSystem& file_system);

    [[nodiscard]] core::Status saveToFile(
        const std::filesystem::path& index_path,
        platform::IFileSystem& file_system) const;

    void refreshDiskStatus(const platform::IFileSystem& file_system);

    RecentProjectEntry recordProject(
        const project::ProjectDocument& doc,
        const std::filesystem::path& project_path,
        std::uint64_t byte_size,
        std::uint64_t timestamp_epoch_ms,
        std::uint32_t max_unpinned_entries = 20);

    [[nodiscard]] std::vector<RecentProjectEntry> query(
        std::string_view search_filter = {},
        RecentSortMode sort_mode = RecentSortMode::LastOpenedDesc,
        bool pinned_only = false) const;

    bool setPinned(std::string_view id, bool pinned);
    bool removeById(std::string_view id);
    std::size_t clearUnpinned();
    void enforceLimit(std::uint32_t max_unpinned_entries);

    [[nodiscard]] const std::vector<RecentProjectEntry>& entries() const noexcept { return entries_; }

private:
    std::vector<RecentProjectEntry> entries_{};
};

enum class RecoveryStorageState : std::uint8_t {
    Uninitialized = 0,
    Available = 1,
    Unavailable = 2,
};

struct RecoverySnapshotRecord {
    std::string id;
    std::uint32_t slot_index{0};
    std::string project_name;
    std::string author;
    std::string origin_path;
    std::string snapshot_file_name;
    std::uint64_t captured_epoch_ms{0};
    std::uint64_t byte_size{0};
    std::uint32_t object_count{0};
    std::string reason{"Autosave"};
    bool unclean_session{false};
};

// Genuine Stage 1 Recovery & Autosave manager backed by real .fe3d ProjectSerializer snapshots.
// Never invents fake records and explicitly distinguishes Unavailable storage from an Empty list.
class RecoveryService {
public:
    static constexpr std::uint32_t kMaxRingSlots = 3;

    [[nodiscard]] core::Status initialize(
        const std::filesystem::path& recovery_dir,
        platform::IFileSystem& file_system);

    [[nodiscard]] RecoveryStorageState storageState() const noexcept { return storage_state_; }
    [[nodiscard]] const std::string& unavailableReason() const noexcept { return unavailable_reason_; }
    [[nodiscard]] bool hadUncleanPreviousSession() const noexcept { return had_unclean_session_; }

    [[nodiscard]] const std::vector<RecoverySnapshotRecord>& snapshots() const noexcept {
        return snapshots_;
    }

    [[nodiscard]] core::Result<RecoverySnapshotRecord> captureSnapshot(
        const project::ProjectDocument& document,
        std::string_view origin_path,
        std::string_view reason,
        std::uint64_t timestamp_epoch_ms,
        platform::IFileSystem& file_system);

    [[nodiscard]] core::Result<project::ProjectDocument> recoverSnapshot(
        std::string_view snapshot_id,
        const platform::IFileSystem& file_system) const;

    [[nodiscard]] core::Status discardSnapshot(
        std::string_view snapshot_id,
        platform::IFileSystem& file_system);

    [[nodiscard]] core::Status discardAll(platform::IFileSystem& file_system);

    [[nodiscard]] core::Status markCleanShutdown(platform::IFileSystem& file_system);

private:
    [[nodiscard]] core::Status persistIndex(platform::IFileSystem& file_system) const;
    [[nodiscard]] core::Status writeSessionMarker(bool active, platform::IFileSystem& file_system) const;

    std::filesystem::path recovery_dir_{};
    RecoveryStorageState storage_state_{RecoveryStorageState::Uninitialized};
    std::string unavailable_reason_{"Recovery service has not been initialized."};
    bool had_unclean_session_{false};
    std::uint32_t next_slot_index_{0};
    std::uint64_t snapshot_counter_{0};
    std::vector<RecoverySnapshotRecord> snapshots_{};
};

struct SystemInfoSummary {
    std::string application_name{"Final Effect 3D"};
    std::string engine_version;
    std::string platform_name;
    std::string graphics_backend;
    std::string graphics_device;
    std::string project_format{".fe3d JSON (v1.0-v1.3) + .fe3dbin Binary Archive (v1.0)"};
    std::string coordinate_system{"right-handed-y-up"};
    std::string license{"GNU GPL v3.0-or-later"};
    RecoveryStorageState recovery_storage_state{RecoveryStorageState::Uninitialized};
};

[[nodiscard]] SystemInfoSummary querySystemInfo(
    const StudioSession& session,
    RecoveryStorageState recovery_state = RecoveryStorageState::Uninitialized);

[[nodiscard]] core::Result<std::string> serializeSystemInfoJson(
    const SystemInfoSummary& info,
    bool pretty = true);

// Validates project name & save path, refuses silent overwrite unless allow_overwrite == true,
// initializes the real C++ ProjectTemplate in EditorController, and writes the valid .fe3d file.
[[nodiscard]] core::Status createNewProjectAtPath(
    EditorController& editor,
    ProjectTemplate tmpl,
    std::string project_name,
    std::string author,
    const std::filesystem::path& save_path,
    bool allow_overwrite,
    platform::IFileSystem& file_system);

} // namespace finaleffect::app
