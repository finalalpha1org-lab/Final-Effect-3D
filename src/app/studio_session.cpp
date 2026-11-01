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
    resetEditorViewportCamera();
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
    selected_objects_.clear();
    if (selected_object_.isValid()) {
        selected_objects_.push_back(selected_object_);
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
        active_tool_ = MayaActiveTool::Rotate;
    } else if (document_.editor_state.gizmo_tool == "scale") {
        gizmo_.setMode(GizmoMode::Scale);
        active_tool_ = MayaActiveTool::Scale;
    } else {
        gizmo_.setMode(GizmoMode::Translate);
        active_tool_ = MayaActiveTool::Move;
    }
    last_tool_ = active_tool_;
    gizmo_.setSpace(
        document_.editor_state.gizmo_space == "local" ? GizmoSpace::Local : GizmoSpace::World);
    mesh_selection_ = document_.editor_state.mesh_selection;
    sanitizeMeshSelection();
    workspace_.setState(document_.workspace);
}

std::string_view mayaActiveToolSlug(MayaActiveTool tool) noexcept {
    switch (tool) {
        case MayaActiveTool::Select:               return "select";
        case MayaActiveTool::Move:                 return "move";
        case MayaActiveTool::Rotate:               return "rotate";
        case MayaActiveTool::Scale:                return "scale";
        case MayaActiveTool::ShowManipulator:      return "show_manipulator";
        case MayaActiveTool::UniversalManipulator: return "universal";
        case MayaActiveTool::PivotEdit:            return "pivot_edit";
    }
    return "move";
}

std::optional<MayaActiveTool> parseMayaActiveToolSlug(std::string_view slug) noexcept {
    if (slug == "select") return MayaActiveTool::Select;
    if (slug == "move" || slug == "translate") return MayaActiveTool::Move;
    if (slug == "rotate") return MayaActiveTool::Rotate;
    if (slug == "scale") return MayaActiveTool::Scale;
    if (slug == "show_manipulator" || slug == "manip") return MayaActiveTool::ShowManipulator;
    if (slug == "universal") return MayaActiveTool::UniversalManipulator;
    if (slug == "pivot_edit" || slug == "pivot") return MayaActiveTool::PivotEdit;
    return std::nullopt;
}

