// SPDX-License-Identifier: GPL-3.0-only
#include "finaleffect/project/ProjectFile.hpp"

#include <cmath>
#include <cstddef>
#include <string>
#include <utility>

#include "finaleffect/core/Version.hpp"

namespace fe::project {

using core::JsonValue;

namespace {

core::Error documentError(core::ErrorCode code, std::string message) {
    return core::makeError(code, std::move(message), "project");
}

core::JsonValue vec3ToJson(const math::Vec3& value) {
    core::JsonValue array = core::JsonValue::array();
    array.append(JsonValue(static_cast<double>(value.x)));
    array.append(JsonValue(static_cast<double>(value.y)));
    array.append(JsonValue(static_cast<double>(value.z)));
    return array;
}

core::Result<math::Vec3> jsonToVec3(const core::JsonValue* value, std::string_view context) {
    if (value == nullptr || !value->isArray() || value->items().size() != 3U) {
        return documentError(core::ErrorCode::ValidationFailed,
                             std::string(context) + " must be an array of three numbers");
    }
    double components[3] = {0.0, 0.0, 0.0};
    for (std::size_t i = 0; i < 3U; ++i) {
        if (!value->items()[i].numberIfFinite(components[i])) {
            return documentError(core::ErrorCode::ValidationFailed,
                                 std::string(context) + " contains a missing or non-finite component");
        }
    }
    return math::Vec3(static_cast<math::Scalar>(components[0]),
                      static_cast<math::Scalar>(components[1]),
                      static_cast<math::Scalar>(components[2]));
}

core::JsonValue transformToJson(const math::Transform& transform) {
    core::JsonValue object = core::JsonValue::object();
    object.set("position", vec3ToJson(transform.position));
    object.set("scale", vec3ToJson(transform.scale));

    core::JsonValue rotation = core::JsonValue::array();
    rotation.append(JsonValue(static_cast<double>(transform.rotation.x)));
    rotation.append(JsonValue(static_cast<double>(transform.rotation.y)));
    rotation.append(JsonValue(static_cast<double>(transform.rotation.z)));
    rotation.append(JsonValue(static_cast<double>(transform.rotation.w)));
    object.set("rotation", std::move(rotation));
    return object;
}

core::Result<math::Transform> jsonToTransform(const core::JsonValue* value) {
    math::Transform transform = math::Transform::identity();
    if (value == nullptr || value->isNull()) {
        return transform;
    }
    if (!value->isObject()) {
        return documentError(core::ErrorCode::ValidationFailed, "transform must be an object");
    }

    if (const core::JsonValue* position = value->find("position"); position != nullptr) {
        core::Result<math::Vec3> parsed = jsonToVec3(position, "transform.position");
        if (!parsed.ok()) {
            return parsed.error();
        }
        transform.position = parsed.value();
    }
    if (const core::JsonValue* scale = value->find("scale"); scale != nullptr) {
        core::Result<math::Vec3> parsed = jsonToVec3(scale, "transform.scale");
        if (!parsed.ok()) {
            return parsed.error();
        }
        transform.scale = parsed.value();
    }
    if (const core::JsonValue* rotation = value->find("rotation"); rotation != nullptr) {
        if (!rotation->isArray() || rotation->items().size() != 4U) {
            return documentError(core::ErrorCode::ValidationFailed,
                                 "transform.rotation must be an array of four numbers (x, y, z, w)");
        }
        double components[4] = {0.0, 0.0, 0.0, 1.0};
        for (std::size_t i = 0; i < 4U; ++i) {
            if (!rotation->items()[i].numberIfFinite(components[i])) {
                return documentError(core::ErrorCode::ValidationFailed,
                                     "transform.rotation contains a missing or non-finite component");
            }
        }
        transform.rotation = math::Quat(static_cast<math::Scalar>(components[0]),
                                        static_cast<math::Scalar>(components[1]),
                                        static_cast<math::Scalar>(components[2]),
                                        static_cast<math::Scalar>(components[3]));
    }
    return transform;
}

core::Result<std::uint64_t> readUnsigned(const core::JsonValue* value, std::string_view context) {
    double parsed = 0.0;
    if (value == nullptr || !value->numberIfFinite(parsed) || parsed < 0.0 ||
        std::floor(parsed) != parsed || parsed > 9007199254740992.0) {
        return documentError(core::ErrorCode::ValidationFailed,
                             std::string(context) + " must be a non-negative integer");
    }
    return static_cast<std::uint64_t>(parsed);
}

core::JsonValue objectToJson(const scene::SceneObject& object) {
    core::JsonValue json = core::JsonValue::object();
    json.set("id", JsonValue(static_cast<double>(object.id.value)));
    json.set("name", JsonValue(object.name));
    json.set("parent", JsonValue(static_cast<double>(object.parent.value)));
    json.set("mesh", JsonValue(object.mesh));
    json.set("visible", JsonValue(object.visible));
    json.set("color", vec3ToJson(object.color));
    json.set("transform", transformToJson(object.localTransform));
    return json;
}

core::Result<scene::SceneObject> jsonToObject(const core::JsonValue& json, std::size_t index) {
    const std::string context = "scene.objects[" + std::to_string(index) + "]";
    if (!json.isObject()) {
        return documentError(core::ErrorCode::ValidationFailed, context + " must be an object");
    }

    scene::SceneObject object;
    core::Result<std::uint64_t> id = readUnsigned(json.find("id"), context + ".id");
    if (!id.ok()) {
        return id.error();
    }
    if (id.value() == 0) {
        return documentError(core::ErrorCode::ValidationFailed, context + ".id must be greater than zero");
    }
    object.id = scene::ObjectId{id.value()};

    const core::JsonValue* name = json.find("name");
    if (name == nullptr || !name->isString() || name->asString().empty()) {
        return documentError(core::ErrorCode::ValidationFailed,
                             context + ".name must be a non-empty string");
    }
    object.name = name->asString();

    const core::JsonValue* parent = json.find("parent");
    if (parent != nullptr && !parent->isNull()) {
        core::Result<std::uint64_t> parentId = readUnsigned(parent, context + ".parent");
        if (!parentId.ok()) {
            return parentId.error();
        }
        object.parent = scene::ObjectId{parentId.value()};
    }

    if (const core::JsonValue* mesh = json.find("mesh"); mesh != nullptr && !mesh->isNull()) {
        if (!mesh->isString()) {
            return documentError(core::ErrorCode::ValidationFailed, context + ".mesh must be a string");
        }
        object.mesh = mesh->asString();
    }

    if (const core::JsonValue* visible = json.find("visible"); visible != nullptr) {
        if (!visible->isBoolean()) {
            return documentError(core::ErrorCode::ValidationFailed, context + ".visible must be a boolean");
        }
        object.visible = visible->asBoolean();
    }

    if (const core::JsonValue* color = json.find("color"); color != nullptr) {
        core::Result<math::Vec3> parsed = jsonToVec3(color, context + ".color");
        if (!parsed.ok()) {
            return parsed.error();
        }
        object.color = parsed.value();
    }

    core::Result<math::Transform> transform = jsonToTransform(json.find("transform"));
    if (!transform.ok()) {
        return transform.error();
    }
    object.localTransform = transform.value();

    if (!object.localTransform.isFinite()) {
        return documentError(core::ErrorCode::ValidationFailed, context + " has a non-finite transform");
    }
    return object;
}

}  // namespace

ProjectDocument ProjectDocument::fromScene(const scene::Scene& sceneValue) {
    ProjectDocument document;
    document.formatVersion = kProjectFormatVersion;
    document.applicationVersion = std::string(core::fullVersionString());
    document.objects = sceneValue.objects();
    return document;
}

core::Result<scene::Scene> ProjectDocument::toScene() const {
    if (formatVersion < kMinimumSupportedFormatVersion || formatVersion > kProjectFormatVersion) {
        return documentError(core::ErrorCode::UnsupportedVersion,
                             "project format version " + std::to_string(formatVersion) +
                                 " is not supported by this build (supported: " +
                                 std::to_string(kMinimumSupportedFormatVersion) + ".." +
                                 std::to_string(kProjectFormatVersion) + ")");
    }
    core::Result<scene::Scene> result = scene::Scene::fromObjects(objects);
    if (!result.ok()) {
        return documentError(result.error().code(), result.error().message());
    }
    return result.value();
}

core::Result<std::string> ProjectDocument::toJsonText() const {
    for (const scene::SceneObject& object : objects) {
        if (!object.id.isValid()) {
            return documentError(core::ErrorCode::ValidationFailed,
                                 "object '" + object.name + "' has no id and cannot be saved");
        }
        if (object.name.empty()) {
            return documentError(core::ErrorCode::ValidationFailed, "an object has an empty name");
        }
        if (!object.localTransform.isFinite()) {
            return documentError(core::ErrorCode::ValidationFailed,
                                 "object '" + object.name + "' has a non-finite transform and cannot be saved");
        }
        if (!object.color.isFinite()) {
            return documentError(core::ErrorCode::ValidationFailed,
                                 "object '" + object.name + "' has a non-finite colour and cannot be saved");
        }
    }

    core::JsonValue root = core::JsonValue::object();
    root.set("format", JsonValue(std::string(kProjectFormatId)));
    root.set("formatVersion", JsonValue(static_cast<double>(formatVersion)));

    core::JsonValue application = core::JsonValue::object();
    application.set("name", JsonValue(std::string(core::kProductName)));
    application.set("id", JsonValue(std::string(core::kProductId)));
    application.set("version",
                    JsonValue(applicationVersion.empty() ? std::string(core::fullVersionString())
                                                         : applicationVersion));
    root.set("application", std::move(application));

    root.set("revision", JsonValue(static_cast<double>(revision)));

    core::JsonValue settingsJson = core::JsonValue::object();
    settingsJson.set("author", JsonValue(settings.author));
    settingsJson.set("description", JsonValue(settings.description));
    settingsJson.set("units", JsonValue(settings.units));
    settingsJson.set("backgroundColor", vec3ToJson(settings.backgroundColor));
    settingsJson.set("rendererHint", JsonValue(settings.rendererHint));
    root.set("settings", std::move(settingsJson));

    core::JsonValue scene = core::JsonValue::object();
    core::JsonValue objectsArray = core::JsonValue::array();
    for (const scene::SceneObject& object : objects) {
        objectsArray.append(objectToJson(object));
    }
    scene.set("objects", std::move(objectsArray));
    root.set("scene", std::move(scene));

    return root.dump(2);
}

core::Result<ProjectDocument> ProjectDocument::fromJsonText(std::string_view text) {
    core::JsonParseResult parsed = core::JsonValue::parse(text);
    if (!parsed.ok) {
        return documentError(core::ErrorCode::ParseError,
                             "invalid JSON at line " + std::to_string(parsed.line) + ", column " +
                                 std::to_string(parsed.column) + ": " + parsed.error);
    }
    const core::JsonValue& root = parsed.value;
    if (!root.isObject()) {
        return documentError(core::ErrorCode::ValidationFailed, "a project file must be a JSON object");
    }

    const core::JsonValue* format = root.find("format");
    if (format == nullptr || !format->isString() || format->asString() != kProjectFormatId) {
        return documentError(core::ErrorCode::ValidationFailed,
                             "missing or unexpected 'format' identifier: this is not a Final Effect 3D project "
                             "file");
    }

    core::Result<std::uint64_t> version = readUnsigned(root.find("formatVersion"), "formatVersion");
    if (!version.ok()) {
        return version.error();
    }
    const auto formatVersion = static_cast<int>(version.value());
    if (formatVersion > kProjectFormatVersion) {
        return documentError(core::ErrorCode::UnsupportedVersion,
                             "project file was written by a newer Final Effect 3D (format version " +
                                 std::to_string(formatVersion) + ", this build supports up to " +
                                 std::to_string(kProjectFormatVersion) + ")");
    }
    if (formatVersion < kMinimumSupportedFormatVersion) {
        return documentError(core::ErrorCode::UnsupportedVersion,
                             "project format version " + std::to_string(formatVersion) +
                                 " is too old for this build");
    }

    ProjectDocument document;
    document.formatVersion = formatVersion;

    if (const core::JsonValue* revision = root.find("revision"); revision != nullptr) {
        core::Result<std::uint64_t> parsedRevision = readUnsigned(revision, "revision");
        if (!parsedRevision.ok()) {
            return parsedRevision.error();
        }
        document.revision = parsedRevision.value();
    }

    if (const core::JsonValue* application = root.find("application");
        application != nullptr && application->isObject()) {
        document.applicationVersion = application->stringOr("version", std::string());
    }

    if (const core::JsonValue* settings = root.find("settings"); settings != nullptr) {
        if (!settings->isObject()) {
            return documentError(core::ErrorCode::ValidationFailed, "settings must be an object");
        }
        document.settings.author = settings->stringOr("author", std::string());
        document.settings.description = settings->stringOr("description", std::string());
        document.settings.units = settings->stringOr("units", std::string("meters"));
        document.settings.rendererHint = settings->stringOr("rendererHint", std::string("auto"));
        if (const core::JsonValue* background = settings->find("backgroundColor");
            background != nullptr && !background->isNull()) {
            core::Result<math::Vec3> parsedBackground = jsonToVec3(background, "settings.backgroundColor");
            if (!parsedBackground.ok()) {
                return parsedBackground.error();
            }
            document.settings.backgroundColor = parsedBackground.value();
        }
    }

    const core::JsonValue* scene = root.find("scene");
    if (scene == nullptr || !scene->isObject()) {
        return documentError(core::ErrorCode::ValidationFailed, "a project file must contain a 'scene' object");
    }
    const core::JsonValue* objects = scene->find("objects");
    if (objects == nullptr || !objects->isArray()) {
        return documentError(core::ErrorCode::ValidationFailed, "scene.objects must be an array");
    }
    document.objects.reserve(objects->items().size());
    for (std::size_t i = 0; i < objects->items().size(); ++i) {
        core::Result<scene::SceneObject> object = jsonToObject(objects->items()[i], i);
        if (!object.ok()) {
            return object.error();
        }
        document.objects.push_back(std::move(object.value()));
    }

    // Full hierarchy validation (unique ids, existing parents, acyclic links).
    core::Result<scene::Scene> validated = document.toScene();
    if (!validated.ok()) {
        return validated.error();
    }

    return document;
}

core::Status saveProjectFile(platform::FileSystem& fileSystem,
                             const std::string& path,
                             const ProjectDocument& document) {
    if (path.empty()) {
        return documentError(core::ErrorCode::InvalidArgument, "project path is empty");
    }
    core::Result<std::string> text = document.toJsonText();
    if (!text.ok()) {
        return text.error();
    }
    return fileSystem.writeTextFileAtomic(path, text.value());
}

core::Result<ProjectDocument> loadProjectFile(platform::FileSystem& fileSystem, const std::string& path) {
    core::Result<std::string> text = fileSystem.readTextFile(path);
    if (!text.ok()) {
        return text.error();
    }
    core::Result<ProjectDocument> document = ProjectDocument::fromJsonText(text.value());
    if (!document.ok()) {
        return documentError(document.error().code(),
                             document.error().message() + " [file: " + path + "]");
    }
    return document;
}

}  // namespace fe::project
