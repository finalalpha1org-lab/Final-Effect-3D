// SPDX-License-Identifier: GPL-3.0-only
#include "finaleffect/engine/DemoScene.hpp"

#include <utility>

namespace fe::engine {

core::Status buildDemoScene(scene::Scene& scene) {
    scene.clear();

    core::Result<scene::ObjectId> ground = scene.createObject("Ground");
    if (!ground.ok()) {
        return ground.error();
    }
    {
        scene::SceneObject* object = scene.find(ground.value());
        object->mesh = "plane";
        object->color = math::Vec3(0.30F, 0.33F, 0.36F);
        object->localTransform.scale = math::Vec3(6.0F, 1.0F, 6.0F);
    }

    core::Result<scene::ObjectId> cube = scene.createObject("Cube");
    if (!cube.ok()) {
        return cube.error();
    }
    {
        scene::SceneObject* object = scene.find(cube.value());
        object->mesh = "cube";
        object->color = math::Vec3(0.85F, 0.42F, 0.25F);
        object->localTransform.position = math::Vec3(-1.4F, 0.6F, 0.0F);
        object->localTransform.rotation = math::Quat::fromEulerAngles(0.6F, 0.25F, 0.0F);
    }

    core::Result<scene::ObjectId> sphere = scene.createObject("Sphere");
    if (!sphere.ok()) {
        return sphere.error();
    }
    {
        scene::SceneObject* object = scene.find(sphere.value());
        object->mesh = "sphere";
        object->color = math::Vec3(0.35F, 0.62F, 0.92F);
        object->localTransform.position = math::Vec3(1.5F, 0.85F, 0.4F);
        object->localTransform.scale = math::Vec3(1.7F, 1.7F, 1.7F);
    }

    // A child of the sphere: proves that hierarchy, world matrices and project
    // round-trips work, without inventing a fake graph.
    core::Result<scene::ObjectId> satellite = scene.createObject("Satellite", sphere.value());
    if (!satellite.ok()) {
        return satellite.error();
    }
    {
        scene::SceneObject* object = scene.find(satellite.value());
        object->mesh = "cube";
        object->color = math::Vec3(0.92F, 0.85F, 0.35F);
        object->localTransform.position = math::Vec3(0.0F, 0.85F, 0.0F);
        object->localTransform.scale = math::Vec3(0.35F, 0.35F, 0.35F);
    }

    return core::successStatus();
}

}  // namespace fe::engine
