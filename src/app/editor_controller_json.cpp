// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/app/editor_controller.hpp"
#include "finaleffect/math/constants.hpp"
#include "finaleffect/project/json.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace finaleffect::app {

namespace {

project::JsonValue vec3ToJson(const math::Vec3& v) {
    return project::JsonArray{
        project::JsonValue(static_cast<double>(v.x)),
        project::JsonValue(static_cast<double>(v.y)),
        project::JsonValue(static_cast<double>(v.z)),
    };
}

project::JsonValue u32SpanToJson(std::span<const std::uint32_t> vals) {
    project::JsonArray arr;
    arr.reserve(vals.size());
    for (const auto v : vals) {
        arr.emplace_back(static_cast<std::uint64_t>(v));
    }
    return arr;
}

project::JsonValue rectToJson(const PanelRect& r) {
    return project::JsonArray{
        project::JsonValue(static_cast<double>(r.x)),
        project::JsonValue(static_cast<double>(r.y)),
        project::JsonValue(static_cast<double>(r.width)),
        project::JsonValue(static_cast<double>(r.height)),
    };
}

core::Result<std::uint64_t> parseJsonId(const project::JsonValue* v, std::string_view name) {
    if (v == nullptr || !v->isNumber()) {
        return core::Result<std::uint64_t>::fail(
            core::ErrorCode::InvalidArgument,
            "Expected numeric ID field",
            std::string(name));
    }
    const double d = v->asNumber();
    if (!std::isfinite(d) || d < 0.0 || d > 9007199254740991.0 || std::floor(d) != d) {
        return core::Result<std::uint64_t>::fail(
            core::ErrorCode::InvalidArgument,
            "ID field must be a non-negative integer",
            std::string(name));
    }
    return core::Result<std::uint64_t>::ok(static_cast<std::uint64_t>(d));
}

core::Result<float> parseJsonFloat(const project::JsonValue* v, std::string_view name) {
    if (v == nullptr || !v->isNumber()) {
        return core::Result<float>::fail(
            core::ErrorCode::InvalidArgument,
            "Expected numeric field",
            std::string(name));
    }
    const double d = v->asNumber();
    if (!std::isfinite(d) || std::fabs(d) > static_cast<double>(std::numeric_limits<float>::max())) {
        return core::Result<float>::fail(
            core::ErrorCode::InvalidArgument,
            "Numeric field out of 32-bit float range",
            std::string(name));
    }
    return core::Result<float>::ok(static_cast<float>(d));
}

core::Result<math::Vec3> parseJsonVec3(const project::JsonValue* v, std::string_view name) {
    if (v == nullptr || !v->isArray() || v->asArray().size() != 3) {
        return core::Result<math::Vec3>::fail(
            core::ErrorCode::InvalidArgument,
            "Expected 3-element numeric array",
            std::string(name));
    }
    const auto& arr = v->asArray();
    auto x = parseJsonFloat(&arr[0], name);
    auto y = parseJsonFloat(&arr[1], name);
    auto z = parseJsonFloat(&arr[2], name);
    if (!x) return x.error();
    if (!y) return y.error();
    if (!z) return z.error();
    return core::Result<math::Vec3>::ok(math::Vec3{x.value(), y.value(), z.value()});
}

std::string_view editModeToString(scene::EditMode mode) noexcept {
    switch (mode) {
        case scene::EditMode::Vertex: return "vertex";
        case scene::EditMode::Edge:   return "edge";
        case scene::EditMode::Face:   return "face";
        case scene::EditMode::Object:
        default:
            return "object";
    }
}

std::optional<scene::EditMode> parseEditModeString(std::string_view s) noexcept {
    if (s == "object") return scene::EditMode::Object;
    if (s == "vertex") return scene::EditMode::Vertex;
    if (s == "edge")   return scene::EditMode::Edge;
    if (s == "face")   return scene::EditMode::Face;
    return std::nullopt;
}

std::optional<project::WorkspacePanelId> parsePanelKeyFlexible(std::string_view s) noexcept {
    if (const auto direct = project::parseWorkspacePanelSlug(s); direct.has_value()) {
        return direct;
    }
    if (s == "viewport" || s == "viewport3d") return project::WorkspacePanelId::Viewport3D;
    if (s == "outliner")  return project::WorkspacePanelId::SceneOutliner;
    if (s == "inspector") return project::WorkspacePanelId::Properties;
    if (s == "materials") return project::WorkspacePanelId::MaterialEditor;
    if (s == "assets")    return project::WorkspacePanelId::AssetBrowser;
    return std::nullopt;
}

} // namespace

