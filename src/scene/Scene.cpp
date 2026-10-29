// SPDX-License-Identifier: GPL-3.0-only
#include "finaleffect/scene/Scene.hpp"

#include <algorithm>
#include <utility>

namespace fe::scene {
namespace {

core::Error error(core::ErrorCode code, std::string message) {
    return core::makeError(code, std::move(message), "scene");
}

}  // namespace

std::string toString(ObjectId id) {
    return std::to_string(id.value);
}

core::Result<ObjectId> Scene::createObject(std::string name, ObjectId parent) {
    if (name.empty()) {
        return error(core::ErrorCode::InvalidArgument, "scene object name must not be empty");
    }
    if (parent.isValid() && !contains(parent)) {
        return error(core::ErrorCode::NotFound, "parent object " + toString(parent) + " does not exist");
    }

    SceneObject object;
    object.id = ObjectId{nextIdValue_++};
    object.name = std::move(name);
    object.parent = parent;

    objects_.push_back(std::move(object));
    index_.emplace(objects_.back().id, objects_.size() - 1);
    if (parent.isValid()) {
        SceneObject* parentObject = find(parent);
        parentObject->children.push_back(objects_.back().id);
    }
    return objects_.back().id;
}

void Scene::appendWithDescendants(ObjectId id, std::vector<ObjectId>& out) const {
    out.push_back(id);
    const SceneObject* object = find(id);
    if (object == nullptr) {
        return;
    }
    for (const ObjectId child : object->children) {
        appendWithDescendants(child, out);
    }
}

core::Status Scene::destroyObject(ObjectId id) {
    if (!contains(id)) {
        return error(core::ErrorCode::NotFound, "object " + toString(id) + " does not exist");
    }

    std::vector<ObjectId> doomed;
    appendWithDescendants(id, doomed);
    const ObjectId parent = find(id)->parent;

    std::vector<SceneObject> remaining;
    remaining.reserve(objects_.size() - doomed.size());
    for (SceneObject& object : objects_) {
        const bool removed = std::find(doomed.begin(), doomed.end(), object.id) != doomed.end();
        if (!removed) {
            object.children.erase(std::remove_if(object.children.begin(),
                                                 object.children.end(),
                                                 [&doomed](ObjectId candidate) {
                                                     return std::find(doomed.begin(), doomed.end(), candidate) !=
                                                            doomed.end();
                                                 }),
                                  object.children.end());
            remaining.push_back(std::move(object));
        }
    }
    objects_ = std::move(remaining);
    rebuildIndex();

    if (parent.isValid()) {
        (void)parent;  // the parent keeps its identity; its child list was rebuilt above.
    }
    return core::successStatus();
}

core::Status Scene::setParent(ObjectId child, ObjectId parent) {
    SceneObject* childObject = find(child);
    if (childObject == nullptr) {
        return error(core::ErrorCode::NotFound, "object " + toString(child) + " does not exist");
    }
    if (parent.isValid() && !contains(parent)) {
        return error(core::ErrorCode::NotFound, "parent object " + toString(parent) + " does not exist");
    }
    if (child == parent) {
        return error(core::ErrorCode::CycleDetected, "object " + toString(child) + " cannot be its own parent");
    }
    if (parent.isValid() && isAncestorOf(child, parent)) {
        return cycleError(child, parent);
    }
    if (childObject->parent == parent) {
        return core::successStatus();
    }

    if (childObject->parent.isValid()) {
        SceneObject* oldParent = find(childObject->parent);
        if (oldParent != nullptr) {
            oldParent->children.erase(std::remove(oldParent->children.begin(), oldParent->children.end(), child),
                                      oldParent->children.end());
        }
    }
    childObject->parent = parent;
    if (parent.isValid()) {
        find(parent)->children.push_back(child);
    }
    return core::successStatus();
}

core::Status Scene::setName(ObjectId id, std::string name) {
    if (name.empty()) {
        return error(core::ErrorCode::InvalidArgument, "scene object name must not be empty");
    }
    SceneObject* object = find(id);
    if (object == nullptr) {
        return error(core::ErrorCode::NotFound, "object " + toString(id) + " does not exist");
    }
    object->name = std::move(name);
    return core::successStatus();
}

SceneObject* Scene::find(ObjectId id) noexcept {
    const auto it = index_.find(id);
    return it == index_.end() ? nullptr : &objects_[it->second];
}

const SceneObject* Scene::find(ObjectId id) const noexcept {
    const auto it = index_.find(id);
    return it == index_.end() ? nullptr : &objects_[it->second];
}

bool Scene::contains(ObjectId id) const noexcept {
    return index_.find(id) != index_.end();
}

std::vector<ObjectId> Scene::roots() const {
    std::vector<ObjectId> result;
    for (const SceneObject& object : objects_) {
        if (!object.parent.isValid()) {
            result.push_back(object.id);
        }
    }
    return result;
}

math::Mat4 Scene::worldMatrix(ObjectId id) const {
    const SceneObject* object = find(id);
    if (object == nullptr) {
        return math::Mat4::identity();
    }
    // Collect the ancestor chain from the object up to the root, then multiply
    // downwards so that world = rootLocal * ... * objectLocal.
    std::vector<const SceneObject*> chain;
    const SceneObject* current = object;
    std::size_t guard = 0;
    while (current != nullptr && guard <= objects_.size()) {
        chain.push_back(current);
        current = current->parent.isValid() ? find(current->parent) : nullptr;
        ++guard;
    }
    math::Mat4 world = math::Mat4::identity();
    for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
        world = world * (*it)->localTransform.matrix();
    }
    return world;
}

