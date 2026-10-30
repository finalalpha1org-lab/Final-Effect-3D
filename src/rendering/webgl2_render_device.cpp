// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/rendering/webgl2_render_device.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>

#if defined(_WIN32)
#define FE_GLES_APIENTRY __stdcall
#else
#define FE_GLES_APIENTRY
#endif

namespace finaleffect::rendering {

namespace {

// Khronos OpenGL ES 3.0 / WebGL 2.0 fundamental types and constants.
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

// Note: WebGL 2.0 requires '#version 300 es' on the very first line without leading whitespace.
constexpr const char* kVertexShaderGlslEs300 =
    "#version 300 es\n"
    "precision highp float;\n"
    "layout(location = 0) in vec3 a_position;\n"
    "layout(location = 1) in vec3 a_normal;\n"
    "layout(location = 2) in vec3 a_color;\n"
    "layout(location = 3) in vec2 a_uv;\n"
    "uniform mat4 u_model;\n"
    "uniform mat4 u_normal_matrix;\n"
    "uniform mat4 u_view_proj;\n"
    "out vec3 v_world_pos;\n"
    "out vec3 v_world_normal;\n"
    "out vec3 v_color;\n"
    "out vec2 v_uv;\n"
    "void main() {\n"
    "    vec4 world_pos = u_model * vec4(a_position, 1.0);\n"
    "    v_world_pos = world_pos.xyz;\n"
    "    v_world_normal = normalize((u_normal_matrix * vec4(a_normal, 0.0)).xyz);\n"
    "    v_color = a_color;\n"
    "    v_uv = a_uv;\n"
    "    gl_Position = u_view_proj * world_pos;\n"
    "}\n";

// Energy-conserving metallic-roughness Cook-Torrance GGX + Schlick Fresnel PBR fragment shader (GLSL ES 3.00).
constexpr const char* kFragmentShaderGlslEs300 =
    "#version 300 es\n"
    "precision highp float;\n"
    "in vec3 v_world_pos;\n"
    "in vec3 v_world_normal;\n"
    "in vec3 v_color;\n"
    "in vec2 v_uv;\n"
    "uniform vec3 u_camera_pos;\n"
    "uniform vec3 u_light_dir;\n"
    "uniform vec3 u_light_radiance;\n"
    "uniform vec3 u_extra_light_dirs[3];\n"
    "uniform vec3 u_extra_light_radiances[3];\n"
    "uniform vec3 u_ambient;\n"
    "uniform vec3 u_selection_tint;\n"
    "uniform vec3 u_base_color;\n"
    "uniform vec3 u_pbr_params;\n"
    "uniform vec3 u_emissive_radiance;\n"
    "uniform vec3 u_surface_flags;\n"
    "out vec4 o_frag_color;\n"
    "const float kPi = 3.14159265359;\n"
    "vec3 fresnelSchlick(float cos_theta, vec3 f0) {\n"
    "    float x = clamp(1.0 - cos_theta, 0.0, 1.0);\n"
    "    float x2 = x * x;\n"
    "    return f0 + (1.0 - f0) * (x2 * x2 * x);\n"
    "}\n"
    "float distributionGgx(float ndoth, float roughness) {\n"
    "    float a = roughness * roughness;\n"
    "    float a2 = a * a;\n"
    "    float denom = ndoth * ndoth * (a2 - 1.0) + 1.0;\n"
    "    return a2 / max(kPi * denom * denom, 1.0e-6);\n"
    "}\n"
    "float geometrySchlickGgx(float ndotx, float k) {\n"
    "    return ndotx / max(ndotx * (1.0 - k) + k, 1.0e-5);\n"
    "}\n"
    "vec3 evalCookTorranceLobe(vec3 n, vec3 v, vec3 l, vec3 radiance, vec3 albedo, vec3 f0, float metallic, float roughness, float k_direct) {\n"
    "    float ndotl = max(dot(n, l), 0.0);\n"
    "    if (ndotl <= 0.0) { return vec3(0.0); }\n"
    "    float ndotv = max(dot(n, v), 1.0e-4);\n"
    "    vec3 h = normalize(v + l);\n"
    "    float ndoth = max(dot(n, h), 0.0);\n"
    "    float hdotv = max(dot(h, v), 0.0);\n"
    "    float ndf = distributionGgx(ndoth, roughness);\n"
    "    float geom = geometrySchlickGgx(ndotv, k_direct) * geometrySchlickGgx(ndotl, k_direct);\n"
    "    vec3 fresnel = fresnelSchlick(hdotv, f0);\n"
    "    vec3 kd = (vec3(1.0) - fresnel) * (1.0 - metallic);\n"
    "    vec3 diffuse = kd * albedo * (1.0 / kPi);\n"
    "    vec3 specular = (ndf * geom * fresnel) / max(4.0 * ndotv * ndotl, 1.0e-4);\n"
    "    return (diffuse + specular) * radiance * ndotl;\n"
    "}\n"
    "void main() {\n"
    "    float opacity = clamp(u_surface_flags.x, 0.0, 1.0);\n"
    "    vec3 weighted_vcol = mix(vec3(1.0), v_color, clamp(u_surface_flags.y, 0.0, 1.0));\n"
    "    vec3 albedo = u_base_color * weighted_vcol;\n"
    "    if (u_surface_flags.z > 0.5) {\n"
    "        o_frag_color = vec4(clamp(albedo + u_selection_tint, 0.0, 1.0), opacity);\n"
    "        return;\n"
    "    }\n"
    "    vec3 n = normalize(v_world_normal);\n"
    "    vec3 v = normalize(u_camera_pos - v_world_pos);\n"
    "    float ndotv = max(dot(n, v), 1.0e-4);\n"
    "    float metallic = clamp(u_pbr_params.x, 0.0, 1.0);\n"
    "    float roughness = clamp(u_pbr_params.y, 0.045, 1.0);\n"
    "    float ao = clamp(u_pbr_params.z, 0.0, 1.0);\n"
    "    float k_direct = ((roughness + 1.0) * (roughness + 1.0)) * 0.125;\n"
    "    vec3 f0 = mix(vec3(0.04), albedo, metallic);\n"
    "    vec3 key_l = normalize(u_light_dir);\n"
    "    vec3 fill_l = normalize(vec3(-0.55, 0.35, -0.65));\n"
    "    vec3 lo = evalCookTorranceLobe(n, v, key_l, u_light_radiance, albedo, f0, metallic, roughness, k_direct)\n"
    "            + evalCookTorranceLobe(n, v, fill_l, u_light_radiance * 0.22, albedo, f0, metallic, roughness, k_direct);\n"
    "    for (int li = 0; li < 3; ++li) {\n"
    "        vec3 ex_rad = u_extra_light_radiances[li];\n"
    "        vec3 ex_dir = u_extra_light_dirs[li];\n"
    "        if (dot(ex_rad, ex_rad) > 1.0e-8 && dot(ex_dir, ex_dir) > 1.0e-8) {\n"
    "            lo += evalCookTorranceLobe(n, v, normalize(ex_dir), ex_rad, albedo, f0, metallic, roughness, k_direct);\n"
    "        }\n"
    "    }\n"
    "    float sky = n.y * 0.5 + 0.5;\n"
    "    vec3 env_irradiance = u_ambient * (0.65 + 0.70 * sky);\n"
    "    vec3 f_env = fresnelSchlick(ndotv, f0);\n"
    "    vec3 kd_env = (vec3(1.0) - f_env) * (1.0 - metallic);\n"
    "    vec3 env_spec = f_env * env_irradiance * (1.0 - 0.75 * roughness);\n"
    "    vec3 ambient_term = (kd_env * albedo * env_irradiance + env_spec) * ao;\n"
    "    vec3 hdr = ambient_term + lo + u_emissive_radiance;\n"
    "    float rim = 1.0 - ndotv;\n"
    "    hdr += u_selection_tint * (0.30 + 0.80 * rim * rim);\n"
    "    vec3 mapped = hdr / (vec3(1.0) + max(hdr, vec3(0.0)));\n"
    "    vec3 srgb = pow(clamp(mapped, 0.0, 1.0), vec3(1.0 / 2.2));\n"
    "    o_frag_color = vec4(srgb, opacity);\n"
    "}\n";

// WebGL 2.0 / GLSL ES 3.00 procedural viewport ground grid shaders ('#version 300 es' on line 1).
constexpr const char* kGridVertexShaderGlslEs300 =
    "#version 300 es\n"
    "precision highp float;\n"
    "layout(location = 0) in vec3 a_position;\n"
    "out vec2 v_ndc;\n"
    "void main() {\n"
    "    v_ndc = a_position.xy;\n"
    "    gl_Position = vec4(a_position.xy, 0.0, 1.0);\n"
    "}\n";

constexpr const char* kGridFragmentShaderGlslEs300 =
    "#version 300 es\n"
    "precision highp float;\n"
    "in vec2 v_ndc;\n"
    "uniform vec3 u_grid_cam_pos;\n"
    "uniform vec3 u_grid_cam_right;\n"
    "uniform vec3 u_grid_cam_up;\n"
    "uniform vec3 u_grid_cam_fwd;\n"
    "uniform vec3 u_grid_frustum;\n"
    "uniform vec3 u_grid_depth;\n"
    "uniform vec3 u_grid_steps;\n"
    "uniform vec3 u_grid_fade;\n"
    "uniform vec3 u_grid_phase_x;\n"
    "uniform vec3 u_grid_phase_z;\n"
    "uniform vec3 u_grid_axis_px;\n"
    "uniform vec3 u_grid_ndc_step;\n"
    "out vec4 o_frag_color;\n"
    "const float kGridDepthBias = 0.000025;\n"
    "float evalCascadeLine(float delta_c, float phase_c, float step_sz, float footprint, float half_w_px) {\n"
    "    if (step_sz <= 1.0e-6) { return 0.0; }\n"
    "    float nyq = 1.0 - smoothstep(0.22 * step_sz, 0.48 * step_sz, footprint);\n"
    "    if (nyq <= 1.0e-4) { return 0.0; }\n"
    "    float u = (delta_c + phase_c) / step_sz;\n"
    "    float dist_world = abs(fract(u + 0.5) - 0.5) * step_sz;\n"
    "    float dist_px = dist_world / max(footprint, 1.0e-6);\n"
    "    float cov = 1.0 - smoothstep(half_w_px - 0.55, half_w_px + 0.55, dist_px);\n"
    "    return cov * nyq;\n"
    "}\n"
    "void main() {\n"
    "    bool is_ortho = (u_grid_frustum.z > 0.5);\n"
    "    bool zero_to_one = (u_grid_depth.z > 0.5);\n"
    "    vec3 R = u_grid_cam_right;\n"
    "    vec3 U = u_grid_cam_up;\n"
    "    vec3 F = u_grid_cam_fwd;\n"
    "    vec3 o_rel = vec3(0.0);\n"
    "    vec3 ray_d = F;\n"
    "    if (is_ortho) {\n"
    "        o_rel = R * (v_ndc.x * u_grid_frustum.x) + U * (v_ndc.y * u_grid_frustum.y);\n"
    "    } else {\n"
    "        ray_d = F + R * (v_ndc.x * u_grid_frustum.x) + U * (v_ndc.y * u_grid_frustum.y);\n"
    "    }\n"
    "    float origin_y = u_grid_cam_pos.y + o_rel.y;\n"
    "    if (is_ortho && abs(ray_d.y) <= 1.0e-4) {\n"
    "        float px_world_y = max(\n"
    "            abs(u_grid_ndc_step.y * u_grid_frustum.y * U.y) + abs(u_grid_ndc_step.x * u_grid_frustum.x * R.y),\n"
    "            1.0e-5);\n"
    "        float line_px = abs(origin_y) / px_world_y;\n"
    "        if (line_px > 1.85) { discard; }\n"
    "        float t_mid = max(-dot(u_grid_cam_pos, F), 0.25);\n"
    "        float ndc_z_h = clamp(-u_grid_depth.x * t_mid + u_grid_depth.y, -0.999, 0.999);\n"
    "        float win_d_h = zero_to_one ? clamp(ndc_z_h, 0.0, 0.999) : (ndc_z_h * 0.5 + 0.5);\n"
    "        float cov_h = 1.0 - smoothstep(0.65, 1.80, line_px);\n"
    "        float a_h = clamp(cov_h * 0.84 * u_grid_fade.z, 0.0, 1.0);\n"
    "        if (a_h <= 0.004) { discard; }\n"
    "        float z_w = abs(F.z) / max(abs(F.x) + abs(F.z), 1.0e-4);\n"
    "        vec3 h_col = mix(vec3(0.26, 0.58, 0.96), vec3(0.92, 0.30, 0.33), z_w);\n"
    "        gl_FragDepth = clamp(win_d_h + kGridDepthBias, 0.0, 1.0);\n"
    "        o_frag_color = vec4(h_col, a_h);\n"
    "        return;\n"
    "    }\n"
    "    if (abs(ray_d.y) <= 1.0e-6) { discard; }\n"
    "    float t = -origin_y / ray_d.y;\n"
    "    if (t <= 0.0) { discard; }\n"
    "    float ndc_z = is_ortho ? (-u_grid_depth.x * t + u_grid_depth.y) : (-u_grid_depth.x + u_grid_depth.y / t);\n"
    "    float min_ndc_z = zero_to_one ? 0.0 : -1.0;\n"
    "    if (ndc_z < min_ndc_z || ndc_z > 1.0) { discard; }\n"
    "    float delta_x = o_rel.x + t * ray_d.x;\n"
    "    float delta_z = o_rel.z + t * ray_d.z;\n"
    "    float radial_dist = length(vec2(delta_x, delta_z));\n"
    "    if (radial_dist >= u_grid_fade.y) { discard; }\n"
    "    float radial_fade = 1.0 - smoothstep(u_grid_fade.y * 0.36, u_grid_fade.y, radial_dist);\n"
    "    float angle_fade = 1.0;\n"
    "    if (!is_ortho) {\n"
    "        float abs_sin_pitch = abs(ray_d.y) / max(length(ray_d), 1.0e-6);\n"
    "        angle_fade = smoothstep(0.012, 0.13, abs_sin_pitch);\n"
    "    }\n"
    "    float global_fade = radial_fade * angle_fade * u_grid_fade.z;\n"
    "    if (global_fade <= 0.004) { discard; }\n"
    "    float sx = is_ortho ? (u_grid_ndc_step.x * u_grid_frustum.x) : (u_grid_ndc_step.x * t * u_grid_frustum.x);\n"
    "    float sy = is_ortho ? (u_grid_ndc_step.y * u_grid_frustum.y) : (u_grid_ndc_step.y * t * u_grid_frustum.y);\n"
    "    float inv_dy = 1.0 / ray_d.y;\n"
    "    float jx_x = sx * (R.x - R.y * inv_dy * ray_d.x);\n"
    "    float jx_z = sx * (R.z - R.y * inv_dy * ray_d.z);\n"
    "    float jy_x = sy * (U.x - U.y * inv_dy * ray_d.x);\n"
    "    float jy_z = sy * (U.z - U.y * inv_dy * ray_d.z);\n"
    "    float fw_x = length(vec2(jx_x, jy_x));\n"
    "    float fw_z = length(vec2(jx_z, jy_z));\n"
    "    float cov_fine = max(\n"
    "        evalCascadeLine(delta_x, u_grid_phase_x.x, u_grid_steps.x, fw_x, 0.65),\n"
    "        evalCascadeLine(delta_z, u_grid_phase_z.x, u_grid_steps.x, fw_z, 0.65));\n"
    "    float cov_minor = max(\n"
    "        evalCascadeLine(delta_x, u_grid_phase_x.y, u_grid_steps.y, fw_x, 0.78),\n"
    "        evalCascadeLine(delta_z, u_grid_phase_z.y, u_grid_steps.y, fw_z, 0.78));\n"
    "    float cov_major = max(\n"
    "        evalCascadeLine(delta_x, u_grid_phase_x.z, u_grid_steps.z, fw_x, 1.06),\n"
    "        evalCascadeLine(delta_z, u_grid_phase_z.z, u_grid_steps.z, fw_z, 1.06));\n"
    "    float blend = u_grid_fade.x;\n"
    "    float a_fine = cov_fine * 0.28 * (1.0 - blend);\n"
    "    float a_minor = cov_minor * mix(0.56, 0.28, blend);\n"
    "    float a_major = cov_major * 0.56;\n"
    "    float alpha = max(a_fine, max(a_minor, a_major));\n"
    "    float major_mix = (alpha > 1.0e-5) ? clamp(a_major / alpha, 0.0, 1.0) : 0.0;\n"
    "    vec3 color = mix(vec3(0.35, 0.40, 0.48), vec3(0.50, 0.56, 0.66), major_mix);\n"
    "    float world_x = delta_x + u_grid_axis_px.x;\n"
    "    float world_z = delta_z + u_grid_axis_px.y;\n"
    "    float px_dist_x_axis = abs(world_z) / max(fw_z, 1.0e-6);\n"
    "    float px_dist_z_axis = abs(world_x) / max(fw_x, 1.0e-6);\n"
    "    float x_axis_nyq = 1.0 - smoothstep(0.30 * u_grid_steps.z, 0.90 * u_grid_steps.z, fw_z);\n"
    "    float z_axis_nyq = 1.0 - smoothstep(0.30 * u_grid_steps.z, 0.90 * u_grid_steps.z, fw_x);\n"
    "    float cov_x_axis = (1.0 - smoothstep(0.75, 1.85, px_dist_x_axis)) * x_axis_nyq;\n"
    "    float cov_z_axis = (1.0 - smoothstep(0.75, 1.85, px_dist_z_axis)) * z_axis_nyq;\n"
    "    if (cov_x_axis > 1.0e-3 || cov_z_axis > 1.0e-3) {\n"
    "        float axis_cov = max(cov_x_axis, cov_z_axis);\n"
    "        float w_z = cov_z_axis / max(cov_x_axis + cov_z_axis, 1.0e-5);\n"
    "        vec3 axis_col = mix(vec3(0.92, 0.30, 0.33), vec3(0.26, 0.58, 0.96), w_z);\n"
    "        color = mix(color, axis_col, clamp(axis_cov, 0.0, 1.0));\n"
    "        alpha = max(alpha, axis_cov * 0.88);\n"
    "    }\n"
    "    if (px_dist_x_axis < 4.2 && px_dist_z_axis < 4.2) {\n"
    "        float origin_r = length(vec2(px_dist_x_axis, px_dist_z_axis));\n"
    "        float origin_cov = (1.0 - smoothstep(1.65, 3.25, origin_r)) * min(x_axis_nyq, z_axis_nyq);\n"
    "        if (origin_cov > 1.0e-3) {\n"
    "            color = mix(color, vec3(0.96, 0.98, 1.00), origin_cov);\n"
    "            alpha = max(alpha, origin_cov * 0.95);\n"
    "        }\n"
    "    }\n"
    "    float final_alpha = clamp(alpha * global_fade, 0.0, 1.0);\n"
    "    if (final_alpha <= 0.004) { discard; }\n"
    "    float win_depth = zero_to_one ? ndc_z : (ndc_z * 0.5 + 0.5);\n"
    "    gl_FragDepth = clamp(win_depth + kGridDepthBias, 0.0, 1.0);\n"
    "    o_frag_color = vec4(color, final_alpha);\n"
    "}\n";

} // namespace

struct WebGL2RenderDevice::GlEs3DispatchTable {
    const GLubyte* (FE_GLES_APIENTRY* GetString)(GLenum) = nullptr;
    void (FE_GLES_APIENTRY* GetIntegerv)(GLenum, GLint*) = nullptr;
    GLenum (FE_GLES_APIENTRY* GetError)() = nullptr;
    void (FE_GLES_APIENTRY* Viewport)(GLint, GLint, GLsizei, GLsizei) = nullptr;
    void (FE_GLES_APIENTRY* ClearColor)(GLfloat, GLfloat, GLfloat, GLfloat) = nullptr;
    void (FE_GLES_APIENTRY* ClearDepthf)(GLfloat) = nullptr;
    void (FE_GLES_APIENTRY* Clear)(GLbitfield) = nullptr;
    void (FE_GLES_APIENTRY* Enable)(GLenum) = nullptr;
    void (FE_GLES_APIENTRY* Disable)(GLenum) = nullptr;
    void (FE_GLES_APIENTRY* BlendFunc)(GLenum, GLenum) = nullptr;
    void (FE_GLES_APIENTRY* DepthMask)(GLboolean) = nullptr;
    void (FE_GLES_APIENTRY* DepthFunc)(GLenum) = nullptr;
    void (FE_GLES_APIENTRY* CullFace)(GLenum) = nullptr;
    void (FE_GLES_APIENTRY* FrontFace)(GLenum) = nullptr;
    void (FE_GLES_APIENTRY* DrawElements)(GLenum, GLsizei, GLenum, const void*) = nullptr;

