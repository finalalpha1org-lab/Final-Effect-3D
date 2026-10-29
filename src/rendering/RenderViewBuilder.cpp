// SPDX-License-Identifier: GPL-3.0-only
#include "finaleffect/rendering/RenderViewBuilder.hpp"

#include <cmath>
#include <string>

namespace fe::rendering {

math::Vec3 Bounds::center() const noexcept {
    return (min + max) * 0.5F;
}

math::Scalar Bounds::halfDiagonal() const noexcept {
    return (max - min).length() * 0.5F;
}

core::Result<RenderView> buildRenderView(const scene::Scene& scene,
                                         const Camera& camera,
                                         const MeshLibrary& meshes) {
    RenderView view;
    view.camera = camera;
    view.items.reserve(scene.objectCount());

    for (const scene::SceneObject& object : scene.objects()) {
        if (object.mesh.empty()) {
            continue;
        }
        const MeshData* mesh = meshes.find(object.mesh);
        if (mesh == nullptr) {
            return core::makeError(core::ErrorCode::NotFound,
                                   "object '" + object.name + "' references unknown mesh '" + object.mesh +
                                       "'",
                                   "render view");
        }
        RenderItem item;
        item.worldMatrix = scene.worldMatrix(object.id);
        item.mesh = mesh;
        item.color = object.color;
        item.visible = object.visible;
        view.items.push_back(std::move(item));
    }
    return view;
}

Bounds computeSceneBounds(const scene::Scene& scene, const MeshLibrary& meshes) {
    Bounds bounds;
    for (const scene::SceneObject& object : scene.objects()) {
        if (object.mesh.empty() || !object.visible) {
            continue;
        }
        const MeshData* mesh = meshes.find(object.mesh);
        if (mesh == nullptr) {
            continue;
        }
        const math::Mat4 world = scene.worldMatrix(object.id);
        for (const Vertex& vertex : mesh->vertices) {
            const math::Vec3 point = world.transformPoint(vertex.position);
            if (!bounds.valid) {
                bounds.min = point;
                bounds.max = point;
                bounds.valid = true;
                continue;
            }
            bounds.min = math::min(bounds.min, point);
            bounds.max = math::max(bounds.max, point);
        }
    }
    return bounds;
}

Camera frameScene(const scene::Scene& scene, const MeshLibrary& meshes) {
    const Bounds bounds = computeSceneBounds(scene, meshes);
    if (!bounds.valid) {
        return Camera::lookingAt(math::Vec3(3.0F, 2.0F, 4.0F), math::Vec3(0.0F, 0.0F, 0.0F),
                                 math::Vec3(0.0F, 1.0F, 0.0F));
    }

    const math::Vec3 center = bounds.center();
    const math::Scalar radius = std::fmax(bounds.halfDiagonal(), 0.5F);
    const math::Vec3 direction = math::Vec3(1.0F, 0.62F, 1.45F).normalized();

    Camera camera;
    camera.fieldOfViewY = 0.87266463F;  // 50 degrees
    const math::Scalar distance = radius / std::sin(camera.fieldOfViewY * 0.5F) * 1.15F;
    camera.position = center + direction * distance;
    camera.target = center;
    camera.up = math::Vec3::unitY();
    camera.nearPlane = std::fmax(0.01F, distance - radius * 4.0F);
    camera.farPlane = distance + radius * 6.0F + 10.0F;
    return camera;
}

}  // namespace fe::rendering