math::Transform Scene::worldTransform(ObjectId id) const {
    return math::Transform::fromMatrix(worldMatrix(id));
}

std::size_t Scene::depthOf(ObjectId id) const {
    std::size_t depth = 0;
    const SceneObject* object = find(id);
    std::size_t guard = 0;
    while (object != nullptr && object->parent.isValid() && guard <= objects_.size()) {
        object = find(object->parent);
        ++depth;
        ++guard;
    }
    return depth;
}

bool Scene::isAncestorOf(ObjectId candidateAncestor, ObjectId id) const {
    if (!candidateAncestor.isValid()) {
        return false;
    }
    const SceneObject* object = find(id);
    std::size_t guard = 0;
    while (object != nullptr && object->parent.isValid() && guard <= objects_.size()) {
        if (object->parent == candidateAncestor) {
            return true;
        }
        object = find(object->parent);
        ++guard;
    }
    return false;
}

core::Error Scene::cycleError(ObjectId child, ObjectId parent) const {
    return error(core::ErrorCode::CycleDetected,
                 "reparenting " + toString(child) + " under " + toString(parent) + " would create a cycle");
}

void Scene::reserve(std::size_t objectCount) {
    objects_.reserve(objectCount);
    index_.reserve(objectCount);
}

void Scene::clear() noexcept {
    objects_.clear();
    index_.clear();
    nextIdValue_ = 1U;
}

void Scene::rebuildIndex() {
    index_.clear();
    index_.reserve(objects_.size());
    for (std::size_t i = 0; i < objects_.size(); ++i) {
        index_.emplace(objects_[i].id, i);
    }
}

void Scene::rebuildChildren() {
    for (SceneObject& object : objects_) {
        object.children.clear();
    }
    for (SceneObject& object : objects_) {
        if (!object.parent.isValid()) {
            continue;
        }
        SceneObject* parent = find(object.parent);
        if (parent != nullptr) {
            parent->children.push_back(object.id);
        }
    }
}

core::Status Scene::validate() const {
    for (std::size_t i = 0; i < objects_.size(); ++i) {
        const SceneObject& object = objects_[i];
        if (!object.id.isValid()) {
            return error(core::ErrorCode::ValidationFailed, "object at index " + std::to_string(i) + " has no id");
        }
        if (object.name.empty()) {
            return error(core::ErrorCode::ValidationFailed,
                         "object " + toString(object.id) + " has an empty name");
        }
        if (!object.localTransform.isFinite()) {
            return error(core::ErrorCode::ValidationFailed,
                         "object " + toString(object.id) + " has a non-finite transform");
        }
        if (object.parent.isValid() && !contains(object.parent)) {
            return error(core::ErrorCode::ValidationFailed,
                         "object " + toString(object.id) + " references unknown parent " +
                             toString(object.parent));
        }
        for (std::size_t j = i + 1; j < objects_.size(); ++j) {
            if (objects_[i].id == objects_[j].id) {
                return error(core::ErrorCode::ValidationFailed,
                             "duplicate object id " + toString(object.id));
            }
        }
    }

    for (const SceneObject& object : objects_) {
        std::size_t guard = 0;
        const SceneObject* cursor = &object;
        while (cursor != nullptr && cursor->parent.isValid() && guard <= objects_.size()) {
            cursor = find(cursor->parent);
            ++guard;
        }
        if (guard > objects_.size()) {
            return error(core::ErrorCode::ValidationFailed,
                         "hierarchy cycle detected above object " + toString(object.id));
        }
    }

    return core::successStatus();
}

core::Result<Scene> Scene::fromObjects(std::vector<SceneObject> objects) {
    Scene scene;
    scene.objects_ = std::move(objects);
    scene.rebuildIndex();
    scene.rebuildChildren();

    const core::Status validation = scene.validate();
    if (!validation.ok()) {
        return validation.error();
    }

    std::uint64_t maxId = 0;
    for (const SceneObject& object : scene.objects_) {
        maxId = std::max(maxId, object.id.value);
    }
    scene.nextIdValue_ = maxId + 1U;
    return scene;
}

}  // namespace fe::scene