    void (FE_GLES_APIENTRY* GenVertexArrays)(GLsizei, GLuint*) = nullptr;
    void (FE_GLES_APIENTRY* BindVertexArray)(GLuint) = nullptr;
    void (FE_GLES_APIENTRY* DeleteVertexArrays)(GLsizei, const GLuint*) = nullptr;

    void (FE_GLES_APIENTRY* GenBuffers)(GLsizei, GLuint*) = nullptr;
    void (FE_GLES_APIENTRY* BindBuffer)(GLenum, GLuint) = nullptr;
    void (FE_GLES_APIENTRY* BufferData)(GLenum, GLsizeiptr, const void*, GLenum) = nullptr;
    void (FE_GLES_APIENTRY* BufferSubData)(GLenum, GLintptr, GLsizeiptr, const void*) = nullptr;
    void (FE_GLES_APIENTRY* DeleteBuffers)(GLsizei, const GLuint*) = nullptr;

    void (FE_GLES_APIENTRY* VertexAttribPointer)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*) = nullptr;
    void (FE_GLES_APIENTRY* EnableVertexAttribArray)(GLuint) = nullptr;

    GLuint (FE_GLES_APIENTRY* CreateShader)(GLenum) = nullptr;
    void (FE_GLES_APIENTRY* ShaderSource)(GLuint, GLsizei, const GLchar* const*, const GLint*) = nullptr;
    void (FE_GLES_APIENTRY* CompileShader)(GLuint) = nullptr;
    void (FE_GLES_APIENTRY* GetShaderiv)(GLuint, GLenum, GLint*) = nullptr;
    void (FE_GLES_APIENTRY* GetShaderInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*) = nullptr;
    void (FE_GLES_APIENTRY* DeleteShader)(GLuint) = nullptr;

