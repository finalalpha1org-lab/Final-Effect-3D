// Final Effect 3D - versioned project file format.
// SPDX-License-Identifier: GPL-3.0-only
//
// Format: JSON (see docs/ARCHITECTURE.md for the full schema).
//   * A project file always starts with a format identifier and a format version.
//   * Scalar data is written explicitly: no raw C++ memory is ever serialized.
//   * Loading validates before it mutates anything: a malformed or unsupported
//     file is rejected and the caller keeps its current scene.
//   * Large binary assets are intentionally not part of Stage 1; the schema is
//     designed so that an "assets" section can be added without breaking files
//     written today.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "finaleffect/core/Error.hpp"
#include "finaleffect/core/Json.hpp"
#include "finaleffect/platform/FileSystem.hpp"
#include "finaleffect/scene/Scene.hpp"

namespace fe::project {

/// Bump when a new, non-backwards-compatible key set is introduced.
inline constexpr int kProjectFormatVersion = 1;
/// Value of the "format" key of every Final Effect 3D project file.
inline constexpr std::string_view kProjectFormatId = "finaleffect3d-project";
/// Conventional file extension.
inline constexpr std::string_view kProjectFileExtension = ".fe3d";

/// Oldest format version this build can read.
inline constexpr int kMinimumSupportedFormatVersion = 1;

struct ProjectSettings {
    std::string author;
    std::string description;
    std::string units = "meters";
    math::Vec3 backgroundColor{0.06F, 0.07F, 0.09F};
    /// "auto", "cpu" or "opengl". A hint, never a requirement.
    std::string rendererHint = "auto";
};

struct ProjectDocument {
    int formatVersion = kProjectFormatVersion;
    std::uint64_t revision = 1;
    std::string applicationVersion;
    ProjectSettings settings;
    std::vector<scene::SceneObject> objects;

    /// Captures the current scene, including identifier allocation state.
    static ProjectDocument fromScene(const scene::Scene& scene);

    /// Validates every object and rebuilds a Scene from this document.
    core::Result<scene::Scene> toScene() const;

    core::Result<std::string> toJsonText() const;
    static core::Result<ProjectDocument> fromJsonText(std::string_view text);
};

/// Writes atomically through FileSystem::writeTextFileAtomic.
core::Status saveProjectFile(platform::FileSystem& fileSystem,
                             const std::string& path,
                             const ProjectDocument& document);

/// Reads, parses and validates a project file.
core::Result<ProjectDocument> loadProjectFile(platform::FileSystem& fileSystem, const std::string& path);

}  // namespace fe::project
