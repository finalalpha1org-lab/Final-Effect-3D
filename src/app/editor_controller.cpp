// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/app/editor_controller.hpp"
#include "finaleffect/core/version.hpp"
#include "finaleffect/math/constants.hpp"
#include "finaleffect/scene/editable_mesh.hpp"
#include "finaleffect/scene/primitives.hpp"

#include <cmath>
#include <cstdio>
#include <memory>
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
        case ScenePreset::Cube:    target_name = "HeroCubeMesh"; break;
        case ScenePreset::Pyramid: target_name = "AccentPyramidMesh"; break;
        case ScenePreset::Plane:   target_name = "StudioGroundMesh"; break;
        case ScenePreset::Sphere:  target_name = "StudioSphereMesh"; break;
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
    std::optional<scene::DirectionalLightComponent> light_comp;
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
            light_comp = scene::DirectionalLightComponent{
                .color = {1.0f, 0.96f, 0.90f},
                .intensity = 1.4f,
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
        light_comp,
        mat_id);
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
        src->directional_light,
        src->material_id);
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
    math::Transform tf;
    tf.position = position;
    tf.rotation = math::Quat::fromEulerXYZ({
        math::degToRad(euler_degrees.x),
        math::degToRad(euler_degrees.y),
        math::degToRad(euler_degrees.z),
    });
    tf.scale = scale;

    auto st = session_.commandStack().execute(
        session_.document().scene,
        std::make_unique<scene::SetTransformCommand>(id, tf, allow_coalesce));
    if (st && session_.isCameraViewActive() && id == session_.document().scene.activeCamera()) {
        session_.cameraController().syncFromCameraTransform(tf, session_.cameraController().target());
    }
    recordStatus(st, "Updated transform");
    return st;
}

core::Status EditorController::resetObjectTransform(scene::ObjectId id) {
    auto st = session_.commandStack().execute(
        session_.document().scene,
        std::make_unique<scene::SetTransformCommand>(id, math::Transform::identity(), false));
    recordStatus(st, "Reset transform to identity");
    return st;
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
        std::make_unique<scene::SetDirectionalLightCommand>(id, light, allow_coalesce));
    recordStatus(st, "Updated directional light");
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
        if (session_.selectedObject().isValid() &&
            session_.document().scene.findObject(session_.selectedObject()) == nullptr) {
            session_.setSelectedObject({});
        }
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
        if (session_.selectedObject().isValid() &&
            session_.document().scene.findObject(session_.selectedObject()) == nullptr) {
            session_.setSelectedObject({});
        }
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
        auto ground_mesh = sc.registerMesh(
            "StudioGroundMesh", scene::Primitives::createCheckerPlane(4.5f, 6));
        auto ground_obj = sc.createObject("GroundPlane");
        if (ground_mesh && ground_obj) {
            (void)sc.assignMesh(ground_obj.value(), ground_mesh.value());
        }

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
        auto ground_mesh = sc.registerMesh(
            "StudioGroundMesh", scene::Primitives::createCheckerPlane(4.5f, 6));
        auto ground_obj = sc.createObject("GroundPlane");
        if (ground_mesh && ground_obj) {
            (void)sc.assignMesh(ground_obj.value(), ground_mesh.value());
        }

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