std::string EditorController::serializeStateJson() const {
    using project::JsonArray;
    using project::JsonObject;
    using project::JsonValue;

    const_cast<EditorController*>(this)->refreshMaterialPreview();

    const auto& doc = session_.document();
    const auto& scene = doc.scene;
    const auto& stack = session_.commandStack();
    const auto stats = session_.collectResourceStats();
    const auto& caps = session_.renderer().device().capabilities();

    JsonValue root{JsonObject{}};
    root.setKey("revision", stateRevision());

    JsonValue proj{JsonObject{}};
    proj.setKey("project_name", doc.metadata.project_name);
    proj.setKey("author", doc.metadata.author);
    proj.setKey("description", doc.metadata.description);
    proj.setKey("engine_version", doc.metadata.engine_version);
    proj.setKey("scene_name", scene.name());
    root.setKey("project", std::move(proj));

    JsonValue vp{JsonObject{}};
    vp.setKey("backend_name", caps.backend_name);
    vp.setKey("device_name", caps.device_name);
    vp.setKey("rig_playing", session_.isRigAnimationEnabled());
    vp.setKey("active_camera_id", scene.activeCamera().value);
    vp.setKey("selected_object_id", session_.selectedObject().value);
    vp.setKey("camera_target", vec3ToJson(session_.cameraController().target()));
    vp.setKey("camera_distance", static_cast<double>(session_.cameraController().distance()));
    vp.setKey("camera_yaw_deg", static_cast<double>(math::radToDeg(session_.cameraController().yawRadians())));
    vp.setKey("camera_pitch_deg", static_cast<double>(math::radToDeg(session_.cameraController().pitchRadians())));
    root.setKey("viewport", std::move(vp));

    // Stage 4: Editor mode, 3D Transform Gizmo state & Mesh Edit selection summary.
    JsonValue ed{JsonObject{}};
    ed.setKey("mode", std::string(editModeToString(session_.editorMode())));
    ed.setKey("gizmo_mode", std::string(gizmoModeName(session_.gizmo().mode())));
    ed.setKey("gizmo_space", std::string(gizmoSpaceName(session_.gizmo().space())));
    ed.setKey("gizmo_hovered_handle", std::string(gizmoHandleName(session_.hoveredGizmoHandle())));
    ed.setKey("gizmo_active_handle", std::string(gizmoHandleName(session_.gizmo().activeHandle())));
    ed.setKey("gizmo_dragging", session_.isGizmoDragging());

    JsonValue msel{JsonObject{}};
    msel.setKey("vertices", u32SpanToJson(session_.meshSelection().vertices));
    msel.setKey("edges", u32SpanToJson(session_.meshSelection().edges));
    msel.setKey("faces", u32SpanToJson(session_.meshSelection().faces));
    ed.setKey("mesh_selection", std::move(msel));

    JsonValue msum{JsonObject{}};
    bool has_editable = false;
    std::uint64_t edit_verts = 0;
    std::uint64_t edit_edges = 0;
    std::uint64_t edit_faces = 0;
    std::uint64_t open_boundary_edges = 0;
    if (session_.selectedObject().isValid()) {
        if (const auto* obj = scene.findObject(session_.selectedObject());
            obj != nullptr && obj->mesh_id.has_value()) {
            if (const auto* ma = scene.findMesh(obj->mesh_id.value());
                ma != nullptr && ma->editable_mesh.has_value()) {
                has_editable = true;
                const auto& em = ma->editable_mesh.value();
                edit_verts = static_cast<std::uint64_t>(em.vertexCount());
                edit_edges = static_cast<std::uint64_t>(em.edgeCount());
                edit_faces = static_cast<std::uint64_t>(em.faceCount());
                for (const auto& e : em.edges()) {
                    if (e.isBoundary()) ++open_boundary_edges;
                }
            }
        }
    }
    msum.setKey("editable", has_editable);
    msum.setKey("vertex_count", edit_verts);
    msum.setKey("edge_count", edit_edges);
    msum.setKey("face_count", edit_faces);
    msum.setKey("open_boundary_edges", open_boundary_edges);
    ed.setKey("editable_mesh", std::move(msum));
    root.setKey("editor", std::move(ed));

    JsonValue hist{JsonObject{}};
    hist.setKey("can_undo", stack.canUndo());
    hist.setKey("can_redo", stack.canRedo());
    hist.setKey("undo_count", static_cast<std::uint64_t>(stack.undoCount()));
    hist.setKey("redo_count", static_cast<std::uint64_t>(stack.redoCount()));
    hist.setKey("next_undo", std::string(stack.nextUndoDescription()));
    hist.setKey("next_redo", std::string(stack.nextRedoDescription()));
    hist.setKey("memory_bytes", static_cast<std::uint64_t>(stack.totalMemoryBytes()));
    root.setKey("history", std::move(hist));

    JsonValue st_json{JsonObject{}};
    st_json.setKey("object_count", static_cast<std::uint64_t>(stats.scene_object_count));
    st_json.setKey("mesh_count", static_cast<std::uint64_t>(stats.mesh_asset_count));
    st_json.setKey("material_count", static_cast<std::uint64_t>(stats.material_count));
    st_json.setKey("total_vertices", static_cast<std::uint64_t>(stats.total_cpu_vertices));
    st_json.setKey("total_triangles", static_cast<std::uint64_t>(stats.total_cpu_indices / 3u));
    st_json.setKey("cpu_mesh_bytes", static_cast<std::uint64_t>(stats.cpu_mesh_bytes));
    st_json.setKey("gpu_buffer_count", static_cast<std::uint64_t>(stats.gpu_mesh_buffer_count));
    st_json.setKey("gpu_mesh_bytes", static_cast<std::uint64_t>(stats.gpu_mesh_bytes));
    st_json.setKey("gpu_buffer_updates", static_cast<std::uint64_t>(stats.gpu_buffer_update_count));
    root.setKey("stats", std::move(st_json));

    JsonValue status_obj{JsonObject{}};
    status_obj.setKey("level", last_status_is_error_ ? "error" : "info");
    status_obj.setKey("message", last_status_message_);
    root.setKey("status", std::move(status_obj));

    // Stage 4: PBR Material Library & Live 3D Sphere Preview metadata.
    JsonArray mats_arr;
    mats_arr.reserve(scene.materials().size());
    for (const auto& mat : scene.materials()) {
        JsonValue mj{JsonObject{}};
        mj.setKey("id", mat.id.value);
        mj.setKey("name", mat.name);
        mj.setKey("base_color", vec3ToJson(mat.base_color));
        mj.setKey("metallic", static_cast<double>(mat.metallic));
        mj.setKey("roughness", static_cast<double>(mat.roughness));
        mj.setKey("ambient_occlusion", static_cast<double>(mat.ambient_occlusion));
        mj.setKey("emissive_color", vec3ToJson(mat.emissive_color));
        mj.setKey("emissive_strength", static_cast<double>(mat.emissive_strength));
        mj.setKey("opacity", static_cast<double>(mat.opacity));
        mats_arr.push_back(std::move(mj));
    }
    root.setKey("materials", std::move(mats_arr));
    root.setKey("active_material_id", selectedMaterial().value);
    root.setKey("material_preview_generation", materialPreviewGeneration());

    // Stage 4: Workspace panels, preset & docking geometry.
    const auto& ws = session_.workspace();
    const auto& ws_state = ws.state();
    const auto dock = ws.computeDockLayout(1280, 720);
    JsonValue ws_json{JsonObject{}};
    ws_json.setKey("layout_name", ws_state.layout_name);
    ws_json.setKey("left_ratio", static_cast<double>(ws_state.left_dock_width_ratio));
    ws_json.setKey("right_ratio", static_cast<double>(ws_state.right_dock_width_ratio));
    ws_json.setKey("bottom_ratio", static_cast<double>(ws_state.bottom_dock_height_ratio));
    ws_json.setKey("center_viewport_rect", rectToJson(dock.center_viewport));

    JsonArray panels_arr;
    panels_arr.reserve(project::kWorkspacePanelCount);
    for (std::size_t i = 0; i < project::kWorkspacePanelCount; ++i) {
        const auto pid = static_cast<project::WorkspacePanelId>(i);
        JsonValue pj{JsonObject{}};
        pj.setKey("id", static_cast<std::uint64_t>(i));
        pj.setKey("key", std::string(project::workspacePanelSlug(pid)));
        pj.setKey("title", std::string(project::workspacePanelTitle(pid)));
        pj.setKey("visible", ws.isPanelVisible(pid));
        pj.setKey("dock", std::string(project::dockRegionSlug(ws_state.panels[i].dock_region)));
        panels_arr.push_back(std::move(pj));
    }
    ws_json.setKey("panels", std::move(panels_arr));
    root.setKey("workspace", std::move(ws_json));

    JsonArray console_arr;
    console_arr.reserve(console_entries_.size());
    for (const auto& ce : console_entries_) {
        JsonValue cj{JsonObject{}};
        cj.setKey("seq", ce.sequence);
        cj.setKey("level", ce.is_error ? "error" : "info");
        cj.setKey("message", ce.message);
        console_arr.push_back(std::move(cj));
    }
    root.setKey("console", std::move(console_arr));

    // Build deterministic hierarchical Outliner tree in pre-order (sorted by ObjectId among siblings).
    std::vector<const scene::SceneObject*> roots;
    for (const auto& obj : scene.objects()) {
        if (!obj.parent_id.isValid() || scene.findObject(obj.parent_id) == nullptr) {
            roots.push_back(&obj);
        }
    }
    std::sort(roots.begin(), roots.end(), [](const auto* a, const auto* b) { return a->id < b->id; });

    JsonArray outliner_arr;
    outliner_arr.reserve(scene.objectCount());

    auto appendSubtree = [&](auto& self, const scene::SceneObject* node, std::uint32_t depth) -> void {
        JsonValue item{JsonObject{}};
        item.setKey("id", node->id.value);
        item.setKey("name", node->name);
        item.setKey("visible", node->visible);
        item.setKey("parent_id", node->parent_id.value);
        item.setKey("depth", static_cast<std::uint64_t>(depth));
        item.setKey("has_mesh", node->mesh_id.has_value());
        item.setKey("has_camera", node->camera.has_value());
        item.setKey("has_light", node->directional_light.has_value());
        item.setKey("material_id", node->material_id.has_value() ? node->material_id->value : std::uint64_t{0});
        item.setKey("is_active_camera", node->id == scene.activeCamera());
        outliner_arr.push_back(std::move(item));

        std::vector<const scene::SceneObject*> kids;
        kids.reserve(node->children.size());
        for (const auto cid : node->children) {
            if (const auto* child_ptr = scene.findObject(cid); child_ptr != nullptr) {
                kids.push_back(child_ptr);
            }
        }
        std::sort(kids.begin(), kids.end(), [](const auto* a, const auto* b) { return a->id < b->id; });
        for (const auto* child_ptr : kids) {
            self(self, child_ptr, depth + 1u);
        }
    };

    for (const auto* r : roots) {
        appendSubtree(appendSubtree, r, 0u);
    }
    root.setKey("objects", std::move(outliner_arr));

    // Populate selected object Inspector payload when valid.
    if (session_.selectedObject().isValid()) {
        if (const auto* sel_obj = scene.findObject(session_.selectedObject()); sel_obj != nullptr) {
            JsonValue insp{JsonObject{}};
            insp.setKey("id", sel_obj->id.value);
            insp.setKey("name", sel_obj->name);
            insp.setKey("visible", sel_obj->visible);
            insp.setKey("parent_id", sel_obj->parent_id.value);
            insp.setKey(
                "material_id",
                sel_obj->material_id.has_value() ? sel_obj->material_id->value : std::uint64_t{0});
            insp.setKey("position", vec3ToJson(sel_obj->local_transform.position));

            const math::Vec3 euler_rad = sel_obj->local_transform.rotation.toEulerXYZ();
            insp.setKey("rotation_deg", vec3ToJson(math::Vec3{
                math::radToDeg(euler_rad.x),
                math::radToDeg(euler_rad.y),
                math::radToDeg(euler_rad.z),
            }));
            insp.setKey("scale", vec3ToJson(sel_obj->local_transform.scale));

            if (sel_obj->mesh_id.has_value()) {
                if (const auto* mesh_asset = scene.findMesh(sel_obj->mesh_id.value()); mesh_asset != nullptr) {
                    JsonValue m_json{JsonObject{}};
                    m_json.setKey("id", mesh_asset->id.value);
                    m_json.setKey("name", mesh_asset->name);
                    m_json.setKey("is_editable", mesh_asset->editable_mesh.has_value());
                    m_json.setKey("vertex_count", static_cast<std::uint64_t>(mesh_asset->data.vertices.size()));
                    m_json.setKey(
                        "triangle_count",
                        static_cast<std::uint64_t>(mesh_asset->data.indices.size() / 3u));
                    m_json.setKey(
                        "face_count",
                        mesh_asset->editable_mesh.has_value()
                            ? static_cast<std::uint64_t>(mesh_asset->editable_mesh->faceCount())
                            : static_cast<std::uint64_t>(mesh_asset->data.indices.size() / 3u));
                    m_json.setKey("bounds_size", vec3ToJson(mesh_asset->local_bounds.size()));
                    insp.setKey("mesh", std::move(m_json));
                }
            }

            if (sel_obj->camera.has_value()) {
                const auto& cam = sel_obj->camera.value();
                JsonValue c_json{JsonObject{}};
                c_json.setKey(
                    "projection",
                    cam.projection == scene::ProjectionType::Perspective ? "perspective" : "orthographic");
                c_json.setKey("fov_deg", static_cast<double>(math::radToDeg(cam.fov_y_radians)));
                c_json.setKey("ortho_height", static_cast<double>(cam.orthographic_height));
                c_json.setKey("near_plane", static_cast<double>(cam.near_plane));
                c_json.setKey("far_plane", static_cast<double>(cam.far_plane));
                c_json.setKey("is_active", sel_obj->id == scene.activeCamera());
                insp.setKey("camera", std::move(c_json));
            }

            if (sel_obj->directional_light.has_value()) {
                const auto& lgt = sel_obj->directional_light.value();
                JsonValue l_json{JsonObject{}};
                l_json.setKey("color", vec3ToJson(lgt.color));
                l_json.setKey("intensity", static_cast<double>(lgt.intensity));
                insp.setKey("light", std::move(l_json));
            }

            root.setKey("inspector", std::move(insp));
        } else {
            root.setKey("inspector", JsonValue{});
        }
    } else {
        root.setKey("inspector", JsonValue{});
    }

    auto serialized = project::writeJson(root, false);
    return serialized.hasValue() ? serialized.value() : "{}";
}

