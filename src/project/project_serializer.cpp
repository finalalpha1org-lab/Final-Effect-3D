// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/project/project.hpp"
#include "finaleffect/project/binary_asset.hpp"
#include "finaleffect/project/json.hpp"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <limits>
#include <unordered_map>
#include <unordered_set>

namespace finaleffect::project {

namespace {

JsonValue encodeVec2(const math::Vec2& v) {
    return JsonArray{JsonValue(static_cast<double>(v.x)), JsonValue(static_cast<double>(v.y))};
}

JsonValue encodeVec3(const math::Vec3& v) {
    return JsonArray{
        JsonValue(static_cast<double>(v.x)),
        JsonValue(static_cast<double>(v.y)),
        JsonValue(static_cast<double>(v.z)),
    };
}

JsonValue encodeQuat(const math::Quat& q) {
    return JsonArray{
        JsonValue(static_cast<double>(q.x)),
        JsonValue(static_cast<double>(q.y)),
        JsonValue(static_cast<double>(q.z)),
        JsonValue(static_cast<double>(q.w)),
    };
}

void appendExtraFields(JsonValue& target_obj, const JsonObject& extra) {
    for (const auto& [k, v] : extra) {
        if (target_obj.findKey(k) == nullptr) {
            target_obj.setKey(k, v);
        }
    }
}

void captureUnknownFields(
    const JsonValue& source_obj,
    std::initializer_list<std::string_view> known_keys,
    std::string_view scope_label,
    JsonObject& out_extra,
    std::vector<std::string>* out_warnings) {
    if (!source_obj.isObject()) {
        return;
    }
    const std::unordered_set<std::string_view> known(known_keys.begin(), known_keys.end());
    for (const auto& [k, v] : source_obj.asObject()) {
        if (!known.contains(k)) {
            out_extra.emplace_back(k, v);
            if (out_warnings != nullptr) {
                out_warnings->push_back(
                    "Preserved unrecognized field '" + std::string(scope_label) + "." + k + "'");
            }
        }
    }
}

core::Result<std::uint64_t> decodeUint64(const JsonValue* val, std::string_view field_name) {
    if (val == nullptr || !val->isNumber()) {
        return core::Result<std::uint64_t>::fail(
            core::ErrorCode::ValidationError,
            "Expected non-negative integer field",
            std::string(field_name));
    }
    const double d = val->asNumber();
    if (!std::isfinite(d) || d < 0.0 || d > 9007199254740991.0 || std::floor(d) != d) {
        return core::Result<std::uint64_t>::fail(
            core::ErrorCode::ValidationError,
            "Field must be an exact non-negative integer",
            std::string(field_name));
    }
    return core::Result<std::uint64_t>::ok(static_cast<std::uint64_t>(d));
}

core::Result<float> decodeFloat(const JsonValue* val, std::string_view field_name) {
    if (val == nullptr || !val->isNumber()) {
        return core::Result<float>::fail(
            core::ErrorCode::ValidationError,
            "Expected numeric field",
            std::string(field_name));
    }
    const double d = val->asNumber();
    if (!std::isfinite(d) || std::fabs(d) > static_cast<double>(std::numeric_limits<float>::max())) {
        return core::Result<float>::fail(
            core::ErrorCode::ValidationError,
            "Numeric field out of 32-bit float range",
            std::string(field_name));
    }
    return core::Result<float>::ok(static_cast<float>(d));
}

core::Result<math::Vec2> decodeVec2(const JsonValue* val, std::string_view field_name) {
    if (val == nullptr || !val->isArray() || val->asArray().size() != 2) {
        return core::Result<math::Vec2>::fail(
            core::ErrorCode::ValidationError,
            "Expected 2-element numeric array",
            std::string(field_name));
    }
    const auto& a = val->asArray();
    auto x = decodeFloat(&a[0], field_name);
    auto y = decodeFloat(&a[1], field_name);
    if (!x) return x.error();
    if (!y) return y.error();
    return core::Result<math::Vec2>::ok(math::Vec2{x.value(), y.value()});
}

core::Result<math::Vec3> decodeVec3(const JsonValue* val, std::string_view field_name) {
    if (val == nullptr || !val->isArray() || val->asArray().size() != 3) {
        return core::Result<math::Vec3>::fail(
            core::ErrorCode::ValidationError,
            "Expected 3-element numeric array",
            std::string(field_name));
    }
    const auto& a = val->asArray();
    auto x = decodeFloat(&a[0], field_name);
    auto y = decodeFloat(&a[1], field_name);
    auto z = decodeFloat(&a[2], field_name);
    if (!x) return x.error();
    if (!y) return y.error();
    if (!z) return z.error();
    return core::Result<math::Vec3>::ok(math::Vec3{x.value(), y.value(), z.value()});
}

core::Result<math::Quat> decodeQuat(const JsonValue* val, std::string_view field_name) {
    if (val == nullptr || !val->isArray() || val->asArray().size() != 4) {
        return core::Result<math::Quat>::fail(
            core::ErrorCode::ValidationError,
            "Expected 4-element quaternion array [x, y, z, w]",
            std::string(field_name));
    }
    const auto& a = val->asArray();
    auto x = decodeFloat(&a[0], field_name);
    auto y = decodeFloat(&a[1], field_name);
    auto z = decodeFloat(&a[2], field_name);
    auto w = decodeFloat(&a[3], field_name);
    if (!x) return x.error();
    if (!y) return y.error();
    if (!z) return z.error();
    if (!w) return w.error();
    const math::Quat q{x.value(), y.value(), z.value(), w.value()};
    const float len_sq = q.lengthSquared();
    if (len_sq <= math::kStrictEpsilon * math::kStrictEpsilon) {
        return core::Result<math::Quat>::fail(
            core::ErrorCode::ValidationError,
            "Quaternion length must be non-zero",
            std::string(field_name));
    }
    if (std::fabs(len_sq - 1.0f) <= 1.0e-5f) {
        return core::Result<math::Quat>::ok(q);
    }
    return core::Result<math::Quat>::ok(q.normalized());
}

core::Result<std::vector<std::uint32_t>> decodeUint32Array(
    const JsonValue* val,
    std::string_view field_name) {
    if (val == nullptr || !val->isArray()) {
        return core::Result<std::vector<std::uint32_t>>::fail(
            core::ErrorCode::ValidationError,
            "Expected integer array",
            std::string(field_name));
    }
    std::vector<std::uint32_t> out;
    out.reserve(val->asArray().size());
    for (const auto& entry : val->asArray()) {
        auto v = decodeUint64(&entry, field_name);
        if (!v) return v.error();
        if (v.value() > static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max())) {
            return core::Result<std::vector<std::uint32_t>>::fail(
                core::ErrorCode::ValidationError,
                "Element index exceeds uint32_t range",
                std::string(field_name));
        }
        out.push_back(static_cast<std::uint32_t>(v.value()));
    }
    return core::Result<std::vector<std::uint32_t>>::ok(std::move(out));
}

JsonArray encodeUint32Array(std::span<const std::uint32_t> values) {
    JsonArray arr;
    arr.reserve(values.size());
    for (const std::uint32_t v : values) {
        arr.emplace_back(static_cast<std::uint64_t>(v));
    }
    return arr;
}

bool isSafeCompanionFilename(std::string_view uri) noexcept {
    if (uri.empty() || uri == "." || uri == "..") {
        return false;
    }
    if (uri.find('/') != std::string_view::npos ||
        uri.find('\\') != std::string_view::npos ||
        uri.find(':') != std::string_view::npos ||
        uri.find("..") != std::string_view::npos) {
        return false;
    }
    return true;
}

std::string_view editModeSlug(scene::EditMode mode) noexcept {
    switch (mode) {
        case scene::EditMode::Object: return "object";
        case scene::EditMode::Vertex: return "vertex";
        case scene::EditMode::Edge:   return "edge";
        case scene::EditMode::Face:   return "face";
    }
    return "object";
}

core::Result<scene::EditMode> parseEditModeSlug(std::string_view s) {
    if (s == "object") return core::Result<scene::EditMode>::ok(scene::EditMode::Object);
    if (s == "vertex") return core::Result<scene::EditMode>::ok(scene::EditMode::Vertex);
    if (s == "edge")   return core::Result<scene::EditMode>::ok(scene::EditMode::Edge);
    if (s == "face")   return core::Result<scene::EditMode>::ok(scene::EditMode::Face);
    return core::Result<scene::EditMode>::fail(
        core::ErrorCode::ValidationError,
        "Unknown editor_state.edit_mode: " + std::string(s));
}

JsonValue encodeEditableMesh(const scene::EditableMesh& em) {
    JsonValue em_json{JsonObject{}};

    JsonArray verts_arr;
    verts_arr.reserve(em.vertices().size());
    for (const auto& ev : em.vertices()) {
        JsonValue v_obj{JsonObject{}};
        v_obj.setKey("position", encodeVec3(ev.position));
        v_obj.setKey("color", encodeVec3(ev.color));
        verts_arr.push_back(std::move(v_obj));
    }
    em_json.setKey("vertices", std::move(verts_arr));

    JsonArray faces_arr;
    faces_arr.reserve(em.faces().size());
    for (const auto& ef : em.faces()) {
        JsonValue f_obj{JsonObject{}};
        JsonArray corners_arr;
        corners_arr.reserve(ef.corners.size());
        for (const auto& c : ef.corners) {
            JsonValue c_obj{JsonObject{}};
            c_obj.setKey("vertex", static_cast<std::uint64_t>(c.vertex));
            c_obj.setKey("uv", encodeVec2(c.uv));
            corners_arr.push_back(std::move(c_obj));
        }
        f_obj.setKey("corners", std::move(corners_arr));
        f_obj.setKey("color", encodeVec3(ef.color));
        f_obj.setKey("material_id", ef.material_id.value);
        f_obj.setKey("smooth_shading", ef.smooth_shading);
        faces_arr.push_back(std::move(f_obj));
    }
    em_json.setKey("faces", std::move(faces_arr));
    return em_json;
}

core::Result<scene::EditableMesh> decodeEditableMesh(const JsonValue& em_json) {
    if (!em_json.isObject()) {
        return core::Result<scene::EditableMesh>::fail(
            core::ErrorCode::ValidationError,
            "'mesh.editable_mesh' must be an object");
    }
    const auto* verts_val = em_json.findKey("vertices");
    const auto* faces_val = em_json.findKey("faces");
    if (verts_val == nullptr || !verts_val->isArray() || faces_val == nullptr || !faces_val->isArray()) {
        return core::Result<scene::EditableMesh>::fail(
            core::ErrorCode::ValidationError,
            "'mesh.editable_mesh' requires 'vertices' and 'faces' arrays");
    }

    std::vector<scene::EditVertex> edit_verts;
    edit_verts.reserve(verts_val->asArray().size());
    for (const auto& v_json : verts_val->asArray()) {
        if (!v_json.isObject()) {
            return core::Result<scene::EditableMesh>::fail(
                core::ErrorCode::ValidationError,
                "'editable_mesh.vertices[]' entry must be an object");
        }
        auto pos = decodeVec3(v_json.findKey("position"), "editable_mesh.vertex.position");
        if (!pos) return pos.error();
        math::Vec3 col{1.0f, 1.0f, 1.0f};
        if (const auto* col_val = v_json.findKey("color"); col_val != nullptr) {
            auto c_res = decodeVec3(col_val, "editable_mesh.vertex.color");
            if (!c_res) return c_res.error();
            col = c_res.value();
        }
        edit_verts.push_back(scene::EditVertex{.position = pos.value(), .normal = {}, .color = col});
    }

    std::vector<scene::EditFace> edit_faces;
    edit_faces.reserve(faces_val->asArray().size());
    for (const auto& f_json : faces_val->asArray()) {
        if (!f_json.isObject()) {
            return core::Result<scene::EditableMesh>::fail(
                core::ErrorCode::ValidationError,
                "'editable_mesh.faces[]' entry must be an object");
        }
        const auto* corners_val = f_json.findKey("corners");
        if (corners_val == nullptr || !corners_val->isArray()) {
            return core::Result<scene::EditableMesh>::fail(
                core::ErrorCode::ValidationError,
                "'editable_mesh.face.corners' must be an array");
        }
        scene::EditFace ef;
        ef.corners.reserve(corners_val->asArray().size());
        for (const auto& c_json : corners_val->asArray()) {
            if (!c_json.isObject()) {
                return core::Result<scene::EditableMesh>::fail(
                    core::ErrorCode::ValidationError,
                    "'editable_mesh.face.corners[]' must be an object");
            }
            auto v_idx = decodeUint64(c_json.findKey("vertex"), "editable_mesh.corner.vertex");
            auto uv_res = decodeVec2(c_json.findKey("uv"), "editable_mesh.corner.uv");
            if (!v_idx) return v_idx.error();
            if (!uv_res) return uv_res.error();
            if (v_idx.value() > static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max())) {
                return core::Result<scene::EditableMesh>::fail(
                    core::ErrorCode::ValidationError,
                    "EditableMesh corner vertex index exceeds uint32_t range");
            }
            ef.corners.push_back(scene::FaceCorner{
                .vertex = static_cast<std::uint32_t>(v_idx.value()),
                .uv = uv_res.value(),
            });
        }
        if (const auto* col_val = f_json.findKey("color"); col_val != nullptr) {
            auto c_res = decodeVec3(col_val, "editable_mesh.face.color");
            if (!c_res) return c_res.error();
            ef.color = c_res.value();
        }
        if (const auto* mid_val = f_json.findKey("material_id"); mid_val != nullptr && !mid_val->isNull()) {
            auto m_res = decodeUint64(mid_val, "editable_mesh.face.material_id");
            if (!m_res) return m_res.error();
            ef.material_id = scene::MaterialId{m_res.value()};
        }
        if (const auto* sm_val = f_json.findKey("smooth_shading"); sm_val != nullptr) {
            if (!sm_val->isBool()) {
                return core::Result<scene::EditableMesh>::fail(
                    core::ErrorCode::ValidationError,
                    "'editable_mesh.face.smooth_shading' must be a boolean");
            }
            ef.smooth_shading = sm_val->asBool();
        }
        edit_faces.push_back(std::move(ef));
    }

