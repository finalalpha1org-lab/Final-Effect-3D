// SPDX-License-Identifier: GPL-3.0-only
#include <cstdint>
#include <string>
#include <vector>

#include "TestFramework.hpp"
#include "finaleffect/scene/Scene.hpp"

namespace {
using fe::scene::ObjectId;
using fe::scene::Scene;
using fe::scene::SceneObject;
}  // namespace

FE3D_TEST(scene, create_lookup_and_count) {
    Scene scene;
    FE3D_CHECK_EQ(scene.objectCount(), std::size_t(0));
    FE3D_CHECK(scene.roots().empty());

    const fe::core::Result<ObjectId> first = scene.createObject("Camera");
    const fe::core::Result<ObjectId> second = scene.createObject("Cube");
    FE3D_REQUIRE(first.ok());
    FE3D_REQUIRE(second.ok());
    FE3D_CHECK(first.value() != second.value());
    FE3D_CHECK_EQ(scene.objectCount(), std::size_t(2));
    FE3D_CHECK_EQ(scene.roots().size(), std::size_t(2));

    FE3D_REQUIRE(scene.find(first.value()) != nullptr);
    FE3D_CHECK_EQ(scene.find(first.value())->name, std::string("Camera"));
    FE3D_CHECK(scene.contains(second.value()));
    FE3D_CHECK(!scene.contains(ObjectId{9999}));
    FE3D_CHECK(scene.find(ObjectId{9999}) == nullptr);
    FE3D_CHECK(!scene.createObject("").ok());
    FE3D_CHECK(!scene.createObject("Orphan", ObjectId{4242}).ok());
}

FE3D_TEST(scene, hierarchy_parenting_and_world_matrices) {
    Scene scene;
    const ObjectId parent = scene.createObject("Parent").value();
    const ObjectId child = scene.createObject("Child", parent).value();
    const ObjectId grandChild = scene.createObject("GrandChild", child).value();

    scene.find(parent)->localTransform.position = fe::math::Vec3(1.0F, 0.0F, 0.0F);
    scene.find(child)->localTransform.position = fe::math::Vec3(0.0F, 2.0F, 0.0F);
    scene.find(grandChild)->localTransform.position = fe::math::Vec3(0.0F, 0.0F, 3.0F);

    FE3D_CHECK_EQ(scene.find(parent)->children.size(), std::size_t(1));
    FE3D_CHECK_EQ(scene.find(child)->children.size(), std::size_t(1));
    FE3D_CHECK_EQ(scene.roots().size(), std::size_t(1));
    FE3D_CHECK_EQ(scene.depthOf(parent), std::size_t(0));
    FE3D_CHECK_EQ(scene.depthOf(grandChild), std::size_t(2));
    FE3D_CHECK(scene.isAncestorOf(parent, grandChild));
    FE3D_CHECK(!scene.isAncestorOf(grandChild, parent));

    const fe::math::Vec3 world = scene.worldMatrix(grandChild).translation();
    FE3D_CHECK(world.isApprox(fe::math::Vec3(1.0F, 2.0F, 3.0F), 1.0e-4F));

    // Rotation of a parent must move the child's world position.
    scene.find(parent)->localTransform.rotation = fe::math::Quat::fromAxisAngle(fe::math::Vec3::unitY(), 1.5707963F);
    // Rotating the parent by +90 degrees about +Y maps the grandchild's local +Z
    // offset onto +X: (1,0,0) + (0,2,0) + (3,0,0) = (4,2,0).
    const fe::math::Vec3 rotated = scene.worldMatrix(grandChild).translation();
    FE3D_CHECK(rotated.isApprox(fe::math::Vec3(4.0F, 2.0F, 0.0F), 1.0e-3F));
}

FE3D_TEST(scene, reparenting_rules_and_cycles) {
    Scene scene;
    const ObjectId a = scene.createObject("A").value();
    const ObjectId b = scene.createObject("B").value();
    const ObjectId c = scene.createObject("C", b).value();

    FE3D_CHECK(scene.setParent(c, a).ok());
    FE3D_CHECK_EQ(scene.find(b)->children.size(), std::size_t(0));
    FE3D_CHECK_EQ(scene.find(a)->children.size(), std::size_t(1));

    const fe::core::Status selfParent = scene.setParent(a, a);
    FE3D_CHECK(!selfParent.ok());
    FE3D_CHECK(selfParent.error().code() == fe::core::ErrorCode::CycleDetected);

    FE3D_CHECK(scene.setParent(b, c).ok());       // c is a child of a, so b under c is fine
    const fe::core::Status cycle = scene.setParent(a, b);  // a -> c -> b, so b under a is a cycle
    FE3D_CHECK(!cycle.ok());
    FE3D_CHECK(cycle.error().code() == fe::core::ErrorCode::CycleDetected);

    FE3D_CHECK(!scene.setParent(ObjectId{7}, a).ok());
    FE3D_CHECK(!scene.setParent(a, ObjectId{7}).ok());
    // Failed reparenting must not modify the hierarchy.
    FE3D_CHECK_EQ(scene.find(a)->parent.value, std::uint64_t(0));
    FE3D_CHECK(scene.setParent(c, a).ok());  // re-assigning the current parent is a no-op
    FE3D_CHECK_EQ(scene.find(a)->children.size(), std::size_t(1));
    FE3D_CHECK(scene.isAncestorOf(a, c));
    // Moving a subtree back to the root keeps it reachable but detached.
    FE3D_CHECK(scene.setParent(c, fe::scene::kInvalidObjectId).ok());
    FE3D_CHECK_EQ(scene.roots().size(), std::size_t(2));
    FE3D_CHECK(!scene.isAncestorOf(a, c));
    FE3D_CHECK(scene.isAncestorOf(c, b));
    FE3D_CHECK(scene.validate().ok());
}

