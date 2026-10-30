// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/scene/commands.hpp"

#include <algorithm>
#include <utility>

namespace finaleffect::scene {

// -----------------------------------------------------------------------------
// SetTransformCommand
// -----------------------------------------------------------------------------
SetTransformCommand::SetTransformCommand(
    ObjectId object_id,
    const math::Transform& new_transform,
    bool allow_coalesce)
    : object_id_(object_id),
      new_transform_(new_transform),
      allow_coalesce_(allow_coalesce) {}

core::Status SetTransformCommand::execute(Scene& scene) {
    const auto* obj = scene.findObject(object_id_);
    if (obj == nullptr) {
        return core::Status::fail(core::ErrorCode::NotFound, "SetTransformCommand target object not found");
    }
    if (!captured_previous_) {
        previous_transform_ = obj->local_transform;
        captured_previous_ = true;
    }
    return scene.setObjectTransform(object_id_, new_transform_);
}

core::Status SetTransformCommand::undo(Scene& scene) {
    if (!captured_previous_) {
        return core::Status::fail(core::ErrorCode::InvalidState, "SetTransformCommand was never executed");
    }
    return scene.setObjectTransform(object_id_, previous_transform_);
}

bool SetTransformCommand::tryCoalesceWith(const ISceneCommand& incoming) noexcept {
    if (!allow_coalesce_) {
        return false;
    }
    const auto* next_tf = dynamic_cast<const SetTransformCommand*>(&incoming);
    if (next_tf == nullptr || !next_tf->allow_coalesce_ || next_tf->object_id_ != object_id_) {
        return false;
    }
    new_transform_ = next_tf->new_transform_;
    return true;
}

// -----------------------------------------------------------------------------
// CreateObjectCommand
// -----------------------------------------------------------------------------
CreateObjectCommand::CreateObjectCommand(
    std::string object_name,
    ObjectId parent_id,
    math::Transform initial_transform,
    std::optional<MeshId> mesh_id,
    std::optional<CameraComponent> camera,
    std::optional<DirectionalLightComponent> directional_light)
    : object_name_(std::move(object_name)),
      parent_id_(parent_id),
      initial_transform_(initial_transform),
      mesh_id_(mesh_id),
      camera_(camera),
      directional_light_(directional_light) {}

std::size_t CreateObjectCommand::estimatedBytes() const noexcept {
    return sizeof(*this) + object_name_.capacity();
}

core::Status CreateObjectCommand::execute(Scene& scene) {
    if (!initial_transform_.isValid()) {
        return core::Status::fail(core::ErrorCode::ValidationError, "CreateObjectCommand initial transform is invalid");
    }
    if (mesh_id_.has_value() && scene.findMesh(mesh_id_.value()) == nullptr) {
        return core::Status::fail(core::ErrorCode::NotFound, "CreateObjectCommand referenced unknown MeshId");
    }

    if (!created_id_.isValid()) {
        auto created = scene.createObject(object_name_, parent_id_);
        if (!created) {
            return created.error();
        }
        created_id_ = created.value();
    } else {
        auto st = scene.createObjectWithId(created_id_, object_name_, parent_id_);
        if (!st) {
            return st;
        }
    }

    auto tf_st = scene.setObjectTransform(created_id_, initial_transform_);
    if (!tf_st) {
        (void)scene.removeObject(created_id_, RemovalMode::Recursive);
        return tf_st;
    }

    if (mesh_id_.has_value()) {
        auto m_st = scene.assignMesh(created_id_, mesh_id_);
        if (!m_st) {
            (void)scene.removeObject(created_id_, RemovalMode::Recursive);
            return m_st;
        }
    }

    if (camera_.has_value()) {
        auto c_st = scene.assignCamera(created_id_, camera_);
        if (!c_st) {
            (void)scene.removeObject(created_id_, RemovalMode::Recursive);
            return c_st;
        }
        if (!scene.activeCamera().isValid()) {
            (void)scene.setActiveCamera(created_id_);
        }
    }

    if (directional_light_.has_value()) {
        auto l_st = scene.assignDirectionalLight(created_id_, directional_light_);
        if (!l_st) {
            (void)scene.removeObject(created_id_, RemovalMode::Recursive);
            return l_st;
        }
    }
    return core::Status::ok();
}

core::Status CreateObjectCommand::undo(Scene& scene) {
    if (!created_id_.isValid()) {
        return core::Status::fail(core::ErrorCode::InvalidState, "CreateObjectCommand was never executed");
    }
    return scene.removeObject(created_id_, RemovalMode::Recursive);
}

// -----------------------------------------------------------------------------
// DeleteObjectCommand
// -----------------------------------------------------------------------------
DeleteObjectCommand::DeleteObjectCommand(ObjectId target_id, RemovalMode mode)
    : target_id_(target_id), mode_(mode) {}

std::size_t DeleteObjectCommand::estimatedBytes() const noexcept {
    std::size_t bytes = sizeof(*this) + reparented_children_.capacity() * sizeof(ObjectId);
    for (const auto& snap : deleted_snapshots_) {
        bytes += sizeof(ObjectSnapshot) + snap.name.capacity();
    }
    return bytes;
}

core::Status DeleteObjectCommand::execute(Scene& scene) {
    const auto* target = scene.findObject(target_id_);
    if (target == nullptr) {
        return core::Status::fail(core::ErrorCode::NotFound, "DeleteObjectCommand target object not found");
    }

    deleted_snapshots_.clear();
    reparented_children_.clear();
    previous_active_camera_ = scene.activeCamera();

    auto makeSnapshot = [](const SceneObject& obj) -> ObjectSnapshot {
        return ObjectSnapshot{
            .id = obj.id,
            .name = obj.name,
            .visible = obj.visible,
            .local_transform = obj.local_transform,
            .parent_id = obj.parent_id,
            .mesh_id = obj.mesh_id,
            .camera = obj.camera,
            .directional_light = obj.directional_light,
        };
    };

    if (mode_ == RemovalMode::ReparentChildren) {
        reparented_children_ = target->children;
        deleted_snapshots_.push_back(makeSnapshot(*target));
    } else {
        // Pre-order BFS/DFS snapshot so parents always precede children during undo restoration.
        std::vector<ObjectId> stack{target_id_};
        while (!stack.empty()) {
            const ObjectId curr_id = stack.back();
            stack.pop_back();
            const auto* node = scene.findObject(curr_id);
            if (node == nullptr) {
                continue;
            }
            deleted_snapshots_.push_back(makeSnapshot(*node));
            for (auto it = node->children.rbegin(); it != node->children.rend(); ++it) {
                stack.push_back(*it);
            }
        }
    }

    return scene.removeObject(target_id_, mode_);
}

core::Status DeleteObjectCommand::undo(Scene& scene) {
    if (deleted_snapshots_.empty()) {
        return core::Status::fail(core::ErrorCode::InvalidState, "DeleteObjectCommand has no captured snapshot");
    }

    for (const auto& snap : deleted_snapshots_) {
        auto st = scene.createObjectWithId(snap.id, snap.name, snap.parent_id);
        if (!st) return st;

        if (auto vis_st = scene.setObjectVisible(snap.id, snap.visible); !vis_st) return vis_st;
        if (auto tf_st = scene.setObjectTransform(snap.id, snap.local_transform); !tf_st) return tf_st;
        if (snap.mesh_id.has_value()) {
            if (auto m_st = scene.assignMesh(snap.id, snap.mesh_id); !m_st) return m_st;
        }
        if (snap.camera.has_value()) {
            if (auto c_st = scene.assignCamera(snap.id, snap.camera); !c_st) return c_st;
        }
        if (snap.directional_light.has_value()) {
            if (auto l_st = scene.assignDirectionalLight(snap.id, snap.directional_light); !l_st) return l_st;
        }
    }

    if (mode_ == RemovalMode::ReparentChildren) {
        for (const ObjectId child_id : reparented_children_) {
            if (scene.findObject(child_id) != nullptr) {
                auto st = scene.setParent(child_id, target_id_);
                if (!st) return st;
            }
        }
    }

    if (previous_active_camera_.isValid() && scene.findObject(previous_active_camera_) != nullptr) {
        auto cam_st = scene.setActiveCamera(previous_active_camera_);
        if (!cam_st) return cam_st;
    }

    return core::Status::ok();
}

// -----------------------------------------------------------------------------
// ReparentObjectCommand
// -----------------------------------------------------------------------------
ReparentObjectCommand::ReparentObjectCommand(ObjectId child_id, ObjectId new_parent_id)
    : child_id_(child_id), new_parent_id_(new_parent_id) {}

core::Status ReparentObjectCommand::execute(Scene& scene) {
    const auto* child = scene.findObject(child_id_);
    if (child == nullptr) {
        return core::Status::fail(core::ErrorCode::NotFound, "ReparentObjectCommand child object not found");
    }
    if (!captured_previous_) {
        previous_parent_id_ = child->parent_id;
        captured_previous_ = true;
    }
    return scene.setParent(child_id_, new_parent_id_);
}

core::Status ReparentObjectCommand::undo(Scene& scene) {
    if (!captured_previous_) {
        return core::Status::fail(core::ErrorCode::InvalidState, "ReparentObjectCommand was never executed");
    }
    return scene.setParent(child_id_, previous_parent_id_);
}

// -----------------------------------------------------------------------------
// RenameObjectCommand
// -----------------------------------------------------------------------------
RenameObjectCommand::RenameObjectCommand(ObjectId object_id, std::string new_name)
    : object_id_(object_id), new_name_(std::move(new_name)) {}

std::size_t RenameObjectCommand::estimatedBytes() const noexcept {
    return sizeof(*this) + new_name_.capacity() + previous_name_.capacity();
}

core::Status RenameObjectCommand::execute(Scene& scene) {
    const auto* obj = scene.findObject(object_id_);
    if (obj == nullptr) {
        return core::Status::fail(core::ErrorCode::NotFound, "RenameObjectCommand target object not found");
    }
    if (!captured_previous_) {
        previous_name_ = obj->name;
        captured_previous_ = true;
    }
    return scene.setObjectName(object_id_, new_name_);
}

core::Status RenameObjectCommand::undo(Scene& scene) {
    if (!captured_previous_) {
        return core::Status::fail(core::ErrorCode::InvalidState, "RenameObjectCommand was never executed");
    }
    return scene.setObjectName(object_id_, previous_name_);
}

// -----------------------------------------------------------------------------
// SetVisibilityCommand
// -----------------------------------------------------------------------------
SetVisibilityCommand::SetVisibilityCommand(ObjectId object_id, bool visible)
    : object_id_(object_id), new_visible_(visible) {}

core::Status SetVisibilityCommand::execute(Scene& scene) {
    const auto* obj = scene.findObject(object_id_);
    if (obj == nullptr) {
        return core::Status::fail(core::ErrorCode::NotFound, "SetVisibilityCommand target object not found");
    }
    if (!captured_previous_) {
        previous_visible_ = obj->visible;
        captured_previous_ = true;
    }
    return scene.setObjectVisible(object_id_, new_visible_);
}

core::Status SetVisibilityCommand::undo(Scene& scene) {
    if (!captured_previous_) {
        return core::Status::fail(core::ErrorCode::InvalidState, "SetVisibilityCommand was never executed");
    }
    return scene.setObjectVisible(object_id_, previous_visible_);
}

// -----------------------------------------------------------------------------
// SetCameraComponentCommand
// -----------------------------------------------------------------------------
SetCameraComponentCommand::SetCameraComponentCommand(
    ObjectId object_id,
    std::optional<CameraComponent> camera)
    : object_id_(object_id), new_camera_(camera) {}

core::Status SetCameraComponentCommand::execute(Scene& scene) {
    const auto* obj = scene.findObject(object_id_);
    if (obj == nullptr) {
        return core::Status::fail(core::ErrorCode::NotFound, "SetCameraComponentCommand target object not found");
    }
    if (!captured_previous_) {
        previous_camera_ = obj->camera;
        previous_active_camera_ = scene.activeCamera();
        captured_previous_ = true;
    }
    return scene.assignCamera(object_id_, new_camera_);
}

core::Status SetCameraComponentCommand::undo(Scene& scene) {
    if (!captured_previous_) {
        return core::Status::fail(core::ErrorCode::InvalidState, "SetCameraComponentCommand was never executed");
    }
    auto st = scene.assignCamera(object_id_, previous_camera_);
    if (!st) {
        return st;
    }
    if (previous_active_camera_.isValid() && scene.findObject(previous_active_camera_) != nullptr) {
        return scene.setActiveCamera(previous_active_camera_);
    }
    return core::Status::ok();
}

// -----------------------------------------------------------------------------
// SetDirectionalLightCommand
// -----------------------------------------------------------------------------
SetDirectionalLightCommand::SetDirectionalLightCommand(
    ObjectId object_id,
    std::optional<DirectionalLightComponent> light,
    bool allow_coalesce)
    : object_id_(object_id), new_light_(light), allow_coalesce_(allow_coalesce) {}

core::Status SetDirectionalLightCommand::execute(Scene& scene) {
    const auto* obj = scene.findObject(object_id_);
    if (obj == nullptr) {
        return core::Status::fail(core::ErrorCode::NotFound, "SetDirectionalLightCommand target object not found");
    }
    if (!captured_previous_) {
        previous_light_ = obj->directional_light;
        captured_previous_ = true;
    }
    return scene.assignDirectionalLight(object_id_, new_light_);
}

core::Status SetDirectionalLightCommand::undo(Scene& scene) {
    if (!captured_previous_) {
        return core::Status::fail(core::ErrorCode::InvalidState, "SetDirectionalLightCommand was never executed");
    }
    return scene.assignDirectionalLight(object_id_, previous_light_);
}

bool SetDirectionalLightCommand::tryCoalesceWith(const ISceneCommand& incoming) noexcept {
    if (!allow_coalesce_) {
        return false;
    }
    const auto* next_light = dynamic_cast<const SetDirectionalLightCommand*>(&incoming);
    if (next_light == nullptr || !next_light->allow_coalesce_ || next_light->object_id_ != object_id_) {
        return false;
    }
    new_light_ = next_light->new_light_;
    return true;
}

// -----------------------------------------------------------------------------
// CompoundTransactionCommand
// -----------------------------------------------------------------------------
CompoundTransactionCommand::CompoundTransactionCommand(
    std::string label,
    std::vector<std::unique_ptr<ISceneCommand>> steps)
    : label_(label.empty() ? "Transaction" : std::move(label)),
      steps_(std::move(steps)) {}

void CompoundTransactionCommand::addStep(std::unique_ptr<ISceneCommand> step) {
    if (step) {
        steps_.push_back(std::move(step));
    }
}

std::size_t CompoundTransactionCommand::estimatedBytes() const noexcept {
    std::size_t total = sizeof(*this) + label_.capacity();
    for (const auto& step : steps_) {
        total += step ? step->estimatedBytes() : 0u;
    }
    return total;
}

core::Status CompoundTransactionCommand::execute(Scene& scene) {
    if (steps_.empty()) {
        return core::Status::fail(core::ErrorCode::InvalidArgument, "CompoundTransactionCommand contains no steps");
    }

    for (std::size_t i = 0; i < steps_.size(); ++i) {
        auto st = steps_[i]->execute(scene);
        if (!st) {
            // Roll back already-applied steps in reverse order.
            for (std::size_t rev = i; rev > 0; --rev) {
                (void)steps_[rev - 1]->undo(scene);
            }
            return st;
        }
    }
    return core::Status::ok();
}

core::Status CompoundTransactionCommand::undo(Scene& scene) {
    for (std::size_t rev = steps_.size(); rev > 0; --rev) {
        auto st = steps_[rev - 1]->undo(scene);
        if (!st) {
            return st;
        }
    }
    return core::Status::ok();
}

// -----------------------------------------------------------------------------
// CommandStack
// -----------------------------------------------------------------------------
CommandStack::CommandStack(CommandStackLimits limits) : limits_(limits) {
    if (limits_.max_entries == 0) {
        limits_.max_entries = 1;
    }
    if (limits_.max_memory_bytes == 0) {
        limits_.max_memory_bytes = 4096;
    }
}

core::Status CommandStack::execute(Scene& scene, std::unique_ptr<ISceneCommand> command) {
    if (!command) {
        return core::Status::fail(core::ErrorCode::InvalidArgument, "Cannot execute null ISceneCommand");
    }

    auto st = command->execute(scene);
    if (!st) {
        return st;
    }

    // Any new committed edit invalidates the redo branch.
    redo_stack_.clear();
    redo_bytes_ = 0;

    if (!undo_stack_.empty() && undo_stack_.back()->tryCoalesceWith(*command)) {
        enforceLimits();
        return core::Status::ok();
    }

    const std::size_t cmd_bytes = command->estimatedBytes();
    undo_bytes_ += cmd_bytes;
    undo_stack_.push_back(std::move(command));
    enforceLimits();
    return core::Status::ok();
}

core::Status CommandStack::undo(Scene& scene) {
    if (undo_stack_.empty()) {
        return core::Status::fail(core::ErrorCode::InvalidState, "Nothing to undo");
    }

    auto cmd = std::move(undo_stack_.back());
    undo_stack_.pop_back();
    const std::size_t cmd_bytes = cmd->estimatedBytes();
    undo_bytes_ = (undo_bytes_ >= cmd_bytes) ? (undo_bytes_ - cmd_bytes) : 0u;

    auto st = cmd->undo(scene);
    if (!st) {
        return st;
    }

    redo_bytes_ += cmd_bytes;
    redo_stack_.push_back(std::move(cmd));
    return core::Status::ok();
}

core::Status CommandStack::redo(Scene& scene) {
    if (redo_stack_.empty()) {
        return core::Status::fail(core::ErrorCode::InvalidState, "Nothing to redo");
    }

    auto cmd = std::move(redo_stack_.back());
    redo_stack_.pop_back();
    const std::size_t cmd_bytes = cmd->estimatedBytes();
    redo_bytes_ = (redo_bytes_ >= cmd_bytes) ? (redo_bytes_ - cmd_bytes) : 0u;

    auto st = cmd->execute(scene);
    if (!st) {
        return st;
    }

    undo_bytes_ += cmd_bytes;
    undo_stack_.push_back(std::move(cmd));
    enforceLimits();
    return core::Status::ok();
}

void CommandStack::clear() noexcept {
    undo_stack_.clear();
    redo_stack_.clear();
    undo_bytes_ = 0;
    redo_bytes_ = 0;
}

std::string_view CommandStack::nextUndoDescription() const noexcept {
    return undo_stack_.empty() ? std::string_view{} : undo_stack_.back()->description();
}

std::string_view CommandStack::nextRedoDescription() const noexcept {
    return redo_stack_.empty() ? std::string_view{} : redo_stack_.back()->description();
}

void CommandStack::enforceLimits() noexcept {
    while (undo_stack_.size() > limits_.max_entries ||
           (undo_stack_.size() > 1 && undo_bytes_ > limits_.max_memory_bytes)) {
        const std::size_t front_bytes = undo_stack_.front()->estimatedBytes();
        undo_bytes_ = (undo_bytes_ >= front_bytes) ? (undo_bytes_ - front_bytes) : 0u;
        undo_stack_.pop_front();
    }
}

} // namespace finaleffect::scene
