// SPDX-License-Identifier: GPL-3.0-only
#include <fstream>
#include <iterator>
#include <string>

#include "finaleffect/platform/FileSystem.hpp"

#include "TestFramework.hpp"
#include "finaleffect/rendering/ImageIO.hpp"
#include "finaleffect/rendering/MeshLibrary.hpp"
#include "finaleffect/rendering/RenderViewBuilder.hpp"
#include "finaleffect/rendering/SoftwareRenderer.hpp"

namespace {
using fe::rendering::ImageBuffer;
using fe::rendering::MeshLibrary;
using fe::rendering::RendererInitInfo;
using fe::rendering::SoftwareRenderer;

bool meshIsConsistent(const fe::rendering::MeshData& mesh) {
    if (!mesh.isValid()) {
        return false;
    }
    for (const std::uint32_t index : mesh.indices) {
        if (index >= mesh.vertices.size()) {
            return false;
        }
    }
    return !mesh.vertices.empty();
}

fe::scene::Scene makeSampleScene() {
    fe::scene::Scene scene;
    const fe::scene::ObjectId ground = scene.createObject("Ground").value();
    scene.find(ground)->mesh = "plane";
    scene.find(ground)->localTransform.scale = fe::math::Vec3(4.0F, 1.0F, 4.0F);
    const fe::scene::ObjectId cube = scene.createObject("Cube").value();
    scene.find(cube)->mesh = "cube";
    scene.find(cube)->localTransform.position = fe::math::Vec3(0.0F, 0.5F, 0.0F);
    return scene;
}
}  // namespace

FE3D_TEST(rendering, mesh_library_provides_consistent_primitives) {
    const MeshLibrary library;
    FE3D_CHECK_EQ(library.size(), std::size_t(3));
    FE3D_CHECK(library.contains("cube"));
    FE3D_CHECK(library.contains("plane"));
    FE3D_CHECK(library.contains("sphere"));
    FE3D_CHECK(!library.contains("teapot"));
    FE3D_CHECK(library.find("missing") == nullptr);
    FE3D_CHECK_EQ(library.names().size(), std::size_t(3));

    for (const std::string& name : library.names()) {
        const fe::rendering::MeshData* mesh = library.find(name);
        FE3D_REQUIRE(mesh != nullptr);
        FE3D_CHECK_MSG(meshIsConsistent(*mesh), "inconsistent primitive: " + name);
    }

    const fe::rendering::MeshData* cube = library.find("cube");
    FE3D_CHECK_EQ(cube->vertices.size(), std::size_t(24));
    FE3D_CHECK_EQ(cube->triangleCount(), std::size_t(12));
    for (const fe::rendering::Vertex& vertex : cube->vertices) {
        FE3D_CHECK_NEAR(vertex.position.length(), 0.8660254F, 1.0e-4F);  // half-diagonal of a unit cube
    }

    // Degenerate generation arguments must not produce broken meshes.
    FE3D_CHECK(meshIsConsistent(MeshLibrary::makePlane(1.0F, 0)));
    FE3D_CHECK(meshIsConsistent(MeshLibrary::makeSphere(0.5F, 1, 1)));
    FE3D_CHECK(meshIsConsistent(MeshLibrary::makeSphere(0.5F, 0, 0)));
}

FE3D_TEST(rendering, render_view_builder_translates_scene) {
    const MeshLibrary library;
    const fe::scene::Scene scene = makeSampleScene();
    const fe::rendering::Camera camera = fe::rendering::frameScene(scene, library);
    const fe::core::Result<fe::rendering::RenderView> view =
        fe::rendering::buildRenderView(scene, camera, library);
    FE3D_REQUIRE(view.ok());
    FE3D_CHECK_EQ(view.value().items.size(), std::size_t(2));
    FE3D_CHECK(view.value().items[0].mesh != nullptr);
    FE3D_CHECK(view.value().items[0].worldMatrix.isFinite());

    fe::scene::Scene withUnknownMesh = scene;
    withUnknownMesh.find(withUnknownMesh.roots()[0])->mesh = "nonesuch";
    const fe::core::Result<fe::rendering::RenderView> broken =
        fe::rendering::buildRenderView(withUnknownMesh, camera, library);
    FE3D_CHECK(!broken.ok());
    FE3D_CHECK(broken.error().code() == fe::core::ErrorCode::NotFound);

    fe::scene::Scene empty;
    const fe::core::Result<fe::rendering::RenderView> emptyView =
        fe::rendering::buildRenderView(empty, camera, library);
    FE3D_REQUIRE(emptyView.ok());
    FE3D_CHECK(emptyView.value().items.empty());
}

