// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/rendering/opengl33_render_device.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

#if defined(_WIN32)
#define FE_GL_APIENTRY __stdcall
#else
#define FE_GL_APIENTRY
#endif

namespace finaleffect::rendering {

namespace {

// Khronos OpenGL 3.3 Core fundamental types and constants.
using GLenum = std::uint32_t;
using GLboolean = std::uint8_t;
using GLbitfield = std::uint32_t;
using GLint = std::int32_t;
using GLuint = std::uint32_t;
using GLsizei = std::int32_t;
using GLfloat = float;
using GLchar = char;
using GLubyte = std::uint8_t;
using GLsizeiptr = std::ptrdiff_t;
using GLintptr = std::ptrdiff_t;

constexpr GLboolean kGlFalse = 0;
constexpr GLboolean kGlTrue = 1;
constexpr GLenum kGlDepthBufferBit = 0x00000100u;
constexpr GLenum kGlColorBufferBit = 0x00004000u;
constexpr GLenum kGlTriangles = 0x0004u;
constexpr GLenum kGlLess = 0x0201u;
constexpr GLenum kGlSrcAlpha = 0x0302u;
constexpr GLenum kGlOneMinusSrcAlpha = 0x0303u;
constexpr GLenum kGlBack = 0x0405u;
constexpr GLenum kGlCcw = 0x0901u;
constexpr GLenum kGlCullFace = 0x0B44u;
constexpr GLenum kGlDepthTest = 0x0B71u;
constexpr GLenum kGlBlend = 0x0BE2u;
constexpr GLenum kGlMaxViewportDims = 0x0D3Au;
constexpr GLenum kGlUnsignedInt = 0x1405u;
constexpr GLenum kGlFloat = 0x1406u;
constexpr GLenum kGlRenderer = 0x1F01u;
constexpr GLenum kGlArrayBuffer = 0x8892u;
constexpr GLenum kGlElementArrayBuffer = 0x8893u;
constexpr GLenum kGlStaticDraw = 0x88E4u;
constexpr GLenum kGlDynamicDraw = 0x88E8u;
constexpr GLenum kGlFragmentShader = 0x8B30u;
constexpr GLenum kGlVertexShader = 0x8B31u;
constexpr GLenum kGlCompileStatus = 0x8B81u;
constexpr GLenum kGlLinkStatus = 0x8B82u;
constexpr GLenum kGlInfoLogLength = 0x8B84u;

constexpr const char* kVertexShaderGlsl330 = R"(#version 330 core
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec3 a_color;
layout(location = 3) in vec2 a_uv;

uniform mat4 u_model;
uniform mat4 u_normal_matrix;
uniform mat4 u_view_proj;

out vec3 v_world_pos;
out vec3 v_world_normal;
out vec3 v_color;
out vec2 v_uv;

void main() {
    vec4 world_pos = u_model * vec4(a_position, 1.0);
    v_world_pos = world_pos.xyz;
    v_world_normal = normalize((u_normal_matrix * vec4(a_normal, 0.0)).xyz);
    v_color = a_color;
    v_uv = a_uv;
    gl_Position = u_view_proj * world_pos;
}
)";

// Energy-conserving metallic-roughness Cook-Torrance GGX + Schlick Fresnel PBR fragment shader.
constexpr const char* kFragmentShaderGlsl330 = R"(#version 330 core
in vec3 v_world_pos;
in vec3 v_world_normal;
in vec3 v_color;
in vec2 v_uv;

uniform vec3 u_camera_pos;
uniform vec3 u_light_dir;
uniform vec3 u_light_radiance;
uniform vec3 u_ambient;
uniform vec3 u_selection_tint;

uniform vec3 u_base_color;
uniform vec3 u_pbr_params;        // x = metallic, y = roughness, z = ambient_occlusion
uniform vec3 u_emissive_radiance;
uniform vec3 u_surface_flags;     // x = opacity, y = vertex_color_weight, z = unlit_overlay

out vec4 o_frag_color;

const float kPi = 3.14159265359;

vec3 fresnelSchlick(float cos_theta, vec3 f0) {
    float x = clamp(1.0 - cos_theta, 0.0, 1.0);
    float x2 = x * x;
    return f0 + (1.0 - f0) * (x2 * x2 * x);
}

float distributionGgx(float ndoth, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float denom = ndoth * ndoth * (a2 - 1.0) + 1.0;
    return a2 / max(kPi * denom * denom, 1.0e-6);
}

float geometrySchlickGgx(float ndotx, float k) {
    return ndotx / max(ndotx * (1.0 - k) + k, 1.0e-5);
}

vec3 evalCookTorranceLobe(vec3 n, vec3 v, vec3 l, vec3 radiance, vec3 albedo, vec3 f0, float metallic, float roughness, float k_direct) {
    float ndotl = max(dot(n, l), 0.0);
    if (ndotl <= 0.0) {
        return vec3(0.0);
    }
    float ndotv = max(dot(n, v), 1.0e-4);
    vec3 h = normalize(v + l);
    float ndoth = max(dot(n, h), 0.0);
    float hdotv = max(dot(h, v), 0.0);

    float ndf = distributionGgx(ndoth, roughness);
    float geom = geometrySchlickGgx(ndotv, k_direct) * geometrySchlickGgx(ndotl, k_direct);
    vec3 fresnel = fresnelSchlick(hdotv, f0);

    vec3 kd = (vec3(1.0) - fresnel) * (1.0 - metallic);
    vec3 diffuse = kd * albedo * (1.0 / kPi);
    vec3 specular = (ndf * geom * fresnel) / max(4.0 * ndotv * ndotl, 1.0e-4);
    return (diffuse + specular) * radiance * ndotl;
}

void main() {
    float opacity = clamp(u_surface_flags.x, 0.0, 1.0);
    vec3 weighted_vcol = mix(vec3(1.0), v_color, clamp(u_surface_flags.y, 0.0, 1.0));
    vec3 albedo = u_base_color * weighted_vcol;

    if (u_surface_flags.z > 0.5) {
        o_frag_color = vec4(clamp(albedo + u_selection_tint, 0.0, 1.0), opacity);
        return;
    }

    vec3 n = normalize(v_world_normal);
    vec3 v = normalize(u_camera_pos - v_world_pos);
    float ndotv = max(dot(n, v), 1.0e-4);

    float metallic = clamp(u_pbr_params.x, 0.0, 1.0);
    float roughness = clamp(u_pbr_params.y, 0.045, 1.0);
    float ao = clamp(u_pbr_params.z, 0.0, 1.0);
    float k_direct = ((roughness + 1.0) * (roughness + 1.0)) * 0.125;

    vec3 f0 = mix(vec3(0.04), albedo, metallic);

    vec3 key_l = normalize(u_light_dir);
    vec3 fill_l = normalize(vec3(-0.55, 0.35, -0.65));

    vec3 lo = evalCookTorranceLobe(n, v, key_l, u_light_radiance, albedo, f0, metallic, roughness, k_direct)
            + evalCookTorranceLobe(n, v, fill_l, u_light_radiance * 0.22, albedo, f0, metallic, roughness, k_direct);

    float sky = n.y * 0.5 + 0.5;
    vec3 env_irradiance = u_ambient * (0.65 + 0.70 * sky);
    vec3 f_env = fresnelSchlick(ndotv, f0);
    vec3 kd_env = (vec3(1.0) - f_env) * (1.0 - metallic);
    vec3 env_spec = f_env * env_irradiance * (1.0 - 0.75 * roughness);
    vec3 ambient_term = (kd_env * albedo * env_irradiance + env_spec) * ao;

    vec3 hdr = ambient_term + lo + u_emissive_radiance;
    float rim = 1.0 - ndotv;
    hdr += u_selection_tint * (0.30 + 0.80 * rim * rim);

    vec3 mapped = hdr / (vec3(1.0) + max(hdr, vec3(0.0)));
    vec3 srgb = pow(clamp(mapped, 0.0, 1.0), vec3(1.0 / 2.2));
    o_frag_color = vec4(srgb, opacity);
}
)";

} // namespace

