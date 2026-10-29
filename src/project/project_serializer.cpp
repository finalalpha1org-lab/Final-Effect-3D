// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/project/project.hpp"
#include "finaleffect/project/json.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

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

} // namespace

core::Result<std::string> ProjectSerializer::serializeToString(const ProjectDocument& document, bool pretty) {
    JsonValue root{JsonObject{}};
    root.setKey("format", std::string(kFormatIdentifier));

    JsonValue schema_ver{JsonObject{}};
    schema_ver.setKey("major", static_cast<std::uint64_t>(core::kProjectSchemaMajor));
    schema_ver.setKey("minor", static_cast<std::uint64_t>(core::kProjectSchemaMinor));
    root.setKey("schema_version", std::move(schema_ver));
    root.setKey("coordinate_system", std::string(kCoordinateSystem));

    JsonValue meta{JsonObject{}};
    meta.setKey("project_name", document.metadata.project_name);
    meta.setKey("author", document.metadata.author);
    meta.setKey("description", document.metadata.description);
    meta.setKey("engine_version", document.metadata.engine_version);
    root.setKey("metadata", std::move(meta));

    JsonValue scene_json{JsonObject{}};
    scene_json.setKey("name", document.scene.name());
    scene_json.setKey("active_camera_id", document.scene.activeCamera().value);

    // Sort meshes by MeshId for deterministic output.
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
        meshes_arr.push_back(std::move(mesh_obj));
    }
    scene_json.setKey("meshes", std::move(meshes_arr));

    // Sort objects by ObjectId for deterministic output.
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

        objects_arr.push_back(std::move(obj_json));
    }
    scene_json.setKey("objects", std::move(objects_arr));
    root.setKey("scene", std::move(scene_json));

    return writeJson(root, pretty);
}

core::Result<ProjectDocument> ProjectSerializer::deserializeFromString(std::string_view json_text) {
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
    if (format_val == nullptr || !format_val->isString() || format_val->asString() != kFormatIdentifier) {
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
    if (coord_val == nullptr || !coord_val->isString() || coord_val->asString() != kCoordinateSystem) {
        return core::Result<ProjectDocument>::fail(
            core::ErrorCode::ValidationError,
            "Unsupported or missing 'coordinate_system'; expected 'right-handed-y-up'");
    }

    ProjectDocument doc;

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

    // 1. Load and validate meshes.
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

            const auto* m_name = m_json.findKey("name");
            if (m_name == nullptr || !m_name->isString() || m_name->asString().empty()) {
                return core::Result<ProjectDocument>::fail(
                    core::ErrorCode::ValidationError,
                    "Mesh 'name' must be a non-empty string");
            }

            const auto* verts_val = m_json.findKey("vertices");
            const auto* idx_val = m_json.findKey("indices");
            if (verts_val == nullptr || !verts_val->isArray() || idx_val == nullptr || !idx_val->isArray()) {
                return core::Result<ProjectDocument>::fail(
                    core::ErrorCode::ValidationError,
                    "Mesh requires 'vertices' and 'indices' arrays");
            }

            scene::MeshData mesh_data;
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

            auto reg_status = doc.scene.registerMeshWithId(
                scene::MeshId{id_res.value()},
                m_name->asString(),
                std::move(mesh_data));
            if (!reg_status) {
                return reg_status.error();
            }
        }
    }

    // 2. Load objects in two passes so parent/child order in JSON is order-independent and cycle-checked.
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

    return core::Result<ProjectDocument>::ok(std::move(doc));
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

core::Result<ProjectDocument> ProjectSerializer::loadFromFile(
    const std::filesystem::path& path,
    const platform::IFileSystem& file_system) {
    auto text_res = file_system.readTextFile(path);
    if (!text_res) {
        return text_res.error();
    }
    return deserializeFromString(text_res.value());
}

} // namespace finaleffect::project
