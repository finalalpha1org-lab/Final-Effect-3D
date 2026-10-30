// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#pragma once

#include "finaleffect/core/result.hpp"
#include "finaleffect/math/transform.hpp"
#include "finaleffect/scene/object_id.hpp"
#include "finaleffect/scene/scene.hpp"

#include <cstddef>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace finaleffect::scene {

class ISceneCommand {
public:
    virtual ~ISceneCommand() = default;

    [[nodiscard]] virtual std::string_view description() const noexcept = 0;
    [[nodiscard]] virtual std::size_t estimatedBytes() const noexcept = 0;

    [[nodiscard]] virtual core::Status execute(Scene& scene) = 0;
    [[nodiscard]] virtual core::Status undo(Scene& scene) = 0;

    // Optional drag-coalescing hook (e.g., continuous gizmo transform updates).
    [[nodiscard]] virtual bool tryCoalesceWith(const ISceneCommand&) noexcept {
        return false;
    }
};

class SetTransformCommand final : public ISceneCommand {
public:
    SetTransformCommand(
        ObjectId object_id,
        const math::Transform& new_transform,
        bool allow_coalesce = false,
        std::uint64_t coalesce_group = 0);

    [[nodiscard]] std::string_view description() const noexcept override { return "Set Transform"; }
    [[nodiscard]] std::size_t estimatedBytes() const noexcept override { return sizeof(*this); }

    [[nodiscard]] core::Status execute(Scene& scene) override;
    [[nodiscard]] core::Status undo(Scene& scene) override;
    [[nodiscard]] bool tryCoalesceWith(const ISceneCommand& incoming) noexcept override;

private:
    ObjectId object_id_;
    math::Transform new_transform_;
    math::Transform previous_transform_{};
    bool captured_previous_{false};
    bool allow_coalesce_{false};
    std::uint64_t coalesce_group_{0};
};

class CreateObjectCommand final : public ISceneCommand {
public:
    CreateObjectCommand(
        std::string object_name,
        ObjectId parent_id = ObjectId::invalid(),
        math::Transform initial_transform = math::Transform::identity(),
        std::optional<MeshId> mesh_id = std::nullopt,
        std::optional<CameraComponent> camera = std::nullopt,
        std::optional<DirectionalLightComponent> directional_light = std::nullopt,
        std::optional<MaterialId> material_id = std::nullopt);

    [[nodiscard]] std::string_view description() const noexcept override { return "Create Object"; }
    [[nodiscard]] std::size_t estimatedBytes() const noexcept override;

    [[nodiscard]] core::Status execute(Scene& scene) override;
    [[nodiscard]] core::Status undo(Scene& scene) override;

    [[nodiscard]] ObjectId createdObjectId() const noexcept { return created_id_; }

private:
    std::string object_name_;
    ObjectId parent_id_;
    math::Transform initial_transform_;
    std::optional<MeshId> mesh_id_;
    std::optional<CameraComponent> camera_;
    std::optional<DirectionalLightComponent> directional_light_;
    std::optional<MaterialId> material_id_;
    ObjectId created_id_{ObjectId::invalid()};
};

class DeleteObjectCommand final : public ISceneCommand {
public:
    explicit DeleteObjectCommand(ObjectId target_id, RemovalMode mode = RemovalMode::Recursive);

    [[nodiscard]] std::string_view description() const noexcept override { return "Delete Object"; }
    [[nodiscard]] std::size_t estimatedBytes() const noexcept override;

    [[nodiscard]] core::Status execute(Scene& scene) override;
    [[nodiscard]] core::Status undo(Scene& scene) override;

private:
    struct ObjectSnapshot {
        ObjectId id;
        std::string name;
        bool visible{true};
        math::Transform local_transform;
        ObjectId parent_id;
        std::optional<MeshId> mesh_id;
        std::optional<MaterialId> material_id;
        std::optional<CameraComponent> camera;
        std::optional<DirectionalLightComponent> directional_light;
    };

    ObjectId target_id_;
    RemovalMode mode_;
    std::vector<ObjectSnapshot> deleted_snapshots_;
    std::vector<ObjectId> reparented_children_;
    ObjectId previous_active_camera_{ObjectId::invalid()};
};

class ReparentObjectCommand final : public ISceneCommand {
public:
    ReparentObjectCommand(ObjectId child_id, ObjectId new_parent_id);

    [[nodiscard]] std::string_view description() const noexcept override { return "Reparent Object"; }
    [[nodiscard]] std::size_t estimatedBytes() const noexcept override { return sizeof(*this); }