struct OpenGL33RenderDevice::GlDispatchTable {
    const GLubyte* (FE_GL_APIENTRY* GetString)(GLenum) = nullptr;
    void (FE_GL_APIENTRY* GetIntegerv)(GLenum, GLint*) = nullptr;
    GLenum (FE_GL_APIENTRY* GetError)() = nullptr;
    void (FE_GL_APIENTRY* Viewport)(GLint, GLint, GLsizei, GLsizei) = nullptr;
    void (FE_GL_APIENTRY* ClearColor)(GLfloat, GLfloat, GLfloat, GLfloat) = nullptr;
    void (FE_GL_APIENTRY* ClearDepth)(double) = nullptr;
    void (FE_GL_APIENTRY* Clear)(GLbitfield) = nullptr;
    void (FE_GL_APIENTRY* Enable)(GLenum) = nullptr;
    void (FE_GL_APIENTRY* Disable)(GLenum) = nullptr;
    void (FE_GL_APIENTRY* BlendFunc)(GLenum, GLenum) = nullptr;
    void (FE_GL_APIENTRY* DepthMask)(GLboolean) = nullptr;
    void (FE_GL_APIENTRY* DepthFunc)(GLenum) = nullptr;
    void (FE_GL_APIENTRY* CullFace)(GLenum) = nullptr;
    void (FE_GL_APIENTRY* FrontFace)(GLenum) = nullptr;
    void (FE_GL_APIENTRY* DrawElements)(GLenum, GLsizei, GLenum, const void*) = nullptr;

