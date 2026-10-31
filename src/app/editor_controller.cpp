// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/app/editor_controller.hpp"
#include "finaleffect/core/version.hpp"
#include "finaleffect/math/constants.hpp"
#include "finaleffect/scene/editable_mesh.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <sstream>
#include <utility>
#include <vector>

namespace finaleffect::app {

namespace {

std::string_view editModeName(scene::EditMode mode) noexcept {
    switch (mode) {
        case scene::EditMode::Vertex: return "vertex";
        case scene::EditMode::Edge:   return "edge";
        case scene::EditMode::Face:   return "face";
        case scene::EditMode::Object:
        default:
            return "object";
    }
}

bool sameMaterialValues(const scene::PbrMaterial& a, const scene::PbrMaterial& b) noexcept {
    return a.id == b.id &&
           a.base_color.approxEqual(b.base_color, 1e-4f) &&
           std::fabs(a.metallic - b.metallic) <= 1e-4f &&
           std::fabs(a.roughness - b.roughness) <= 1e-4f &&
           std::fabs(a.ambient_occlusion - b.ambient_occlusion) <= 1e-4f &&
           a.emissive_color.approxEqual(b.emissive_color, 1e-4f) &&
           std::fabs(a.emissive_strength - b.emissive_strength) <= 1e-4f &&
           std::fabs(a.opacity - b.opacity) <= 1e-4f;
}

} // namespace

EditorController::EditorController(StudioSession& session) : session_(session) {
    (void)material_preview_.initialize(96, 96);
    console_entries_.push_back(EditorConsoleEntry{
        .sequence = ++console_seq_,
        .is_error = false,
        .message = "Final Effect 3D Stage 4 Studio ready",
    });
    refreshMaterialPreview();
}

void EditorController::recordStatus(const core::Status& st, std::string_view ok_message) {
    ++state_revision_;
    if (st.hasValue()) {
        last_status_is_error_ = false;
        last_status_message_ = std::string(ok_message);
    } else {
        last_status_is_error_ = true;
        last_status_message_ = st.error().format();
    }

    if (console_entries_.empty() || console_entries_.back().message != last_status_message_) {
        console_entries_.push_back(EditorConsoleEntry{
            .sequence = ++console_seq_,
            .is_error = last_status_is_error_,
            .message = last_status_message_,
        });
        while (console_entries_.size() > 64) {
            console_entries_.pop_front();
        }
    }
}

scene::MaterialId EditorController::selectedMaterial() const noexcept {
    const auto& scene = session_.document().scene;
    if (selected_material_id_.isValid() && scene.findMaterial(selected_material_id_) != nullptr) {
        return selected_material_id_;
    }
    if (session_.selectedObject().isValid()) {
        if (const auto* obj = scene.findObject(session_.selectedObject());
            obj != nullptr && obj->material_id.has_value() &&
            scene.findMaterial(obj->material_id.value()) != nullptr) {
            return obj->material_id.value();
        }
    }
    if (!scene.materials().empty()) {
        return scene.materials().front().id;
    }
    return scene::MaterialId::invalid();
}

void EditorController::refreshMaterialPreview() {
    const scene::MaterialId active_id = selectedMaterial();
    const auto* mat = active_id.isValid() ? session_.document().scene.findMaterial(active_id) : nullptr;
    scene::PbrMaterial target = (mat != nullptr) ? *mat : scene::PbrMaterial{.name = "DefaultPreview"};

    if (!has_preview_snapshot_ || active_id != previewed_material_id_ ||
        !sameMaterialValues(target, previewed_material_snapshot_)) {
        if (material_preview_.renderMaterial(target).hasValue()) {
            previewed_material_id_ = active_id;
            previewed_material_snapshot_ = target;
            has_preview_snapshot_ = true;
        }
    }
}

core::Status EditorController::selectObject(scene::ObjectId id) {
    if (id.isValid() && session_.document().scene.findObject(id) == nullptr) {
        auto st = core::Status::fail(core::ErrorCode::NotFound, "Selected object ID does not exist in scene");
        recordStatus(st, {});
        return st;
    }
    session_.setSelectedObject(id);
    if (id.isValid()) {
        const auto* obj = session_.document().scene.findObject(id);
        if (obj->material_id.has_value()) {
            selected_material_id_ = obj->material_id.value();
            session_.syncDocumentState(selected_material_id_);
        }
        refreshMaterialPreview();
        recordStatus(core::Status::ok(), "Selected '" + obj->name + "'");
    } else {
        recordStatus(core::Status::ok(), "Cleared selection");
    }
    return core::Status::ok();
}

core::Status EditorController::selectObjects(std::span<const scene::ObjectId> ids) {
    for (const auto id : ids) {
        if (id.isValid() && session_.document().scene.findObject(id) == nullptr) {
            auto st = core::Status::fail(core::ErrorCode::NotFound, "Object ID in selection list does not exist");
            recordStatus(st, {});
            return st;
        }
    }
    session_.setSelectedObjects(ids);
    if (session_.selectedObject().isValid()) {
        if (const auto* obj = session_.document().scene.findObject(session_.selectedObject());
            obj != nullptr && obj->material_id.has_value()) {
            selected_material_id_ = obj->material_id.value();
            session_.syncDocumentState(selected_material_id_);
            refreshMaterialPreview();
        }
    }
    recordStatus(
        core::Status::ok(),
        session_.selectedObjects().empty()
            ? "Cleared selection"
            : ("Selected " + std::to_string(session_.selectedObjects().size()) + " object(s)"));
    return core::Status::ok();
}

core::Status EditorController::toggleSelectObject(scene::ObjectId id) {
    if (!id.isValid() || session_.document().scene.findObject(id) == nullptr) {
        auto st = core::Status::fail(core::ErrorCode::NotFound, "Object ID does not exist");
        recordStatus(st, {});
        return st;
    }
    session_.toggleObjectInSelection(id);
    if (session_.selectedObject().isValid()) {
        if (const auto* obj = session_.document().scene.findObject(session_.selectedObject());
            obj != nullptr && obj->material_id.has_value()) {
            selected_material_id_ = obj->material_id.value();
            session_.syncDocumentState(selected_material_id_);
            refreshMaterialPreview();
        }
    }
    recordStatus(
        core::Status::ok(),
        "Selection count: " + std::to_string(session_.selectedObjects().size()));
    return core::Status::ok();
}

core::Status EditorController::selectAllObjects() {
    std::vector<scene::ObjectId> all_ids;
    for (const auto& obj : session_.document().scene.objects()) {
        if (const auto* layer = session_.document().scene.findObjectDisplayLayer(obj.id);
            layer != nullptr && (!layer->visible || layer->display_type != scene::DisplayLayerType::Normal)) {
            continue;
        }
        all_ids.push_back(obj.id);
    }
    return selectObjects(all_ids);
}

core::Status EditorController::invertSelection() {
    const auto& current = session_.selectedObjects();
    std::vector<scene::ObjectId> inverted;
    for (const auto& obj : session_.document().scene.objects()) {
        if (std::find(current.begin(), current.end(), obj.id) == current.end()) {
            if (const auto* layer = session_.document().scene.findObjectDisplayLayer(obj.id);
                layer != nullptr && (!layer->visible || layer->display_type != scene::DisplayLayerType::Normal)) {
                continue;
            }
            inverted.push_back(obj.id);
        }
    }
    return selectObjects(inverted);
}

core::Status EditorController::selectHierarchy() {
    if (session_.selectedObjects().empty()) {
        auto st = core::Status::fail(core::ErrorCode::InvalidState, "Select an object first to select hierarchy");
        recordStatus(st, {});
        return st;
    }
    std::vector<scene::ObjectId> expanded;
    std::vector<scene::ObjectId> stack = session_.selectedObjects();
    while (!stack.empty()) {
        const scene::ObjectId cid = stack.back();
        stack.pop_back();
        if (std::find(expanded.begin(), expanded.end(), cid) != expanded.end()) continue;
        expanded.push_back(cid);
        if (const auto* node = session_.document().scene.findObject(cid)) {
            for (const auto ch : node->children) {
                stack.push_back(ch);
            }
        }
    }
    return selectObjects(expanded);
}

core::Status EditorController::selectSimilar() {
    const auto* lead = session_.document().scene.findObject(session_.selectedObject());
    if (lead == nullptr) {
        auto st = core::Status::fail(core::ErrorCode::InvalidState, "Select a lead object first");
        recordStatus(st, {});
        return st;
    }
    std::vector<scene::ObjectId> similar;
    for (const auto& obj : session_.document().scene.objects()) {
        bool match = false;
        if (lead->mesh_id.has_value() && obj.mesh_id.has_value()) {
            match = (lead->mesh_id == obj.mesh_id) || true;
        } else if (lead->camera.has_value() && obj.camera.has_value()) {
            match = true;
        } else if (lead->hasAnyLight() && obj.hasAnyLight()) {
            match = (lead->lightType() == obj.lightType());
        } else if (!lead->mesh_id.has_value() && !lead->camera.has_value() && !lead->hasAnyLight() &&
                   !obj.mesh_id.has_value() && !obj.camera.has_value() && !obj.hasAnyLight()) {
            match = true;
        }
        if (match) {
            similar.push_back(obj.id);
        }
    }
    return selectObjects(similar);
}

std::optional<scene::RaycastHit> EditorController::pickViewportPixel(float x, float y) {
    auto hit = session_.pickAtScreenPosition(x, y);
    ++state_revision_;
    if (hit.has_value()) {
        if (const auto* obj = session_.document().scene.findObject(hit->object_id); obj != nullptr) {
            if (obj->material_id.has_value()) {
                selected_material_id_ = obj->material_id.value();
                session_.syncDocumentState(selected_material_id_);
                refreshMaterialPreview();
            }
            last_status_is_error_ = false;
            last_status_message_ = "Picked '" + obj->name + "'";
        }
    } else {
        last_status_is_error_ = false;
        last_status_message_ = "Cleared selection";
    }
    return hit;
}

core::Status EditorController::frameSelectionOrScene() {
    auto st = session_.frameSelectionOrScene();
    recordStatus(st, "Framed viewport camera");
    return st;
}

core::Status EditorController::frameAllScene() {
    auto st = session_.frameAllScene();
    recordStatus(st, "Framed all scene objects");
    return st;
}

void EditorController::setActiveTool(MayaActiveTool tool) noexcept {
    session_.setActiveTool(tool);
    recordStatus(core::Status::ok(), "Active tool: " + std::string(mayaActiveToolSlug(tool)));
}

void EditorController::activateLastTool() noexcept {
    session_.activateLastTool();
    recordStatus(core::Status::ok(), "Restored last tool: " + std::string(mayaActiveToolSlug(session_.activeTool())));
}

void EditorController::setViewportDisplay(rendering::ViewportDisplayOptions opts) noexcept {
    session_.setViewportDisplay(std::move(opts));
    ++state_revision_;
}

void EditorController::setIsolateSelect(bool enabled) noexcept {
    session_.setIsolateSelect(enabled);
    recordStatus(
        core::Status::ok(),
        enabled ? "Isolate Select: ON" : "Isolate Select: OFF");
}

void EditorController::setSnapping(const MayaSnappingState& snap) noexcept {
    session_.setSnapping(snap);
    ++state_revision_;
}

void EditorController::setSelectionMask(const ObjectSelectionMask& mask) noexcept {
    session_.setSelectionMask(mask);
    ++state_revision_;
}

void EditorController::setRigAnimationPlaying(bool playing) noexcept {
    session_.setRigAnimationEnabled(playing);
    ++state_revision_;
    last_status_is_error_ = false;
    last_status_message_ = playing ? "Showcase rig animation resumed" : "Showcase rig animation paused";
}

void EditorController::setCameraViewActive(bool active) noexcept {
    session_.setCameraViewActive(active);
    ++state_revision_;
    last_status_is_error_ = false;
    last_status_message_ = active
                               ? "Viewing through active scene camera"
                               : "Switched to free perspective viewport";
}

void EditorController::setEditorHelpersVisible(bool visible) noexcept {
    session_.setEditorHelpersVisible(visible);
    ++state_revision_;
    last_status_is_error_ = false;
    last_status_message_ = visible
                               ? "Shown camera & light viewport helpers"
                               : "Hidden camera & light viewport helpers";
}

void EditorController::setEditorGridVisible(bool visible) noexcept {
    session_.setEditorGridVisible(visible);
    ++state_revision_;
    last_status_is_error_ = false;
    last_status_message_ = visible
                               ? "Shown viewport ground grid"
                               : "Hidden viewport ground grid";
}

void EditorController::setViewportViewPreset(ViewportViewPreset preset, bool smooth) noexcept {
    session_.setViewportViewPreset(preset, smooth);
    ++state_revision_;
    last_status_is_error_ = false;
    last_status_message_ = "Viewport view: " + std::string(session_.viewportViewLabel());
}

void EditorController::resetViewportHomeView(bool smooth) noexcept {
    session_.resetViewportHomeView(smooth);
    ++state_revision_;
    last_status_is_error_ = false;
    last_status_message_ = "Reset viewport to Home view";
}

void EditorController::setViewportProjection(scene::ProjectionType projection) noexcept {
    session_.setViewportProjection(projection);
    ++state_revision_;
    last_status_is_error_ = false;
    last_status_message_ =
        (projection == scene::ProjectionType::Orthographic)
            ? "Editor viewport: Orthographic"
            : "Editor viewport: Perspective";
}

void EditorController::toggleViewportProjection() noexcept {
    setViewportProjection(
        (session_.viewportProjection() == scene::ProjectionType::Orthographic)
            ? scene::ProjectionType::Perspective
            : scene::ProjectionType::Orthographic);
}

core::Status EditorController::orbitEditorObserverCamera(float delta_pixels_x, float delta_pixels_y) {
    auto st = session_.orbitEditorObserverCamera(delta_pixels_x, delta_pixels_y);
    ++state_revision_;
    return st;
}

core::Status EditorController::setEditorMode(scene::EditMode mode) {
    auto st = session_.setEditorMode(mode);
    recordStatus(st, "Switched editor mode to " + std::string(editModeName(mode)));
    return st;
}

void EditorController::setGizmoMode(GizmoMode mode) noexcept {
    session_.setGizmoMode(mode);
    recordStatus(core::Status::ok(), "Gizmo tool: " + std::string(gizmoModeName(mode)));
}

void EditorController::setGizmoSpace(GizmoSpace space) noexcept {
    session_.setGizmoSpace(space);
    recordStatus(core::Status::ok(), "Gizmo space: " + std::string(gizmoSpaceName(space)));
}

core::Status EditorController::cancelGizmoDrag() {
    auto st = session_.cancelActiveGizmoDrag();
    recordStatus(st, "Canceled active gizmo transform");
    return st;
}

core::Status EditorController::selectMeshElement(
    scene::EditMode element_kind,
    std::uint32_t element_index,
    bool additive) {
    if (element_kind == scene::EditMode::Object) {
        auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Invalid mesh element kind");
        recordStatus(st, {});
        return st;
    }
    if (session_.editorMode() != element_kind) {
        auto mst = session_.setEditorMode(element_kind);
        if (!mst) {
            recordStatus(mst, {});
            return mst;
        }
    }

    const auto* obj = session_.document().scene.findObject(session_.selectedObject());
    const auto* ma = (obj != nullptr && obj->mesh_id.has_value())
                         ? session_.document().scene.findMesh(obj->mesh_id.value())
                         : nullptr;
    if (ma == nullptr || !ma->editable_mesh.has_value()) {
        auto st = core::Status::fail(core::ErrorCode::InvalidState, "No editable mesh selected");
        recordStatus(st, {});
        return st;
    }
    const auto& em = ma->editable_mesh.value();
    auto& sel = session_.meshSelection();

    if (element_kind == scene::EditMode::Vertex) {
        if (element_index >= em.vertexCount()) {
            auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Vertex index out of range");
            recordStatus(st, {});
            return st;
        }
        sel.toggleOrSetVertex(element_index, additive);
    } else if (element_kind == scene::EditMode::Edge) {
        if (element_index >= em.edgeCount()) {
            auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Edge index out of range");
            recordStatus(st, {});
            return st;
        }
        sel.toggleOrSetEdge(element_index, additive);
    } else if (element_kind == scene::EditMode::Face) {
        if (element_index >= em.faceCount()) {
            auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Face index out of range");
            recordStatus(st, {});
            return st;
        }
        sel.toggleOrSetFace(element_index, additive);
    }

    session_.syncDocumentState();
    recordStatus(core::Status::ok(), "Updated mesh element selection");
    return core::Status::ok();
}

void EditorController::clearMeshSelection() noexcept {
    session_.meshSelection().clear();
    session_.syncDocumentState();
    recordStatus(core::Status::ok(), "Cleared mesh element selection");
}

core::Status EditorController::selectBoundaryLoopVertices() {
    if (!session_.selectedObject().isValid()) {
        auto st = core::Status::fail(core::ErrorCode::InvalidState, "Select an editable mesh first");
        recordStatus(st, {});
        return st;
    }
    auto mst = session_.setEditorMode(scene::EditMode::Vertex);
    if (!mst) {
        recordStatus(mst, {});
        return mst;
    }
    const auto* obj = session_.document().scene.findObject(session_.selectedObject());
    const auto* ma = session_.document().scene.findMesh(obj->mesh_id.value());
    const auto& em = ma->editable_mesh.value();

    std::vector<std::pair<std::uint32_t, std::uint32_t>> boundary_edges;
    for (const auto& e : em.edges()) {
        if (e.isBoundary()) {
            boundary_edges.emplace_back(e.v0, e.v1);
        }
    }
    if (boundary_edges.size() < 3) {
        auto st = core::Status::fail(
            core::ErrorCode::InvalidState,
            "Mesh is closed (no open boundary loop to select)");
        recordStatus(st, {});
        return st;
    }

    std::vector<bool> used(boundary_edges.size(), false);
    std::vector<std::uint32_t> loop;
    loop.push_back(boundary_edges[0].first);
    loop.push_back(boundary_edges[0].second);
    used[0] = true;

    while (true) {
        const std::uint32_t cur = loop.back();
        if (cur == loop.front() && loop.size() >= 4) {
            loop.pop_back();
            break;
        }
        bool advanced = false;
        for (std::size_t i = 0; i < boundary_edges.size(); ++i) {
            if (used[i]) continue;
            if (boundary_edges[i].first == cur) {
                used[i] = true;
                loop.push_back(boundary_edges[i].second);
                advanced = true;
                break;
            }
            if (boundary_edges[i].second == cur) {
                used[i] = true;
                loop.push_back(boundary_edges[i].first);
                advanced = true;
                break;
            }
        }
        if (!advanced) break;
    }
    if (loop.size() >= 2 && loop.back() == loop.front()) {
        loop.pop_back();
    }

    session_.meshSelection().clear();
    session_.meshSelection().vertices = std::move(loop);
    session_.syncDocumentState();
    recordStatus(
        core::Status::ok(),
        "Selected " + std::to_string(session_.meshSelection().vertices.size()) + " boundary loop vertices");
    return core::Status::ok();
}

core::Status EditorController::translateMeshSelection(
    const math::Vec3& local_delta,
    bool allow_coalesce) {
    if (session_.editorMode() == scene::EditMode::Object || !session_.selectedObject().isValid()) {
        auto st = core::Status::fail(core::ErrorCode::InvalidState, "Enter Mesh Edit mode first");
        recordStatus(st, {});
        return st;
    }
    const auto* obj = session_.document().scene.findObject(session_.selectedObject());
    const auto* ma = (obj != nullptr && obj->mesh_id.has_value())
                         ? session_.document().scene.findMesh(obj->mesh_id.value())
                         : nullptr;
    if (ma == nullptr || !ma->editable_mesh.has_value()) {
        auto st = core::Status::fail(core::ErrorCode::InvalidState, "Selected object has no editable mesh");
        recordStatus(st, {});
        return st;
    }
    const auto& sel = session_.meshSelection();
    if (sel.empty()) {
        auto st = core::Status::fail(core::ErrorCode::InvalidState, "Select at least one mesh element to move");
        recordStatus(st, {});
        return st;
    }

    scene::EditableMesh after = ma->editable_mesh.value();
    core::Status op_st = core::Status::ok();
    if (session_.editorMode() == scene::EditMode::Vertex) {
        op_st = after.translateVertices(sel.vertices, local_delta);
    } else if (session_.editorMode() == scene::EditMode::Edge) {
        op_st = after.translateEdges(sel.edges, local_delta);
    } else {
        op_st = after.translateFaces(sel.faces, local_delta);
    }
    if (!op_st) {
        recordStatus(op_st, {});
        return op_st;
    }

    auto cmd_st = session_.commandStack().execute(
        session_.document().scene,
        std::make_unique<scene::ModifyMeshCommand>(
            obj->mesh_id.value(),
            std::move(after),
            "Translate Mesh Selection",
            allow_coalesce));
    recordStatus(cmd_st, "Moved selected mesh elements");
    return cmd_st;
}

core::Status EditorController::extrudeSelectedFaces(float distance) {
    if (session_.editorMode() != scene::EditMode::Face || !session_.selectedObject().isValid()) {
        auto st = core::Status::fail(core::ErrorCode::InvalidState, "Switch to Face Edit mode to extrude faces");
        recordStatus(st, {});
        return st;
    }
    const auto& sel = session_.meshSelection();
    if (sel.faces.empty()) {
        auto st = core::Status::fail(core::ErrorCode::InvalidState, "Select one or more faces to extrude");
        recordStatus(st, {});
        return st;
    }
    const auto* obj = session_.document().scene.findObject(session_.selectedObject());
    const auto* ma = (obj != nullptr && obj->mesh_id.has_value())
                         ? session_.document().scene.findMesh(obj->mesh_id.value())
                         : nullptr;
    if (ma == nullptr || !ma->editable_mesh.has_value()) {
        auto st = core::Status::fail(core::ErrorCode::InvalidState, "Selected object has no editable mesh");
        recordStatus(st, {});
        return st;
    }

    scene::EditableMesh after = ma->editable_mesh.value();
    auto ext_res = after.extrudeFaces(sel.faces, distance);
    if (!ext_res) {
        core::Status err(ext_res.error());
        recordStatus(err, {});
        return err;
    }

    auto cmd_st = session_.commandStack().execute(
        session_.document().scene,
        std::make_unique<scene::ModifyMeshCommand>(
            obj->mesh_id.value(),
            std::move(after),
            "Extrude Faces",
            false));
    if (cmd_st) {
        session_.meshSelection().faces = ext_res.value();
        session_.syncDocumentState();
    }
    recordStatus(cmd_st, "Extruded selected faces");
    return cmd_st;
}

core::Status EditorController::deleteMeshSelection() {
    if (session_.editorMode() == scene::EditMode::Object || !session_.selectedObject().isValid()) {
        auto st = core::Status::fail(core::ErrorCode::InvalidState, "Enter Mesh Edit mode to delete elements");
        recordStatus(st, {});
        return st;
    }
    const auto& sel = session_.meshSelection();
    if (sel.empty()) {
        auto st = core::Status::fail(core::ErrorCode::InvalidState, "No mesh elements selected to delete");
        recordStatus(st, {});
        return st;
    }
    const auto* obj = session_.document().scene.findObject(session_.selectedObject());
    const auto* ma = (obj != nullptr && obj->mesh_id.has_value())
                         ? session_.document().scene.findMesh(obj->mesh_id.value())
                         : nullptr;
    if (ma == nullptr || !ma->editable_mesh.has_value()) {
        auto st = core::Status::fail(core::ErrorCode::InvalidState, "Selected object has no editable mesh");
        recordStatus(st, {});
        return st;
    }

    scene::EditableMesh after = ma->editable_mesh.value();
    core::Status del_st = core::Status::ok();
    if (session_.editorMode() == scene::EditMode::Face) {
        del_st = after.deleteFaces(sel.faces);
    } else if (session_.editorMode() == scene::EditMode::Edge) {
        del_st = after.deleteEdges(sel.edges);
    } else {
        del_st = after.deleteVertices(sel.vertices);
    }
    if (!del_st) {
        recordStatus(del_st, {});
        return del_st;
    }

    auto cmd_st = session_.commandStack().execute(
        session_.document().scene,
        std::make_unique<scene::ModifyMeshCommand>(
            obj->mesh_id.value(),
            std::move(after),
            "Delete Mesh Elements",
            false));
    if (cmd_st) {
        session_.meshSelection().clear();
        session_.syncDocumentState();
    }
    recordStatus(cmd_st, "Deleted selected mesh elements");
    return cmd_st;
}

core::Status EditorController::createFaceFromSelectedVertices() {
    if (session_.editorMode() != scene::EditMode::Vertex || !session_.selectedObject().isValid()) {
        auto st = core::Status::fail(
            core::ErrorCode::InvalidState,
            "Switch to Vertex Edit mode and select >= 3 boundary vertices");
        recordStatus(st, {});
        return st;
    }
    const auto& sel = session_.meshSelection();
    if (sel.vertices.size() < 3) {
        auto st = core::Status::fail(
            core::ErrorCode::InvalidArgument,
            "Select at least 3 boundary vertices to create a face");
        recordStatus(st, {});
        return st;
    }
    const auto* obj = session_.document().scene.findObject(session_.selectedObject());
    const auto* ma = (obj != nullptr && obj->mesh_id.has_value())
                         ? session_.document().scene.findMesh(obj->mesh_id.value())
                         : nullptr;
    if (ma == nullptr || !ma->editable_mesh.has_value()) {
        auto st = core::Status::fail(core::ErrorCode::InvalidState, "Selected object has no editable mesh");
        recordStatus(st, {});
        return st;
    }

    scene::EditableMesh after = ma->editable_mesh.value();
    auto face_res = after.fillBoundaryLoop(sel.vertices);
    if (!face_res) {
        core::Status err(face_res.error());
        recordStatus(err, {});
        return err;
    }
    if (selectedMaterial().isValid()) {
        const std::uint32_t fi = face_res.value();
        (void)after.assignMaterialToFaces({&fi, 1}, selectedMaterial());
    }

    auto cmd_st = session_.commandStack().execute(
        session_.document().scene,
        std::make_unique<scene::ModifyMeshCommand>(
            obj->mesh_id.value(),
            std::move(after),
            "Create Polygon Face",
            false));
    if (cmd_st) {
        (void)session_.setEditorMode(scene::EditMode::Face);
        session_.meshSelection().faces = {face_res.value()};
        session_.syncDocumentState();
    }
    recordStatus(cmd_st, "Created new polygon face from boundary vertices");
    return cmd_st;
}

core::Status EditorController::assignMaterialToSelectedFaces(scene::MaterialId material_id) {
    if (session_.editorMode() != scene::EditMode::Face || !session_.selectedObject().isValid()) {
        auto st = core::Status::fail(core::ErrorCode::InvalidState, "Switch to Face Edit mode first");
        recordStatus(st, {});
        return st;
    }
    const auto& sel = session_.meshSelection();
    if (sel.faces.empty()) {
        auto st = core::Status::fail(core::ErrorCode::InvalidState, "Select one or more faces first");
        recordStatus(st, {});
        return st;
    }
    if (material_id.isValid() && session_.document().scene.findMaterial(material_id) == nullptr) {
        auto st = core::Status::fail(core::ErrorCode::NotFound, "Material ID not found in scene");
        recordStatus(st, {});
        return st;
    }
    const auto* obj = session_.document().scene.findObject(session_.selectedObject());
    const auto* ma = (obj != nullptr && obj->mesh_id.has_value())
                         ? session_.document().scene.findMesh(obj->mesh_id.value())
                         : nullptr;
    if (ma == nullptr || !ma->editable_mesh.has_value()) {
        auto st = core::Status::fail(core::ErrorCode::InvalidState, "Selected object has no editable mesh");
        recordStatus(st, {});
        return st;
    }

    scene::EditableMesh after = ma->editable_mesh.value();
    auto af_st = after.assignMaterialToFaces(sel.faces, material_id);
    if (!af_st) {
        recordStatus(af_st, {});
        return af_st;
    }

    auto cmd_st = session_.commandStack().execute(
        session_.document().scene,
        std::make_unique<scene::ModifyMeshCommand>(
            obj->mesh_id.value(),
            std::move(after),
            "Assign Face Material",
            false));
    recordStatus(cmd_st, "Assigned PBR material to selected faces");
    return cmd_st;
}

std::string EditorController::generateUniqueMaterialName(std::string_view base_prefix) const {
    const auto& scene = session_.document().scene;
    if (scene.findMaterialByName(base_prefix) == nullptr) {
        return std::string(base_prefix);
    }
    for (std::uint32_t idx = 1; idx < 10000; ++idx) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.*s.%03u", static_cast<int>(base_prefix.size()), base_prefix.data(), idx);
        if (scene.findMaterialByName(buf) == nullptr) {
            return std::string(buf);
        }
    }
    return std::string(base_prefix) + "_copy";
}