    scene::EditableMesh result(std::move(edit_verts), std::move(edit_faces));
    auto st = result.validateAndRebuild();
    if (!st) {
        return st.error();
    }
    return core::Result<scene::EditableMesh>::ok(std::move(result));
}

core::Result<std::string> serializeDocumentImpl(
    const ProjectDocument& document,
    std::string_view binary_bundle_uri,
    BinaryAssetBuilder* binary_builder,
    bool pretty) {
    JsonValue root{JsonObject{}};
    root.setKey("format", std::string(ProjectSerializer::kFormatIdentifier));

    JsonValue schema_ver{JsonObject{}};
    schema_ver.setKey("major", static_cast<std::uint64_t>(core::kProjectSchemaMajor));
    schema_ver.setKey("minor", static_cast<std::uint64_t>(core::kProjectSchemaMinor));
    root.setKey("schema_version", std::move(schema_ver));
    root.setKey("coordinate_system", std::string(ProjectSerializer::kCoordinateSystem));

    JsonValue meta{JsonObject{}};
    meta.setKey("project_name", document.metadata.project_name);
    meta.setKey("author", document.metadata.author);
    meta.setKey("description", document.metadata.description);
    meta.setKey("engine_version", document.metadata.engine_version);
    appendExtraFields(meta, document.preserved_fields.metadata_extra);
    root.setKey("metadata", std::move(meta));

    JsonValue scene_json{JsonObject{}};
    scene_json.setKey("name", document.scene.name());
    scene_json.setKey("active_camera_id", document.scene.activeCamera().value);

    // 1. Sort and serialize PBR materials deterministically by MaterialId.
    std::vector<const scene::PbrMaterial*> sorted_materials;
    sorted_materials.reserve(document.scene.materials().size());
    for (const auto& mat : document.scene.materials()) {
        sorted_materials.push_back(&mat);
    }
    std::sort(sorted_materials.begin(), sorted_materials.end(), [](const auto* a, const auto* b) {
        return a->id < b->id;
    });

    JsonArray materials_arr;
    materials_arr.reserve(sorted_materials.size());
    for (const auto* mat : sorted_materials) {
        JsonValue mat_obj{JsonObject{}};
        mat_obj.setKey("id", mat->id.value);
        mat_obj.setKey("name", mat->name);
        mat_obj.setKey("base_color", encodeVec3(mat->base_color));
        mat_obj.setKey("metallic", static_cast<double>(mat->metallic));
        mat_obj.setKey("roughness", static_cast<double>(mat->roughness));
        mat_obj.setKey("ambient_occlusion", static_cast<double>(mat->ambient_occlusion));
        mat_obj.setKey("emissive_color", encodeVec3(mat->emissive_color));
        mat_obj.setKey("emissive_strength", static_cast<double>(mat->emissive_strength));
        mat_obj.setKey("opacity", static_cast<double>(mat->opacity));
        if (const auto it = document.preserved_fields.material_extra.find(mat->id);
            it != document.preserved_fields.material_extra.end()) {
            appendExtraFields(mat_obj, it->second);
        }
        materials_arr.push_back(std::move(mat_obj));
    }
    scene_json.setKey("materials", std::move(materials_arr));

    // 2. Sort and serialize meshes by MeshId.
    std::vector<const scene::MeshAsset*> sorted_meshes;
    sorted_meshes.reserve(document.scene.meshes().size());
    for (const auto& m : document.scene.meshes()) {
        sorted_meshes.push_back(&m);
    }
    std::sort(sorted_meshes.begin(), sorted_meshes.end(), [](const auto* a, const auto* b) {
        return a->id < b->id;
    });

    JsonArray meshes_arr;
    meshes_arr.reserve(sorted_meshes.size());
    for (const auto* mesh : sorted_meshes) {
        JsonValue mesh_obj{JsonObject{}};
        mesh_obj.setKey("id", mesh->id.value);
        mesh_obj.setKey("name", mesh->name);

        if (binary_builder != nullptr) {
            auto hash_res = binary_builder->addMesh(mesh->data);
            if (!hash_res) {
                return hash_res.error();
            }
            JsonValue chunk_ref{JsonObject{}};
            chunk_ref.setKey("uri", std::string(binary_bundle_uri));
            chunk_ref.setKey("content_hash", formatContentHashHex(hash_res.value()));
            mesh_obj.setKey("binary_chunk", std::move(chunk_ref));
        } else {
            JsonArray verts_arr;
            verts_arr.reserve(mesh->data.vertices.size());
            for (const auto& v : mesh->data.vertices) {
                JsonValue v_obj{JsonObject{}};
                v_obj.setKey("position", encodeVec3(v.position));
                v_obj.setKey("normal", encodeVec3(v.normal));
                v_obj.setKey("color", encodeVec3(v.color));
                v_obj.setKey("uv", encodeVec2(v.uv));
                verts_arr.push_back(std::move(v_obj));
            }
            mesh_obj.setKey("vertices", std::move(verts_arr));

            JsonArray idx_arr;
            idx_arr.reserve(mesh->data.indices.size());
            for (const std::uint32_t idx : mesh->data.indices) {
                idx_arr.emplace_back(static_cast<std::uint64_t>(idx));
            }
            mesh_obj.setKey("indices", std::move(idx_arr));
        }

        if (mesh->editable_mesh.has_value()) {
            mesh_obj.setKey("editable_mesh", encodeEditableMesh(mesh->editable_mesh.value()));
        }

        if (const auto it = document.preserved_fields.mesh_extra.find(mesh->id);
            it != document.preserved_fields.mesh_extra.end()) {
            appendExtraFields(mesh_obj, it->second);
        }

        meshes_arr.push_back(std::move(mesh_obj));
    }
    scene_json.setKey("meshes", std::move(meshes_arr));

    // 3. Sort and serialize objects by ObjectId.
    std::vector<const scene::SceneObject*> sorted_objects;
    sorted_objects.reserve(document.scene.objects().size());
    for (const auto& obj : document.scene.objects()) {
        sorted_objects.push_back(&obj);
    }
    std::sort(sorted_objects.begin(), sorted_objects.end(), [](const auto* a, const auto* b) {
        return a->id < b->id;
    });

    JsonArray objects_arr;
    objects_arr.reserve(sorted_objects.size());
    for (const auto* obj : sorted_objects) {
        JsonValue obj_json{JsonObject{}};
        obj_json.setKey("id", obj->id.value);
        obj_json.setKey("name", obj->name);
        obj_json.setKey("visible", obj->visible);
        obj_json.setKey("parent_id", obj->parent_id.value);

        JsonValue tf_json{JsonObject{}};
        tf_json.setKey("position", encodeVec3(obj->local_transform.position));
        tf_json.setKey("rotation", encodeQuat(obj->local_transform.rotation));
        tf_json.setKey("scale", encodeVec3(obj->local_transform.scale));
        obj_json.setKey("local_transform", std::move(tf_json));

        if (obj->mesh_id.has_value()) {
            obj_json.setKey("mesh_id", obj->mesh_id->value);
        }
        if (obj->material_id.has_value()) {
            obj_json.setKey("material_id", obj->material_id->value);
        }

        if (obj->camera.has_value()) {
            const auto& cam = obj->camera.value();
            JsonValue cam_json{JsonObject{}};
            cam_json.setKey(
                "projection",
                cam.projection == scene::ProjectionType::Perspective ? "perspective" : "orthographic");
            cam_json.setKey("fov_y_radians", static_cast<double>(cam.fov_y_radians));
            cam_json.setKey("orthographic_height", static_cast<double>(cam.orthographic_height));
            cam_json.setKey("near_plane", static_cast<double>(cam.near_plane));
            cam_json.setKey("far_plane", static_cast<double>(cam.far_plane));
            obj_json.setKey("camera", std::move(cam_json));
        }

        if (obj->directional_light.has_value()) {
            const auto& light = obj->directional_light.value();
            JsonValue light_json{JsonObject{}};
            light_json.setKey("color", encodeVec3(light.color));
            light_json.setKey("intensity", static_cast<double>(light.intensity));
            obj_json.setKey("directional_light", std::move(light_json));
        }

        if (const auto it = document.preserved_fields.object_extra.find(obj->id);
            it != document.preserved_fields.object_extra.end()) {
            appendExtraFields(obj_json, it->second);
        }

        objects_arr.push_back(std::move(obj_json));
    }
    scene_json.setKey("objects", std::move(objects_arr));
    appendExtraFields(scene_json, document.preserved_fields.scene_extra);
    root.setKey("scene", std::move(scene_json));

    // 4. Serialize Stage 4 editor_state & workspace layout.
    JsonValue ed_json{JsonObject{}};
    ed_json.setKey("edit_mode", std::string(editModeSlug(document.editor_state.edit_mode)));
    ed_json.setKey("gizmo_tool", document.editor_state.gizmo_tool);
    ed_json.setKey("gizmo_space", document.editor_state.gizmo_space);
    ed_json.setKey("selected_object_id", document.editor_state.selected_object_id.value);
    ed_json.setKey("active_material_id", document.editor_state.active_material_id.value);

    JsonValue sel_json{JsonObject{}};
    sel_json.setKey("vertices", encodeUint32Array(document.editor_state.mesh_selection.vertices));
    sel_json.setKey("edges", encodeUint32Array(document.editor_state.mesh_selection.edges));
    sel_json.setKey("faces", encodeUint32Array(document.editor_state.mesh_selection.faces));
    ed_json.setKey("mesh_selection", std::move(sel_json));
    appendExtraFields(ed_json, document.preserved_fields.editor_state_extra);
    root.setKey("editor_state", std::move(ed_json));

    root.setKey(
        "workspace",
        ProjectSerializer::encodeWorkspaceJson(document.workspace, document.preserved_fields.workspace_extra));

    appendExtraFields(root, document.preserved_fields.root_extra);
    return writeJson(root, pretty);
}