    GLuint (FE_GLES_APIENTRY* CreateProgram)() = nullptr;
    void (FE_GLES_APIENTRY* AttachShader)(GLuint, GLuint) = nullptr;
    void (FE_GLES_APIENTRY* LinkProgram)(GLuint) = nullptr;
    void (FE_GLES_APIENTRY* GetProgramiv)(GLuint, GLenum, GLint*) = nullptr;
    void (FE_GLES_APIENTRY* GetProgramInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*) = nullptr;
    void (FE_GLES_APIENTRY* UseProgram)(GLuint) = nullptr;
    void (FE_GLES_APIENTRY* DeleteProgram)(GLuint) = nullptr;

    GLint (FE_GLES_APIENTRY* GetUniformLocation)(GLuint, const GLchar*) = nullptr;
    void (FE_GLES_APIENTRY* UniformMatrix4fv)(GLint, GLsizei, GLboolean, const GLfloat*) = nullptr;
    void (FE_GLES_APIENTRY* Uniform3fv)(GLint, GLsizei, const GLfloat*) = nullptr;
};

WebGL2RenderDevice::WebGL2RenderDevice(platform::GlProcLoaderFn proc_loader)
    : proc_loader_(proc_loader),
      gl_(std::make_unique<GlEs3DispatchTable>()) {
    capabilities_.backend = BackendType::WebGL2;
    capabilities_.backend_name = "WebGL 2.0";
    capabilities_.device_name = "Uninitialized WebGL 2.0 Context";
    capabilities_.clip_space_depth = math::ClipSpaceDepth::NegativeOneToOne;
    capabilities_.max_viewport_dimension = 4096;
}

