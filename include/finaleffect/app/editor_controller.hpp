// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include "finaleffect/app/studio_session.hpp"
#include "finaleffect/core/result.hpp"
#include "finaleffect/math/vec3.hpp"
#include "finaleffect/platform/file_system.hpp"
#include "finaleffect/scene/camera.hpp"
#include "finaleffect/scene/light.hpp"
#include "finaleffect/scene/object_id.hpp"
#include "finaleffect/scene/scene.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace finaleffect::app {

enum class ScenePreset : std::uint8_t {
    Cube = 0,
    Pyramid = 1,
    Plane = 2,
    EmptyPivot = 3,
    DirectionalLight = 4,
    Camera = 5,
};

// Portable studio editor controller binding UI panels (Outliner, Inspector, Toolbar,
// Project I/O, and Telemetry) directly to StudioSession, Scene, CommandStack, and ProjectSerializer.
class EditorController {
public:
    explicit EditorController(StudioSession& session);

    [[nodiscard]] StudioSession& session() noexcept { return session_; }
    [[nodiscard]] const StudioSession& session() const noexcept { return session_; }

    // Selection & viewport operations.
    [[nodiscard]] core::Status selectObject(scene::ObjectId id);
    [[nodiscard]] std::optional<scene::RaycastHit> pickViewportPixel(float x, float y);
    [[nodiscard]] core::Status frameSelectionOrScene();
    void setRigAnimationPlaying(bool playing) noexcept;

    // Scene authoring operations (all backed by undoable CommandStack transactions).
    [[nodiscard]] core::Result<scene::ObjectId> addPresetObject(
        ScenePreset preset,
        scene::ObjectId parent_id = scene::ObjectId::invalid());

    [[nodiscard]] core::Result<scene::ObjectId> duplicateSelectedObject();
    [[nodiscard]] core::Status deleteSelectedObject(
        scene::RemovalMode mode = scene::RemovalMode::Recursive);

    [[nodiscard]] core::Status renameObject(scene::ObjectId id, std::string new_name);
    [[nodiscard]] core::Status setObjectVisibility(scene::ObjectId id, bool visible);
    [[nodiscard]] core::Status reparentObject(scene::ObjectId child_id, scene::ObjectId new_parent_id);

    [[nodiscard]] core::Status setObjectTransformEulerDegrees(
        scene::ObjectId id,
        const math::Vec3& position,
        const math::Vec3& euler_degrees,
        const math::Vec3& scale,
        bool allow_coalesce = false);

    [[nodiscard]] core::Status resetObjectTransform(scene::ObjectId id);

    [[nodiscard]] core::Status setCameraComponent(
        scene::ObjectId id,
        const scene::CameraComponent& camera);
    [[nodiscard]] core::Status setActiveCamera(scene::ObjectId id);

    [[nodiscard]] core::Status setDirectionalLightComponent(
        scene::ObjectId id,
        const scene::DirectionalLightComponent& light,
        bool allow_coalesce = false);

    [[nodiscard]] core::Status updateProjectMetadata(
        std::string project_name,
        std::string author,
        std::string scene_name);

    // Undo / Redo & project lifecycle.
    [[nodiscard]] core::Status undo();
    [[nodiscard]] core::Status redo();
    [[nodiscard]] core::Status newStarterProject();

    [[nodiscard]] core::Result<std::string> exportInlineProjectJson() const;
    [[nodiscard]] core::Status importInlineProjectJson(std::string_view json_text);

    [[nodiscard]] core::Status saveBundleProject(
        const std::filesystem::path& fe3d_path,
        platform::IFileSystem& file_system);
    [[nodiscard]] core::Status loadProjectFile(
        const std::filesystem::path& fe3d_path,
        const platform::IFileSystem& file_system);

    // Structured JSON UI state & command dispatch for host UIs (WebAssembly DOM, IPC, testing).
    [[nodiscard]] std::string serializeStateJson() const;
    [[nodiscard]] core::Status executeActionJson(std::string_view action_json);

    [[nodiscard]] std::uint64_t stateRevision() const noexcept { return state_revision_; }
    [[nodiscard]] const std::string& lastStatusMessage() const noexcept { return last_status_message_; }
    [[nodiscard]] bool lastStatusIsError() const noexcept { return last_status_is_error_; }

private:
    [[nodiscard]] core::Result<scene::MeshId> ensurePresetMesh(ScenePreset preset);
    [[nodiscard]] std::string generateUniqueObjectName(std::string_view base_prefix) const;
    void recordStatus(const core::Status& st, std::string_view ok_message);

    StudioSession& session_;
    std::uint64_t state_revision_{1};
    std::uint32_t spawn_counter_{0};
    std::string last_status_message_{"Ready"};
    bool last_status_is_error_{false};
};

} // namespace finaleffect::app