core::Result<ProjectDocument> deserializeDocumentImpl(
    std::string_view json_text,
    const std::filesystem::path& base_dir,
    const platform::IFileSystem* file_system) {
    auto parsed = parseJson(json_text);
    if (!parsed) {
        return parsed.error();
    }

    const JsonValue& root = parsed.value();
    if (!root.isObject()) {
        return core::Result<ProjectDocument>::fail(
            core::ErrorCode::ValidationError,
            "Project root JSON value must be an object");
    }

    const JsonValue* format_val = root.findKey("format");
    if (format_val == nullptr || !format_val->isString() ||
        format_val->asString() != ProjectSerializer::kFormatIdentifier) {
        return core::Result<ProjectDocument>::fail(
            core::ErrorCode::ValidationError,
            "Invalid or missing 'format' identifier; expected 'finaleffect.project'");
    }

    const JsonValue* ver_val = root.findKey("schema_version");
    if (ver_val == nullptr || !ver_val->isObject()) {
        return core::Result<ProjectDocument>::fail(
            core::ErrorCode::UnsupportedVersion,
            "Missing or invalid 'schema_version' object");
    }

    auto major_res = decodeUint64(ver_val->findKey("major"), "schema_version.major");
    auto minor_res = decodeUint64(ver_val->findKey("minor"), "schema_version.minor");
    if (!major_res) return major_res.error();
    if (!minor_res) return minor_res.error();

    if (major_res.value() != core::kProjectSchemaMajor || minor_res.value() > core::kProjectSchemaMinor) {
        return core::Result<ProjectDocument>::fail(
            core::ErrorCode::UnsupportedVersion,
            "Unsupported project schema version " + std::to_string(major_res.value()) + "." +
                std::to_string(minor_res.value()) + " (supported: " +
                std::to_string(core::kProjectSchemaMajor) + ".0.." +
                std::to_string(core::kProjectSchemaMinor) + ")");
    }

    const JsonValue* coord_val = root.findKey("coordinate_system");
    if (coord_val == nullptr || !coord_val->isString() ||
        coord_val->asString() != ProjectSerializer::kCoordinateSystem) {
        return core::Result<ProjectDocument>::fail(
            core::ErrorCode::ValidationError,
            "Unsupported or missing 'coordinate_system'; expected 'right-handed-y-up'");
    }

    ProjectDocument doc;
    captureUnknownFields(
        root,
        {"format", "schema_version", "coordinate_system", "metadata", "scene", "editor_state", "workspace"},
        "root",
        doc.preserved_fields.root_extra,
        &doc.load_warnings);

    if (const JsonValue* meta_val = root.findKey("metadata"); meta_val != nullptr) {
        if (!meta_val->isObject()) {
            return core::Result<ProjectDocument>::fail(
                core::ErrorCode::ValidationError,
                "'metadata' must be a JSON object");
        }
        if (const auto* pn = meta_val->findKey("project_name"); pn != nullptr) {
            if (!pn->isString() || pn->asString().empty()) {
                return core::Result<ProjectDocument>::fail(
                    core::ErrorCode::ValidationError,
                    "'metadata.project_name' must be a non-empty string");
            }
            doc.metadata.project_name = pn->asString();
        }
        if (const auto* au = meta_val->findKey("author"); au != nullptr && au->isString()) {
            doc.metadata.author = au->asString();
        }
        if (const auto* ds = meta_val->findKey("description"); ds != nullptr && ds->isString()) {
            doc.metadata.description = ds->asString();
        }
        if (const auto* ev = meta_val->findKey("engine_version"); ev != nullptr && ev->isString()) {
            doc.metadata.engine_version = ev->asString();
        }
        captureUnknownFields(
            *meta_val,
            {"project_name", "author", "description", "engine_version"},
            "metadata",
            doc.preserved_fields.metadata_extra,
            &doc.load_warnings);
    }

    const JsonValue* scene_val = root.findKey("scene");
    if (scene_val == nullptr || !scene_val->isObject()) {
        return core::Result<ProjectDocument>::fail(
            core::ErrorCode::ValidationError,
            "Missing or invalid 'scene' object");
    }

    if (const auto* sn = scene_val->findKey("name"); sn != nullptr) {
        if (!sn->isString() || sn->asString().empty()) {
            return core::Result<ProjectDocument>::fail(
                core::ErrorCode::ValidationError,
                "'scene.name' must be a non-empty string");
        }
        doc.scene.setName(sn->asString());
    }
    captureUnknownFields(
        *scene_val,
        {"name", "active_camera_id", "materials", "meshes", "objects"},
        "scene",
        doc.preserved_fields.scene_extra,
        &doc.load_warnings);

    // 1. Load PBR materials FIRST so mesh face assignments and object assignments validate against them.
    if (const JsonValue* mats_val = scene_val->findKey("materials"); mats_val != nullptr && !mats_val->isNull()) {
        if (!mats_val->isArray()) {
            return core::Result<ProjectDocument>::fail(
                core::ErrorCode::ValidationError,
                "'scene.materials' must be an array");
        }
        for (const auto& mat_json : mats_val->asArray()) {
            if (!mat_json.isObject()) {
                return core::Result<ProjectDocument>::fail(
                    core::ErrorCode::ValidationError,
                    "Each entry in 'scene.materials' must be an object");
            }
            auto id_res = decodeUint64(mat_json.findKey("id"), "material.id");
            if (!id_res) return id_res.error();
            const scene::MaterialId mat_id{id_res.value()};

            const auto* m_name = mat_json.findKey("name");
            if (m_name == nullptr || !m_name->isString() || m_name->asString().empty()) {
                return core::Result<ProjectDocument>::fail(
                    core::ErrorCode::ValidationError,
                    "Material 'name' must be a non-empty string");
            }
            auto base_col = decodeVec3(mat_json.findKey("base_color"), "material.base_color");
            auto metallic = decodeFloat(mat_json.findKey("metallic"), "material.metallic");
            auto roughness = decodeFloat(mat_json.findKey("roughness"), "material.roughness");
            if (!base_col) return base_col.error();
            if (!metallic) return metallic.error();
            if (!roughness) return roughness.error();

            scene::PbrMaterial mat{
                .id = mat_id,
                .name = m_name->asString(),
                .base_color = base_col.value(),
                .metallic = metallic.value(),
                .roughness = roughness.value(),
            };
            if (const auto* ao_val = mat_json.findKey("ambient_occlusion"); ao_val != nullptr) {
                auto ao = decodeFloat(ao_val, "material.ambient_occlusion");
                if (!ao) return ao.error();
                mat.ambient_occlusion = ao.value();
            }
            if (const auto* ec_val = mat_json.findKey("emissive_color"); ec_val != nullptr) {
                auto ec = decodeVec3(ec_val, "material.emissive_color");
                if (!ec) return ec.error();
                mat.emissive_color = ec.value();
            }
            if (const auto* es_val = mat_json.findKey("emissive_strength"); es_val != nullptr) {
                auto es = decodeFloat(es_val, "material.emissive_strength");
                if (!es) return es.error();
                mat.emissive_strength = es.value();
            }
            if (const auto* op_val = mat_json.findKey("opacity"); op_val != nullptr) {
                auto op = decodeFloat(op_val, "material.opacity");
                if (!op) return op.error();
                mat.opacity = op.value();
            }

            auto create_st = doc.scene.createMaterialWithId(mat_id, std::move(mat));
            if (!create_st) {
                return create_st.error();
            }

            JsonObject mat_extra;
            captureUnknownFields(
                mat_json,
                {"id", "name", "base_color", "metallic", "roughness",
                 "ambient_occlusion", "emissive_color", "emissive_strength", "opacity"},
                "scene.materials[" + std::to_string(mat_id.value) + "]",
                mat_extra,
                &doc.load_warnings);
            if (!mat_extra.empty()) {
                doc.preserved_fields.material_extra.emplace(mat_id, std::move(mat_extra));
            }
        }
    }

    // Cache decoded .fe3dbin archives per companion filename during load.
    std::unordered_map<std::string, BinaryAssetArchive> archive_cache;

    // 2. Load and validate meshes (supporting both triangulated v1.0/v1.1 and editable_mesh v1.2).
    if (const JsonValue* meshes_val = scene_val->findKey("meshes"); meshes_val != nullptr) {
        if (!meshes_val->isArray()) {
            return core::Result<ProjectDocument>::fail(
                core::ErrorCode::ValidationError,
                "'scene.meshes' must be an array");
        }
        for (const auto& m_json : meshes_val->asArray()) {
            if (!m_json.isObject()) {
                return core::Result<ProjectDocument>::fail(
                    core::ErrorCode::ValidationError,
                    "Each entry in 'scene.meshes' must be an object");
            }
            auto id_res = decodeUint64(m_json.findKey("id"), "mesh.id");
            if (!id_res) return id_res.error();
            const scene::MeshId mesh_id{id_res.value()};

            const auto* m_name = m_json.findKey("name");
            if (m_name == nullptr || !m_name->isString() || m_name->asString().empty()) {
                return core::Result<ProjectDocument>::fail(
                    core::ErrorCode::ValidationError,
                    "Mesh 'name' must be a non-empty string");
            }

            // Always validate companion binary_chunk or inline triangulated data if present so corrupted archives fail cleanly!
            scene::MeshData mesh_data;
            bool has_tri_payload = false;
            if (const auto* chunk_val = m_json.findKey("binary_chunk");
                chunk_val != nullptr && !chunk_val->isNull()) {
                has_tri_payload = true;
                if (file_system == nullptr) {
                    return core::Result<ProjectDocument>::fail(
                        core::ErrorCode::ValidationError,
                        "Mesh references external 'binary_chunk' (.fe3dbin), which requires loadFromFile");
                }
                if (!chunk_val->isObject()) {
                    return core::Result<ProjectDocument>::fail(
                        core::ErrorCode::ValidationError,
                        "'mesh.binary_chunk' must be an object");
                }
                const auto* uri_val = chunk_val->findKey("uri");
                const auto* hash_val = chunk_val->findKey("content_hash");
                if (uri_val == nullptr || !uri_val->isString() ||
                    hash_val == nullptr || !hash_val->isString()) {
                    return core::Result<ProjectDocument>::fail(
                        core::ErrorCode::ValidationError,
                        "'mesh.binary_chunk' requires string 'uri' and 'content_hash'");
                }

                const std::string& uri = uri_val->asString();
                if (!isSafeCompanionFilename(uri)) {
                    return core::Result<ProjectDocument>::fail(
                        core::ErrorCode::ValidationError,
                        "Unsafe or non-local binary_chunk 'uri' rejected",
                        uri);
                }

                auto hash_res = parseContentHashHex(hash_val->asString());
                if (!hash_res) {
                    return hash_res.error();
                }

                auto cache_it = archive_cache.find(uri);
                if (cache_it == archive_cache.end()) {
                    const std::filesystem::path bin_path =
                        base_dir.empty() ? std::filesystem::path(uri) : (base_dir / uri);
                    auto archive_res = BinaryAssetArchive::loadFromFile(bin_path, *file_system);
                    if (!archive_res) {
                        return archive_res.error();
                    }
                    cache_it = archive_cache.emplace(uri, std::move(archive_res.value())).first;
                }

                auto extracted = cache_it->second.extractMesh(hash_res.value());
                if (!extracted) {
                    return extracted.error();
                }
                mesh_data = std::move(extracted.value());
            } else if (m_json.findKey("vertices") != nullptr || m_json.findKey("indices") != nullptr) {
                has_tri_payload = true;
                const auto* verts_val = m_json.findKey("vertices");
                const auto* idx_val = m_json.findKey("indices");
                if (verts_val == nullptr || !verts_val->isArray() || idx_val == nullptr || !idx_val->isArray()) {
                    return core::Result<ProjectDocument>::fail(
                        core::ErrorCode::ValidationError,
                        "Mesh requires 'vertices' and 'indices' arrays (or 'binary_chunk')");
                }

                mesh_data.vertices.reserve(verts_val->asArray().size());
                for (const auto& v_json : verts_val->asArray()) {
                    if (!v_json.isObject()) {
                        return core::Result<ProjectDocument>::fail(
                            core::ErrorCode::ValidationError,
                            "Mesh vertex entry must be an object");
                    }
                    auto pos = decodeVec3(v_json.findKey("position"), "vertex.position");
                    auto nrm = decodeVec3(v_json.findKey("normal"), "vertex.normal");
                    auto col = decodeVec3(v_json.findKey("color"), "vertex.color");
                    auto uv = decodeVec2(v_json.findKey("uv"), "vertex.uv");
                    if (!pos) return pos.error();
                    if (!nrm) return nrm.error();
                    if (!col) return col.error();
                    if (!uv) return uv.error();
                    mesh_data.vertices.push_back(scene::Vertex{
                        .position = pos.value(),
                        .normal = nrm.value(),
                        .color = col.value(),
                        .uv = uv.value(),
                    });
                }

                mesh_data.indices.reserve(idx_val->asArray().size());
                for (const auto& idx_entry : idx_val->asArray()) {
                    auto i_res = decodeUint64(&idx_entry, "mesh.indices[]");
                    if (!i_res) return i_res.error();
                    if (i_res.value() > static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max())) {
                        return core::Result<ProjectDocument>::fail(
                            core::ErrorCode::ValidationError,
                            "Mesh index exceeds uint32_t range");
                    }
                    mesh_data.indices.push_back(static_cast<std::uint32_t>(i_res.value()));
                }
            }

            if (const auto* em_val = m_json.findKey("editable_mesh"); em_val != nullptr && !em_val->isNull()) {
                auto em_res = decodeEditableMesh(*em_val);
                if (!em_res) {
                    return em_res.error();
                }
                auto reg_st = doc.scene.registerEditableMeshWithId(
                    mesh_id,
                    m_name->asString(),
                    std::move(em_res.value()));
                if (!reg_st) {
                    return reg_st.error();
                }
            } else {
                if (!has_tri_payload) {
                    return core::Result<ProjectDocument>::fail(
                        core::ErrorCode::ValidationError,
                        "Mesh requires 'vertices'+'indices', 'binary_chunk', or 'editable_mesh'");
                }
                auto reg_status = doc.scene.registerMeshWithId(
                    mesh_id,
                    m_name->asString(),
                    std::move(mesh_data));
                if (!reg_status) {
                    return reg_status.error();
                }
            }

            JsonObject mesh_extra;
            captureUnknownFields(
                m_json,
                {"id", "name", "vertices", "indices", "binary_chunk", "editable_mesh"},
                "scene.meshes[" + std::to_string(mesh_id.value) + "]",
                mesh_extra,
                &doc.load_warnings);
            if (!mesh_extra.empty()) {
                doc.preserved_fields.mesh_extra.emplace(mesh_id, std::move(mesh_extra));
            }
        }
    }

    // 3. Load objects in two passes so parent/child order in JSON is order-independent and cycle-checked.
    const JsonValue* objects_val = scene_val->findKey("objects");
    if (objects_val == nullptr || !objects_val->isArray()) {
        return core::Result<ProjectDocument>::fail(
            core::ErrorCode::ValidationError,
            "'scene.objects' must be an array");
    }

    struct PendingParentLink {
        scene::ObjectId child;
        scene::ObjectId parent;
    };
    std::vector<PendingParentLink> pending_links;
    pending_links.reserve(objects_val->asArray().size());

    for (const auto& obj_json : objects_val->asArray()) {
        if (!obj_json.isObject()) {
            return core::Result<ProjectDocument>::fail(
                core::ErrorCode::ValidationError,
                "Each entry in 'scene.objects' must be an object");
        }

        auto id_res = decodeUint64(obj_json.findKey("id"), "object.id");
        if (!id_res) return id_res.error();
        const scene::ObjectId obj_id{id_res.value()};

        const auto* name_val = obj_json.findKey("name");
        if (name_val == nullptr || !name_val->isString() || name_val->asString().empty()) {
            return core::Result<ProjectDocument>::fail(
                core::ErrorCode::ValidationError,
                "Scene object 'name' must be a non-empty string");
        }

        auto create_st = doc.scene.createObjectWithId(obj_id, name_val->asString());
        if (!create_st) {
            return create_st.error();
        }

        if (const auto* vis_val = obj_json.findKey("visible"); vis_val != nullptr) {
            if (!vis_val->isBool()) {
                return core::Result<ProjectDocument>::fail(
                    core::ErrorCode::ValidationError,
                    "'object.visible' must be a boolean");
            }
            auto st = doc.scene.setObjectVisible(obj_id, vis_val->asBool());
            if (!st) return st.error();
        }

        if (const auto* parent_val = obj_json.findKey("parent_id"); parent_val != nullptr && !parent_val->isNull()) {
            auto p_res = decodeUint64(parent_val, "object.parent_id");
            if (!p_res) return p_res.error();
            if (p_res.value() != 0) {
                pending_links.push_back(PendingParentLink{obj_id, scene::ObjectId{p_res.value()}});
            }
        }

        const auto* tf_val = obj_json.findKey("local_transform");
        if (tf_val == nullptr || !tf_val->isObject()) {
            return core::Result<ProjectDocument>::fail(
                core::ErrorCode::ValidationError,
                "Scene object requires 'local_transform' object");
        }
        auto pos = decodeVec3(tf_val->findKey("position"), "local_transform.position");
        auto rot = decodeQuat(tf_val->findKey("rotation"), "local_transform.rotation");
        auto scl = decodeVec3(tf_val->findKey("scale"), "local_transform.scale");
        if (!pos) return pos.error();
        if (!rot) return rot.error();
        if (!scl) return scl.error();

        math::Transform tf{
            .position = pos.value(),
            .rotation = rot.value(),
            .scale = scl.value(),
        };
        auto tf_st = doc.scene.setObjectTransform(obj_id, tf);
        if (!tf_st) return tf_st.error();

        if (const auto* mesh_id_val = obj_json.findKey("mesh_id");
            mesh_id_val != nullptr && !mesh_id_val->isNull()) {
            auto m_res = decodeUint64(mesh_id_val, "object.mesh_id");
            if (!m_res) return m_res.error();
            if (m_res.value() == 0) {
                return core::Result<ProjectDocument>::fail(
                    core::ErrorCode::ValidationError,
                    "'object.mesh_id' cannot be 0");
            }
            auto m_st = doc.scene.assignMesh(obj_id, scene::MeshId{m_res.value()});
            if (!m_st) {
                return core::Result<ProjectDocument>::fail(
                    core::ErrorCode::ValidationError,
                    m_st.error().message,
                    m_st.error().context);
            }
        }

        if (const auto* mat_id_val = obj_json.findKey("material_id");
            mat_id_val != nullptr && !mat_id_val->isNull()) {
            auto mat_res = decodeUint64(mat_id_val, "object.material_id");
            if (!mat_res) return mat_res.error();
            if (mat_res.value() != 0) {
                auto mat_st = doc.scene.assignMaterial(obj_id, scene::MaterialId{mat_res.value()});
                if (!mat_st) {
                    return core::Result<ProjectDocument>::fail(
                        core::ErrorCode::ValidationError,
                        mat_st.error().message,
                        mat_st.error().context);
                }
            }
        }

        if (const auto* cam_val = obj_json.findKey("camera"); cam_val != nullptr && !cam_val->isNull()) {
            if (!cam_val->isObject()) {
                return core::Result<ProjectDocument>::fail(
                    core::ErrorCode::ValidationError,
                    "'object.camera' must be an object");
            }
            scene::CameraComponent cam{};
            const auto* proj_val = cam_val->findKey("projection");
            if (proj_val == nullptr || !proj_val->isString()) {
                return core::Result<ProjectDocument>::fail(
                    core::ErrorCode::ValidationError,
                    "'camera.projection' must be 'perspective' or 'orthographic'");
            }
            if (proj_val->asString() == "perspective") {
                cam.projection = scene::ProjectionType::Perspective;
            } else if (proj_val->asString() == "orthographic") {
                cam.projection = scene::ProjectionType::Orthographic;
            } else {
                return core::Result<ProjectDocument>::fail(
                    core::ErrorCode::ValidationError,
                    "Unknown camera projection type: " + proj_val->asString());
            }

            auto fov = decodeFloat(cam_val->findKey("fov_y_radians"), "camera.fov_y_radians");
            auto oh = decodeFloat(cam_val->findKey("orthographic_height"), "camera.orthographic_height");
            auto np = decodeFloat(cam_val->findKey("near_plane"), "camera.near_plane");
            auto fp = decodeFloat(cam_val->findKey("far_plane"), "camera.far_plane");
            if (!fov) return fov.error();
            if (!oh) return oh.error();
            if (!np) return np.error();
            if (!fp) return fp.error();

            cam.fov_y_radians = fov.value();
            cam.orthographic_height = oh.value();
            cam.near_plane = np.value();
            cam.far_plane = fp.value();

            auto c_st = doc.scene.assignCamera(obj_id, cam);
            if (!c_st) return c_st.error();
        }

        if (const auto* light_val = obj_json.findKey("directional_light");
            light_val != nullptr && !light_val->isNull()) {
            if (!light_val->isObject()) {
                return core::Result<ProjectDocument>::fail(
                    core::ErrorCode::ValidationError,
                    "'object.directional_light' must be an object");
            }
            auto col = decodeVec3(light_val->findKey("color"), "directional_light.color");
            auto inten = decodeFloat(light_val->findKey("intensity"), "directional_light.intensity");
            if (!col) return col.error();
            if (!inten) return inten.error();

            scene::DirectionalLightComponent light{
                .color = col.value(),
                .intensity = inten.value(),
            };
            auto l_st = doc.scene.assignDirectionalLight(obj_id, light);
            if (!l_st) return l_st.error();
        }

        JsonObject obj_extra;
        captureUnknownFields(
            obj_json,
            {"id", "name", "visible", "parent_id", "local_transform",
             "mesh_id", "material_id", "camera", "directional_light"},
            "scene.objects[" + std::to_string(obj_id.value) + "]",
            obj_extra,
            &doc.load_warnings);
        if (!obj_extra.empty()) {
            doc.preserved_fields.object_extra.emplace(obj_id, std::move(obj_extra));
        }
    }

    for (const auto& link : pending_links) {
        auto link_st = doc.scene.setParent(link.child, link.parent);
        if (!link_st) {
            return core::Result<ProjectDocument>::fail(
                link_st.error().code == core::ErrorCode::HierarchyCycle
                    ? core::ErrorCode::HierarchyCycle
                    : core::ErrorCode::ValidationError,
                link_st.error().message,
                "child=" + std::to_string(link.child.value) + ", parent=" + std::to_string(link.parent.value));
        }
    }

    if (const auto* active_cam_val = scene_val->findKey("active_camera_id");
        active_cam_val != nullptr && !active_cam_val->isNull()) {
        auto cam_id_res = decodeUint64(active_cam_val, "scene.active_camera_id");
        if (!cam_id_res) return cam_id_res.error();
        if (cam_id_res.value() != 0) {
            auto cam_st = doc.scene.setActiveCamera(scene::ObjectId{cam_id_res.value()});
            if (!cam_st) {
                return core::Result<ProjectDocument>::fail(
                    core::ErrorCode::ValidationError,
                    cam_st.error().message);
            }
        }
    }

    // 4. Decode optional Stage 4 editor_state.
    if (const JsonValue* ed_val = root.findKey("editor_state"); ed_val != nullptr && !ed_val->isNull()) {
        if (!ed_val->isObject()) {
            return core::Result<ProjectDocument>::fail(
                core::ErrorCode::ValidationError,
                "'editor_state' must be an object");
        }
        if (const auto* em_s = ed_val->findKey("edit_mode"); em_s != nullptr) {
            if (!em_s->isString()) {
                return core::Result<ProjectDocument>::fail(
                    core::ErrorCode::ValidationError,
                    "'editor_state.edit_mode' must be a string");
            }
            auto em_res = parseEditModeSlug(em_s->asString());
            if (!em_res) return em_res.error();
            doc.editor_state.edit_mode = em_res.value();
        }
        if (const auto* gt = ed_val->findKey("gizmo_tool"); gt != nullptr) {
            if (!gt->isString() ||
                (gt->asString() != "translate" && gt->asString() != "rotate" && gt->asString() != "scale")) {
                return core::Result<ProjectDocument>::fail(
                    core::ErrorCode::ValidationError,
                    "'editor_state.gizmo_tool' must be 'translate', 'rotate', or 'scale'");
            }
            doc.editor_state.gizmo_tool = gt->asString();
        }
        if (const auto* gs = ed_val->findKey("gizmo_space"); gs != nullptr) {
            if (!gs->isString() || (gs->asString() != "world" && gs->asString() != "local")) {
                return core::Result<ProjectDocument>::fail(
                    core::ErrorCode::ValidationError,
                    "'editor_state.gizmo_space' must be 'world' or 'local'");
            }
            doc.editor_state.gizmo_space = gs->asString();
        }
        if (const auto* so = ed_val->findKey("selected_object_id"); so != nullptr && !so->isNull()) {
            auto so_res = decodeUint64(so, "editor_state.selected_object_id");
            if (!so_res) return so_res.error();
            const scene::ObjectId cand{so_res.value()};
            if (!cand.isValid() || doc.scene.findObject(cand) != nullptr) {
                doc.editor_state.selected_object_id = cand;
            }
        }
        if (const auto* am = ed_val->findKey("active_material_id"); am != nullptr && !am->isNull()) {
            auto am_res = decodeUint64(am, "editor_state.active_material_id");
            if (!am_res) return am_res.error();
            const scene::MaterialId cand{am_res.value()};
            if (!cand.isValid() || doc.scene.findMaterial(cand) != nullptr) {
                doc.editor_state.active_material_id = cand;
            }
        }
        if (const auto* ms = ed_val->findKey("mesh_selection"); ms != nullptr && !ms->isNull()) {
            if (!ms->isObject()) {
                return core::Result<ProjectDocument>::fail(
                    core::ErrorCode::ValidationError,
                    "'editor_state.mesh_selection' must be an object");
            }
            if (const auto* sv = ms->findKey("vertices"); sv != nullptr) {
                auto v_res = decodeUint32Array(sv, "mesh_selection.vertices");
                if (!v_res) return v_res.error();
                doc.editor_state.mesh_selection.vertices = std::move(v_res.value());
            }
            if (const auto* se = ms->findKey("edges"); se != nullptr) {
                auto e_res = decodeUint32Array(se, "mesh_selection.edges");
                if (!e_res) return e_res.error();
                doc.editor_state.mesh_selection.edges = std::move(e_res.value());
            }
            if (const auto* sf = ms->findKey("faces"); sf != nullptr) {
                auto f_res = decodeUint32Array(sf, "mesh_selection.faces");
                if (!f_res) return f_res.error();
                doc.editor_state.mesh_selection.faces = std::move(f_res.value());
            }

            // Sanitize element selection against selected object's EditableMesh counts if present.
            if (const auto* sel_obj = doc.scene.findObject(doc.editor_state.selected_object_id);
                sel_obj != nullptr && sel_obj->mesh_id.has_value()) {
                if (const auto* ma = doc.scene.findMesh(sel_obj->mesh_id.value());
                    ma != nullptr && ma->editable_mesh.has_value()) {
                    doc.editor_state.mesh_selection.sanitizeForCounts(
                        ma->editable_mesh->vertexCount(),
                        ma->editable_mesh->edgeCount(),
                        ma->editable_mesh->faceCount());
                }
            }
        }
        captureUnknownFields(
            *ed_val,
            {"edit_mode", "gizmo_tool", "gizmo_space", "selected_object_id", "active_material_id", "mesh_selection"},
            "editor_state",
            doc.preserved_fields.editor_state_extra,
            &doc.load_warnings);
    }

    // 5. Decode optional Stage 4 workspace state.
    if (const JsonValue* ws_val = root.findKey("workspace"); ws_val != nullptr && !ws_val->isNull()) {
        auto ws_res = ProjectSerializer::decodeWorkspaceJson(
            *ws_val,
            &doc.preserved_fields.workspace_extra,
            &doc.load_warnings);
        if (!ws_res) {
            return ws_res.error();
        }
        doc.workspace = std::move(ws_res.value());
    }

    return core::Result<ProjectDocument>::ok(std::move(doc));
}

} // namespace