    void (FE_GL_APIENTRY* GenVertexArrays)(GLsizei, GLuint*) = nullptr;
    void (FE_GL_APIENTRY* BindVertexArray)(GLuint) = nullptr;
    void (FE_GL_APIENTRY* DeleteVertexArrays)(GLsizei, const GLuint*) = nullptr;

    void (FE_GL_APIENTRY* GenBuffers)(GLsizei, GLuint*) = nullptr;
    void (FE_GL_APIENTRY* BindBuffer)(GLenum, GLuint) = nullptr;
    void (FE_GL_APIENTRY* BufferData)(GLenum, GLsizeiptr, const void*, GLenum) = nullptr;
    void (FE_GL_APIENTRY* BufferSubData)(GLenum, GLintptr, GLsizeiptr, const void*) = nullptr;
    void (FE_GL_APIENTRY* DeleteBuffers)(GLsizei, const GLuint*) = nullptr;

    void (FE_GL_APIENTRY* VertexAttribPointer)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*) = nullptr;
    void (FE_GL_APIENTRY* EnableVertexAttribArray)(GLuint) = nullptr;

    GLuint (FE_GL_APIENTRY* CreateShader)(GLenum) = nullptr;
    void (FE_GL_APIENTRY* ShaderSource)(GLuint, GLsizei, const GLchar* const*, const GLint*) = nullptr;
    void (FE_GL_APIENTRY* CompileShader)(GLuint) = nullptr;
    void (FE_GL_APIENTRY* GetShaderiv)(GLuint, GLenum, GLint*) = nullptr;
    void (FE_GL_APIENTRY* GetShaderInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*) = nullptr;
    void (FE_GL_APIENTRY* DeleteShader)(GLuint) = nullptr;

    GLuint (FE_GL_APIENTRY* CreateProgram)() = nullptr;
    void (FE_GL_APIENTRY* AttachShader)(GLuint, GLuint) = nullptr;
    void (FE_GL_APIENTRY* LinkProgram)(GLuint) = nullptr;
    void (FE_GL_APIENTRY* GetProgramiv)(GLuint, GLenum, GLint*) = nullptr;
    void (FE_GL_APIENTRY* GetProgramInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*) = nullptr;
    void (FE_GL_APIENTRY* UseProgram)(GLuint) = nullptr;
    void (FE_GL_APIENTRY* DeleteProgram)(GLuint) = nullptr;

    GLint (FE_GL_APIENTRY* GetUniformLocation)(GLuint, const GLchar*) = nullptr;
    void (FE_GL_APIENTRY* UniformMatrix4fv)(GLint, GLsizei, GLboolean, const GLfloat*) = nullptr;
    void (FE_GL_APIENTRY* Uniform3fv)(GLint, GLsizei, const GLfloat*) = nullptr;
};

OpenGL33RenderDevice::OpenGL33RenderDevice(platform::GlProcLoaderFn proc_loader)
    : proc_loader_(proc_loader),
      gl_(std::make_unique<GlDispatchTable>()) {
    capabilities_.backend = BackendType::OpenGL33;
    capabilities_.backend_name = "OpenGL 3.3 Core";
    capabilities_.device_name = "Uninitialized OpenGL Device";
    capabilities_.clip_space_depth = math::ClipSpaceDepth::NegativeOneToOne;
    capabilities_.max_viewport_dimension = 8192;
}

OpenGL33RenderDevice::~OpenGL33RenderDevice() {
    shutdown();
}

