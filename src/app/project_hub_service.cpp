// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/app/project_hub_service.hpp"
#include "finaleffect/core/version.hpp"
#include "finaleffect/platform/paths.hpp"
#include "finaleffect/project/json.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <thread>

namespace finaleffect::app {

namespace {

std::string toLowerAscii(std::string_view sv) {
    std::string out;
    out.reserve(sv.size());
    for (const char ch : sv) {
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
    return out;
}

std::string trimString(std::string_view sv) {
    std::size_t a = 0;
    while (a < sv.size() && std::isspace(static_cast<unsigned char>(sv[a])) != 0) {
        ++a;
    }
    std::size_t b = sv.size();
    while (b > a && std::isspace(static_cast<unsigned char>(sv[b - 1])) != 0) {
        --b;
    }
    return std::string(sv.substr(a, b - a));
}

bool containsCaseInsensitive(std::string_view haystack, std::string_view needle_lower) {
    if (needle_lower.empty()) {
        return true;
    }
    return toLowerAscii(haystack).find(needle_lower) != std::string::npos;
}

std::string makeDeterministicPathId(std::string_view normalized_path) {
    std::uint64_t hash = 1469598103934665603ULL;
    for (const char ch : normalized_path) {
        const auto b = static_cast<unsigned char>(ch);
        hash ^= static_cast<std::uint64_t>(b);
        hash *= 1099511628211ULL;
    }
    static constexpr char kHex[] = "0123456789abcdef";
    std::string out = "rec_";
    for (int i = 15; i >= 0; --i) {
        out.push_back(kHex[(hash >> (i * 4)) & 0xFULL]);
    }
    return out;
}

std::string_view recoveryStateSlug(RecoveryStorageState st) noexcept {
    switch (st) {
        case RecoveryStorageState::Available:   return "available";
        case RecoveryStorageState::Unavailable: return "unavailable";
        case RecoveryStorageState::Uninitialized:
        default:
            return "uninitialized";
    }
}

} // namespace

const std::vector<TemplateDescriptor>& TemplateCatalog::all() noexcept {
    static const std::vector<TemplateDescriptor> kTemplates = {
        TemplateDescriptor{
            .id = ProjectTemplate::StarterShowcase,
            .slug = "starter",
            .title = "Starter Showcase",
            .category = "General",
            .summary = "Hierarchical showcase rig with HeroCube, AccentPyramid, MainCamera, SunLight, and 3 PBR materials.",
            .expected_objects = 5u,
            .expected_meshes = 2u,
        },
        TemplateDescriptor{
            .id = ProjectTemplate::MinimalCube,
            .slug = "minimal_cube",
            .title = "Minimal Cube",
            .category = "Modeling",
            .summary = "Single editable polygon Cube at world origin with Studio Clay PBR material, MainCamera, and SunLight.",
            .expected_objects = 3u,
            .expected_meshes = 1u,
        },
        TemplateDescriptor{
            .id = ProjectTemplate::EmptyStage,
            .slug = "empty_stage",
            .title = "Empty Modeling Stage",
            .category = "Modeling",
            .summary = "Clean right-handed +Y origin stage with calibrated MainCamera, SunLight, and procedural grid.",
            .expected_objects = 2u,
            .expected_meshes = 0u,
        },
        TemplateDescriptor{
            .id = ProjectTemplate::ShaderBall,
            .slug = "shader_ball",
            .title = "Shader Ball Studio",
            .category = "LookDev & Shading",
            .summary = "UV ShaderSphere configured with Brushed Gold, Chrome Mirror, and Ceramic Matte PBR materials.",
            .expected_objects = 3u,
            .expected_meshes = 1u,
        },
    };
    return kTemplates;
}

std::optional<ProjectTemplate> TemplateCatalog::parseSlug(std::string_view slug) noexcept {
    const std::string lower = toLowerAscii(trimString(slug));
    if (lower == "starter" || lower == "showcase" || lower == "starter_showcase") {
        return ProjectTemplate::StarterShowcase;
    }
    if (lower == "minimal_cube" || lower == "cube") {
        return ProjectTemplate::MinimalCube;
    }
    if (lower == "empty_stage" || lower == "empty") {
        return ProjectTemplate::EmptyStage;
    }
    if (lower == "shader_ball" || lower == "shading") {
        return ProjectTemplate::ShaderBall;
    }
    return std::nullopt;
}

std::string_view TemplateCatalog::canonicalSlug(ProjectTemplate tmpl) noexcept {
    return describe(tmpl).slug;
}

const TemplateDescriptor& TemplateCatalog::describe(ProjectTemplate tmpl) noexcept {
    const auto& list = all();
    for (const auto& item : list) {
        if (item.id == tmpl) {
            return item;
        }
    }
    return list.front();
}

// ============================================================================
// RecentProjectsIndex
// ============================================================================

core::Status RecentProjectsIndex::loadFromFile(
    const std::filesystem::path& index_path,
    const platform::IFileSystem& file_system) {
    entries_.clear();
    if (index_path.empty() || !file_system.exists(index_path)) {
        return core::Status::ok();
    }

    auto text_res = file_system.readTextFile(index_path);
    if (!text_res) {
        return text_res.error();
    }

    auto parsed = project::parseJson(text_res.value());
    if (!parsed) {
        return parsed.error();
    }
    const auto& root = parsed.value();
    if (!root.isObject()) {
        return core::Status::fail(core::ErrorCode::ParseError, "Recent projects JSON root must be an object");
    }

    const auto* arr_val = root.findKey("entries");
    if (arr_val == nullptr || !arr_val->isArray()) {
        return core::Status::ok();
    }

    for (const auto& item : arr_val->asArray()) {
        if (!item.isObject()) {
            continue;
        }
        RecentProjectEntry rec{};
        if (const auto* v = item.findKey("id"); v != nullptr && v->isString()) rec.id = v->asString();
        if (const auto* v = item.findKey("project_name"); v != nullptr && v->isString()) rec.project_name = v->asString();
        if (const auto* v = item.findKey("author"); v != nullptr && v->isString()) rec.author = v->asString();
        if (const auto* v = item.findKey("scene_name"); v != nullptr && v->isString()) rec.scene_name = v->asString();
        if (const auto* v = item.findKey("file_path"); v != nullptr && v->isString()) rec.file_path = v->asString();
        if (const auto* v = item.findKey("format_label"); v != nullptr && v->isString()) rec.format_label = v->asString();
        if (const auto* v = item.findKey("byte_size"); v != nullptr && v->isNumber() && v->asNumber() >= 0.0) {
            rec.byte_size = static_cast<std::uint64_t>(v->asNumber());
        }
        if (const auto* v = item.findKey("object_count"); v != nullptr && v->isNumber() && v->asNumber() >= 0.0) {
            rec.object_count = static_cast<std::uint32_t>(v->asNumber());
        }
        if (const auto* v = item.findKey("mesh_count"); v != nullptr && v->isNumber() && v->asNumber() >= 0.0) {
            rec.mesh_count = static_cast<std::uint32_t>(v->asNumber());
        }
        if (const auto* v = item.findKey("material_count"); v != nullptr && v->isNumber() && v->asNumber() >= 0.0) {
            rec.material_count = static_cast<std::uint32_t>(v->asNumber());
        }
        if (const auto* v = item.findKey("last_opened_epoch_ms"); v != nullptr && v->isNumber() && v->asNumber() >= 0.0) {
            rec.last_opened_epoch_ms = static_cast<std::uint64_t>(v->asNumber());
        }
        if (const auto* v = item.findKey("pinned"); v != nullptr && v->isBool()) {
            rec.pinned = v->asBool();
        }
        if (rec.id.empty() && !rec.file_path.empty()) {
            rec.id = makeDeterministicPathId(rec.file_path);
        }
        if (!rec.id.empty() && !rec.project_name.empty()) {
            entries_.push_back(std::move(rec));
        }
    }

    refreshDiskStatus(file_system);
    return core::Status::ok();
}

core::Status RecentProjectsIndex::saveToFile(
    const std::filesystem::path& index_path,
    platform::IFileSystem& file_system) const {
    if (index_path.empty()) {
        return core::Status::fail(core::ErrorCode::InvalidArgument, "Recent projects index path cannot be empty");
    }

    project::JsonArray arr;
    arr.reserve(entries_.size());
    for (const auto& rec : entries_) {
        project::JsonValue obj{project::JsonObject{}};
        obj.setKey("id", rec.id);
        obj.setKey("project_name", rec.project_name);
        obj.setKey("author", rec.author);
        obj.setKey("scene_name", rec.scene_name);
        obj.setKey("file_path", rec.file_path);
        obj.setKey("format_label", rec.format_label);
        obj.setKey("byte_size", rec.byte_size);
        obj.setKey("object_count", static_cast<std::uint64_t>(rec.object_count));
        obj.setKey("mesh_count", static_cast<std::uint64_t>(rec.mesh_count));
        obj.setKey("material_count", static_cast<std::uint64_t>(rec.material_count));
        obj.setKey("last_opened_epoch_ms", rec.last_opened_epoch_ms);
        obj.setKey("pinned", rec.pinned);
        arr.push_back(std::move(obj));
    }

    project::JsonValue root{project::JsonObject{}};
    root.setKey("format", std::string(kFormatIdentifier));
    root.setKey("version", static_cast<std::uint64_t>(1));
    root.setKey("entries", std::move(arr));

    auto json_res = project::writeJson(root, true);
    if (!json_res) {
        return json_res.error();
    }
    return file_system.writeTextFile(index_path, json_res.value());
}

void RecentProjectsIndex::refreshDiskStatus(const platform::IFileSystem& file_system) {
    for (auto& rec : entries_) {
        rec.exists_on_disk = !rec.file_path.empty() && file_system.exists(std::filesystem::path(rec.file_path));
    }
}

RecentProjectEntry RecentProjectsIndex::recordProject(
    const project::ProjectDocument& doc,
    const std::filesystem::path& project_path,
    std::uint64_t byte_size,
    std::uint64_t timestamp_epoch_ms,
    std::uint32_t max_unpinned_entries) {
    const std::string norm_path = project_path.lexically_normal().generic_string();
    const std::string id = makeDeterministicPathId(
        norm_path.empty() ? doc.metadata.project_name : norm_path);

    bool was_pinned = false;
    auto existing = std::find_if(entries_.begin(), entries_.end(), [&](const RecentProjectEntry& e) {
        return e.id == id || (!norm_path.empty() && e.file_path == norm_path);
    });
    if (existing != entries_.end()) {
        was_pinned = existing->pinned;
        entries_.erase(existing);
    }

    RecentProjectEntry entry{
        .id = id,
        .project_name = doc.metadata.project_name.empty() ? "Untitled Project" : doc.metadata.project_name,
        .author = doc.metadata.author,
        .scene_name = doc.scene.name(),
        .file_path = norm_path,
        .format_label = ".fe3d v1.3",
        .byte_size = byte_size,
        .object_count = static_cast<std::uint32_t>(doc.scene.objectCount()),
        .mesh_count = static_cast<std::uint32_t>(doc.scene.meshCount()),
        .material_count = static_cast<std::uint32_t>(doc.scene.materials().size()),
        .last_opened_epoch_ms = timestamp_epoch_ms,
        .pinned = was_pinned,
        .exists_on_disk = true,
    };

    entries_.insert(entries_.begin(), entry);
    enforceLimit(max_unpinned_entries);
    return entry;
}

std::vector<RecentProjectEntry> RecentProjectsIndex::query(
    std::string_view search_filter,
    RecentSortMode sort_mode,
    bool pinned_only) const {
    const std::string needle = toLowerAscii(trimString(search_filter));
    std::vector<RecentProjectEntry> out;
    out.reserve(entries_.size());

    for (const auto& rec : entries_) {
        if (pinned_only && !rec.pinned) {
            continue;
        }
        if (!needle.empty()) {
            const bool matched =
                containsCaseInsensitive(rec.project_name, needle) ||
                containsCaseInsensitive(rec.file_path, needle) ||
                containsCaseInsensitive(rec.author, needle) ||
                containsCaseInsensitive(rec.scene_name, needle);
            if (!matched) {
                continue;
            }
        }
        out.push_back(rec);
    }

    std::stable_sort(out.begin(), out.end(), [&](const RecentProjectEntry& a, const RecentProjectEntry& b) {
        if (a.pinned != b.pinned) {
            return a.pinned > b.pinned;
        }
        switch (sort_mode) {
            case RecentSortMode::NameAsc: {
                const std::string la = toLowerAscii(a.project_name);
                const std::string lb = toLowerAscii(b.project_name);
                if (la != lb) return la < lb;
                return a.last_opened_epoch_ms > b.last_opened_epoch_ms;
            }
            case RecentSortMode::ByteSizeDesc:
                if (a.byte_size != b.byte_size) return a.byte_size > b.byte_size;
                return a.last_opened_epoch_ms > b.last_opened_epoch_ms;
            case RecentSortMode::LastOpenedDesc:
            default:
                return a.last_opened_epoch_ms > b.last_opened_epoch_ms;
        }
    });

    return out;
}

bool RecentProjectsIndex::setPinned(std::string_view id, bool pinned) {
    for (auto& rec : entries_) {
        if (rec.id == id) {
            rec.pinned = pinned;
            return true;
        }
    }
    return false;
}

bool RecentProjectsIndex::removeById(std::string_view id) {
    const auto before = entries_.size();
    entries_.erase(
        std::remove_if(entries_.begin(), entries_.end(), [&](const RecentProjectEntry& r) {
            return r.id == id;
        }),
        entries_.end());
    return entries_.size() != before;
}

std::size_t RecentProjectsIndex::clearUnpinned() {
    const auto before = entries_.size();
    entries_.erase(
        std::remove_if(entries_.begin(), entries_.end(), [](const RecentProjectEntry& r) {
            return !r.pinned;
        }),
        entries_.end());
    return before - entries_.size();
}

void RecentProjectsIndex::enforceLimit(std::uint32_t max_unpinned_entries) {
    const std::size_t cap = std::max<std::size_t>(1u, static_cast<std::size_t>(max_unpinned_entries));
    std::size_t unpinned_seen = 0;
    std::vector<RecentProjectEntry> kept;
    kept.reserve(entries_.size());
    for (auto& rec : entries_) {
        if (rec.pinned) {
            kept.push_back(std::move(rec));
        } else if (unpinned_seen < cap) {
            ++unpinned_seen;
            kept.push_back(std::move(rec));
        }
    }
    entries_ = std::move(kept);
}

// ============================================================================
// RecoveryService
// ============================================================================

core::Status RecoveryService::initialize(
    const std::filesystem::path& recovery_dir,
    platform::IFileSystem& file_system) {
    snapshots_.clear();
    had_unclean_session_ = false;
    next_slot_index_ = 0;
    snapshot_counter_ = 0;
    recovery_dir_ = recovery_dir;

    if (recovery_dir_.empty()) {
        storage_state_ = RecoveryStorageState::Unavailable;
        unavailable_reason_ = "Recovery directory path is empty.";
        return core::Status::fail(core::ErrorCode::InvalidArgument, unavailable_reason_);
    }

    auto dir_st = file_system.createDirectories(recovery_dir_);
    if (!dir_st) {
        storage_state_ = RecoveryStorageState::Unavailable;
        unavailable_reason_ = dir_st.error().message;
        return dir_st;
    }

    const auto session_marker_path = recovery_dir_ / "session_marker.json";
    if (file_system.exists(session_marker_path)) {
        if (auto marker_txt = file_system.readTextFile(session_marker_path); marker_txt) {
            if (auto marker_json = project::parseJson(marker_txt.value());
                marker_json && marker_json.value().isObject()) {
                if (const auto* act = marker_json.value().findKey("session_active");
                    act != nullptr && act->isBool() && act->asBool()) {
                    had_unclean_session_ = true;
                }
            }
        }
    }

    const auto index_path = recovery_dir_ / "recovery_index.json";
    if (file_system.exists(index_path)) {
        if (auto idx_txt = file_system.readTextFile(index_path); idx_txt) {
            if (auto idx_json = project::parseJson(idx_txt.value());
                idx_json && idx_json.value().isObject()) {
                if (const auto* ns = idx_json.value().findKey("next_slot_index");
                    ns != nullptr && ns->isNumber() && ns->asNumber() >= 0.0) {
                    next_slot_index_ = static_cast<std::uint32_t>(ns->asNumber()) % kMaxRingSlots;
                }
                if (const auto* sc = idx_json.value().findKey("snapshot_counter");
                    sc != nullptr && sc->isNumber() && sc->asNumber() >= 0.0) {
                    snapshot_counter_ = static_cast<std::uint64_t>(sc->asNumber());
                }
                if (const auto* arr = idx_json.value().findKey("snapshots");
                    arr != nullptr && arr->isArray()) {
                    for (const auto& item : arr->asArray()) {
                        if (!item.isObject()) continue;
                        RecoverySnapshotRecord rec{};
                        if (const auto* v = item.findKey("id"); v != nullptr && v->isString()) rec.id = v->asString();
                        if (const auto* v = item.findKey("slot_index"); v != nullptr && v->isNumber()) {
                            rec.slot_index = static_cast<std::uint32_t>(std::max(0.0, v->asNumber()));
                        }
                        if (const auto* v = item.findKey("project_name"); v != nullptr && v->isString()) {
                            rec.project_name = v->asString();
                        }
                        if (const auto* v = item.findKey("author"); v != nullptr && v->isString()) {
                            rec.author = v->asString();
                        }
                        if (const auto* v = item.findKey("origin_path"); v != nullptr && v->isString()) {
                            rec.origin_path = v->asString();
                        }
                        if (const auto* v = item.findKey("snapshot_file_name"); v != nullptr && v->isString()) {
                            rec.snapshot_file_name = v->asString();
                        }
                        if (const auto* v = item.findKey("captured_epoch_ms"); v != nullptr && v->isNumber()) {
                            rec.captured_epoch_ms = static_cast<std::uint64_t>(std::max(0.0, v->asNumber()));
                        }
                        if (const auto* v = item.findKey("byte_size"); v != nullptr && v->isNumber()) {
                            rec.byte_size = static_cast<std::uint64_t>(std::max(0.0, v->asNumber()));
                        }
                        if (const auto* v = item.findKey("object_count"); v != nullptr && v->isNumber()) {
                            rec.object_count = static_cast<std::uint32_t>(std::max(0.0, v->asNumber()));
                        }
                        if (const auto* v = item.findKey("reason"); v != nullptr && v->isString()) {
                            rec.reason = v->asString();
                        }
                        if (const auto* v = item.findKey("unclean_session"); v != nullptr && v->isBool()) {
                            rec.unclean_session = v->asBool();
                        }
                        if (had_unclean_session_) {
                            rec.unclean_session = true;
                        }
                        // Only include snapshots whose backing .fe3d file genuinely exists on disk!
                        if (!rec.id.empty() && !rec.snapshot_file_name.empty() &&
                            file_system.exists(recovery_dir_ / rec.snapshot_file_name)) {
                            snapshots_.push_back(std::move(rec));
                        }
                    }
                }
            }
        }
    }

    auto marker_st = writeSessionMarker(true, file_system);
    if (!marker_st) {
        storage_state_ = RecoveryStorageState::Unavailable;
        unavailable_reason_ = marker_st.error().message;
        snapshots_.clear();
        return marker_st;
    }

    storage_state_ = RecoveryStorageState::Available;
    unavailable_reason_.clear();
    if (had_unclean_session_ && !snapshots_.empty()) {
        (void)persistIndex(file_system);
    }
    return core::Status::ok();
}

core::Result<RecoverySnapshotRecord> RecoveryService::captureSnapshot(
    const project::ProjectDocument& document,
    std::string_view origin_path,
    std::string_view reason,
    std::uint64_t timestamp_epoch_ms,
    platform::IFileSystem& file_system) {
    if (storage_state_ != RecoveryStorageState::Available) {
        return core::Result<RecoverySnapshotRecord>::fail(
            core::ErrorCode::InvalidState,
            unavailable_reason_.empty() ? "Recovery storage is unavailable" : unavailable_reason_);
    }

    auto serialized = project::ProjectSerializer::serializeToString(document, false);
    if (!serialized) {
        return serialized.error();
    }

    const std::uint32_t slot = next_slot_index_ % kMaxRingSlots;
    next_slot_index_ = (slot + 1u) % kMaxRingSlots;
    const std::uint64_t seq = ++snapshot_counter_;
    const std::string file_name = "autosave_slot_" + std::to_string(slot) + ".fe3d";
    const auto slot_path = recovery_dir_ / file_name;

    auto write_st = file_system.writeTextFile(slot_path, serialized.value());
    if (!write_st) {
        return write_st.error();
    }

    snapshots_.erase(
        std::remove_if(snapshots_.begin(), snapshots_.end(), [&](const RecoverySnapshotRecord& r) {
            return r.slot_index == slot || r.snapshot_file_name == file_name;
        }),
        snapshots_.end());

    RecoverySnapshotRecord record{
        .id = "snap_" + std::to_string(seq),
        .slot_index = slot,
        .project_name = document.metadata.project_name.empty() ? "Untitled Project" : document.metadata.project_name,
        .author = document.metadata.author,
        .origin_path = std::string(origin_path),
        .snapshot_file_name = file_name,
        .captured_epoch_ms = timestamp_epoch_ms,
        .byte_size = static_cast<std::uint64_t>(serialized.value().size()),
        .object_count = static_cast<std::uint32_t>(document.scene.objectCount()),
        .reason = reason.empty() ? "Autosave" : std::string(reason),
        .unclean_session = false,
    };

    snapshots_.insert(snapshots_.begin(), record);
    auto idx_st = persistIndex(file_system);
    if (!idx_st) {
        return idx_st.error();
    }

    return core::Result<RecoverySnapshotRecord>::ok(std::move(record));
}

core::Result<project::ProjectDocument> RecoveryService::recoverSnapshot(
    std::string_view snapshot_id,
    const platform::IFileSystem& file_system) const {
    if (storage_state_ != RecoveryStorageState::Available) {
        return core::Result<project::ProjectDocument>::fail(
            core::ErrorCode::InvalidState,
            unavailable_reason_.empty() ? "Recovery storage is unavailable" : unavailable_reason_);
    }
    if (snapshots_.empty()) {
        return core::Result<project::ProjectDocument>::fail(
            core::ErrorCode::NotFound,
            "No recovery snapshots are available");
    }

    const RecoverySnapshotRecord* target = nullptr;
    if (snapshot_id.empty() || snapshot_id == "latest") {
        target = &snapshots_.front();
    } else {
        for (const auto& rec : snapshots_) {
            if (rec.id == snapshot_id) {
                target = &rec;
                break;
            }
        }
    }

    if (target == nullptr) {
        return core::Result<project::ProjectDocument>::fail(
            core::ErrorCode::NotFound,
            "Requested recovery snapshot was not found",
            std::string(snapshot_id));
    }

    return project::ProjectSerializer::loadFromFile(
        recovery_dir_ / target->snapshot_file_name,
        file_system);
}

core::Status RecoveryService::discardSnapshot(
    std::string_view snapshot_id,
    platform::IFileSystem& file_system) {
    if (storage_state_ != RecoveryStorageState::Available) {
        return core::Status::fail(core::ErrorCode::InvalidState, "Recovery storage is unavailable");
    }
    auto it = std::find_if(snapshots_.begin(), snapshots_.end(), [&](const RecoverySnapshotRecord& r) {
        return r.id == snapshot_id;
    });
    if (it == snapshots_.end()) {
        return core::Status::fail(core::ErrorCode::NotFound, "Recovery snapshot not found", std::string(snapshot_id));
    }
    snapshots_.erase(it);
    return persistIndex(file_system);
}

core::Status RecoveryService::discardAll(platform::IFileSystem& file_system) {
    if (storage_state_ != RecoveryStorageState::Available) {
        return core::Status::fail(core::ErrorCode::InvalidState, "Recovery storage is unavailable");
    }
    snapshots_.clear();
    had_unclean_session_ = false;
    return persistIndex(file_system);
}

core::Status RecoveryService::markCleanShutdown(platform::IFileSystem& file_system) {
    if (storage_state_ != RecoveryStorageState::Available) {
        return core::Status::ok();
    }
    had_unclean_session_ = false;
    return writeSessionMarker(false, file_system);
}

core::Status RecoveryService::persistIndex(platform::IFileSystem& file_system) const {
    project::JsonArray arr;
    arr.reserve(snapshots_.size());
    for (const auto& rec : snapshots_) {
        project::JsonValue obj{project::JsonObject{}};
        obj.setKey("id", rec.id);
        obj.setKey("slot_index", static_cast<std::uint64_t>(rec.slot_index));
        obj.setKey("project_name", rec.project_name);
        obj.setKey("author", rec.author);
        obj.setKey("origin_path", rec.origin_path);
        obj.setKey("snapshot_file_name", rec.snapshot_file_name);
        obj.setKey("captured_epoch_ms", rec.captured_epoch_ms);
        obj.setKey("byte_size", rec.byte_size);
        obj.setKey("object_count", static_cast<std::uint64_t>(rec.object_count));
        obj.setKey("reason", rec.reason);
        obj.setKey("unclean_session", rec.unclean_session);
        arr.push_back(std::move(obj));
    }

    project::JsonValue root{project::JsonObject{}};
    root.setKey("format", std::string("finaleffect.recovery_index"));
    root.setKey("version", static_cast<std::uint64_t>(1));
    root.setKey("next_slot_index", static_cast<std::uint64_t>(next_slot_index_));
    root.setKey("snapshot_counter", snapshot_counter_);
    root.setKey("snapshots", std::move(arr));

    auto json_res = project::writeJson(root, true);
    if (!json_res) {
        return json_res.error();
    }
    return file_system.writeTextFile(recovery_dir_ / "recovery_index.json", json_res.value());
}

core::Status RecoveryService::writeSessionMarker(bool active, platform::IFileSystem& file_system) const {
    project::JsonValue root{project::JsonObject{}};
    root.setKey("format", std::string("finaleffect.session_marker"));
    root.setKey("session_active", active);
    auto json_res = project::writeJson(root, true);
    if (!json_res) {
        return json_res.error();
    }
    return file_system.writeTextFile(recovery_dir_ / "session_marker.json", json_res.value());
}

// ============================================================================
// System Info & Validated New Project Creation
// ============================================================================

SystemInfoSummary querySystemInfo(
    const StudioSession& session,
    RecoveryStorageState recovery_state) {
    const auto& caps = session.renderer().device().capabilities();
#if defined(NDEBUG)
    constexpr const char* kBuildCfg = "Release";
#else
    constexpr const char* kBuildCfg = "Debug";
#endif
    return SystemInfoSummary{
        .application_name = "Final Effect 3D",
        .engine_version = std::string(core::engineVersionString()),
        .platform_name = std::string(platform::hostPlatformName()),
        .graphics_backend = caps.backend_name.empty() ? std::string("Unavailable") : caps.backend_name,
        .graphics_device = caps.device_name.empty() ? std::string("Unavailable") : caps.device_name,
        .graphics_vendor = caps.vendor_name.empty() ? std::string("Unavailable") : caps.vendor_name,
        .graphics_api_version = caps.api_version.empty() ? std::string("Unavailable") : caps.api_version,
        .shading_language_version =
            caps.shading_language_version.empty() ? std::string("Unavailable") : caps.shading_language_version,
        .build_configuration = kBuildCfg,
        .logical_cpu_threads = std::thread::hardware_concurrency(),
        .max_texture_dimension = caps.max_texture_size,
        .max_viewport_dimension = caps.max_viewport_dimension,
        .project_format = ".fe3d JSON (v1.0-v1.3) + .fe3dbin Binary Archive (v1.0)",
        .coordinate_system = std::string(project::ProjectSerializer::kCoordinateSystem),
        .license = "GNU GPL v3.0-or-later",
        .recovery_storage_state = recovery_state,
    };
}

core::Result<std::string> serializeSystemInfoJson(const SystemInfoSummary& info, bool pretty) {
    project::JsonValue root{project::JsonObject{}};
    root.setKey("application_name", info.application_name);
    root.setKey("engine_version", info.engine_version);
    root.setKey("build_configuration", info.build_configuration.empty() ? std::string("Unavailable") : info.build_configuration);
    root.setKey("platform_name", info.platform_name);
    root.setKey("logical_cpu_threads", static_cast<std::uint64_t>(info.logical_cpu_threads));
    root.setKey("graphics_backend", info.graphics_backend);
    root.setKey("graphics_device", info.graphics_device);
    root.setKey("graphics_vendor", info.graphics_vendor.empty() ? std::string("Unavailable") : info.graphics_vendor);
    root.setKey("graphics_api_version", info.graphics_api_version.empty() ? std::string("Unavailable") : info.graphics_api_version);
    root.setKey(
        "shading_language_version",
        info.shading_language_version.empty() ? std::string("Unavailable") : info.shading_language_version);
    root.setKey("max_texture_dimension", static_cast<std::uint64_t>(info.max_texture_dimension));
    root.setKey("max_viewport_dimension", static_cast<std::uint64_t>(info.max_viewport_dimension));
    root.setKey("project_format", info.project_format);
    root.setKey("coordinate_system", info.coordinate_system);
    root.setKey("license", info.license);
    root.setKey("recovery_storage_state", std::string(recoveryStateSlug(info.recovery_storage_state)));
    return project::writeJson(root, pretty);
}

core::Status createNewProjectAtPath(
    EditorController& editor,
    ProjectTemplate tmpl,
    std::string project_name,
    std::string author,
    const std::filesystem::path& save_path,
    bool allow_overwrite,
    platform::IFileSystem& file_system) {
    const std::string clean_name = trimString(project_name);
    if (clean_name.empty()) {
        return core::Status::fail(
            core::ErrorCode::InvalidArgument,
            "Project name cannot be empty");
    }
    if (save_path.empty()) {
        return core::Status::fail(
            core::ErrorCode::InvalidArgument,
            "Project save location cannot be empty");
    }
    if (!allow_overwrite && file_system.exists(save_path)) {
        return core::Status::fail(
            core::ErrorCode::AlreadyExists,
            "A project file already exists at the selected location; confirmation is required before overwriting",
            save_path.string());
    }

    auto create_st = editor.createNewProject(tmpl, clean_name, std::move(author));
    if (!create_st) {
        return create_st;
    }

    editor.session().syncDocumentState(editor.selectedMaterial());
    auto save_st = editor.session().saveProjectToFile(save_path, file_system);
    if (!save_st) {
        return save_st;
    }

    editor.setActiveProjectPath(save_path.lexically_normal().generic_string());
    editor.markDocumentSaved();
    return core::Status::ok();
}

} // namespace finaleffect::app