JsonValue ProjectSerializer::encodeWorkspaceJson(
    const WorkspaceState& state,
    const JsonObject& extra_fields) {
    JsonValue ws{JsonObject{}};
    ws.setKey("layout_name", state.layout_name);
    ws.setKey("focused_panel", std::string(workspacePanelSlug(state.focused_panel)));
    ws.setKey("left_dock_width_ratio", static_cast<double>(state.left_dock_width_ratio));
    ws.setKey("right_dock_width_ratio", static_cast<double>(state.right_dock_width_ratio));
    ws.setKey("bottom_dock_height_ratio", static_cast<double>(state.bottom_dock_height_ratio));

    JsonArray panels_arr;
    panels_arr.reserve(state.panels.size());
    for (const auto& p : state.panels) {
        JsonValue p_obj{JsonObject{}};
        p_obj.setKey("id", std::string(workspacePanelSlug(p.id)));
        p_obj.setKey("visible", p.visible);
        p_obj.setKey("dock_region", std::string(dockRegionSlug(p.dock_region)));
        p_obj.setKey("tab_order", static_cast<std::uint64_t>(p.tab_order));
        panels_arr.push_back(std::move(p_obj));
    }
    ws.setKey("panels", std::move(panels_arr));
    appendExtraFields(ws, extra_fields);
    return ws;
}