core::Status EditorController::selectMaterial(scene::MaterialId id) {
    if (!id.isValid() || session_.document().scene.findMaterial(id) == nullptr) {
        auto st = core::Status::fail(core::ErrorCode::NotFound, "Material ID not found in scene");
        recordStatus(st, {});
        return st;
    }
    selected_material_id_ = id;
    session_.syncDocumentState(id);
    refreshMaterialPreview();
    const auto* mat = session_.document().scene.findMaterial(id);
    recordStatus(core::Status::ok(), "Selected material '" + mat->name + "'");
    return core::Status::ok();
}

core::Result<scene::MaterialId> EditorController::createMaterial(const scene::PbrMaterial& material) {
    scene::PbrMaterial clean = material.sanitized();
    clean.id = scene::MaterialId::invalid();
    clean.name = generateUniqueMaterialName(clean.name.empty() ? "PBR Material" : clean.name);

    auto cmd = std::make_unique<scene::CreateMaterialCommand>(clean);
    const auto* raw = cmd.get();
    auto st = session_.commandStack().execute(session_.document().scene, std::move(cmd));
    if (!st) {
        recordStatus(st, {});
        return st.error();
    }
    const scene::MaterialId new_id = raw->createdMaterialId();
    selected_material_id_ = new_id;
    session_.syncDocumentState(new_id);
    refreshMaterialPreview();
    recordStatus(core::Status::ok(), "Created material '" + clean.name + "'");
    return core::Result<scene::MaterialId>::ok(new_id);
}