core::Status OpenGL33RenderDevice::initialize(std::uint32_t width, std::uint32_t height) {
    if (initialized_) {
        return core::Status::fail(core::ErrorCode::InvalidState, "OpenGL33RenderDevice is already initialized");
    }
    if (proc_loader_ == nullptr) {
        return core::Status::fail(
            core::ErrorCode::RenderInitFailed,
            "OpenGL33RenderDevice requires a valid GlProcLoaderFn");
    }
    if (width == 0 || height == 0) {
        return core::Status::fail(
            core::ErrorCode::InvalidArgument,
            "Initial OpenGL framebuffer dimensions must be non-zero");
    }

    std::string missing_symbol;
    auto loadSym = [&](auto& target_fn, const char* name) -> bool {
        const auto raw = proc_loader_(name);
        if (raw == nullptr) {
            missing_symbol = name;
            return false;
        }
        static_assert(sizeof(target_fn) == sizeof(raw));
        std::memcpy(&target_fn, &raw, sizeof(target_fn));
        return true;
    };
    auto loadOptSym = [&](auto& target_fn, const char* name) {
        const auto raw = proc_loader_(name);
        if (raw != nullptr) {
            static_assert(sizeof(target_fn) == sizeof(raw));
            std::memcpy(&target_fn, &raw, sizeof(target_fn));
        }
    };

    const bool all_loaded =
        loadSym(gl_->GetString, "glGetString") &&
        loadSym(gl_->GetIntegerv, "glGetIntegerv") &&
        loadSym(gl_->GetError, "glGetError") &&
        loadSym(gl_->Viewport, "glViewport") &&
        loadSym(gl_->ClearColor, "glClearColor") &&
        loadSym(gl_->ClearDepth, "glClearDepth") &&
        loadSym(gl_->Clear, "glClear") &&
        loadSym(gl_->Enable, "glEnable") &&
        loadSym(gl_->DepthFunc, "glDepthFunc") &&
        loadSym(gl_->CullFace, "glCullFace") &&
        loadSym(gl_->FrontFace, "glFrontFace") &&
        loadSym(gl_->DrawElements, "glDrawElements") &&
        loadSym(gl_->GenVertexArrays, "glGenVertexArrays") &&
        loadSym(gl_->BindVertexArray, "glBindVertexArray") &&
        loadSym(gl_->DeleteVertexArrays, "glDeleteVertexArrays") &&
        loadSym(gl_->GenBuffers, "glGenBuffers") &&
        loadSym(gl_->BindBuffer, "glBindBuffer") &&
        loadSym(gl_->BufferData, "glBufferData") &&
        loadSym(gl_->DeleteBuffers, "glDeleteBuffers") &&
        loadSym(gl_->VertexAttribPointer, "glVertexAttribPointer") &&
        loadSym(gl_->EnableVertexAttribArray, "glEnableVertexAttribArray") &&
        loadSym(gl_->CreateShader, "glCreateShader") &&
        loadSym(gl_->ShaderSource, "glShaderSource") &&
        loadSym(gl_->CompileShader, "glCompileShader") &&
        loadSym(gl_->GetShaderiv, "glGetShaderiv") &&
        loadSym(gl_->GetShaderInfoLog, "glGetShaderInfoLog") &&
        loadSym(gl_->DeleteShader, "glDeleteShader") &&
        loadSym(gl_->CreateProgram, "glCreateProgram") &&
        loadSym(gl_->AttachShader, "glAttachShader") &&
        loadSym(gl_->LinkProgram, "glLinkProgram") &&
        loadSym(gl_->GetProgramiv, "glGetProgramiv") &&
        loadSym(gl_->GetProgramInfoLog, "glGetProgramInfoLog") &&
        loadSym(gl_->UseProgram, "glUseProgram") &&
        loadSym(gl_->DeleteProgram, "glDeleteProgram") &&
        loadSym(gl_->GetUniformLocation, "glGetUniformLocation") &&
        loadSym(gl_->UniformMatrix4fv, "glUniformMatrix4fv") &&
        loadSym(gl_->Uniform3fv, "glUniform3fv");

    if (!all_loaded) {
        return core::Status::fail(
            core::ErrorCode::RenderInitFailed,
            "Failed to resolve required OpenGL 3.3 Core entry point",
            missing_symbol);
    }

    loadOptSym(gl_->BufferSubData, "glBufferSubData");
    loadOptSym(gl_->Disable, "glDisable");
    loadOptSym(gl_->BlendFunc, "glBlendFunc");
    loadOptSym(gl_->DepthMask, "glDepthMask");

    if (const GLubyte* renderer_str = gl_->GetString(kGlRenderer); renderer_str != nullptr) {
        capabilities_.device_name = reinterpret_cast<const char*>(renderer_str);
    } else {
        capabilities_.device_name = "OpenGL 3.3 Core Device";
    }

    GLint viewport_dims[2] = {8192, 8192};
    gl_->GetIntegerv(kGlMaxViewportDims, viewport_dims);
    if (viewport_dims[0] > 0 && viewport_dims[1] > 0) {
        capabilities_.max_viewport_dimension =
            static_cast<std::uint32_t>(viewport_dims[0] < viewport_dims[1] ? viewport_dims[0] : viewport_dims[1]);
    }

    auto shader_status = buildShaderPipeline();
    if (!shader_status) {
        return shader_status;
    }

    gl_->Enable(kGlDepthTest);
    gl_->DepthFunc(kGlLess);
    gl_->Enable(kGlCullFace);
    gl_->FrontFace(kGlCcw);
    gl_->CullFace(kGlBack);

    width_ = width;
    height_ = height;
    gl_->Viewport(0, 0, static_cast<GLsizei>(width_), static_cast<GLsizei>(height_));
    initialized_ = true;
    in_frame_ = false;
    return core::Status::ok();
}