FE3D_TEST(rendering, software_renderer_draws_geometry) {
    SoftwareRenderer renderer;
    RendererInitInfo info;
    info.width = 128;
    info.height = 96;
    FE3D_REQUIRE(renderer.initialize(info).ok());
    FE3D_CHECK(renderer.isInitialized());
    FE3D_CHECK_EQ(renderer.width(), std::uint32_t(128));
    FE3D_CHECK_EQ(renderer.capabilities().backendName, std::string("cpu-rasterizer"));
    FE3D_CHECK(renderer.capabilities().supportsReadback);

    const MeshLibrary library;
    const fe::scene::Scene scene = makeSampleScene();
    const fe::core::Result<fe::rendering::RenderView> view =
        fe::rendering::buildRenderView(scene, fe::rendering::frameScene(scene, library), library);
    FE3D_REQUIRE(view.ok());

    fe::rendering::FrameInfo frame;
    frame.clearColor = fe::math::Vec3(0.05F, 0.05F, 0.10F);
    FE3D_REQUIRE(renderer.beginFrame(frame).ok());
    FE3D_REQUIRE(renderer.submit(view.value()).ok());
    FE3D_REQUIRE(renderer.endFrame().ok());

    FE3D_CHECK(renderer.lastFrameTriangleCount() > 0);
    FE3D_CHECK(renderer.lastFrameShadedPixelCount() > 0);

    ImageBuffer image;
    FE3D_REQUIRE(renderer.readbackImage(image).ok());
    FE3D_CHECK_EQ(image.width, std::uint32_t(128));
    FE3D_CHECK_EQ(image.height, std::uint32_t(96));
    FE3D_CHECK(!image.isEmpty());
    FE3D_CHECK(!fe::rendering::isCompletelyBlack(image));
    FE3D_CHECK(fe::rendering::countNonBackgroundPixels(image) > 500);

    // The centre of the frame is inside the rendered geometry.
    const std::size_t centre = (static_cast<std::size_t>(image.height / 2U) * image.width + image.width / 2U) * 4U;
    FE3D_CHECK(image.rgba[centre] > 5U || image.rgba[centre + 1U] > 5U || image.rgba[centre + 2U] > 5U);

    FE3D_REQUIRE(renderer.shutdown().ok());
    FE3D_CHECK(!renderer.isInitialized());
    FE3D_CHECK(renderer.shutdown().ok());  // second shutdown is safe
}

FE3D_TEST(rendering, software_renderer_validates_call_order_and_sizes) {
    SoftwareRenderer renderer;
    RendererInitInfo info;
    info.width = 0;
    info.height = 10;
    FE3D_CHECK(!renderer.initialize(info).ok());

    info.width = 32;
    info.height = 32;
    FE3D_REQUIRE(renderer.initialize(info).ok());
    FE3D_CHECK(!renderer.resize(0, 32).ok());

    const fe::rendering::RenderView emptyView;
    FE3D_CHECK(!renderer.submit(emptyView).ok());  // outside a frame
    FE3D_REQUIRE(renderer.beginFrame(fe::rendering::FrameInfo{}).ok());
    FE3D_CHECK(!renderer.beginFrame(fe::rendering::FrameInfo{}).ok());  // nested begin
    FE3D_REQUIRE(renderer.submit(emptyView).ok());
    FE3D_REQUIRE(renderer.endFrame().ok());
    FE3D_CHECK(!renderer.endFrame().ok());

    ImageBuffer image;
    FE3D_CHECK(renderer.readbackImage(image).ok());

    SoftwareRenderer blank;
    ImageBuffer untouched;
    FE3D_CHECK(!blank.readbackImage(untouched).ok());  // not initialized
    FE3D_REQUIRE(blank.initialize(RendererInitInfo{}).ok());
}

FE3D_TEST(rendering, ppm_output_matches_expected_layout) {
    SoftwareRenderer renderer;
    RendererInitInfo info;
    info.width = 16;
    info.height = 8;
    FE3D_REQUIRE(renderer.initialize(info).ok());

    bool ok = renderer.beginFrame(fe::rendering::FrameInfo{}).ok();
    FE3D_REQUIRE(ok);
    ok = renderer.endFrame().ok();
    FE3D_REQUIRE(ok);

    ImageBuffer image;
    FE3D_REQUIRE(renderer.readbackImage(image).ok());

    const std::string path = "fe3d-rendering-test.ppm";
    FE3D_REQUIRE(fe::rendering::writePpm(image, path).ok());

    std::ifstream file(path, std::ios::binary);
    FE3D_REQUIRE(file.is_open());
    std::string header;
    std::getline(file, header);
    FE3D_CHECK_EQ(header, std::string("P6"));
    std::string dimensions;
    std::getline(file, dimensions);
    FE3D_CHECK_EQ(dimensions, std::string("16 8"));
    std::string maximum;
    std::getline(file, maximum);
    FE3D_CHECK_EQ(maximum, std::string("255"));

    std::string payload((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();
    FE3D_CHECK_EQ(payload.size(), std::size_t(16 * 8 * 3));
    FE3D_CHECK(!fe::rendering::isCompletelyBlack(image));

    ImageBuffer empty;
    FE3D_CHECK(!fe::rendering::writePpm(empty, path).ok());
    FE3D_CHECK(!fe::rendering::writePpm(image, "fe3d-missing-directory/frame.ppm").ok());

    (void)fe::platform::StdFileSystem{}.removeFile(path);
}

FE3D_TEST(rendering, frame_scene_camera_sees_the_bounds) {
    const MeshLibrary library;
    const fe::scene::Scene scene = makeSampleScene();
    const fe::rendering::Bounds bounds = fe::rendering::computeSceneBounds(scene, library);
    FE3D_REQUIRE(bounds.valid);
    FE3D_CHECK(bounds.max.x >= bounds.min.x);
    FE3D_CHECK(bounds.halfDiagonal() > 0.0F);

    const fe::rendering::Camera camera = fe::rendering::frameScene(scene, library);
    FE3D_CHECK(camera.position.isFinite());
    FE3D_CHECK((camera.position - camera.target).length() > 1.0F);
    FE3D_CHECK(camera.target.isApprox(bounds.center(), 1.0e-2F));
    FE3D_CHECK(camera.farPlane > camera.nearPlane);

    fe::scene::Scene empty;
    const fe::rendering::Camera defaultCamera = fe::rendering::frameScene(empty, library);
    FE3D_CHECK(defaultCamera.position.isFinite());
    FE3D_CHECK_NEAR((defaultCamera.position - defaultCamera.target).length(), 5.3851648F, 1.0e-3F);
}