core::Result<scene::MaterialId> EditorController::duplicateMaterial(scene::MaterialId id) {
    const auto* src = session_.document().scene.findMaterial(id);
    if (src == nullptr) {
        auto err = core::Status::fail(core::ErrorCode::NotFound, "Source material not found");
        recordStatus(err, {});
        return err.error();
    }
    scene::PbrMaterial copy = *src;
    copy.id = scene::MaterialId::invalid();
    copy.name = generateUniqueMaterialName(src->name);
    return createMaterial(copy);
}

core::Status EditorController::renameMaterial(scene::MaterialId id, std::string new_name) {
    const auto* existing = session_.document().scene.findMaterial(id);
    if (existing == nullptr) {
        auto st = core::Status::fail(core::ErrorCode::NotFound, "Material not found");
        recordStatus(st, {});
        return st;
    }
    if (new_name.empty()) {
        auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Material name cannot be empty");
        recordStatus(st, {});
        return st;
    }
    scene::PbrMaterial updated = *existing;
    updated.name = std::move(new_name);
    auto st = session_.commandStack().execute(
        session_.document().scene,
        std::make_unique<scene::UpdateMaterialCommand>(id, updated, "Rename Material", false));
    recordStatus(st, "Renamed material");
    return st;
}

core::Status EditorController::deleteMaterial(scene::MaterialId id) {
    const auto* mat = session_.document().scene.findMaterial(id);
    if (mat == nullptr) {
        auto st = core::Status::fail(core::ErrorCode::NotFound, "Material not found");
        recordStatus(st, {});
        return st;
    }
    const std::string mat_name = mat->name;
    auto st = session_.commandStack().execute(
        session_.document().scene,
        std::make_unique<scene::DeleteMaterialCommand>(id));
    if (st) {
        if (selected_material_id_ == id) {
            selected_material_id_ = scene::MaterialId::invalid();
        }
        refreshMaterialPreview();
    }
    recordStatus(st, "Deleted material '" + mat_name + "'");
    return st;
}

core::Status EditorController::updateMaterial(
    scene::MaterialId id,
    const scene::PbrMaterial& material,
    bool allow_coalesce) {
    const auto* existing = session_.document().scene.findMaterial(id);
    if (existing == nullptr) {
        auto st = core::Status::fail(core::ErrorCode::NotFound, "Material not found");
        recordStatus(st, {});
        return st;
    }
    scene::PbrMaterial updated = material.sanitized();
    updated.id = id;
    if (material.name.empty()) {
        updated.name = existing->name;
    }

    auto st = session_.commandStack().execute(
        session_.document().scene,
        std::make_unique<scene::UpdateMaterialCommand>(id, updated, "Update Material", allow_coalesce));
    if (st) {
        selected_material_id_ = id;
        session_.syncDocumentState(id);
        refreshMaterialPreview();
    }
    recordStatus(st, "Updated PBR material '" + updated.name + "'");
    return st;
}

core::Status EditorController::assignObjectMaterial(
    scene::ObjectId object_id,
    std::optional<scene::MaterialId> material_id) {
    auto st = session_.commandStack().execute(
        session_.document().scene,
        std::make_unique<scene::AssignObjectMaterialCommand>(object_id, material_id));
    if (st && material_id.has_value() && material_id->isValid()) {
        selected_material_id_ = material_id.value();
        session_.syncDocumentState(selected_material_id_);
        refreshMaterialPreview();
    }
    recordStatus(st, material_id.has_value() ? "Assigned material to object" : "Cleared object material");
    return st;
}

void EditorController::setPanelVisible(project::WorkspacePanelId panel, bool visible) noexcept {
    session_.workspace().setPanelVisible(panel, visible);
    session_.syncDocumentState();
    recordStatus(
        core::Status::ok(),
        std::string(visible ? "Shown panel: " : "Hidden panel: ") +
            std::string(project::workspacePanelTitle(panel)));
}

void EditorController::setWorkspaceDockRatios(
    float left_width,
    float right_width,
    float bottom_height) noexcept {
    session_.workspace().setDockRatios(left_width, right_width, bottom_height);
    session_.syncDocumentState();
    ++state_revision_;
}

void EditorController::setWorkspacePanelDock(
    project::WorkspacePanelId panel,
    project::DockRegion region,
    std::uint32_t tab_order) noexcept {
    session_.workspace().setPanelDock(panel, region, tab_order);
    session_.syncDocumentState();
    recordStatus(
        core::Status::ok(),
        "Moved " + std::string(project::workspacePanelTitle(panel)) + " to " +
            std::string(project::dockRegionSlug(region)) + " dock");
}

void EditorController::applyWorkspacePreset(WorkspacePreset preset) noexcept {
    session_.workspace().applyPreset(preset);
    session_.syncDocumentState();
    recordStatus(core::Status::ok(), "Applied workspace preset: " + session_.workspace().state().layout_name);
}

core::Status EditorController::applyWorkspacePresetByName(std::string_view preset_name) {
    auto st = session_.workspace().applyPresetByName(preset_name);
    if (st) {
        session_.syncDocumentState();
    }
    recordStatus(st, "Applied workspace preset: " + session_.workspace().state().layout_name);
    return st;
}

void EditorController::setTextInputFocused(bool focused) noexcept {
    session_.setTextInputFocused(focused);
}

core::Result<std::string> EditorController::exportWorkspaceLayoutJson() const {
    return session_.workspace().serializeLayoutToJson(true);
}

core::Status EditorController::importWorkspaceLayoutJson(std::string_view json_text) {
    auto st = session_.workspace().deserializeLayoutFromJson(json_text);
    if (st) {
        session_.syncDocumentState();
    }
    recordStatus(st, "Restored workspace layout");
    return st;
}

core::Result<scene::MeshId> EditorController::ensurePresetMesh(ScenePreset preset) {
    auto& scene = session_.document().scene;

    std::string_view target_name;
    switch (preset) {
        case ScenePreset::Cube:     target_name = "HeroCubeMesh"; break;
        case ScenePreset::Pyramid:  target_name = "AccentPyramidMesh"; break;
        case ScenePreset::Plane:    target_name = "StudioPlaneMesh"; break;
        case ScenePreset::Sphere:   target_name = "StudioSphereMesh"; break;
        case ScenePreset::Cylinder: target_name = "StudioCylinderMesh"; break;
        case ScenePreset::Cone:     target_name = "StudioConeMesh"; break;
        case ScenePreset::Torus:    target_name = "StudioTorusMesh"; break;
        default:
            return core::Result<scene::MeshId>::fail(core::ErrorCode::InvalidArgument, "Not a mesh preset");
    }

    for (const auto& existing : scene.meshes()) {
        if (existing.name == target_name) {
            return core::Result<scene::MeshId>::ok(existing.id);
        }
    }

    if (preset == ScenePreset::Cube) {
        return scene.registerEditableMesh(
            std::string(target_name),
            scene::EditableMesh::createCube(1.25f, math::Vec3{0.86f, 0.48f, 0.22f}));
    }
    if (preset == ScenePreset::Pyramid) {
        return scene.registerEditableMesh(
            std::string(target_name),
            scene::EditableMesh::createPyramid(0.95f, 1.25f, math::Vec3{0.24f, 0.68f, 0.88f}));
    }
    if (preset == ScenePreset::Sphere) {
        return scene.registerEditableMesh(
            std::string(target_name),
            scene::EditableMesh::createUvSphere(0.75f, 14, 24, math::Vec3{0.85f, 0.88f, 0.92f}));
    }
    if (preset == ScenePreset::Cylinder) {
        return scene.registerEditableMesh(
            std::string(target_name),
            scene::EditableMesh::createCylinder(0.55f, 1.25f, 20, math::Vec3{0.84f, 0.86f, 0.90f}));
    }
    if (preset == ScenePreset::Cone) {
        return scene.registerEditableMesh(
            std::string(target_name),
            scene::EditableMesh::createCone(0.60f, 1.25f, 20, math::Vec3{0.84f, 0.86f, 0.90f}));
    }
    if (preset == ScenePreset::Torus) {
        return scene.registerEditableMesh(
            std::string(target_name),
            scene::EditableMesh::createTorus(0.65f, 0.22f, 24, 12, math::Vec3{0.84f, 0.86f, 0.90f}));
    }
    return scene.registerEditableMesh(
        std::string(target_name),
        scene::EditableMesh::createGridPlane(4.0f, 4));
}

std::string EditorController::generateUniqueObjectName(std::string_view base_prefix) const {
    const auto& scene = session_.document().scene;
    if (scene.findObjectByName(base_prefix) == nullptr) {
        return std::string(base_prefix);
    }
    for (std::uint32_t idx = 1; idx < 10000; ++idx) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.*s.%03u", static_cast<int>(base_prefix.size()), base_prefix.data(), idx);
        if (scene.findObjectByName(buf) == nullptr) {
            return std::string(buf);
        }
    }
    return std::string(base_prefix) + "_copy";
}

core::Result<scene::ObjectId> EditorController::addPresetObject(
    ScenePreset preset,
    scene::ObjectId parent_id) {
    auto& scene = session_.document().scene;
    if (parent_id.isValid() && scene.findObject(parent_id) == nullptr) {
        return core::Result<scene::ObjectId>::fail(core::ErrorCode::NotFound, "Parent object not found");
    }

    std::string base_name = "Object";
    std::optional<scene::MeshId> mesh_id;
    std::optional<scene::CameraComponent> camera_comp;
    scene::AnyLightComponent any_light{std::monostate{}};
    std::optional<scene::MaterialId> mat_id;
    if (selectedMaterial().isValid()) {
        mat_id = selectedMaterial();
    }

    const float ring_angle = static_cast<float>(spawn_counter_++) * 1.15f;
    const float ring_radius = 1.35f;
    math::Transform init_tf = math::Transform::identity();
    init_tf.position = {
        std::cos(ring_angle) * ring_radius,
        0.75f,
        std::sin(ring_angle) * ring_radius,
    };

    switch (preset) {
        case ScenePreset::Cube: {
            base_name = "Cube";
            auto m = ensurePresetMesh(preset);
            if (!m) return m.error();
            mesh_id = m.value();
            break;
        }
        case ScenePreset::Pyramid: {
            base_name = "Pyramid";
            auto m = ensurePresetMesh(preset);
            if (!m) return m.error();
            mesh_id = m.value();
            break;
        }
        case ScenePreset::Sphere: {
            base_name = "Sphere";
            auto m = ensurePresetMesh(preset);
            if (!m) return m.error();
            mesh_id = m.value();
            break;
        }
        case ScenePreset::Cylinder: {
            base_name = "pCylinder";
            auto m = ensurePresetMesh(preset);
            if (!m) return m.error();
            mesh_id = m.value();
            break;
        }
        case ScenePreset::Cone: {
            base_name = "pCone";
            auto m = ensurePresetMesh(preset);
            if (!m) return m.error();
            mesh_id = m.value();
            break;
        }
        case ScenePreset::Torus: {
            base_name = "pTorus";
            auto m = ensurePresetMesh(preset);
            if (!m) return m.error();
            mesh_id = m.value();
            break;
        }
        case ScenePreset::Plane: {
            base_name = "Plane";
            auto m = ensurePresetMesh(preset);
            if (!m) return m.error();
            mesh_id = m.value();
            init_tf.position = {0.0f, 0.0f, 0.0f};
            break;
        }
        case ScenePreset::EmptyPivot: {
            base_name = "Pivot";
            init_tf.position = {0.0f, 0.0f, 0.0f};
            mat_id = std::nullopt;
            break;
        }
        case ScenePreset::DirectionalLight: {
            base_name = "KeyLight";
            init_tf.position = {
                1.10f + 0.45f * std::sin(ring_angle),
                2.35f,
                -0.45f + 0.45f * std::cos(ring_angle),
            };
            init_tf.rotation = math::Quat::fromEulerXYZ(
                {math::degToRad(-46.0f), math::degToRad(36.0f), 0.0f});
            any_light = scene::DirectionalLightComponent{
                .color = {1.0f, 0.96f, 0.90f},
                .intensity = 1.4f,
            };
            mat_id = std::nullopt;
            break;
        }
        case ScenePreset::PointLight: {
            base_name = "PointLight";
            init_tf.position = {
                1.25f * std::cos(ring_angle),
                1.85f,
                1.25f * std::sin(ring_angle),
            };
            any_light = scene::PointLightComponent{
                .color = {1.0f, 0.92f, 0.80f},
                .intensity = 8.0f,
                .enabled = true,
                .range = 12.0f,
                .attenuation = 2.0f,
            };
            mat_id = std::nullopt;
            break;
        }
        case ScenePreset::SpotLight: {
            base_name = "SpotLight";
            const math::Vec3 spot_eye{
                1.65f + 0.35f * std::cos(ring_angle),
                2.55f,
                1.55f + 0.35f * std::sin(ring_angle),
            };
            const math::Vec3 spot_target{0.0f, 0.65f, 0.0f};
            const math::Vec3 off = spot_eye - spot_target;
            const float horiz = std::sqrt(off.x * off.x + off.z * off.z);
            init_tf.position = spot_eye;
            init_tf.rotation = math::Quat::fromEulerXYZ(
                {-std::atan2(off.y, horiz), std::atan2(off.x, off.z), 0.0f});
            any_light = scene::SpotLightComponent{
                .color = {0.96f, 0.98f, 1.0f},
                .intensity = 14.0f,
                .enabled = true,
                .range = 16.0f,
                .attenuation = 2.0f,
                .inner_cone_radians = math::degToRad(18.0f),
                .outer_cone_radians = math::degToRad(34.0f),
                .cone_falloff = 1.0f,
            };
            mat_id = std::nullopt;
            break;
        }
        case ScenePreset::AreaLight: {
            base_name = "AreaLight";
            const math::Vec3 area_eye{
                -1.55f + 0.35f * std::sin(ring_angle),
                2.15f,
                1.55f + 0.35f * std::cos(ring_angle),
            };
            const math::Vec3 area_target{0.0f, 0.70f, 0.0f};
            const math::Vec3 off = area_eye - area_target;
            const float horiz = std::sqrt(off.x * off.x + off.z * off.z);
            init_tf.position = area_eye;
            init_tf.rotation = math::Quat::fromEulerXYZ(
                {-std::atan2(off.y, horiz), std::atan2(off.x, off.z), 0.0f});
            any_light = scene::AreaLightComponent{
                .shape = scene::AreaLightShape::Rectangle,
                .color = {1.0f, 0.96f, 0.90f},
                .intensity = 10.0f,
                .enabled = true,
                .width = 1.6f,
                .height = 1.0f,
                .range = 16.0f,
                .two_sided = false,
            };
            mat_id = std::nullopt;
            break;
        }
        case ScenePreset::Camera: {
            base_name = "Camera";
            const math::Vec3 cam_eye{
                -1.55f + 0.40f * std::cos(ring_angle),
                1.85f,
                1.95f + 0.35f * std::sin(ring_angle),
            };
            const math::Vec3 cam_target{0.0f, 0.72f, 0.0f};
            const math::Vec3 cam_off = cam_eye - cam_target;
            const float cam_horiz = std::sqrt(cam_off.x * cam_off.x + cam_off.z * cam_off.z);
            init_tf.position = cam_eye;
            init_tf.rotation = math::Quat::fromEulerXYZ(
                {-std::atan2(cam_off.y, cam_horiz), std::atan2(cam_off.x, cam_off.z), 0.0f});
            camera_comp = scene::CameraComponent{};
            mat_id = std::nullopt;
            break;
        }
    }

    const std::string unique_name = generateUniqueObjectName(base_name);
    auto create_cmd = std::make_unique<scene::CreateObjectCommand>(
        unique_name,
        parent_id,
        init_tf,
        mesh_id,
        camera_comp,
        std::nullopt,
        mat_id);
    create_cmd->setAnyLightComponent(std::move(any_light));
    const auto* raw_cmd = create_cmd.get();

    auto st = session_.commandStack().execute(scene, std::move(create_cmd));
    if (!st) {
        recordStatus(st, {});
        return st.error();
    }

    const scene::ObjectId new_id = raw_cmd->createdObjectId();
    session_.setSelectedObject(new_id);
    recordStatus(core::Status::ok(), "Created '" + unique_name + "'");
    return core::Result<scene::ObjectId>::ok(new_id);
}

