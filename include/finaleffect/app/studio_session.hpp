// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include "finaleffect/app/orbit_camera_controller.hpp"
#include "finaleffect/app/transform_gizmo.hpp"
#include "finaleffect/app/workspace_manager.hpp"
#include "finaleffect/core/application.hpp"
#include "finaleffect/core/engine.hpp"
#include "finaleffect/platform/window.hpp"
#include "finaleffect/project/project.hpp"
#include "finaleffect/rendering/renderer.hpp"
#include "finaleffect/scene/commands.hpp"
#include "finaleffect/scene/editable_mesh.hpp"
#include "finaleffect/scene/picking.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>

namespace finaleffect::app {

struct StudioSessionConfig {
    bool animate_showcase_rig{true};
    float rig_angular_speed_radians{0.65f};
    float orbit_radians_per_pixel{0.005f};
    std::size_t command_history_max_bytes{16u * 1024u * 1024u};
};

// Application layer session connecting project/scene state, command stack, viewport camera,
// 3D transform gizmos, mesh element selection/editing, workspace layout, raycast picking,
// window input routing, and Renderer.
class StudioSession final : public core::IApplication {
public:
    StudioSession(
        platform::IWindow& window,
        std::unique_ptr<rendering::IRenderDevice> render_device,
        StudioSessionConfig config = {});

    [[nodiscard]] core::Status onInitialize(core::Engine& engine) override;
    [[nodiscard]] core::Status onUpdate(core::Engine& engine, const core::FrameTimings& timings) override;
    void onShutdown(core::Engine& engine) noexcept override;

    [[nodiscard]] core::Status loadProjectFromFile(
        const std::filesystem::path& path,
        const platform::IFileSystem& file_system);

    [[nodiscard]] core::Status saveProjectToFile(
        const std::filesystem::path& path,
        platform::IFileSystem& file_system) const;

    [[nodiscard]] core::Status saveProjectBundleToFile(
        const std::filesystem::path& fe3d_path,
        platform::IFileSystem& file_system) const;

    // Resets document to the starter 3D showcase project and clears command history.
    [[nodiscard]] core::Status resetStarterProject();

    // Performs a raycast from framebuffer coordinates and updates selectedObject().
    [[nodiscard]] std::optional<scene::RaycastHit> pickAtScreenPosition(float screen_x, float screen_y);

    // Picks a mesh element (Vertex / Edge / Face) in active edit mode at framebuffer coordinates.
    [[nodiscard]] bool pickMeshElementAtScreenPosition(
        float screen_x,
        float screen_y,
        bool additive = false);

    // Frames selectedObject() (or the entire visible scene when nothing is selected) in the active camera.
    [[nodiscard]] core::Status frameSelectionOrScene();

    [[nodiscard]] scene::ObjectId selectedObject() const noexcept { return selected_object_; }
    void setSelectedObject(scene::ObjectId id) noexcept;

    // Editor Mode (Object / Vertex / Edge / Face) & Mesh Element Selection.
    [[nodiscard]] scene::EditMode editorMode() const noexcept { return editor_mode_; }
    [[nodiscard]] core::Status setEditorMode(scene::EditMode mode);

    [[nodiscard]] scene::MeshElementSelection& meshSelection() noexcept { return mesh_selection_; }
    [[nodiscard]] const scene::MeshElementSelection& meshSelection() const noexcept { return mesh_selection_; }
    void sanitizeMeshSelection() noexcept;

    // 3D Transform Gizmo access & modal drag control.
    [[nodiscard]] TransformGizmo& gizmo() noexcept { return gizmo_; }
    [[nodiscard]] const TransformGizmo& gizmo() const noexcept { return gizmo_; }
    void setGizmoMode(GizmoMode mode) noexcept;
    void setGizmoSpace(GizmoSpace space) noexcept;
    [[nodiscard]] GizmoHandle hoveredGizmoHandle() const noexcept { return gizmo_.hoveredHandle(); }
    [[nodiscard]] bool isGizmoDragging() const noexcept { return gizmo_.isDragging(); }
    [[nodiscard]] core::Status cancelActiveGizmoDrag();
    [[nodiscard]] std::optional<GizmoFrame> computeActiveGizmoFrame() const noexcept;

