// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include "finaleffect/app/studio_session.hpp"
#include "finaleffect/core/result.hpp"
#include "finaleffect/math/vec3.hpp"
#include "finaleffect/platform/file_system.hpp"
#include "finaleffect/rendering/material_preview.hpp"
#include "finaleffect/scene/camera.hpp"
#include "finaleffect/scene/light.hpp"
#include "finaleffect/scene/material.hpp"
#include "finaleffect/scene/object_id.hpp"
#include "finaleffect/scene/scene.hpp"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <optional>
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
    Sphere = 6,
};

enum class ProjectTemplate : std::uint8_t {
    StarterShowcase = 0,
    MinimalCube = 1,
    EmptyStage = 2,
    ShaderBall = 3,
};

struct EditorConsoleEntry {
    std::uint64_t sequence{0};
    bool is_error{false};
    std::string message;
};

// Portable studio editor controller binding UI panels (Outliner, Properties, Material Editor,
// Asset Browser, Workspace Dock, Console, Toolbar, Project I/O, and Telemetry) directly to
// StudioSession, Scene, CommandStack, TransformGizmo, EditableMesh, and ProjectSerializer.
class EditorController {
public:
    explicit EditorController(StudioSession& session);

    [[nodiscard]] StudioSession& session() noexcept { return session_; }
    [[nodiscard]] const StudioSession& session() const noexcept { return session_; }

    // Selection, editor mode, gizmo & viewport operations.
    [[nodiscard]] core::Status selectObject(scene::ObjectId id);
    [[nodiscard]] std::optional<scene::RaycastHit> pickViewportPixel(float x, float y);
    [[nodiscard]] core::Status frameSelectionOrScene();
    void setRigAnimationPlaying(bool playing) noexcept;

    [[nodiscard]] core::Status setEditorMode(scene::EditMode mode);
    void setGizmoMode(GizmoMode mode) noexcept;
    void setGizmoSpace(GizmoSpace space) noexcept;
    [[nodiscard]] core::Status cancelGizmoDrag();

    // Mesh editing operations (all validated and backed by undoable ModifyMeshCommand).
    [[nodiscard]] core::Status selectMeshElement(
        scene::EditMode element_kind,
        std::uint32_t element_index,
        bool additive = false);
    void clearMeshSelection() noexcept;
    [[nodiscard]] core::Status selectBoundaryLoopVertices();
    [[nodiscard]] core::Status translateMeshSelection(
        const math::Vec3& local_delta,
        bool allow_coalesce = false);
    [[nodiscard]] core::Status extrudeSelectedFaces(float distance);
    [[nodiscard]] core::Status deleteMeshSelection();
    [[nodiscard]] core::Status createFaceFromSelectedVertices();
    [[nodiscard]] core::Status assignMaterialToSelectedFaces(scene::MaterialId material_id);

    // PBR material authoring & live 3D sphere preview.
    [[nodiscard]] scene::MaterialId selectedMaterial() const noexcept;
    [[nodiscard]] core::Status selectMaterial(scene::MaterialId id);
    [[nodiscard]] core::Result<scene::MaterialId> createMaterial(const scene::PbrMaterial& material = {});
    [[nodiscard]] core::Result<scene::MaterialId> duplicateMaterial(scene::MaterialId id);
    [[nodiscard]] core::Status renameMaterial(scene::MaterialId id, std::string new_name);
    [[nodiscard]] core::Status deleteMaterial(scene::MaterialId id);
    [[nodiscard]] core::Status updateMaterial(
        scene::MaterialId id,
        const scene::PbrMaterial& material,
        bool allow_coalesce = false);
    [[nodiscard]] core::Status assignObjectMaterial(
        scene::ObjectId object_id,
        std::optional<scene::MaterialId> material_id);

    void refreshMaterialPreview();
    [[nodiscard]] std::uint32_t materialPreviewWidth() const noexcept { return material_preview_.width(); }
    [[nodiscard]] std::uint32_t materialPreviewHeight() const noexcept { return material_preview_.height(); }
    [[nodiscard]] const std::uint8_t* materialPreviewRgbaBytes() const noexcept {
        return material_preview_.rawRgbaBytes();
    }
    [[nodiscard]] std::uint64_t materialPreviewGeneration() const noexcept {
        return material_preview_.renderGeneration();
    }

    // Workspace management & layout persistence.
    void setPanelVisible(project::WorkspacePanelId panel, bool visible) noexcept;
    void setWorkspaceDockRatios(float left_width, float right_width, float bottom_height) noexcept;
    void setWorkspacePanelDock(
        project::WorkspacePanelId panel,
        project::DockRegion region,
        std::uint32_t tab_order = 0) noexcept;
    void applyWorkspacePreset(WorkspacePreset preset) noexcept;
    [[nodiscard]] core::Status applyWorkspacePresetByName(std::string_view preset_name);
    void setTextInputFocused(bool focused) noexcept;
    [[nodiscard]] core::Result<std::string> exportWorkspaceLayoutJson() const;
    [[nodiscard]] core::Status importWorkspaceLayoutJson(std::string_view json_text);

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
    [[nodiscard]] core::Status createNewProject(
        ProjectTemplate tmpl,
        std::string project_name = {},
        std::string author = {});

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

    [[nodiscard]] std::uint64_t stateRevision() const noexcept {
        return state_revision_ + session_.interactionRevision();
    }
    [[nodiscard]] const std::string& lastStatusMessage() const noexcept { return last_status_message_; }
    [[nodiscard]] bool lastStatusIsError() const noexcept { return last_status_is_error_; }
    [[nodiscard]] const std::deque<EditorConsoleEntry>& consoleEntries() const noexcept {
        return console_entries_;
    }

private:
    [[nodiscard]] core::Result<scene::MeshId> ensurePresetMesh(ScenePreset preset);
    [[nodiscard]] std::string generateUniqueObjectName(std::string_view base_prefix) const;
    [[nodiscard]] std::string generateUniqueMaterialName(std::string_view base_prefix) const;
    void recordStatus(const core::Status& st, std::string_view ok_message);

    StudioSession& session_;
    mutable rendering::MaterialPreviewRenderer material_preview_{};
    mutable scene::MaterialId previewed_material_id_{scene::MaterialId::invalid()};
    mutable scene::PbrMaterial previewed_material_snapshot_{};
    mutable bool has_preview_snapshot_{false};

    scene::MaterialId selected_material_id_{scene::MaterialId::invalid()};
    std::uint64_t state_revision_{1};
    std::uint64_t console_seq_{0};
    std::uint32_t spawn_counter_{0};
    std::string last_status_message_{"Ready"};
    bool last_status_is_error_{false};
    std::deque<EditorConsoleEntry> console_entries_{};
};

} // namespace finaleffect::app