core::Result<scene::ObjectId> EditorController::duplicateSelectedObject() {
    const scene::ObjectId sel = session_.selectedObject();
    if (!sel.isValid()) {
        auto err = core::Status::fail(core::ErrorCode::InvalidState, "Select an object before duplicating");
        recordStatus(err, {});
        return err.error();
    }
    const auto* src = session_.document().scene.findObject(sel);
    if (src == nullptr) {
        auto err = core::Status::fail(core::ErrorCode::NotFound, "Selected object no longer exists");
        recordStatus(err, {});
        return err.error();
    }

    math::Transform dup_tf = src->local_transform;
    dup_tf.position += math::Vec3{0.65f, 0.0f, 0.65f};
    const std::string dup_name = generateUniqueObjectName(src->name);

    auto cmd = std::make_unique<scene::CreateObjectCommand>(
        dup_name,
        src->parent_id,
        dup_tf,
        src->mesh_id,
        src->camera,
        std::nullopt,
        src->material_id);
    cmd->setAnyLightComponent(src->anyLightComponent());
    const auto* raw = cmd.get();

    auto st = session_.commandStack().execute(session_.document().scene, std::move(cmd));
    if (!st) {
        recordStatus(st, {});
        return st.error();
    }

    const scene::ObjectId new_id = raw->createdObjectId();
    session_.setSelectedObject(new_id);
    recordStatus(core::Status::ok(), "Duplicated '" + dup_name + "'");
    return core::Result<scene::ObjectId>::ok(new_id);
}

core::Status EditorController::deleteSelectedObject(scene::RemovalMode mode) {
    const scene::ObjectId sel = session_.selectedObject();
    if (!sel.isValid()) {
        auto st = core::Status::fail(core::ErrorCode::InvalidState, "No object selected to delete");
        recordStatus(st, {});
        return st;
    }
    if (sel == session_.document().scene.activeCamera()) {
        auto st = core::Status::fail(
            core::ErrorCode::InvalidState,
            "Cannot delete the active viewport camera; switch active camera first");
        recordStatus(st, {});
        return st;
    }
    const auto* obj = session_.document().scene.findObject(sel);
    if (obj == nullptr) {
        session_.setSelectedObject({});
        auto st = core::Status::fail(core::ErrorCode::NotFound, "Selected object not found");
        recordStatus(st, {});
        return st;
    }

    const std::string deleted_name = obj->name;
    auto st = session_.commandStack().execute(
        session_.document().scene,
        std::make_unique<scene::DeleteObjectCommand>(sel, mode));
    if (st) {
        session_.setSelectedObject({});
    }
    recordStatus(st, "Deleted '" + deleted_name + "'");
    return st;
}

core::Status EditorController::renameObject(scene::ObjectId id, std::string new_name) {
    auto st = session_.commandStack().execute(
        session_.document().scene,
        std::make_unique<scene::RenameObjectCommand>(id, std::move(new_name)));
    recordStatus(st, "Renamed object");
    return st;
}

core::Status EditorController::setObjectVisibility(scene::ObjectId id, bool visible) {
    if (const auto* obj = session_.document().scene.findObject(id);
        obj != nullptr && (obj->locked || (obj->locked_channels_mask & scene::ChannelBit::kVisibility) != 0u)) {
        auto st = core::Status::fail(core::ErrorCode::ValidationError, "Visibility channel is locked on '" + obj->name + "'");
        recordStatus(st, {});
        return st;
    }
    auto st = session_.commandStack().execute(
        session_.document().scene,
        std::make_unique<scene::SetVisibilityCommand>(id, visible));
    recordStatus(st, visible ? "Shown object" : "Hidden object");
    return st;
}

core::Status EditorController::reparentObject(scene::ObjectId child_id, scene::ObjectId new_parent_id) {
    auto st = session_.commandStack().execute(
        session_.document().scene,
        std::make_unique<scene::ReparentObjectCommand>(child_id, new_parent_id));
    recordStatus(st, "Updated object parent hierarchy");
    return st;
}

core::Status EditorController::setObjectTransformEulerDegrees(
    scene::ObjectId id,
    const math::Vec3& position,
    const math::Vec3& euler_degrees,
    const math::Vec3& scale,
    bool allow_coalesce) {
    const auto* existing = session_.document().scene.findObject(id);
    if (existing == nullptr) {
        auto st = core::Status::fail(core::ErrorCode::NotFound, "Scene object not found");
        recordStatus(st, {});
        return st;
    }
    if (existing->locked) {
        auto st = core::Status::fail(
            core::ErrorCode::ValidationError,
            "Object '" + existing->name + "' is locked; unlock before transforming");
        recordStatus(st, {});
        return st;
    }

    math::Vec3 pos = position;
    math::Vec3 eul = euler_degrees;
    math::Vec3 scl = scale;
    const std::uint16_t mask = existing->locked_channels_mask;
    if (mask != 0u) {
        const math::Vec3 cur_rad = existing->local_transform.rotation.toEulerXYZ();
        const math::Vec3 cur_deg{
            math::radToDeg(cur_rad.x),
            math::radToDeg(cur_rad.y),
            math::radToDeg(cur_rad.z),
        };
        if (mask & scene::ChannelBit::kTranslateX) pos.x = existing->local_transform.position.x;
        if (mask & scene::ChannelBit::kTranslateY) pos.y = existing->local_transform.position.y;
        if (mask & scene::ChannelBit::kTranslateZ) pos.z = existing->local_transform.position.z;
        if (mask & scene::ChannelBit::kRotateX)    eul.x = cur_deg.x;
        if (mask & scene::ChannelBit::kRotateY)    eul.y = cur_deg.y;
        if (mask & scene::ChannelBit::kRotateZ)    eul.z = cur_deg.z;
        if (mask & scene::ChannelBit::kScaleX)     scl.x = existing->local_transform.scale.x;
        if (mask & scene::ChannelBit::kScaleY)     scl.y = existing->local_transform.scale.y;
        if (mask & scene::ChannelBit::kScaleZ)     scl.z = existing->local_transform.scale.z;
    }

    math::Transform tf;
    tf.position = pos;
    tf.rotation = math::Quat::fromEulerXYZ({
        math::degToRad(eul.x),
        math::degToRad(eul.y),
        math::degToRad(eul.z),
    });
    tf.scale = scl;

    auto st = session_.commandStack().execute(
        session_.document().scene,
        std::make_unique<scene::SetTransformCommand>(id, tf, allow_coalesce));
    if (st) {
        if (session_.isCameraViewActive() && id == session_.document().scene.activeCamera()) {
            session_.cameraController().syncFromCameraTransform(tf, session_.cameraController().target());
        }
        if (!allow_coalesce &&
            session_.document().scene.timeline().auto_key &&
            session_.document().scene.findAnimationTrack(id) != nullptr) {
            std::vector<scene::ObjectAnimationTrack> tracks = session_.document().scene.animationTracks();
            const float cur_f = session_.document().scene.timeline().current_frame;
            const scene::TransformKeyframe kf{
                .frame = cur_f,
                .position = pos,
                .rotation_deg = eul,
                .scale = scl,
                .visible = session_.document().scene.findObject(id)->visible,
            };
            for (auto& tr : tracks) {
                if (tr.object_id == id) {
                    bool updated = false;
                    for (auto& k : tr.keyframes) {
                        if (std::fabs(k.frame - cur_f) <= 1.0e-3f) {
                            k = kf;
                            updated = true;
                            break;
                        }
                    }
                    if (!updated) tr.keyframes.push_back(kf);
                    break;
                }
            }
            (void)session_.document().scene.setAnimationTracks(std::move(tracks));
        }
    }
    recordStatus(st, "Updated transform");
    return st;
}

core::Status EditorController::resetObjectTransform(scene::ObjectId id) {
    if (const auto* existing = session_.document().scene.findObject(id);
        existing != nullptr && existing->locked) {
        auto st = core::Status::fail(
            core::ErrorCode::ValidationError,
            "Object '" + existing->name + "' is locked");
        recordStatus(st, {});
        return st;
    }
    auto st = session_.commandStack().execute(
        session_.document().scene,
        std::make_unique<scene::SetTransformCommand>(id, math::Transform::identity(), false));
    recordStatus(st, "Reset transform to identity");
    return st;
}

core::Result<scene::ObjectId> EditorController::groupSelectedObjects(std::string group_name) {
    auto& sc = session_.document().scene;
    std::vector<scene::ObjectId> targets = session_.selectedObjects();
    if (targets.empty() && session_.selectedObject().isValid()) {
        targets.push_back(session_.selectedObject());
    }
    if (targets.empty()) {
        auto st = core::Status::fail(core::ErrorCode::InvalidState, "Select one or more objects to group");
        recordStatus(st, {});
        return st.error();
    }

    scene::ObjectId common_parent = scene::ObjectId::invalid();
    if (const auto* first = sc.findObject(targets.front())) {
        common_parent = first->parent_id;
        for (const auto tid : targets) {
            const auto* n = sc.findObject(tid);
            if (n == nullptr || n->parent_id != common_parent) {
                common_parent = scene::ObjectId::invalid();
                break;
            }
        }
    }

    const std::string resolved_name =
        group_name.empty() ? generateUniqueObjectName("group1") : generateUniqueObjectName(group_name);

    // Pre-create + undo pattern inside CompoundTransactionCommand: createObject assigns stable ID on execute!
    auto create_grp = std::make_unique<scene::CreateObjectCommand>(
        resolved_name,
        common_parent,
        math::Transform::identity());
    const auto* raw_grp = create_grp.get();

    // Execute group creation first via a compound transaction built step-by-step:
    auto create_st = create_grp->execute(sc);
    if (!create_st) {
        recordStatus(create_st, {});
        return create_st.error();
    }
    const scene::ObjectId grp_id = raw_grp->createdObjectId();
    // Undo temporary step so CommandStack::execute runs the full compound cleanly from initial state!
    (void)create_grp->undo(sc);

    std::vector<std::unique_ptr<scene::ISceneCommand>> steps;
    steps.push_back(std::move(create_grp));
    for (const auto tid : targets) {
        if (tid != grp_id) {
            steps.push_back(std::make_unique<scene::ReparentObjectCommand>(tid, grp_id));
        }
    }

    auto st = session_.commandStack().execute(
        sc,
        std::make_unique<scene::CompoundTransactionCommand>("Group Objects", std::move(steps)));
    if (!st) {
        recordStatus(st, {});
        return st.error();
    }
    session_.setSelectedObject(grp_id);
    recordStatus(core::Status::ok(), "Grouped under '" + resolved_name + "'");
    return core::Result<scene::ObjectId>::ok(grp_id);
}

core::Status EditorController::parentSelectedObjects() {
    const auto& sel = session_.selectedObjects();
    const scene::ObjectId lead = session_.selectedObject();
    if (sel.size() < 2u || !lead.isValid()) {
        auto st = core::Status::fail(
            core::ErrorCode::InvalidState,
            "Select child object(s) then Shift/Ctrl-select parent object last to parent");
        recordStatus(st, {});
        return st;
    }
    std::vector<std::unique_ptr<scene::ISceneCommand>> steps;
    for (const auto id : sel) {
        if (id != lead) {
            steps.push_back(std::make_unique<scene::ReparentObjectCommand>(id, lead));
        }
    }
    auto st = session_.commandStack().execute(
        session_.document().scene,
        std::make_unique<scene::CompoundTransactionCommand>("Parent Objects", std::move(steps)));
    recordStatus(st, "Parented selected objects");
    return st;
}

