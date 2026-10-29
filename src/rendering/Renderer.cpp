// SPDX-License-Identifier: GPL-3.0-only
#include "finaleffect/rendering/Renderer.hpp"

namespace fe::rendering {

Camera Camera::lookingAt(const math::Vec3& eye, const math::Vec3& focus, const math::Vec3& upAxis) noexcept {
    Camera camera;
    camera.position = eye;
    camera.target = focus;
    camera.up = upAxis;
    return camera;
}

math::Mat4 Camera::viewMatrix() const noexcept {
    return math::Mat4::lookAt(position, target, up);
}

math::Mat4 Camera::projectionMatrix(math::Scalar aspect) const noexcept {
    return math::Mat4::perspective(fieldOfViewY, aspect, nearPlane, farPlane);
}

math::Mat4 Camera::viewProjectionMatrix(math::Scalar aspect) const noexcept {
    return projectionMatrix(aspect) * viewMatrix();
}

Renderer::~Renderer() = default;

core::Status Renderer::readbackImage(ImageBuffer& out) const {
    (void)out;
    return core::makeError(core::ErrorCode::UnsupportedOperation,
                           "this renderer cannot read back frames",
                           "renderer");
}

}  // namespace fe::rendering