void StudioSession::setSelectedObject(scene::ObjectId id) noexcept {
    const bool single_already =
        (selected_object_ == id) &&
        ((id.isValid() && selected_objects_.size() == 1u && selected_objects_.front() == id) ||
         (!id.isValid() && selected_objects_.empty()));
    if (!single_already) {
        if (gizmo_.isDragging()) {
            (void)cancelActiveGizmoDrag();
        }
        selected_object_ = id;
        selected_objects_.clear();
        if (id.isValid() && document_.scene.findObject(id) != nullptr) {
            selected_objects_.push_back(id);
        } else {
            selected_object_ = {};
        }
        mesh_selection_.clear();
        if (editor_mode_ != scene::EditMode::Object) {
            const auto* obj = selected_object_.isValid() ? document_.scene.findObject(selected_object_) : nullptr;
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

void StudioSession::setSelectedObjects(std::span<const scene::ObjectId> ids) noexcept {
    if (gizmo_.isDragging()) {
        (void)cancelActiveGizmoDrag();
    }
    selected_objects_.clear();
    for (const auto id : ids) {
        if (id.isValid() && document_.scene.findObject(id) != nullptr &&
            std::find(selected_objects_.begin(), selected_objects_.end(), id) == selected_objects_.end()) {
            selected_objects_.push_back(id);
        }
    }
    selected_object_ = selected_objects_.empty() ? scene::ObjectId{} : selected_objects_.back();
    mesh_selection_.clear();
    if (editor_mode_ != scene::EditMode::Object) {
        const auto* obj = selected_object_.isValid() ? document_.scene.findObject(selected_object_) : nullptr;
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

void StudioSession::toggleObjectInSelection(scene::ObjectId id) noexcept {
    if (!id.isValid() || document_.scene.findObject(id) == nullptr) {
        return;
    }
    std::vector<scene::ObjectId> next = selected_objects_;
    auto it = std::find(next.begin(), next.end(), id);
    if (it != next.end()) {
        next.erase(it);
    } else {
        next.push_back(id);
    }
    setSelectedObjects(next);
}

void StudioSession::sanitizeObjectSelection() noexcept {
    std::erase_if(selected_objects_, [this](scene::ObjectId id) {
        return !id.isValid() || document_.scene.findObject(id) == nullptr;
    });
    if (selected_object_.isValid() && document_.scene.findObject(selected_object_) == nullptr) {
        selected_object_ = selected_objects_.empty() ? scene::ObjectId{} : selected_objects_.back();
    }
    if (selected_object_.isValid() &&
        std::find(selected_objects_.begin(), selected_objects_.end(), selected_object_) == selected_objects_.end()) {
        selected_objects_.push_back(selected_object_);
    }
    syncDocumentState();
}

void StudioSession::setActiveTool(MayaActiveTool tool) noexcept {
    if (gizmo_.isDragging()) {
        (void)cancelActiveGizmoDrag();
    }
    if (active_tool_ != MayaActiveTool::Select && tool != active_tool_) {
        last_tool_ = active_tool_;
    }
    active_tool_ = tool;
    if (tool == MayaActiveTool::Move || tool == MayaActiveTool::PivotEdit ||
        tool == MayaActiveTool::UniversalManipulator) {
        gizmo_.setMode(GizmoMode::Translate);
    } else if (tool == MayaActiveTool::Rotate) {
        gizmo_.setMode(GizmoMode::Rotate);
    } else if (tool == MayaActiveTool::Scale) {
        gizmo_.setMode(GizmoMode::Scale);
    } else if (tool == MayaActiveTool::ShowManipulator) {
        const auto* obj = selected_object_.isValid() ? document_.scene.findObject(selected_object_) : nullptr;
        if (obj != nullptr && (obj->camera.has_value() || obj->directional_light.has_value() ||
                               obj->spot_light.has_value() || obj->area_light.has_value())) {
            gizmo_.setMode(GizmoMode::Rotate);
        } else {
            gizmo_.setMode(GizmoMode::Translate);
        }
    }
    gizmo_.setHoveredHandle(GizmoHandle::None);
    syncDocumentState();
    ++interaction_revision_;
}

void StudioSession::activateLastTool() noexcept {
    setActiveTool(last_tool_);
}

void StudioSession::setViewportDisplay(rendering::ViewportDisplayOptions opts) noexcept {
    opts.mesh_display_smoothness = std::clamp<std::uint8_t>(opts.mesh_display_smoothness, 1u, 3u);
    viewport_display_ = std::move(opts);
    ++interaction_revision_;
}

void StudioSession::setIsolateSelect(bool enabled) noexcept {
    viewport_display_.isolate_select = enabled;
    viewport_display_.isolated_objects.clear();
    if (enabled) {
        viewport_display_.isolated_objects = selected_objects_;
    }
    ++interaction_revision_;
}

math::Vec3 StudioSession::applyWorldSnapping(
    const math::Vec3& world_pos,
    scene::ObjectId exclude_id) const noexcept {
    math::Vec3 p = world_pos;
    if (snapping_.snap_point) {
        float best_d2 = 1.5f * 1.5f;
        math::Vec3 best_v = p;
        bool found = false;
        for (const auto& obj : document_.scene.objects()) {
            if (obj.id == exclude_id || !obj.mesh_id.has_value() || !obj.visible) continue;
            const auto* ma = document_.scene.findMesh(obj.mesh_id.value());
            const auto wm = document_.scene.computeWorldMatrix(obj.id);
            if (ma == nullptr || !wm) continue;
            if (ma->editable_mesh.has_value()) {
                for (const auto& ev : ma->editable_mesh->vertices()) {
                    const math::Vec3 wv = wm.value().transformPoint(ev.position);
                    const float d2 = (wv - p).lengthSquared();
                    if (d2 < best_d2) { best_d2 = d2; best_v = wv; found = true; }
                }
            }
        }
        if (found) return best_v;
    }
    if (snapping_.snap_curve) {
        float best_d2 = 1.5f * 1.5f;
        math::Vec3 best_pt = p;
        bool found = false;
        for (const auto& obj : document_.scene.objects()) {
            if (obj.id == exclude_id || !obj.mesh_id.has_value() || !obj.visible) continue;
            const auto* ma = document_.scene.findMesh(obj.mesh_id.value());
            const auto wm = document_.scene.computeWorldMatrix(obj.id);
            if (ma == nullptr || !wm || !ma->editable_mesh.has_value()) continue;
            const auto& verts = ma->editable_mesh->vertices();
            for (const auto& ed : ma->editable_mesh->edges()) {
                const math::Vec3 a = wm.value().transformPoint(verts[ed.v0].position);
                const math::Vec3 b = wm.value().transformPoint(verts[ed.v1].position);
                const math::Vec3 ab = b - a;
                const float l2 = ab.lengthSquared();
                const float t = (l2 > 1.0e-8f) ? std::clamp((p - a).dot(ab) / l2, 0.0f, 1.0f) : 0.0f;
                const math::Vec3 proj = a + ab * t;
                const float d2 = (proj - p).lengthSquared();
                if (d2 < best_d2) { best_d2 = d2; best_pt = proj; found = true; }
            }
        }
        if (found) return best_pt;
    }
    if (snapping_.snap_surface) {
        const math::Ray down_ray(p + math::Vec3{0.0f, 10.0f, 0.0f}, math::Vec3{0.0f, -1.0f, 0.0f});
        float best_t = 40.0f;
        math::Vec3 best_hit = p;
        bool found = false;
        for (const auto& obj : document_.scene.objects()) {
            if (obj.id == exclude_id || !obj.mesh_id.has_value() || !obj.visible) continue;
            if (const auto wb = document_.scene.computeObjectWorldBounds(obj.id); wb && wb.value().isValid()) {
                if (const auto hit_t = math::intersectRayAabb(down_ray, wb.value(), 0.0f, best_t); hit_t.has_value()) {
                    best_t = hit_t.value();
                    best_hit = down_ray.at(best_t);
                    found = true;
                }
            }
        }
        if (found) return best_hit;
    }
    if (snapping_.snap_grid) {
        const float step = std::max(0.05f, snapping_.grid_step);
        p.x = std::round(p.x / step) * step;
        p.y = std::round(p.y / step) * step;
        p.z = std::round(p.z / step) * step;
    }
    return p;
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
    if (active_tool_ != MayaActiveTool::Select) {
        last_tool_ = active_tool_;
    }
    if (mode == GizmoMode::Translate) active_tool_ = MayaActiveTool::Move;
    else if (mode == GizmoMode::Rotate) active_tool_ = MayaActiveTool::Rotate;
    else if (mode == GizmoMode::Scale) active_tool_ = MayaActiveTool::Scale;
    gizmo_.setMode(mode);
    gizmo_.setHoveredHandle(GizmoHandle::None);
    syncDocumentState();
    ++interaction_revision_;
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
    if (active_tool_ == MayaActiveTool::Select && editor_mode_ == scene::EditMode::Object) {
        return std::nullopt;
    }
    if (!selected_object_.isValid() ||
        (camera_view_active_ && selected_object_ == document_.scene.activeCamera())) {
        return std::nullopt;
    }
    const auto* obj = document_.scene.findObject(selected_object_);
    if (obj == nullptr || !isObjectVisibleInHierarchy(document_.scene, selected_object_)) {
        return std::nullopt;
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
        if (!obj->pivot_offset.approxEqual(math::Vec3::zero(), 1.0e-6f)) {
            const math::Vec3 world_piv = target_world.transformPoint(obj->pivot_offset);
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
    scene::RaycastOptions ray_opts{
        .max_distance = cam_st.camera.far_plane,
        .respect_display_layers = true,
        .allow_meshes = selection_mask_.meshes,
        .allow_lights = selection_mask_.lights,
        .allow_cameras = selection_mask_.cameras,
        .isolated_objects = viewport_display_.isolate_select ? &viewport_display_.isolated_objects : nullptr,
    };
    auto hit = pickSceneOrViewportHelper(
        document_.scene,
        ray.value(),
        allow_helpers,
        hidden_cam,
        ray_opts);
    setSelectedObject(hit.has_value() ? hit->object_id : scene::ObjectId{});
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
        if (prev_mode == DragMode::GizmoObject && gizmo_drag_object_id_.isValid()) {
            return document_.scene.setObjectTransform(gizmo_drag_object_id_, gizmo_drag_initial_tf_);
        }
        if (prev_mode == DragMode::GizmoPivot && gizmo_drag_object_id_.isValid()) {
            return document_.scene.setObjectPivotOffset(gizmo_drag_object_id_, gizmo_drag_initial_pivot_);
        }
        if (prev_mode == DragMode::GizmoMeshElements && gizmo_drag_mesh_id_.isValid()) {
            return document_.scene.updateMeshEditable(gizmo_drag_mesh_id_, gizmo_drag_base_mesh_);
        }
    }
    return core::Status::ok();
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
        if (cmd_st && document_.scene.timeline().auto_key &&
            document_.scene.findAnimationTrack(gizmo_drag_object_id_) != nullptr) {
            const auto* obj = document_.scene.findObject(gizmo_drag_object_id_);
            if (obj != nullptr) {
                std::vector<scene::ObjectAnimationTrack> tracks = document_.scene.animationTracks();
                const float cur_f = document_.scene.timeline().current_frame;
                const math::Vec3 eul = obj->local_transform.rotation.toEulerXYZ();
                const scene::TransformKeyframe kf{
                    .frame = cur_f,
                    .position = obj->local_transform.position,
                    .rotation_deg = {math::radToDeg(eul.x), math::radToDeg(eul.y), math::radToDeg(eul.z)},
                    .scale = obj->local_transform.scale,
                    .visible = obj->visible,
                };
                for (auto& tr : tracks) {
                    if (tr.object_id == gizmo_drag_object_id_) {
                        bool updated = false;
                        for (auto& existing : tr.keyframes) {
                            if (std::fabs(existing.frame - cur_f) <= 1.0e-3f) {
                                existing = kf;
                                updated = true;
                                break;
                            }
                        }
                        if (!updated) tr.keyframes.push_back(kf);
                        break;
                    }
                }
                (void)document_.scene.setAnimationTracks(std::move(tracks));
            }
        }
        return cmd_st;
    }

    if (prev_mode == DragMode::GizmoPivot && gizmo_drag_object_id_.isValid()) {
        const auto* obj = document_.scene.findObject(gizmo_drag_object_id_);
        if (obj == nullptr) {
            return core::Status::ok();
        }
        (void)document_.scene.setObjectPivotOffset(gizmo_drag_object_id_, gizmo_drag_initial_pivot_);
        scene::SetObjectMetadataCommand::MetadataState next_meta{
            .locked = obj->locked,
            .locked_channels_mask = obj->locked_channels_mask,
            .pivot_offset = gizmo_drag_latest_pivot_,
            .custom_attributes = obj->custom_attributes,
        };
        return command_stack_.execute(
            document_.scene,
            std::make_unique<scene::SetObjectMetadataCommand>(
                gizmo_drag_object_id_,
                std::move(next_meta),
                "Edit Pivot"));
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
    for (const auto sel_id : selected_objects_) {
        if (sel_id.isValid() && document_.scene.findObject(sel_id) != nullptr) {
            if (auto b = document_.scene.computeObjectWorldBounds(sel_id); b && b.value().isValid()) {
                target_bounds.expand(b.value());
            }
        }
    }
    if (!target_bounds.isValid() && selected_object_.isValid() &&
        document_.scene.findObject(selected_object_) != nullptr) {
        if (auto bounds_res = document_.scene.computeObjectWorldBounds(selected_object_);
            bounds_res && bounds_res.value().isValid()) {
            target_bounds = bounds_res.value();
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
        if (const auto* layer = document_.scene.findObjectDisplayLayer(obj.id);
            layer != nullptr && !layer->visible) {
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
                // Maya Alt+MMB (and plain MMB) = Pan
                view_transition_.active = false;
                drag_mode_ = DragMode::Pan;
            } else if (mb->button == platform::MouseButton::Right) {
                // Maya Alt+RMB = Continuous Dolly Zoom; plain RMB drag = Orbit (Shift+RMB = Pan)
                view_transition_.active = false;
                drag_mode_ = mb->mods.alt ? DragMode::Dolly : (mb->mods.shift ? DragMode::Pan : DragMode::Orbit);
            } else if (mb->button == platform::MouseButton::Left) {
                if (mb->mods.alt) {
                    // Maya Alt+LMB = Orbit (Alt+Shift+LMB = Pan)
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

                            if (editor_mode_ == scene::EditMode::Object) {
                                const auto* obj = document_.scene.findObject(selected_object_);
                                if (obj != nullptr && obj->locked) {
                                    return core::Status::ok();
                                }
                                if (obj != nullptr && active_tool_ == MayaActiveTool::PivotEdit) {
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
                                        gizmo_drag_initial_pivot_ = obj->pivot_offset;
                                        gizmo_drag_latest_pivot_ = obj->pivot_offset;
                                        gizmo_drag_obj_world_ = ow.value();
                                        drag_mode_ = DragMode::GizmoPivot;
                                        ++interaction_revision_;
                                        return core::Status::ok();
                                    }
                                }
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

                    left_click_shift_ = mb->mods.shift || mb->mods.ctrl;
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
                            if (left_click_shift_) {
                                const std::vector<scene::ObjectId> prev_sel = selected_objects_;
                                const auto hit = pickAtScreenPosition(static_cast<float>(mb->x), static_cast<float>(mb->y));
                                setSelectedObjects(prev_sel);
                                if (hit.has_value()) {
                                    toggleObjectInSelection(hit->object_id);
                                }
                            } else {
                                (void)pickAtScreenPosition(static_cast<float>(mb->x), static_cast<float>(mb->y));
                            }
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

        if (drag_mode_ == DragMode::GizmoObject) {
            const auto ray = computeViewportRay(static_cast<float>(cm->x), static_cast<float>(cm->y));
            if (ray.has_value()) {
                auto upd = gizmo_.updateDrag(ray.value());
                if (upd.hasValue()) {
                    math::Transform next_tf = upd.value().local_transform;
                    const auto* obj = document_.scene.findObject(gizmo_drag_object_id_);
                    if (obj != nullptr) {
                        if (gizmo_.mode() == GizmoMode::Translate &&
                            (snapping_.snap_grid || snapping_.snap_curve || snapping_.snap_point || snapping_.snap_surface)) {
                            math::Mat4 parent_world = math::Mat4::identity();
                            if (obj->parent_id.isValid()) {
                                if (auto pw = document_.scene.computeWorldMatrix(obj->parent_id); pw.hasValue()) {
                                    parent_world = pw.value();
                                }
                            }
                            const math::Vec3 world_pos = parent_world.transformPoint(next_tf.position);
                            const math::Vec3 snapped_world = applyWorldSnapping(world_pos, gizmo_drag_object_id_);
                            if (const auto inv_pw = parent_world.inverted(); inv_pw.has_value()) {
                                next_tf.position = inv_pw->transformPoint(snapped_world);
                            }
                        }
                        const std::uint16_t mask = obj->locked_channels_mask;
                        if (mask != 0u) {
                            if (mask & scene::ChannelBit::kTranslateX) next_tf.position.x = gizmo_drag_initial_tf_.position.x;
                            if (mask & scene::ChannelBit::kTranslateY) next_tf.position.y = gizmo_drag_initial_tf_.position.y;
                            if (mask & scene::ChannelBit::kTranslateZ) next_tf.position.z = gizmo_drag_initial_tf_.position.z;
                            if (mask & (scene::ChannelBit::kRotateX | scene::ChannelBit::kRotateY | scene::ChannelBit::kRotateZ)) {
                                const math::Vec3 init_e = gizmo_drag_initial_tf_.rotation.toEulerXYZ();
                                math::Vec3 cur_e = next_tf.rotation.toEulerXYZ();
                                if (mask & scene::ChannelBit::kRotateX) cur_e.x = init_e.x;
                                if (mask & scene::ChannelBit::kRotateY) cur_e.y = init_e.y;
                                if (mask & scene::ChannelBit::kRotateZ) cur_e.z = init_e.z;
                                next_tf.rotation = math::Quat::fromEulerXYZ(cur_e);
                            }
                            if (mask & scene::ChannelBit::kScaleX) next_tf.scale.x = gizmo_drag_initial_tf_.scale.x;
                            if (mask & scene::ChannelBit::kScaleY) next_tf.scale.y = gizmo_drag_initial_tf_.scale.y;
                            if (mask & scene::ChannelBit::kScaleZ) next_tf.scale.z = gizmo_drag_initial_tf_.scale.z;
                        }
                    }
                    gizmo_drag_latest_tf_ = next_tf;
                    gizmo_drag_mutated_ = true;
                    ++interaction_revision_;
                    return document_.scene.setObjectTransform(gizmo_drag_object_id_, gizmo_drag_latest_tf_);
                }
            }
            return core::Status::ok();
        }

        if (drag_mode_ == DragMode::GizmoPivot) {
            const auto ray = computeViewportRay(static_cast<float>(cm->x), static_cast<float>(cm->y));
            const auto inv_world = gizmo_drag_obj_world_.inverted();
            if (ray.has_value() && inv_world.has_value()) {
                auto upd = gizmo_.updateDrag(ray.value());
                if (upd.hasValue()) {
                    const math::Vec3 local_delta =
                        inv_world->transformDirection(upd.value().world_translation_delta);
                    gizmo_drag_latest_pivot_ = gizmo_drag_initial_pivot_ + local_delta;
                    gizmo_drag_mutated_ = true;
                    ++interaction_revision_;
                    return document_.scene.setObjectPivotOffset(gizmo_drag_object_id_, gizmo_drag_latest_pivot_);
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
        if (drag_mode_ == DragMode::Dolly) {
            view_transition_.active = false;
            const float steps = (dx - dy) * 0.03f;
            if (std::fabs(steps) > 1.0e-5f) {
                camera_controller_.zoom(steps);
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
                setActiveTool(MayaActiveTool::Select);
                return core::Status::ok();
            }
            if (key->key == platform::KeyCode::W) {
                setGizmoMode(GizmoMode::Translate);
                return core::Status::ok();
            }
            if (key->key == platform::KeyCode::E) {
                setGizmoMode(GizmoMode::Rotate);
                return core::Status::ok();
            }
            if (key->key == platform::KeyCode::R) {
                setGizmoMode(GizmoMode::Scale);
                return core::Status::ok();
            }
            if (key->key == platform::KeyCode::T) {
                setActiveTool(MayaActiveTool::ShowManipulator);
                return core::Status::ok();
            }
            if (key->key == platform::KeyCode::Y) {
                activateLastTool();
                return core::Status::ok();
            }
            if (key->key == platform::KeyCode::D) {
                setActiveTool(
                    active_tool_ == MayaActiveTool::PivotEdit ? last_tool_ : MayaActiveTool::PivotEdit);
                return core::Status::ok();
            }
            if (key->key == platform::KeyCode::A) {
                return frameAllScene();
            }
            if (key->key == platform::KeyCode::Tab || key->key == platform::KeyCode::F8) {
                const auto target = (editor_mode_ == scene::EditMode::Object)
                                        ? scene::EditMode::Face
                                        : scene::EditMode::Object;
                uv_component_mode_ = false;
                (void)setEditorMode(target);
                return core::Status::ok();
            }
            if (key->key == platform::KeyCode::F9) {
                uv_component_mode_ = false;
                (void)setEditorMode(scene::EditMode::Vertex);
                return core::Status::ok();
            }
            if (key->key == platform::KeyCode::F10) {
                uv_component_mode_ = false;
                (void)setEditorMode(scene::EditMode::Edge);
                return core::Status::ok();
            }
            if (key->key == platform::KeyCode::F11) {
                uv_component_mode_ = false;
                (void)setEditorMode(scene::EditMode::Face);
                return core::Status::ok();
            }
            if (key->key == platform::KeyCode::F12) {
                if (setEditorMode(scene::EditMode::Vertex).hasValue()) {
                    uv_component_mode_ = true;
                }
                return core::Status::ok();
            }
            if (key->key == platform::KeyCode::Digit1) {
                viewport_display_.mesh_display_smoothness = 1u;
                ++interaction_revision_;
                return core::Status::ok();
            }
            if (key->key == platform::KeyCode::Digit2) {
                viewport_display_.mesh_display_smoothness = 2u;
                ++interaction_revision_;
                return core::Status::ok();
            }
            if (key->key == platform::KeyCode::Digit3) {
                viewport_display_.mesh_display_smoothness = 3u;
                ++interaction_revision_;
                return core::Status::ok();
            }
            if (key->key == platform::KeyCode::Digit4) {
                viewport_display_.shading_mode = rendering::ViewportShadingMode::Wireframe;
                ++interaction_revision_;
                return core::Status::ok();
            }
            if (key->key == platform::KeyCode::Digit5) {
                viewport_display_.shading_mode = rendering::ViewportShadingMode::SmoothShaded;
                ++interaction_revision_;
                return core::Status::ok();
            }
            if (key->key == platform::KeyCode::Digit6) {
                viewport_display_.shading_mode = rendering::ViewportShadingMode::ShadedTextured;
                ++interaction_revision_;
                return core::Status::ok();
            }
            if (key->key == platform::KeyCode::Digit7) {
                viewport_display_.use_all_lights = !viewport_display_.use_all_lights;
                ++interaction_revision_;
                return core::Status::ok();
            }
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
                sanitizeObjectSelection();
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
                sanitizeObjectSelection();
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

    if (timeline_playing_ && !gizmo_.isDragging()) {
        const float dt = std::clamp(static_cast<float>(timings.delta_seconds), 0.001f, 0.10f);
        scene::TimelineSettings tl = document_.scene.timeline();
        float next_f = tl.current_frame + dt * tl.fps * static_cast<float>(timeline_oscillate_dir_);
        if (tl.loop_mode == scene::TimelineLoopMode::Once) {
            if (next_f >= tl.playback_end) {
                next_f = tl.playback_end;
                timeline_playing_ = false;
            }
        } else if (tl.loop_mode == scene::TimelineLoopMode::Oscillate) {
            if (next_f >= tl.playback_end) {
                next_f = tl.playback_end;
                timeline_oscillate_dir_ = -1;
            } else if (next_f <= tl.playback_start) {
                next_f = tl.playback_start;
                timeline_oscillate_dir_ = 1;
            }
        } else {
            if (next_f > tl.playback_end) {
                next_f = tl.playback_start + std::fmod(next_f - tl.playback_start, std::max(1.0f, tl.playback_end - tl.playback_start));
            }
        }
        document_.scene.evaluateAnimationAtFrame(next_f);
        ++interaction_revision_;
    }

    if (config_.animate_showcase_rig && !gizmo_.isDragging() && !timeline_playing_) {
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

    rendering::ViewportDisplayOptions frame_display = viewport_display_;
    frame_display.timeline_playing = timeline_playing_;
    frame_display.secondary_selected_objects.clear();
    if (editor_mode_ == scene::EditMode::Object) {
        for (const auto sid : selected_objects_) {
            if (sid != highlight_obj) {
                frame_display.secondary_selected_objects.push_back(sid);
            }
        }
    }

    auto render_st = renderer_.renderScene(
        document_.scene,
        {},
        highlight_obj,
        std::span<const rendering::ViewportOverlayMesh>{overlays.data(), overlay_count},
        override_ptr,
        &grid_options,
        &frame_display);
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
    selected_objects_.clear();
    active_tool_ = MayaActiveTool::Move;
    last_tool_ = MayaActiveTool::Move;
    uv_component_mode_ = false;
    multi_component_mode_ = false;
    timeline_playing_ = false;
    timeline_oscillate_dir_ = 1;
    viewport_display_ = rendering::ViewportDisplayOptions{};
    editor_mode_ = scene::EditMode::Object;
    mesh_selection_.clear();
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