core::Status EditorController::unparentSelectedObjects() {
    std::vector<scene::ObjectId> sel = session_.selectedObjects();
    if (sel.empty() && session_.selectedObject().isValid()) {
        sel.push_back(session_.selectedObject());
    }
    if (sel.empty()) {
        auto st = core::Status::fail(core::ErrorCode::InvalidState, "Select object(s) to unparent");
        recordStatus(st, {});
        return st;
    }
    std::vector<std::unique_ptr<scene::ISceneCommand>> steps;
    for (const auto id : sel) {
        steps.push_back(std::make_unique<scene::ReparentObjectCommand>(id, scene::ObjectId::invalid()));
    }
    auto st = session_.commandStack().execute(
        session_.document().scene,
        std::make_unique<scene::CompoundTransactionCommand>("Unparent Objects", std::move(steps)));
    recordStatus(st, "Unparented selected objects to world root");
    return st;
}

core::Status EditorController::freezeObjectTransforms(scene::ObjectId id) {
    auto& sc = session_.document().scene;
    const auto* obj = sc.findObject(id);
    if (obj == nullptr) {
        auto st = core::Status::fail(core::ErrorCode::NotFound, "Select an object to freeze transformations");
        recordStatus(st, {});
        return st;
    }
    if (obj->locked) {
        auto st = core::Status::fail(core::ErrorCode::ValidationError, "Object is locked");
        recordStatus(st, {});
        return st;
    }

    const math::Mat4 local_mat = obj->local_transform.toMatrix();
    std::vector<std::unique_ptr<scene::ISceneCommand>> steps;

    if (obj->mesh_id.has_value()) {
        const scene::MeshId mid = obj->mesh_id.value();
        (void)sc.ensureEditableMesh(mid);
        const auto* ma = sc.findMesh(mid);
        if (ma != nullptr && ma->editable_mesh.has_value()) {
            std::size_t users = 0;
            for (const auto& other : sc.objects()) {
                if (other.mesh_id == mid) ++users;
            }
            if (users == 1u) {
                scene::EditableMesh baked = ma->editable_mesh.value();
                std::vector<std::uint32_t> all_verts(baked.vertexCount());
                for (std::size_t vi = 0; vi < all_verts.size(); ++vi) {
                    all_verts[vi] = static_cast<std::uint32_t>(vi);
                }
                if (!all_verts.empty()) {
                    auto tr_st = baked.transformVertices(all_verts, local_mat);
                    if (!tr_st) {
                        recordStatus(tr_st, {});
                        return tr_st;
                    }
                    steps.push_back(std::make_unique<scene::ModifyMeshCommand>(
                        mid, std::move(baked), "Bake Frozen Mesh", false));
                }
            }
        }
    }

    steps.push_back(std::make_unique<scene::SetTransformCommand>(id, math::Transform::identity(), false));
    auto st = session_.commandStack().execute(
        sc,
        std::make_unique<scene::CompoundTransactionCommand>("Freeze Transformations", std::move(steps)));
    recordStatus(st, "Froze transformations on '" + obj->name + "'");
    return st;
}

core::Status EditorController::centerObjectPivot(scene::ObjectId id) {
    const auto* obj = session_.document().scene.findObject(id);
    if (obj == nullptr) {
        auto st = core::Status::fail(core::ErrorCode::NotFound, "Select an object to center pivot");
        recordStatus(st, {});
        return st;
    }
    math::Vec3 target_piv{0.0f, 0.0f, 0.0f};
    if (obj->mesh_id.has_value()) {
        if (const auto* ma = session_.document().scene.findMesh(obj->mesh_id.value());
            ma != nullptr && ma->local_bounds.isValid()) {
            target_piv = ma->local_bounds.center();
        }
    }
    return setObjectPivotOffset(id, target_piv, false);
}

core::Status EditorController::setObjectLocked(scene::ObjectId id, bool locked) {
    const auto* obj = session_.document().scene.findObject(id);
    if (obj == nullptr) {
        auto st = core::Status::fail(core::ErrorCode::NotFound, "Object not found");
        recordStatus(st, {});
        return st;
    }
    scene::SetObjectMetadataCommand::MetadataState next{
        .locked = locked,
        .locked_channels_mask = obj->locked_channels_mask,
        .pivot_offset = obj->pivot_offset,
        .custom_attributes = obj->custom_attributes,
    };
    auto st = session_.commandStack().execute(
        session_.document().scene,
        std::make_unique<scene::SetObjectMetadataCommand>(
            id, std::move(next), locked ? "Lock Object" : "Unlock Object"));
    recordStatus(st, (locked ? "Locked '" : "Unlocked '") + obj->name + "'");
    return st;
}

core::Status EditorController::setObjectChannelLocks(scene::ObjectId id, std::uint16_t mask) {
    const auto* obj = session_.document().scene.findObject(id);
    if (obj == nullptr) {
        auto st = core::Status::fail(core::ErrorCode::NotFound, "Object not found");
        recordStatus(st, {});
        return st;
    }
    scene::SetObjectMetadataCommand::MetadataState next{
        .locked = obj->locked,
        .locked_channels_mask = mask,
        .pivot_offset = obj->pivot_offset,
        .custom_attributes = obj->custom_attributes,
    };
    auto st = session_.commandStack().execute(
        session_.document().scene,
        std::make_unique<scene::SetObjectMetadataCommand>(id, std::move(next), "Set Channel Locks"));
    recordStatus(st, "Updated channel locks on '" + obj->name + "'");
    return st;
}

core::Status EditorController::setObjectPivotOffset(
    scene::ObjectId id,
    const math::Vec3& pivot_offset,
    bool allow_coalesce) {
    const auto* obj = session_.document().scene.findObject(id);
    if (obj == nullptr) {
        auto st = core::Status::fail(core::ErrorCode::NotFound, "Object not found");
        recordStatus(st, {});
        return st;
    }
    scene::SetObjectMetadataCommand::MetadataState next{
        .locked = obj->locked,
        .locked_channels_mask = obj->locked_channels_mask,
        .pivot_offset = pivot_offset,
        .custom_attributes = obj->custom_attributes,
    };
    auto st = session_.commandStack().execute(
        session_.document().scene,
        std::make_unique<scene::SetObjectMetadataCommand>(
            id, std::move(next), "Set Pivot Offset", allow_coalesce, 1u));
    recordStatus(st, "Updated pivot on '" + obj->name + "'");
    return st;
}

core::Status EditorController::addCustomAttribute(
    scene::ObjectId id,
    scene::CustomAttribute attr) {
    const auto* obj = session_.document().scene.findObject(id);
    if (obj == nullptr) {
        auto st = core::Status::fail(core::ErrorCode::NotFound, "Select an object to add custom attribute");
        recordStatus(st, {});
        return st;
    }
    if (attr.name.empty()) {
        auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Custom attribute name must not be empty");
        recordStatus(st, {});
        return st;
    }
    for (const auto& ex : obj->custom_attributes) {
        if (ex.name == attr.name) {
            auto st = core::Status::fail(core::ErrorCode::AlreadyExists, "Attribute '" + attr.name + "' already exists");
            recordStatus(st, {});
            return st;
        }
    }
    if (attr.has_limits) {
        attr.value = std::clamp(attr.value, attr.min_value, attr.max_value);
    }
    scene::SetObjectMetadataCommand::MetadataState next{
        .locked = obj->locked,
        .locked_channels_mask = obj->locked_channels_mask,
        .pivot_offset = obj->pivot_offset,
        .custom_attributes = obj->custom_attributes,
    };
    next.custom_attributes.push_back(attr);
    auto st = session_.commandStack().execute(
        session_.document().scene,
        std::make_unique<scene::SetObjectMetadataCommand>(id, std::move(next), "Add Custom Attribute"));
    recordStatus(st, "Added attribute '" + attr.name + "'");
    return st;
}

core::Status EditorController::setCustomAttributeValue(
    scene::ObjectId id,
    std::string_view attr_name,
    double value,
    bool allow_coalesce) {
    const auto* obj = session_.document().scene.findObject(id);
    if (obj == nullptr) {
        auto st = core::Status::fail(core::ErrorCode::NotFound, "Object not found");
        recordStatus(st, {});
        return st;
    }
    scene::SetObjectMetadataCommand::MetadataState next{
        .locked = obj->locked,
        .locked_channels_mask = obj->locked_channels_mask,
        .pivot_offset = obj->pivot_offset,
        .custom_attributes = obj->custom_attributes,
    };
    bool found = false;
    for (auto& ca : next.custom_attributes) {
        if (ca.name == attr_name) {
            if (ca.locked) {
                auto st = core::Status::fail(core::ErrorCode::ValidationError, "Attribute '" + ca.name + "' is locked");
                recordStatus(st, {});
                return st;
            }
            ca.value = ca.has_limits ? std::clamp(value, ca.min_value, ca.max_value) : value;
            found = true;
            break;
        }
    }
    if (!found) {
        auto st = core::Status::fail(core::ErrorCode::NotFound, "Custom attribute not found: " + std::string(attr_name));
        recordStatus(st, {});
        return st;
    }
    auto st = session_.commandStack().execute(
        session_.document().scene,
        std::make_unique<scene::SetObjectMetadataCommand>(
            id, std::move(next), "Set Custom Attribute", allow_coalesce, 2u));
    recordStatus(st, "Updated '" + std::string(attr_name) + "'");
    return st;
}

core::Status EditorController::deleteCustomAttribute(
    scene::ObjectId id,
    std::string_view attr_name) {
    const auto* obj = session_.document().scene.findObject(id);
    if (obj == nullptr) {
        auto st = core::Status::fail(core::ErrorCode::NotFound, "Object not found");
        recordStatus(st, {});
        return st;
    }
    scene::SetObjectMetadataCommand::MetadataState next{
        .locked = obj->locked,
        .locked_channels_mask = obj->locked_channels_mask,
        .pivot_offset = obj->pivot_offset,
        .custom_attributes = obj->custom_attributes,
    };
    const auto erased = std::erase_if(
        next.custom_attributes,
        [attr_name](const scene::CustomAttribute& ca) { return ca.name == attr_name; });
    if (erased == 0) {
        auto st = core::Status::fail(core::ErrorCode::NotFound, "Custom attribute not found");
        recordStatus(st, {});
        return st;
    }
    auto st = session_.commandStack().execute(
        session_.document().scene,
        std::make_unique<scene::SetObjectMetadataCommand>(id, std::move(next), "Delete Custom Attribute"));
    recordStatus(st, "Deleted attribute '" + std::string(attr_name) + "'");
    return st;
}

core::Result<std::uint32_t> EditorController::createDisplayLayer(
    std::string layer_name,
    bool assign_selected) {
    std::vector<scene::DisplayLayer> layers = session_.document().scene.displayLayers();
    std::uint32_t next_id = 1u;
    for (const auto& l : layers) {
        next_id = std::max(next_id, l.id + 1u);
    }
    if (layer_name.empty()) {
        layer_name = "layer" + std::to_string(next_id);
    }
    scene::DisplayLayer created{
        .id = next_id,
        .name = std::move(layer_name),
        .visible = true,
        .playback_visible = true,
        .display_type = scene::DisplayLayerType::Normal,
        .color = {0.34f, 0.56f, 0.86f},
    };
    if (assign_selected) {
        for (const auto oid : session_.selectedObjects()) {
            for (auto& existing : layers) {
                std::erase(existing.members, oid);
            }
            created.members.push_back(oid);
        }
    }
    const std::string created_name = created.name;
    layers.push_back(std::move(created));
    auto st = session_.commandStack().execute(
        session_.document().scene,
        std::make_unique<scene::SetDisplayLayersCommand>(std::move(layers), "Create Display Layer"));
    if (!st) {
        recordStatus(st, {});
        return st.error();
    }
    recordStatus(core::Status::ok(), "Created display layer '" + created_name + "'");
    return core::Result<std::uint32_t>::ok(next_id);
}

core::Status EditorController::renameDisplayLayer(std::uint32_t layer_id, std::string new_name) {
    if (new_name.empty()) {
        auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Layer name must not be empty");
        recordStatus(st, {});
        return st;
    }
    std::vector<scene::DisplayLayer> layers = session_.document().scene.displayLayers();
    bool found = false;
    for (auto& l : layers) {
        if (l.id == layer_id) { l.name = new_name; found = true; break; }
    }
    if (!found) {
        auto st = core::Status::fail(core::ErrorCode::NotFound, "Display layer not found");
        recordStatus(st, {});
        return st;
    }
    auto st = session_.commandStack().execute(
        session_.document().scene,
        std::make_unique<scene::SetDisplayLayersCommand>(std::move(layers), "Rename Display Layer"));
    recordStatus(st, "Renamed display layer to '" + new_name + "'");
    return st;
}

core::Status EditorController::deleteDisplayLayer(std::uint32_t layer_id) {
    std::vector<scene::DisplayLayer> layers = session_.document().scene.displayLayers();
    const auto erased = std::erase_if(layers, [layer_id](const scene::DisplayLayer& l) { return l.id == layer_id; });
    if (erased == 0) {
        auto st = core::Status::fail(core::ErrorCode::NotFound, "Display layer not found");
        recordStatus(st, {});
        return st;
    }
    auto st = session_.commandStack().execute(
        session_.document().scene,
        std::make_unique<scene::SetDisplayLayersCommand>(std::move(layers), "Delete Display Layer"));
    recordStatus(st, "Deleted display layer (objects preserved in scene)");
    return st;
}

core::Status EditorController::updateDisplayLayer(
    std::uint32_t layer_id,
    bool visible,
    bool playback_visible,
    scene::DisplayLayerType display_type,
    const math::Vec3& color) {
    std::vector<scene::DisplayLayer> layers = session_.document().scene.displayLayers();
    bool found = false;
    for (auto& l : layers) {
        if (l.id == layer_id) {
            l.visible = visible;
            l.playback_visible = playback_visible;
            l.display_type = display_type;
            l.color = color;
            found = true;
            break;
        }
    }
    if (!found) {
        auto st = core::Status::fail(core::ErrorCode::NotFound, "Display layer not found");
        recordStatus(st, {});
        return st;
    }
    auto st = session_.commandStack().execute(
        session_.document().scene,
        std::make_unique<scene::SetDisplayLayersCommand>(std::move(layers), "Update Display Layer"));
    recordStatus(st, "Updated display layer state");
    return st;
}