WebGL2RenderDevice::~WebGL2RenderDevice() {
    shutdown();
}

core::Status WebGL2RenderDevice::initialize(std::uint32_t width, std::uint32_t height) {
    if (initialized_) {
        return core::Status::fail(core::ErrorCode::InvalidState, "WebGL2RenderDevice is already initialized");
    }
    if (proc_loader_ == nullptr) {
        return core::Status::fail(
            core::ErrorCode::RenderInitFailed,
            "WebGL2RenderDevice requires a valid GlProcLoaderFn");
    }
    if (width == 0 || height == 0) {
        return core::Status::fail(
            core::ErrorCode::InvalidArgument,
            "Initial WebGL 2.0 framebuffer dimensions must be non-zero");
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
        loadSym(gl_->ClearDepthf, "glClearDepthf") &&
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
            "Failed to resolve required WebGL 2.0 / GLES 3.0 entry point",
            missing_symbol);
    }

    loadOptSym(gl_->BufferSubData, "glBufferSubData");
    loadOptSym(gl_->Disable, "glDisable");
    loadOptSym(gl_->BlendFunc, "glBlendFunc");
    loadOptSym(gl_->DepthMask, "glDepthMask");

    if (const GLubyte* renderer_str = gl_->GetString(kGlRenderer); renderer_str != nullptr) {
        capabilities_.device_name = reinterpret_cast<const char*>(renderer_str);
    } else {
        capabilities_.device_name = "WebGL 2.0 Browser Context";
    }

    GLint viewport_dims[2] = {4096, 4096};
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