    [[nodiscard]] core::Status execute(Scene& scene) override;
    [[nodiscard]] core::Status undo(Scene& scene) override;

private:
    ObjectId child_id_;
    ObjectId new_parent_id_;
    ObjectId previous_parent_id_{ObjectId::invalid()};
    bool captured_previous_{false};
};

class RenameObjectCommand final : public ISceneCommand {
public:
    RenameObjectCommand(ObjectId object_id, std::string new_name);

    [[nodiscard]] std::string_view description() const noexcept override { return "Rename Object"; }
    [[nodiscard]] std::size_t estimatedBytes() const noexcept override;

    [[nodiscard]] core::Status execute(Scene& scene) override;
    [[nodiscard]] core::Status undo(Scene& scene) override;

private:
    ObjectId object_id_;
    std::string new_name_;
    std::string previous_name_;
    bool captured_previous_{false};
};

class SetVisibilityCommand final : public ISceneCommand {
public:
    SetVisibilityCommand(ObjectId object_id, bool visible);

    [[nodiscard]] std::string_view description() const noexcept override { return "Set Visibility"; }
    [[nodiscard]] std::size_t estimatedBytes() const noexcept override { return sizeof(*this); }

    [[nodiscard]] core::Status execute(Scene& scene) override;
    [[nodiscard]] core::Status undo(Scene& scene) override;

private:
    ObjectId object_id_;
    bool new_visible_{true};
    bool previous_visible_{true};
    bool captured_previous_{false};
};

class SetCameraComponentCommand final : public ISceneCommand {
public:
    SetCameraComponentCommand(ObjectId object_id, std::optional<CameraComponent> camera);

    [[nodiscard]] std::string_view description() const noexcept override { return "Edit Camera"; }
    [[nodiscard]] std::size_t estimatedBytes() const noexcept override { return sizeof(*this); }

    [[nodiscard]] core::Status execute(Scene& scene) override;
    [[nodiscard]] core::Status undo(Scene& scene) override;

private:
    ObjectId object_id_;
    std::optional<CameraComponent> new_camera_;
    std::optional<CameraComponent> previous_camera_;
    ObjectId previous_active_camera_{ObjectId::invalid()};
    bool captured_previous_{false};
};

class SetDirectionalLightCommand final : public ISceneCommand {
public:
    SetDirectionalLightCommand(
        ObjectId object_id,
        std::optional<DirectionalLightComponent> light,
        bool allow_coalesce = false);

    [[nodiscard]] std::string_view description() const noexcept override { return "Edit Light"; }
    [[nodiscard]] std::size_t estimatedBytes() const noexcept override { return sizeof(*this); }

    [[nodiscard]] core::Status execute(Scene& scene) override;
    [[nodiscard]] core::Status undo(Scene& scene) override;
    [[nodiscard]] bool tryCoalesceWith(const ISceneCommand& incoming) noexcept override;

private:
    ObjectId object_id_;
    std::optional<DirectionalLightComponent> new_light_;
    std::optional<DirectionalLightComponent> previous_light_;
    bool captured_previous_{false};
    bool allow_coalesce_{false};
};

class ModifyMeshCommand final : public ISceneCommand {
public:
    ModifyMeshCommand(
        MeshId mesh_id,
        EditableMesh new_mesh,
        std::string label = "Edit Mesh",
        bool allow_coalesce = false,
        std::uint64_t coalesce_group = 0);

    [[nodiscard]] std::string_view description() const noexcept override { return label_; }
    [[nodiscard]] std::size_t estimatedBytes() const noexcept override;

    [[nodiscard]] core::Status execute(Scene& scene) override;
    [[nodiscard]] core::Status undo(Scene& scene) override;
    [[nodiscard]] bool tryCoalesceWith(const ISceneCommand& incoming) noexcept override;

private:
    MeshId mesh_id_;
    EditableMesh new_mesh_;
    EditableMesh previous_mesh_;
    std::string label_;
    bool captured_previous_{false};
    bool allow_coalesce_{false};
    std::uint64_t coalesce_group_{0};
};

class CreateMaterialCommand final : public ISceneCommand {
public:
    explicit CreateMaterialCommand(PbrMaterial material, std::string label = "Create Material");

    [[nodiscard]] std::string_view description() const noexcept override { return label_; }
    [[nodiscard]] std::size_t estimatedBytes() const noexcept override;

    [[nodiscard]] core::Status execute(Scene& scene) override;
    [[nodiscard]] core::Status undo(Scene& scene) override;

    [[nodiscard]] MaterialId createdMaterialId() const noexcept { return created_id_; }

private:
    PbrMaterial material_;
    std::string label_;
    MaterialId created_id_{MaterialId::invalid()};
};