core::Status EditorController::assignSelectedToDisplayLayer(std::uint32_t layer_id) {
    const auto& sel = session_.selectedObjects();
    if (sel.empty()) {
        auto st = core::Status::fail(core::ErrorCode::InvalidState, "Select objects first to assign to layer");
        recordStatus(st, {});
        return st;
    }
    std::vector<scene::DisplayLayer> layers = session_.document().scene.displayLayers();
    scene::DisplayLayer* target = nullptr;
    for (auto& l : layers) {
        if (l.id == layer_id) target = &l;
    }
    if (target == nullptr) {
        auto st = core::Status::fail(core::ErrorCode::NotFound, "Display layer not found");
        recordStatus(st, {});
        return st;
    }
    for (const auto oid : sel) {
        for (auto& l : layers) {
            std::erase(l.members, oid);
        }
        target->members.push_back(oid);
    }
    auto st = session_.commandStack().execute(
        session_.document().scene,
        std::make_unique<scene::SetDisplayLayersCommand>(std::move(layers), "Assign Objects to Layer"));
    recordStatus(st, "Assigned selected objects to layer");
    return st;
}

core::Status EditorController::removeSelectedFromDisplayLayer(std::uint32_t layer_id) {
    std::vector<scene::DisplayLayer> layers = session_.document().scene.displayLayers();
    for (auto& l : layers) {
        if (l.id == layer_id) {
            for (const auto oid : session_.selectedObjects()) {
                std::erase(l.members, oid);
            }
        }
    }
    auto st = session_.commandStack().execute(
        session_.document().scene,
        std::make_unique<scene::SetDisplayLayersCommand>(std::move(layers), "Remove Objects from Layer"));
    recordStatus(st, "Removed selected objects from layer");
    return st;
}

core::Status EditorController::selectDisplayLayerObjects(std::uint32_t layer_id) {
    for (const auto& l : session_.document().scene.displayLayers()) {
        if (l.id == layer_id) {
            return selectObjects(l.members);
        }
    }
    auto st = core::Status::fail(core::ErrorCode::NotFound, "Display layer not found");
    recordStatus(st, {});
    return st;
}

void EditorController::setTimelineSettings(const scene::TimelineSettings& settings) noexcept {
    session_.document().scene.setTimeline(settings);
    session_.document().scene.evaluateAnimationAtFrame(session_.document().scene.timeline().current_frame);
    ++state_revision_;
}

void EditorController::setCurrentFrame(float frame) noexcept {
    session_.setTimelinePlaying(false);
    session_.document().scene.evaluateAnimationAtFrame(frame);
    ++state_revision_;
}

void EditorController::setTimelinePlaying(bool playing) noexcept {
    if (playing) {
        session_.setRigAnimationEnabled(false);
    }
    session_.setTimelinePlaying(playing);
    ++state_revision_;
    last_status_is_error_ = false;
    last_status_message_ = playing ? "Timeline playback started" : "Timeline playback paused";
}

core::Status EditorController::setKeyframeOnSelected(std::optional<float> frame_opt) {
    std::vector<scene::ObjectId> targets = session_.selectedObjects();
    if (targets.empty() && session_.selectedObject().isValid()) {
        targets.push_back(session_.selectedObject());
    }
    if (targets.empty()) {
        auto st = core::Status::fail(core::ErrorCode::InvalidState, "Select object(s) before setting keyframe");
        recordStatus(st, {});
        return st;
    }
    const float frame = frame_opt.value_or(session_.document().scene.timeline().current_frame);
    std::vector<scene::ObjectAnimationTrack> tracks = session_.document().scene.animationTracks();

    for (const auto oid : targets) {
        const auto* obj = session_.document().scene.findObject(oid);
        if (obj == nullptr) continue;
        const math::Vec3 rad = obj->local_transform.rotation.toEulerXYZ();
        const scene::TransformKeyframe kf{
            .frame = frame,
            .position = obj->local_transform.position,
            .rotation_deg = {math::radToDeg(rad.x), math::radToDeg(rad.y), math::radToDeg(rad.z)},
            .scale = obj->local_transform.scale,
            .visible = obj->visible,
        };
        scene::ObjectAnimationTrack* tr_ptr = nullptr;
        for (auto& tr : tracks) {
            if (tr.object_id == oid) { tr_ptr = &tr; break; }
        }
        if (tr_ptr == nullptr) {
            tracks.push_back(scene::ObjectAnimationTrack{.object_id = oid, .keyframes = {kf}});
        } else {
            bool replaced = false;
            for (auto& k : tr_ptr->keyframes) {
                if (std::fabs(k.frame - frame) <= 1.0e-3f) {
                    k = kf;
                    replaced = true;
                    break;
                }
            }
            if (!replaced) {
                tr_ptr->keyframes.push_back(kf);
            }
        }
    }

    auto st = session_.commandStack().execute(
        session_.document().scene,
        std::make_unique<scene::SetAnimationTracksCommand>(std::move(tracks), "Set Keyframe"));
    recordStatus(st, "Set keyframe at frame " + std::to_string(static_cast<int>(std::round(frame))));
    return st;
}

core::Status EditorController::deleteKeyframeOnSelected(std::optional<float> frame_opt) {
    std::vector<scene::ObjectId> targets = session_.selectedObjects();
    if (targets.empty() && session_.selectedObject().isValid()) {
        targets.push_back(session_.selectedObject());
    }
    if (targets.empty()) {
        auto st = core::Status::fail(core::ErrorCode::InvalidState, "Select object(s) to delete keyframe");
        recordStatus(st, {});
        return st;
    }
    const float frame = frame_opt.value_or(session_.document().scene.timeline().current_frame);
    std::vector<scene::ObjectAnimationTrack> tracks = session_.document().scene.animationTracks();
    std::size_t removed = 0;
    for (auto& tr : tracks) {
        if (std::find(targets.begin(), targets.end(), tr.object_id) != targets.end()) {
            removed += std::erase_if(tr.keyframes, [frame](const scene::TransformKeyframe& k) {
                return std::fabs(k.frame - frame) <= 1.0e-3f;
            });
        }
    }
    if (removed == 0) {
        auto st = core::Status::fail(core::ErrorCode::NotFound, "No keyframe at current frame on selected object");
        recordStatus(st, {});
        return st;
    }
    auto st = session_.commandStack().execute(
        session_.document().scene,
        std::make_unique<scene::SetAnimationTracksCommand>(std::move(tracks), "Delete Keyframe"));
    recordStatus(st, "Deleted keyframe at frame " + std::to_string(static_cast<int>(std::round(frame))));
    return st;
}

core::Status EditorController::executeCommandLine(std::string_view command_text) {
    // Strip optional trailing semicolons (MEL habit) and whitespace; NEVER execute arbitrary code or shell!
    std::string cleaned;
    cleaned.reserve(command_text.size());
    for (char ch : command_text) {
        if (ch == ';') cleaned.push_back(' ');
        else cleaned.push_back(ch);
    }
    std::istringstream iss(cleaned);
    std::vector<std::string> tokens;
    std::string tok;
    while (iss >> tok) {
        tokens.push_back(std::move(tok));
    }
    if (tokens.empty()) {
        auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Command Line: enter a valid FE3D command (type 'help')");
        recordStatus(st, {});
        return st;
    }

    auto lowerStr = [](std::string s) {
        for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return s;
    };
    auto parseNum = [](const std::string& s, float& out) -> bool {
        char* endp = nullptr;
        const float v = std::strtof(s.c_str(), &endp);
        if (endp == s.c_str() || *endp != '\0' || !std::isfinite(v)) return false;
        out = v;
        return true;
    };

    const std::string verb = lowerStr(tokens[0]);

    if (verb == "help") {
        recordStatus(
            core::Status::ok(),
            "Commands: polyCube, polySphere, polyCylinder, polyCone, polyPlane, polyTorus, camera, "
            "directionalLight, pointLight, spotLight, areaLight, select <name|-cl|-all|-hi>, move <x y z>, "
            "rotate <x y z>, scale <x y z>, setAttr <ch> <val>, addAttr <name> [val], rename <name>, "
            "duplicate, delete, group, parent, unparent, freezeTransforms, resetTransforms, centerPivot, "
            "setKeyframe, currentTime <f>, play, stop, undo, redo, viewFit");
        return core::Status::ok();
    }

    if (verb == "polycube")     return addPresetObject(ScenePreset::Cube).hasValue() ? core::Status::ok() : core::Status::fail(core::ErrorCode::InvalidState, last_status_message_);
    if (verb == "polysphere")   return addPresetObject(ScenePreset::Sphere).hasValue() ? core::Status::ok() : core::Status::fail(core::ErrorCode::InvalidState, last_status_message_);
    if (verb == "polycylinder") return addPresetObject(ScenePreset::Cylinder).hasValue() ? core::Status::ok() : core::Status::fail(core::ErrorCode::InvalidState, last_status_message_);
    if (verb == "polycone")     return addPresetObject(ScenePreset::Cone).hasValue() ? core::Status::ok() : core::Status::fail(core::ErrorCode::InvalidState, last_status_message_);
    if (verb == "polyplane")    return addPresetObject(ScenePreset::Plane).hasValue() ? core::Status::ok() : core::Status::fail(core::ErrorCode::InvalidState, last_status_message_);
    if (verb == "polytorus")    return addPresetObject(ScenePreset::Torus).hasValue() ? core::Status::ok() : core::Status::fail(core::ErrorCode::InvalidState, last_status_message_);
    if (verb == "locator" || verb == "spacelocator") return addPresetObject(ScenePreset::EmptyPivot).hasValue() ? core::Status::ok() : core::Status::fail(core::ErrorCode::InvalidState, last_status_message_);
    if (verb == "camera")       return addPresetObject(ScenePreset::Camera).hasValue() ? core::Status::ok() : core::Status::fail(core::ErrorCode::InvalidState, last_status_message_);
    if (verb == "directionallight") return addPresetObject(ScenePreset::DirectionalLight).hasValue() ? core::Status::ok() : core::Status::fail(core::ErrorCode::InvalidState, last_status_message_);
    if (verb == "pointlight")   return addPresetObject(ScenePreset::PointLight).hasValue() ? core::Status::ok() : core::Status::fail(core::ErrorCode::InvalidState, last_status_message_);
    if (verb == "spotlight")    return addPresetObject(ScenePreset::SpotLight).hasValue() ? core::Status::ok() : core::Status::fail(core::ErrorCode::InvalidState, last_status_message_);
    if (verb == "arealight")    return addPresetObject(ScenePreset::AreaLight).hasValue() ? core::Status::ok() : core::Status::fail(core::ErrorCode::InvalidState, last_status_message_);

    if (verb == "select") {
        if (tokens.size() < 2u) {
            auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Usage: select <objectName | -cl | -all | -hi>");
            recordStatus(st, {});
            return st;
        }
        const std::string arg1 = lowerStr(tokens[1]);
        if (arg1 == "-cl" || arg1 == "-clear" || arg1 == "none") return selectObject(scene::ObjectId::invalid());
        if (arg1 == "-all" || arg1 == "-ado") return selectAllObjects();
        if (arg1 == "-hi" || arg1 == "-hierarchy") return selectHierarchy();

        std::vector<scene::ObjectId> found_ids;
        for (std::size_t i = 1; i < tokens.size(); ++i) {
            if (const auto* by_name = session_.document().scene.findObjectByName(tokens[i])) {
                found_ids.push_back(by_name->id);
            } else {
                auto st = core::Status::fail(core::ErrorCode::NotFound, "Object not found: '" + tokens[i] + "'");
                recordStatus(st, {});
                return st;
            }
        }
        return selectObjects(found_ids);
    }

    if (verb == "move" || verb == "rotate" || verb == "scale") {
        const auto* obj = session_.document().scene.findObject(session_.selectedObject());
        if (obj == nullptr) {
            auto st = core::Status::fail(core::ErrorCode::InvalidState, "Select an object before running '" + verb + "'");
            recordStatus(st, {});
            return st;
        }
        bool relative = false;
        std::size_t val_idx = 1;
        if (tokens.size() >= 5u && (tokens[1] == "-r" || tokens[1] == "-relative")) {
            relative = true;
            val_idx = 2;
        }
        if (tokens.size() < val_idx + 3u) {
            auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Usage: " + verb + " [-r] <x> <y> <z>");
            recordStatus(st, {});
            return st;
        }
        float vx = 0.0f, vy = 0.0f, vz = 0.0f;
        if (!parseNum(tokens[val_idx], vx) || !parseNum(tokens[val_idx + 1], vy) || !parseNum(tokens[val_idx + 2], vz)) {
            auto st = core::Status::fail(core::ErrorCode::ValidationError, "Numeric arguments required for '" + verb + "'");
            recordStatus(st, {});
            return st;
        }
        math::Vec3 pos = obj->local_transform.position;
        const math::Vec3 cur_rad = obj->local_transform.rotation.toEulerXYZ();
        math::Vec3 rot{math::radToDeg(cur_rad.x), math::radToDeg(cur_rad.y), math::radToDeg(cur_rad.z)};
        math::Vec3 scl = obj->local_transform.scale;
        if (verb == "move") {
            pos = relative ? (pos + math::Vec3{vx, vy, vz}) : math::Vec3{vx, vy, vz};
        } else if (verb == "rotate") {
            rot = relative ? (rot + math::Vec3{vx, vy, vz}) : math::Vec3{vx, vy, vz};
        } else {
            scl = relative ? math::Vec3{scl.x * vx, scl.y * vy, scl.z * vz} : math::Vec3{vx, vy, vz};
        }
        return setObjectTransformEulerDegrees(obj->id, pos, rot, scl, false);
    }

    if (verb == "setattr") {
        const auto* obj = session_.document().scene.findObject(session_.selectedObject());
        if (obj == nullptr || tokens.size() < 3u) {
            auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Usage: setAttr <channelName> <value> (with object selected)");
            recordStatus(st, {});
            return st;
        }
        float v = 0.0f;
        if (!parseNum(tokens[2], v)) {
            auto st = core::Status::fail(core::ErrorCode::ValidationError, "Attribute value must be numeric");
            recordStatus(st, {});
            return st;
        }
        std::string ch = tokens[1];
        if (const auto dot = ch.rfind('.'); dot != std::string::npos) ch = ch.substr(dot + 1);
        const std::string chl = lowerStr(ch);
        math::Vec3 pos = obj->local_transform.position;
        const math::Vec3 cur_rad = obj->local_transform.rotation.toEulerXYZ();
        math::Vec3 rot{math::radToDeg(cur_rad.x), math::radToDeg(cur_rad.y), math::radToDeg(cur_rad.z)};
        math::Vec3 scl = obj->local_transform.scale;

        if (chl == "tx" || chl == "translatex") pos.x = v;
        else if (chl == "ty" || chl == "translatey") pos.y = v;
        else if (chl == "tz" || chl == "translatez") pos.z = v;
        else if (chl == "rx" || chl == "rotatex") rot.x = v;
        else if (chl == "ry" || chl == "rotatey") rot.y = v;
        else if (chl == "rz" || chl == "rotatez") rot.z = v;
        else if (chl == "sx" || chl == "scalex") scl.x = v;
        else if (chl == "sy" || chl == "scaley") scl.y = v;
        else if (chl == "sz" || chl == "scalez") scl.z = v;
        else if (chl == "v" || chl == "visibility") return setObjectVisibility(obj->id, v != 0.0f);
        else return setCustomAttributeValue(obj->id, ch, static_cast<double>(v), false);
        return setObjectTransformEulerDegrees(obj->id, pos, rot, scl, false);
    }

    if (verb == "addattr") {
        if (!session_.selectedObject().isValid() || tokens.size() < 2u) {
            auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Usage: addAttr <attrName> [defaultValue]");
            recordStatus(st, {});
            return st;
        }
        float def_v = 0.0f;
        if (tokens.size() >= 3u) (void)parseNum(tokens[2], def_v);
        return addCustomAttribute(session_.selectedObject(), scene::CustomAttribute{.name = tokens[1], .value = static_cast<double>(def_v)});
    }

    if (verb == "rename") {
        if (!session_.selectedObject().isValid() || tokens.size() < 2u) {
            auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Usage: rename <newName>");
            recordStatus(st, {});
            return st;
        }
        return renameObject(session_.selectedObject(), tokens[1]);
    }

    if (verb == "duplicate") return duplicateSelectedObject().hasValue() ? core::Status::ok() : core::Status::fail(core::ErrorCode::InvalidState, last_status_message_);
    if (verb == "delete" || verb == "dodelete") return deleteSelectedObject();
    if (verb == "group") return groupSelectedObjects(tokens.size() >= 2u ? tokens[1] : std::string{}).hasValue() ? core::Status::ok() : core::Status::fail(core::ErrorCode::InvalidState, last_status_message_);
    if (verb == "parent") return parentSelectedObjects();
    if (verb == "unparent") return unparentSelectedObjects();
    if (verb == "makeidentity" || verb == "freezetransforms") return freezeObjectTransforms(session_.selectedObject());
    if (verb == "resettransforms") return resetObjectTransform(session_.selectedObject());
    if (verb == "centerpivot" || (verb == "xform" && tokens.size() >= 2u && lowerStr(tokens[1]) == "-cp")) {
        return centerObjectPivot(session_.selectedObject());
    }
    if (verb == "hide") return setObjectVisibility(session_.selectedObject(), false);
    if (verb == "showhidden") {
        if (session_.selectedObject().isValid()) return setObjectVisibility(session_.selectedObject(), true);
        for (const auto& obj : session_.document().scene.objects()) {
            if (!obj.visible) (void)setObjectVisibility(obj.id, true);
        }
        return core::Status::ok();
    }
    if (verb == "setkeyframe") return setKeyframeOnSelected();
    if (verb == "currenttime") {
        float f = 1.0f;
        if (tokens.size() < 2u || !parseNum(tokens[1], f)) {
            auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Usage: currentTime <frame>");
            recordStatus(st, {});
            return st;
        }
        setCurrentFrame(f);
        recordStatus(core::Status::ok(), "Current time set to frame " + std::to_string(static_cast<int>(std::round(f))));
        return core::Status::ok();
    }
    if (verb == "play") { setTimelinePlaying(true); return core::Status::ok(); }
    if (verb == "stop") { setTimelinePlaying(false); return core::Status::ok(); }
    if (verb == "undo") return undo();
    if (verb == "redo") return redo();
    if (verb == "viewfit") {
        if (tokens.size() >= 2u && lowerStr(tokens[1]) == "-all") return frameAllScene();
        return frameSelectionOrScene();
    }

    auto err = core::Status::fail(
        core::ErrorCode::ValidationError,
        "Unknown FE3D command '" + tokens[0] + "'. Type 'help' for supported structured commands.");
    recordStatus(err, {});
    return err;
}