core::Status EditorController::executeActionJson(std::string_view action_json) {
    auto parsed = project::parseJson(action_json);
    if (!parsed) {
        recordStatus(core::Status(parsed.error()), {});
        return parsed.error();
    }
    const auto& cmd = parsed.value();
    if (!cmd.isObject()) {
        auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Editor action JSON must be an object");
        recordStatus(st, {});
        return st;
    }

    const auto* action_val = cmd.findKey("action");
    if (action_val == nullptr || !action_val->isString()) {
        auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Missing string 'action' key");
        recordStatus(st, {});
        return st;
    }
    const std::string& action = action_val->asString();

    if (action == "select_object") {
        auto id_res = parseJsonId(cmd.findKey("id"), "id");
        if (!id_res) { recordStatus(core::Status(id_res.error()), {}); return id_res.error(); }
        return selectObject(scene::ObjectId{id_res.value()});
    }

    // Stage 4: Editor mode & Gizmo actions.
    if (action == "set_editor_mode") {
        const auto* mv = cmd.findKey("mode");
        if (mv == nullptr || !mv->isString()) {
            auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Missing string 'mode'");
            recordStatus(st, {});
            return st;
        }
        const auto parsed_mode = parseEditModeString(mv->asString());
        if (!parsed_mode.has_value()) {
            auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Invalid editor mode: " + mv->asString());
            recordStatus(st, {});
            return st;
        }
        return setEditorMode(parsed_mode.value());
    }

    if (action == "set_gizmo_mode") {
        const auto* mv = cmd.findKey("mode");
        if (mv == nullptr || !mv->isString()) {
            auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Missing string 'mode'");
            recordStatus(st, {});
            return st;
        }
        const std::string& ms = mv->asString();
        if (ms == "translate" || ms == "move") setGizmoMode(GizmoMode::Translate);
        else if (ms == "rotate") setGizmoMode(GizmoMode::Rotate);
        else if (ms == "scale") setGizmoMode(GizmoMode::Scale);
        else {
            auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Invalid gizmo mode: " + ms);
            recordStatus(st, {});
            return st;
        }
        return core::Status::ok();
    }

    if (action == "set_gizmo_space" || action == "set_gizmo_orientation") {
        const auto* ov = cmd.findKey("space");
        if (ov == nullptr) ov = cmd.findKey("orientation");
        if (ov == nullptr || !ov->isString()) {
            auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Missing string 'space'");
            recordStatus(st, {});
            return st;
        }
        const std::string& os = ov->asString();
        if (os == "world") setGizmoSpace(GizmoSpace::World);
        else if (os == "local") setGizmoSpace(GizmoSpace::Local);
        else {
            auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Invalid gizmo space: " + os);
            recordStatus(st, {});
            return st;
        }
        return core::Status::ok();
    }

    if (action == "cancel_gizmo_drag") {
        return cancelGizmoDrag();
    }

    // Stage 4: Mesh Editing actions.
    if (action == "select_mesh_element") {
        const auto* kv = cmd.findKey("kind");
        auto idx = parseJsonId(cmd.findKey("index"), "index");
        if (kv == nullptr || !kv->isString() || !idx) {
            auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Invalid select_mesh_element payload");
            recordStatus(st, {});
            return st;
        }
        const auto kind = parseEditModeString(kv->asString());
        if (!kind.has_value() || kind.value() == scene::EditMode::Object) {
            auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Invalid mesh element kind");
            recordStatus(st, {});
            return st;
        }
        bool additive = false;
        if (const auto* av = cmd.findKey("additive"); av != nullptr && av->isBool()) {
            additive = av->asBool();
        }
        return selectMeshElement(kind.value(), static_cast<std::uint32_t>(idx.value()), additive);
    }

    if (action == "clear_mesh_selection") {
        clearMeshSelection();
        return core::Status::ok();
    }

    if (action == "select_boundary_loop") {
        return selectBoundaryLoopVertices();
    }

    if (action == "translate_mesh_selection") {
        auto delta = parseJsonVec3(cmd.findKey("delta"), "delta");
        if (!delta) { recordStatus(core::Status(delta.error()), {}); return delta.error(); }
        bool coalesce = false;
        if (const auto* cv = cmd.findKey("coalesce"); cv != nullptr && cv->isBool()) {
            coalesce = cv->asBool();
        }
        return translateMeshSelection(delta.value(), coalesce);
    }

    if (action == "extrude_selected_faces") {
        float dist = 0.35f;
        if (const auto* dv = cmd.findKey("distance"); dv != nullptr) {
            auto dr = parseJsonFloat(dv, "distance");
            if (!dr) { recordStatus(core::Status(dr.error()), {}); return dr.error(); }
            dist = dr.value();
        }
        return extrudeSelectedFaces(dist);
    }

    if (action == "delete_mesh_selection") {
        return deleteMeshSelection();
    }

    if (action == "fill_selected_face") {
        return createFaceFromSelectedVertices();
    }

    if (action == "assign_face_material") {
        auto mid = parseJsonId(cmd.findKey("material_id"), "material_id");
        if (!mid) { recordStatus(core::Status(mid.error()), {}); return mid.error(); }
        return assignMaterialToSelectedFaces(scene::MaterialId{mid.value()});
    }

    // Stage 4: PBR Material actions.
    if (action == "select_material") {
        auto mid = parseJsonId(cmd.findKey("id"), "id");
        if (!mid) { recordStatus(core::Status(mid.error()), {}); return mid.error(); }
        return selectMaterial(scene::MaterialId{mid.value()});
    }

    if (action == "create_material") {
        scene::PbrMaterial mat{};
        if (const auto* nv = cmd.findKey("name"); nv != nullptr && nv->isString()) {
            mat.name = nv->asString();
        }
        if (const auto* bc = cmd.findKey("base_color"); bc != nullptr) {
            if (auto v = parseJsonVec3(bc, "base_color"); v.hasValue()) mat.base_color = v.value();
        }
        if (const auto* mv = cmd.findKey("metallic"); mv != nullptr) {
            if (auto f = parseJsonFloat(mv, "metallic"); f.hasValue()) mat.metallic = f.value();
        }
        if (const auto* rv = cmd.findKey("roughness"); rv != nullptr) {
            if (auto f = parseJsonFloat(rv, "roughness"); f.hasValue()) mat.roughness = f.value();
        }
        auto res = createMaterial(mat);
        return res.hasValue() ? core::Status::ok() : core::Status(res.error());
    }

    if (action == "duplicate_material") {
        auto mid = parseJsonId(cmd.findKey("id"), "id");
        if (!mid) { recordStatus(core::Status(mid.error()), {}); return mid.error(); }
        auto res = duplicateMaterial(scene::MaterialId{mid.value()});
        return res.hasValue() ? core::Status::ok() : core::Status(res.error());
    }

    if (action == "rename_material") {
        auto mid = parseJsonId(cmd.findKey("id"), "id");
        const auto* nv = cmd.findKey("name");
        if (!mid) { recordStatus(core::Status(mid.error()), {}); return mid.error(); }
        if (nv == nullptr || !nv->isString()) {
            auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Missing string 'name'");
            recordStatus(st, {});
            return st;
        }
        return renameMaterial(scene::MaterialId{mid.value()}, nv->asString());
    }

    if (action == "delete_material") {
        auto mid = parseJsonId(cmd.findKey("id"), "id");
        if (!mid) { recordStatus(core::Status(mid.error()), {}); return mid.error(); }
        return deleteMaterial(scene::MaterialId{mid.value()});
    }

    if (action == "update_material") {
        auto mid = parseJsonId(cmd.findKey("id"), "id");
        if (!mid) { recordStatus(core::Status(mid.error()), {}); return mid.error(); }
        const scene::MaterialId target_id{mid.value()};
        const auto* existing = session_.document().scene.findMaterial(target_id);
        if (existing == nullptr) {
            auto st = core::Status::fail(core::ErrorCode::NotFound, "Material not found");
            recordStatus(st, {});
            return st;
        }
        scene::PbrMaterial updated = *existing;
        if (const auto* nv = cmd.findKey("name"); nv != nullptr && nv->isString() && !nv->asString().empty()) {
            updated.name = nv->asString();
        }
        if (const auto* bc = cmd.findKey("base_color"); bc != nullptr) {
            auto v = parseJsonVec3(bc, "base_color");
            if (!v) { recordStatus(core::Status(v.error()), {}); return v.error(); }
            updated.base_color = v.value();
        }
        if (const auto* mv = cmd.findKey("metallic"); mv != nullptr) {
            auto f = parseJsonFloat(mv, "metallic");
            if (!f) { recordStatus(core::Status(f.error()), {}); return f.error(); }
            updated.metallic = f.value();
        }
        if (const auto* rv = cmd.findKey("roughness"); rv != nullptr) {
            auto f = parseJsonFloat(rv, "roughness");
            if (!f) { recordStatus(core::Status(f.error()), {}); return f.error(); }
            updated.roughness = f.value();
        }
        if (const auto* av = cmd.findKey("ambient_occlusion"); av != nullptr) {
            auto f = parseJsonFloat(av, "ambient_occlusion");
            if (!f) { recordStatus(core::Status(f.error()), {}); return f.error(); }
            updated.ambient_occlusion = f.value();
        }
        if (const auto* ec = cmd.findKey("emissive_color"); ec != nullptr) {
            auto v = parseJsonVec3(ec, "emissive_color");
            if (!v) { recordStatus(core::Status(v.error()), {}); return v.error(); }
            updated.emissive_color = v.value();
        }
        if (const auto* es = cmd.findKey("emissive_strength"); es != nullptr) {
            auto f = parseJsonFloat(es, "emissive_strength");
            if (!f) { recordStatus(core::Status(f.error()), {}); return f.error(); }
            updated.emissive_strength = f.value();
        }
        if (const auto* op = cmd.findKey("opacity"); op != nullptr) {
            auto f = parseJsonFloat(op, "opacity");
            if (!f) { recordStatus(core::Status(f.error()), {}); return f.error(); }
            updated.opacity = f.value();
        }
        bool coalesce = false;
        if (const auto* cv = cmd.findKey("coalesce"); cv != nullptr && cv->isBool()) {
            coalesce = cv->asBool();
        }
        return updateMaterial(target_id, updated, coalesce);
    }

    if (action == "assign_object_material") {
        auto oid = parseJsonId(cmd.findKey("object_id"), "object_id");
        auto mid = parseJsonId(cmd.findKey("material_id"), "material_id");
        if (!oid) { recordStatus(core::Status(oid.error()), {}); return oid.error(); }
        if (!mid) { recordStatus(core::Status(mid.error()), {}); return mid.error(); }
        std::optional<scene::MaterialId> opt_mat;
        if (mid.value() != 0) {
            opt_mat = scene::MaterialId{mid.value()};
        }
        return assignObjectMaterial(scene::ObjectId{oid.value()}, opt_mat);
    }

    // Stage 4: Workspace layout actions.
    if (action == "set_panel_visible") {
        const auto* pv = cmd.findKey("panel");
        const auto* vv = cmd.findKey("visible");
        if (pv == nullptr || !pv->isString() || vv == nullptr || !vv->isBool()) {
            auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Invalid set_panel_visible payload");
            recordStatus(st, {});
            return st;
        }
        const auto panel = parsePanelKeyFlexible(pv->asString());
        if (!panel.has_value()) {
            auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Unknown panel: " + pv->asString());
            recordStatus(st, {});
            return st;
        }
        setPanelVisible(panel.value(), vv->asBool());
        return core::Status::ok();
    }

    if (action == "apply_workspace_preset") {
        const auto* pv = cmd.findKey("preset");
        if (pv == nullptr || !pv->isString()) {
            auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Missing string 'preset'");
            recordStatus(st, {});
            return st;
        }
        return applyWorkspacePresetByName(pv->asString());
    }

    if (action == "set_dock_ratios") {
        const auto& cur = session_.workspace().state();
        float left_r = cur.left_dock_width_ratio;
        float right_r = cur.right_dock_width_ratio;
        float bottom_r = cur.bottom_dock_height_ratio;
        if (const auto* lv = cmd.findKey("left"); lv != nullptr) {
            auto lr = parseJsonFloat(lv, "left");
            if (!lr) { recordStatus(core::Status(lr.error()), {}); return lr.error(); }
            left_r = lr.value();
        }
        if (const auto* rv = cmd.findKey("right"); rv != nullptr) {
            auto rr = parseJsonFloat(rv, "right");
            if (!rr) { recordStatus(core::Status(rr.error()), {}); return rr.error(); }
            right_r = rr.value();
        }
        if (const auto* bv = cmd.findKey("bottom"); bv != nullptr) {
            auto br = parseJsonFloat(bv, "bottom");
            if (!br) { recordStatus(core::Status(br.error()), {}); return br.error(); }
            bottom_r = br.value();
        }
        setWorkspaceDockRatios(left_r, right_r, bottom_r);
        return core::Status::ok();
    }

    if (action == "set_panel_dock") {
        const auto* pv = cmd.findKey("panel");
        const auto* dv = cmd.findKey("dock");
        if (pv == nullptr || !pv->isString() || dv == nullptr || !dv->isString()) {
            auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Invalid set_panel_dock payload");
            recordStatus(st, {});
            return st;
        }
        const auto panel = parsePanelKeyFlexible(pv->asString());
        const auto dock = project::parseDockRegionSlug(dv->asString());
        if (!panel.has_value() || !dock.has_value()) {
            auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Unknown panel or dock region");
            recordStatus(st, {});
            return st;
        }
        std::uint32_t order = 0;
        if (const auto* ov = cmd.findKey("tab_order"); ov != nullptr && ov->isNumber()) {
            if (auto oid = parseJsonId(ov, "tab_order"); oid.hasValue()) {
                order = static_cast<std::uint32_t>(oid.value());
            }
        }
        setWorkspacePanelDock(panel.value(), dock.value(), order);
        return core::Status::ok();
    }

    if (action == "import_workspace_json") {
        const auto* jv = cmd.findKey("json_text");
        if (jv == nullptr || !jv->isString()) {
            auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Missing string 'json_text'");
            recordStatus(st, {});
            return st;
        }
        return importWorkspaceLayoutJson(jv->asString());
    }

    if (action == "set_text_input_focused") {
        if (const auto* fv = cmd.findKey("focused"); fv != nullptr && fv->isBool()) {
            setTextInputFocused(fv->asBool());
        }
        return core::Status::ok();
    }

    // Stage 1-3 Scene & Project actions.
    if (action == "add_preset") {
        const auto* preset_val = cmd.findKey("preset");
        if (preset_val == nullptr || !preset_val->isString()) {
            auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Missing string 'preset'");
            recordStatus(st, {});
            return st;
        }
        ScenePreset preset = ScenePreset::Cube;
        const std::string& p_str = preset_val->asString();
        if (p_str == "cube") preset = ScenePreset::Cube;
        else if (p_str == "pyramid") preset = ScenePreset::Pyramid;
        else if (p_str == "sphere") preset = ScenePreset::Sphere;
        else if (p_str == "plane") preset = ScenePreset::Plane;
        else if (p_str == "pivot") preset = ScenePreset::EmptyPivot;
        else if (p_str == "light") preset = ScenePreset::DirectionalLight;
        else if (p_str == "camera") preset = ScenePreset::Camera;
        else {
            auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Unknown preset: " + p_str);
            recordStatus(st, {});
            return st;
        }

        scene::ObjectId parent_id{};
        if (const auto* pv = cmd.findKey("parent_id"); pv != nullptr && pv->isNumber()) {
            auto pid = parseJsonId(pv, "parent_id");
            if (!pid) { recordStatus(core::Status(pid.error()), {}); return pid.error(); }
            parent_id = scene::ObjectId{pid.value()};
        }
        auto res = addPresetObject(preset, parent_id);
        return res.hasValue() ? core::Status::ok() : core::Status(res.error());
    }

    if (action == "duplicate_selected") {
        auto res = duplicateSelectedObject();
        return res.hasValue() ? core::Status::ok() : core::Status(res.error());
    }

    if (action == "delete_selected") {
        return deleteSelectedObject(scene::RemovalMode::Recursive);
    }

    if (action == "rename_object") {
        auto id_res = parseJsonId(cmd.findKey("id"), "id");
        const auto* nv = cmd.findKey("name");
        if (!id_res) { recordStatus(core::Status(id_res.error()), {}); return id_res.error(); }
        if (nv == nullptr || !nv->isString()) {
            auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Missing string 'name'");
            recordStatus(st, {});
            return st;
        }
        return renameObject(scene::ObjectId{id_res.value()}, nv->asString());
    }

    if (action == "set_visibility") {
        auto id_res = parseJsonId(cmd.findKey("id"), "id");
        const auto* vv = cmd.findKey("visible");
        if (!id_res) { recordStatus(core::Status(id_res.error()), {}); return id_res.error(); }
        if (vv == nullptr || !vv->isBool()) {
            auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Missing boolean 'visible'");
            recordStatus(st, {});
            return st;
        }
        return setObjectVisibility(scene::ObjectId{id_res.value()}, vv->asBool());
    }

    if (action == "reparent_object") {
        auto cid = parseJsonId(cmd.findKey("child_id"), "child_id");
        auto pid = parseJsonId(cmd.findKey("parent_id"), "parent_id");
        if (!cid) { recordStatus(core::Status(cid.error()), {}); return cid.error(); }
        if (!pid) { recordStatus(core::Status(pid.error()), {}); return pid.error(); }
        return reparentObject(scene::ObjectId{cid.value()}, scene::ObjectId{pid.value()});
    }

    if (action == "set_transform") {
        auto id_res = parseJsonId(cmd.findKey("id"), "id");
        auto pos_res = parseJsonVec3(cmd.findKey("position"), "position");
        auto rot_res = parseJsonVec3(cmd.findKey("rotation_deg"), "rotation_deg");
        auto scl_res = parseJsonVec3(cmd.findKey("scale"), "scale");
        if (!id_res) { recordStatus(core::Status(id_res.error()), {}); return id_res.error(); }
        if (!pos_res) { recordStatus(core::Status(pos_res.error()), {}); return pos_res.error(); }
        if (!rot_res) { recordStatus(core::Status(rot_res.error()), {}); return rot_res.error(); }
        if (!scl_res) { recordStatus(core::Status(scl_res.error()), {}); return scl_res.error(); }

        bool coalesce = false;
        if (const auto* cv = cmd.findKey("coalesce"); cv != nullptr && cv->isBool()) {
            coalesce = cv->asBool();
        }
        return setObjectTransformEulerDegrees(
            scene::ObjectId{id_res.value()},
            pos_res.value(),
            rot_res.value(),
            scl_res.value(),
            coalesce);
    }

    if (action == "reset_transform") {
        auto id_res = parseJsonId(cmd.findKey("id"), "id");
        if (!id_res) { recordStatus(core::Status(id_res.error()), {}); return id_res.error(); }
        return resetObjectTransform(scene::ObjectId{id_res.value()});
    }

    if (action == "set_camera") {
        auto id_res = parseJsonId(cmd.findKey("id"), "id");
        auto fov_res = parseJsonFloat(cmd.findKey("fov_deg"), "fov_deg");
        auto oh_res = parseJsonFloat(cmd.findKey("ortho_height"), "ortho_height");
        auto np_res = parseJsonFloat(cmd.findKey("near_plane"), "near_plane");
        auto fp_res = parseJsonFloat(cmd.findKey("far_plane"), "far_plane");
        if (!id_res) { recordStatus(core::Status(id_res.error()), {}); return id_res.error(); }
        if (!fov_res) { recordStatus(core::Status(fov_res.error()), {}); return fov_res.error(); }
        if (!oh_res) { recordStatus(core::Status(oh_res.error()), {}); return oh_res.error(); }
        if (!np_res) { recordStatus(core::Status(np_res.error()), {}); return np_res.error(); }
        if (!fp_res) { recordStatus(core::Status(fp_res.error()), {}); return fp_res.error(); }

        scene::CameraComponent cam{};
        if (const auto* pv = cmd.findKey("projection"); pv != nullptr && pv->isString()) {
            cam.projection = (pv->asString() == "orthographic")
                                 ? scene::ProjectionType::Orthographic
                                 : scene::ProjectionType::Perspective;
        }
        cam.fov_y_radians = math::degToRad(fov_res.value());
        cam.orthographic_height = oh_res.value();
        cam.near_plane = np_res.value();
        cam.far_plane = fp_res.value();
        return setCameraComponent(scene::ObjectId{id_res.value()}, cam);
    }

    if (action == "set_active_camera") {
        auto id_res = parseJsonId(cmd.findKey("id"), "id");
        if (!id_res) { recordStatus(core::Status(id_res.error()), {}); return id_res.error(); }
        return setActiveCamera(scene::ObjectId{id_res.value()});
    }

    if (action == "set_light") {
        auto id_res = parseJsonId(cmd.findKey("id"), "id");
        auto col_res = parseJsonVec3(cmd.findKey("color"), "color");
        auto int_res = parseJsonFloat(cmd.findKey("intensity"), "intensity");
        if (!id_res) { recordStatus(core::Status(id_res.error()), {}); return id_res.error(); }
        if (!col_res) { recordStatus(core::Status(col_res.error()), {}); return col_res.error(); }
        if (!int_res) { recordStatus(core::Status(int_res.error()), {}); return int_res.error(); }

        bool coalesce = false;
        if (const auto* cv = cmd.findKey("coalesce"); cv != nullptr && cv->isBool()) {
            coalesce = cv->asBool();
        }
        scene::DirectionalLightComponent lgt{
            .color = col_res.value(),
            .intensity = int_res.value(),
        };
        return setDirectionalLightComponent(scene::ObjectId{id_res.value()}, lgt, coalesce);
    }

    if (action == "set_metadata") {
        const auto* pn = cmd.findKey("project_name");
        const auto* au = cmd.findKey("author");
        const auto* sn = cmd.findKey("scene_name");
        if (pn == nullptr || !pn->isString() || au == nullptr || !au->isString() || sn == nullptr || !sn->isString()) {
            auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Metadata fields must be strings");
            recordStatus(st, {});
            return st;
        }
        return updateProjectMetadata(pn->asString(), au->asString(), sn->asString());
    }

    if (action == "frame_selection") {
        return frameSelectionOrScene();
    }

    if (action == "set_rig_playing") {
        const auto* pv = cmd.findKey("playing");
        if (pv == nullptr || !pv->isBool()) {
            auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Missing boolean 'playing'");
            recordStatus(st, {});
            return st;
        }
        setRigAnimationPlaying(pv->asBool());
        return core::Status::ok();
    }

    if (action == "undo") {
        return undo();
    }

    if (action == "redo") {
        return redo();
    }

    if (action == "new_starter_project") {
        return newStarterProject();
    }

    if (action == "create_new_project") {
        ProjectTemplate tmpl = ProjectTemplate::StarterShowcase;
        if (const auto* tv = cmd.findKey("template"); tv != nullptr && tv->isString()) {
            const auto& ts = tv->asString();
            if (ts == "starter" || ts == "showcase") tmpl = ProjectTemplate::StarterShowcase;
            else if (ts == "minimal_cube" || ts == "cube") tmpl = ProjectTemplate::MinimalCube;
            else if (ts == "empty" || ts == "empty_stage") tmpl = ProjectTemplate::EmptyStage;
            else if (ts == "shader_ball" || ts == "shading") tmpl = ProjectTemplate::ShaderBall;
            else {
                auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Unknown project template: " + ts);
                recordStatus(st, {});
                return st;
            }
        }
        std::string pname;
        if (const auto* nv = cmd.findKey("project_name"); nv != nullptr && nv->isString()) {
            pname = nv->asString();
        }
        std::string pauthor;
        if (const auto* av = cmd.findKey("author"); av != nullptr && av->isString()) {
            pauthor = av->asString();
        }
        return createNewProject(tmpl, std::move(pname), std::move(pauthor));
    }

    if (action == "load_inline_json") {
        const auto* jv = cmd.findKey("json_text");
        if (jv == nullptr || !jv->isString()) {
            auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Missing string 'json_text'");
            recordStatus(st, {});
            return st;
        }
        return importInlineProjectJson(jv->asString());
    }

    auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Unknown editor action: " + action);
    recordStatus(st, {});
    return st;
}

} // namespace finaleffect::app