core::Status WebGL2RenderDevice::buildShaderPipeline() {
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
                "GLSL ES 3.00 shader compilation failed: " + log,
                stage_label);
        }
        return core::Result<GLuint>::ok(shader);
    };

    auto vs = compileStage(kGlVertexShader, kVertexShaderGlslEs300, "vertex");
    if (!vs) {
        return vs.error();
    }
    auto fs = compileStage(kGlFragmentShader, kFragmentShaderGlslEs300, "fragment");
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
            "GLSL ES 3.00 program link failed: " + log);
    }

    shader_program_ = program;
    loc_model_ = gl_->GetUniformLocation(shader_program_, "u_model");
    loc_normal_mat_ = gl_->GetUniformLocation(shader_program_, "u_normal_matrix");
    loc_view_proj_ = gl_->GetUniformLocation(shader_program_, "u_view_proj");
    loc_camera_pos_ = gl_->GetUniformLocation(shader_program_, "u_camera_pos");
    loc_light_dir_ = gl_->GetUniformLocation(shader_program_, "u_light_dir");
    loc_light_radiance_ = gl_->GetUniformLocation(shader_program_, "u_light_radiance");
    loc_extra_light_dirs_ = gl_->GetUniformLocation(shader_program_, "u_extra_light_dirs[0]");
    if (loc_extra_light_dirs_ < 0) {
        loc_extra_light_dirs_ = gl_->GetUniformLocation(shader_program_, "u_extra_light_dirs");
    }
    loc_extra_light_radiances_ = gl_->GetUniformLocation(shader_program_, "u_extra_light_radiances[0]");
    if (loc_extra_light_radiances_ < 0) {
        loc_extra_light_radiances_ = gl_->GetUniformLocation(shader_program_, "u_extra_light_radiances");
    }
    loc_ambient_ = gl_->GetUniformLocation(shader_program_, "u_ambient");
    loc_selection_tint_ = gl_->GetUniformLocation(shader_program_, "u_selection_tint");
    loc_base_color_ = gl_->GetUniformLocation(shader_program_, "u_base_color");
    loc_pbr_params_ = gl_->GetUniformLocation(shader_program_, "u_pbr_params");
    loc_emissive_radiance_ = gl_->GetUniformLocation(shader_program_, "u_emissive_radiance");
    loc_surface_flags_ = gl_->GetUniformLocation(shader_program_, "u_surface_flags");
    return core::Status::ok();
}