core::Status EditorController::setCameraComponent(
    scene::ObjectId id,
    const scene::CameraComponent& camera) {
    auto st = session_.commandStack().execute(
        session_.document().scene,
        std::make_unique<scene::SetCameraComponentCommand>(id, camera));
    recordStatus(st, "Updated camera settings");
    return st;
}

core::Status EditorController::setActiveCamera(scene::ObjectId id) {
    auto st = session_.document().scene.setActiveCamera(id);
    if (st && session_.isCameraViewActive()) {
        if (const auto* cam_obj = session_.document().scene.findObject(id); cam_obj != nullptr) {
            session_.cameraController().syncFromCameraTransform(
                cam_obj->local_transform,
                session_.cameraController().target());
        }
    }
    recordStatus(st, "Switched active viewport camera");
    return st;
}

core::Status EditorController::setDirectionalLightComponent(
    scene::ObjectId id,
    const scene::DirectionalLightComponent& light,
    bool allow_coalesce) {
    auto st = session_.commandStack().execute(
        session_.document().scene,
        std::make_unique<scene::SetLightComponentCommand>(id, light, allow_coalesce, "Edit Directional Light"));
    recordStatus(st, "Updated directional light");
    return st;
}

core::Status EditorController::setPointLightComponent(
    scene::ObjectId id,
    const scene::PointLightComponent& light,
    bool allow_coalesce) {
    auto st = session_.commandStack().execute(
        session_.document().scene,
        std::make_unique<scene::SetLightComponentCommand>(id, light, allow_coalesce, "Edit Point Light"));
    recordStatus(st, "Updated point light");
    return st;
}

core::Status EditorController::setSpotLightComponent(
    scene::ObjectId id,
    const scene::SpotLightComponent& light,
    bool allow_coalesce) {
    auto st = session_.commandStack().execute(
        session_.document().scene,
        std::make_unique<scene::SetLightComponentCommand>(id, light, allow_coalesce, "Edit Spot Light"));
    recordStatus(st, "Updated spot light");
    return st;
}

core::Status EditorController::setAreaLightComponent(
    scene::ObjectId id,
    const scene::AreaLightComponent& light,
    bool allow_coalesce) {
    auto st = session_.commandStack().execute(
        session_.document().scene,
        std::make_unique<scene::SetLightComponentCommand>(id, light, allow_coalesce, "Edit Area Light"));
    recordStatus(st, "Updated area light");
    return st;
}

core::Status EditorController::setAnyLightComponent(
    scene::ObjectId id,
    scene::AnyLightComponent light,
    bool allow_coalesce,
    std::string label) {
    auto st = session_.commandStack().execute(
        session_.document().scene,
        std::make_unique<scene::SetLightComponentCommand>(
            id, std::move(light), allow_coalesce, std::move(label)));
    recordStatus(st, "Updated light settings");
    return st;
}

core::Status EditorController::convertObjectLightType(
    scene::ObjectId id,
    scene::LightType target_type) {
    const auto* obj = session_.document().scene.findObject(id);
    if (obj == nullptr) {
        auto st = core::Status::fail(core::ErrorCode::NotFound, "Object not found");
        recordStatus(st, {});
        return st;
    }

    scene::AnyLightComponent converted =
        scene::convertLightComponentType(obj->anyLightComponent(), target_type);

    auto st = session_.commandStack().execute(
        session_.document().scene,
        std::make_unique<scene::SetLightComponentCommand>(
            id, std::move(converted), false, "Change Light Type"));
    recordStatus(st, "Changed light type to " + std::string(scene::lightTypeSlug(target_type)));
    return st;
}

core::Status EditorController::setLightLinking(
    scene::ObjectId light_id,
    scene::LightLinkingSettings linking) {
    const auto* obj = session_.document().scene.findObject(light_id);
    if (obj == nullptr || !obj->hasAnyLight()) {
        auto st = core::Status::fail(core::ErrorCode::InvalidState, "Target object has no light component");
        recordStatus(st, {});
        return st;
    }

    // Keep only valid existing scene object IDs.
    scene::LightLinkingSettings cleaned{};
    cleaned.mode = linking.mode;
    cleaned.include_descendants = linking.include_descendants;
    for (const auto tid : linking.linked_objects) {
        if (tid.isValid() && session_.document().scene.findObject(tid) != nullptr) {
            cleaned.linked_objects.push_back(tid);
        }
    }
    cleaned.normalize();

    scene::AnyLightComponent updated = obj->anyLightComponent();
    if (auto* d = std::get_if<scene::DirectionalLightComponent>(&updated)) d->linking = std::move(cleaned);
    else if (auto* p = std::get_if<scene::PointLightComponent>(&updated)) p->linking = std::move(cleaned);
    else if (auto* s = std::get_if<scene::SpotLightComponent>(&updated)) s->linking = std::move(cleaned);
    else if (auto* a = std::get_if<scene::AreaLightComponent>(&updated)) a->linking = std::move(cleaned);

    auto st = session_.commandStack().execute(
        session_.document().scene,
        std::make_unique<scene::SetLightComponentCommand>(
            light_id, std::move(updated), false, "Update Light Linking"));
    recordStatus(st, "Updated light linking");
    return st;
}

core::Status EditorController::toggleLightLinkTarget(
    scene::ObjectId light_id,
    scene::ObjectId target_id,
    bool linked) {
    const auto* obj = session_.document().scene.findObject(light_id);
    if (obj == nullptr || !obj->hasAnyLight()) {
        auto st = core::Status::fail(core::ErrorCode::InvalidState, "Target object has no light component");
        recordStatus(st, {});
        return st;
    }
    if (!target_id.isValid() || session_.document().scene.findObject(target_id) == nullptr) {
        auto st = core::Status::fail(core::ErrorCode::NotFound, "Linked target object not found");
        recordStatus(st, {});
        return st;
    }

    const auto* link_ptr = obj->lightLinking();
    if (link_ptr == nullptr) {
        auto st = core::Status::fail(core::ErrorCode::InvalidState, "Target object has no light linking state");
        recordStatus(st, {});
        return st;
    }
    scene::LightLinkingSettings next = *link_ptr;
    if (linked) {
        next.linked_objects.push_back(target_id);
        next.normalize();
    } else {
        next.removeObjectReference(target_id);
    }
    return setLightLinking(light_id, std::move(next));
}

core::Status EditorController::setEnvironmentSettings(
    const scene::EnvironmentLighting& env,
    bool allow_coalesce,
    std::string label) {
    auto st = session_.commandStack().execute(
        session_.document().scene,
        std::make_unique<scene::SetEnvironmentLightingCommand>(env, allow_coalesce, std::move(label)));
    recordStatus(st, "Updated environment lighting");
    return st;
}

core::Status EditorController::loadEnvironmentHdriFromMemory(
    std::span<const std::uint8_t> bytes,
    std::string source_name) {
    const std::string resolved_name = source_name.empty() ? "Environment.hdr" : source_name;
    auto asset_res = scene::HdriEnvironmentAsset::loadFromMemory(bytes, resolved_name);
    if (!asset_res) {
        core::Status err = asset_res.error();
        recordStatus(err, {});
        return err;
    }

    scene::EnvironmentLighting next = session_.document().scene.environment();
    next.enabled = true;
    next.source_path = resolved_name;
    next.hdri_asset = std::move(asset_res.value());

    auto st = session_.commandStack().execute(
        session_.document().scene,
        std::make_unique<scene::SetEnvironmentLightingCommand>(
            std::move(next), false, "Assign HDRI Environment"));
    recordStatus(st, "Loaded HDRI environment '" + resolved_name + "'");
    return st;
}

core::Status EditorController::loadEnvironmentHdriFromFile(
    const std::filesystem::path& path,
    const platform::IFileSystem& file_system) {
    auto asset_res = scene::HdriEnvironmentAsset::loadFromFile(path, file_system);
    if (!asset_res) {
        core::Status err = asset_res.error();
        recordStatus(err, {});
        return err;
    }

    scene::EnvironmentLighting next = session_.document().scene.environment();
    next.enabled = true;
    next.source_path = path.string();
    next.hdri_asset = std::move(asset_res.value());

    auto st = session_.commandStack().execute(
        session_.document().scene,
        std::make_unique<scene::SetEnvironmentLightingCommand>(
            std::move(next), false, "Assign HDRI Environment"));
    recordStatus(st, "Loaded HDRI environment '" + path.filename().string() + "'");
    return st;
}

core::Status EditorController::loadBuiltInHdriPreset(std::string_view preset_slug) {
    scene::HdriImage img{};
    img.width = 64;
    img.height = 32;
    img.pixels.resize(static_cast<std::size_t>(img.width) * static_cast<std::size_t>(img.height));

    std::string label_name;
    if (preset_slug == "studio_softbox") {
        label_name = "Studio_Softbox_Rig.hdr";
    } else if (preset_slug == "warm_sunset") {
        label_name = "Golden_Sunset_Horizon.hdr";
    } else if (preset_slug == "cool_courtyard") {
        label_name = "Overcast_Courtyard_Dome.hdr";
    } else {
        auto st = core::Status::fail(
            core::ErrorCode::InvalidArgument,
            "Unknown studio HDRI preset: " + std::string(preset_slug));
        recordStatus(st, {});
        return st;
    }

    for (std::uint32_t y = 0; y < img.height; ++y) {
        const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(img.height);
        const float theta = v * math::kPi;
        const float sin_t = std::sin(theta);
        const float cos_t = std::cos(theta);
        for (std::uint32_t x = 0; x < img.width; ++x) {
            const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(img.width);
            const float phi = (u - 0.5f) * math::kTwoPi;
            const math::Vec3 dir{
                sin_t * std::sin(phi),
                cos_t,
                sin_t * std::cos(phi),
            };

            math::Vec3 rad{};
            if (preset_slug == "studio_softbox") {
                const float sky = std::clamp(dir.y * 0.5f + 0.5f, 0.0f, 1.0f);
                rad = math::Vec3{0.10f, 0.12f, 0.16f} * (1.0f - sky) +
                      math::Vec3{0.28f, 0.32f, 0.40f} * sky;
                const math::Vec3 key_dir = math::Vec3{-0.55f, 0.62f, 0.56f}.normalized();
                const math::Vec3 rim_dir = math::Vec3{0.70f, 0.38f, -0.60f}.normalized();
                const float key_lobe = std::pow(std::max(0.0f, dir.dot(key_dir)), 14.0f);
                const float rim_lobe = std::pow(std::max(0.0f, dir.dot(rim_dir)), 18.0f);
                rad += math::Vec3{11.5f, 10.6f, 9.4f} * key_lobe;
                rad += math::Vec3{3.8f, 5.4f, 8.2f} * rim_lobe;
            } else if (preset_slug == "warm_sunset") {
                const float horizon = std::exp(-std::fabs(dir.y) * 5.0f);
                const float upper = std::max(0.0f, dir.y);
                rad = math::Vec3{0.12f, 0.09f, 0.08f} +
                      math::Vec3{0.26f, 0.36f, 0.68f} * upper +
                      math::Vec3{1.85f, 0.78f, 0.28f} * horizon;
                const math::Vec3 sun_dir = math::Vec3{0.72f, 0.22f, 0.66f}.normalized();
                const float sun_lobe = std::pow(std::max(0.0f, dir.dot(sun_dir)), 36.0f);
                rad += math::Vec3{16.0f, 9.2f, 3.6f} * sun_lobe;
            } else {
                const float zenith = std::clamp(dir.y * 0.5f + 0.5f, 0.0f, 1.0f);
                rad = math::Vec3{0.14f, 0.16f, 0.18f} * (1.0f - zenith) +
                      math::Vec3{0.85f, 1.05f, 1.35f} * (zenith * zenith);
                const math::Vec3 sky_open = math::Vec3{0.0f, 0.94f, 0.34f}.normalized();
                rad += math::Vec3{3.4f, 3.9f, 4.6f} * std::pow(std::max(0.0f, dir.dot(sky_open)), 8.0f);
            }
            img.pixels[static_cast<std::size_t>(y) * img.width + x] = rad;
        }
    }

    auto asset_res = scene::HdriEnvironmentAsset::buildFromImage(img, label_name);
    if (!asset_res) {
        core::Status err = asset_res.error();
        recordStatus(err, {});
        return err;
    }

    scene::EnvironmentLighting next = session_.document().scene.environment();
    next.enabled = true;
    next.source_path.clear(); // Portable generated HDRI embeds RGBE directly when saved
    next.hdri_asset = std::move(asset_res.value());

    auto st = session_.commandStack().execute(
        session_.document().scene,
        std::make_unique<scene::SetEnvironmentLightingCommand>(
            std::move(next), false, "Assign Studio HDRI"));
    recordStatus(st, "Assigned HDRI preset '" + label_name + "'");
    return st;
}

