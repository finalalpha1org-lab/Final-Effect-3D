// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include "finaleffect/core/result.hpp"
#include "finaleffect/core/version.hpp"
#include "finaleffect/platform/file_system.hpp"
#include "finaleffect/project/json.hpp"
#include "finaleffect/project/workspace_state.hpp"
#include "finaleffect/scene/scene.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace finaleffect::project {

struct ProjectMetadata {
    std::string project_name{"Untitled Project"};
    std::string author;
    std::string description;
    std::string engine_version{std::string(core::engineVersionString())};
};

// Preserves unrecognized/custom JSON extension fields across round-trip save/load
// so unknown project data is never silently discarded.
struct ProjectPreservedFields {
    JsonObject root_extra;
    JsonObject metadata_extra;
    JsonObject scene_extra;
    JsonObject editor_state_extra;
    JsonObject workspace_extra;
    std::unordered_map<scene::MaterialId, JsonObject> material_extra;
    std::unordered_map<scene::MeshId, JsonObject> mesh_extra;
    std::unordered_map<scene::ObjectId, JsonObject> object_extra;
};

struct ProjectDocument {
    ProjectMetadata metadata{};
    scene::Scene scene{"Untitled Scene"};
    ProjectEditorState editor_state{};
    WorkspaceState workspace{};
    ProjectPreservedFields preserved_fields{};
    std::vector<std::string> load_warnings{};
};

// Versioned Final Effect 3D (.fe3d) JSON project serializer and validator (v1.0..v1.2).
class ProjectSerializer {
public:
    static constexpr std::string_view kFormatIdentifier = "finaleffect.project";
    static constexpr std::string_view kCoordinateSystem = "right-handed-y-up";

    [[nodiscard]] static core::Result<std::string> serializeToString(
        const ProjectDocument& document,
        bool pretty = true);

    [[nodiscard]] static core::Result<ProjectDocument> deserializeFromString(std::string_view json_text);

    [[nodiscard]] static core::Status saveToFile(
        const ProjectDocument& document,
        const std::filesystem::path& path,
        platform::IFileSystem& file_system);

    // Saves .fe3d JSON metadata + content-addressed companion <stem>.fe3dbin geometry archive.
    [[nodiscard]] static core::Status saveBundleToFile(
        const ProjectDocument& document,
        const std::filesystem::path& fe3d_path,
        platform::IFileSystem& file_system);

    [[nodiscard]] static core::Result<ProjectDocument> loadFromFile(
        const std::filesystem::path& path,
        const platform::IFileSystem& file_system);

    // Standalone workspace layout JSON helpers (used by both ProjectSerializer and WorkspaceManager).
    [[nodiscard]] static JsonValue encodeWorkspaceJson(
        const WorkspaceState& state,
        const JsonObject& extra_fields = {});

    [[nodiscard]] static core::Result<WorkspaceState> decodeWorkspaceJson(
        const JsonValue& json,
        JsonObject* out_extra_fields = nullptr,
        std::vector<std::string>* out_warnings = nullptr);
};

} // namespace finaleffect::project