core::Status OpenGL33RenderDevice::buildShaderPipeline() {
    auto compileStage = [&](GLenum stage, const char* source, const char* stage_label) -> core::Result<GLuint> {
        const GLuint shader = gl_->CreateShader(stage);
        if (shader == 0) {
            return core::Result<GLuint>::fail(
                core::ErrorCode::ResourceCreationFailed,
                "glCreateShader returned 0",
                stage_label);
        }
        gl_->ShaderSource(shader, 1, &source, nullptr);
        gl_->CompileShader(shader);

        GLint compiled = 0;
        gl_->GetShaderiv(shader, kGlCompileStatus, &compiled);
        if (compiled == 0) {
            GLint log_len = 0;
            gl_->GetShaderiv(shader, kGlInfoLogLength, &log_len);
            std::string log(static_cast<std::size_t>(log_len > 1 ? log_len : 1), '\0');
            if (log_len > 1) {
                gl_->GetShaderInfoLog(shader, log_len, nullptr, log.data());
            }
            gl_->DeleteShader(shader);
            return core::Result<GLuint>::fail(
                core::ErrorCode::ShaderCompileFailed,
                "GLSL 330 shader compilation failed: " + log,
                stage_label);
        }
        return core::Result<GLuint>::ok(shader);
    };

    auto vs = compileStage(kGlVertexShader, kVertexShaderGlsl330, "vertex");
    if (!vs) {
        return vs.error();
    }
    auto fs = compileStage(kGlFragmentShader, kFragmentShaderGlsl330, "fragment");
    if (!fs) {
        gl_->DeleteShader(vs.value());
        return fs.error();
    }

    const GLuint program = gl_->CreateProgram();
    if (program == 0) {
        gl_->DeleteShader(vs.value());
        gl_->DeleteShader(fs.value());
        return core::Status::fail(core::ErrorCode::ResourceCreationFailed, "glCreateProgram returned 0");
    }

    gl_->AttachShader(program, vs.value());
    gl_->AttachShader(program, fs.value());
    gl_->LinkProgram(program);

    gl_->DeleteShader(vs.value());
    gl_->DeleteShader(fs.value());

    GLint linked = 0;
    gl_->GetProgramiv(program, kGlLinkStatus, &linked);
    if (linked == 0) {
        GLint log_len = 0;
        gl_->GetProgramiv(program, kGlInfoLogLength, &log_len);
        std::string log(static_cast<std::size_t>(log_len > 1 ? log_len : 1), '\0');
        if (log_len > 1) {
            gl_->GetProgramInfoLog(program, log_len, nullptr, log.data());
        }
        gl_->DeleteProgram(program);
        return core::Status::fail(
            core::ErrorCode::ShaderCompileFailed,
            "GLSL 330 program link failed: " + log);
    }

    shader_program_ = program;
    loc_model_ = gl_->GetUniformLocation(shader_program_, "u_model");
    loc_normal_mat_ = gl_->GetUniformLocation(shader_program_, "u_normal_matrix");
    loc_view_proj_ = gl_->GetUniformLocation(shader_program_, "u_view_proj");
    loc_camera_pos_ = gl_->GetUniformLocation(shader_program_, "u_camera_pos");
    loc_light_dir_ = gl_->GetUniformLocation(shader_program_, "u_light_dir");
    loc_light_radiance_ = gl_->GetUniformLocation(shader_program_, "u_light_radiance");
    loc_ambient_ = gl_->GetUniformLocation(shader_program_, "u_ambient");
    loc_selection_tint_ = gl_->GetUniformLocation(shader_program_, "u_selection_tint");
    loc_base_color_ = gl_->GetUniformLocation(shader_program_, "u_base_color");
    loc_pbr_params_ = gl_->GetUniformLocation(shader_program_, "u_pbr_params");
    loc_emissive_radiance_ = gl_->GetUniformLocation(shader_program_, "u_emissive_radiance");
    loc_surface_flags_ = gl_->GetUniformLocation(shader_program_, "u_surface_flags");
    return core::Status::ok();
}

void OpenGL33RenderDevice::shutdown() noexcept {
    if (!initialized_) {
        return;
    }

    for (const auto& [handle_id, rec] : meshes_) {
        (void)handle_id;
        if (rec.ebo != 0) gl_->DeleteBuffers(1, &rec.ebo);
        if (rec.vbo != 0) gl_->DeleteBuffers(1, &rec.vbo);
        if (rec.vao != 0) gl_->DeleteVertexArrays(1, &rec.vao);
    }
    meshes_.clear();
    total_gpu_bytes_ = 0;
    buffer_updates_ = 0;

    if (shader_program_ != 0) {
        gl_->UseProgram(0);
        gl_->DeleteProgram(shader_program_);
        shader_program_ = 0;
    }

    in_frame_ = false;
    initialized_ = false;
}

core::Status OpenGL33RenderDevice::resize(std::uint32_t width, std::uint32_t height) {
    if (!initialized_) {
        return core::Status::fail(core::ErrorCode::InvalidState, "OpenGL33RenderDevice not initialized");
    }
    if (width > capabilities_.max_viewport_dimension || height > capabilities_.max_viewport_dimension) {
        return core::Status::fail(core::ErrorCode::InvalidArgument, "Requested viewport exceeds OpenGL device limit");
    }
    width_ = width;
    height_ = height;
    if (width_ > 0 && height_ > 0) {
        gl_->Viewport(0, 0, static_cast<GLsizei>(width_), static_cast<GLsizei>(height_));
    }
    return core::Status::ok();
}

