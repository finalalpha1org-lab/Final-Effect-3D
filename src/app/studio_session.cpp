// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/app/studio_session.hpp"
#include "finaleffect/app/starter_scene.hpp"
#include "finaleffect/app/viewport_scene_helpers.hpp"
#include "finaleffect/math/constants.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>
#include <variant>

namespace finaleffect::app {

namespace {

constexpr math::Vec3 kDefaultHomeEye{0.0f, 2.95f, 6.10f};
constexpr math::Vec3 kDefaultHomeTarget{0.0f, 0.80f, 0.0f};
constexpr float kIsoPitchRadians = 0.61547971f; // atan(1 / sqrt(2)) ~= 35.26439 deg
constexpr float kPolePitchRadians = math::degToRad(89.0f);

struct PresetAngles {
    float yaw{0.0f};
    float pitch{-0.339f};
};

PresetAngles computePresetAngles(ViewportViewPreset preset) noexcept {
    switch (preset) {
        case ViewportViewPreset::Home: {
            const math::Vec3 off = kDefaultHomeEye - kDefaultHomeTarget;
            const float horiz = std::sqrt(off.x * off.x + off.z * off.z);
            return {
                .yaw = std::atan2(off.x, off.z),
                .pitch = -std::atan2(off.y, horiz),
            };
        }
        case ViewportViewPreset::RightPosX:
            return {.yaw = math::kHalfPi, .pitch = 0.0f};
        case ViewportViewPreset::LeftNegX:
            return {.yaw = -math::kHalfPi, .pitch = 0.0f};
        case ViewportViewPreset::TopPosY:
            return {.yaw = 0.0f, .pitch = -kPolePitchRadians};
        case ViewportViewPreset::BottomNegY:
            return {.yaw = 0.0f, .pitch = kPolePitchRadians};
        case ViewportViewPreset::FrontPosZ:
            return {.yaw = 0.0f, .pitch = 0.0f};
        case ViewportViewPreset::BackNegZ:
            return {.yaw = math::kPi, .pitch = 0.0f};
        case ViewportViewPreset::DiagPosXPosYPosZ:
            return {.yaw = math::degToRad(45.0f), .pitch = -kIsoPitchRadians};
        case ViewportViewPreset::DiagNegXPosYPosZ:
            return {.yaw = math::degToRad(-45.0f), .pitch = -kIsoPitchRadians};
        case ViewportViewPreset::DiagPosXPosYNegZ:
            return {.yaw = math::degToRad(135.0f), .pitch = -kIsoPitchRadians};
        case ViewportViewPreset::DiagNegXPosYNegZ:
            return {.yaw = math::degToRad(-135.0f), .pitch = -kIsoPitchRadians};
        case ViewportViewPreset::DiagPosXNegYPosZ:
            return {.yaw = math::degToRad(45.0f), .pitch = kIsoPitchRadians};
        case ViewportViewPreset::DiagNegXNegYPosZ:
            return {.yaw = math::degToRad(-45.0f), .pitch = kIsoPitchRadians};
        case ViewportViewPreset::DiagPosXNegYNegZ:
            return {.yaw = math::degToRad(135.0f), .pitch = kIsoPitchRadians};
        case ViewportViewPreset::DiagNegXNegYNegZ:
            return {.yaw = math::degToRad(-135.0f), .pitch = kIsoPitchRadians};
    }
    return {};
}

} // namespace

std::optional<ViewportViewPreset> parseViewportViewPreset(std::string_view name) noexcept {
    if (name == "home" || name == "reset") return ViewportViewPreset::Home;
    if (name == "pos_x" || name == "+x" || name == "right") return ViewportViewPreset::RightPosX;
    if (name == "neg_x" || name == "-x" || name == "left") return ViewportViewPreset::LeftNegX;
    if (name == "pos_y" || name == "+y" || name == "top") return ViewportViewPreset::TopPosY;
    if (name == "neg_y" || name == "-y" || name == "bottom") return ViewportViewPreset::BottomNegY;
    if (name == "pos_z" || name == "+z" || name == "front") return ViewportViewPreset::FrontPosZ;
    if (name == "neg_z" || name == "-z" || name == "back") return ViewportViewPreset::BackNegZ;
    if (name == "diag_px_py_pz") return ViewportViewPreset::DiagPosXPosYPosZ;
    if (name == "diag_nx_py_pz") return ViewportViewPreset::DiagNegXPosYPosZ;
    if (name == "diag_px_py_nz") return ViewportViewPreset::DiagPosXPosYNegZ;
    if (name == "diag_nx_py_nz") return ViewportViewPreset::DiagNegXPosYNegZ;
    if (name == "diag_px_ny_pz") return ViewportViewPreset::DiagPosXNegYPosZ;
    if (name == "diag_nx_ny_pz") return ViewportViewPreset::DiagNegXNegYPosZ;
    if (name == "diag_px_ny_nz") return ViewportViewPreset::DiagPosXNegYNegZ;
    if (name == "diag_nx_ny_nz") return ViewportViewPreset::DiagNegXNegYNegZ;
    return std::nullopt;
}

std::string_view viewportViewPresetSlug(ViewportViewPreset preset) noexcept {
    switch (preset) {
        case ViewportViewPreset::Home: return "home";
        case ViewportViewPreset::RightPosX: return "pos_x";
        case ViewportViewPreset::LeftNegX: return "neg_x";
        case ViewportViewPreset::TopPosY: return "pos_y";
        case ViewportViewPreset::BottomNegY: return "neg_y";
        case ViewportViewPreset::FrontPosZ: return "pos_z";
        case ViewportViewPreset::BackNegZ: return "neg_z";
        case ViewportViewPreset::DiagPosXPosYPosZ: return "diag_px_py_pz";
        case ViewportViewPreset::DiagNegXPosYPosZ: return "diag_nx_py_pz";
        case ViewportViewPreset::DiagPosXPosYNegZ: return "diag_px_py_nz";
        case ViewportViewPreset::DiagNegXPosYNegZ: return "diag_nx_py_nz";
        case ViewportViewPreset::DiagPosXNegYPosZ: return "diag_px_ny_pz";
        case ViewportViewPreset::DiagNegXNegYPosZ: return "diag_nx_ny_pz";
        case ViewportViewPreset::DiagPosXNegYNegZ: return "diag_px_ny_nz";
        case ViewportViewPreset::DiagNegXNegYNegZ: return "diag_nx_ny_nz";
    }
    return "home";
}

std::string_view viewportViewPresetLabel(ViewportViewPreset preset) noexcept {
    switch (preset) {
        case ViewportViewPreset::Home: return "User Persp";
        case ViewportViewPreset::RightPosX: return "Right (+X)";
        case ViewportViewPreset::LeftNegX: return "Left (-X)";
        case ViewportViewPreset::TopPosY: return "Top (+Y)";
        case ViewportViewPreset::BottomNegY: return "Bottom (-Y)";
        case ViewportViewPreset::FrontPosZ: return "Front (+Z)";
        case ViewportViewPreset::BackNegZ: return "Back (-Z)";
        case ViewportViewPreset::DiagPosXPosYPosZ: return "Iso (+X+Y+Z)";
        case ViewportViewPreset::DiagNegXPosYPosZ: return "Iso (-X+Y+Z)";
        case ViewportViewPreset::DiagPosXPosYNegZ: return "Iso (+X+Y-Z)";
        case ViewportViewPreset::DiagNegXPosYNegZ: return "Iso (-X+Y-Z)";
        case ViewportViewPreset::DiagPosXNegYPosZ: return "Iso (+X-Y+Z)";
        case ViewportViewPreset::DiagNegXNegYPosZ: return "Iso (-X-Y+Z)";
        case ViewportViewPreset::DiagPosXNegYNegZ: return "Iso (+X-Y-Z)";
        case ViewportViewPreset::DiagNegXNegYNegZ: return "Iso (-X-Y-Z)";
    }
    return "User Persp";
}

StudioSession::StudioSession(
    platform::IWindow& window,
    std::unique_ptr<rendering::IRenderDevice> render_device,
    StudioSessionConfig config)
    : window_(window),
      renderer_(std::move(render_device)),
      config_(config),
      command_stack_(scene::CommandStackLimits{
          .max_entries = 128,
          .max_memory_bytes = config.command_history_max_bytes,
      }) {
    viewport_camera_.projection = scene::ProjectionType::Perspective;
    viewport_camera_.fov_y_radians = math::degToRad(48.0f);
    viewport_camera_.near_plane = 0.1f;
    viewport_camera_.far_plane = 250.0f;
    resetMayaSessionAuxiliaryState();
    resetEditorViewportCamera();
}

void StudioSession::resetMayaSessionAuxiliaryState() noexcept {
    selected_objects_.clear();
    if (selected_object_.isValid()) {
        selected_objects_.push_back(selected_object_);
    }
    object_meta_.clear();
    display_layers_.clear();
    display_layers_.push_back(DisplayLayer{
        .id = 0,
        .name = "defaultLayer",
        .visible = true,
        .mode = DisplayLayerMode::Normal,
        .color = {0.34f, 0.56f, 0.84f},
        .members = {},
    });
    next_display_layer_id_ = 1;
    active_display_layer_id_ = 0;
    isolate_select_active_ = false;
    isolated_objects_.clear();
    timeline_ = TimelineState{};
}

void StudioSession::syncDocumentState(scene::MaterialId active_material) noexcept {
    document_.editor_state.edit_mode = editor_mode_;
    document_.editor_state.gizmo_tool = std::string(gizmoModeName(gizmo_.mode()));
    document_.editor_state.gizmo_space = std::string(gizmoSpaceName(gizmo_.space()));
    document_.editor_state.selected_object_id = selected_object_;
    if (active_material.isValid()) {
        document_.editor_state.active_material_id = active_material;
    }
    document_.editor_state.mesh_selection = mesh_selection_;
    document_.workspace = workspace_.state();
}

void StudioSession::restoreFromDocumentState() noexcept {
    selected_object_ = document_.editor_state.selected_object_id;
    if (selected_object_.isValid() && document_.scene.findObject(selected_object_) == nullptr) {
        selected_object_ = {};
    }
    editor_mode_ = document_.editor_state.edit_mode;
    if (editor_mode_ != scene::EditMode::Object) {
        const auto* obj = selected_object_.isValid() ? document_.scene.findObject(selected_object_) : nullptr;
        const auto* ma = (obj != nullptr && obj->mesh_id.has_value())
                             ? document_.scene.findMesh(obj->mesh_id.value())
                             : nullptr;
        if (ma == nullptr || !ma->editable_mesh.has_value()) {
            editor_mode_ = scene::EditMode::Object;
        }
    }
    if (document_.editor_state.gizmo_tool == "rotate") {
        gizmo_.setMode(GizmoMode::Rotate);
        active_tool_ = MayaTool::Rotate;
        last_tool_ = MayaTool::Rotate;
    } else if (document_.editor_state.gizmo_tool == "scale") {
        gizmo_.setMode(GizmoMode::Scale);
        active_tool_ = MayaTool::Scale;
        last_tool_ = MayaTool::Scale;
    } else {
        gizmo_.setMode(GizmoMode::Translate);
        active_tool_ = MayaTool::Move;
        last_tool_ = MayaTool::Move;
    }
    gizmo_.setSpace(
        document_.editor_state.gizmo_space == "local" ? GizmoSpace::Local : GizmoSpace::World);
    selected_objects_.clear();
    if (selected_object_.isValid()) {
        selected_objects_.push_back(selected_object_);
    }
    mesh_selection_ = document_.editor_state.mesh_selection;
    sanitizeMeshSelection();
    workspace_.setState(document_.workspace);
}

void StudioSession::sanitizeMultiSelection() noexcept {
    std::erase_if(selected_objects_, [this](scene::ObjectId oid) {
        return !oid.isValid() || document_.scene.findObject(oid) == nullptr;
    });
    if (selected_object_.isValid() && document_.scene.findObject(selected_object_) == nullptr) {
        selected_object_ = selected_objects_.empty() ? scene::ObjectId{} : selected_objects_.back();
    }
    if (selected_object_.isValid() &&
        std::find(selected_objects_.begin(), selected_objects_.end(), selected_object_) ==
            selected_objects_.end()) {
        selected_objects_.push_back(selected_object_);
    }
}

void StudioSession::setSelectedObject(scene::ObjectId id) noexcept {
    const bool single_matches =
        (selected_object_ == id) &&
        ((id.isValid() && selected_objects_.size() == 1 && selected_objects_.front() == id) ||
         (!id.isValid() && selected_objects_.empty()));
    if (!single_matches) {
        if (gizmo_.isDragging()) {
            (void)cancelActiveGizmoDrag();
        }
        selected_object_ = id;
        selected_objects_.clear();
        if (id.isValid()) {
            selected_objects_.push_back(id);
        }
        mesh_selection_.clear();
        if (editor_mode_ != scene::EditMode::Object) {
            const auto* obj = id.isValid() ? document_.scene.findObject(id) : nullptr;
            const auto* ma = (obj != nullptr && obj->mesh_id.has_value())
                                 ? document_.scene.findMesh(obj->mesh_id.value())
                                 : nullptr;
            if (ma == nullptr || !ma->editable_mesh.has_value()) {
                editor_mode_ = scene::EditMode::Object;
            }
        }
        gizmo_.setHoveredHandle(GizmoHandle::None);
        syncDocumentState();
        ++interaction_revision_;
    }
}

void StudioSession::toggleOrAddSelectedObject(scene::ObjectId id, bool additive) noexcept {
    if (!additive) {
        setSelectedObject(id);
        return;
    }
    if (!id.isValid() || document_.scene.findObject(id) == nullptr) {
        return;
    }
    if (gizmo_.isDragging()) {
        (void)cancelActiveGizmoDrag();
    }
    sanitizeMultiSelection();
    auto it = std::find(selected_objects_.begin(), selected_objects_.end(), id);
    if (it != selected_objects_.end()) {
        selected_objects_.erase(it);
        selected_object_ = selected_objects_.empty() ? scene::ObjectId{} : selected_objects_.back();
    } else {
        selected_objects_.push_back(id);
        selected_object_ = id;
    }
    if (selected_objects_.size() > 1 && editor_mode_ != scene::EditMode::Object) {
        editor_mode_ = scene::EditMode::Object;
        mesh_selection_.clear();
    }
    gizmo_.setHoveredHandle(GizmoHandle::None);
    syncDocumentState();
    ++interaction_revision_;
}

void StudioSession::setSelectedObjects(std::span<const scene::ObjectId> ids) noexcept {
    if (gizmo_.isDragging()) {
        (void)cancelActiveGizmoDrag();
    }
    selected_objects_.clear();
    for (const scene::ObjectId id : ids) {
        if (!id.isValid() || document_.scene.findObject(id) == nullptr) {
            continue;
        }
        if (std::find(selected_objects_.begin(), selected_objects_.end(), id) == selected_objects_.end()) {
            selected_objects_.push_back(id);
        }
    }
    selected_object_ = selected_objects_.empty() ? scene::ObjectId{} : selected_objects_.back();
    if (selected_objects_.size() != 1 && editor_mode_ != scene::EditMode::Object) {
        editor_mode_ = scene::EditMode::Object;
    }
    mesh_selection_.clear();
    gizmo_.setHoveredHandle(GizmoHandle::None);
    syncDocumentState();
    ++interaction_revision_;
}

core::Status StudioSession::setEditorMode(scene::EditMode mode) {
    if (gizmo_.isDragging()) {
        (void)cancelActiveGizmoDrag();
    }
    if (mode != scene::EditMode::Object) {
        if (!selected_object_.isValid()) {
            return core::Status::fail(
                core::ErrorCode::InvalidState,
                "Select an editable mesh object before entering Mesh Edit mode");
        }
        const auto* obj = document_.scene.findObject(selected_object_);
        if (obj == nullptr || !obj->mesh_id.has_value()) {
            return core::Status::fail(
                core::ErrorCode::InvalidState,
                "Selected object does not have a mesh asset");
        }
        const auto* mesh_asset = document_.scene.findMesh(obj->mesh_id.value());
        if (mesh_asset == nullptr || !mesh_asset->editable_mesh.has_value()) {
            return core::Status::fail(
                core::ErrorCode::InvalidState,
                "Selected object mesh is not an editable polygon mesh");
        }
        config_.animate_showcase_rig = false;
    }

    if (mode != scene::EditMode::Object) {
        last_component_mode_ = mode;
    }
    editor_mode_ = mode;
    mesh_selection_.clear();
    gizmo_.setHoveredHandle(GizmoHandle::None);
    syncDocumentState();
    ++interaction_revision_;
    return core::Status::ok();
}

void StudioSession::sanitizeMeshSelection() noexcept {
    if (editor_mode_ == scene::EditMode::Object || !selected_object_.isValid()) {
        mesh_selection_.clear();
        return;
    }
    const auto* obj = document_.scene.findObject(selected_object_);
    const auto* ma = (obj != nullptr && obj->mesh_id.has_value())
                         ? document_.scene.findMesh(obj->mesh_id.value())
                         : nullptr;
    if (ma == nullptr || !ma->editable_mesh.has_value()) {
        mesh_selection_.clear();
        editor_mode_ = scene::EditMode::Object;
        return;
    }
    const auto& em = ma->editable_mesh.value();
    mesh_selection_.sanitizeForCounts(em.vertexCount(), em.edgeCount(), em.faceCount());
    syncDocumentState();
}

void StudioSession::setGizmoMode(GizmoMode mode) noexcept {
    if (gizmo_.isDragging()) {
        (void)cancelActiveGizmoDrag();
    }
    gizmo_.setMode(mode);
    switch (mode) {
        case GizmoMode::Translate:
            active_tool_ = MayaTool::Move;
            last_tool_ = MayaTool::Move;
            break;
        case GizmoMode::Rotate:
            active_tool_ = MayaTool::Rotate;
            last_tool_ = MayaTool::Rotate;
            break;
        case GizmoMode::Scale:
            active_tool_ = MayaTool::Scale;
            last_tool_ = MayaTool::Scale;
            break;
    }
    gizmo_.setHoveredHandle(GizmoHandle::None);
    syncDocumentState();
    ++interaction_revision_;
}

void StudioSession::setMenuSet(MayaMenuSet set) noexcept {
    if (menu_set_ != set) {
        menu_set_ = set;
        ++interaction_revision_;
    }
}

void StudioSession::setActiveTool(MayaTool tool) noexcept {
    if (gizmo_.isDragging()) {
        (void)cancelActiveGizmoDrag();
    }
    active_tool_ = tool;
    switch (tool) {
        case MayaTool::Select:
            break;
        case MayaTool::Move:
            gizmo_.setMode(GizmoMode::Translate);
            last_tool_ = MayaTool::Move;
            break;
        case MayaTool::Rotate:
            gizmo_.setMode(GizmoMode::Rotate);
            last_tool_ = MayaTool::Rotate;
            break;
        case MayaTool::Scale:
            gizmo_.setMode(GizmoMode::Scale);
            last_tool_ = MayaTool::Scale;
            break;
        case MayaTool::ShowManipulator: {
            const auto* obj = selected_object_.isValid() ? document_.scene.findObject(selected_object_) : nullptr;
            if (obj != nullptr && (obj->hasAnyLight() || obj->camera.has_value())) {
                gizmo_.setMode(GizmoMode::Rotate);
            } else {
                gizmo_.setMode(GizmoMode::Translate);
            }
            last_tool_ = MayaTool::ShowManipulator;
            break;
        }
        case MayaTool::Universal:
            gizmo_.setMode(GizmoMode::Translate);
            last_tool_ = MayaTool::Universal;
            break;
        case MayaTool::PivotEdit:
            gizmo_.setMode(GizmoMode::Translate);
            break;
    }
    gizmo_.setHoveredHandle(GizmoHandle::None);
    syncDocumentState();
    ++interaction_revision_;
}

void StudioSession::activateLastTool() noexcept {
    setActiveTool(last_tool_);
}

void StudioSession::setSelectionMasks(SelectionTypeMasks masks) noexcept {
    selection_masks_ = masks;
    ++interaction_revision_;
}

void StudioSession::setSnapSettings(SnapSettings snap) noexcept {
    snap.grid_step = std::clamp(snap.grid_step, 0.01f, 100.0f);
    snap.rotate_step_deg = std::clamp(snap.rotate_step_deg, 0.5f, 180.0f);
    snap.scale_step = std::clamp(snap.scale_step, 0.01f, 10.0f);
    snap_settings_ = snap;
    ++interaction_revision_;
}

void StudioSession::setViewportShadingMode(rendering::ViewportShadingMode mode) noexcept {
    if (shading_mode_ != mode) {
        shading_mode_ = mode;
        ++interaction_revision_;
    }
}

void StudioSession::applyMayaViewportDisplayKey(int digit_key) noexcept {
    if (digit_key == 4) {
        shading_mode_ = rendering::ViewportShadingMode::Wireframe;
        ++interaction_revision_;
    } else if (digit_key == 5) {
        shading_mode_ = rendering::ViewportShadingMode::SmoothShaded;
        use_default_lighting_ = true;
        ++interaction_revision_;
    } else if (digit_key == 6) {
        shading_mode_ = rendering::ViewportShadingMode::MaterialTextured;
        use_default_lighting_ = true;
        ++interaction_revision_;
    } else if (digit_key == 7) {
        if (shading_mode_ == rendering::ViewportShadingMode::Wireframe) {
            shading_mode_ = rendering::ViewportShadingMode::MaterialTextured;
        }
        use_default_lighting_ = false;
        ++interaction_revision_;
    }
}

void StudioSession::setWireframeOnShaded(bool enabled) noexcept {
    if (wireframe_on_shaded_ != enabled) {
        wireframe_on_shaded_ = enabled;
        ++interaction_revision_;
    }
}

void StudioSession::setXRayEnabled(bool enabled) noexcept {
    if (xray_enabled_ != enabled) {
        xray_enabled_ = enabled;
        ++interaction_revision_;
    }
}

void StudioSession::setDefaultLightingActive(bool enabled) noexcept {
    if (use_default_lighting_ != enabled) {
        use_default_lighting_ = enabled;
        ++interaction_revision_;
    }
}

void StudioSession::setIsolateSelectActive(bool active) noexcept {
    if (isolate_select_active_ == active && (!active || !isolated_objects_.empty())) {
        return;
    }
    isolate_select_active_ = active;
    isolated_objects_.clear();
    if (active) {
        sanitizeMultiSelection();
        if (!selected_objects_.empty()) {
            isolated_objects_ = selected_objects_;
        } else if (selected_object_.isValid()) {
            isolated_objects_.push_back(selected_object_);
        } else {
            isolate_select_active_ = false;
        }
    }
    ++interaction_revision_;
}

void StudioSession::toggleIsolateSelect() noexcept {
    setIsolateSelectActive(!isolate_select_active_);
}

void StudioSession::setDisplaySmoothness(std::uint8_t level) noexcept {
    const std::uint8_t clamped = std::clamp<std::uint8_t>(level, 1, 3);
    display_smoothness_ = clamped;
    if (selected_object_.isValid()) {
        const auto* obj = document_.scene.findObject(selected_object_);
        const auto* ma = (obj != nullptr && obj->mesh_id.has_value())
                             ? document_.scene.findMesh(obj->mesh_id.value())
                             : nullptr;
        if (ma != nullptr && ma->editable_mesh.has_value()) {
            scene::EditableMesh updated = ma->editable_mesh.value();
            updated.setSmoothShading(clamped >= 2);
            (void)document_.scene.updateMeshEditable(obj->mesh_id.value(), std::move(updated));
        }
    }
    if (clamped == 2) {
        wireframe_on_shaded_ = true;
    } else if (clamped == 3) {
        wireframe_on_shaded_ = false;
    }
    ++interaction_revision_;
}

const ObjectEditorMeta& StudioSession::objectEditorMeta(scene::ObjectId id) const noexcept {
    static const ObjectEditorMeta kDefaultMeta{};
    const auto it = object_meta_.find(id);
    return (it != object_meta_.end()) ? it->second : kDefaultMeta;
}

ObjectEditorMeta& StudioSession::objectEditorMetaMutable(scene::ObjectId id) {
    return object_meta_[id];
}

bool StudioSession::isObjectLocked(scene::ObjectId id) const noexcept {
    return objectEditorMeta(id).locked;
}

void StudioSession::setObjectLocked(scene::ObjectId id, bool locked) {
    if (!id.isValid()) {
        return;
    }
    object_meta_[id].locked = locked;
    ++interaction_revision_;
}

void StudioSession::setObjectChannelMask(scene::ObjectId id, std::uint16_t mask) {
    if (!id.isValid()) {
        return;
    }
    object_meta_[id].locked_channels = mask;
    ++interaction_revision_;
}

math::Vec3 StudioSession::objectPivotOffset(scene::ObjectId id) const noexcept {
    return objectEditorMeta(id).pivot_offset;
}

void StudioSession::setObjectPivotOffset(scene::ObjectId id, const math::Vec3& offset) {
    if (!id.isValid()) {
        return;
    }
    object_meta_[id].pivot_offset = offset;
    ++interaction_revision_;
}

void StudioSession::centerObjectPivot(scene::ObjectId id) {
    const auto* obj = document_.scene.findObject(id);
    if (obj == nullptr) {
        return;
    }
    math::Vec3 center = math::Vec3::zero();
    if (obj->mesh_id.has_value()) {
        if (const auto* ma = document_.scene.findMesh(obj->mesh_id.value());
            ma != nullptr && ma->local_bounds.isValid()) {
            center = ma->local_bounds.center();
        }
    }
    object_meta_[id].pivot_offset = center;
    ++interaction_revision_;
}

void StudioSession::resetObjectPivot(scene::ObjectId id) {
    if (!id.isValid()) {
        return;
    }
    object_meta_[id].pivot_offset = math::Vec3::zero();
    ++interaction_revision_;
}

math::Transform StudioSession::applyChannelLocks(
    scene::ObjectId id,
    const math::Transform& previous_tf,
    const math::Transform& candidate_tf) const noexcept {
    const auto& meta = objectEditorMeta(id);
    if (meta.locked) {
        return previous_tf;
    }
    if (meta.locked_channels == 0) {
        return candidate_tf;
    }
    math::Transform out = candidate_tf;
    if (meta.isChannelLocked(0)) out.position.x = previous_tf.position.x;
    if (meta.isChannelLocked(1)) out.position.y = previous_tf.position.y;
    if (meta.isChannelLocked(2)) out.position.z = previous_tf.position.z;

    if (meta.isChannelLocked(3) || meta.isChannelLocked(4) || meta.isChannelLocked(5)) {
        const math::Vec3 prev_e = previous_tf.rotation.toEulerXYZ();
        math::Vec3 cand_e = candidate_tf.rotation.toEulerXYZ();
        if (meta.isChannelLocked(3)) cand_e.x = prev_e.x;
        if (meta.isChannelLocked(4)) cand_e.y = prev_e.y;
        if (meta.isChannelLocked(5)) cand_e.z = prev_e.z;
        out.rotation = math::Quat::fromEulerXYZ(cand_e);
    }

    if (meta.isChannelLocked(6)) out.scale.x = previous_tf.scale.x;
    if (meta.isChannelLocked(7)) out.scale.y = previous_tf.scale.y;
    if (meta.isChannelLocked(8)) out.scale.z = previous_tf.scale.z;
    return out;
}

void StudioSession::setActiveDisplayLayerId(std::uint32_t layer_id) noexcept {
    for (const auto& layer : display_layers_) {
        if (layer.id == layer_id) {
            active_display_layer_id_ = layer_id;
            ++interaction_revision_;
            return;
        }
    }
}

std::uint32_t StudioSession::createDisplayLayer(std::string name, bool assign_selected) {
    const std::uint32_t lid = next_display_layer_id_++;
    if (name.empty()) {
        name = "layer" + std::to_string(lid);
    }
    static constexpr std::array<math::Vec3, 6> kPalette{{
        {0.34f, 0.56f, 0.84f},
        {0.82f, 0.44f, 0.30f},
        {0.36f, 0.74f, 0.48f},
        {0.76f, 0.64f, 0.28f},
        {0.64f, 0.44f, 0.82f},
        {0.30f, 0.74f, 0.76f},
    }};
    DisplayLayer layer{
        .id = lid,
        .name = std::move(name),
        .visible = true,
        .mode = DisplayLayerMode::Normal,
        .color = kPalette[lid % kPalette.size()],
        .members = {},
    };
    display_layers_.push_back(std::move(layer));
    active_display_layer_id_ = lid;
    if (assign_selected) {
        sanitizeMultiSelection();
        if (!selected_objects_.empty()) {
            (void)assignObjectsToDisplayLayer(lid, selected_objects_);
        }
    }
    ++interaction_revision_;
    return lid;
}

core::Status StudioSession::deleteDisplayLayer(std::uint32_t layer_id) {
    if (layer_id == 0) {
        return core::Status::fail(core::ErrorCode::InvalidArgument, "defaultLayer cannot be deleted");
    }
    const auto it = std::find_if(display_layers_.begin(), display_layers_.end(), [layer_id](const auto& l) {
        return l.id == layer_id;
    });
    if (it == display_layers_.end()) {
        return core::Status::fail(core::ErrorCode::NotFound, "Display layer not found");
    }
    display_layers_.erase(it);
    if (active_display_layer_id_ == layer_id) {
        active_display_layer_id_ = 0;
    }
    ++interaction_revision_;
    return core::Status::ok();
}

core::Status StudioSession::renameDisplayLayer(std::uint32_t layer_id, std::string new_name) {
    if (layer_id == 0) {
        return core::Status::fail(core::ErrorCode::InvalidArgument, "defaultLayer cannot be renamed");
    }
    if (new_name.empty()) {
        return core::Status::fail(core::ErrorCode::InvalidArgument, "Display layer name cannot be empty");
    }
    for (auto& layer : display_layers_) {
        if (layer.id == layer_id) {
            layer.name = std::move(new_name);
            ++interaction_revision_;
            return core::Status::ok();
        }
    }
    return core::Status::fail(core::ErrorCode::NotFound, "Display layer not found");
}

core::Status StudioSession::setDisplayLayerState(
    std::uint32_t layer_id,
    std::optional<bool> visible,
    std::optional<DisplayLayerMode> mode,
    std::optional<math::Vec3> color) {
    for (auto& layer : display_layers_) {
        if (layer.id == layer_id) {
            if (visible.has_value()) layer.visible = visible.value();
            if (mode.has_value()) layer.mode = mode.value();
            if (color.has_value()) layer.color = color.value();
            ++interaction_revision_;
            return core::Status::ok();
        }
    }
    return core::Status::fail(core::ErrorCode::NotFound, "Display layer not found");
}

core::Status StudioSession::assignObjectsToDisplayLayer(
    std::uint32_t layer_id,
    std::span<const scene::ObjectId> objects) {
    DisplayLayer* target = nullptr;
    for (auto& layer : display_layers_) {
        if (layer.id == layer_id) {
            target = &layer;
            break;
        }
    }
    if (target == nullptr) {
        return core::Status::fail(core::ErrorCode::NotFound, "Display layer not found");
    }
    for (const scene::ObjectId oid : objects) {
        if (!oid.isValid() || document_.scene.findObject(oid) == nullptr) {
            continue;
        }
        for (auto& layer : display_layers_) {
            std::erase(layer.members, oid);
        }
        if (layer_id != 0) {
            target->members.push_back(oid);
        }
    }
    ++interaction_revision_;
    return core::Status::ok();
}

std::uint32_t StudioSession::objectDisplayLayerId(scene::ObjectId id) const noexcept {
    for (const auto& layer : display_layers_) {
        if (layer.id == 0) {
            continue;
        }
        if (std::find(layer.members.begin(), layer.members.end(), id) != layer.members.end()) {
            return layer.id;
        }
    }
    return 0;
}

bool StudioSession::isObjectSelectableInViewport(scene::ObjectId id) const noexcept {
    const auto* obj = document_.scene.findObject(id);
    if (obj == nullptr) {
        return false;
    }
    if (isObjectLocked(id)) {
        return false;
    }
    if (isolate_select_active_ && !isolated_objects_.empty()) {
        if (std::find(isolated_objects_.begin(), isolated_objects_.end(), id) == isolated_objects_.end()) {
            return false;
        }
    }
    const std::uint32_t lid = objectDisplayLayerId(id);
    if (lid != 0) {
        for (const auto& layer : display_layers_) {
            if (layer.id == lid) {
                if (!layer.visible || layer.mode != DisplayLayerMode::Normal) {
                    return false;
                }
                break;
            }
        }
    }
    if (obj->mesh_id.has_value() && !selection_masks_.meshes) {
        return false;
    }
    if (obj->camera.has_value() && !selection_masks_.cameras) {
        return false;
    }
    if (obj->hasAnyLight() && !selection_masks_.lights) {
        return false;
    }
    if (!obj->mesh_id.has_value() && !obj->camera.has_value() &&
        !obj->hasAnyLight() && !selection_masks_.helpers) {
        return false;
    }
    return true;
}

void StudioSession::evaluateTimelineAtCurrentFrame() noexcept {
    const double f = timeline_.current_frame;
    for (const auto& track : timeline_.tracks) {
        if (track.keys.empty() || document_.scene.findObject(track.object_id) == nullptr) {
            continue;
        }
        math::Transform evaluated{};
        if (f <= static_cast<double>(track.keys.front().frame) || track.keys.size() == 1) {
            evaluated = track.keys.front().transform;
        } else if (f >= static_cast<double>(track.keys.back().frame)) {
            evaluated = track.keys.back().transform;
        } else {
            for (std::size_t i = 0; i + 1 < track.keys.size(); ++i) {
                const auto& k0 = track.keys[i];
                const auto& k1 = track.keys[i + 1];
                if (f >= static_cast<double>(k0.frame) && f <= static_cast<double>(k1.frame)) {
                    const double span = static_cast<double>(k1.frame - k0.frame);
                    const float t = (span > 1.0e-6)
                                        ? static_cast<float>((f - static_cast<double>(k0.frame)) / span)
                                        : 0.0f;
                    evaluated.position = k0.transform.position +
                                         (k1.transform.position - k0.transform.position) * t;
                    evaluated.rotation = math::Quat::slerp(k0.transform.rotation, k1.transform.rotation, t);
                    evaluated.scale = k0.transform.scale +
                                      (k1.transform.scale - k0.transform.scale) * t;
                    break;
                }
            }
        }
        (void)document_.scene.setObjectTransform(track.object_id, evaluated);
    }
}

void StudioSession::setTimelineFrame(double frame) {
    timeline_.current_frame = std::clamp(
        frame,
        static_cast<double>(timeline_.scene_start),
        static_cast<double>(timeline_.scene_end));
    evaluateTimelineAtCurrentFrame();
    ++interaction_revision_;
}

void StudioSession::setTimelineRanges(
    int scene_start,
    int scene_end,
    int playback_start,
    int playback_end) noexcept {
    timeline_.scene_start = std::min(scene_start, scene_end - 1);
    timeline_.scene_end = std::max(scene_end, timeline_.scene_start + 1);
    timeline_.playback_start = std::clamp(playback_start, timeline_.scene_start, timeline_.scene_end - 1);
    timeline_.playback_end = std::clamp(playback_end, timeline_.playback_start + 1, timeline_.scene_end);
    timeline_.current_frame = std::clamp(
        timeline_.current_frame,
        static_cast<double>(timeline_.scene_start),
        static_cast<double>(timeline_.scene_end));
    ++interaction_revision_;
}

void StudioSession::setTimelinePlaying(bool playing) noexcept {
    if (timeline_.playing != playing) {
        timeline_.playing = playing;
        if (playing) {
            timeline_.playback_direction = 1;
            if (timeline_.current_frame >= static_cast<double>(timeline_.playback_end) &&
                timeline_.loop_mode == PlaybackLoopMode::Once) {
                timeline_.current_frame = static_cast<double>(timeline_.playback_start);
            }
        }
        ++interaction_revision_;
    }
}

void StudioSession::setTimelineFps(float fps) noexcept {
    timeline_.fps = std::clamp(fps, 1.0f, 240.0f);
    ++interaction_revision_;
}

void StudioSession::setTimelineLoopMode(PlaybackLoopMode mode) noexcept {
    timeline_.loop_mode = mode;
    ++interaction_revision_;
}

void StudioSession::setTimelineAutoKey(bool enabled) noexcept {
    timeline_.auto_key = enabled;
    ++interaction_revision_;
}

void StudioSession::stepTimelineFrames(int delta_frames) {
    timeline_.playing = false;
    const int target = static_cast<int>(std::llround(timeline_.current_frame)) + delta_frames;
    setTimelineFrame(static_cast<double>(target));
}

void StudioSession::jumpToAdjacentKeyframe(bool forward) {
    timeline_.playing = false;
    const int cur = static_cast<int>(std::llround(timeline_.current_frame));
    const auto keys = keyframesForObject(selected_object_);
    if (keys.empty()) {
        setTimelineFrame(static_cast<double>(forward ? timeline_.playback_end : timeline_.playback_start));
        return;
    }
    if (forward) {
        for (const int k : keys) {
            if (k > cur) {
                setTimelineFrame(static_cast<double>(k));
                return;
            }
        }
        setTimelineFrame(static_cast<double>(keys.front()));
    } else {
        for (auto it = keys.rbegin(); it != keys.rend(); ++it) {
            if (*it < cur) {
                setTimelineFrame(static_cast<double>(*it));
                return;
            }
        }
        setTimelineFrame(static_cast<double>(keys.back()));
    }
}

core::Status StudioSession::setKeyframeForObject(scene::ObjectId id, int frame) {
    const auto* obj = document_.scene.findObject(id);
    if (obj == nullptr) {
        return core::Status::fail(core::ErrorCode::NotFound, "Select an object to set a keyframe");
    }
    const int clamped_frame = std::clamp(frame, timeline_.scene_start, timeline_.scene_end);
    ObjectAnimationTrack* track_ptr = nullptr;
    for (auto& track : timeline_.tracks) {
        if (track.object_id == id) {
            track_ptr = &track;
            break;
        }
    }
    if (track_ptr == nullptr) {
        timeline_.tracks.push_back(ObjectAnimationTrack{.object_id = id, .keys = {}});
        track_ptr = &timeline_.tracks.back();
    }
    auto it = std::find_if(track_ptr->keys.begin(), track_ptr->keys.end(), [clamped_frame](const auto& k) {
        return k.frame == clamped_frame;
    });
    if (it != track_ptr->keys.end()) {
        it->transform = obj->local_transform;
    } else {
        track_ptr->keys.push_back(TransformKeyframe{
            .frame = clamped_frame,
            .transform = obj->local_transform,
        });
        std::sort(track_ptr->keys.begin(), track_ptr->keys.end(), [](const auto& a, const auto& b) {
            return a.frame < b.frame;
        });
    }
    ++interaction_revision_;
    return core::Status::ok();
}

core::Status StudioSession::setKeyframeForSelection() {
    const int frame = static_cast<int>(std::llround(timeline_.current_frame));
    sanitizeMultiSelection();
    if (selected_objects_.empty()) {
        if (selected_object_.isValid()) {
            return setKeyframeForObject(selected_object_, frame);
        }
        return core::Status::fail(core::ErrorCode::InvalidState, "Select an object to set a keyframe");
    }
    core::Status last_st = core::Status::ok();
    for (const scene::ObjectId id : selected_objects_) {
        last_st = setKeyframeForObject(id, frame);
    }
    return last_st;
}

core::Status StudioSession::deleteKeyframeForObject(scene::ObjectId id, int frame) {
    for (auto it = timeline_.tracks.begin(); it != timeline_.tracks.end(); ++it) {
        if (it->object_id == id) {
            const auto erased = std::erase_if(it->keys, [frame](const auto& k) { return k.frame == frame; });
            if (it->keys.empty()) {
                timeline_.tracks.erase(it);
            }
            if (erased > 0) {
                evaluateTimelineAtCurrentFrame();
                ++interaction_revision_;
                return core::Status::ok();
            }
            break;
        }
    }
    return core::Status::fail(core::ErrorCode::NotFound, "No keyframe at specified frame");
}

void StudioSession::recordAutoKeyIfEnabled(scene::ObjectId id) {
    if (!timeline_.auto_key || !id.isValid()) {
        return;
    }
    for (const auto& track : timeline_.tracks) {
        if (track.object_id == id && !track.keys.empty()) {
            const int frame = static_cast<int>(std::llround(timeline_.current_frame));
            (void)setKeyframeForObject(id, frame);
            return;
        }
    }
    // In Maya Auto Key, if Auto Key is enabled and user transforms an object, also record if track has keys
    // or create an initial key at frame 1 + current frame so scrubbing immediately animates!
    const int frame = static_cast<int>(std::llround(timeline_.current_frame));
    (void)setKeyframeForObject(id, frame);
}

std::vector<int> StudioSession::keyframesForObject(scene::ObjectId id) const {
    std::vector<int> frames;
    if (!id.isValid()) {
        return frames;
    }
    for (const auto& track : timeline_.tracks) {
        if (track.object_id == id) {
            frames.reserve(track.keys.size());
            for (const auto& k : track.keys) {
                frames.push_back(k.frame);
            }
            break;
        }
    }
    return frames;
}

void StudioSession::setGizmoSpace(GizmoSpace space) noexcept {
    if (gizmo_.isDragging()) {
        (void)cancelActiveGizmoDrag();
    }
    gizmo_.setSpace(space);
    gizmo_.setHoveredHandle(GizmoHandle::None);
    syncDocumentState();
    ++interaction_revision_;
}

void StudioSession::setCameraViewActive(bool active) noexcept {
    if (camera_view_active_ == active) {
        return;
    }
    if (gizmo_.isDragging()) {
        (void)cancelActiveGizmoDrag();
    }
    view_transition_.active = false;
    camera_view_active_ = active;
    if (camera_view_active_) {
        const scene::ObjectId cam_id = document_.scene.activeCamera();
        if (cam_id.isValid()) {
            if (const auto* cam_obj = document_.scene.findObject(cam_id); cam_obj != nullptr) {
                camera_controller_.syncFromCameraTransform(
                    cam_obj->local_transform,
                    {0.0f, 0.72f, 0.0f});
            }
        }
    } else {
        resetEditorViewportCamera();
    }
    gizmo_.setHoveredHandle(GizmoHandle::None);
    ++interaction_revision_;
}

void StudioSession::detachFromActiveSceneCameraForObserverControl() noexcept {
    if (!camera_view_active_) {
        return;
    }
    const math::Vec3 keep_target = camera_controller_.target();
    const scene::ObjectId cam_id = document_.scene.activeCamera();
    if (cam_id.isValid()) {
        if (const auto* cam_obj = document_.scene.findObject(cam_id); cam_obj != nullptr) {
            camera_controller_.syncFromCameraTransform(cam_obj->local_transform, keep_target);
        }
    }
    camera_view_active_ = false;
    syncViewportOrthoHeightFromDistance();
}

void StudioSession::syncViewportOrthoHeightFromDistance() noexcept {
    const float half_tan = std::tan(std::clamp(viewport_camera_.fov_y_radians, 0.1f, 2.4f) * 0.5f);
    viewport_camera_.orthographic_height =
        std::clamp(2.0f * camera_controller_.distance() * half_tan, 0.15f, 400.0f);
}

void StudioSession::markUserFreeViewLabel() noexcept {
    viewport_view_label_ =
        (viewport_camera_.projection == scene::ProjectionType::Orthographic) ? "User Ortho" : "User Persp";
}

core::Status StudioSession::orbitEditorObserverCamera(float delta_pixels_x, float delta_pixels_y) {
    if (gizmo_.isDragging()) {
        (void)cancelActiveGizmoDrag();
    }
    view_transition_.active = false;
    detachFromActiveSceneCameraForObserverControl();
    camera_controller_.orbit(
        -delta_pixels_x * config_.orbit_radians_per_pixel,
        -delta_pixels_y * config_.orbit_radians_per_pixel);
    markUserFreeViewLabel();
    ++interaction_revision_;
    return core::Status::ok();
}

void StudioSession::setViewportViewPreset(ViewportViewPreset preset, bool smooth) noexcept {
    if (gizmo_.isDragging()) {
        (void)cancelActiveGizmoDrag();
    }
    detachFromActiveSceneCameraForObserverControl();

    const PresetAngles angles = computePresetAngles(preset);
    const math::Vec3 goal_target =
        (preset == ViewportViewPreset::Home) ? kDefaultHomeTarget : camera_controller_.target();
    const float goal_dist =
        (preset == ViewportViewPreset::Home)
            ? (kDefaultHomeEye - kDefaultHomeTarget).length()
            : camera_controller_.distance();

    if (preset == ViewportViewPreset::Home) {
        markUserFreeViewLabel();
    } else {
        viewport_view_label_ = std::string(viewportViewPresetLabel(preset));
    }

    if (!smooth) {
        view_transition_.active = false;
        camera_controller_.setPose(goal_target, angles.yaw, angles.pitch, goal_dist);
        syncViewportOrthoHeightFromDistance();
        gizmo_.setHoveredHandle(GizmoHandle::None);
        ++interaction_revision_;
        return;
    }

    const float cur_yaw = camera_controller_.yawRadians();
    const float cur_pitch = camera_controller_.pitchRadians();
    const float shortest_goal_yaw = cur_yaw + std::remainder(angles.yaw - cur_yaw, math::kTwoPi);

    view_transition_.active = true;
    view_transition_.elapsed_seconds = 0.0f;
    view_transition_.duration_seconds = 0.22f;
    view_transition_.start_yaw = cur_yaw;
    view_transition_.goal_yaw = shortest_goal_yaw;
    view_transition_.start_pitch = cur_pitch;
    view_transition_.goal_pitch = angles.pitch;
    view_transition_.start_target = camera_controller_.target();
    view_transition_.goal_target = goal_target;
    view_transition_.start_distance = camera_controller_.distance();
    view_transition_.goal_distance = goal_dist;

    gizmo_.setHoveredHandle(GizmoHandle::None);
    ++interaction_revision_;
}

void StudioSession::resetViewportHomeView(bool smooth) noexcept {
    viewport_camera_.projection = scene::ProjectionType::Perspective;
    setViewportViewPreset(ViewportViewPreset::Home, smooth);
}

void StudioSession::setViewportProjection(scene::ProjectionType projection) noexcept {
    detachFromActiveSceneCameraForObserverControl();
    if (viewport_camera_.projection == projection) {
        ++interaction_revision_;
        return;
    }
    viewport_camera_.projection = projection;
    syncViewportOrthoHeightFromDistance();
    if (viewport_view_label_ == "User Persp" || viewport_view_label_ == "User Ortho") {
        markUserFreeViewLabel();
    }
    ++interaction_revision_;
}

void StudioSession::toggleViewportProjection() noexcept {
    setViewportProjection(
        (viewport_camera_.projection == scene::ProjectionType::Orthographic)
            ? scene::ProjectionType::Perspective
            : scene::ProjectionType::Orthographic);
}

std::string_view StudioSession::viewportViewLabel() const noexcept {
    if (camera_view_active_) {
        return "Camera Lens";
    }
    return viewport_view_label_;
}

void StudioSession::setEditorHelpersVisible(bool visible) noexcept {
    if (editor_helpers_visible_ == visible) {
        return;
    }
    editor_helpers_visible_ = visible;
    ++interaction_revision_;
}

void StudioSession::setEditorGridVisible(bool visible) noexcept {
    if (editor_grid_visible_ == visible) {
        return;
    }
    editor_grid_visible_ = visible;
    ++interaction_revision_;
}

core::Status StudioSession::renderFinalScene(const rendering::FrameClearDesc& clear) {
    return renderer_.renderScene(
        document_.scene,
        clear,
        scene::ObjectId{},
        {},
        nullptr,
        nullptr);
}

void StudioSession::resetEditorViewportCamera() noexcept {
    view_transition_.active = false;
    math::Transform editor_eye{};
    editor_eye.position = kDefaultHomeEye;
    camera_controller_.syncFromCameraTransform(editor_eye, kDefaultHomeTarget);
    syncViewportOrthoHeightFromDistance();
    markUserFreeViewLabel();
}

void StudioSession::syncCameraControllerFromScene() noexcept {
    if (!camera_view_active_) {
        resetEditorViewportCamera();
        return;
    }
    math::Vec3 focus{0.0f, 0.72f, 0.0f};
    if (custom_project_loaded_) {
        focus = math::Vec3::zero();
    }
    const scene::ObjectId cam_id = document_.scene.activeCamera();
    if (cam_id.isValid()) {
        if (const auto* cam_obj = document_.scene.findObject(cam_id); cam_obj != nullptr) {
            camera_controller_.syncFromCameraTransform(cam_obj->local_transform, focus);
        }
    }
}

core::Status StudioSession::applyCameraControllerToScene() {
    syncViewportOrthoHeightFromDistance();
    ++interaction_revision_;
    if (!camera_view_active_) {
        return core::Status::ok();
    }
    const scene::ObjectId cam_id = document_.scene.activeCamera();
    if (!cam_id.isValid()) {
        return core::Status::ok();
    }
    const auto* cam_obj = document_.scene.findObject(cam_id);
    if (cam_obj == nullptr) {
        return core::Status::ok();
    }
    return document_.scene.setObjectTransform(cam_id, camera_controller_.computeCameraTransform());
}

core::Status StudioSession::onInitialize(core::Engine& engine) {
    const auto fb = window_.framebufferSize();
    auto render_st = renderer_.initialize(fb.width, fb.height);
    if (!render_st) {
        return render_st;
    }
    last_fb_width_ = fb.width;
    last_fb_height_ = fb.height;
    last_content_scale_ = window_.contentScale();

    engine.logger().info(
        "App.Session",
        "Renderer initialized: backend=" + renderer_.device().capabilities().backend_name +
            ", device=" + renderer_.device().capabilities().device_name);

    if (!custom_project_loaded_) {
        document_.metadata.project_name = "Final Effect 3D Starter Project";
        document_.metadata.author = "Final Effect 3D";
        document_.metadata.description = "Stage 4 interactive 3D studio project";
        auto starter_res = populateStarterScene(document_.scene);
        if (!starter_res) {
            return starter_res.error();
        }
        syncDocumentState();
    }

    syncCameraControllerFromScene();
    return core::Status::ok();
}

StudioSession::ViewportCameraState StudioSession::activeCameraState() const noexcept {
    ViewportCameraState state{};
    state.camera_world = camera_controller_.computeCameraTransform().toMatrix();
    state.camera = viewport_camera_;

    if (camera_view_active_ && document_.scene.activeCamera().isValid()) {
        if (const auto* cam_obj = document_.scene.findObject(document_.scene.activeCamera());
            cam_obj != nullptr && cam_obj->camera.has_value()) {
            if (auto cw = document_.scene.computeWorldMatrix(cam_obj->id); cw.hasValue()) {
                state.camera_world = cw.value();
            }
            state.camera = cam_obj->camera.value();
        }
    }
    return state;
}

std::optional<math::Vec3> StudioSession::projectWorldPointToScreen(
    const math::Vec3& world_point) const noexcept {
    const auto fb = window_.framebufferSize();
    if (fb.width == 0 || fb.height == 0) {
        return std::nullopt;
    }
    const auto cam_st = activeCameraState();
    const auto inv_cam = cam_st.camera_world.inverted();
    if (!inv_cam.has_value()) {
        return std::nullopt;
    }
    const float aspect = static_cast<float>(fb.width) / static_cast<float>(fb.height);
    const math::Mat4 proj = cam_st.camera.computeProjectionMatrix(
        aspect,
        renderer_.device().capabilities().clip_space_depth);
    const math::Vec4 clip = (proj * inv_cam.value()) * math::Vec4(world_point, 1.0f);
    if (clip.w <= 1.0e-4f || !std::isfinite(clip.w)) {
        return std::nullopt;
    }
    const float inv_w = 1.0f / clip.w;
    const float ndc_x = clip.x * inv_w;
    const float ndc_y = clip.y * inv_w;
    const float ndc_z = clip.z * inv_w;
    const float sx = (ndc_x * 0.5f + 0.5f) * static_cast<float>(fb.width);
    const float sy = (0.5f - ndc_y * 0.5f) * static_cast<float>(fb.height);
    return math::Vec3{sx, sy, ndc_z};
}

std::optional<math::Ray> StudioSession::computeViewportRay(
    float screen_x,
    float screen_y) const noexcept {
    const auto fb = window_.framebufferSize();
    if (fb.width == 0 || fb.height == 0) {
        return std::nullopt;
    }
    const auto cam_st = activeCameraState();
    auto ray_res = OrbitCameraController::screenPointToWorldRay(
        screen_x, screen_y, fb.width, fb.height, cam_st.camera_world, cam_st.camera);
    if (!ray_res) {
        return std::nullopt;
    }
    return ray_res.value();
}

std::optional<GizmoFrame> StudioSession::computeActiveGizmoFrame() const noexcept {
    if (!selected_object_.isValid() ||
        (camera_view_active_ && selected_object_ == document_.scene.activeCamera())) {
        return std::nullopt;
    }
    if (editor_mode_ == scene::EditMode::Object && active_tool_ == MayaTool::Select) {
        return std::nullopt;
    }
    if (isObjectLocked(selected_object_)) {
        return std::nullopt;
    }
    const auto* obj = document_.scene.findObject(selected_object_);
    if (obj == nullptr || !isObjectVisibleInHierarchy(document_.scene, selected_object_)) {
        return std::nullopt;
    }
    const std::uint32_t lid = objectDisplayLayerId(selected_object_);
    if (lid != 0) {
        for (const auto& layer : display_layers_) {
            if (layer.id == lid && (!layer.visible || layer.mode != DisplayLayerMode::Normal)) {
                return std::nullopt;
            }
        }
    }
    if (!editor_helpers_visible_ && !obj->mesh_id.has_value()) {
        return std::nullopt;
    }
    auto obj_world_res = document_.scene.computeWorldMatrix(selected_object_);
    if (!obj_world_res) {
        return std::nullopt;
    }

    const auto fb = window_.framebufferSize();
    const float pixel_scale = window_.contentScale();
    const auto cam_st = activeCameraState();
    if (editor_mode_ == scene::EditMode::Object) {
        math::Mat4 target_world = obj_world_res.value();
        const math::Vec3 piv = objectPivotOffset(selected_object_);
        if (piv.lengthSquared() > 1.0e-8f) {
            const math::Vec3 world_piv = target_world.transformPoint(piv);
            target_world(0, 3) = world_piv.x;
            target_world(1, 3) = world_piv.y;
            target_world(2, 3) = world_piv.z;
        }
        return TransformGizmo::computeFrame(
            target_world,
            cam_st.camera_world,
            cam_st.camera,
            gizmo_.mode(),
            gizmo_.space(),
            fb.width,
            fb.height,
            pixel_scale);
    }

    // Mesh Edit mode: anchor Translate gizmo at world centroid of selected vertices/edges/faces.
    if (!obj->mesh_id.has_value() || mesh_selection_.empty()) {
        return std::nullopt;
    }
    const auto* ma = document_.scene.findMesh(obj->mesh_id.value());
    if (ma == nullptr || !ma->editable_mesh.has_value()) {
        return std::nullopt;
    }
    const auto local_centroid =
        ma->editable_mesh->computeSelectionCentroid(editor_mode_, mesh_selection_);
    if (!local_centroid.has_value()) {
        return std::nullopt;
    }

    math::Mat4 target_world = obj_world_res.value();
    const math::Vec3 world_center = obj_world_res.value().transformPoint(local_centroid.value());
    target_world(0, 3) = world_center.x;
    target_world(1, 3) = world_center.y;
    target_world(2, 3) = world_center.z;

    return TransformGizmo::computeFrame(
        target_world,
        cam_st.camera_world,
        cam_st.camera,
        GizmoMode::Translate,
        gizmo_.space(),
        fb.width,
        fb.height,
        pixel_scale);
}

std::optional<scene::RaycastHit> StudioSession::pickAtScreenPosition(float screen_x, float screen_y) {
    const auto ray = computeViewportRay(screen_x, screen_y);
    if (!ray.has_value()) {
        return std::nullopt;
    }
    const auto cam_st = activeCameraState();
    const bool allow_helpers = editor_helpers_visible_ && (editor_mode_ == scene::EditMode::Object);
    const scene::ObjectId hidden_cam =
        camera_view_active_ ? document_.scene.activeCamera() : scene::ObjectId::invalid();
    scene::RaycastOptions opts{};
    opts.max_distance = cam_st.camera.far_plane;
    opts.object_filter = [](scene::ObjectId oid, const void* ud) noexcept -> bool {
        const auto* self = static_cast<const StudioSession*>(ud);
        return self->isObjectSelectableInViewport(oid);
    };
    opts.filter_user_data = this;

    auto hit = pickSceneOrViewportHelper(
        document_.scene,
        ray.value(),
        allow_helpers,
        hidden_cam,
        opts);
    if (left_click_shift_ && hit.has_value()) {
        toggleOrAddSelectedObject(hit->object_id, true);
    } else if (!left_click_shift_) {
        setSelectedObject(hit.has_value() ? hit->object_id : scene::ObjectId{});
    }
    return hit;
}

bool StudioSession::pickMeshElementAtScreenPosition(
    float screen_x,
    float screen_y,
    bool additive) {
    if (editor_mode_ == scene::EditMode::Object || !selected_object_.isValid()) {
        return false;
    }
    const auto* obj = document_.scene.findObject(selected_object_);
    if (obj == nullptr || !obj->mesh_id.has_value()) {
        return false;
    }
    const auto* ma = document_.scene.findMesh(obj->mesh_id.value());
    if (ma == nullptr || !ma->editable_mesh.has_value()) {
        return false;
    }
    auto obj_world = document_.scene.computeWorldMatrix(selected_object_);
    const auto ray = computeViewportRay(screen_x, screen_y);
    if (!obj_world || !ray.has_value()) {
        return false;
    }

    const auto& em = ma->editable_mesh.value();
    const auto world_bounds = document_.scene.computeObjectWorldBounds(selected_object_);
    const float world_radius = world_bounds.hasValue()
                                   ? std::max(0.06f, world_bounds.value().size().length() * 0.065f)
                                   : 0.12f;

    bool matched = false;
    if (editor_mode_ == scene::EditMode::Vertex) {
        if (const auto vi = em.pickVertex(ma->data, obj_world.value(), ray.value(), world_radius);
            vi.has_value()) {
            mesh_selection_.toggleOrSetVertex(vi.value(), additive);
            matched = true;
        }
    } else if (editor_mode_ == scene::EditMode::Edge) {
        if (const auto ei = em.pickEdge(ma->data, obj_world.value(), ray.value(), world_radius);
            ei.has_value()) {
            mesh_selection_.toggleOrSetEdge(ei.value(), additive);
            matched = true;
        }
    } else if (editor_mode_ == scene::EditMode::Face) {
        if (const auto fi = em.pickFace(ma->data, obj_world.value(), ray.value()); fi.has_value()) {
            mesh_selection_.toggleOrSetFace(fi.value(), additive);
            matched = true;
        }
    }

    if (!matched && !additive) {
        mesh_selection_.clear();
    }
    syncDocumentState();
    ++interaction_revision_;
    return matched;
}

core::Status StudioSession::cancelActiveGizmoDrag() {
    if (!gizmo_.isDragging()) {
        drag_mode_ = DragMode::None;
        return core::Status::ok();
    }

    (void)gizmo_.cancelDrag();
    const DragMode prev_mode = drag_mode_;
    drag_mode_ = DragMode::None;

    if (gizmo_drag_mutated_) {
        gizmo_drag_mutated_ = false;
        ++interaction_revision_;
        if (prev_mode == DragMode::GizmoPivot && gizmo_drag_object_id_.isValid()) {
            setObjectPivotOffset(gizmo_drag_object_id_, gizmo_drag_initial_pivot_);
            return core::Status::ok();
        }
        if (prev_mode == DragMode::GizmoObject && gizmo_drag_object_id_.isValid()) {
            return document_.scene.setObjectTransform(gizmo_drag_object_id_, gizmo_drag_initial_tf_);
        }
        if (prev_mode == DragMode::GizmoMeshElements && gizmo_drag_mesh_id_.isValid()) {
            return document_.scene.updateMeshEditable(gizmo_drag_mesh_id_, gizmo_drag_base_mesh_);
        }
    }
    return core::Status::ok();
}

math::Transform StudioSession::applySnapToTransform(
    scene::ObjectId id,
    const math::Transform& candidate_local_tf,
    const math::Ray& world_ray) const noexcept {
    math::Transform out = candidate_local_tf;
    const auto* obj = document_.scene.findObject(id);
    if (obj == nullptr) {
        return out;
    }

    math::Mat4 parent_world = math::Mat4::identity();
    if (obj->parent_id.isValid()) {
        if (auto pw = document_.scene.computeWorldMatrix(obj->parent_id); pw.hasValue()) {
            parent_world = pw.value();
        }
    }
    const auto inv_parent = parent_world.inverted().value_or(math::Mat4::identity());

    if (gizmo_.mode() == GizmoMode::Translate) {
        math::Vec3 world_pos = parent_world.transformPoint(out.position);
        bool snapped_target = false;

        if (snap_settings_.surface && world_ray.isValid()) {
            struct FilterCtx {
                const StudioSession* self;
                scene::ObjectId exclude_id;
            } ctx{this, id};
            scene::RaycastOptions opts{};
            opts.object_filter = [](scene::ObjectId oid, const void* ud) noexcept -> bool {
                const auto* c = static_cast<const FilterCtx*>(ud);
                return oid != c->exclude_id && isObjectVisibleInHierarchy(c->self->document_.scene, oid);
            };
            opts.filter_user_data = &ctx;
            if (auto hit = scene::raycastScene(document_.scene, world_ray, opts); hit.has_value()) {
                world_pos = hit->world_point;
                snapped_target = true;
            }
        }

        if (!snapped_target && (snap_settings_.point || snap_settings_.curve)) {
            float best_d2 = 0.75f * 0.75f;
            math::Vec3 best_pt = world_pos;
            for (const auto& other : document_.scene.objects()) {
                if (other.id == id || !other.mesh_id.has_value() ||
                    !isObjectVisibleInHierarchy(document_.scene, other.id)) {
                    continue;
                }
                const auto* ma = document_.scene.findMesh(other.mesh_id.value());
                auto ow = document_.scene.computeWorldMatrix(other.id);
                if (ma == nullptr || !ow.hasValue()) {
                    continue;
                }
                if (snap_settings_.point) {
                    for (const auto& v : ma->data.vertices) {
                        const math::Vec3 wp = ow.value().transformPoint(v.position);
                        const float d2 = (wp - world_pos).lengthSquared();
                        if (d2 < best_d2) {
                            best_d2 = d2;
                            best_pt = wp;
                            snapped_target = true;
                        }
                    }
                }
                if (snap_settings_.curve) {
                    const auto& idx = ma->data.indices;
                    const auto& verts = ma->data.vertices;
                    for (std::size_t ti = 0; ti + 2 < idx.size(); ti += 3) {
                        const std::array<std::pair<std::uint32_t, std::uint32_t>, 3> tri_edges{{
                            {idx[ti], idx[ti + 1]},
                            {idx[ti + 1], idx[ti + 2]},
                            {idx[ti + 2], idx[ti]},
                        }};
                        for (const auto& [a, b] : tri_edges) {
                            const math::Vec3 mid =
                                (ow.value().transformPoint(verts[a].position) +
                                 ow.value().transformPoint(verts[b].position)) *
                                0.5f;
                            const float d2 = (mid - world_pos).lengthSquared();
                            if (d2 < best_d2) {
                                best_d2 = d2;
                                best_pt = mid;
                                snapped_target = true;
                            }
                        }
                    }
                }
            }
            if (snapped_target) {
                world_pos = best_pt;
            }
        }

        if (!snapped_target && snap_settings_.grid) {
            const float step = std::max(0.01f, snap_settings_.grid_step);
            world_pos.x = std::round(world_pos.x / step) * step;
            world_pos.y = std::round(world_pos.y / step) * step;
            world_pos.z = std::round(world_pos.z / step) * step;
            snapped_target = true;
        }

        if (snapped_target) {
            out.position = inv_parent.transformPoint(world_pos);
        }
    } else if (gizmo_.mode() == GizmoMode::Rotate && snap_settings_.grid) {
        const float step_rad = math::degToRad(std::max(0.5f, snap_settings_.rotate_step_deg));
        math::Vec3 euler = out.rotation.toEulerXYZ();
        euler.x = std::round(euler.x / step_rad) * step_rad;
        euler.y = std::round(euler.y / step_rad) * step_rad;
        euler.z = std::round(euler.z / step_rad) * step_rad;
        out.rotation = math::Quat::fromEulerXYZ(euler);
    } else if (gizmo_.mode() == GizmoMode::Scale && snap_settings_.grid) {
        const float step = std::max(0.01f, snap_settings_.scale_step);
        out.scale.x = std::max(0.01f, std::round(out.scale.x / step) * step);
        out.scale.y = std::max(0.01f, std::round(out.scale.y / step) * step);
        out.scale.z = std::max(0.01f, std::round(out.scale.z / step) * step);
    }

    return out;
}

core::Status StudioSession::commitActiveGizmoDrag() {
    if (!gizmo_.isDragging()) {
        drag_mode_ = DragMode::None;
        return core::Status::ok();
    }

    gizmo_.endDrag();
    const DragMode prev_mode = drag_mode_;
    drag_mode_ = DragMode::None;

    if (!gizmo_drag_mutated_) {
        return core::Status::ok();
    }
    gizmo_drag_mutated_ = false;
    ++interaction_revision_;

    if (prev_mode == DragMode::GizmoPivot) {
        return core::Status::ok();
    }

    if (prev_mode == DragMode::GizmoObject && gizmo_drag_object_id_.isValid()) {
        auto restore_st = document_.scene.setObjectTransform(gizmo_drag_object_id_, gizmo_drag_initial_tf_);
        if (!restore_st) {
            return restore_st;
        }
        auto cmd_st = command_stack_.execute(
            document_.scene,
            std::make_unique<scene::SetTransformCommand>(
                gizmo_drag_object_id_,
                gizmo_drag_latest_tf_,
                false));
        if (cmd_st) {
            recordAutoKeyIfEnabled(gizmo_drag_object_id_);
        }
        return cmd_st;
    }

    if (prev_mode == DragMode::GizmoMeshElements && gizmo_drag_mesh_id_.isValid()) {
        auto restore_st = document_.scene.updateMeshEditable(gizmo_drag_mesh_id_, gizmo_drag_base_mesh_);
        if (!restore_st) {
            return restore_st;
        }
        return command_stack_.execute(
            document_.scene,
            std::make_unique<scene::ModifyMeshCommand>(
                gizmo_drag_mesh_id_,
                gizmo_drag_latest_mesh_,
                "Move Mesh Elements",
                false));
    }

    return core::Status::ok();
}

core::Status StudioSession::frameSelectionOrScene() {
    const float fov_y = activeCameraState().camera.fov_y_radians;

    math::AABB target_bounds = math::AABB::empty();
    sanitizeMultiSelection();
    for (const scene::ObjectId id : selected_objects_) {
        if (document_.scene.findObject(id) != nullptr) {
            if (auto b = document_.scene.computeObjectWorldBounds(id); b && b.value().isValid()) {
                target_bounds.expand(b.value());
            }
        }
    }
    if (!target_bounds.isValid() && selected_object_.isValid() &&
        document_.scene.findObject(selected_object_) != nullptr) {
        if (auto b = document_.scene.computeObjectWorldBounds(selected_object_); b && b.value().isValid()) {
            target_bounds = b.value();
        }
    }

    if (!target_bounds.isValid()) {
        return frameAllScene();
    }

    view_transition_.active = false;
    camera_controller_.frameBounds(target_bounds, fov_y);
    return applyCameraControllerToScene();
}

core::Status StudioSession::frameAllScene() {
    const float fov_y = activeCameraState().camera.fov_y_radians;
    math::AABB target_bounds = math::AABB::empty();

    for (const auto& obj : document_.scene.objects()) {
        if (!isObjectVisibleInHierarchy(document_.scene, obj.id)) {
            continue;
        }
        const bool is_helper =
            !camera_view_active_ && editor_helpers_visible_ &&
            (obj.camera.has_value() || obj.hasAnyLight());
        if (!obj.mesh_id.has_value() && !is_helper) {
            continue;
        }
        if (auto b = document_.scene.computeObjectWorldBounds(obj.id); b && b.value().isValid()) {
            target_bounds.expand(b.value());
        }
    }

    if (target_bounds.isValid()) {
        view_transition_.active = false;
        camera_controller_.frameBounds(target_bounds, fov_y);
        return applyCameraControllerToScene();
    }
    return core::Status::ok();
}

core::Status StudioSession::orbitViewportCamera(float delta_pixels_x, float delta_pixels_y) {
    view_transition_.active = false;
    camera_controller_.orbit(
        -delta_pixels_x * config_.orbit_radians_per_pixel,
        -delta_pixels_y * config_.orbit_radians_per_pixel);
    if (!camera_view_active_) {
        markUserFreeViewLabel();
    }
    return applyCameraControllerToScene();
}

core::Status StudioSession::panViewportCamera(float delta_pixels_x, float delta_pixels_y) {
    view_transition_.active = false;
    const float fov_y = activeCameraState().camera.fov_y_radians;
    camera_controller_.pan(
        delta_pixels_x,
        delta_pixels_y,
        window_.framebufferSize().height,
        fov_y);
    return applyCameraControllerToScene();
}

core::Status StudioSession::zoomViewportCamera(float wheel_steps) {
    if (std::fabs(wheel_steps) > 1.0e-5f) {
        view_transition_.active = false;
        camera_controller_.zoom(wheel_steps);
        return applyCameraControllerToScene();
    }
    return core::Status::ok();
}

bool StudioSession::hitTestGizmoAtScreenPosition(float screen_x, float screen_y) const noexcept {
    const auto frame = computeActiveGizmoFrame();
    const auto ray = computeViewportRay(screen_x, screen_y);
    if (!frame.has_value() || !ray.has_value()) {
        return false;
    }
    const GizmoMode eff_mode =
        (editor_mode_ == scene::EditMode::Object) ? gizmo_.mode() : GizmoMode::Translate;
    return TransformGizmo::pickHandle(eff_mode, frame.value(), ray.value()).has_value();
}

core::Status StudioSession::processInputEvent(const platform::InputEvent& event) {
    if (const auto* mb = std::get_if<platform::MouseButtonEvent>(&event)) {
        last_cursor_x_ = mb->x;
        last_cursor_y_ = mb->y;
        has_cursor_pos_ = true;

        if (mb->action == platform::KeyAction::Press) {
            press_cursor_x_ = mb->x;
            press_cursor_y_ = mb->y;
            left_press_near_gizmo_ = false;

            if (mb->button == platform::MouseButton::Right && gizmo_.isDragging()) {
                return cancelActiveGizmoDrag();
            }

            if (mb->button == platform::MouseButton::Middle) {
                view_transition_.active = false;
                drag_mode_ = DragMode::Pan;
            } else if (mb->button == platform::MouseButton::Right) {
                view_transition_.active = false;
                if (mb->mods.alt) {
                    drag_mode_ = DragMode::Zoom;
                } else {
                    drag_mode_ = mb->mods.shift ? DragMode::Pan : DragMode::Orbit;
                }
            } else if (mb->button == platform::MouseButton::Left) {
                if (mb->mods.alt) {
                    view_transition_.active = false;
                    drag_mode_ = mb->mods.shift ? DragMode::Pan : DragMode::Orbit;
                } else {
                    // 1. Test 3D Transform Gizmo handles FIRST so dragging never reselects objects underneath.
                    const auto frame = computeActiveGizmoFrame();
                    const auto ray = computeViewportRay(static_cast<float>(mb->x), static_cast<float>(mb->y));
                    if (frame.has_value() && ray.has_value()) {
                        const GizmoMode eff_mode =
                            (editor_mode_ == scene::EditMode::Object) ? gizmo_.mode() : GizmoMode::Translate;
                        const auto gizmo_hit = TransformGizmo::pickHandle(eff_mode, frame.value(), ray.value());
                        if (gizmo_hit.has_value() && gizmo_hit->handle != GizmoHandle::None) {
                            config_.animate_showcase_rig = false;
                            gizmo_drag_mutated_ = false;
                            gizmo_.setHoveredHandle(gizmo_hit->handle);
                            const auto cam_st = activeCameraState();

                            if (editor_mode_ == scene::EditMode::Object && active_tool_ == MayaTool::PivotEdit) {
                                auto ow = document_.scene.computeWorldMatrix(selected_object_);
                                math::Transform piv_tf = math::Transform::identity();
                                piv_tf.position = frame->origin;
                                if (ow.hasValue() &&
                                    gizmo_.beginDrag(
                                        gizmo_hit->handle,
                                        frame.value(),
                                        ray.value(),
                                        cam_st.camera_world,
                                        piv_tf,
                                        math::Mat4::identity()).hasValue()) {
                                    gizmo_drag_object_id_ = selected_object_;
                                    gizmo_drag_initial_pivot_ = objectPivotOffset(selected_object_);
                                    gizmo_drag_obj_world_ = ow.value();
                                    drag_mode_ = DragMode::GizmoPivot;
                                    ++interaction_revision_;
                                    return core::Status::ok();
                                }
                            } else if (editor_mode_ == scene::EditMode::Object) {
                                const auto* obj = document_.scene.findObject(selected_object_);
                                math::Mat4 parent_world = math::Mat4::identity();
                                if (obj != nullptr && obj->parent_id.isValid()) {
                                    if (auto pw = document_.scene.computeWorldMatrix(obj->parent_id); pw.hasValue()) {
                                        parent_world = pw.value();
                                    }
                                }
                                if (obj != nullptr &&
                                    gizmo_.beginDrag(
                                        gizmo_hit->handle,
                                        frame.value(),
                                        ray.value(),
                                        cam_st.camera_world,
                                        obj->local_transform,
                                        parent_world).hasValue()) {
                                    gizmo_drag_object_id_ = selected_object_;
                                    gizmo_drag_initial_tf_ = obj->local_transform;
                                    gizmo_drag_latest_tf_ = obj->local_transform;
                                    drag_mode_ = DragMode::GizmoObject;
                                    ++interaction_revision_;
                                    return core::Status::ok();
                                }
                            } else {
                                const auto* obj = document_.scene.findObject(selected_object_);
                                const auto* ma = (obj != nullptr && obj->mesh_id.has_value())
                                                     ? document_.scene.findMesh(obj->mesh_id.value())
                                                     : nullptr;
                                auto ow = document_.scene.computeWorldMatrix(selected_object_);
                                math::Transform elem_tf = math::Transform::identity();
                                elem_tf.position = frame->origin;
                                if (ma != nullptr && ma->editable_mesh.has_value() && ow.hasValue() &&
                                    gizmo_.beginDrag(
                                        gizmo_hit->handle,
                                        frame.value(),
                                        ray.value(),
                                        cam_st.camera_world,
                                        elem_tf,
                                        math::Mat4::identity()).hasValue()) {
                                    gizmo_drag_mesh_id_ = obj->mesh_id.value();
                                    gizmo_drag_base_mesh_ = ma->editable_mesh.value();
                                    gizmo_drag_latest_mesh_ = gizmo_drag_base_mesh_;
                                    gizmo_drag_obj_world_ = ow.value();
                                    drag_mode_ = DragMode::GizmoMeshElements;
                                    ++interaction_revision_;
                                    return core::Status::ok();
                                }
                            }
                        }
                        left_press_near_gizmo_ =
                            selected_object_.isValid() &&
                            TransformGizmo::isNearAnyHandle(eff_mode, frame.value(), ray.value());
                    }

                    left_click_shift_ = mb->mods.shift;
                    drag_mode_ = DragMode::PendingLeftClick;
                }
            }
        } else if (mb->action == platform::KeyAction::Release) {
            if ((drag_mode_ == DragMode::GizmoObject ||
                 drag_mode_ == DragMode::GizmoPivot ||
                 drag_mode_ == DragMode::GizmoMeshElements) &&
                mb->button == platform::MouseButton::Left) {
                return commitActiveGizmoDrag();
            }
            if (drag_mode_ == DragMode::PendingLeftClick && mb->button == platform::MouseButton::Left) {
                const double dx = mb->x - press_cursor_x_;
                const double dy = mb->y - press_cursor_y_;
                if ((dx * dx + dy * dy) <= 16.0) {
                    if (editor_mode_ == scene::EditMode::Object) {
                        bool allow_pick = true;
                        if (left_press_near_gizmo_ && selected_object_.isValid()) {
                            const auto frame = computeActiveGizmoFrame();
                            const auto ray = computeViewportRay(static_cast<float>(mb->x), static_cast<float>(mb->y));
                            if (frame.has_value() && ray.has_value()) {
                                const auto cam_st = activeCameraState();
                                const bool allow_helpers = editor_helpers_visible_;
                                const scene::ObjectId hidden_cam =
                                    camera_view_active_ ? document_.scene.activeCamera() : scene::ObjectId::invalid();
                                const auto probe = pickSceneOrViewportHelper(
                                    document_.scene,
                                    ray.value(),
                                    allow_helpers,
                                    hidden_cam,
                                    scene::RaycastOptions{.max_distance = cam_st.camera.far_plane});
                                const float gizmo_depth = (frame->origin - ray->origin).length();
                                if (!probe.has_value() || probe->distance + 0.15f >= gizmo_depth) {
                                    allow_pick = false;
                                }
                            }
                        }
                        if (allow_pick) {
                            (void)pickAtScreenPosition(static_cast<float>(mb->x), static_cast<float>(mb->y));
                        }
                    } else {
                        (void)pickMeshElementAtScreenPosition(
                            static_cast<float>(mb->x),
                            static_cast<float>(mb->y),
                            left_click_shift_);
                    }
                }
            }
            left_press_near_gizmo_ = false;
            drag_mode_ = DragMode::None;
        }
        return core::Status::ok();
    }

    if (const auto* cm = std::get_if<platform::CursorMoveEvent>(&event)) {
        if (!has_cursor_pos_) {
            last_cursor_x_ = cm->x;
            last_cursor_y_ = cm->y;
            has_cursor_pos_ = true;
        }
        const auto dx = static_cast<float>(cm->x - last_cursor_x_);
        const auto dy = static_cast<float>(cm->y - last_cursor_y_);
        last_cursor_x_ = cm->x;
        last_cursor_y_ = cm->y;

        if (drag_mode_ == DragMode::GizmoPivot) {
            const auto ray = computeViewportRay(static_cast<float>(cm->x), static_cast<float>(cm->y));
            const auto inv_world = gizmo_drag_obj_world_.inverted();
            if (ray.has_value() && inv_world.has_value()) {
                auto upd = gizmo_.updateDrag(ray.value());
                if (upd.hasValue()) {
                    const math::Vec3 local_delta =
                        inv_world->transformDirection(upd.value().world_translation_delta);
                    setObjectPivotOffset(gizmo_drag_object_id_, gizmo_drag_initial_pivot_ + local_delta);
                    gizmo_drag_mutated_ = true;
                }
            }
            return core::Status::ok();
        }

        if (drag_mode_ == DragMode::GizmoObject) {
            const auto ray = computeViewportRay(static_cast<float>(cm->x), static_cast<float>(cm->y));
            if (ray.has_value()) {
                auto upd = gizmo_.updateDrag(ray.value());
                if (upd.hasValue()) {
                    math::Transform cand = applySnapToTransform(
                        gizmo_drag_object_id_,
                        upd.value().local_transform,
                        ray.value());
                    cand = applyChannelLocks(gizmo_drag_object_id_, gizmo_drag_initial_tf_, cand);
                    gizmo_drag_latest_tf_ = cand;
                    gizmo_drag_mutated_ = true;
                    ++interaction_revision_;
                    return document_.scene.setObjectTransform(gizmo_drag_object_id_, gizmo_drag_latest_tf_);
                }
            }
            return core::Status::ok();
        }

        if (drag_mode_ == DragMode::GizmoMeshElements) {
            const auto ray = computeViewportRay(static_cast<float>(cm->x), static_cast<float>(cm->y));
            const auto inv_world = gizmo_drag_obj_world_.inverted();
            if (ray.has_value() && inv_world.has_value()) {
                auto upd = gizmo_.updateDrag(ray.value());
                if (upd.hasValue()) {
                    const math::Vec3 local_delta =
                        inv_world->transformDirection(upd.value().world_translation_delta);
                    scene::EditableMesh moved = gizmo_drag_base_mesh_;
                    core::Status st = core::Status::ok();
                    if (editor_mode_ == scene::EditMode::Vertex) {
                        st = moved.translateVertices(mesh_selection_.vertices, local_delta);
                    } else if (editor_mode_ == scene::EditMode::Edge) {
                        st = moved.translateEdges(mesh_selection_.edges, local_delta);
                    } else if (editor_mode_ == scene::EditMode::Face) {
                        st = moved.translateFaces(mesh_selection_.faces, local_delta);
                    }
                    if (st) {
                        gizmo_drag_latest_mesh_ = std::move(moved);
                        gizmo_drag_mutated_ = true;
                        ++interaction_revision_;
                        return document_.scene.updateMeshEditable(gizmo_drag_mesh_id_, gizmo_drag_latest_mesh_);
                    }
                }
            }
            return core::Status::ok();
        }

        if (drag_mode_ == DragMode::Orbit) {
            view_transition_.active = false;
            camera_controller_.orbit(
                -dx * config_.orbit_radians_per_pixel,
                -dy * config_.orbit_radians_per_pixel);
            if (!camera_view_active_) {
                markUserFreeViewLabel();
            }
            return applyCameraControllerToScene();
        }
        if (drag_mode_ == DragMode::Pan) {
            view_transition_.active = false;
            const float fov_y = activeCameraState().camera.fov_y_radians;
            camera_controller_.pan(dx, dy, window_.framebufferSize().height, fov_y);
            return applyCameraControllerToScene();
        }
        if (drag_mode_ == DragMode::Zoom) {
            view_transition_.active = false;
            const float dolly_steps = (dx - dy) * 0.035f;
            if (std::fabs(dolly_steps) > 1.0e-5f) {
                camera_controller_.zoom(dolly_steps);
                return applyCameraControllerToScene();
            }
            return core::Status::ok();
        }

        if (drag_mode_ == DragMode::None) {
            GizmoHandle new_hover = GizmoHandle::None;
            const auto frame = computeActiveGizmoFrame();
            const auto ray = computeViewportRay(static_cast<float>(cm->x), static_cast<float>(cm->y));
            if (frame.has_value() && ray.has_value()) {
                const GizmoMode eff_mode =
                    (editor_mode_ == scene::EditMode::Object) ? gizmo_.mode() : GizmoMode::Translate;
                if (const auto hit = TransformGizmo::pickHandle(eff_mode, frame.value(), ray.value());
                    hit.has_value()) {
                    new_hover = hit->handle;
                }
            }
            if (new_hover != gizmo_.hoveredHandle()) {
                gizmo_.setHoveredHandle(new_hover);
                ++interaction_revision_;
            }
        }
        return core::Status::ok();
    }

    if (const auto* sc = std::get_if<platform::ScrollEvent>(&event)) {
        if (std::fabs(sc->delta_y) > 1.0e-5) {
            view_transition_.active = false;
            camera_controller_.zoom(static_cast<float>(sc->delta_y));
            return applyCameraControllerToScene();
        }
        return core::Status::ok();
    }

    if (const auto* key = std::get_if<platform::KeyEvent>(&event)) {
        if (key->action != platform::KeyAction::Press) {
            return core::Status::ok();
        }

        if (key->key == platform::KeyCode::Escape) {
            if (gizmo_.isDragging()) {
                return cancelActiveGizmoDrag();
            }
            if (editor_mode_ != scene::EditMode::Object) {
                return setEditorMode(scene::EditMode::Object);
            }
            if (selected_object_.isValid()) {
                setSelectedObject({});
            }
            return core::Status::ok();
        }

        const auto route = workspace_.routeKeyEvent(*key, text_input_focused_);
        if (route.target == RoutedInputTarget::TextInput || route.target == RoutedInputTarget::Ignored) {
            return core::Status::ok();
        }

        if (!key->mods.ctrl && !key->mods.alt) {
            if (key->key == platform::KeyCode::Q) {
                setActiveTool(MayaTool::Select);
                return core::Status::ok();
            }
            if (key->key == platform::KeyCode::W) {
                setActiveTool(MayaTool::Move);
                return core::Status::ok();
            }
            if (key->key == platform::KeyCode::E) {
                setActiveTool(MayaTool::Rotate);
                return core::Status::ok();
            }
            if (key->key == platform::KeyCode::R) {
                setActiveTool(MayaTool::Scale);
                return core::Status::ok();
            }
            if (key->key == platform::KeyCode::T) {
                setActiveTool(MayaTool::ShowManipulator);
                return core::Status::ok();
            }
            if (key->key == platform::KeyCode::Y) {
                activateLastTool();
                return core::Status::ok();
            }
            if (key->key == platform::KeyCode::D || key->key == platform::KeyCode::Insert) {
                setActiveTool(active_tool_ == MayaTool::PivotEdit ? last_tool_ : MayaTool::PivotEdit);
                return core::Status::ok();
            }
            if (key->key == platform::KeyCode::A) {
                return frameAllScene();
            }
            if (key->key == platform::KeyCode::S) {
                (void)setKeyframeForSelection();
                return core::Status::ok();
            }
            if (key->key == platform::KeyCode::H) {
                if (selected_object_.isValid()) {
                    if (const auto* obj = document_.scene.findObject(selected_object_); obj != nullptr) {
                        auto st = command_stack_.execute(
                            document_.scene,
                            std::make_unique<scene::SetVisibilityCommand>(selected_object_, !obj->visible));
                        ++interaction_revision_;
                        return st;
                    }
                }
                return core::Status::ok();
            }
            if (key->key == platform::KeyCode::X) {
                snap_settings_.grid = !snap_settings_.grid;
                ++interaction_revision_;
                return core::Status::ok();
            }
            if (key->key == platform::KeyCode::V) {
                snap_settings_.point = !snap_settings_.point;
                ++interaction_revision_;
                return core::Status::ok();
            }
            if (key->key == platform::KeyCode::C) {
                snap_settings_.curve = !snap_settings_.curve;
                ++interaction_revision_;
                return core::Status::ok();
            }
            if (key->key == platform::KeyCode::F2) {
                setMenuSet(MayaMenuSet::Modeling);
                return core::Status::ok();
            }
            if (key->key == platform::KeyCode::F3) {
                setMenuSet(MayaMenuSet::Rigging);
                return core::Status::ok();
            }
            if (key->key == platform::KeyCode::F4) {
                setMenuSet(MayaMenuSet::Animation);
                return core::Status::ok();
            }
            if (key->key == platform::KeyCode::F5) {
                setMenuSet(MayaMenuSet::FX);
                return core::Status::ok();
            }
            if (key->key == platform::KeyCode::F6) {
                setMenuSet(MayaMenuSet::Rendering);
                return core::Status::ok();
            }
            if (key->key == platform::KeyCode::Tab || key->key == platform::KeyCode::F8) {
                const auto target = (editor_mode_ == scene::EditMode::Object)
                                        ? last_component_mode_
                                        : scene::EditMode::Object;
                (void)setEditorMode(target);
                return core::Status::ok();
            }
            if (key->key == platform::KeyCode::F9) {
                (void)setEditorMode(scene::EditMode::Vertex);
                return core::Status::ok();
            }
            if (key->key == platform::KeyCode::F10) {
                (void)setEditorMode(scene::EditMode::Edge);
                return core::Status::ok();
            }
            if (key->key == platform::KeyCode::F11) {
                (void)setEditorMode(scene::EditMode::Face);
                return core::Status::ok();
            }
            if (key->key == platform::KeyCode::F12) {
                selection_masks_.uv_mode = !selection_masks_.uv_mode;
                if (selection_masks_.uv_mode && editor_mode_ == scene::EditMode::Object) {
                    (void)setEditorMode(scene::EditMode::Vertex);
                } else {
                    ++interaction_revision_;
                }
                return core::Status::ok();
            }
            if (key->key == platform::KeyCode::Digit1) {
                setDisplaySmoothness(1);
                return core::Status::ok();
            }
            if (key->key == platform::KeyCode::Digit2) {
                setDisplaySmoothness(2);
                return core::Status::ok();
            }
            if (key->key == platform::KeyCode::Digit3) {
                setDisplaySmoothness(3);
                return core::Status::ok();
            }
            if (key->key == platform::KeyCode::Digit4) {
                applyMayaViewportDisplayKey(4);
                return core::Status::ok();
            }
            if (key->key == platform::KeyCode::Digit5) {
                applyMayaViewportDisplayKey(5);
                return core::Status::ok();
            }
            if (key->key == platform::KeyCode::Digit6) {
                applyMayaViewportDisplayKey(6);
                return core::Status::ok();
            }
            if (key->key == platform::KeyCode::Digit7) {
                applyMayaViewportDisplayKey(7);
                return core::Status::ok();
            }
        }

        if (key->mods.ctrl && !key->mods.alt && key->key == platform::KeyCode::Digit1) {
            toggleIsolateSelect();
            return core::Status::ok();
        }

        if (key->key == platform::KeyCode::F && !key->mods.ctrl) {
            return frameSelectionOrScene();
        }

        if (key->key == platform::KeyCode::Space && !key->mods.ctrl) {
            config_.animate_showcase_rig = !config_.animate_showcase_rig;
            ++interaction_revision_;
            return core::Status::ok();
        }

        if (key->key == platform::KeyCode::Delete || key->key == platform::KeyCode::Backspace) {
            if (editor_mode_ == scene::EditMode::Object) {
                if (selected_object_.isValid() &&
                    selected_object_ != document_.scene.activeCamera() &&
                    !isObjectLocked(selected_object_) &&
                    document_.scene.findObject(selected_object_) != nullptr) {
                    const scene::ObjectId to_delete = selected_object_;
                    setSelectedObject({});
                    return command_stack_.execute(
                        document_.scene,
                        std::make_unique<scene::DeleteObjectCommand>(
                            to_delete, scene::RemovalMode::Recursive));
                }
            } else if (selected_object_.isValid() && !mesh_selection_.empty()) {
                const auto* obj = document_.scene.findObject(selected_object_);
                const auto* ma = (obj != nullptr && obj->mesh_id.has_value())
                                     ? document_.scene.findMesh(obj->mesh_id.value())
                                     : nullptr;
                if (ma != nullptr && ma->editable_mesh.has_value()) {
                    scene::EditableMesh after = ma->editable_mesh.value();
                    core::Status del_st = core::Status::ok();
                    if (editor_mode_ == scene::EditMode::Face) {
                        del_st = after.deleteFaces(mesh_selection_.faces);
                    } else if (editor_mode_ == scene::EditMode::Edge) {
                        del_st = after.deleteEdges(mesh_selection_.edges);
                    } else {
                        del_st = after.deleteVertices(mesh_selection_.vertices);
                    }
                    if (del_st) {
                        mesh_selection_.clear();
                        syncDocumentState();
                        ++interaction_revision_;
                        return command_stack_.execute(
                            document_.scene,
                            std::make_unique<scene::ModifyMeshCommand>(
                                obj->mesh_id.value(),
                                std::move(after),
                                "Delete Mesh Elements",
                                false));
                    }
                }
            }
            return core::Status::ok();
        }

        if (key->mods.ctrl && key->key == platform::KeyCode::Z && !key->mods.shift) {
            if (command_stack_.canUndo()) {
                auto st = command_stack_.undo(document_.scene);
                if (selected_object_.isValid() && document_.scene.findObject(selected_object_) == nullptr) {
                    selected_object_ = {};
                }
                sanitizeMultiSelection();
                sanitizeMeshSelection();
                ++interaction_revision_;
                return st;
            }
            return core::Status::ok();
        }

        if (key->mods.ctrl &&
            (key->key == platform::KeyCode::Y || (key->key == platform::KeyCode::Z && key->mods.shift))) {
            if (command_stack_.canRedo()) {
                auto st = command_stack_.redo(document_.scene);
                if (selected_object_.isValid() && document_.scene.findObject(selected_object_) == nullptr) {
                    selected_object_ = {};
                }
                sanitizeMultiSelection();
                sanitizeMeshSelection();
                ++interaction_revision_;
                return st;
            }
            return core::Status::ok();
        }
    }

    return core::Status::ok();
}

core::Status StudioSession::onUpdate(core::Engine&, const core::FrameTimings& timings) {
    window_.pollEvents();

    const auto fb = window_.framebufferSize();
    const float content_scale = window_.contentScale();
    auto resize_st = renderer_.resize(fb.width, fb.height);
    if (!resize_st) {
        return resize_st;
    }
    if (fb.width != last_fb_width_ || fb.height != last_fb_height_ ||
        std::fabs(content_scale - last_content_scale_) > 1.0e-3f) {
        last_fb_width_ = fb.width;
        last_fb_height_ = fb.height;
        last_content_scale_ = content_scale;
        ++interaction_revision_;
    }

    for (const auto& ev : window_.consumeInputEvents()) {
        auto ev_st = processInputEvent(ev);
        if (!ev_st) {
            return ev_st;
        }
    }

    if (view_transition_.active) {
        const float dt = std::clamp(static_cast<float>(timings.delta_seconds), 0.001f, 0.10f);
        view_transition_.elapsed_seconds += dt;
        const float dur = std::max(0.01f, view_transition_.duration_seconds);
        const float raw_t = std::clamp(view_transition_.elapsed_seconds / dur, 0.0f, 1.0f);
        const float inv = 1.0f - raw_t;
        const float u = 1.0f - inv * inv * inv;

        const float yaw = view_transition_.start_yaw +
                          (view_transition_.goal_yaw - view_transition_.start_yaw) * u;
        const float pitch = view_transition_.start_pitch +
                            (view_transition_.goal_pitch - view_transition_.start_pitch) * u;
        const math::Vec3 tgt = view_transition_.start_target +
                               (view_transition_.goal_target - view_transition_.start_target) * u;
        const float dist = view_transition_.start_distance +
                           (view_transition_.goal_distance - view_transition_.start_distance) * u;
        camera_controller_.setPose(tgt, yaw, pitch, dist);
        syncViewportOrthoHeightFromDistance();
        if (raw_t >= 1.0f) {
            view_transition_.active = false;
        }
        ++interaction_revision_;
    }

    if (timeline_.playing) {
        const double dt = std::clamp(timings.delta_seconds, 0.0, 0.25);
        double next_frame =
            timeline_.current_frame + dt * static_cast<double>(timeline_.fps) * timeline_.playback_direction;
        const double p_start = static_cast<double>(timeline_.playback_start);
        const double p_end = static_cast<double>(timeline_.playback_end);
        const double span = std::max(1.0, p_end - p_start);
        if (timeline_.loop_mode == PlaybackLoopMode::Once) {
            if (next_frame >= p_end) {
                next_frame = p_end;
                timeline_.playing = false;
            }
        } else if (timeline_.loop_mode == PlaybackLoopMode::Oscillate) {
            if (next_frame >= p_end) {
                next_frame = p_end - std::fmod(next_frame - p_end, span);
                timeline_.playback_direction = -1;
            } else if (next_frame <= p_start) {
                next_frame = p_start + std::fmod(p_start - next_frame, span);
                timeline_.playback_direction = 1;
            }
        } else {
            if (next_frame > p_end) {
                next_frame = p_start + std::fmod(next_frame - p_start, span);
            } else if (next_frame < p_start) {
                next_frame = p_end;
            }
        }
        timeline_.current_frame = next_frame;
        evaluateTimelineAtCurrentFrame();
        ++interaction_revision_;
    }

    if (config_.animate_showcase_rig && !gizmo_.isDragging()) {
        if (const auto* rig = document_.scene.findObjectByName("RigPivot"); rig != nullptr) {
            rig_angle_radians_ += static_cast<float>(timings.delta_seconds) * config_.rig_angular_speed_radians;
            math::Transform tf = rig->local_transform;
            tf.rotation = math::Quat::fromAxisAngle(math::Vec3::unitY(), rig_angle_radians_);
            auto tf_st = document_.scene.setObjectTransform(rig->id, tf);
            if (!tf_st) {
                return tf_st;
            }
        }
    }

    // Build 3D viewport overlays:
    //  [0] Depth-tested 3D Camera & Directional Light visualization helpers (excluded from final renders).
    //  [1] Mesh Edit wireframe/vertex/face overlay (foreground pass).
    //  [2] 3D Transform Gizmo (foreground pass).
    std::array<rendering::ViewportOverlayMesh, 3> overlays{};
    std::size_t overlay_count = 0;

    const scene::ObjectId highlight_obj =
        (editor_mode_ == scene::EditMode::Object) ? selected_object_ : scene::ObjectId{};
    const float vp_aspect = (fb.height > 0)
                                ? (static_cast<float>(fb.width) / static_cast<float>(fb.height))
                                : (16.0f / 9.0f);

    if (editor_helpers_visible_) {
        scene_helpers_overlay_mesh_ = buildViewportSceneHelpersMesh(
            document_.scene,
            ViewportHelperBuildOptions{
                .viewport_aspect = vp_aspect,
                .selected_object = highlight_obj,
                .hidden_camera_object =
                    camera_view_active_ ? document_.scene.activeCamera() : scene::ObjectId::invalid(),
            });
        if (!scene_helpers_overlay_mesh_.vertices.empty() &&
            !scene_helpers_overlay_mesh_.indices.empty()) {
            overlays[overlay_count++] = rendering::ViewportOverlayMesh{
                .mesh_data = &scene_helpers_overlay_mesh_,
                .model_matrix = math::Mat4::identity(),
                .depth_test_scene = true,
            };
        }
    }

    if (editor_mode_ != scene::EditMode::Object && selected_object_.isValid()) {
        const auto* obj = document_.scene.findObject(selected_object_);
        const auto* ma = (obj != nullptr && obj->mesh_id.has_value())
                             ? document_.scene.findMesh(obj->mesh_id.value())
                             : nullptr;
        auto ow = document_.scene.computeWorldMatrix(selected_object_);
        if (ma != nullptr && ma->editable_mesh.has_value() && ow.hasValue()) {
            const auto cam_st = activeCameraState();
            const auto ref_frame = TransformGizmo::computeFrame(
                ow.value(),
                cam_st.camera_world,
                cam_st.camera,
                GizmoMode::Translate,
                GizmoSpace::World,
                fb.width,
                fb.height,
                content_scale);
            edit_overlay_mesh_ = ma->editable_mesh->buildEditOverlayMesh(
                editor_mode_,
                mesh_selection_,
                ow.value(),
                ref_frame.scale);
            overlays[overlay_count++] = rendering::ViewportOverlayMesh{
                .mesh_data = &edit_overlay_mesh_,
                .model_matrix = math::Mat4::identity(),
                .depth_test_scene = false,
            };
        }
    }

    if (const auto frame = computeActiveGizmoFrame(); frame.has_value()) {
        const GizmoMode eff_mode =
            (editor_mode_ == scene::EditMode::Object) ? gizmo_.mode() : GizmoMode::Translate;
        gizmo_overlay_mesh_ = TransformGizmo::buildRenderMesh(
            eff_mode,
            frame.value(),
            gizmo_.hoveredHandle(),
            gizmo_.activeHandle());
        overlays[overlay_count++] = rendering::ViewportOverlayMesh{
            .mesh_data = &gizmo_overlay_mesh_,
            .model_matrix = math::Mat4::identity(),
            .depth_test_scene = false,
        };
    }

    rendering::ViewportCameraOverride cam_override{};
    const rendering::ViewportCameraOverride* override_ptr = nullptr;
    float grid_focus_distance = camera_controller_.distance();
    if (!camera_view_active_) {
        const auto cam_st = activeCameraState();
        cam_override.camera_world = cam_st.camera_world;
        cam_override.camera = cam_st.camera;
        override_ptr = &cam_override;
    } else {
        const auto cam_st = activeCameraState();
        const math::Vec3 cam_eye = cam_st.camera_world.transformPoint(math::Vec3::zero());
        grid_focus_distance = std::max(cam_eye.length(), 1.0f);
    }

    const rendering::ViewportGridOptions grid_options{
        .enabled = editor_grid_visible_,
        .opacity = 1.0f,
        .focus_distance = grid_focus_distance,
    };

    scratch_hidden_layer_objs_.clear();
    scratch_template_objs_.clear();
    for (const auto& layer : display_layers_) {
        if (layer.id == 0 || layer.members.empty()) {
            continue;
        }
        if (!layer.visible) {
            scratch_hidden_layer_objs_.insert(
                scratch_hidden_layer_objs_.end(),
                layer.members.begin(),
                layer.members.end());
        } else if (layer.mode == DisplayLayerMode::Template) {
            scratch_template_objs_.insert(
                scratch_template_objs_.end(),
                layer.members.begin(),
                layer.members.end());
        }
    }

    sanitizeMultiSelection();
    const bool has_custom_display =
        shading_mode_ != rendering::ViewportShadingMode::MaterialTextured ||
        wireframe_on_shaded_ ||
        xray_enabled_ ||
        use_default_lighting_ ||
        isolate_select_active_ ||
        !scratch_hidden_layer_objs_.empty() ||
        !scratch_template_objs_.empty() ||
        selected_objects_.size() > 1;

    rendering::ViewportDisplayOptions display_opts{};
    const rendering::ViewportDisplayOptions* display_opts_ptr = nullptr;
    if (has_custom_display) {
        display_opts.shading_mode = shading_mode_;
        display_opts.wireframe_on_shaded = wireframe_on_shaded_;
        display_opts.xray_enabled = xray_enabled_;
        display_opts.use_default_lighting = use_default_lighting_;
        display_opts.isolate_select_active = isolate_select_active_ && !isolated_objects_.empty();
        display_opts.isolated_objects = isolated_objects_;
        display_opts.hidden_by_layer_objects = scratch_hidden_layer_objs_;
        display_opts.template_objects = scratch_template_objs_;
        if (editor_mode_ == scene::EditMode::Object && selected_objects_.size() > 1) {
            display_opts.multi_selected_objects = selected_objects_;
        }
        display_opts_ptr = &display_opts;
    }

    auto render_st = renderer_.renderScene(
        document_.scene,
        {},
        highlight_obj,
        std::span<const rendering::ViewportOverlayMesh>{overlays.data(), overlay_count},
        override_ptr,
        &grid_options,
        display_opts_ptr);
    if (!render_st) {
        return render_st;
    }

    window_.swapBuffers();
    return core::Status::ok();
}

void StudioSession::onShutdown(core::Engine& engine) noexcept {
    if (gizmo_.isDragging()) {
        (void)cancelActiveGizmoDrag();
    }
    const auto stats = collectResourceStats();
    engine.logger().info(
        "App.Session",
        "Shutting down session (objects=" + std::to_string(stats.scene_object_count) +
            ", meshes=" + std::to_string(stats.mesh_asset_count) +
            ", materials=" + std::to_string(stats.material_count) +
            ", gpu_buffers=" + std::to_string(stats.gpu_mesh_buffer_count) +
            ", gpu_updates=" + std::to_string(stats.gpu_buffer_update_count) +
            ", undo_cmds=" + std::to_string(stats.undo_command_count) + ")");
    renderer_.shutdown();
}

core::Status StudioSession::resetStarterProject() {
    if (gizmo_.isDragging()) {
        (void)cancelActiveGizmoDrag();
    }
    if (renderer_.isInitialized()) {
        renderer_.releaseSceneResources();
    }
    document_ = project::ProjectDocument{};
    document_.metadata.project_name = "Final Effect 3D Starter Project";
    document_.metadata.author = "Final Effect 3D";
    document_.metadata.description = "Interactive 3D studio showcase project";
    auto starter_res = populateStarterScene(document_.scene);
    if (!starter_res) {
        return starter_res.error();
    }
    command_stack_.clear();
    selected_object_ = {};
    editor_mode_ = scene::EditMode::Object;
    mesh_selection_.clear();
    resetMayaSessionAuxiliaryState();
    gizmo_.setHoveredHandle(GizmoHandle::None);
    viewport_camera_.projection = scene::ProjectionType::Perspective;
    camera_view_active_ = false;
    custom_project_loaded_ = false;
    rig_angle_radians_ = 0.0f;
    syncDocumentState();
    syncCameraControllerFromScene();
    ++interaction_revision_;
    return core::Status::ok();
}

core::Status StudioSession::loadProjectFromFile(
    const std::filesystem::path& path,
    const platform::IFileSystem& file_system) {
    if (gizmo_.isDragging()) {
        (void)cancelActiveGizmoDrag();
    }
    auto loaded = project::ProjectSerializer::loadFromFile(path, file_system);
    if (!loaded) {
        return loaded.error();
    }
    if (renderer_.isInitialized()) {
        renderer_.releaseSceneResources();
    }
    document_ = std::move(loaded).value();
    command_stack_.clear();
    resetMayaSessionAuxiliaryState();
    restoreFromDocumentState();
    viewport_camera_.projection = scene::ProjectionType::Perspective;
    camera_view_active_ = false;
    custom_project_loaded_ = true;
    rig_angle_radians_ = 0.0f;
    syncCameraControllerFromScene();
    ++interaction_revision_;
    return core::Status::ok();
}

core::Status StudioSession::saveProjectToFile(
    const std::filesystem::path& path,
    platform::IFileSystem& file_system) const {
    return project::ProjectSerializer::saveToFile(document_, path, file_system);
}

core::Status StudioSession::saveProjectBundleToFile(
    const std::filesystem::path& fe3d_path,
    platform::IFileSystem& file_system) const {
    return project::ProjectSerializer::saveBundleToFile(document_, fe3d_path, file_system);
}

core::ResourceStats StudioSession::collectResourceStats() const noexcept {
    core::ResourceStats stats = renderer_.collectResourceStats(document_.scene);
    stats.undo_command_count = command_stack_.undoCount();
    stats.redo_command_count = command_stack_.redoCount();
    stats.command_history_bytes = command_stack_.totalMemoryBytes();
    return stats;
}

} // namespace finaleffect::app