core::Status WebGL2RenderDevice::ensureGridPipeline() {
    if (grid_program_ != 0 && grid_vao_ != 0) {
        return core::Status::ok();
    }

    auto compileStage = [&](GLenum stage, const char* source, const char* label) -> core::Result<GLuint> {
        const GLuint sh = gl_->CreateShader(stage);
        if (sh == 0) {
            return core::Result<GLuint>::fail(
                core::ErrorCode::ResourceCreationFailed,
                "glCreateShader returned 0",
                label);
        }
        gl_->ShaderSource(sh, 1, &source, nullptr);
        gl_->CompileShader(sh);
        GLint compiled = 0;
        gl_->GetShaderiv(sh, kGlCompileStatus, &compiled);
        if (compiled == 0) {
            GLint log_len = 0;
            gl_->GetShaderiv(sh, kGlInfoLogLength, &log_len);
            std::string log(static_cast<std::size_t>(log_len > 1 ? log_len : 1), '\0');
            if (log_len > 1) {
                gl_->GetShaderInfoLog(sh, log_len, nullptr, log.data());
            }
            gl_->DeleteShader(sh);
            return core::Result<GLuint>::fail(
                core::ErrorCode::ShaderCompileFailed,
                "GLSL ES 3.00 grid shader compilation failed: " + log,
                label);
        }
        return core::Result<GLuint>::ok(sh);
    };

    auto vs = compileStage(kGlVertexShader, kGridVertexShaderGlslEs300, "grid_vertex");
    if (!vs) return vs.error();
    auto fs = compileStage(kGlFragmentShader, kGridFragmentShaderGlslEs300, "grid_fragment");
    if (!fs) {
        gl_->DeleteShader(vs.value());
        return fs.error();
    }

    const GLuint prog = gl_->CreateProgram();
    if (prog == 0) {
        gl_->DeleteShader(vs.value());
        gl_->DeleteShader(fs.value());
        return core::Status::fail(core::ErrorCode::ResourceCreationFailed, "glCreateProgram returned 0");
    }
    gl_->AttachShader(prog, vs.value());
    gl_->AttachShader(prog, fs.value());
    gl_->LinkProgram(prog);
    gl_->DeleteShader(vs.value());
    gl_->DeleteShader(fs.value());

    GLint linked = 0;
    gl_->GetProgramiv(prog, kGlLinkStatus, &linked);
    if (linked == 0) {
        GLint log_len = 0;
        gl_->GetProgramiv(prog, kGlInfoLogLength, &log_len);
        std::string log(static_cast<std::size_t>(log_len > 1 ? log_len : 1), '\0');
        if (log_len > 1) {
            gl_->GetProgramInfoLog(prog, log_len, nullptr, log.data());
        }
        gl_->DeleteProgram(prog);
        return core::Status::fail(
            core::ErrorCode::ShaderCompileFailed,
            "GLSL ES 3.00 grid program link failed: " + log);
    }

    // Internal clip-space fullscreen pass triangle (CCW winding, never registered in Scene).
    const float fs_verts[9] = {
        -1.0f, -1.0f, 0.0f,
         3.0f, -1.0f, 0.0f,
        -1.0f,  3.0f, 0.0f,
    };
    const std::uint32_t fs_indices[3] = {0u, 1u, 2u};

    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    gl_->GenVertexArrays(1, &vao);
    gl_->GenBuffers(1, &vbo);
    gl_->GenBuffers(1, &ebo);
    if (vao == 0 || vbo == 0 || ebo == 0) {
        if (ebo != 0) gl_->DeleteBuffers(1, &ebo);
        if (vbo != 0) gl_->DeleteBuffers(1, &vbo);
        if (vao != 0) gl_->DeleteVertexArrays(1, &vao);
        gl_->DeleteProgram(prog);
        return core::Status::fail(core::ErrorCode::ResourceCreationFailed, "Failed to allocate WebGL2 grid pass VAO");
    }

    gl_->BindVertexArray(vao);
    gl_->BindBuffer(kGlArrayBuffer, vbo);
    gl_->BufferData(kGlArrayBuffer, static_cast<GLsizeiptr>(sizeof(fs_verts)), fs_verts, kGlStaticDraw);
    gl_->BindBuffer(kGlElementArrayBuffer, ebo);
    gl_->BufferData(kGlElementArrayBuffer, static_cast<GLsizeiptr>(sizeof(fs_indices)), fs_indices, kGlStaticDraw);
    gl_->EnableVertexAttribArray(0);
    gl_->VertexAttribPointer(0, 3, kGlFloat, kGlFalse, static_cast<GLsizei>(3 * sizeof(float)), nullptr);
    gl_->BindVertexArray(0);
    gl_->BindBuffer(kGlArrayBuffer, 0);

    grid_program_ = prog;
    grid_vao_ = vao;
    grid_vbo_ = vbo;
    grid_ebo_ = ebo;
    loc_grid_cam_pos_ = gl_->GetUniformLocation(grid_program_, "u_grid_cam_pos");
    loc_grid_cam_right_ = gl_->GetUniformLocation(grid_program_, "u_grid_cam_right");
    loc_grid_cam_up_ = gl_->GetUniformLocation(grid_program_, "u_grid_cam_up");
    loc_grid_cam_fwd_ = gl_->GetUniformLocation(grid_program_, "u_grid_cam_fwd");
    loc_grid_frustum_ = gl_->GetUniformLocation(grid_program_, "u_grid_frustum");
    loc_grid_depth_ = gl_->GetUniformLocation(grid_program_, "u_grid_depth");
    loc_grid_steps_ = gl_->GetUniformLocation(grid_program_, "u_grid_steps");
    loc_grid_fade_ = gl_->GetUniformLocation(grid_program_, "u_grid_fade");
    loc_grid_phase_x_ = gl_->GetUniformLocation(grid_program_, "u_grid_phase_x");
    loc_grid_phase_z_ = gl_->GetUniformLocation(grid_program_, "u_grid_phase_z");
    loc_grid_axis_px_ = gl_->GetUniformLocation(grid_program_, "u_grid_axis_px");
    loc_grid_ndc_step_ = gl_->GetUniformLocation(grid_program_, "u_grid_ndc_step");
    return core::Status::ok();
}

