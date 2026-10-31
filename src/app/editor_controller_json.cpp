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

    const auto cam_st = session_.activeCameraState();
    const math::Vec3 cam_right = cam_st.camera_world.transformDirection({1.0f, 0.0f, 0.0f}).normalized();
    const math::Vec3 cam_up = cam_st.camera_world.transformDirection({0.0f, 1.0f, 0.0f}).normalized();
    const math::Vec3 cam_fwd = cam_st.camera_world.transformDirection({0.0f, 0.0f, -1.0f}).normalized();
    const math::Vec3 cam_eye = cam_st.camera_world.transformPoint(math::Vec3::zero());

    JsonValue vp{JsonObject{}};
    vp.setKey("backend_name", caps.backend_name);
    vp.setKey("device_name", caps.device_name);
    vp.setKey("rig_playing", session_.isRigAnimationEnabled());
    vp.setKey("camera_view_active", session_.isCameraViewActive());
    vp.setKey("editor_helpers_visible", session_.areEditorHelpersVisible());
    vp.setKey("editor_grid_visible", session_.isEditorGridVisible());
    vp.setKey("projection",
              (cam_st.camera.projection == scene::ProjectionType::Orthographic)
                  ? "orthographic"
                  : "perspective");
    vp.setKey("editor_projection",
              (session_.viewportProjection() == scene::ProjectionType::Orthographic)
                  ? "orthographic"
                  : "perspective");
    vp.setKey("view_label", std::string(session_.viewportViewLabel()));
    vp.setKey("view_transition_active", session_.isViewTransitionActive());
    vp.setKey("active_camera_id", scene.activeCamera().value);
    vp.setKey("selected_object_id", session_.selectedObject().value);
    {
        JsonArray sel_ids;
        sel_ids.reserve(session_.selectedObjects().size());
        for (const auto sid : session_.selectedObjects()) {
            if (sid.isValid()) sel_ids.emplace_back(sid.value);
        }
        vp.setKey("selected_object_ids", std::move(sel_ids));
    }
    {
        const auto& vdisp = session_.viewportDisplay();
        std::string_view sm_slug = "shaded_textured";
        if (vdisp.shading_mode == rendering::ViewportShadingMode::Wireframe) sm_slug = "wireframe";
        else if (vdisp.shading_mode == rendering::ViewportShadingMode::SmoothShaded) sm_slug = "shaded";
        vp.setKey("shading_mode", std::string(sm_slug));
        vp.setKey("wireframe_on_shaded", vdisp.wireframe_on_shaded);
        vp.setKey("xray_mode", vdisp.xray_mode);
        vp.setKey("use_all_lights", vdisp.use_all_lights);
        vp.setKey("default_material", vdisp.default_material);
        vp.setKey("mesh_display_smoothness", static_cast<std::uint64_t>(vdisp.mesh_display_smoothness));
        vp.setKey("isolate_select", vdisp.isolate_select);
    }
    vp.setKey("camera_position", vec3ToJson(cam_eye));
    vp.setKey("camera_target", vec3ToJson(session_.cameraController().target()));
    vp.setKey("camera_distance", static_cast<double>(session_.cameraController().distance()));
    vp.setKey("camera_yaw_deg", static_cast<double>(math::radToDeg(session_.cameraController().yawRadians())));
    vp.setKey("camera_pitch_deg", static_cast<double>(math::radToDeg(session_.cameraController().pitchRadians())));
    vp.setKey("camera_right", vec3ToJson(cam_right));
    vp.setKey("camera_up", vec3ToJson(cam_up));
    vp.setKey("camera_forward", vec3ToJson(cam_fwd));
    root.setKey("viewport", std::move(vp));

    // Stage 4 / 5.3 / Maya Stage 1: Editor mode, Active Tool, Snapping, Selection Masks & Gizmo state.
    JsonValue ed{JsonObject{}};
    ed.setKey("mode", std::string(editModeToString(session_.editorMode())));
    ed.setKey("active_tool", std::string(mayaActiveToolSlug(session_.activeTool())));
    ed.setKey("last_tool", std::string(mayaActiveToolSlug(session_.lastTool())));
    {
        const auto& sn = session_.snapping();
        JsonValue sn_j{JsonObject{}};
        sn_j.setKey("grid", sn.snap_grid);
        sn_j.setKey("grid_step", static_cast<double>(sn.grid_step));
        sn_j.setKey("point", sn.snap_point);
        sn_j.setKey("curve", sn.snap_curve);
        sn_j.setKey("surface", sn.snap_surface);
        ed.setKey("snapping", std::move(sn_j));
    }
    {
        const auto& sm = session_.selectionMask();
        JsonValue sm_j{JsonObject{}};
        sm_j.setKey("surfaces", sm.meshes);
        sm_j.setKey("lights", sm.lights);
        sm_j.setKey("cameras", sm.cameras);
        sm_j.setKey("helpers", sm.transforms);
        ed.setKey("selection_mask", std::move(sm_j));
    }
    ed.setKey("gizmo_mode", std::string(gizmoModeName(session_.gizmo().mode())));
    ed.setKey("gizmo_space", std::string(gizmoSpaceName(session_.gizmo().space())));
    ed.setKey("gizmo_hovered_handle", std::string(gizmoHandleName(session_.hoveredGizmoHandle())));
    ed.setKey("gizmo_active_handle", std::string(gizmoHandleName(session_.gizmo().activeHandle())));
    ed.setKey("gizmo_dragging", session_.isGizmoDragging());
    if (const auto gframe = session_.computeActiveGizmoFrame(); gframe.has_value()) {
        JsonValue gh{JsonObject{}};
        const bool is_rot =
            (session_.gizmo().mode() == GizmoMode::Rotate && session_.editorMode() == scene::EditMode::Object);
        const float shaft_dist = gframe->scale * 0.72f;
        const float plane_dist = gframe->scale * 0.39f;
        const float ring_dist = gframe->scale * 0.92f;
        const math::Vec3 to_cam = (cam_eye - gframe->origin).normalized();

        auto projPt = [&](const math::Vec3& wp) -> JsonValue {
            if (const auto sp = session_.projectWorldPointToScreen(wp); sp.has_value()) {
                return JsonArray{
                    JsonValue(static_cast<double>(sp->x)),
                    JsonValue(static_cast<double>(sp->y)),
                };
            }
            return JsonValue{};
        };

        auto bestRingMidpoint = [&](std::size_t axis_i) -> math::Vec3 {
            const math::Vec3& u = gframe->axes[(axis_i + 1) % 3];
            const math::Vec3& v = gframe->axes[(axis_i + 2) % 3];
            constexpr float kInvSqrt2 = 0.70710678f;
            const std::array<math::Vec3, 4> dirs{{
                (u + v) * kInvSqrt2,
                (u - v) * kInvSqrt2,
                (-u + v) * kInvSqrt2,
                (-u - v) * kInvSqrt2,
            }};
            math::Vec3 best = dirs[0];
            float best_dot = best.dot(to_cam);
            for (std::size_t k = 1; k < dirs.size(); ++k) {
                const float d = dirs[k].dot(to_cam);
                if (d > best_dot) {
                    best_dot = d;
                    best = dirs[k];
                }
            }
            return gframe->origin + best * ring_dist;
        };

        gh.setKey("world_scale", static_cast<double>(gframe->scale));
        gh.setKey("origin", projPt(gframe->origin));
        gh.setKey("center", projPt(gframe->origin));
        if (is_rot) {
            const math::Vec3 rx_pt = bestRingMidpoint(0);
            const math::Vec3 ry_pt = bestRingMidpoint(1);
            const math::Vec3 rz_pt = bestRingMidpoint(2);
            gh.setKey("x", projPt(rx_pt));
            gh.setKey("y", projPt(ry_pt));
            gh.setKey("z", projPt(rz_pt));
            gh.setKey("rx", projPt(rx_pt));
            gh.setKey("ry", projPt(ry_pt));
            gh.setKey("rz", projPt(rz_pt));
        } else {
            gh.setKey("x", projPt(gframe->origin + gframe->axes[0] * shaft_dist));
            gh.setKey("y", projPt(gframe->origin + gframe->axes[1] * shaft_dist));
            gh.setKey("z", projPt(gframe->origin + gframe->axes[2] * shaft_dist));
            gh.setKey("xy", projPt(gframe->origin + (gframe->axes[0] + gframe->axes[1]) * plane_dist));
            gh.setKey("xz", projPt(gframe->origin + (gframe->axes[0] + gframe->axes[2]) * plane_dist));
            gh.setKey("yz", projPt(gframe->origin + (gframe->axes[1] + gframe->axes[2]) * plane_dist));
        }
        if (const auto sp_o = session_.projectWorldPointToScreen(gframe->origin); sp_o.has_value()) {
            float max_px = 0.0f;
            for (std::size_t ai = 0; ai < 3; ++ai) {
                if (const auto sp_a = session_.projectWorldPointToScreen(
                        gframe->origin + gframe->axes[ai] * gframe->scale);
                    sp_a.has_value()) {
                    const float dx = sp_a->x - sp_o->x;
                    const float dy = sp_a->y - sp_o->y;
                    max_px = std::max(max_px, std::sqrt(dx * dx + dy * dy));
                }
            }
            gh.setKey("screen_radius_px", static_cast<double>(max_px));
        }
        ed.setKey("gizmo_handles_screen", std::move(gh));
    }

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
        item.setKey("locked", node->locked);
        item.setKey("locked_channels_mask", static_cast<std::uint64_t>(node->locked_channels_mask));
        if (const auto* dl = scene.findObjectDisplayLayer(node->id); dl != nullptr) {
            item.setKey("display_layer_id", static_cast<std::uint64_t>(dl->id));
            item.setKey("display_layer_type", std::string(scene::displayLayerTypeSlug(dl->display_type)));
            item.setKey("display_layer_visible", dl->visible);
        } else {
            item.setKey("display_layer_id", std::uint64_t{0});
            item.setKey("display_layer_type", std::string("normal"));
            item.setKey("display_layer_visible", true);
        }
        item.setKey("parent_id", node->parent_id.value);
        item.setKey("depth", static_cast<std::uint64_t>(depth));
        item.setKey("has_mesh", node->mesh_id.has_value());
        item.setKey("has_camera", node->camera.has_value());
        item.setKey("has_light", node->hasAnyLight());
        item.setKey(
            "light_type",
            node->lightType().has_value()
                ? std::string(scene::lightTypeSlug(node->lightType().value()))
                : std::string{});
        item.setKey("material_id", node->material_id.has_value() ? node->material_id->value : std::uint64_t{0});
        item.setKey("is_active_camera", node->id == scene.activeCamera());
        if (const auto wm = scene.computeWorldMatrix(node->id); wm.hasValue()) {
            const math::Vec3 wpos = wm.value().transformPoint(math::Vec3::zero());
            item.setKey("world_position", vec3ToJson(wpos));
            const math::Vec3 pick_anchor =
                node->camera.has_value()
                    ? wm.value().transformPoint(math::Vec3{0.0f, 0.04f, 0.18f})
                    : wpos;
            if (const auto sp = session_.projectWorldPointToScreen(pick_anchor); sp.has_value()) {
                item.setKey("pick_screen_pos", JsonArray{
                    JsonValue(static_cast<double>(sp->x)),
                    JsonValue(static_cast<double>(sp->y)),
                });
            }
        }
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
            insp.setKey("locked", sel_obj->locked);
            insp.setKey("locked_channels_mask", static_cast<std::uint64_t>(sel_obj->locked_channels_mask));
            insp.setKey("pivot_offset", vec3ToJson(sel_obj->pivot_offset));
            {
                JsonArray cattrs;
                cattrs.reserve(sel_obj->custom_attributes.size());
                for (const auto& ca : sel_obj->custom_attributes) {
                    JsonValue cj{JsonObject{}};
                    cj.setKey("name", ca.name);
                    cj.setKey("value", ca.value);
                    cj.setKey("min_value", ca.min_value);
                    cj.setKey("max_value", ca.max_value);
                    cj.setKey("has_limits", ca.has_limits);
                    cj.setKey("keyable", ca.keyable);
                    cj.setKey("locked", ca.locked);
                    cattrs.push_back(std::move(cj));
                }
                insp.setKey("custom_attributes", std::move(cattrs));
            }
            {
                JsonArray kframes;
                bool keyed_now = false;
                const float cur_f = scene.timeline().current_frame;
                if (const auto* tr = scene.findAnimationTrack(sel_obj->id); tr != nullptr) {
                    for (const auto& k : tr->keyframes) {
                        kframes.emplace_back(static_cast<double>(k.frame));
                        if (std::fabs(k.frame - cur_f) <= 1.0e-3f) keyed_now = true;
                    }
                }
                insp.setKey("keyframe_frames", std::move(kframes));
                insp.setKey("has_keyframe_at_current", keyed_now);
            }
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

            if (sel_obj->hasAnyLight()) {
                JsonValue l_json{JsonObject{}};
                l_json.setKey(
                    "type",
                    sel_obj->lightType().has_value()
                        ? std::string(scene::lightTypeSlug(sel_obj->lightType().value()))
                        : std::string("directional"));

                auto encodeShadow = [](const scene::LightShadowSettings& s) {
                    JsonValue sj{JsonObject{}};
                    sj.setKey("enabled", s.enabled);
                    sj.setKey("strength", static_cast<double>(s.strength));
                    sj.setKey("quality", std::string(scene::shadowQualitySlug(s.quality)));
                    sj.setKey("filter", std::string(scene::shadowFilterModeSlug(s.filter)));
                    sj.setKey("bias", static_cast<double>(s.bias));
                    sj.setKey("normal_bias", static_cast<double>(s.normal_bias));
                    sj.setKey("softness", static_cast<double>(s.softness));
                    return sj;
                };
                auto encodeLinking = [](const scene::LightLinkingSettings& lnk) {
                    JsonValue lj{JsonObject{}};
                    lj.setKey("mode", std::string(scene::lightLinkModeSlug(lnk.mode)));
                    lj.setKey("include_descendants", lnk.include_descendants);
                    JsonArray ids;
                    ids.reserve(lnk.linked_objects.size());
                    for (const auto tid : lnk.linked_objects) {
                        if (tid.isValid()) {
                            ids.emplace_back(tid.value);
                        }
                    }
                    lj.setKey("object_ids", std::move(ids));
                    return lj;
                };

                if (sel_obj->directional_light.has_value()) {
                    const auto& lgt = sel_obj->directional_light.value();
                    l_json.setKey("color", vec3ToJson(lgt.color));
                    l_json.setKey("intensity", static_cast<double>(lgt.intensity));
                    l_json.setKey("enabled", lgt.enabled);
                    l_json.setKey("shadow", encodeShadow(lgt.shadow));
                    l_json.setKey("linking", encodeLinking(lgt.linking));
                } else if (sel_obj->point_light.has_value()) {
                    const auto& lgt = sel_obj->point_light.value();
                    l_json.setKey("color", vec3ToJson(lgt.color));
                    l_json.setKey("intensity", static_cast<double>(lgt.intensity));
                    l_json.setKey("enabled", lgt.enabled);
                    l_json.setKey("range", static_cast<double>(lgt.range));
                    l_json.setKey("attenuation", static_cast<double>(lgt.attenuation));
                    l_json.setKey("falloff", std::string(scene::lightFalloffModeSlug(lgt.falloff)));
                    l_json.setKey("source_radius", static_cast<double>(lgt.source_radius));
                    l_json.setKey("shadow", encodeShadow(lgt.shadow));
                    l_json.setKey("linking", encodeLinking(lgt.linking));
                } else if (sel_obj->spot_light.has_value()) {
                    const auto& lgt = sel_obj->spot_light.value();
                    l_json.setKey("color", vec3ToJson(lgt.color));
                    l_json.setKey("intensity", static_cast<double>(lgt.intensity));
                    l_json.setKey("enabled", lgt.enabled);
                    l_json.setKey("range", static_cast<double>(lgt.range));
                    l_json.setKey("attenuation", static_cast<double>(lgt.attenuation));
                    l_json.setKey("falloff", std::string(scene::lightFalloffModeSlug(lgt.falloff)));
                    l_json.setKey("inner_cone_deg", static_cast<double>(math::radToDeg(lgt.inner_cone_radians)));
                    l_json.setKey("outer_cone_deg", static_cast<double>(math::radToDeg(lgt.outer_cone_radians)));
                    l_json.setKey("cone_falloff", static_cast<double>(lgt.cone_falloff));
                    l_json.setKey("shadow", encodeShadow(lgt.shadow));
                    l_json.setKey("linking", encodeLinking(lgt.linking));
                } else if (sel_obj->area_light.has_value()) {
                    const auto& lgt = sel_obj->area_light.value();
                    l_json.setKey("color", vec3ToJson(lgt.color));
                    l_json.setKey("intensity", static_cast<double>(lgt.intensity));
                    l_json.setKey("enabled", lgt.enabled);
                    l_json.setKey("shape", std::string(scene::areaLightShapeSlug(lgt.shape)));
                    l_json.setKey("width", static_cast<double>(lgt.width));
                    l_json.setKey("height", static_cast<double>(lgt.height));
                    l_json.setKey("range", static_cast<double>(lgt.range));
                    l_json.setKey("two_sided", lgt.two_sided);
                    l_json.setKey("sample_quality", static_cast<std::uint64_t>(lgt.sample_quality));
                    l_json.setKey("shadow", encodeShadow(lgt.shadow));
                    l_json.setKey("linking", encodeLinking(lgt.linking));
                }
                insp.setKey("light", std::move(l_json));
            }

            root.setKey("inspector", std::move(insp));
        } else {
            root.setKey("inspector", JsonValue{});
        }
    } else {
        root.setKey("inspector", JsonValue{});
    }

    // Stage 5.4: Scene Environment / HDRI & IBL settings + real-time Lighting Diagnostics.
    {
        const auto& env = scene.environment();
        JsonValue env_json{JsonObject{}};
        env_json.setKey("enabled", env.enabled);
        env_json.setKey("intensity", static_cast<double>(env.intensity));
        env_json.setKey("rotation_deg", static_cast<double>(math::radToDeg(env.rotation_radians)));
        env_json.setKey("diffuse_ibl_scale", static_cast<double>(env.diffuse_ibl_scale));
        env_json.setKey("specular_ibl_scale", static_cast<double>(env.specular_ibl_scale));
        env_json.setKey("has_hdri", env.hasHdri());
        env_json.setKey("source_path", env.source_path);
        env_json.setKey(
            "hdri_name",
            (env.hasHdri() && env.hdri_asset != nullptr) ? env.hdri_asset->name : std::string{});
        if (env.hasHdri() && env.hdri_asset != nullptr) {
            env_json.setKey("hdri_width", static_cast<std::uint64_t>(env.hdri_asset->source_width));
            env_json.setKey("hdri_height", static_cast<std::uint64_t>(env.hdri_asset->source_height));
            env_json.setKey("gpu_bytes", static_cast<std::uint64_t>(env.hdri_asset->prefiltered_cubemap.gpuByteSize()));
        } else {
            env_json.setKey("hdri_width", std::uint64_t{0});
            env_json.setKey("hdri_height", std::uint64_t{0});
            env_json.setKey("gpu_bytes", std::uint64_t{0});
        }
        root.setKey("environment", std::move(env_json));
    }

    {
        const auto& diag = session_.renderer().lastLightingDiagnostics();
        std::uint64_t dir_cnt = 0;
        std::uint64_t pt_cnt = 0;
        std::uint64_t spt_cnt = 0;
        std::uint64_t area_cnt = 0;
        for (const auto& obj : scene.objects()) {
            if (obj.directional_light.has_value()) ++dir_cnt;
            else if (obj.point_light.has_value()) ++pt_cnt;
            else if (obj.spot_light.has_value()) ++spt_cnt;
            else if (obj.area_light.has_value()) ++area_cnt;
        }
        JsonValue dj{JsonObject{}};
        dj.setKey("total_enabled_lights", static_cast<std::uint64_t>(diag.total_enabled_lights));
        dj.setKey("active_lights", static_cast<std::uint64_t>(diag.active_realtime_lights));
        dj.setKey("clamped_lights", static_cast<std::uint64_t>(diag.overflow_lights_folded));
        dj.setKey("directional_count", dir_cnt);
        dj.setKey("point_count", pt_cnt);
        dj.setKey("spot_count", spt_cnt);
        dj.setKey("area_count", area_cnt);
        dj.setKey("requested_shadow_lights", static_cast<std::uint64_t>(diag.requested_shadow_lights));
        dj.setKey("active_shadow_lights", static_cast<std::uint64_t>(diag.active_shadow_lights));
        dj.setKey("clamped_shadow_lights", static_cast<std::uint64_t>(diag.shadow_limit_clamped_lights));
        dj.setKey("shadow_tiles_rendered", static_cast<std::uint64_t>(diag.shadow_tiles_rendered_last_frame));
        dj.setKey("shadow_tiles_cached", static_cast<std::uint64_t>(diag.shadow_tiles_cached_last_frame));
        dj.setKey("shadow_atlas_dimension", static_cast<std::uint64_t>(diag.shadow_atlas_dimension));
        dj.setKey("shadow_gpu_bytes", static_cast<std::uint64_t>(diag.shadow_gpu_bytes));
        dj.setKey("ibl_gpu_bytes", static_cast<std::uint64_t>(diag.environment_gpu_bytes));
        dj.setKey("last_frame_render_ms", diag.last_frame_render_ms);
        dj.setKey("warning_message", diag.warning_message);
        root.setKey("lighting_diagnostics", std::move(dj));
    }

    // Stage 1 Maya: Display Layers & Animation Timeline state.
    {
        JsonArray dl_arr;
        dl_arr.reserve(scene.displayLayers().size());
        for (const auto& dl : scene.displayLayers()) {
            JsonValue dj{JsonObject{}};
            dj.setKey("id", static_cast<std::uint64_t>(dl.id));
            dj.setKey("name", dl.name);
            dj.setKey("visible", dl.visible);
            dj.setKey("playback_visible", dl.playback_visible);
            dj.setKey("display_type", std::string(scene::displayLayerTypeSlug(dl.display_type)));
            dj.setKey("color", vec3ToJson(dl.color));
            JsonArray mids;
            mids.reserve(dl.members.size());
            for (const auto mid : dl.members) {
                if (mid.isValid()) mids.emplace_back(mid.value);
            }
            dj.setKey("member_ids", std::move(mids));
            dl_arr.push_back(std::move(dj));
        }
        root.setKey("display_layers", std::move(dl_arr));
    }

    {
        const auto& tl = scene.timeline();
        JsonValue tj{JsonObject{}};
        tj.setKey("current_frame", static_cast<double>(tl.current_frame));
        tj.setKey("start_frame", static_cast<double>(tl.start_frame));
        tj.setKey("end_frame", static_cast<double>(tl.end_frame));
        tj.setKey("playback_start", static_cast<double>(tl.playback_start));
        tj.setKey("playback_end", static_cast<double>(tl.playback_end));
        tj.setKey("fps", static_cast<double>(tl.fps));
        tj.setKey("playback_mode", std::string(scene::timelineLoopModeSlug(tl.loop_mode)));
        tj.setKey("auto_key", tl.auto_key);
        tj.setKey("playing", session_.isTimelinePlaying());

        JsonArray sel_keys;
        if (session_.selectedObject().isValid()) {
            if (const auto* tr = scene.findAnimationTrack(session_.selectedObject()); tr != nullptr) {
                for (const auto& k : tr->keyframes) {
                    sel_keys.emplace_back(static_cast<double>(k.frame));
                }
            }
        }
        tj.setKey("selected_key_frames", std::move(sel_keys));

        std::vector<float> union_keys;
        for (const auto& tr : scene.animationTracks()) {
            for (const auto& k : tr.keyframes) {
                bool dup = false;
                for (float ex : union_keys) {
                    if (std::fabs(ex - k.frame) <= 1.0e-3f) { dup = true; break; }
                }
                if (!dup) union_keys.push_back(k.frame);
            }
        }
        std::sort(union_keys.begin(), union_keys.end());
        JsonArray all_keys;
        all_keys.reserve(union_keys.size());
        for (float kf : union_keys) all_keys.emplace_back(static_cast<double>(kf));
        tj.setKey("all_key_frames", std::move(all_keys));

        root.setKey("timeline", std::move(tj));
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
        else if (p_str == "cylinder") preset = ScenePreset::Cylinder;
        else if (p_str == "cone") preset = ScenePreset::Cone;
        else if (p_str == "torus") preset = ScenePreset::Torus;
        else if (p_str == "plane") preset = ScenePreset::Plane;
        else if (p_str == "pivot" || p_str == "locator") preset = ScenePreset::EmptyPivot;
        else if (p_str == "light" || p_str == "directional_light") preset = ScenePreset::DirectionalLight;
        else if (p_str == "point_light") preset = ScenePreset::PointLight;
        else if (p_str == "spot_light") preset = ScenePreset::SpotLight;
        else if (p_str == "area_light") preset = ScenePreset::AreaLight;
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
        if (!id_res) { recordStatus(core::Status(id_res.error()), {}); return id_res.error(); }
        const scene::ObjectId obj_id{id_res.value()};
        const auto* obj = session_.document().scene.findObject(obj_id);
        if (obj == nullptr) {
            auto st = core::Status::fail(core::ErrorCode::NotFound, "Light object not found");
            recordStatus(st, {});
            return st;
        }

        bool coalesce = false;
        if (const auto* cv = cmd.findKey("coalesce"); cv != nullptr && cv->isBool()) {
            coalesce = cv->asBool();
        }

        auto applyShadowPatch = [&](scene::LightShadowSettings& s) -> core::Status {
            const auto* sv = cmd.findKey("shadow");
            if (sv == nullptr || sv->isNull()) return core::Status::ok();
            if (!sv->isObject()) {
                return core::Status::fail(core::ErrorCode::InvalidArgument, "'shadow' must be an object");
            }
            if (const auto* en = sv->findKey("enabled"); en != nullptr && en->isBool()) s.enabled = en->asBool();
            if (const auto* st = sv->findKey("strength"); st != nullptr) {
                auto v = parseJsonFloat(st, "shadow.strength");
                if (!v) return v.error();
                s.strength = v.value();
            }
            if (const auto* ql = sv->findKey("quality"); ql != nullptr && ql->isString()) {
                const auto parsed_q = scene::parseShadowQualitySlug(ql->asString());
                if (!parsed_q.has_value()) {
                    return core::Status::fail(core::ErrorCode::InvalidArgument, "Unknown shadow quality");
                }
                s.quality = parsed_q.value();
            }
            if (const auto* fl = sv->findKey("filter"); fl != nullptr && fl->isString()) {
                const auto parsed_f = scene::parseShadowFilterModeSlug(fl->asString());
                if (!parsed_f.has_value()) {
                    return core::Status::fail(core::ErrorCode::InvalidArgument, "Unknown shadow filter");
                }
                s.filter = parsed_f.value();
            }
            if (const auto* bi = sv->findKey("bias"); bi != nullptr) {
                auto v = parseJsonFloat(bi, "shadow.bias");
                if (!v) return v.error();
                s.bias = v.value();
            }
            if (const auto* nb = sv->findKey("normal_bias"); nb != nullptr) {
                auto v = parseJsonFloat(nb, "shadow.normal_bias");
                if (!v) return v.error();
                s.normal_bias = v.value();
            }
            if (const auto* sf = sv->findKey("softness"); sf != nullptr) {
                auto v = parseJsonFloat(sf, "shadow.softness");
                if (!v) return v.error();
                s.softness = v.value();
            }
            return s.validate();
        };

        // Determine target light type (defaulting to current object's light type, or Directional).
        scene::LightType target_type = obj->lightType().value_or(scene::LightType::Directional);
        if (const auto* tv = cmd.findKey("type"); tv != nullptr && tv->isString()) {
            if (const auto pt = scene::parseLightTypeSlug(tv->asString()); pt.has_value()) {
                target_type = pt.value();
            }
        }

        if (target_type == scene::LightType::Point) {
            scene::PointLightComponent lgt = obj->point_light.value_or(scene::PointLightComponent{});
            if (const auto* c = cmd.findKey("color"); c != nullptr) {
                auto v = parseJsonVec3(c, "color");
                if (!v) { recordStatus(core::Status(v.error()), {}); return v.error(); }
                lgt.color = v.value();
            }
            if (const auto* iv = cmd.findKey("intensity"); iv != nullptr) {
                auto v = parseJsonFloat(iv, "intensity");
                if (!v) { recordStatus(core::Status(v.error()), {}); return v.error(); }
                lgt.intensity = v.value();
            }
            if (const auto* en = cmd.findKey("enabled"); en != nullptr && en->isBool()) lgt.enabled = en->asBool();
            if (const auto* rg = cmd.findKey("range"); rg != nullptr) {
                auto v = parseJsonFloat(rg, "range");
                if (!v) { recordStatus(core::Status(v.error()), {}); return v.error(); }
                lgt.range = v.value();
            }
            if (const auto* at = cmd.findKey("attenuation"); at != nullptr) {
                auto v = parseJsonFloat(at, "attenuation");
                if (!v) { recordStatus(core::Status(v.error()), {}); return v.error(); }
                lgt.attenuation = v.value();
            }
            if (const auto* fo = cmd.findKey("falloff"); fo != nullptr && fo->isString()) {
                if (const auto pf = scene::parseLightFalloffModeSlug(fo->asString()); pf.has_value()) {
                    lgt.falloff = pf.value();
                }
            }
            if (const auto* sr = cmd.findKey("source_radius"); sr != nullptr) {
                auto v = parseJsonFloat(sr, "source_radius");
                if (!v) { recordStatus(core::Status(v.error()), {}); return v.error(); }
                lgt.source_radius = v.value();
            }
            if (auto st = applyShadowPatch(lgt.shadow); !st) { recordStatus(st, {}); return st; }
            return setPointLightComponent(obj_id, lgt, coalesce);
        }

        if (target_type == scene::LightType::Spot) {
            scene::SpotLightComponent lgt = obj->spot_light.value_or(scene::SpotLightComponent{});
            if (const auto* c = cmd.findKey("color"); c != nullptr) {
                auto v = parseJsonVec3(c, "color");
                if (!v) { recordStatus(core::Status(v.error()), {}); return v.error(); }
                lgt.color = v.value();
            }
            if (const auto* iv = cmd.findKey("intensity"); iv != nullptr) {
                auto v = parseJsonFloat(iv, "intensity");
                if (!v) { recordStatus(core::Status(v.error()), {}); return v.error(); }
                lgt.intensity = v.value();
            }
            if (const auto* en = cmd.findKey("enabled"); en != nullptr && en->isBool()) lgt.enabled = en->asBool();
            if (const auto* rg = cmd.findKey("range"); rg != nullptr) {
                auto v = parseJsonFloat(rg, "range");
                if (!v) { recordStatus(core::Status(v.error()), {}); return v.error(); }
                lgt.range = v.value();
            }
            if (const auto* at = cmd.findKey("attenuation"); at != nullptr) {
                auto v = parseJsonFloat(at, "attenuation");
                if (!v) { recordStatus(core::Status(v.error()), {}); return v.error(); }
                lgt.attenuation = v.value();
            }
            if (const auto* fo = cmd.findKey("falloff"); fo != nullptr && fo->isString()) {
                if (const auto pf = scene::parseLightFalloffModeSlug(fo->asString()); pf.has_value()) {
                    lgt.falloff = pf.value();
                }
            }
            if (const auto* ic = cmd.findKey("inner_cone_deg"); ic != nullptr) {
                auto v = parseJsonFloat(ic, "inner_cone_deg");
                if (!v) { recordStatus(core::Status(v.error()), {}); return v.error(); }
                lgt.inner_cone_radians = math::degToRad(std::clamp(v.value(), 0.0f, 88.9f));
            }
            if (const auto* oc = cmd.findKey("outer_cone_deg"); oc != nullptr) {
                auto v = parseJsonFloat(oc, "outer_cone_deg");
                if (!v) { recordStatus(core::Status(v.error()), {}); return v.error(); }
                lgt.outer_cone_radians = math::degToRad(std::clamp(v.value(), 0.5f, 89.0f));
            }
            if (lgt.inner_cone_radians > lgt.outer_cone_radians) {
                lgt.inner_cone_radians = lgt.outer_cone_radians;
            }
            if (const auto* cf = cmd.findKey("cone_falloff"); cf != nullptr) {
                auto v = parseJsonFloat(cf, "cone_falloff");
                if (!v) { recordStatus(core::Status(v.error()), {}); return v.error(); }
                lgt.cone_falloff = v.value();
            }
            if (auto st = applyShadowPatch(lgt.shadow); !st) { recordStatus(st, {}); return st; }
            return setSpotLightComponent(obj_id, lgt, coalesce);
        }

        if (target_type == scene::LightType::Area) {
            scene::AreaLightComponent lgt = obj->area_light.value_or(scene::AreaLightComponent{});
            if (const auto* c = cmd.findKey("color"); c != nullptr) {
                auto v = parseJsonVec3(c, "color");
                if (!v) { recordStatus(core::Status(v.error()), {}); return v.error(); }
                lgt.color = v.value();
            }
            if (const auto* iv = cmd.findKey("intensity"); iv != nullptr) {
                auto v = parseJsonFloat(iv, "intensity");
                if (!v) { recordStatus(core::Status(v.error()), {}); return v.error(); }
                lgt.intensity = v.value();
            }
            if (const auto* en = cmd.findKey("enabled"); en != nullptr && en->isBool()) lgt.enabled = en->asBool();
            if (const auto* sh = cmd.findKey("shape"); sh != nullptr && sh->isString()) {
                const auto ps = scene::parseAreaLightShapeSlug(sh->asString());
                if (!ps.has_value()) {
                    auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Unknown area light shape");
                    recordStatus(st, {});
                    return st;
                }
                lgt.shape = ps.value();
            }
            if (const auto* wd = cmd.findKey("width"); wd != nullptr) {
                auto v = parseJsonFloat(wd, "width");
                if (!v) { recordStatus(core::Status(v.error()), {}); return v.error(); }
                lgt.width = v.value();
            }
            if (const auto* ht = cmd.findKey("height"); ht != nullptr) {
                auto v = parseJsonFloat(ht, "height");
                if (!v) { recordStatus(core::Status(v.error()), {}); return v.error(); }
                lgt.height = v.value();
            }
            if (const auto* rg = cmd.findKey("range"); rg != nullptr) {
                auto v = parseJsonFloat(rg, "range");
                if (!v) { recordStatus(core::Status(v.error()), {}); return v.error(); }
                lgt.range = v.value();
            }
            if (const auto* ts = cmd.findKey("two_sided"); ts != nullptr && ts->isBool()) {
                lgt.two_sided = ts->asBool();
            }
            if (const auto* sq = cmd.findKey("sample_quality"); sq != nullptr) {
                auto v = parseJsonId(sq, "sample_quality");
                if (!v) { recordStatus(core::Status(v.error()), {}); return v.error(); }
                lgt.sample_quality = static_cast<std::uint32_t>(std::clamp<std::uint64_t>(v.value(), 1u, 3u));
            }
            if (auto st = applyShadowPatch(lgt.shadow); !st) { recordStatus(st, {}); return st; }
            return setAreaLightComponent(obj_id, lgt, coalesce);
        }

        scene::DirectionalLightComponent lgt = obj->directional_light.value_or(scene::DirectionalLightComponent{});
        if (const auto* c = cmd.findKey("color"); c != nullptr) {
            auto v = parseJsonVec3(c, "color");
            if (!v) { recordStatus(core::Status(v.error()), {}); return v.error(); }
            lgt.color = v.value();
        }
        if (const auto* iv = cmd.findKey("intensity"); iv != nullptr) {
            auto v = parseJsonFloat(iv, "intensity");
            if (!v) { recordStatus(core::Status(v.error()), {}); return v.error(); }
            lgt.intensity = v.value();
        }
        if (const auto* en = cmd.findKey("enabled"); en != nullptr && en->isBool()) lgt.enabled = en->asBool();
        if (auto st = applyShadowPatch(lgt.shadow); !st) { recordStatus(st, {}); return st; }
        return setDirectionalLightComponent(obj_id, lgt, coalesce);
    }

    if (action == "change_light_type") {
        auto id_res = parseJsonId(cmd.findKey("id"), "id");
        const auto* tv = cmd.findKey("light_type");
        if (!id_res) { recordStatus(core::Status(id_res.error()), {}); return id_res.error(); }
        if (tv == nullptr || !tv->isString()) {
            auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Missing string 'light_type'");
            recordStatus(st, {});
            return st;
        }
        const auto parsed_type = scene::parseLightTypeSlug(tv->asString());
        if (!parsed_type.has_value()) {
            auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Invalid light_type: " + tv->asString());
            recordStatus(st, {});
            return st;
        }
        return convertObjectLightType(scene::ObjectId{id_res.value()}, parsed_type.value());
    }

    if (action == "set_light_linking") {
        auto id_res = parseJsonId(cmd.findKey("id"), "id");
        const auto* mv = cmd.findKey("mode");
        if (!id_res) { recordStatus(core::Status(id_res.error()), {}); return id_res.error(); }
        if (mv == nullptr || !mv->isString()) {
            auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Missing string 'mode'");
            recordStatus(st, {});
            return st;
        }
        const auto parsed_mode = scene::parseLightLinkModeSlug(mv->asString());
        if (!parsed_mode.has_value()) {
            auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Invalid light link mode: " + mv->asString());
            recordStatus(st, {});
            return st;
        }
        const scene::ObjectId light_id{id_res.value()};
        const auto* obj = session_.document().scene.findObject(light_id);
        scene::LightLinkingSettings linking{};
        if (obj != nullptr && obj->lightLinking() != nullptr) {
            linking = *obj->lightLinking();
        }
        linking.mode = parsed_mode.value();
        if (const auto* inc_d = cmd.findKey("include_descendants"); inc_d != nullptr && inc_d->isBool()) {
            linking.include_descendants = inc_d->asBool();
        }
        if (const auto* ids = cmd.findKey("object_ids"); ids != nullptr && ids->isArray()) {
            linking.linked_objects.clear();
            for (const auto& entry : ids->asArray()) {
                auto tid = parseJsonId(&entry, "object_ids[]");
                if (!tid) { recordStatus(core::Status(tid.error()), {}); return tid.error(); }
                if (tid.value() != 0) {
                    linking.linked_objects.push_back(scene::ObjectId{tid.value()});
                }
            }
            linking.normalize();
        }
        return setLightLinking(light_id, std::move(linking));
    }

    if (action == "toggle_light_link_target") {
        auto id_res = parseJsonId(cmd.findKey("id"), "id");
        auto tid_res = parseJsonId(cmd.findKey("target_id"), "target_id");
        const auto* lv = cmd.findKey("linked");
        if (!id_res) { recordStatus(core::Status(id_res.error()), {}); return id_res.error(); }
        if (!tid_res) { recordStatus(core::Status(tid_res.error()), {}); return tid_res.error(); }
        if (lv == nullptr || !lv->isBool()) {
            auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Missing boolean 'linked'");
            recordStatus(st, {});
            return st;
        }
        return toggleLightLinkTarget(
            scene::ObjectId{id_res.value()},
            scene::ObjectId{tid_res.value()},
            lv->asBool());
    }

    if (action == "set_environment") {
        scene::EnvironmentLighting env = session_.document().scene.environment();
        if (const auto* en = cmd.findKey("enabled"); en != nullptr && en->isBool()) {
            env.enabled = en->asBool();
        }
        if (const auto* iv = cmd.findKey("intensity"); iv != nullptr) {
            auto v = parseJsonFloat(iv, "intensity");
            if (!v) { recordStatus(core::Status(v.error()), {}); return v.error(); }
            env.intensity = v.value();
        }
        if (const auto* rv = cmd.findKey("rotation_deg"); rv != nullptr) {
            auto v = parseJsonFloat(rv, "rotation_deg");
            if (!v) { recordStatus(core::Status(v.error()), {}); return v.error(); }
            env.rotation_radians = math::degToRad(v.value());
        }
        if (const auto* dv = cmd.findKey("diffuse_ibl_scale"); dv != nullptr) {
            auto v = parseJsonFloat(dv, "diffuse_ibl_scale");
            if (!v) { recordStatus(core::Status(v.error()), {}); return v.error(); }
            env.diffuse_ibl_scale = v.value();
        }
        if (const auto* sv = cmd.findKey("specular_ibl_scale"); sv != nullptr) {
            auto v = parseJsonFloat(sv, "specular_ibl_scale");
            if (!v) { recordStatus(core::Status(v.error()), {}); return v.error(); }
            env.specular_ibl_scale = v.value();
        }
        bool coalesce = false;
        if (const auto* cv = cmd.findKey("coalesce"); cv != nullptr && cv->isBool()) {
            coalesce = cv->asBool();
        }
        return setEnvironmentSettings(env, coalesce);
    }

    if (action == "load_hdri_preset") {
        const auto* pv = cmd.findKey("preset");
        if (pv == nullptr || !pv->isString()) {
            auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Missing string 'preset'");
            recordStatus(st, {});
            return st;
        }
        return loadBuiltInHdriPreset(pv->asString());
    }

    if (action == "clear_hdri") {
        return clearEnvironmentHdri();
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

    if (action == "set_camera_view") {
        const auto* av = cmd.findKey("active");
        if (av == nullptr || !av->isBool()) {
            auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Missing boolean 'active'");
            recordStatus(st, {});
            return st;
        }
        setCameraViewActive(av->asBool());
        return core::Status::ok();
    }

    if (action == "toggle_camera_view") {
        setCameraViewActive(!session_.isCameraViewActive());
        return core::Status::ok();
    }

    if (action == "set_editor_helpers_visible") {
        const auto* vv = cmd.findKey("visible");
        if (vv == nullptr || !vv->isBool()) {
            auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Missing boolean 'visible'");
            recordStatus(st, {});
            return st;
        }
        setEditorHelpersVisible(vv->asBool());
        return core::Status::ok();
    }

    if (action == "toggle_editor_helpers") {
        setEditorHelpersVisible(!session_.areEditorHelpersVisible());
        return core::Status::ok();
    }

    if (action == "set_editor_grid_visible") {
        const auto* vv = cmd.findKey("visible");
        if (vv == nullptr || !vv->isBool()) {
            auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Missing boolean 'visible'");
            recordStatus(st, {});
            return st;
        }
        setEditorGridVisible(vv->asBool());
        return core::Status::ok();
    }

    if (action == "toggle_editor_grid") {
        setEditorGridVisible(!session_.isEditorGridVisible());
        return core::Status::ok();
    }

    if (action == "set_viewport_view") {
        const auto* vv = cmd.findKey("view");
        if (vv == nullptr || !vv->isString()) {
            auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Missing string 'view'");
            recordStatus(st, {});
            return st;
        }
        const auto preset = parseViewportViewPreset(vv->asString());
        if (!preset.has_value()) {
            auto st = core::Status::fail(
                core::ErrorCode::InvalidArgument,
                "Invalid viewport view preset: " + vv->asString());
            recordStatus(st, {});
            return st;
        }
        bool smooth = true;
        if (const auto* sv = cmd.findKey("smooth"); sv != nullptr && sv->isBool()) {
            smooth = sv->asBool();
        }
        setViewportViewPreset(preset.value(), smooth);
        return core::Status::ok();
    }

    if (action == "reset_viewport_view") {
        bool smooth = true;
        if (const auto* sv = cmd.findKey("smooth"); sv != nullptr && sv->isBool()) {
            smooth = sv->asBool();
        }
        resetViewportHomeView(smooth);
        return core::Status::ok();
    }

    if (action == "set_viewport_projection") {
        const auto* pv = cmd.findKey("projection");
        if (pv == nullptr || !pv->isString()) {
            auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Missing string 'projection'");
            recordStatus(st, {});
            return st;
        }
        const std::string& ps = pv->asString();
        if (ps == "orthographic" || ps == "ortho") {
            setViewportProjection(scene::ProjectionType::Orthographic);
        } else if (ps == "perspective" || ps == "persp") {
            setViewportProjection(scene::ProjectionType::Perspective);
        } else {
            auto st = core::Status::fail(
                core::ErrorCode::InvalidArgument,
                "Invalid viewport projection: " + ps);
            recordStatus(st, {});
            return st;
        }
        return core::Status::ok();
    }

    if (action == "toggle_viewport_projection") {
        toggleViewportProjection();
        return core::Status::ok();
    }

    if (action == "orbit_editor_camera") {
        auto dx_res = parseJsonFloat(cmd.findKey("dx"), "dx");
        auto dy_res = parseJsonFloat(cmd.findKey("dy"), "dy");
        if (!dx_res) { recordStatus(core::Status(dx_res.error()), {}); return dx_res.error(); }
        if (!dy_res) { recordStatus(core::Status(dy_res.error()), {}); return dy_res.error(); }
        return orbitEditorObserverCamera(dx_res.value(), dy_res.value());
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

    // =========================================================================
    // Stage 1 Maya Core Interface Actions (Selection, Tools, Display, Layers,
    // Channel Locks, Custom Attributes, Pivot, Hierarchy, Timeline, CommandLine)
    // =========================================================================
    if (action == "select_objects") {
        const auto* arr_v = cmd.findKey("ids");
        if (arr_v == nullptr || !arr_v->isArray()) {
            auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Missing 'ids' array");
            recordStatus(st, {});
            return st;
        }
        std::vector<scene::ObjectId> ids;
        for (const auto& iv : arr_v->asArray()) {
            auto parsed_id = parseJsonId(&iv, "ids[]");
            if (!parsed_id) { recordStatus(core::Status(parsed_id.error()), {}); return parsed_id.error(); }
            if (parsed_id.value() != 0u) ids.push_back(scene::ObjectId{parsed_id.value()});
        }
        return selectObjects(ids);
    }
    if (action == "toggle_select_object") {
        auto id_res = parseJsonId(cmd.findKey("id"), "id");
        if (!id_res) { recordStatus(core::Status(id_res.error()), {}); return id_res.error(); }
        return toggleSelectObject(scene::ObjectId{id_res.value()});
    }
    if (action == "select_all")       return selectAllObjects();
    if (action == "invert_selection") return invertSelection();
    if (action == "select_hierarchy") return selectHierarchy();
    if (action == "select_similar")   return selectSimilar();
    if (action == "frame_all")        return frameAllScene();

    if (action == "set_active_tool") {
        const auto* tv = cmd.findKey("tool");
        if (tv == nullptr || !tv->isString()) {
            auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Missing string 'tool'");
            recordStatus(st, {});
            return st;
        }
        const auto parsed_tool = parseMayaActiveToolSlug(tv->asString());
        if (!parsed_tool.has_value()) {
            auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Unknown Maya tool: " + tv->asString());
            recordStatus(st, {});
            return st;
        }
        setActiveTool(parsed_tool.value());
        return core::Status::ok();
    }
    if (action == "activate_last_tool") {
        activateLastTool();
        return core::Status::ok();
    }

    if (action == "set_viewport_display") {
        rendering::ViewportDisplayOptions opts = session_.viewportDisplay();
        if (const auto* sm = cmd.findKey("shading_mode"); sm != nullptr && sm->isString()) {
            const std::string& s = sm->asString();
            if (s == "wireframe") opts.shading_mode = rendering::ViewportShadingMode::Wireframe;
            else if (s == "shaded" || s == "smooth_shaded") {
                opts.shading_mode = rendering::ViewportShadingMode::SmoothShaded;
            } else if (s == "shaded_textured" || s == "textured") {
                opts.shading_mode = rendering::ViewportShadingMode::ShadedTextured;
            }
        }
        if (const auto* wos = cmd.findKey("wireframe_on_shaded"); wos != nullptr && wos->isBool()) {
            opts.wireframe_on_shaded = wos->asBool();
        }
        if (const auto* xr = cmd.findKey("xray_mode"); xr != nullptr && xr->isBool()) {
            opts.xray_mode = xr->asBool();
        }
        if (const auto* ual = cmd.findKey("use_all_lights"); ual != nullptr && ual->isBool()) {
            opts.use_all_lights = ual->asBool();
        }
        if (const auto* dm = cmd.findKey("default_material"); dm != nullptr && dm->isBool()) {
            opts.default_material = dm->asBool();
        }
        if (const auto* mds = cmd.findKey("mesh_display_smoothness"); mds != nullptr && mds->isNumber()) {
            if (auto sv = parseJsonId(mds, "mesh_display_smoothness"); sv.hasValue()) {
                opts.mesh_display_smoothness = static_cast<std::uint8_t>(std::clamp<std::uint64_t>(sv.value(), 1u, 3u));
            }
        }
        if (const auto* iso = cmd.findKey("isolate_select"); iso != nullptr && iso->isBool()) {
            setIsolateSelect(iso->asBool());
            opts.isolate_select = session_.viewportDisplay().isolate_select;
            opts.isolated_objects = session_.viewportDisplay().isolated_objects;
        }
        setViewportDisplay(std::move(opts));
        return core::Status::ok();
    }
    if (action == "set_isolate_select") {
        const auto* ev = cmd.findKey("enabled");
        if (ev == nullptr || !ev->isBool()) {
            auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Missing boolean 'enabled'");
            recordStatus(st, {});
            return st;
        }
        setIsolateSelect(ev->asBool());
        return core::Status::ok();
    }

    if (action == "set_snapping") {
        MayaSnappingState sn = session_.snapping();
        if (const auto* v = cmd.findKey("grid"); v != nullptr && v->isBool()) sn.snap_grid = v->asBool();
        if (const auto* v = cmd.findKey("grid_step"); v != nullptr) {
            if (auto f = parseJsonFloat(v, "grid_step"); f.hasValue() && f.value() > 0.001f) sn.grid_step = f.value();
        }
        if (const auto* v = cmd.findKey("point"); v != nullptr && v->isBool()) sn.snap_point = v->asBool();
        if (const auto* v = cmd.findKey("curve"); v != nullptr && v->isBool()) sn.snap_curve = v->asBool();
        if (const auto* v = cmd.findKey("surface"); v != nullptr && v->isBool()) sn.snap_surface = v->asBool();
        setSnapping(sn);
        return core::Status::ok();
    }

    if (action == "set_selection_mask") {
        ObjectSelectionMask m = session_.selectionMask();
        if (const auto* v = cmd.findKey("surfaces"); v != nullptr && v->isBool()) m.meshes = v->asBool();
        if (const auto* v = cmd.findKey("meshes"); v != nullptr && v->isBool()) m.meshes = v->asBool();
        if (const auto* v = cmd.findKey("lights"); v != nullptr && v->isBool()) m.lights = v->asBool();
        if (const auto* v = cmd.findKey("cameras"); v != nullptr && v->isBool()) m.cameras = v->asBool();
        if (const auto* v = cmd.findKey("helpers"); v != nullptr && v->isBool()) m.transforms = v->asBool();
        if (const auto* v = cmd.findKey("transforms"); v != nullptr && v->isBool()) m.transforms = v->asBool();
        setSelectionMask(m);
        return core::Status::ok();
    }

    if (action == "group_selected") {
        std::string gname;
        if (const auto* nv = cmd.findKey("name"); nv != nullptr && nv->isString()) gname = nv->asString();
        auto res = groupSelectedObjects(std::move(gname));
        return res.hasValue() ? core::Status::ok() : core::Status(res.error());
    }
    if (action == "parent_selected")   return parentSelectedObjects();
    if (action == "unparent_selected") return unparentSelectedObjects();

    if (action == "freeze_transforms") {
        scene::ObjectId oid = session_.selectedObject();
        if (const auto* iv = cmd.findKey("id"); iv != nullptr && iv->isNumber()) {
            if (auto p = parseJsonId(iv, "id"); p.hasValue()) oid = scene::ObjectId{p.value()};
        }
        return freezeObjectTransforms(oid);
    }
    if (action == "center_pivot") {
        scene::ObjectId oid = session_.selectedObject();
        if (const auto* iv = cmd.findKey("id"); iv != nullptr && iv->isNumber()) {
            if (auto p = parseJsonId(iv, "id"); p.hasValue()) oid = scene::ObjectId{p.value()};
        }
        return centerObjectPivot(oid);
    }

    if (action == "set_object_locked") {
        auto id_res = parseJsonId(cmd.findKey("id"), "id");
        const auto* lv = cmd.findKey("locked");
        if (!id_res || lv == nullptr || !lv->isBool()) {
            auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Invalid set_object_locked payload");
            recordStatus(st, {});
            return st;
        }
        return setObjectLocked(scene::ObjectId{id_res.value()}, lv->asBool());
    }
    if (action == "set_channel_locks") {
        auto id_res = parseJsonId(cmd.findKey("id"), "id");
        auto mv_res = parseJsonId(cmd.findKey("mask"), "mask");
        if (!id_res || !mv_res) {
            auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Invalid set_channel_locks payload");
            recordStatus(st, {});
            return st;
        }
        return setObjectChannelLocks(scene::ObjectId{id_res.value()}, static_cast<std::uint16_t>(mv_res.value()));
    }
    if (action == "set_pivot_offset") {
        auto id_res = parseJsonId(cmd.findKey("id"), "id");
        auto pv_res = parseJsonVec3(cmd.findKey("pivot_offset"), "pivot_offset");
        if (!id_res || !pv_res) {
            auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Invalid set_pivot_offset payload");
            recordStatus(st, {});
            return st;
        }
        bool coalesce = false;
        if (const auto* cv = cmd.findKey("coalesce"); cv != nullptr && cv->isBool()) coalesce = cv->asBool();
        return setObjectPivotOffset(scene::ObjectId{id_res.value()}, pv_res.value(), coalesce);
    }
    if (action == "add_custom_attribute") {
        scene::ObjectId oid = session_.selectedObject();
        if (const auto* iv = cmd.findKey("id"); iv != nullptr && iv->isNumber()) {
            if (auto p = parseJsonId(iv, "id"); p.hasValue()) oid = scene::ObjectId{p.value()};
        }
        const auto* nv = cmd.findKey("name");
        if (nv == nullptr || !nv->isString()) {
            auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Missing string 'name'");
            recordStatus(st, {});
            return st;
        }
        scene::CustomAttribute attr{.name = nv->asString()};
        if (const auto* vv = cmd.findKey("value"); vv != nullptr && vv->isNumber()) attr.value = vv->asNumber();
        if (const auto* mn = cmd.findKey("min_value"); mn != nullptr && mn->isNumber()) attr.min_value = mn->asNumber();
        if (const auto* mx = cmd.findKey("max_value"); mx != nullptr && mx->isNumber()) attr.max_value = mx->asNumber();
        if (const auto* hl = cmd.findKey("has_limits"); hl != nullptr && hl->isBool()) attr.has_limits = hl->asBool();
        if (const auto* kb = cmd.findKey("keyable"); kb != nullptr && kb->isBool()) attr.keyable = kb->asBool();
        if (const auto* lk = cmd.findKey("locked"); lk != nullptr && lk->isBool()) attr.locked = lk->asBool();
        return addCustomAttribute(oid, std::move(attr));
    }
    if (action == "set_custom_attribute") {
        auto id_res = parseJsonId(cmd.findKey("id"), "id");
        const auto* nv = cmd.findKey("name");
        const auto* vv = cmd.findKey("value");
        if (!id_res || nv == nullptr || !nv->isString() || vv == nullptr || !vv->isNumber()) {
            auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Invalid set_custom_attribute payload");
            recordStatus(st, {});
            return st;
        }
        bool coalesce = false;
        if (const auto* cv = cmd.findKey("coalesce"); cv != nullptr && cv->isBool()) coalesce = cv->asBool();
        return setCustomAttributeValue(scene::ObjectId{id_res.value()}, nv->asString(), vv->asNumber(), coalesce);
    }
    if (action == "delete_custom_attribute") {
        auto id_res = parseJsonId(cmd.findKey("id"), "id");
        const auto* nv = cmd.findKey("name");
        if (!id_res || nv == nullptr || !nv->isString()) {
            auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Invalid delete_custom_attribute payload");
            recordStatus(st, {});
            return st;
        }
        return deleteCustomAttribute(scene::ObjectId{id_res.value()}, nv->asString());
    }

    if (action == "create_display_layer") {
        std::string lname;
        if (const auto* nv = cmd.findKey("name"); nv != nullptr && nv->isString()) lname = nv->asString();
        bool assign_sel = false;
        if (const auto* av = cmd.findKey("assign_selected"); av != nullptr && av->isBool()) assign_sel = av->asBool();
        auto res = createDisplayLayer(std::move(lname), assign_sel);
        return res.hasValue() ? core::Status::ok() : core::Status(res.error());
    }
    if (action == "rename_display_layer") {
        auto lid = parseJsonId(cmd.findKey("layer_id"), "layer_id");
        const auto* nv = cmd.findKey("name");
        if (!lid || nv == nullptr || !nv->isString()) {
            auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Invalid rename_display_layer payload");
            recordStatus(st, {});
            return st;
        }
        return renameDisplayLayer(static_cast<std::uint32_t>(lid.value()), nv->asString());
    }
    if (action == "delete_display_layer") {
        auto lid = parseJsonId(cmd.findKey("layer_id"), "layer_id");
        if (!lid) { recordStatus(core::Status(lid.error()), {}); return lid.error(); }
        return deleteDisplayLayer(static_cast<std::uint32_t>(lid.value()));
    }
    if (action == "update_display_layer") {
        auto lid = parseJsonId(cmd.findKey("layer_id"), "layer_id");
        if (!lid) { recordStatus(core::Status(lid.error()), {}); return lid.error(); }
        const std::uint32_t target_lid = static_cast<std::uint32_t>(lid.value());
        const scene::DisplayLayer* cur = nullptr;
        for (const auto& l : session_.document().scene.displayLayers()) {
            if (l.id == target_lid) { cur = &l; break; }
        }
        if (cur == nullptr) {
            auto st = core::Status::fail(core::ErrorCode::NotFound, "Display layer not found");
            recordStatus(st, {});
            return st;
        }
        bool vis = cur->visible;
        bool pvis = cur->playback_visible;
        scene::DisplayLayerType dt = cur->display_type;
        math::Vec3 col = cur->color;
        if (const auto* v = cmd.findKey("visible"); v != nullptr && v->isBool()) vis = v->asBool();
        if (const auto* v = cmd.findKey("playback_visible"); v != nullptr && v->isBool()) pvis = v->asBool();
        if (const auto* v = cmd.findKey("display_type"); v != nullptr && v->isString()) {
            if (const auto parsed_dt = scene::parseDisplayLayerTypeSlug(v->asString()); parsed_dt.has_value()) {
                dt = parsed_dt.value();
            }
        }
        if (const auto* v = cmd.findKey("color"); v != nullptr) {
            if (auto parsed_col = parseJsonVec3(v, "color"); parsed_col.hasValue()) col = parsed_col.value();
        }
        return updateDisplayLayer(target_lid, vis, pvis, dt, col);
    }
    if (action == "assign_to_display_layer") {
        auto lid = parseJsonId(cmd.findKey("layer_id"), "layer_id");
        if (!lid) { recordStatus(core::Status(lid.error()), {}); return lid.error(); }
        return assignSelectedToDisplayLayer(static_cast<std::uint32_t>(lid.value()));
    }
    if (action == "remove_from_display_layer") {
        auto lid = parseJsonId(cmd.findKey("layer_id"), "layer_id");
        if (!lid) { recordStatus(core::Status(lid.error()), {}); return lid.error(); }
        return removeSelectedFromDisplayLayer(static_cast<std::uint32_t>(lid.value()));
    }
    if (action == "select_display_layer_objects") {
        auto lid = parseJsonId(cmd.findKey("layer_id"), "layer_id");
        if (!lid) { recordStatus(core::Status(lid.error()), {}); return lid.error(); }
        return selectDisplayLayerObjects(static_cast<std::uint32_t>(lid.value()));
    }

    if (action == "set_timeline") {
        scene::TimelineSettings tl = session_.document().scene.timeline();
        if (const auto* v = cmd.findKey("current_frame"); v != nullptr) {
            if (auto f = parseJsonFloat(v, "current_frame"); f.hasValue()) tl.current_frame = f.value();
        }
        if (const auto* v = cmd.findKey("start_frame"); v != nullptr) {
            if (auto f = parseJsonFloat(v, "start_frame"); f.hasValue()) tl.start_frame = f.value();
        }
        if (const auto* v = cmd.findKey("end_frame"); v != nullptr) {
            if (auto f = parseJsonFloat(v, "end_frame"); f.hasValue()) tl.end_frame = f.value();
        }
        if (const auto* v = cmd.findKey("playback_start"); v != nullptr) {
            if (auto f = parseJsonFloat(v, "playback_start"); f.hasValue()) tl.playback_start = f.value();
        }
        if (const auto* v = cmd.findKey("playback_end"); v != nullptr) {
            if (auto f = parseJsonFloat(v, "playback_end"); f.hasValue()) tl.playback_end = f.value();
        }
        if (const auto* v = cmd.findKey("fps"); v != nullptr) {
            if (auto f = parseJsonFloat(v, "fps"); f.hasValue() && f.value() >= 1.0f) tl.fps = f.value();
        }
        if (const auto* v = cmd.findKey("playback_mode"); v != nullptr && v->isString()) {
            if (const auto parsed_lm = scene::parseTimelineLoopModeSlug(v->asString()); parsed_lm.has_value()) {
                tl.loop_mode = parsed_lm.value();
            }
        }
        if (const auto* v = cmd.findKey("auto_key"); v != nullptr && v->isBool()) {
            tl.auto_key = v->asBool();
        }
        setTimelineSettings(tl);
        return core::Status::ok();
    }
    if (action == "set_current_frame") {
        auto fr = parseJsonFloat(cmd.findKey("frame"), "frame");
        if (!fr) { recordStatus(core::Status(fr.error()), {}); return fr.error(); }
        setCurrentFrame(fr.value());
        return core::Status::ok();
    }
    if (action == "set_timeline_playing") {
        const auto* pv = cmd.findKey("playing");
        if (pv == nullptr || !pv->isBool()) {
            auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Missing boolean 'playing'");
            recordStatus(st, {});
            return st;
        }
        setTimelinePlaying(pv->asBool());
        return core::Status::ok();
    }
    if (action == "set_keyframe") {
        std::optional<float> fr_opt;
        if (const auto* fv = cmd.findKey("frame"); fv != nullptr && fv->isNumber()) {
            if (auto f = parseJsonFloat(fv, "frame"); f.hasValue()) fr_opt = f.value();
        }
        return setKeyframeOnSelected(fr_opt);
    }
    if (action == "delete_keyframe") {
        std::optional<float> fr_opt;
        if (const auto* fv = cmd.findKey("frame"); fv != nullptr && fv->isNumber()) {
            if (auto f = parseJsonFloat(fv, "frame"); fv != nullptr && f.hasValue()) fr_opt = f.value();
        }
        return deleteKeyframeOnSelected(fr_opt);
    }

    if (action == "execute_command_line") {
        const auto* cv = cmd.findKey("command");
        if (cv == nullptr || !cv->isString()) {
            auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Missing string 'command'");
            recordStatus(st, {});
            return st;
        }
        return executeCommandLine(cv->asString());
    }

    auto st = core::Status::fail(core::ErrorCode::InvalidArgument, "Unknown editor action: " + action);
    recordStatus(st, {});
    return st;
}

} // namespace finaleffect::app
