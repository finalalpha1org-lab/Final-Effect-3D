// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/rendering/material_preview.hpp"
#include "finaleffect/math/constants.hpp"
#include "finaleffect/scene/primitives.hpp"

#include <utility>

namespace finaleffect::rendering {

MaterialPreviewRenderer::MaterialPreviewRenderer() = default;

core::Status MaterialPreviewRenderer::initialize(std::uint32_t width, std::uint32_t height) {
    if (initialized_) {
        return core::Status::ok();
    }

    auto sw = std::make_unique<SoftwareRenderDevice>();
    software_device_ = sw.get();
    renderer_ = std::make_unique<Renderer>(std::move(sw));

    auto init_st = renderer_->initialize(width, height);
    if (!init_st) {
        return init_st;
    }

    preview_scene_.clear();
    preview_scene_.setName("MaterialPreviewStudio");

    // 1. Studio checker backdrop behind the sphere (visibly refracted/transmitted when opacity < 1).
    auto backdrop_mesh = preview_scene_.registerMesh(
        "PreviewBackdropMesh",
        scene::Primitives::createCheckerPlane(
            2.4f,
            6,
            math::Vec3{0.15f, 0.17f, 0.21f},
            math::Vec3{0.28f, 0.31f, 0.36f}));
    if (!backdrop_mesh) {
        return backdrop_mesh.error();
    }
    auto backdrop_obj = preview_scene_.createObject("Backdrop");
    if (!backdrop_obj) {
        return backdrop_obj.error();
    }
    math::Transform backdrop_tf;
    backdrop_tf.position = {0.0f, 0.0f, -1.35f};
    backdrop_tf.rotation = math::Quat::fromAxisAngle(math::Vec3::unitX(), math::degToRad(90.0f));
    (void)preview_scene_.setObjectTransform(backdrop_obj.value(), backdrop_tf);
    (void)preview_scene_.assignMesh(backdrop_obj.value(), backdrop_mesh.value());

    // 2. Smooth 3D UV sphere showcase mesh.
    auto sphere_mesh = preview_scene_.registerEditableMesh(
        "PreviewSphereMesh",
        scene::EditableMesh::createUvSphere(0.86f, 18, 28, math::Vec3{1.0f, 1.0f, 1.0f}));
    if (!sphere_mesh) {
        return sphere_mesh.error();
    }
    auto sphere_obj = preview_scene_.createObject("PreviewSphere");
    if (!sphere_obj) {
        return sphere_obj.error();
    }
    sphere_obj_id_ = sphere_obj.value();
    (void)preview_scene_.assignMesh(sphere_obj_id_, sphere_mesh.value());

    auto default_mat = preview_scene_.createMaterial(scene::PbrMaterial{.name = "PreviewSlot"});
    if (!default_mat) {
        return default_mat.error();
    }
    preview_mat_id_ = default_mat.value();
    (void)preview_scene_.assignMaterial(sphere_obj_id_, preview_mat_id_);

    // 3. Dedicated studio camera framing the 3D sphere.
    auto cam_obj = preview_scene_.createObject("PreviewCamera");
    if (!cam_obj) {
        return cam_obj.error();
    }
    math::Transform cam_tf;
    cam_tf.position = {0.0f, 0.16f, 2.35f};
    cam_tf.rotation = math::Quat::fromAxisAngle(math::Vec3::unitX(), math::degToRad(-3.8f));
    (void)preview_scene_.setObjectTransform(cam_obj.value(), cam_tf);
    scene::CameraComponent cam_comp;
    cam_comp.fov_y_radians = math::degToRad(45.0f);
    cam_comp.near_plane = 0.1f;
    cam_comp.far_plane = 20.0f;
    (void)preview_scene_.assignCamera(cam_obj.value(), cam_comp);
    (void)preview_scene_.setActiveCamera(cam_obj.value());

    // 4. Studio key light angled from upper-right-front.
    auto light_obj = preview_scene_.createObject("PreviewKeyLight");
    if (!light_obj) {
        return light_obj.error();
    }
    const math::Quat yaw = math::Quat::fromAxisAngle(math::Vec3::unitY(), math::degToRad(34.0f));
    const math::Quat pitch = math::Quat::fromAxisAngle(math::Vec3::unitX(), math::degToRad(-38.0f));
    math::Transform light_tf;
    light_tf.rotation = (yaw * pitch).normalized();
    (void)preview_scene_.setObjectTransform(light_obj.value(), light_tf);
    (void)preview_scene_.assignDirectionalLight(
        light_obj.value(),
        scene::DirectionalLightComponent{.color = {1.0f, 0.98f, 0.95f}, .intensity = 2.4f});

    initialized_ = true;
    return renderMaterial(*preview_scene_.findMaterial(preview_mat_id_));
}

core::Status MaterialPreviewRenderer::renderMaterial(const scene::PbrMaterial& material) {
    if (!initialized_) {
        auto st = initialize();
        if (!st) {
            return st;
        }
    }

    scene::PbrMaterial clean = material.sanitized();
    clean.id = preview_mat_id_;
    auto up_st = preview_scene_.updateMaterial(preview_mat_id_, clean);
    if (!up_st) {
        return up_st;
    }

    FrameClearDesc clear;
    clear.clear_color = {0.08f, 0.09f, 0.12f, 1.0f};
    auto r_st = renderer_->renderScene(preview_scene_, clear, scene::ObjectId::invalid());
    if (!r_st) {
        return r_st;
    }

    ++generation_;
    return core::Status::ok();
}

std::uint32_t MaterialPreviewRenderer::width() const noexcept {
    return (software_device_ != nullptr) ? software_device_->width() : 0u;
}

std::uint32_t MaterialPreviewRenderer::height() const noexcept {
    return (software_device_ != nullptr) ? software_device_->height() : 0u;
}

const std::vector<Rgba8>& MaterialPreviewRenderer::rgbaPixels() const noexcept {
    static const std::vector<Rgba8> kEmpty;
    return (software_device_ != nullptr) ? software_device_->colorBuffer() : kEmpty;
}

const std::uint8_t* MaterialPreviewRenderer::rawRgbaBytes() const noexcept {
    const auto& buf = rgbaPixels();
    return buf.empty() ? nullptr : reinterpret_cast<const std::uint8_t*>(buf.data());
}

std::size_t MaterialPreviewRenderer::rawByteSize() const noexcept {
    return rgbaPixels().size() * sizeof(Rgba8);
}

Rgba8 MaterialPreviewRenderer::pixelAt(std::uint32_t x, std::uint32_t y) const noexcept {
    return (software_device_ != nullptr) ? software_device_->pixelAt(x, y) : Rgba8{};
}

} // namespace finaleffect::rendering