void WebGL2RenderDevice::shutdown() noexcept {
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

    if (grid_ebo_ != 0) {
        gl_->DeleteBuffers(1, &grid_ebo_);
        grid_ebo_ = 0;
    }
    if (grid_vbo_ != 0) {
        gl_->DeleteBuffers(1, &grid_vbo_);
        grid_vbo_ = 0;
    }
    if (grid_vao_ != 0) {
        gl_->DeleteVertexArrays(1, &grid_vao_);
        grid_vao_ = 0;
    }
    if (grid_program_ != 0) {
        gl_->UseProgram(0);
        gl_->DeleteProgram(grid_program_);
        grid_program_ = 0;
    }

    if (shader_program_ != 0) {
        gl_->UseProgram(0);
        gl_->DeleteProgram(shader_program_);
        shader_program_ = 0;
    }

    in_frame_ = false;
    initialized_ = false;
}

core::Status WebGL2RenderDevice::resize(std::uint32_t width, std::uint32_t height) {
    if (!initialized_) {
        return core::Status::fail(core::ErrorCode::InvalidState, "WebGL2RenderDevice not initialized");
    }
    if (width > capabilities_.max_viewport_dimension || height > capabilities_.max_viewport_dimension) {
        return core::Status::fail(core::ErrorCode::InvalidArgument, "Requested viewport exceeds WebGL2 device limit");
    }
    width_ = width;
    height_ = height;
    if (width_ > 0 && height_ > 0) {
        gl_->Viewport(0, 0, static_cast<GLsizei>(width_), static_cast<GLsizei>(height_));
    }
    return core::Status::ok();
}

core::Result<GpuMeshHandle> WebGL2RenderDevice::createMesh(const scene::MeshData& mesh) {
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
            "Mesh index count exceeds WebGL2 GLsizei limit");
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
            "Failed to allocate WebGL2 VAO/VBO/EBO");
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