core::Result<WorkspaceState> ProjectSerializer::decodeWorkspaceJson(
    const JsonValue& json,
    JsonObject* out_extra_fields,
    std::vector<std::string>* out_warnings) {
    if (!json.isObject()) {
        return core::Result<WorkspaceState>::fail(
            core::ErrorCode::ValidationError,
            "'workspace' must be a JSON object");
    }

    WorkspaceState ws{};
    if (const auto* ln = json.findKey("layout_name"); ln != nullptr) {
        if (!ln->isString() || ln->asString().empty()) {
            return core::Result<WorkspaceState>::fail(
                core::ErrorCode::ValidationError,
                "'workspace.layout_name' must be a non-empty string");
        }
        ws.layout_name = ln->asString();
    }
    if (const auto* fp = json.findKey("focused_panel"); fp != nullptr) {
        if (!fp->isString()) {
            return core::Result<WorkspaceState>::fail(
                core::ErrorCode::ValidationError,
                "'workspace.focused_panel' must be a string");
        }
        const auto parsed_fp = parseWorkspacePanelSlug(fp->asString());
        if (!parsed_fp.has_value()) {
            return core::Result<WorkspaceState>::fail(
                core::ErrorCode::ValidationError,
                "Unknown workspace panel id: " + fp->asString());
        }
        ws.focused_panel = parsed_fp.value();
    }
    if (const auto* lw = json.findKey("left_dock_width_ratio"); lw != nullptr) {
        auto v = decodeFloat(lw, "workspace.left_dock_width_ratio");
        if (!v) return v.error();
        ws.left_dock_width_ratio = std::clamp(v.value(), 0.08f, 0.45f);
    }
    if (const auto* rw = json.findKey("right_dock_width_ratio"); rw != nullptr) {
        auto v = decodeFloat(rw, "workspace.right_dock_width_ratio");
        if (!v) return v.error();
        ws.right_dock_width_ratio = std::clamp(v.value(), 0.08f, 0.45f);
    }
    if (const auto* bh = json.findKey("bottom_dock_height_ratio"); bh != nullptr) {
        auto v = decodeFloat(bh, "workspace.bottom_dock_height_ratio");
        if (!v) return v.error();
        ws.bottom_dock_height_ratio = std::clamp(v.value(), 0.08f, 0.50f);
    }

    if (const auto* panels_val = json.findKey("panels"); panels_val != nullptr) {
        if (!panels_val->isArray()) {
            return core::Result<WorkspaceState>::fail(
                core::ErrorCode::ValidationError,
                "'workspace.panels' must be an array");
        }
        for (const auto& p_json : panels_val->asArray()) {
            if (!p_json.isObject()) {
                return core::Result<WorkspaceState>::fail(
                    core::ErrorCode::ValidationError,
                    "Each entry in 'workspace.panels' must be an object");
            }
            const auto* id_val = p_json.findKey("id");
            if (id_val == nullptr || !id_val->isString()) {
                return core::Result<WorkspaceState>::fail(
                    core::ErrorCode::ValidationError,
                    "'workspace.panels[].id' must be a string");
            }
            const auto pid = parseWorkspacePanelSlug(id_val->asString());
            if (!pid.has_value()) {
                return core::Result<WorkspaceState>::fail(
                    core::ErrorCode::ValidationError,
                    "Unknown workspace panel slug: " + id_val->asString());
            }
            auto& slot = ws.panels[static_cast<std::size_t>(pid.value())];
            if (const auto* vis = p_json.findKey("visible"); vis != nullptr) {
                if (!vis->isBool()) {
                    return core::Result<WorkspaceState>::fail(
                        core::ErrorCode::ValidationError,
                        "'workspace.panels[].visible' must be a boolean");
                }
                slot.visible = vis->asBool();
            }
            if (const auto* dr = p_json.findKey("dock_region"); dr != nullptr) {
                if (!dr->isString()) {
                    return core::Result<WorkspaceState>::fail(
                        core::ErrorCode::ValidationError,
                        "'workspace.panels[].dock_region' must be a string");
                }
                const auto reg = parseDockRegionSlug(dr->asString());
                if (!reg.has_value()) {
                    return core::Result<WorkspaceState>::fail(
                        core::ErrorCode::ValidationError,
                        "Unknown dock region: " + dr->asString());
                }
                slot.dock_region = reg.value();
            }
            if (const auto* ord = p_json.findKey("tab_order"); ord != nullptr) {
                auto o_res = decodeUint64(ord, "workspace.panels[].tab_order");
                if (!o_res) return o_res.error();
                slot.tab_order = static_cast<std::uint32_t>(std::min<std::uint64_t>(o_res.value(), 1000u));
            }
        }
    }

    if (out_extra_fields != nullptr) {
        captureUnknownFields(
            json,
            {"layout_name", "focused_panel", "left_dock_width_ratio",
             "right_dock_width_ratio", "bottom_dock_height_ratio", "panels"},
            "workspace",
            *out_extra_fields,
            out_warnings);
    }

    return core::Result<WorkspaceState>::ok(std::move(ws));
}