core::Result<GpuMeshHandle> OpenGL33RenderDevice::createMesh(const scene::MeshData& mesh) {
    if (!initialized_) {
        return core::Result<GpuMeshHandle>::fail(core::ErrorCode::InvalidState, "Device not initialized");
    }
    auto valid = mesh.validate();
    if (!valid) {
        return valid.error();
    }
    if (mesh.indices.size() > static_cast<std::size_t>(std::numeric_limits<GLsizei>::max())) {
        return core::Result<GpuMeshHandle>::fail(
            core::ErrorCode::InvalidArgument,
            "Mesh index count exceeds OpenGL GLsizei limit");
    }

    GpuMeshRecord rec{};
    gl_->GenVertexArrays(1, &rec.vao);
    gl_->GenBuffers(1, &rec.vbo);
    gl_->GenBuffers(1, &rec.ebo);

    if (rec.vao == 0 || rec.vbo == 0 || rec.ebo == 0) {
        if (rec.ebo != 0) gl_->DeleteBuffers(1, &rec.ebo);
        if (rec.vbo != 0) gl_->DeleteBuffers(1, &rec.vbo);
        if (rec.vao != 0) gl_->DeleteVertexArrays(1, &rec.vao);
        return core::Result<GpuMeshHandle>::fail(
            core::ErrorCode::ResourceCreationFailed,
            "Failed to allocate OpenGL VAO/VBO/EBO");
    }

    const std::size_t vbo_bytes = mesh.vertices.size() * sizeof(scene::Vertex);
    const std::size_t ebo_bytes = mesh.indices.size() * sizeof(std::uint32_t);

    gl_->BindVertexArray(rec.vao);

    gl_->BindBuffer(kGlArrayBuffer, rec.vbo);
    gl_->BufferData(kGlArrayBuffer, static_cast<GLsizeiptr>(vbo_bytes), mesh.vertices.data(), kGlStaticDraw);

    gl_->BindBuffer(kGlElementArrayBuffer, rec.ebo);
    gl_->BufferData(kGlElementArrayBuffer, static_cast<GLsizeiptr>(ebo_bytes), mesh.indices.data(), kGlStaticDraw);

    constexpr auto stride = static_cast<GLsizei>(sizeof(scene::Vertex));
    const auto pos_offset = offsetof(scene::Vertex, position);
    const auto nrm_offset = offsetof(scene::Vertex, normal);
    const auto col_offset = offsetof(scene::Vertex, color);
    const auto uv_offset = offsetof(scene::Vertex, uv);

    gl_->EnableVertexAttribArray(0);
    gl_->VertexAttribPointer(0, 3, kGlFloat, kGlFalse, stride, reinterpret_cast<const void*>(pos_offset));

    gl_->EnableVertexAttribArray(1);
    gl_->VertexAttribPointer(1, 3, kGlFloat, kGlFalse, stride, reinterpret_cast<const void*>(nrm_offset));

    gl_->EnableVertexAttribArray(2);
    gl_->VertexAttribPointer(2, 3, kGlFloat, kGlFalse, stride, reinterpret_cast<const void*>(col_offset));

    gl_->EnableVertexAttribArray(3);
    gl_->VertexAttribPointer(3, 2, kGlFloat, kGlFalse, stride, reinterpret_cast<const void*>(uv_offset));

    gl_->BindVertexArray(0);
    gl_->BindBuffer(kGlArrayBuffer, 0);

    rec.index_count = static_cast<GLsizei>(mesh.indices.size());
    rec.byte_size = mesh.byteSize();
    rec.vbo_capacity_bytes = vbo_bytes;
    rec.ebo_capacity_bytes = ebo_bytes;

    const std::uint64_t handle_val = next_handle_++;
    total_gpu_bytes_ += rec.byte_size;
    meshes_.emplace(handle_val, rec);
    return core::Result<GpuMeshHandle>::ok(GpuMeshHandle{handle_val});
}

