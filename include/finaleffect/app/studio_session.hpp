// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include "finaleffect/app/maya_editor_types.hpp"
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
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace finaleffect::app {

enum class ViewportViewPreset : std::uint8_t {
    Home = 0,
    RightPosX,
    LeftNegX,
    TopPosY,
    BottomNegY,
    FrontPosZ,
    BackNegZ,
    DiagPosXPosYPosZ,
    DiagNegXPosYPosZ,
    DiagPosXPosYNegZ,
    DiagNegXPosYNegZ,
    DiagPosXNegYPosZ,
    DiagNegXNegYPosZ,
    DiagPosXNegYNegZ,
    DiagNegXNegYNegZ,
};

[[nodiscard]] std::optional<ViewportViewPreset> parseViewportViewPreset(std::string_view name) noexcept;
[[nodiscard]] std::string_view viewportViewPresetSlug(ViewportViewPreset preset) noexcept;
[[nodiscard]] std::string_view viewportViewPresetLabel(ViewportViewPreset preset) noexcept;

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
    [[nodiscard]] core::Status frameAllScene();

    // Direct camera & gizmo hit-test helpers for touch/stylus/multi-pointer hosts.
    [[nodiscard]] core::Status orbitViewportCamera(float delta_pixels_x, float delta_pixels_y);
    [[nodiscard]] core::Status panViewportCamera(float delta_pixels_x, float delta_pixels_y);
    [[nodiscard]] core::Status zoomViewportCamera(float wheel_steps);
    [[nodiscard]] bool hitTestGizmoAtScreenPosition(float screen_x, float screen_y) const noexcept;

    [[nodiscard]] scene::ObjectId selectedObject() const noexcept { return selected_object_; }
    [[nodiscard]] std::span<const scene::ObjectId> selectedObjects() const noexcept { return selected_objects_; }
    void setSelectedObject(scene::ObjectId id) noexcept;
    void toggleOrAddSelectedObject(scene::ObjectId id, bool additive) noexcept;
    void setSelectedObjects(std::span<const scene::ObjectId> ids) noexcept;
    void sanitizeMultiSelection() noexcept;

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

    struct ViewportCameraState {
        math::Mat4 camera_world{math::Mat4::identity()};
        scene::CameraComponent camera{};
    };

    // Active viewport camera pose & projection (free editor perspective by default, or active scene camera lens).
    [[nodiscard]] ViewportCameraState activeCameraState() const noexcept;
    [[nodiscard]] std::optional<math::Vec3> projectWorldPointToScreen(
        const math::Vec3& world_point) const noexcept;

    [[nodiscard]] bool isCameraViewActive() const noexcept { return camera_view_active_; }
    void setCameraViewActive(bool active) noexcept;

    // Editor observer camera controls for the 3D View Navigation Gizmo (never mutates scene objects/cameras).
    [[nodiscard]] core::Status orbitEditorObserverCamera(float delta_pixels_x, float delta_pixels_y);
    void setViewportViewPreset(ViewportViewPreset preset, bool smooth = true) noexcept;
    void resetViewportHomeView(bool smooth = true) noexcept;
    [[nodiscard]] scene::ProjectionType viewportProjection() const noexcept {
        return viewport_camera_.projection;
    }
    void setViewportProjection(scene::ProjectionType projection) noexcept;
    void toggleViewportProjection() noexcept;
    [[nodiscard]] bool isViewTransitionActive() const noexcept { return view_transition_.active; }
    [[nodiscard]] std::string_view viewportViewLabel() const noexcept;

    [[nodiscard]] bool areEditorHelpersVisible() const noexcept { return editor_helpers_visible_; }
    void setEditorHelpersVisible(bool visible) noexcept;

    [[nodiscard]] bool isEditorGridVisible() const noexcept { return editor_grid_visible_; }
    void setEditorGridVisible(bool visible) noexcept;

    // Maya Stage 1: Menu Set, Toolbox, Selection Masks, Snapping, Viewport Display, Display Layers & Timeline.
    [[nodiscard]] MayaMenuSet menuSet() const noexcept { return menu_set_; }
    void setMenuSet(MayaMenuSet set) noexcept;

    [[nodiscard]] MayaTool activeTool() const noexcept { return active_tool_; }
    [[nodiscard]] MayaTool lastTool() const noexcept { return last_tool_; }
    void setActiveTool(MayaTool tool) noexcept;
    void activateLastTool() noexcept;

    [[nodiscard]] SelectionTypeMasks& selectionMasks() noexcept { return selection_masks_; }
    [[nodiscard]] const SelectionTypeMasks& selectionMasks() const noexcept { return selection_masks_; }
    void setSelectionMasks(SelectionTypeMasks masks) noexcept;

    [[nodiscard]] SnapSettings& snapSettings() noexcept { return snap_settings_; }
    [[nodiscard]] const SnapSettings& snapSettings() const noexcept { return snap_settings_; }
    void setSnapSettings(SnapSettings snap) noexcept;

    [[nodiscard]] rendering::ViewportShadingMode viewportShadingMode() const noexcept { return shading_mode_; }
    void setViewportShadingMode(rendering::ViewportShadingMode mode) noexcept;
    void applyMayaViewportDisplayKey(int digit_key) noexcept;
    [[nodiscard]] bool isWireframeOnShaded() const noexcept { return wireframe_on_shaded_; }
    void setWireframeOnShaded(bool enabled) noexcept;
    [[nodiscard]] bool isXRayEnabled() const noexcept { return xray_enabled_; }
    void setXRayEnabled(bool enabled) noexcept;
    [[nodiscard]] bool isDefaultLightingActive() const noexcept { return use_default_lighting_; }
    void setDefaultLightingActive(bool enabled) noexcept;
    [[nodiscard]] bool isIsolateSelectActive() const noexcept { return isolate_select_active_; }
    void setIsolateSelectActive(bool active) noexcept;
    void toggleIsolateSelect() noexcept;
    [[nodiscard]] std::uint8_t displaySmoothness() const noexcept { return display_smoothness_; }
    void setDisplaySmoothness(std::uint8_t level) noexcept;

    // Per-object Lock, Channel Lock, Pivot Offset & Custom Channel Attributes.
    [[nodiscard]] const ObjectEditorMeta& objectEditorMeta(scene::ObjectId id) const noexcept;
    [[nodiscard]] ObjectEditorMeta& objectEditorMetaMutable(scene::ObjectId id);
    [[nodiscard]] bool isObjectLocked(scene::ObjectId id) const noexcept;
    void setObjectLocked(scene::ObjectId id, bool locked);
    void setObjectChannelMask(scene::ObjectId id, std::uint16_t mask);
    [[nodiscard]] math::Vec3 objectPivotOffset(scene::ObjectId id) const noexcept;
    void setObjectPivotOffset(scene::ObjectId id, const math::Vec3& offset);
    void centerObjectPivot(scene::ObjectId id);
    void resetObjectPivot(scene::ObjectId id);
    [[nodiscard]] math::Transform applyChannelLocks(
        scene::ObjectId id,
        const math::Transform& previous_tf,
        const math::Transform& candidate_tf) const noexcept;

    // Maya Display Layers (Normal / Template / Reference / Visibility).
    [[nodiscard]] const std::vector<DisplayLayer>& displayLayers() const noexcept { return display_layers_; }
    [[nodiscard]] std::uint32_t activeDisplayLayerId() const noexcept { return active_display_layer_id_; }
    void setActiveDisplayLayerId(std::uint32_t layer_id) noexcept;
    [[nodiscard]] std::uint32_t createDisplayLayer(std::string name = {}, bool assign_selected = true);
    [[nodiscard]] core::Status deleteDisplayLayer(std::uint32_t layer_id);
    [[nodiscard]] core::Status renameDisplayLayer(std::uint32_t layer_id, std::string new_name);
    [[nodiscard]] core::Status setDisplayLayerState(
        std::uint32_t layer_id,
        std::optional<bool> visible,
        std::optional<DisplayLayerMode> mode,
        std::optional<math::Vec3> color = std::nullopt);
    [[nodiscard]] core::Status assignObjectsToDisplayLayer(
        std::uint32_t layer_id,
        std::span<const scene::ObjectId> objects);
    [[nodiscard]] std::uint32_t objectDisplayLayerId(scene::ObjectId id) const noexcept;
    [[nodiscard]] bool isObjectSelectableInViewport(scene::ObjectId id) const noexcept;

    // Real C++20 Animation Timeline & Transform Keyframe Engine.
    [[nodiscard]] const TimelineState& timeline() const noexcept { return timeline_; }
    [[nodiscard]] TimelineState& timelineMutable() noexcept { return timeline_; }
    void setTimelineFrame(double frame);
    void setTimelineRanges(int scene_start, int scene_end, int playback_start, int playback_end) noexcept;
    void setTimelinePlaying(bool playing) noexcept;
    void setTimelineFps(float fps) noexcept;
    void setTimelineLoopMode(PlaybackLoopMode mode) noexcept;
    void setTimelineAutoKey(bool enabled) noexcept;
    void stepTimelineFrames(int delta_frames);
    void jumpToAdjacentKeyframe(bool forward);
    [[nodiscard]] core::Status setKeyframeForObject(scene::ObjectId id, int frame);
    [[nodiscard]] core::Status setKeyframeForSelection();
    [[nodiscard]] core::Status deleteKeyframeForObject(scene::ObjectId id, int frame);
    void recordAutoKeyIfEnabled(scene::ObjectId id);
    [[nodiscard]] std::vector<int> keyframesForObject(scene::ObjectId id) const;
    void resetMayaSessionAuxiliaryState() noexcept;

    // Renders the final/offline scene image from the active scene camera without editor grid,
    // selection highlights, transform gizmos, or camera/light viewport helpers.
    [[nodiscard]] core::Status renderFinalScene(const rendering::FrameClearDesc& clear = {});

    [[nodiscard]] core::ResourceStats collectResourceStats() const noexcept;

private:
    enum class DragMode : std::uint8_t {
        None = 0,
        Orbit,
        Pan,
        Zoom,
        PendingLeftClick,
        GizmoObject,
        GizmoPivot,
        GizmoMeshElements,
    };

    struct ViewTransitionState {
        bool active{false};
        float elapsed_seconds{0.0f};
        float duration_seconds{0.22f};
        float start_yaw{0.0f};
        float goal_yaw{0.0f};
        float start_pitch{0.0f};
        float goal_pitch{0.0f};
        math::Vec3 start_target{0.0f, 0.80f, 0.0f};
        math::Vec3 goal_target{0.0f, 0.80f, 0.0f};
        float start_distance{6.46f};
        float goal_distance{6.46f};
    };

    [[nodiscard]] std::optional<math::Ray> computeViewportRay(float screen_x, float screen_y) const noexcept;
    void resetEditorViewportCamera() noexcept;
    void detachFromActiveSceneCameraForObserverControl() noexcept;
    void syncViewportOrthoHeightFromDistance() noexcept;
    void markUserFreeViewLabel() noexcept;
    void syncCameraControllerFromScene() noexcept;
    [[nodiscard]] core::Status applyCameraControllerToScene();
    [[nodiscard]] core::Status processInputEvent(const platform::InputEvent& event);
    [[nodiscard]] core::Status commitActiveGizmoDrag();
    [[nodiscard]] math::Transform applySnapToTransform(
        scene::ObjectId id,
        const math::Transform& candidate_local_tf,
        const math::Ray& world_ray) const noexcept;
    void evaluateTimelineAtCurrentFrame() noexcept;

    platform::IWindow& window_;
    rendering::Renderer renderer_;
    StudioSessionConfig config_;
    project::ProjectDocument document_{};
    scene::CommandStack command_stack_;
    OrbitCameraController camera_controller_{};
    scene::CameraComponent viewport_camera_{};
    ViewTransitionState view_transition_{};
    std::string viewport_view_label_{"User Persp"};
    bool camera_view_active_{false};
    bool editor_helpers_visible_{true};
    bool editor_grid_visible_{true};
    TransformGizmo gizmo_{};
    WorkspaceManager workspace_{};

    scene::ObjectId selected_object_{};
    std::vector<scene::ObjectId> selected_objects_{};
    scene::EditMode editor_mode_{scene::EditMode::Object};
    scene::EditMode last_component_mode_{scene::EditMode::Face};
    scene::MeshElementSelection mesh_selection_{};
    bool text_input_focused_{false};

    MayaMenuSet menu_set_{MayaMenuSet::Modeling};
    MayaTool active_tool_{MayaTool::Move};
    MayaTool last_tool_{MayaTool::Move};
    SelectionTypeMasks selection_masks_{};
    SnapSettings snap_settings_{};
    rendering::ViewportShadingMode shading_mode_{rendering::ViewportShadingMode::MaterialTextured};
    bool wireframe_on_shaded_{false};
    bool xray_enabled_{false};
    bool use_default_lighting_{false};
    bool isolate_select_active_{false};
    std::vector<scene::ObjectId> isolated_objects_{};
    std::uint8_t display_smoothness_{1};

    std::unordered_map<scene::ObjectId, ObjectEditorMeta> object_meta_{};
    std::vector<DisplayLayer> display_layers_{};
    std::uint32_t next_display_layer_id_{1};
    std::uint32_t active_display_layer_id_{0};
    TimelineState timeline_{};

    math::Vec3 gizmo_drag_initial_pivot_{0.0f, 0.0f, 0.0f};
    std::vector<scene::ObjectId> scratch_hidden_layer_objs_{};
    std::vector<scene::ObjectId> scratch_template_objs_{};

    DragMode drag_mode_{DragMode::None};
    bool left_click_shift_{false};
    bool left_press_near_gizmo_{false};
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
    scene::MeshData scene_helpers_overlay_mesh_{};
    scene::MeshData edit_overlay_mesh_{};
    scene::MeshData gizmo_overlay_mesh_{};

    std::uint64_t interaction_revision_{1};
    std::uint32_t last_fb_width_{0};
    std::uint32_t last_fb_height_{0};
    float last_content_scale_{1.0f};
    bool custom_project_loaded_{false};
    float rig_angle_radians_{0.0f};
};

} // namespace finaleffect::app