core::Result<std::string> ProjectSerializer::serializeToString(const ProjectDocument& document, bool pretty) {
    return serializeDocumentImpl(document, {}, nullptr, pretty);
}

core::Result<ProjectDocument> ProjectSerializer::deserializeFromString(std::string_view json_text) {
    return deserializeDocumentImpl(json_text, {}, nullptr);
}

core::Status ProjectSerializer::saveToFile(
    const ProjectDocument& document,
    const std::filesystem::path& path,
    platform::IFileSystem& file_system) {
    auto json_res = serializeToString(document, true);
    if (!json_res) {
        return json_res.error();
    }
    return file_system.writeTextFile(path, json_res.value());
}

core::Status ProjectSerializer::saveBundleToFile(
    const ProjectDocument& document,
    const std::filesystem::path& fe3d_path,
    platform::IFileSystem& file_system) {
    const std::string stem = fe3d_path.stem().string();
    const std::string bin_filename = (stem.empty() ? std::string("project") : stem) + ".fe3dbin";
    const std::filesystem::path bin_path = fe3d_path.parent_path().empty()
                                               ? std::filesystem::path(bin_filename)
                                               : (fe3d_path.parent_path() / bin_filename);

    BinaryAssetBuilder builder;
    auto json_res = serializeDocumentImpl(document, bin_filename, &builder, true);
    if (!json_res) {
        return json_res.error();
    }

    auto bin_status = builder.saveToFile(bin_path, file_system);
    if (!bin_status) {
        return bin_status.error();
    }

    return file_system.writeTextFile(fe3d_path, json_res.value());
}

core::Result<ProjectDocument> ProjectSerializer::loadFromFile(
    const std::filesystem::path& path,
    const platform::IFileSystem& file_system) {
    auto text_res = file_system.readTextFile(path);
    if (!text_res) {
        return text_res.error();
    }
    return deserializeDocumentImpl(text_res.value(), path.parent_path(), &file_system);
}

} // namespace finaleffect::project