core::Status OpenGL33RenderDevice::updateMesh(GpuMeshHandle handle, const scene::MeshData& mesh) {
    if (!initialized_) {
        return core::Status::fail(core::ErrorCode::InvalidState, "Device not initialized");
    }
    const auto it = meshes_.find(handle.value);
    if (it == meshes_.end()) {
        return core::Status::fail(core::ErrorCode::NotFound, "updateMesh referenced unknown GpuMeshHandle");
    }
    auto valid = mesh.validate();
    if (!valid) {
        return valid;
    }
    if (mesh.indices.size() > static_cast<std::size_t>(std::numeric_limits<GLsizei>::max())) {
        return core::Status::fail(core::ErrorCode::InvalidArgument, "Mesh index count exceeds OpenGL GLsizei limit");
    }

    GpuMeshRecord& rec = it->second;
    const std::size_t vbo_bytes = mesh.vertices.size() * sizeof(scene::Vertex);
    const std::size_t ebo_bytes = mesh.indices.size() * sizeof(std::uint32_t);

    gl_->BindVertexArray(rec.vao);

    gl_->BindBuffer(kGlArrayBuffer, rec.vbo);
    if (gl_->BufferSubData != nullptr && vbo_bytes <= rec.vbo_capacity_bytes) {
        gl_->BufferSubData(kGlArrayBuffer, 0, static_cast<GLsizeiptr>(vbo_bytes), mesh.vertices.data());
    } else {
        gl_->BufferData(kGlArrayBuffer, static_cast<GLsizeiptr>(vbo_bytes), mesh.vertices.data(), kGlDynamicDraw);
        rec.vbo_capacity_bytes = vbo_bytes;
    }

    gl_->BindBuffer(kGlElementArrayBuffer, rec.ebo);
    if (gl_->BufferSubData != nullptr && ebo_bytes <= rec.ebo_capacity_bytes) {
        gl_->BufferSubData(kGlElementArrayBuffer, 0, static_cast<GLsizeiptr>(ebo_bytes), mesh.indices.data());
    } else {
        gl_->BufferData(kGlElementArrayBuffer, static_cast<GLsizeiptr>(ebo_bytes), mesh.indices.data(), kGlDynamicDraw);
        rec.ebo_capacity_bytes = ebo_bytes;
    }

    gl_->BindVertexArray(0);
    gl_->BindBuffer(kGlArrayBuffer, 0);

    const std::size_t new_byte_size = mesh.byteSize();
    total_gpu_bytes_ = (total_gpu_bytes_ - rec.byte_size) + new_byte_size;
    rec.byte_size = new_byte_size;
    rec.index_count = static_cast<GLsizei>(mesh.indices.size());
    ++buffer_updates_;
    return core::Status::ok();
}

void OpenGL33RenderDevice::destroyMesh(GpuMeshHandle handle) noexcept {
    if (!initialized_) {
        return;
    }
    const auto it = meshes_.find(handle.value);
    if (it == meshes_.end()) {
        return;
    }
    const GpuMeshRecord& rec = it->second;
    if (rec.ebo != 0) gl_->DeleteBuffers(1, &rec.ebo);
    if (rec.vbo != 0) gl_->DeleteBuffers(1, &rec.vbo);
    if (rec.vao != 0) gl_->DeleteVertexArrays(1, &rec.vao);
    total_gpu_bytes_ -= rec.byte_size;
    meshes_.erase(it);
}

core::Status OpenGL33RenderDevice::beginFrame(const FrameClearDesc& clear) {
    if (!initialized_ || in_frame_) {
        return core::Status::fail(core::ErrorCode::InvalidState, "Invalid OpenGL33RenderDevice::beginFrame state");
    }
    in_frame_ = true;

    if (width_ == 0 || height_ == 0) {
        return core::Status::ok();
    }

    gl_->Viewport(0, 0, static_cast<GLsizei>(width_), static_cast<GLsizei>(height_));
    gl_->ClearColor(
        clear.clear_color.x,
        clear.clear_color.y,
        clear.clear_color.z,
        clear.clear_color.w);
    gl_->ClearDepth(static_cast<double>(clear.clear_depth));
    gl_->Clear(kGlColorBufferBit | kGlDepthBufferBit);
    gl_->UseProgram(shader_program_);
    return core::Status::ok();
}

core::Status OpenGL33RenderDevice::beginOverlayPass(float clear_depth) {
    if (!initialized_ || !in_frame_) {
        return core::Status::fail(
            core::ErrorCode::InvalidState,
            "OpenGL33RenderDevice::beginOverlayPass called outside active frame");
    }
    if (width_ == 0 || height_ == 0) {
        return core::Status::ok();
    }
    gl_->ClearDepth(static_cast<double>(clear_depth));
    gl_->Clear(kGlDepthBufferBit);
    return core::Status::ok();
}