    // Modular Desktop/Web Workspace Layout Manager.
    [[nodiscard]] WorkspaceManager& workspace() noexcept { return workspace_; }
    [[nodiscard]] const WorkspaceManager& workspace() const noexcept { return workspace_; }
    [[nodiscard]] bool isTextInputFocused() const noexcept { return text_input_focused_; }
    void setTextInputFocused(bool focused) noexcept { text_input_focused_ = focused; }
    void syncDocumentState(scene::MaterialId active_material = scene::MaterialId::invalid()) noexcept;
    void restoreFromDocumentState() noexcept;

    // Explicit input injection helper for programmatic/E2E testing and custom hosts.
    [[nodiscard]] core::Status handleInputEvent(const platform::InputEvent& event) {
        return processInputEvent(event);
    }

    [[nodiscard]] std::uint64_t interactionRevision() const noexcept { return interaction_revision_; }

    [[nodiscard]] bool isRigAnimationEnabled() const noexcept { return config_.animate_showcase_rig; }
    void setRigAnimationEnabled(bool enabled) noexcept { config_.animate_showcase_rig = enabled; }

    [[nodiscard]] project::ProjectDocument& document() noexcept { return document_; }
    [[nodiscard]] const project::ProjectDocument& document() const noexcept { return document_; }

    [[nodiscard]] scene::CommandStack& commandStack() noexcept { return command_stack_; }
    [[nodiscard]] const scene::CommandStack& commandStack() const noexcept { return command_stack_; }

    [[nodiscard]] OrbitCameraController& cameraController() noexcept { return camera_controller_; }
    [[nodiscard]] const OrbitCameraController& cameraController() const noexcept { return camera_controller_; }

    [[nodiscard]] rendering::Renderer& renderer() noexcept { return renderer_; }
    [[nodiscard]] const rendering::Renderer& renderer() const noexcept { return renderer_; }

    [[nodiscard]] core::ResourceStats collectResourceStats() const noexcept;

private:
    enum class DragMode : std::uint8_t {
        None = 0,
        Orbit,
        Pan,
        PendingLeftClick,
        GizmoObject,
        GizmoMeshElements,
    };

    struct ViewportCameraState {
        math::Mat4 camera_world{math::Mat4::identity()};
        scene::CameraComponent camera{};
    };

    [[nodiscard]] ViewportCameraState activeCameraState() const noexcept;
    [[nodiscard]] std::optional<math::Ray> computeViewportRay(float screen_x, float screen_y) const noexcept;
    void syncCameraControllerFromScene() noexcept;
    [[nodiscard]] core::Status applyCameraControllerToScene();
    [[nodiscard]] core::Status processInputEvent(const platform::InputEvent& event);
    [[nodiscard]] core::Status commitActiveGizmoDrag();

    platform::IWindow& window_;
    rendering::Renderer renderer_;
    StudioSessionConfig config_;
    project::ProjectDocument document_{};
    scene::CommandStack command_stack_;
    OrbitCameraController camera_controller_{};
    TransformGizmo gizmo_{};
    WorkspaceManager workspace_{};

    scene::ObjectId selected_object_{};
    scene::EditMode editor_mode_{scene::EditMode::Object};
    scene::MeshElementSelection mesh_selection_{};
    bool text_input_focused_{false};

    DragMode drag_mode_{DragMode::None};
    bool left_click_shift_{false};
    double last_cursor_x_{0.0};
    double last_cursor_y_{0.0};
    double press_cursor_x_{0.0};
    double press_cursor_y_{0.0};
    bool has_cursor_pos_{false};

    // Active gizmo modal drag snapshot (for live updates, clean commit, and safe Escape cancellation).
    scene::ObjectId gizmo_drag_object_id_{};
    math::Transform gizmo_drag_initial_tf_{};
    math::Transform gizmo_drag_latest_tf_{};
    bool gizmo_drag_mutated_{false};

    scene::MeshId gizmo_drag_mesh_id_{};
    scene::EditableMesh gizmo_drag_base_mesh_{};
    scene::EditableMesh gizmo_drag_latest_mesh_{};
    math::Mat4 gizmo_drag_obj_world_{math::Mat4::identity()};

    // Reusable CPU overlay mesh scratch buffers.
    scene::MeshData edit_overlay_mesh_{};
    scene::MeshData gizmo_overlay_mesh_{};

    std::uint64_t interaction_revision_{1};
    bool custom_project_loaded_{false};
    float rig_angle_radians_{0.0f};
};

} // namespace finaleffect::app
