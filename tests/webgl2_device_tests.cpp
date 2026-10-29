// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors
// Verifies WebGL2RenderDevice GLSL ES 3.00 shader validation ('#version 300 es' on line 1,
// 'precision highp float;'), glClearDepthf(float) usage, VAO/VBO/EBO lifecycle, and leak-free shutdown.

#include "test_harness.hpp"

#include "finaleffect/rendering/renderer.hpp"
#include "finaleffect/rendering/webgl2_render_device.hpp"
#include "finaleffect/scene/primitives.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#if defined(_WIN32)
#define FE_GLES_APIENTRY __stdcall
#else
#define FE_GLES_APIENTRY
#endif

namespace {

using namespace finaleffect;

struct MockGles3State {
    bool fail_shader_compile{false};
    std::string_view missing_symbol{};
    std::uint32_t next_id{1};
    int live_vaos{0};
    int live_buffers{0};
    int live_shaders{0};
    int live_programs{0};
    int draw_elements_calls{0};
    int clear_depthf_calls{0};
    float last_clear_depth{0.0f};
    std::vector<std::string> uploaded_shader_sources;
};

MockGles3State g_gles{};

const std::uint8_t* FE_GLES_APIENTRY mockGetString(std::uint32_t) {
    return reinterpret_cast<const std::uint8_t*>("Mock WebGL 2.0 (OpenGL ES 3.0 Chromium)");
}
void FE_GLES_APIENTRY mockGetIntegerv(std::uint32_t, std::int32_t* out) {
    out[0] = 4096;
    out[1] = 4096;
}
std::uint32_t FE_GLES_APIENTRY mockGetError() { return 0; }
void FE_GLES_APIENTRY mockViewport(std::int32_t, std::int32_t, std::int32_t, std::int32_t) {}
void FE_GLES_APIENTRY mockClearColor(float, float, float, float) {}
void FE_GLES_APIENTRY mockClearDepthf(float d) {
    ++g_gles.clear_depthf_calls;
    g_gles.last_clear_depth = d;
}
void FE_GLES_APIENTRY mockClear(std::uint32_t) {}
void FE_GLES_APIENTRY mockEnable(std::uint32_t) {}
void FE_GLES_APIENTRY mockDepthFunc(std::uint32_t) {}
void FE_GLES_APIENTRY mockCullFace(std::uint32_t) {}
void FE_GLES_APIENTRY mockFrontFace(std::uint32_t) {}
void FE_GLES_APIENTRY mockDrawElements(std::uint32_t, std::int32_t, std::uint32_t, const void*) {
    ++g_gles.draw_elements_calls;
}
void FE_GLES_APIENTRY mockGenVertexArrays(std::int32_t n, std::uint32_t* ids) {
    for (std::int32_t i = 0; i < n; ++i) {
        ids[i] = g_gles.next_id++;
        ++g_gles.live_vaos;
    }
}
void FE_GLES_APIENTRY mockBindVertexArray(std::uint32_t) {}
void FE_GLES_APIENTRY mockDeleteVertexArrays(std::int32_t n, const std::uint32_t* ids) {
    for (std::int32_t i = 0; i < n; ++i) {
        if (ids[i] != 0) --g_gles.live_vaos;
    }
}
void FE_GLES_APIENTRY mockGenBuffers(std::int32_t n, std::uint32_t* ids) {
    for (std::int32_t i = 0; i < n; ++i) {
        ids[i] = g_gles.next_id++;
        ++g_gles.live_buffers;
    }
}
void FE_GLES_APIENTRY mockBindBuffer(std::uint32_t, std::uint32_t) {}
void FE_GLES_APIENTRY mockBufferData(std::uint32_t, std::ptrdiff_t, const void*, std::uint32_t) {}
void FE_GLES_APIENTRY mockDeleteBuffers(std::int32_t n, const std::uint32_t* ids) {
    for (std::int32_t i = 0; i < n; ++i) {
        if (ids[i] != 0) --g_gles.live_buffers;
    }
}
void FE_GLES_APIENTRY mockVertexAttribPointer(
    std::uint32_t, std::int32_t, std::uint32_t, std::uint8_t, std::int32_t, const void*) {}
void FE_GLES_APIENTRY mockEnableVertexAttribArray(std::uint32_t) {}

std::uint32_t FE_GLES_APIENTRY mockCreateShader(std::uint32_t) {
    ++g_gles.live_shaders;
    return g_gles.next_id++;
}
void FE_GLES_APIENTRY mockShaderSource(
    std::uint32_t,
    std::int32_t count,
    const char* const* strings,
    const std::int32_t*) {
    if (count > 0 && strings != nullptr && strings[0] != nullptr) {
        g_gles.uploaded_shader_sources.emplace_back(strings[0]);
    }
}
void FE_GLES_APIENTRY mockCompileShader(std::uint32_t) {}
void FE_GLES_APIENTRY mockGetShaderiv(std::uint32_t, std::uint32_t pname, std::int32_t* out) {
    if (pname == 0x8B81u) {
        *out = g_gles.fail_shader_compile ? 0 : 1;
    } else {
        *out = 28;
    }
}
void FE_GLES_APIENTRY mockGetShaderInfoLog(std::uint32_t, std::int32_t max_len, std::int32_t*, char* log) {
    const char* msg = "Simulated GLSL ES 3.00 error";
    std::strncpy(log, msg, static_cast<std::size_t>(max_len));
    if (max_len > 0) log[max_len - 1] = '\0';
}
void FE_GLES_APIENTRY mockDeleteShader(std::uint32_t id) {
    if (id != 0) --g_gles.live_shaders;
}

std::uint32_t FE_GLES_APIENTRY mockCreateProgram() {
    ++g_gles.live_programs;
    return g_gles.next_id++;
}
void FE_GLES_APIENTRY mockAttachShader(std::uint32_t, std::uint32_t) {}
void FE_GLES_APIENTRY mockLinkProgram(std::uint32_t) {}
void FE_GLES_APIENTRY mockGetProgramiv(std::uint32_t, std::uint32_t pname, std::int32_t* out) {
    *out = (pname == 0x8B82u) ? 1 : 0;
}
void FE_GLES_APIENTRY mockGetProgramInfoLog(std::uint32_t, std::int32_t, std::int32_t*, char*) {}
void FE_GLES_APIENTRY mockUseProgram(std::uint32_t) {}
void FE_GLES_APIENTRY mockDeleteProgram(std::uint32_t id) {
    if (id != 0) --g_gles.live_programs;
}
std::int32_t FE_GLES_APIENTRY mockGetUniformLocation(std::uint32_t, const char*) { return 1; }
void FE_GLES_APIENTRY mockUniformMatrix4fv(std::int32_t, std::int32_t, std::uint8_t, const float*) {}
void FE_GLES_APIENTRY mockUniform3fv(std::int32_t, std::int32_t, const float*) {}

template <typename Fn>
platform::GlProcAddressFn toProc(Fn fn) noexcept {
    platform::GlProcAddressFn out = nullptr;
    static_assert(sizeof(out) == sizeof(fn));
    std::memcpy(&out, &fn, sizeof(out));
    return out;
}

platform::GlProcAddressFn mockGles3Loader(const char* name) {
    const std::string_view sym(name);
    if (!g_gles.missing_symbol.empty() && sym == g_gles.missing_symbol) {
        return nullptr;
    }
    if (sym == "glGetString") return toProc(&mockGetString);
    if (sym == "glGetIntegerv") return toProc(&mockGetIntegerv);
    if (sym == "glGetError") return toProc(&mockGetError);
    if (sym == "glViewport") return toProc(&mockViewport);
    if (sym == "glClearColor") return toProc(&mockClearColor);
    if (sym == "glClearDepthf") return toProc(&mockClearDepthf);
    if (sym == "glClear") return toProc(&mockClear);
    if (sym == "glEnable") return toProc(&mockEnable);
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

FE_TEST(WebGL2Device, GlslEs300ComplianceClearDepthfAndZeroLeakLifecycle) {
    // 1. Missing glClearDepthf (GLES3-specific symbol) is detected cleanly.
    g_gles = MockGles3State{};
    g_gles.missing_symbol = "glClearDepthf";
    rendering::WebGL2RenderDevice missing_depthf(&mockGles3Loader);
    const auto miss_st = missing_depthf.initialize(960, 540);
    FE_REQUIRE_TRUE(miss_st.isError());
    FE_EXPECT_EQ(miss_st.error().code, core::ErrorCode::RenderInitFailed);
    FE_EXPECT_EQ(miss_st.error().context, std::string("glClearDepthf"));

    // 2. Full render cycle verifies '#version 300 es' on line 1, 'precision highp float;', and glClearDepthf.
    g_gles = MockGles3State{};
    rendering::Renderer renderer(std::make_unique<rendering::WebGL2RenderDevice>(&mockGles3Loader));
    FE_REQUIRE_TRUE(renderer.initialize(960, 540).hasValue());
    FE_EXPECT_EQ(renderer.device().capabilities().backend, rendering::BackendType::WebGL2);

    FE_REQUIRE_TRUE(g_gles.uploaded_shader_sources.size() == std::size_t{2});
    for (const auto& src : g_gles.uploaded_shader_sources) {
        FE_EXPECT_TRUE(src.rfind("#version 300 es\n", 0) == 0);
        FE_EXPECT_TRUE(src.find("precision highp float;") != std::string::npos);
    }

    scene::Scene scene;
    const auto pyr_mesh = scene.registerMesh("Pyramid", scene::Primitives::createPyramid()).value();
    const auto obj = scene.createObject("WebPyramid").value();
    FE_REQUIRE_TRUE(scene.assignMesh(obj, pyr_mesh).hasValue());

    FE_REQUIRE_TRUE(renderer.renderScene(scene).hasValue());
    FE_EXPECT_EQ(g_gles.clear_depthf_calls, 1);
    FE_EXPECT_NEAR(g_gles.last_clear_depth, 1.0f, 1e-5f);
    FE_EXPECT_EQ(g_gles.draw_elements_calls, 1);

    renderer.shutdown();
    FE_EXPECT_EQ(g_gles.live_vaos, 0);
    FE_EXPECT_EQ(g_gles.live_buffers, 0);
    FE_EXPECT_EQ(g_gles.live_programs, 0);
    FE_EXPECT_EQ(g_gles.live_shaders, 0);
}

} // namespace

int main(int argc, char** argv) {
    return finaleffect::testing::runAllTests(argc, argv);
}