core::Status OpenGL33RenderDevice::drawMesh(const DrawMeshCommand& command) {
    if (!initialized_ || !in_frame_) {
        return core::Status::fail(core::ErrorCode::InvalidState, "drawMesh called outside active frame");
    }
    const auto it = meshes_.find(command.mesh_handle.value);
    if (it == meshes_.end()) {
        return core::Status::fail(core::ErrorCode::NotFound, "Unknown GpuMeshHandle in OpenGL33RenderDevice");
    }
    if (width_ == 0 || height_ == 0) {
        return core::Status::ok();
    }

    const GpuMeshRecord& rec = it->second;
    const auto total_idx = static_cast<std::uint32_t>(rec.index_count);
    if (command.index_offset >= total_idx) {
        return core::Status::ok();
    }
    const std::uint32_t max_rem = total_idx - command.index_offset;
    const std::uint32_t draw_count = (command.index_count == 0)
                                         ? max_rem
                                         : std::min(command.index_count, max_rem);

    const auto inv_model = command.model_matrix.inverted();
    const math::Mat4 normal_matrix =
        inv_model.has_value() ? inv_model->transposed() : command.model_matrix;

    if (loc_model_ >= 0) {
        gl_->UniformMatrix4fv(loc_model_, 1, kGlFalse, command.model_matrix.raw());
    }
    if (loc_normal_mat_ >= 0) {
        gl_->UniformMatrix4fv(loc_normal_mat_, 1, kGlFalse, normal_matrix.raw());
    }
    if (loc_view_proj_ >= 0) {
        gl_->UniformMatrix4fv(loc_view_proj_, 1, kGlFalse, command.view_projection_matrix.raw());
    }
    if (loc_camera_pos_ >= 0) {
        const float cam[3] = {command.camera_position.x, command.camera_position.y, command.camera_position.z};
        gl_->Uniform3fv(loc_camera_pos_, 1, cam);
    }
    if (loc_light_dir_ >= 0) {
        const math::Vec3 ld = command.lighting.direction_to_light.normalized();
        const float dir[3] = {ld.x, ld.y, ld.z};
        gl_->Uniform3fv(loc_light_dir_, 1, dir);
    }
    if (loc_light_radiance_ >= 0) {
        const float rad[3] = {
            command.lighting.radiance.x,
            command.lighting.radiance.y,
            command.lighting.radiance.z,
        };
        gl_->Uniform3fv(loc_light_radiance_, 1, rad);
    }
    if (loc_ambient_ >= 0) {
        const float amb[3] = {
            command.lighting.ambient.x,
            command.lighting.ambient.y,
            command.lighting.ambient.z,
        };
        gl_->Uniform3fv(loc_ambient_, 1, amb);
    }
    if (loc_selection_tint_ >= 0) {
        const float tint[3] = {
            command.selected ? 0.42f : 0.0f,
            command.selected ? 0.28f : 0.0f,
            command.selected ? 0.06f : 0.0f,
        };
        gl_->Uniform3fv(loc_selection_tint_, 1, tint);
    }
    if (loc_base_color_ >= 0) {
        const float bc[3] = {
            command.surface.base_color.x,
            command.surface.base_color.y,
            command.surface.base_color.z,
        };
        gl_->Uniform3fv(loc_base_color_, 1, bc);
    }
    if (loc_pbr_params_ >= 0) {
        const float pbr[3] = {
            command.surface.metallic,
            command.surface.roughness,
            command.surface.ambient_occlusion,
        };
        gl_->Uniform3fv(loc_pbr_params_, 1, pbr);
    }
    if (loc_emissive_radiance_ >= 0) {
        const float em[3] = {
            command.surface.emissive_radiance.x,
            command.surface.emissive_radiance.y,
            command.surface.emissive_radiance.z,
        };
        gl_->Uniform3fv(loc_emissive_radiance_, 1, em);
    }
    if (loc_surface_flags_ >= 0) {
        const float flags[3] = {
            command.surface.opacity,
            command.surface.vertex_color_weight,
            command.surface.unlit_overlay ? 1.0f : 0.0f,
        };
        gl_->Uniform3fv(loc_surface_flags_, 1, flags);
    }

    const bool translucent = (command.surface.opacity < 0.999f) && !command.surface.unlit_overlay;
    if (translucent && gl_->BlendFunc != nullptr && gl_->Disable != nullptr && gl_->DepthMask != nullptr) {
        gl_->Enable(kGlBlend);
        gl_->BlendFunc(kGlSrcAlpha, kGlOneMinusSrcAlpha);
        gl_->DepthMask(kGlFalse);
    }

    const auto byte_offset = static_cast<std::uintptr_t>(command.index_offset) * sizeof(std::uint32_t);
    gl_->BindVertexArray(rec.vao);
    gl_->DrawElements(
        kGlTriangles,
        static_cast<GLsizei>(draw_count),
        kGlUnsignedInt,
        reinterpret_cast<const void*>(byte_offset));
    gl_->BindVertexArray(0);

    if (translucent && gl_->BlendFunc != nullptr && gl_->Disable != nullptr && gl_->DepthMask != nullptr) {
        gl_->DepthMask(kGlTrue);
        gl_->Disable(kGlBlend);
    }
    return core::Status::ok();
}

core::Status OpenGL33RenderDevice::endFrame() {
    if (!initialized_ || !in_frame_) {
        return core::Status::fail(core::ErrorCode::InvalidState, "endFrame called without matching beginFrame");
    }
    gl_->UseProgram(0);
    in_frame_ = false;
    return core::Status::ok();
}

} // namespace finaleffect::rendering
