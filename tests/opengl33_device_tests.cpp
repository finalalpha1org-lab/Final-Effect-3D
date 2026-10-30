// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors
// Verifies OpenGL33RenderDevice initialization guards, shader error handling,
// VAO/VBO/EBO lifecycle, resize behavior, and leak-free shutdown using a controlled
// in-process OpenGL 3.3 Core dispatch harness.

#include "test_harness.hpp"

#include "finaleffect/rendering/opengl33_render_device.hpp"
#include "finaleffect/rendering/renderer.hpp"
#include "finaleffect/scene/editable_mesh.hpp"
#include "finaleffect/scene/primitives.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>

#if defined(_WIN32)
#define FE_GL_APIENTRY __stdcall
#else
#define FE_GL_APIENTRY
#endif

namespace {

using namespace finaleffect;

struct MockGlState {
    bool fail_shader_compile{false};
    std::string_view missing_symbol{};
    std::uint32_t next_id{1};
    int live_vaos{0};
    int live_buffers{0};
    int live_shaders{0};
    int live_programs{0};
    int draw_elements_calls{0};
    int buffer_sub_data_calls{0};
    int clear_calls{0};
    std::int32_t last_viewport_w{0};
    std::int32_t last_viewport_h{0};
};

MockGlState g_gl{};

const std::uint8_t* FE_GL_APIENTRY mockGetString(std::uint32_t) {
    return reinterpret_cast<const std::uint8_t*>("Mock OpenGL 3.3 Core Verification Driver");
}
void FE_GL_APIENTRY mockGetIntegerv(std::uint32_t, std::int32_t* out) {
    out[0] = 8192;
    out[1] = 8192;
}
std::uint32_t FE_GL_APIENTRY mockGetError() { return 0; }
void FE_GL_APIENTRY mockViewport(std::int32_t, std::int32_t, std::int32_t w, std::int32_t h) {
    g_gl.last_viewport_w = w;
    g_gl.last_viewport_h = h;
}
void FE_GL_APIENTRY mockClearColor(float, float, float, float) {}
void FE_GL_APIENTRY mockClearDepth(double) {}
void FE_GL_APIENTRY mockClear(std::uint32_t) { ++g_gl.clear_calls; }
void FE_GL_APIENTRY mockEnable(std::uint32_t) {}
void FE_GL_APIENTRY mockDisable(std::uint32_t) {}
void FE_GL_APIENTRY mockBlendFunc(std::uint32_t, std::uint32_t) {}
void FE_GL_APIENTRY mockDepthMask(std::uint8_t) {}
void FE_GL_APIENTRY mockDepthFunc(std::uint32_t) {}
void FE_GL_APIENTRY mockCullFace(std::uint32_t) {}
void FE_GL_APIENTRY mockFrontFace(std::uint32_t) {}
void FE_GL_APIENTRY mockDrawElements(std::uint32_t, std::int32_t, std::uint32_t, const void*) {
    ++g_gl.draw_elements_calls;
}
void FE_GL_APIENTRY mockGenVertexArrays(std::int32_t n, std::uint32_t* ids) {
    for (std::int32_t i = 0; i < n; ++i) {
        ids[i] = g_gl.next_id++;
        ++g_gl.live_vaos;
    }
}
void FE_GL_APIENTRY mockBindVertexArray(std::uint32_t) {}
void FE_GL_APIENTRY mockDeleteVertexArrays(std::int32_t n, const std::uint32_t* ids) {
    for (std::int32_t i = 0; i < n; ++i) {
        if (ids[i] != 0) --g_gl.live_vaos;
    }
}
void FE_GL_APIENTRY mockGenBuffers(std::int32_t n, std::uint32_t* ids) {
    for (std::int32_t i = 0; i < n; ++i) {
        ids[i] = g_gl.next_id++;
        ++g_gl.live_buffers;
    }
}
void FE_GL_APIENTRY mockBindBuffer(std::uint32_t, std::uint32_t) {}
void FE_GL_APIENTRY mockBufferData(std::uint32_t, std::ptrdiff_t, const void*, std::uint32_t) {}
void FE_GL_APIENTRY mockBufferSubData(std::uint32_t, std::ptrdiff_t, std::ptrdiff_t, const void*) {
    ++g_gl.buffer_sub_data_calls;
}
void FE_GL_APIENTRY mockDeleteBuffers(std::int32_t n, const std::uint32_t* ids) {
    for (std::int32_t i = 0; i < n; ++i) {
        if (ids[i] != 0) --g_gl.live_buffers;
    }
}
void FE_GL_APIENTRY mockVertexAttribPointer(
    std::uint32_t, std::int32_t, std::uint32_t, std::uint8_t, std::int32_t, const void*) {}
void FE_GL_APIENTRY mockEnableVertexAttribArray(std::uint32_t) {}

std::uint32_t FE_GL_APIENTRY mockCreateShader(std::uint32_t) {
    ++g_gl.live_shaders;
    return g_gl.next_id++;
}
void FE_GL_APIENTRY mockShaderSource(std::uint32_t, std::int32_t, const char* const*, const std::int32_t*) {}
void FE_GL_APIENTRY mockCompileShader(std::uint32_t) {}
void FE_GL_APIENTRY mockGetShaderiv(std::uint32_t, std::uint32_t pname, std::int32_t* out) {
    if (pname == 0x8B81u) { // GL_COMPILE_STATUS
        *out = g_gl.fail_shader_compile ? 0 : 1;
    } else {
        *out = 24;
    }
}
void FE_GL_APIENTRY mockGetShaderInfoLog(std::uint32_t, std::int32_t max_len, std::int32_t*, char* log) {
    const char* msg = "Simulated GLSL error";
    std::strncpy(log, msg, static_cast<std::size_t>(max_len));
    if (max_len > 0) log[max_len - 1] = '\0';
}
void FE_GL_APIENTRY mockDeleteShader(std::uint32_t id) {
    if (id != 0) --g_gl.live_shaders;
}

std::uint32_t FE_GL_APIENTRY mockCreateProgram() {
    ++g_gl.live_programs;
    return g_gl.next_id++;
}
void FE_GL_APIENTRY mockAttachShader(std::uint32_t, std::uint32_t) {}
void FE_GL_APIENTRY mockLinkProgram(std::uint32_t) {}
void FE_GL_APIENTRY mockGetProgramiv(std::uint32_t, std::uint32_t pname, std::int32_t* out) {
    *out = (pname == 0x8B82u) ? 1 : 0;
}
void FE_GL_APIENTRY mockGetProgramInfoLog(std::uint32_t, std::int32_t, std::int32_t*, char*) {}
void FE_GL_APIENTRY mockUseProgram(std::uint32_t) {}
void FE_GL_APIENTRY mockDeleteProgram(std::uint32_t id) {
    if (id != 0) --g_gl.live_programs;
}
std::int32_t FE_GL_APIENTRY mockGetUniformLocation(std::uint32_t, const char*) { return 1; }
void FE_GL_APIENTRY mockUniformMatrix4fv(std::int32_t, std::int32_t, std::uint8_t, const float*) {}
void FE_GL_APIENTRY mockUniform3fv(std::int32_t, std::int32_t, const float*) {}

template <typename Fn>
platform::GlProcAddressFn toProc(Fn fn) noexcept {
    platform::GlProcAddressFn out = nullptr;
    static_assert(sizeof(out) == sizeof(fn));
    std::memcpy(&out, &fn, sizeof(out));
    return out;
}

platform::GlProcAddressFn mockGlLoader(const char* name) {
    const std::string_view sym(name);
    if (!g_gl.missing_symbol.empty() && sym == g_gl.missing_symbol) {
        return nullptr;
    }
    if (sym == "glGetString") return toProc(&mockGetString);
    if (sym == "glGetIntegerv") return toProc(&mockGetIntegerv);
    if (sym == "glGetError") return toProc(&mockGetError);
    if (sym == "glViewport") return toProc(&mockViewport);
    if (sym == "glClearColor") return toProc(&mockClearColor);
    if (sym == "glClearDepth") return toProc(&mockClearDepth);
    if (sym == "glClear") return toProc(&mockClear);
    if (sym == "glEnable") return toProc(&mockEnable);
    if (sym == "glDisable") return toProc(&mockDisable);
    if (sym == "glBlendFunc") return toProc(&mockBlendFunc);
    if (sym == "glDepthMask") return toProc(&mockDepthMask);
    if (sym == "glDepthFunc") return toProc(&mockDepthFunc);
    if (sym == "glCullFace") return toProc(&mockCullFace);
    if (sym == "glFrontFace") return toProc(&mockFrontFace);
    if (sym == "glDrawElements") return toProc(&mockDrawElements);
    if (sym == "glGenVertexArrays") return toProc(&mockGenVertexArrays);
    if (sym == "glBindVertexArray") return toProc(&mockBindVertexArray);
    if (sym == "glDeleteVertexArrays") return toProc(&mockDeleteVertexArrays);
    if (sym == "glGenBuffers") return toProc(&mockGenBuffers);
    if (sym == "glBindBuffer") return toProc(&mockBindBuffer);
    if (sym == "glBufferData") return toProc(&mockBufferData);
    if (sym == "glBufferSubData") return toProc(&mockBufferSubData);
    if (sym == "glDeleteBuffers") return toProc(&mockDeleteBuffers);
    if (sym == "glVertexAttribPointer") return toProc(&mockVertexAttribPointer);
    if (sym == "glEnableVertexAttribArray") return toProc(&mockEnableVertexAttribArray);
    if (sym == "glCreateShader") return toProc(&mockCreateShader);
    if (sym == "glShaderSource") return toProc(&mockShaderSource);
    if (sym == "glCompileShader") return toProc(&mockCompileShader);
    if (sym == "glGetShaderiv") return toProc(&mockGetShaderiv);
    if (sym == "glGetShaderInfoLog") return toProc(&mockGetShaderInfoLog);
    if (sym == "glDeleteShader") return toProc(&mockDeleteShader);
    if (sym == "glCreateProgram") return toProc(&mockCreateProgram);
    if (sym == "glAttachShader") return toProc(&mockAttachShader);
    if (sym == "glLinkProgram") return toProc(&mockLinkProgram);
    if (sym == "glGetProgramiv") return toProc(&mockGetProgramiv);
    if (sym == "glGetProgramInfoLog") return toProc(&mockGetProgramInfoLog);
    if (sym == "glUseProgram") return toProc(&mockUseProgram);
    if (sym == "glDeleteProgram") return toProc(&mockDeleteProgram);
    if (sym == "glGetUniformLocation") return toProc(&mockGetUniformLocation);
    if (sym == "glUniformMatrix4fv") return toProc(&mockUniformMatrix4fv);
    if (sym == "glUniform3fv") return toProc(&mockUniform3fv);
    return nullptr;
}

FE_TEST(OpenGL33Device, FailureHandlingAndZeroLeakLifecycle) {
    // 1. Null loader is rejected cleanly.
    rendering::OpenGL33RenderDevice null_loader_dev(nullptr);
    FE_EXPECT_EQ(null_loader_dev.initialize(800, 600).error().code, core::ErrorCode::RenderInitFailed);

    // 2. Missing symbol is reported cleanly with symbol context.
    g_gl = MockGlState{};
    g_gl.missing_symbol = "glGenVertexArrays";
    rendering::OpenGL33RenderDevice missing_sym_dev(&mockGlLoader);
    const auto miss_st = missing_sym_dev.initialize(800, 600);
    FE_REQUIRE_TRUE(miss_st.isError());
    FE_EXPECT_EQ(miss_st.error().code, core::ErrorCode::RenderInitFailed);
    FE_EXPECT_EQ(miss_st.error().context, std::string("glGenVertexArrays"));

    // 3. Shader compile failure deletes transient shader and reports ShaderCompileFailed.
    g_gl = MockGlState{};
    g_gl.fail_shader_compile = true;
    rendering::OpenGL33RenderDevice bad_shader_dev(&mockGlLoader);
    FE_EXPECT_EQ(bad_shader_dev.initialize(800, 600).error().code, core::ErrorCode::ShaderCompileFailed);
    FE_EXPECT_EQ(g_gl.live_shaders, 0);

    // 4. Full lifecycle: upload, draw, resize, and clean GPU resource destruction.
    g_gl = MockGlState{};
    rendering::Renderer renderer(std::make_unique<rendering::OpenGL33RenderDevice>(&mockGlLoader));
    FE_REQUIRE_TRUE(renderer.initialize(1280, 720).hasValue());
    FE_EXPECT_EQ(g_gl.live_shaders, 0); // Attached shaders deleted immediately after program link.
    FE_EXPECT_EQ(g_gl.live_programs, 1);

    scene::Scene scene;
    const auto mesh_id = scene.registerEditableMesh("Cube", scene::EditableMesh::createCube(1.0f)).value();
    const auto obj_id = scene.createObject("CubeObj").value();
    FE_REQUIRE_TRUE(scene.assignMesh(obj_id, mesh_id).hasValue());

    FE_REQUIRE_TRUE(renderer.renderScene(scene).hasValue());
    FE_EXPECT_EQ(g_gl.live_vaos, 1);
    FE_EXPECT_EQ(g_gl.live_buffers, 2); // 1 VBO + 1 EBO
    FE_EXPECT_EQ(g_gl.draw_elements_calls, 1);

    // Same-capacity mesh mutation updates VBO/EBO in place via glBufferSubData without allocating new VAOs/VBOs.
    auto moved_cube = scene::EditableMesh::createCube(1.0f);
    const std::uint32_t v0 = 0;
    FE_REQUIRE_TRUE(moved_cube.translateVertices({&v0, 1}, {0.0f, 0.25f, 0.0f}).hasValue());
    FE_REQUIRE_TRUE(scene.updateMeshEditable(mesh_id, std::move(moved_cube)).hasValue());
    FE_REQUIRE_TRUE(renderer.renderScene(scene).hasValue());
    FE_EXPECT_EQ(g_gl.live_vaos, 1);
    FE_EXPECT_EQ(g_gl.live_buffers, 2);
    FE_EXPECT_EQ(g_gl.buffer_sub_data_calls, 2);

    FE_REQUIRE_TRUE(renderer.resize(1920, 1080).hasValue());
    FE_EXPECT_EQ(g_gl.last_viewport_w, 1920);
    FE_EXPECT_EQ(g_gl.last_viewport_h, 1080);

    renderer.shutdown();
    FE_EXPECT_EQ(g_gl.live_vaos, 0);
    FE_EXPECT_EQ(g_gl.live_buffers, 0);
    FE_EXPECT_EQ(g_gl.live_programs, 0);
}

} // namespace

int main(int argc, char** argv) {
    return finaleffect::testing::runAllTests(argc, argv);
}