core::Status EditorController::clearEnvironmentHdri() {
    scene::EnvironmentLighting next = session_.document().scene.environment();
    next.source_path.clear();
    next.hdri_asset = nullptr;

    auto st = session_.commandStack().execute(
        session_.document().scene,
        std::make_unique<scene::SetEnvironmentLightingCommand>(
            std::move(next), false, "Remove HDRI Environment"));
    recordStatus(st, "Removed HDRI map (reverted to studio sky IBL)");
    return st;
}

core::Status EditorController::updateProjectMetadata(
    std::string project_name,
    std::string author,
    std::string scene_name) {
    if (project_name.empty() || scene_name.empty()) {
        auto st = core::Status::fail(
            core::ErrorCode::InvalidArgument,
            "Project name and scene name must be non-empty");
        recordStatus(st, {});
        return st;
    }
    session_.document().metadata.project_name = std::move(project_name);
    session_.document().metadata.author = std::move(author);
    session_.document().scene.setName(std::move(scene_name));
    recordStatus(core::Status::ok(), "Updated project metadata");
    return core::Status::ok();
}

core::Status EditorController::undo() {
    if (!session_.commandStack().canUndo()) {
        auto st = core::Status::fail(core::ErrorCode::InvalidState, "Nothing to undo");
        recordStatus(st, {});
        return st;
    }
    const std::string desc(session_.commandStack().nextUndoDescription());
    auto st = session_.commandStack().undo(session_.document().scene);
    if (st) {
        session_.sanitizeObjectSelection();
        session_.sanitizeMeshSelection();
        refreshMaterialPreview();
        const auto active_cam = session_.document().scene.activeCamera();
        if (session_.isCameraViewActive() && active_cam.isValid()) {
            if (const auto* cam_obj = session_.document().scene.findObject(active_cam); cam_obj != nullptr) {
                session_.cameraController().syncFromCameraTransform(
                    cam_obj->local_transform,
                    session_.cameraController().target());
            }
        }
    }
    recordStatus(st, "Undid: " + desc);
    return st;
}

core::Status EditorController::redo() {
    if (!session_.commandStack().canRedo()) {
        auto st = core::Status::fail(core::ErrorCode::InvalidState, "Nothing to redo");
        recordStatus(st, {});
        return st;
    }
    const std::string desc(session_.commandStack().nextRedoDescription());
    auto st = session_.commandStack().redo(session_.document().scene);
    if (st) {
        session_.sanitizeObjectSelection();
        session_.sanitizeMeshSelection();
        refreshMaterialPreview();
        const auto active_cam = session_.document().scene.activeCamera();
        if (session_.isCameraViewActive() && active_cam.isValid()) {
            if (const auto* cam_obj = session_.document().scene.findObject(active_cam); cam_obj != nullptr) {
                session_.cameraController().syncFromCameraTransform(
                    cam_obj->local_transform,
                    session_.cameraController().target());
            }
        }
    }
    recordStatus(st, "Redid: " + desc);
    return st;
}

core::Status EditorController::newStarterProject() {
    auto st = session_.resetStarterProject();
    spawn_counter_ = 0;
    selected_material_id_ = scene::MaterialId::invalid();
    refreshMaterialPreview();
    recordStatus(st, "Reset to starter 3D showcase project");
    return st;
}

core::Status EditorController::createNewProject(
    ProjectTemplate tmpl,
    std::string project_name,
    std::string author) {
    if (session_.isGizmoDragging()) {
        (void)session_.cancelActiveGizmoDrag();
    }

    const std::string resolved_name =
        project_name.empty() ? "Untitled 3D Project" : std::move(project_name);
    const std::string resolved_author =
        author.empty() ? "Final Effect 3D" : std::move(author);

    if (tmpl == ProjectTemplate::StarterShowcase) {
        auto st = session_.resetStarterProject();
        if (!st) {
            recordStatus(st, {});
            return st;
        }
        if (!project_name.empty() || resolved_name != "Untitled 3D Project") {
            session_.document().metadata.project_name = resolved_name;
        }
        session_.document().metadata.author = resolved_author;
        spawn_counter_ = 0;
        selected_material_id_ = scene::MaterialId::invalid();
        refreshMaterialPreview();
        recordStatus(core::Status::ok(), "Created project '" + session_.document().metadata.project_name + "'");
        return core::Status::ok();
    }

    if (session_.renderer().isInitialized()) {
        session_.renderer().releaseSceneResources();
    }

    const auto preserved_ws = session_.workspace().state();
    project::ProjectDocument fresh{};
    fresh.metadata.project_name = resolved_name;
    fresh.metadata.author = resolved_author;
    fresh.metadata.engine_version = std::string(core::engineVersionString());
    fresh.workspace = preserved_ws;

    auto& sc = fresh.scene;
    sc.setName("Scene");

    // Standard studio camera + directional key light for every template, positioned visibly inside the stage.
    auto cam_res = sc.createObject("MainCamera");
    if (!cam_res) { recordStatus(core::Status(cam_res.error()), {}); return cam_res.error(); }
    math::Transform cam_tf{};
    const math::Vec3 eye{1.55f, 2.05f, 1.95f};
    const math::Vec3 target{0.0f, 0.72f, 0.0f};
    const math::Vec3 cam_off = eye - target;
    const float cam_horiz = std::sqrt(cam_off.x * cam_off.x + cam_off.z * cam_off.z);
    cam_tf.position = eye;
    cam_tf.rotation = math::Quat::fromEulerXYZ(
        {-std::atan2(cam_off.y, cam_horiz), std::atan2(cam_off.x, cam_off.z), 0.0f});
    (void)sc.setObjectTransform(cam_res.value(), cam_tf);
    scene::CameraComponent cam_comp{};
    cam_comp.projection = scene::ProjectionType::Perspective;
    cam_comp.fov_y_radians = math::degToRad(48.0f);
    (void)sc.assignCamera(cam_res.value(), cam_comp);
    (void)sc.setActiveCamera(cam_res.value());

    auto light_res = sc.createObject("SunLight");
    if (!light_res) { recordStatus(core::Status(light_res.error()), {}); return light_res.error(); }
    math::Transform light_tf{};
    light_tf.position = {-1.45f, 2.25f, 0.85f};
    light_tf.rotation = math::Quat::fromEulerXYZ(
        {math::degToRad(-42.0f), math::degToRad(-52.0f), 0.0f});
    (void)sc.setObjectTransform(light_res.value(), light_tf);
    (void)sc.assignDirectionalLight(
        light_res.value(),
        scene::DirectionalLightComponent{.color = {1.0f, 0.98f, 0.94f}, .intensity = 1.65f});

    scene::ObjectId default_selected{};
    scene::MaterialId default_mat{};

    if (tmpl == ProjectTemplate::MinimalCube) {
        auto neutral_mat = sc.createMaterial(scene::PbrMaterial{
            .name = "Studio Clay",
            .base_color = {0.76f, 0.78f, 0.82f},
            .metallic = 0.08f,
            .roughness = 0.42f,
            .ambient_occlusion = 1.0f,
        });
        auto cube_mesh = sc.registerEditableMesh(
            "CubeMesh", scene::EditableMesh::createCube(1.3f, {0.80f, 0.82f, 0.85f}));
        auto cube_obj = sc.createObject("Cube");
        if (cube_mesh && cube_obj) {
            math::Transform ctf{};
            ctf.position = {0.0f, 0.65f, 0.0f};
            (void)sc.setObjectTransform(cube_obj.value(), ctf);
            (void)sc.assignMesh(cube_obj.value(), cube_mesh.value());
            if (neutral_mat) {
                (void)sc.assignMaterial(cube_obj.value(), neutral_mat.value());
                default_mat = neutral_mat.value();
            }
            default_selected = cube_obj.value();
        }
    } else if (tmpl == ProjectTemplate::ShaderBall) {
        auto gold_mat = sc.createMaterial(scene::PbrMaterial{
            .name = "Brushed Gold",
            .base_color = {0.96f, 0.76f, 0.28f},
            .metallic = 0.94f,
            .roughness = 0.18f,
            .ambient_occlusion = 1.0f,
        });
        (void)sc.createMaterial(scene::PbrMaterial{
            .name = "Chrome Mirror",
            .base_color = {0.91f, 0.93f, 0.95f},
            .metallic = 1.0f,
            .roughness = 0.06f,
            .ambient_occlusion = 1.0f,
        });
        (void)sc.createMaterial(scene::PbrMaterial{
            .name = "Ceramic Matte",
            .base_color = {0.22f, 0.58f, 0.86f},
            .metallic = 0.05f,
            .roughness = 0.48f,
            .ambient_occlusion = 1.0f,
        });

        auto sphere_mesh = sc.registerEditableMesh(
            "ShaderSphereMesh",
            scene::EditableMesh::createUvSphere(0.85f, 18, 32, {0.95f, 0.95f, 0.95f}));
        auto sphere_obj = sc.createObject("ShaderSphere");
        if (sphere_mesh && sphere_obj) {
            math::Transform stf{};
            stf.position = {0.0f, 0.85f, 0.0f};
            (void)sc.setObjectTransform(sphere_obj.value(), stf);
            (void)sc.assignMesh(sphere_obj.value(), sphere_mesh.value());
            if (gold_mat) {
                (void)sc.assignMaterial(sphere_obj.value(), gold_mat.value());
                default_mat = gold_mat.value();
            }
            default_selected = sphere_obj.value();
        }
    }

    fresh.editor_state.selected_object_id = default_selected;
    fresh.editor_state.active_material_id = default_mat;

    session_.document() = std::move(fresh);
    session_.commandStack().clear();
    session_.restoreFromDocumentState();
    session_.setRigAnimationEnabled(false);
    session_.setCameraViewActive(false);
    math::Transform editor_eye{};
    editor_eye.position = {0.0f, 2.95f, 6.10f};
    session_.cameraController().syncFromCameraTransform(editor_eye, {0.0f, 0.80f, 0.0f});
    spawn_counter_ = 0;
    selected_material_id_ = default_mat;
    refreshMaterialPreview();
    recordStatus(core::Status::ok(), "Created project '" + session_.document().metadata.project_name + "'");
    return core::Status::ok();
}

core::Result<std::string> EditorController::exportInlineProjectJson() const {
    const_cast<StudioSession&>(session_).syncDocumentState(selectedMaterial());
    return project::ProjectSerializer::serializeToString(session_.document(), true);
}

core::Status EditorController::importInlineProjectJson(std::string_view json_text) {
    if (session_.isGizmoDragging()) {
        (void)session_.cancelActiveGizmoDrag();
    }
    auto loaded = project::ProjectSerializer::deserializeFromString(json_text);
    if (!loaded) {
        core::Status st = loaded.error();
        recordStatus(st, {});
        return st;
    }
    if (session_.renderer().isInitialized()) {
        session_.renderer().releaseSceneResources();
    }
    session_.document() = std::move(loaded).value();
    session_.commandStack().clear();
    session_.restoreFromDocumentState();
    session_.setRigAnimationEnabled(false);
    session_.setCameraViewActive(false);
    math::Transform editor_eye{};
    editor_eye.position = {0.0f, 2.95f, 6.10f};
    session_.cameraController().syncFromCameraTransform(editor_eye, {0.0f, 0.80f, 0.0f});
    selected_material_id_ = session_.document().editor_state.active_material_id;
    refreshMaterialPreview();
    for (const auto& warn : session_.document().load_warnings) {
        console_entries_.push_back(EditorConsoleEntry{
            .sequence = ++console_seq_,
            .is_error = false,
            .message = "[Warning] " + warn,
        });
    }
    recordStatus(core::Status::ok(), "Loaded project '" + session_.document().metadata.project_name + "'");
    return core::Status::ok();
}

core::Status EditorController::saveBundleProject(
    const std::filesystem::path& fe3d_path,
    platform::IFileSystem& file_system) {
    session_.syncDocumentState(selectedMaterial());
    auto st = session_.saveProjectBundleToFile(fe3d_path, file_system);
    recordStatus(st, "Exported .fe3d + .fe3dbin binary bundle");
    return st;
}

core::Status EditorController::loadProjectFile(
    const std::filesystem::path& fe3d_path,
    const platform::IFileSystem& file_system) {
    auto st = session_.loadProjectFromFile(fe3d_path, file_system);
    if (st) {
        session_.setRigAnimationEnabled(false);
        selected_material_id_ = session_.document().editor_state.active_material_id;
        refreshMaterialPreview();
        for (const auto& warn : session_.document().load_warnings) {
            console_entries_.push_back(EditorConsoleEntry{
                .sequence = ++console_seq_,
                .is_error = false,
                .message = "[Warning] " + warn,
            });
        }
    }
    recordStatus(st, "Loaded project '" + session_.document().metadata.project_name + "'");
    return st;
}

} // namespace finaleffect::app