class UpdateMaterialCommand final : public ISceneCommand {
public:
    UpdateMaterialCommand(
        MaterialId material_id,
        PbrMaterial updated_material,
        bool allow_coalesce = false,
        std::uint64_t coalesce_group = 0);

    [[nodiscard]] std::string_view description() const noexcept override { return "Edit Material"; }
    [[nodiscard]] std::size_t estimatedBytes() const noexcept override;

    [[nodiscard]] core::Status execute(Scene& scene) override;
    [[nodiscard]] core::Status undo(Scene& scene) override;
    [[nodiscard]] bool tryCoalesceWith(const ISceneCommand& incoming) noexcept override;

private:
    MaterialId material_id_;
    PbrMaterial updated_material_;
    PbrMaterial previous_material_{};
    bool captured_previous_{false};
    bool allow_coalesce_{false};
    std::uint64_t coalesce_group_{0};
};

class DeleteMaterialCommand final : public ISceneCommand {
public:
    explicit DeleteMaterialCommand(MaterialId material_id);

    [[nodiscard]] std::string_view description() const noexcept override { return "Delete Material"; }
    [[nodiscard]] std::size_t estimatedBytes() const noexcept override;

    [[nodiscard]] core::Status execute(Scene& scene) override;
    [[nodiscard]] core::Status undo(Scene& scene) override;

private:
    struct FaceMaterialRef {
        MeshId mesh_id;
        std::uint32_t face_index;
    };

    MaterialId material_id_;
    PbrMaterial snapshot_{};
    bool captured_{false};
    std::vector<ObjectId> affected_objects_;
    std::vector<FaceMaterialRef> affected_faces_;
};

class AssignObjectMaterialCommand final : public ISceneCommand {
public:
    AssignObjectMaterialCommand(ObjectId object_id, std::optional<MaterialId> material_id);

    [[nodiscard]] std::string_view description() const noexcept override { return "Assign Material"; }
    [[nodiscard]] std::size_t estimatedBytes() const noexcept override { return sizeof(*this); }

    [[nodiscard]] core::Status execute(Scene& scene) override;
    [[nodiscard]] core::Status undo(Scene& scene) override;

private:
    ObjectId object_id_;
    std::optional<MaterialId> new_material_id_;
    std::optional<MaterialId> previous_material_id_;
    bool captured_previous_{false};
};

// Atomic multi-step transaction; rolls back already-executed steps if any step fails.
class CompoundTransactionCommand final : public ISceneCommand {
public:
    explicit CompoundTransactionCommand(
        std::string label,
        std::vector<std::unique_ptr<ISceneCommand>> steps = {});

    void addStep(std::unique_ptr<ISceneCommand> step);

    [[nodiscard]] std::string_view description() const noexcept override { return label_; }
    [[nodiscard]] std::size_t estimatedBytes() const noexcept override;

    [[nodiscard]] core::Status execute(Scene& scene) override;
    [[nodiscard]] core::Status undo(Scene& scene) override;

private:
    std::string label_;
    std::vector<std::unique_ptr<ISceneCommand>> steps_;
};

struct CommandStackLimits {
    std::size_t max_entries{128};
    std::size_t max_memory_bytes{16u * 1024u * 1024u}; // 16 MiB default cap
};

// Deterministic, memory-bounded undo/redo history stack.
class CommandStack {
public:
    explicit CommandStack(CommandStackLimits limits = {});

    [[nodiscard]] core::Status execute(Scene& scene, std::unique_ptr<ISceneCommand> command);
    [[nodiscard]] core::Status undo(Scene& scene);
    [[nodiscard]] core::Status redo(Scene& scene);

    void clear() noexcept;

    [[nodiscard]] bool canUndo() const noexcept { return !undo_stack_.empty(); }
    [[nodiscard]] bool canRedo() const noexcept { return !redo_stack_.empty(); }
    [[nodiscard]] std::size_t undoCount() const noexcept { return undo_stack_.size(); }
    [[nodiscard]] std::size_t redoCount() const noexcept { return redo_stack_.size(); }
    [[nodiscard]] std::size_t totalMemoryBytes() const noexcept { return undo_bytes_ + redo_bytes_; }

    [[nodiscard]] std::string_view nextUndoDescription() const noexcept;
    [[nodiscard]] std::string_view nextRedoDescription() const noexcept;

private:
    void enforceLimits() noexcept;

    CommandStackLimits limits_;
    std::deque<std::unique_ptr<ISceneCommand>> undo_stack_;
    std::vector<std::unique_ptr<ISceneCommand>> redo_stack_;
    std::size_t undo_bytes_{0};
    std::size_t redo_bytes_{0};
};

} // namespace finaleffect::scene
