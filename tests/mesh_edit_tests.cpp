// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/scene/commands.hpp"
#include "finaleffect/scene/editable_mesh.hpp"
#include "finaleffect/scene/primitives.hpp"
#include "finaleffect/scene/scene.hpp"
#include "test_harness.hpp"

#include <cmath>
#include <memory>

namespace {

using finaleffect::math::Mat4;
using finaleffect::math::Ray;
using finaleffect::math::Vec3;
using finaleffect::scene::CommandStack;
using finaleffect::scene::EditableMesh;
using finaleffect::scene::EditMode;
using finaleffect::scene::MeshElementSelection;
using finaleffect::scene::ModifyMeshCommand;
using finaleffect::scene::Primitives;
using finaleffect::scene::Scene;

FE_TEST(MeshEdit, EditablePrimitivesAndTriangulationAreManifold) {
    EditableMesh cube = EditableMesh::createCube(2.0f);
    FE_EXPECT_TRUE(cube.validateAndRebuild().hasValue());
    FE_EXPECT_EQ(cube.vertexCount(), static_cast<std::size_t>(8));
    FE_EXPECT_EQ(cube.edgeCount(), static_cast<std::size_t>(12));
    FE_EXPECT_EQ(cube.faceCount(), static_cast<std::size_t>(6));

    const auto tri_cube = cube.toMeshData();
    FE_EXPECT_TRUE(tri_cube.validate().hasValue());
    FE_EXPECT_EQ(tri_cube.vertices.size(), static_cast<std::size_t>(24));
    FE_EXPECT_EQ(tri_cube.indices.size(), static_cast<std::size_t>(36));
    FE_EXPECT_EQ(tri_cube.triangle_face_ids.size(), static_cast<std::size_t>(12));
    FE_EXPECT_EQ(tri_cube.submeshes.size(), static_cast<std::size_t>(1));

    EditableMesh pyramid = EditableMesh::createPyramid(1.5f, 2.0f);
    FE_EXPECT_TRUE(pyramid.validateAndRebuild().hasValue());
    FE_EXPECT_EQ(pyramid.vertexCount(), static_cast<std::size_t>(5));
    FE_EXPECT_EQ(pyramid.edgeCount(), static_cast<std::size_t>(8));
    FE_EXPECT_EQ(pyramid.faceCount(), static_cast<std::size_t>(5));

    EditableMesh grid = EditableMesh::createGridPlane(4.0f, 4);
    FE_EXPECT_TRUE(grid.validateAndRebuild().hasValue());
    FE_EXPECT_EQ(grid.vertexCount(), static_cast<std::size_t>(25));
    FE_EXPECT_EQ(grid.faceCount(), static_cast<std::size_t>(16));

    EditableMesh sphere = EditableMesh::createUvSphere(1.0f, 8, 12);
    FE_EXPECT_TRUE(sphere.validateAndRebuild().hasValue());
    FE_EXPECT_TRUE(sphere.toMeshData().validate().hasValue());

    // Triangle-soup welding via fromMeshData
    auto welded = EditableMesh::fromMeshData(Primitives::createCube(1.0f));
    FE_EXPECT_TRUE(welded.hasValue());
    FE_EXPECT_EQ(welded.value().vertexCount(), static_cast<std::size_t>(8));
    FE_EXPECT_EQ(welded.value().faceCount(), static_cast<std::size_t>(12));
}

FE_TEST(MeshEdit, ElementSelectionAndViewportPicking) {
    MeshElementSelection sel;
    FE_EXPECT_TRUE(sel.empty());

    sel.toggleOrSetVertex(2, false);
    FE_EXPECT_EQ(sel.vertices.size(), static_cast<std::size_t>(1));
    sel.toggleOrSetVertex(5, true);
    FE_EXPECT_EQ(sel.vertices.size(), static_cast<std::size_t>(2));
    sel.toggleOrSetVertex(2, true); // toggle off
    FE_EXPECT_EQ(sel.vertices.size(), static_cast<std::size_t>(1));
    FE_EXPECT_EQ(sel.vertices[0], 5u);

    EditableMesh cube = EditableMesh::createCube(2.0f); // [-1, 1]^3
    const auto tri = cube.toMeshData();
    const Mat4 world = Mat4::identity();

    // Ray straight down onto +Y top face (face 4)
    const Ray top_ray({0.0f, 5.0f, 0.0f}, {0.0f, -1.0f, 0.0f});
    const auto picked_face = cube.pickFace(tri, world, top_ray);
    FE_EXPECT_TRUE(picked_face.has_value());
    FE_EXPECT_EQ(picked_face.value(), 4u);

    // Ray aimed directly at front-top-right corner (1, 1, 1) -> vertex 2
    const Ray corner_ray({1.0f, 1.0f, 5.0f}, {0.0f, 0.0f, -1.0f});
    const auto picked_vert = cube.pickVertex(tri, world, corner_ray, 0.15f);
    FE_EXPECT_TRUE(picked_vert.has_value());
    FE_EXPECT_EQ(picked_vert.value(), 2u);

    // Ray aimed at midpoint of top-front edge (0, 1, 1)
    const Ray edge_ray({0.0f, 1.0f, 5.0f}, {0.0f, 0.0f, -1.0f});
    const auto picked_edge = cube.pickEdge(tri, world, edge_ray, 0.15f);
    FE_EXPECT_TRUE(picked_edge.has_value());
    const auto& e = cube.edges()[picked_edge.value()];
    FE_EXPECT_TRUE((e.v0 == 2u && e.v1 == 3u) || (e.v0 == 3u && e.v1 == 2u));

    // Overlay mesh generation produces non-empty highlight geometry in Vertex/Edge/Face modes
    sel.clear();
    sel.toggleOrSetFace(4, false);
    const auto face_overlay = cube.buildEditOverlayMesh(EditMode::Face, sel, world, 1.0f);
    FE_EXPECT_TRUE(!face_overlay.vertices.empty());
    FE_EXPECT_TRUE(cube.buildEditOverlayMesh(EditMode::Object, sel, world, 1.0f).vertices.empty());
}

FE_TEST(MeshEdit, VertexEdgeAndFaceTransformsAndBoundsUpdate) {
    EditableMesh cube = EditableMesh::createCube(2.0f);

    // Move top face (+Y, face 4) upward by +1.0 -> max.y becomes 2.0
    const std::uint32_t top_face = 4;
    FE_EXPECT_TRUE(cube.translateFaces({&top_face, 1}, Vec3{0.0f, 1.0f, 0.0f}).hasValue());
    const auto bounds = cube.toMeshData().computeBounds();
    FE_EXPECT_NEAR(bounds.max.y, 2.0f, 1.0e-4f);
    FE_EXPECT_NEAR(bounds.min.y, -1.0f, 1.0e-4f);

    // Move single vertex 2 outward in +X
    const std::uint32_t v2 = 2;
    FE_EXPECT_TRUE(cube.translateVertices({&v2, 1}, Vec3{0.5f, 0.0f, 0.0f}).hasValue());
    FE_EXPECT_NEAR(cube.vertices()[2].position.x, 1.5f, 1.0e-4f);

    // Move edge 0 in -Z
    const std::uint32_t e0 = 0;
    FE_EXPECT_TRUE(cube.translateEdges({&e0, 1}, Vec3{0.0f, 0.0f, -0.25f}).hasValue());
    FE_EXPECT_TRUE(cube.validateAndRebuild().hasValue());
}

FE_TEST(MeshEdit, FaceExtrusionDeletionEdgeDissolveAndBoundaryFill) {
    EditableMesh cube = EditableMesh::createCube(2.0f);

    // Extruding ALL 6 faces of a closed shell has no boundary and must be cleanly rejected without mutation!
    const std::uint32_t all_faces[6] = {0, 1, 2, 3, 4, 5};
    auto bad_extrude = cube.extrudeFaces(all_faces, 1.0f);
    FE_EXPECT_TRUE(!bad_extrude.hasValue());
    FE_EXPECT_EQ(cube.vertexCount(), static_cast<std::size_t>(8));
    FE_EXPECT_EQ(cube.faceCount(), static_cast<std::size_t>(6));

    // Extrude top face (+Y, face 4) by +1.5 -> adds 4 cap verts & 4 wall quads (12 verts, 10 faces)
    const std::uint32_t top_face = 4;
    auto extrude_res = cube.extrudeFaces({&top_face, 1}, 1.5f);
    FE_EXPECT_TRUE(extrude_res.hasValue());
    FE_EXPECT_EQ(cube.vertexCount(), static_cast<std::size_t>(12));
    FE_EXPECT_EQ(cube.faceCount(), static_cast<std::size_t>(10));
    FE_EXPECT_NEAR(cube.toMeshData().computeBounds().max.y, 2.5f, 1.0e-4f);

    // Delete top cap face (face 4) -> creates an open 4-edge boundary loop (9 faces, 12 verts)
    FE_EXPECT_TRUE(cube.deleteFaces({&top_face, 1}).hasValue());
    FE_EXPECT_EQ(cube.faceCount(), static_cast<std::size_t>(9));

    // Fill the open boundary loop -> closes the shell back to 10 manifold faces!
    auto fill_res = cube.fillBoundaryLoop();
    FE_EXPECT_TRUE(fill_res.hasValue());
    FE_EXPECT_EQ(cube.faceCount(), static_cast<std::size_t>(10));
    FE_EXPECT_TRUE(cube.validateAndRebuild().hasValue());

    // Calling fillBoundaryLoop on an already closed manifold shell fails cleanly without mutation.
    FE_EXPECT_TRUE(!cube.fillBoundaryLoop().hasValue());
    FE_EXPECT_EQ(cube.faceCount(), static_cast<std::size_t>(10));

    // Edge dissolve on a 2x1 planar grid merges two adjacent quads into one 6-corner polygon!
    EditableMesh grid = EditableMesh::createGridPlane(2.0f, 2); // 4 quads
    FE_EXPECT_EQ(grid.faceCount(), static_cast<std::size_t>(4));
    std::optional<std::uint32_t> shared_edge;
    for (std::size_t ei = 0; ei < grid.edges().size(); ++ei) {
        if (grid.edges()[ei].adjacent_faces.size() == 2) {
            shared_edge = static_cast<std::uint32_t>(ei);
            break;
        }
    }
    FE_EXPECT_TRUE(shared_edge.has_value());
    FE_EXPECT_TRUE(grid.deleteEdges({&shared_edge.value(), 1}).hasValue());
    FE_EXPECT_EQ(grid.faceCount(), static_cast<std::size_t>(3));

    // Vertex deletion removes incident faces cleanly while preserving remaining valid faces
    const std::uint32_t corner_v = 0;
    FE_EXPECT_TRUE(grid.deleteVertices({&corner_v, 1}).hasValue());
    FE_EXPECT_TRUE(grid.faceCount() >= static_cast<std::size_t>(1));
    FE_EXPECT_TRUE(grid.validateAndRebuild().hasValue());
}

FE_TEST(MeshEdit, CorruptingMutationsAreRejectedWithoutMutatingMesh) {
    EditableMesh cube = EditableMesh::createCube(2.0f);
    const auto orig_verts = cube.vertices();
    const auto orig_faces = cube.faces();

    // Scale all vertices of face 0 to zero at the origin -> zero-area degenerate face -> rejected!
    const std::uint32_t face0_verts[4] = {0, 1, 2, 3};
    const auto zero_scale = Mat4::scale(Vec3{0.0f, 0.0f, 0.0f});
    const auto st = cube.transformVertices(face0_verts, zero_scale);
    FE_EXPECT_TRUE(!st.hasValue());

    // Verify original mesh was left 100% intact
    FE_EXPECT_EQ(cube.vertexCount(), orig_verts.size());
    FE_EXPECT_EQ(cube.faceCount(), orig_faces.size());
    for (std::size_t i = 0; i < orig_verts.size(); ++i) {
        FE_EXPECT_TRUE(cube.vertices()[i].position.approxEqual(orig_verts[i].position, 1.0e-6f));
    }

    // Deleting all faces is cleanly rejected
    const std::uint32_t all_f[6] = {0, 1, 2, 3, 4, 5};
    FE_EXPECT_TRUE(!cube.deleteFaces(all_f).hasValue());
    FE_EXPECT_EQ(cube.faceCount(), static_cast<std::size_t>(6));
}

FE_TEST(MeshEdit, ModifyMeshCommandUndoRedoAndCoalescing) {
    Scene scene("MeshEditCmdScene");
    const auto mesh_id = scene.registerEditableMesh("EditableCube", EditableMesh::createCube(2.0f)).value();
    const std::uint64_t initial_rev = scene.findMesh(mesh_id)->revision;

    CommandStack stack;

    // Coalesced interactive vertex drag across 3 incremental steps
    for (int step = 1; step <= 3; ++step) {
        EditableMesh edited = scene.findMesh(mesh_id)->editable_mesh.value();
        const std::uint32_t top_face = 4;
        FE_EXPECT_TRUE(edited.translateFaces({&top_face, 1}, Vec3{0.0f, 0.25f, 0.0f}).hasValue());
        FE_EXPECT_TRUE(stack.execute(
            scene,
            std::make_unique<ModifyMeshCommand>(mesh_id, std::move(edited), "Move Face", true, 42)).hasValue());
    }

    FE_EXPECT_EQ(stack.undoCount(), static_cast<std::size_t>(1));
    FE_EXPECT_NEAR(scene.findMesh(mesh_id)->local_bounds.max.y, 1.75f, 1.0e-4f);
    FE_EXPECT_TRUE(scene.findMesh(mesh_id)->revision > initial_rev);

    // Discrete extrude command does NOT coalesce with the move drag
    {
        EditableMesh extruded = scene.findMesh(mesh_id)->editable_mesh.value();
        const std::uint32_t top_face = 4;
        FE_EXPECT_TRUE(extruded.extrudeFaces({&top_face, 1}, 0.5f).hasValue());
        FE_EXPECT_TRUE(stack.execute(
            scene,
            std::make_unique<ModifyMeshCommand>(mesh_id, std::move(extruded), "Extrude Face", false, 0)).hasValue());
    }
    FE_EXPECT_EQ(stack.undoCount(), static_cast<std::size_t>(2));
    FE_EXPECT_EQ(scene.findMesh(mesh_id)->editable_mesh->faceCount(), static_cast<std::size_t>(10));

    // Undo extrude -> back to 6 faces at y = 1.75
    FE_EXPECT_TRUE(stack.undo(scene).hasValue());
    FE_EXPECT_EQ(scene.findMesh(mesh_id)->editable_mesh->faceCount(), static_cast<std::size_t>(6));
    FE_EXPECT_NEAR(scene.findMesh(mesh_id)->local_bounds.max.y, 1.75f, 1.0e-4f);

    // Undo coalesced move -> back to original cube at y = 1.0
    FE_EXPECT_TRUE(stack.undo(scene).hasValue());
    FE_EXPECT_NEAR(scene.findMesh(mesh_id)->local_bounds.max.y, 1.0f, 1.0e-4f);

    // Redo both
    FE_EXPECT_TRUE(stack.redo(scene).hasValue());
    FE_EXPECT_TRUE(stack.redo(scene).hasValue());
    FE_EXPECT_EQ(scene.findMesh(mesh_id)->editable_mesh->faceCount(), static_cast<std::size_t>(10));
    FE_EXPECT_NEAR(scene.findMesh(mesh_id)->local_bounds.max.y, 2.25f, 1.0e-4f);
}

} // namespace

int main(int argc, char** argv) {
    return finaleffect::testing::runAllTests(argc, argv);
}
