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
#include <span>
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

// Preserves unrecognized/custom extension fields across round-trip save/load
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

// Versioned Final Effect 3D project serializer for the .fe3d / .fe3a binary container (v1.0).
//
// Projects are stored as the app's own binary container (see binary_project.hpp); no JSON text is
// written to disk and legacy JSON project files are refused by the loader.
class ProjectSerializer {
public:
    static constexpr std::string_view kFormatIdentifier = "finaleffect.project";
    static constexpr std::string_view kCoordinateSystem = "right-handed-y-up";
    // Standalone workspace-layout container identifier (used by WorkspaceManager persistence).
    static constexpr std::string_view kWorkspaceFormatIdentifier = "finaleffect.workspace";

    // Encodes the document (plus its geometry, embedded in the container) as .fe3d / .fe3a bytes.
    [[nodiscard]] static core::Result<std::vector<std::uint8_t>> serializeToBytes(const ProjectDocument& document);

    [[nodiscard]] static core::Result<ProjectDocument> deserializeFromBytes(std::span<const std::uint8_t> bytes);

    [[nodiscard]] static core::Status saveToFile(
        const ProjectDocument& document,
        const std::filesystem::path& path,
        platform::IFileSystem& file_system);

    [[nodiscard]] static core::Result<ProjectDocument> loadFromFile(
        const std::filesystem::path& path,
        const platform::IFileSystem& file_system);

    // Standalone workspace layout helpers. The JsonValue form is the in-memory tree; the byte form is
    // the same binary container used for persistence (workspace layouts are never written as JSON text).
    [[nodiscard]] static JsonValue encodeWorkspaceValue(
        const WorkspaceState& state,
        const JsonObject& extra_fields = {});

    [[nodiscard]] static core::Result<WorkspaceState> decodeWorkspaceValue(
        const JsonValue& json,
        JsonObject* out_extra_fields = nullptr,
        std::vector<std::string>* out_warnings = nullptr);

    [[nodiscard]] static core::Result<std::vector<std::uint8_t>> serializeWorkspaceToBytes(
        const WorkspaceState& state,
        const JsonObject& extra_fields = {});

    [[nodiscard]] static core::Result<WorkspaceState> deserializeWorkspaceFromBytes(
        std::span<const std::uint8_t> bytes,
        JsonObject* out_extra_fields = nullptr,
        std::vector<std::string>* out_warnings = nullptr);
};

} // namespace finaleffect::project
