// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include "finaleffect/core/result.hpp"
#include "finaleffect/core/version.hpp"
#include "finaleffect/platform/file_system.hpp"
#include "finaleffect/scene/scene.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace finaleffect::project {

struct ProjectMetadata {
    std::string project_name{"Untitled Project"};
    std::string author;
    std::string description;
    std::string engine_version{std::string(core::engineVersionString())};
};

struct ProjectDocument {
    ProjectMetadata metadata{};
    scene::Scene scene{"Untitled Scene"};
};

// Versioned Final Effect 3D (.fe3d) JSON project serializer and validator.
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

    [[nodiscard]] static core::Result<ProjectDocument> loadFromFile(
        const std::filesystem::path& path,
        const platform::IFileSystem& file_system);
};

} // namespace finaleffect::project