FE3D_TEST(scene, destroy_removes_descendants_and_keeps_other_objects) {
    Scene scene;
    const ObjectId root = scene.createObject("Root").value();
    const ObjectId child = scene.createObject("Child", root).value();
    const ObjectId grandChild = scene.createObject("GrandChild", child).value();
    const ObjectId keeper = scene.createObject("Keeper").value();

    FE3D_CHECK(scene.destroyObject(child).ok());
    FE3D_CHECK_EQ(scene.objectCount(), std::size_t(2));
    FE3D_CHECK(!scene.contains(child));
    FE3D_CHECK(!scene.contains(grandChild));
    FE3D_CHECK(scene.contains(root));
    FE3D_CHECK(scene.contains(keeper));
    FE3D_CHECK_EQ(scene.find(root)->children.size(), std::size_t(0));

    FE3D_CHECK(!scene.destroyObject(child).ok());
    FE3D_CHECK(scene.destroyObject(root).ok());
    FE3D_CHECK_EQ(scene.objectCount(), std::size_t(1));

    // Identifiers are never reused.
    const ObjectId fresh = scene.createObject("Fresh").value();
    FE3D_CHECK(fresh.value > grandChild.value);

    scene.clear();
    FE3D_CHECK_EQ(scene.objectCount(), std::size_t(0));
}

FE3D_TEST(scene, validate_detects_broken_data) {
    Scene healthy;
    const ObjectId parent = healthy.createObject("Parent").value();
    (void)healthy.createObject("Child", parent);
    FE3D_CHECK(healthy.validate().ok());

    std::vector<SceneObject> objects;
    SceneObject first;
    first.id = ObjectId{1};
    first.name = "First";
    objects.push_back(first);
    SceneObject duplicate;
    duplicate.id = ObjectId{1};
    duplicate.name = "Duplicate";
    objects.push_back(duplicate);
    const fe::core::Result<Scene> duplicated = Scene::fromObjects(objects);
    FE3D_CHECK(!duplicated.ok());
    FE3D_CHECK(duplicated.error().code() == fe::core::ErrorCode::ValidationFailed);

    std::vector<SceneObject> dangling;
    SceneObject orphan;
    orphan.id = ObjectId{5};
    orphan.name = "Orphan";
    orphan.parent = ObjectId{4};
    dangling.push_back(orphan);
    const fe::core::Result<Scene> orphaned = Scene::fromObjects(dangling);
    FE3D_CHECK(!orphaned.ok());
    FE3D_CHECK(orphaned.error().message().find("unknown parent") != std::string::npos);

    std::vector<SceneObject> cycle;
    SceneObject one;
    one.id = ObjectId{1};
    one.name = "One";
    one.parent = ObjectId{2};
    SceneObject two;
    two.id = ObjectId{2};
    two.name = "Two";
    two.parent = ObjectId{1};
    cycle.push_back(one);
    cycle.push_back(two);
    const fe::core::Result<Scene> cyclic = Scene::fromObjects(cycle);
    FE3D_CHECK(!cyclic.ok());
    FE3D_CHECK(cyclic.error().code() == fe::core::ErrorCode::ValidationFailed);

    std::vector<SceneObject> nameless;
    SceneObject empty;
    empty.id = ObjectId{1};
    empty.name = "";
    nameless.push_back(empty);
    FE3D_CHECK(!Scene::fromObjects(nameless).ok());

    std::vector<SceneObject> noId;
    SceneObject unset;
    unset.id = fe::scene::kInvalidObjectId;
    unset.name = "NoId";
    noId.push_back(unset);
    FE3D_CHECK(!Scene::fromObjects(noId).ok());

    std::vector<SceneObject> nonFinite;
    SceneObject broken;
    broken.id = ObjectId{9};
    broken.name = "Broken";
    broken.localTransform.position = fe::math::Vec3(0.0F, std::nanf(""), 0.0F);
    nonFinite.push_back(broken);
    FE3D_CHECK(!Scene::fromObjects(nonFinite).ok());
}