core::Status WebGL2RenderDevice::updateMesh(GpuMeshHandle handle, const scene::MeshData& mesh) {
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
        return core::Status::fail(core::ErrorCode::InvalidArgument, "Mesh index count exceeds WebGL2 GLsizei limit");
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

void WebGL2RenderDevice::destroyMesh(GpuMeshHandle handle) noexcept {
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

core::Status WebGL2RenderDevice::beginFrame(const FrameClearDesc& clear) {
    if (!initialized_ || in_frame_) {
        return core::Status::fail(core::ErrorCode::InvalidState, "Invalid WebGL2RenderDevice::beginFrame state");
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
    gl_->ClearDepthf(clear.clear_depth);
    gl_->Clear(kGlColorBufferBit | kGlDepthBufferBit);
    gl_->UseProgram(shader_program_);
    return core::Status::ok();
}

core::Status WebGL2RenderDevice::drawEditorGrid(const EditorGridCommand& command) {
    if (!initialized_ || !in_frame_) {
        return core::Status::fail(
            core::ErrorCode::InvalidState,
            "WebGL2RenderDevice::drawEditorGrid called outside active frame");
    }
    if (width_ == 0 || height_ == 0 || command.opacity <= 1.0e-4f) {
        return core::Status::ok();
    }

    auto pipe_st = ensureGridPipeline();
    if (!pipe_st) {
        return pipe_st;
    }

    gl_->UseProgram(grid_program_);

    auto setVec3 = [&](GLint loc, float x, float y, float z) {
        if (loc >= 0) {
            const float v[3] = {x, y, z};
            gl_->Uniform3fv(loc, 1, v);
        }
    };

    setVec3(loc_grid_cam_pos_, command.camera_position.x, command.camera_position.y, command.camera_position.z);
    setVec3(loc_grid_cam_right_, command.camera_right.x, command.camera_right.y, command.camera_right.z);
    setVec3(loc_grid_cam_up_, command.camera_up.x, command.camera_up.y, command.camera_up.z);
    setVec3(loc_grid_cam_fwd_, command.camera_forward.x, command.camera_forward.y, command.camera_forward.z);
    setVec3(
        loc_grid_frustum_,
        command.frustum_half_x,
        command.frustum_half_y,
        command.is_orthographic ? 1.0f : 0.0f);
    setVec3(
        loc_grid_depth_,
        command.proj_22,
        command.proj_23,
        command.zero_to_one_depth ? 1.0f : 0.0f);
    setVec3(loc_grid_steps_, command.step_fine, command.step_minor, command.step_major);
    setVec3(loc_grid_fade_, command.level_blend, command.fade_distance, command.opacity);
    setVec3(loc_grid_phase_x_, command.phase_x.x, command.phase_x.y, command.phase_x.z);
    setVec3(loc_grid_phase_z_, command.phase_z.x, command.phase_z.y, command.phase_z.z);
    setVec3(loc_grid_axis_px_, command.axis_cam_x, command.axis_cam_z, 0.0f);
    setVec3(
        loc_grid_ndc_step_,
        2.0f / static_cast<float>(width_),
        2.0f / static_cast<float>(height_),
        0.0f);

    gl_->Enable(kGlBlend);
    gl_->BlendFunc(kGlSrcAlpha, kGlOneMinusSrcAlpha);
    gl_->DepthMask(kGlFalse);

    gl_->BindVertexArray(grid_vao_);
    gl_->DrawElements(kGlTriangles, 3, kGlUnsignedInt, nullptr);
    gl_->BindVertexArray(0);

    gl_->DepthMask(kGlTrue);
    gl_->Disable(kGlBlend);
    gl_->UseProgram(shader_program_);
    return core::Status::ok();
}

core::Status WebGL2RenderDevice::beginOverlayPass(float clear_depth) {
    if (!initialized_ || !in_frame_) {
        return core::Status::fail(
            core::ErrorCode::InvalidState,
            "WebGL2RenderDevice::beginOverlayPass called outside active frame");
    }
    if (width_ == 0 || height_ == 0) {
        return core::Status::ok();
    }
    gl_->ClearDepthf(clear_depth);
    gl_->Clear(kGlDepthBufferBit);
    return core::Status::ok();
}

core::Status WebGL2RenderDevice::drawMesh(const DrawMeshCommand& command) {
    if (!initialized_ || !in_frame_) {
        return core::Status::fail(core::ErrorCode::InvalidState, "drawMesh called outside active frame");
    }
    const auto it = meshes_.find(command.mesh_handle.value);
    if (it == meshes_.end()) {
        return core::Status::fail(core::ErrorCode::NotFound, "Unknown GpuMeshHandle in WebGL2RenderDevice");
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
    if (loc_extra_light_dirs_ >= 0 || loc_extra_light_radiances_ >= 0) {
        float ex_dirs[9] = {0.0f};
        float ex_rads[9] = {0.0f};
        const std::uint32_t ex_cnt = std::min<std::uint32_t>(
            command.lighting.extra_light_count,
            static_cast<std::uint32_t>(DirectionalLightUniforms::kMaxExtraLights));
        for (std::uint32_t li = 0; li < ex_cnt; ++li) {
            const auto& el = command.lighting.extra_lights[li];
            const math::Vec3 eld = el.direction_to_light.normalized();
            ex_dirs[li * 3 + 0] = eld.x;
            ex_dirs[li * 3 + 1] = eld.y;
            ex_dirs[li * 3 + 2] = eld.z;
            ex_rads[li * 3 + 0] = el.radiance.x;
            ex_rads[li * 3 + 1] = el.radiance.y;
            ex_rads[li * 3 + 2] = el.radiance.z;
        }
        if (loc_extra_light_dirs_ >= 0) {
            gl_->Uniform3fv(loc_extra_light_dirs_, 3, ex_dirs);
        }
        if (loc_extra_light_radiances_ >= 0) {
            gl_->Uniform3fv(loc_extra_light_radiances_, 3, ex_rads);
        }
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

core::Status WebGL2RenderDevice::endFrame() {
    if (!initialized_ || !in_frame_) {
        return core::Status::fail(core::ErrorCode::InvalidState, "endFrame called without matching beginFrame");
    }
    gl_->UseProgram(0);
    in_frame_ = false;
    return core::Status::ok();
}

} // namespace finaleffect::rendering
