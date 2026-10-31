// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/rendering/webgl2_render_device.hpp"
#include "finaleffect/rendering/lighting_system.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

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
constexpr GLenum kGlNone = 0;
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
constexpr GLenum kGlScissorTest = 0x0C11u;
constexpr GLenum kGlMaxViewportDims = 0x0D3Au;
constexpr GLenum kGlTexture2D = 0x0DE1u;
constexpr GLenum kGlUnsignedByte = 0x1401u;
constexpr GLenum kGlUnsignedInt = 0x1405u;
constexpr GLenum kGlFloat = 0x1406u;
constexpr GLenum kGlHalfFloat = 0x140Bu;
constexpr GLenum kGlDepthComponent = 0x1902u;
constexpr GLenum kGlRgba = 0x1908u;
constexpr GLenum kGlRenderer = 0x1F01u;
constexpr GLenum kGlNearest = 0x2600u;
constexpr GLenum kGlLinear = 0x2601u;
constexpr GLenum kGlLinearMipmapLinear = 0x2703u;
constexpr GLenum kGlTextureMagFilter = 0x2800u;
constexpr GLenum kGlTextureMinFilter = 0x2801u;
constexpr GLenum kGlTextureWrapS = 0x2802u;
constexpr GLenum kGlTextureWrapT = 0x2803u;
constexpr GLenum kGlTextureWrapR = 0x8072u;
constexpr GLenum kGlRgba8 = 0x8058u;
constexpr GLenum kGlClampToEdge = 0x812Fu;
constexpr GLenum kGlTextureBaseLevel = 0x813Cu;
constexpr GLenum kGlTextureMaxLevel = 0x813Du;
constexpr GLenum kGlDepthComponent24 = 0x81A6u;
constexpr GLenum kGlTexture0 = 0x84C0u;
constexpr GLenum kGlTexture1 = 0x84C1u;
constexpr GLenum kGlTextureCubeMap = 0x8513u;
constexpr GLenum kGlTextureCubeMapPositiveX = 0x8515u;
constexpr GLenum kGlRgba16f = 0x881Au;
constexpr GLenum kGlArrayBuffer = 0x8892u;
constexpr GLenum kGlElementArrayBuffer = 0x8893u;
constexpr GLenum kGlStaticDraw = 0x88E4u;
constexpr GLenum kGlDynamicDraw = 0x88E8u;
constexpr GLenum kGlFragmentShader = 0x8B30u;
constexpr GLenum kGlVertexShader = 0x8B31u;
constexpr GLenum kGlCompileStatus = 0x8B81u;
constexpr GLenum kGlLinkStatus = 0x8B82u;
constexpr GLenum kGlInfoLogLength = 0x8B84u;
constexpr GLenum kGlFramebufferComplete = 0x8CD5u;
constexpr GLenum kGlDepthAttachment = 0x8D00u;
constexpr GLenum kGlFramebuffer = 0x8D40u;

constexpr const char* kShadowVertexShaderGlslEs300 = R"(#version 300 es
precision highp float;
precision highp int;

layout(location = 0) in vec3 a_position;
uniform mat4 u_light_mvp;

void main() {
    gl_Position = u_light_mvp * vec4(a_position, 1.0);
}
)";

constexpr const char* kShadowFragmentShaderGlslEs300 = R"(#version 300 es
precision highp float;
precision highp int;

void main() {
}
)";

constexpr const char* kVertexShaderGlslEs300 = R"(#version 300 es
precision highp float;
precision highp int;

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

// Unified Stage 5.4 GLSL ES 3.00 Cook-Torrance GGX PBR + Directional/Point/Spot/Area +
// 2D Depth Shadow Atlas + Light Linking + Order-2 SH9 / RGBA16F Cubemap Split-Sum IBL shader.
// Split across <=8 KB raw string segments so MSVC (/WX, C2026 16,380-byte literal limit) compiles cleanly.
constexpr const char* kFragmentShaderGlslEs300PartA = R"(#version 300 es
precision highp float;
precision highp int;

in vec3 v_world_pos;
in vec3 v_world_normal;
in vec3 v_color;
in vec2 v_uv;

uniform vec3 u_camera_pos;
uniform vec3 u_light_dir;
uniform vec3 u_light_radiance;
uniform vec3 u_extra_light_dirs[3];
uniform vec3 u_extra_light_radiances[3];
uniform vec3 u_ambient;
uniform vec3 u_selection_tint;

uniform vec3 u_base_color;
uniform vec3 u_pbr_params;        // x = metallic, y = roughness, z = ambient_occlusion
uniform vec3 u_emissive_radiance;
uniform vec3 u_surface_flags;     // x = opacity, y = vertex_color_weight, z = unlit_overlay

uniform vec4 u_light_meta;
uniform int u_light_mask;
uniform vec4 u_light_pos_type[12];
uniform vec4 u_light_dir_range[12];
uniform vec4 u_light_radiance_atten[12];
uniform vec4 u_light_shape[12];
uniform vec4 u_light_tangent_shadow[12];
uniform vec4 u_light_shadow_params[12];

uniform mat4 u_shadow_2d_vp[4];
uniform vec4 u_shadow_2d_rect[4];
uniform vec4 u_point_shadow_params[2];
uniform vec4 u_point_shadow_far;

uniform vec4 u_env_params;
uniform vec4 u_env_rot_scales;
uniform vec3 u_overflow_irradiance;
uniform vec3 u_ibl_sh[9];

uniform highp sampler2D u_shadow_atlas;
uniform highp samplerCube u_ibl_cubemap;

out vec4 o_frag_color;

const float kPi = 3.14159265359;
const float kPointGuardTan = 1.03;

vec3 fresnelSchlick(float cos_theta, vec3 f0) {
    float x = clamp(1.0 - cos_theta, 0.0, 1.0);
    float x2 = x * x;
    return f0 + (1.0 - f0) * (x2 * x2 * x);
}

vec3 fresnelSchlickRoughness(float cos_theta, vec3 f0, float roughness) {
    float x = clamp(1.0 - cos_theta, 0.0, 1.0);
    float x2 = x * x;
    return f0 + (max(vec3(1.0 - roughness), f0) - f0) * (x2 * x2 * x);
}

vec2 envBrdfApprox(float roughness, float ndotv) {
    vec4 c0 = vec4(-1.0, -0.0275, -0.572, 0.022);
    vec4 c1 = vec4(1.0, 0.0425, 1.04, -0.04);
    vec4 r = roughness * c0 + c1;
    float a004 = min(r.x * r.x, exp2(-9.28 * ndotv)) * r.x + r.y;
    return clamp(vec2(-1.04, 1.04) * a004 + r.zw, vec2(0.0), vec2(1.0));
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

float evalDistanceAttenuation(float dist, float range, float exponent, float src_radius, int mode) {
    if (dist >= range || range <= 1.0e-4) {
        return 0.0;
    }
    float u = clamp(dist / range, 0.0, 1.0);
    float d_eff = max(dist, max(src_radius, 0.05));
    if (mode == 0) {
        float u2 = u * u;
        float w = max(0.0, 1.0 - u2 * u2);
        float denom = (abs(exponent - 2.0) <= 1.0e-3) ? (d_eff * d_eff) : pow(d_eff, max(exponent, 0.0));
        return (w * w) / max(denom, 1.0e-4);
    }
    if (mode == 1) {
        float om = 1.0 - u;
        return (om * om) / (1.0 + 0.18 * exponent * d_eff * d_eff);
    }
    return (1.0 - u) / (1.0 + 0.12 * exponent * d_eff);
}
)";

constexpr const char* kFragmentShaderGlslEs300PartB = R"(
vec2 concentricDisk(float sx, float sy) {
    if (abs(sx) <= 1.0e-6 && abs(sy) <= 1.0e-6) {
        return vec2(0.0);
    }
    float r = 0.0;
    float theta = 0.0;
    if (abs(sx) > abs(sy)) {
        r = sx;
        theta = (kPi * 0.25) * (sy / sx);
    } else {
        r = sy;
        theta = (kPi * 0.5) - (kPi * 0.25) * (sx / sy);
    }
    return vec2(r * cos(theta), r * sin(theta));
}

vec3 evalSh9Irradiance(vec3 d) {
    float x = d.x;
    float y = d.y;
    float z = d.z;
    vec3 res = u_ibl_sh[0] * 0.88622693
             + u_ibl_sh[1] * (1.02332671 * y)
             + u_ibl_sh[2] * (1.02332671 * z)
             + u_ibl_sh[3] * (1.02332671 * x)
             + u_ibl_sh[4] * (0.85808553 * x * y)
             + u_ibl_sh[5] * (0.85808553 * y * z)
             + u_ibl_sh[6] * (0.24770796 * (3.0 * z * z - 1.0))
             + u_ibl_sh[7] * (0.85808553 * x * z)
             + u_ibl_sh[8] * (0.42904277 * (x * x - y * y));
    return max(res, vec3(0.0));
}

float sampleShadowTap(vec2 base_uv, vec2 offs, float step_uv, vec2 min_uv, vec2 max_uv, float cmp_z) {
    vec2 suv = clamp(base_uv + offs * step_uv, min_uv, max_uv);
    float stored = textureLod(u_shadow_atlas, suv, 0.0).r;
    return (cmp_z <= stored) ? 1.0 : 0.0;
}

float filterShadowRect(vec4 uv_rect, float u, float v, float z_win, float depth_bias, float filter_code, float extra_soft) {
    float inv_atlas = u_light_meta.w;
    if (inv_atlas <= 0.0 || uv_rect.z <= 0.0 || u < 0.0 || u > 1.0 || v < 0.0 || v > 1.0 || z_win <= 0.0 || z_win > 1.0) {
        return 1.0;
    }
    vec2 min_uv = uv_rect.xy + vec2(0.5 * inv_atlas);
    vec2 max_uv = uv_rect.xy + uv_rect.zw - vec2(0.5 * inv_atlas);
    vec2 base_uv = uv_rect.xy + vec2(u, v) * uv_rect.zw;
    float cmp_z = z_win - depth_bias;

    if (filter_code < 0.0) {
        return sampleShadowTap(base_uv, vec2(0.0), 0.0, min_uv, max_uv, cmp_z);
    }

    bool wide_pcf = (filter_code >= 50.0);
    float softness = (wide_pcf ? (filter_code - 100.0) : filter_code) * extra_soft;
    float step_uv = inv_atlas * softness;

    float sum = sampleShadowTap(base_uv, vec2(-0.75, -0.25), step_uv, min_uv, max_uv, cmp_z)
              + sampleShadowTap(base_uv, vec2( 0.25, -0.75), step_uv, min_uv, max_uv, cmp_z)
              + sampleShadowTap(base_uv, vec2(-0.25,  0.75), step_uv, min_uv, max_uv, cmp_z)
              + sampleShadowTap(base_uv, vec2( 0.75,  0.25), step_uv, min_uv, max_uv, cmp_z);
    if (!wide_pcf) {
        return sum * 0.25;
    }
    sum += sampleShadowTap(base_uv, vec2(-1.55,  0.85), step_uv, min_uv, max_uv, cmp_z)
         + sampleShadowTap(base_uv, vec2( 1.55, -0.85), step_uv, min_uv, max_uv, cmp_z)
         + sampleShadowTap(base_uv, vec2(-0.85, -1.55), step_uv, min_uv, max_uv, cmp_z)
         + sampleShadowTap(base_uv, vec2( 0.85,  1.55), step_uv, min_uv, max_uv, cmp_z);
    return sum * 0.125;
}

float evalLightShadow(int shadow_slot, vec4 shadow_params, vec3 light_pos, vec3 world_pos, vec3 n, vec3 l_dir, float area_soft_boost) {
    if (shadow_slot < 0 || shadow_params.x <= 1.0e-4 || u_light_meta.w <= 0.0) {
        return 1.0;
    }
    float ndotl = clamp(dot(n, l_dir), 0.0, 1.0);
    float slope = 1.0 - ndotl;
    vec3 biased_pos = world_pos + n * (shadow_params.z * (0.35 + 0.65 * slope));
    float depth_bias = shadow_params.y * (1.0 + 1.5 * slope);
    float raw_vis = 1.0;

    if (shadow_slot < 4) {
        mat4 vp = (shadow_slot == 0) ? u_shadow_2d_vp[0]
                : (shadow_slot == 1) ? u_shadow_2d_vp[1]
                : (shadow_slot == 2) ? u_shadow_2d_vp[2] : u_shadow_2d_vp[3];
        vec4 rect = (shadow_slot == 0) ? u_shadow_2d_rect[0]
                  : (shadow_slot == 1) ? u_shadow_2d_rect[1]
                  : (shadow_slot == 2) ? u_shadow_2d_rect[2] : u_shadow_2d_rect[3];
        vec4 clip = vp * vec4(biased_pos, 1.0);
        if (clip.w > 1.0e-5) {
            vec3 ndc = clip.xyz / clip.w;
            raw_vis = filterShadowRect(
                rect,
                ndc.x * 0.5 + 0.5,
                ndc.y * 0.5 + 0.5,
                ndc.z * 0.5 + 0.5,
                depth_bias,
                shadow_params.w,
                area_soft_boost);
        }
    } else if (shadow_slot >= 100 && shadow_slot <= 101) {
        int p_idx = shadow_slot - 100;
        vec4 p_params = (p_idx == 0) ? u_point_shadow_params[0] : u_point_shadow_params[1];
        float z_far = (p_idx == 0) ? u_point_shadow_far.x : u_point_shadow_far.y;
        float z_near = p_params.w;

        vec3 d = biased_pos - light_pos;
        vec3 ad = abs(d);
        int face = 0;
        float xv = 0.0;
        float yv = 0.0;
        float z_eye = 1.0;
        if (ad.x >= ad.y && ad.x >= ad.z) {
            z_eye = ad.x;
            if (d.x >= 0.0) { face = 0; xv = d.z;  yv = d.y; }
            else            { face = 1; xv = -d.z; yv = d.y; }
        } else if (ad.y >= ad.x && ad.y >= ad.z) {
            z_eye = ad.y;
            if (d.y >= 0.0) { face = 2; xv = d.x; yv = d.z; }
            else            { face = 3; xv = d.x; yv = -d.z; }
        } else {
            z_eye = ad.z;
            if (d.z >= 0.0) { face = 4; xv = -d.x; yv = d.y; }
            else            { face = 5; xv = d.x;  yv = d.y; }
        }

        if (z_eye > z_near && z_eye < z_far) {
            float inv_guard = 1.0 / (z_eye * kPointGuardTan);
            float u = 0.5 * (xv * inv_guard) + 0.5;
            float v = 0.5 * (yv * inv_guard) + 0.5;
            float z_win = (z_far * (z_eye - z_near)) / ((z_far - z_near) * z_eye);
            float t_sz = p_params.z;
            int row = (face >= 3) ? 1 : 0;
            int col = face - row * 3;
            vec4 face_rect = vec4(
                p_params.x + float(col) * t_sz,
                p_params.y + float(row) * t_sz,
                t_sz,
                t_sz);
            raw_vis = filterShadowRect(face_rect, u, v, z_win, depth_bias, shadow_params.w, 1.0);
        }
    }
    return 1.0 - shadow_params.x * (1.0 - raw_vis);
}
)";

constexpr const char* kFragmentShaderGlslEs300PartC = R"(
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

    vec3 lo = vec3(0.0);
    vec3 ambient_term = vec3(0.0);

    if (u_light_meta.x > 0.5) {
        int count = clamp(int(u_light_meta.y + 0.5), 0, 12);
        for (int i = 0; i < 12; ++i) {
            if (i >= count) break;
            if ((u_light_mask & (1 << i)) == 0) continue;

            vec4 pos_type = u_light_pos_type[i];
            vec4 dir_range = u_light_dir_range[i];
            vec4 rad_atten = u_light_radiance_atten[i];
            vec4 shape = u_light_shape[i];
            vec4 tan_sh = u_light_tangent_shadow[i];
            vec4 sh_params = u_light_shadow_params[i];

            vec3 radiance = rad_atten.xyz;
            if (dot(radiance, radiance) <= 1.0e-8) continue;

            int ltype = int(floor(pos_type.w + 0.5));
            int sh_slot = int(floor(tan_sh.w + 0.5));

            vec3 shade_l = vec3(0.0, 1.0, 0.0);
            vec3 shade_rad = vec3(0.0);
            vec3 area_contrib = vec3(0.0);
            float soft_boost = 1.0;
            bool has_direct = false;
            bool is_area = false;

            if (ltype == 0) {
                shade_l = normalize(-dir_range.xyz);
                shade_rad = radiance;
                has_direct = true;
            } else if (ltype == 1) {
                vec3 to_l = pos_type.xyz - v_world_pos;
                float dist = length(to_l);
                int fmode = int(floor(shape.y + 0.5));
                float att = evalDistanceAttenuation(dist, dir_range.w, rad_atten.w, shape.x, fmode);
                if (att > 1.0e-6) {
                    shade_l = to_l / max(dist, 1.0e-5);
                    shade_rad = radiance * att;
                    has_direct = true;
                }
            } else if (ltype == 2) {
                vec3 to_l = pos_type.xyz - v_world_pos;
                float dist = length(to_l);
                int fmode = int(floor(shape.w + 0.5));
                float att = evalDistanceAttenuation(dist, dir_range.w, rad_atten.w, 0.08, fmode);
                if (att > 1.0e-6) {
                    vec3 l = to_l / max(dist, 1.0e-5);
                    float cos_t = dot(dir_range.xyz, -l);
                    if (cos_t > shape.x) {
                        float t = clamp((cos_t - shape.x) * shape.y, 0.0, 1.0);
                        float sm = t * t * (3.0 - 2.0 * t);
                        float cone = (abs(shape.z - 1.0) <= 1.0e-3) ? sm : pow(sm, max(shape.z, 0.1));
                        shade_l = l;
                        shade_rad = radiance * (att * cone);
                        has_direct = true;
                    }
                }
            } else {
                vec3 n_emit = normalize(dir_range.xyz);
                vec3 tan_u = normalize(tan_sh.xyz);
                vec3 tan_v = normalize(cross(n_emit, tan_u));
                float half_w = shape.x;
                float half_h = shape.y;
                int flags = int(floor(shape.z + 0.5));
                bool is_disk = ((flags & 1) != 0);
                bool two_sided = ((flags & 2) != 0);
                float flux_scale = shape.w;

                vec3 c_delta = v_world_pos - pos_type.xyz;
                float plane_d = dot(c_delta, n_emit);
                if ((two_sided || plane_d > 1.0e-4) && length(c_delta) < (dir_range.w + max(half_w, half_h))) {
                    vec3 diff_irr = vec3(0.0);
                    for (int qs = 0; qs < 4; ++qs) {
                        float su = ((qs == 1 || qs == 3) ? 0.55 : -0.55);
                        float sv = ((qs >= 2) ? 0.55 : -0.55);
                        vec2 offs = is_disk ? (concentricDisk(su, sv) * half_w) : vec2(su * half_w, sv * half_h);
                        vec3 s_pos = pos_type.xyz + tan_u * offs.x + tan_v * offs.y;
                        vec3 to_s = s_pos - v_world_pos;
                        float d_s = length(to_s);
                        if (d_s > 1.0e-5 && d_s < dir_range.w) {
                            vec3 wi = to_s / d_s;
                            float ec_raw = dot(n_emit, -wi);
                            float ec = two_sided ? abs(ec_raw) : max(ec_raw, 0.0);
                            float rc = max(dot(n, wi), 0.0);
                            if (ec > 0.0 && rc > 0.0) {
                                float s_att = evalDistanceAttenuation(d_s, dir_range.w, 2.0, 0.12, 0);
                                diff_irr += radiance * (ec * rc * s_att);
                            }
                        }
                    }
                    diff_irr *= flux_scale * 0.25;

                    vec3 r_vec = reflect(-v, n);
                    float r_dot_p = dot(r_vec, n_emit);
                    vec3 p_hit = (abs(r_dot_p) > 1.0e-4)
                        ? (v_world_pos + r_vec * max(-plane_d / r_dot_p, 0.0))
                        : (v_world_pos - n_emit * plane_d);
                    vec3 rel = p_hit - pos_type.xyz;
                    float du = dot(rel, tan_u);
                    float dv = dot(rel, tan_v);
                    if (is_disk) {
                        float rad_len = length(vec2(du, dv));
                        if (rad_len > half_w && rad_len > 1.0e-6) {
                            float sc = half_w / rad_len;
                            du *= sc;
                            dv *= sc;
                        }
                    } else {
                        du = clamp(du, -half_w, half_w);
                        dv = clamp(dv, -half_h, half_h);
                    }
                    vec3 rep_p = pos_type.xyz + tan_u * du + tan_v * dv;
                    vec3 to_rep = rep_p - v_world_pos;
                    float rep_d = max(length(to_rep), 1.0e-4);
                    vec3 l_spec = to_rep / rep_d;
                    float es_raw = dot(n_emit, -l_spec);
                    float es = two_sided ? abs(es_raw) : max(es_raw, 0.0);

                    vec3 f_view = fresnelSchlick(ndotv, f0);
                    vec3 kd = (vec3(1.0) - f_view) * (1.0 - metallic);
                    vec3 area_diff = kd * albedo * diff_irr * (1.0 / kPi);

                    vec3 area_spec = vec3(0.0);
                    float ndotls = max(dot(n, l_spec), 0.0);
                    if (ndotls > 0.0 && es > 0.0) {
                        float eff_r = 0.5 * (half_w + half_h);
                        float alpha = roughness * roughness;
                        float alpha_p = clamp(alpha + eff_r / (2.0 * rep_d), alpha, 1.0);
                        float sp_rough = sqrt(alpha_p);
                        float e_norm = (alpha * alpha) / max(alpha_p * alpha_p, 1.0e-5);
                        float sp_att = evalDistanceAttenuation(rep_d, dir_range.w, 2.0, 0.12, 0);
                        vec3 h = normalize(v + l_spec);
                        float ndf = distributionGgx(max(dot(n, h), 0.0), sp_rough) * e_norm;
                        float geom = geometrySchlickGgx(ndotv, k_direct) * geometrySchlickGgx(ndotls, k_direct);
                        vec3 f_sp = fresnelSchlick(max(dot(h, v), 0.0), f0);
                        area_spec = f_sp * radiance * (((ndf * geom) / max(4.0 * ndotv * ndotls, 1.0e-4)) * ndotls * es * sp_att * flux_scale);
                    }
                    shade_l = normalize(pos_type.xyz - v_world_pos);
                    soft_boost = 1.0 + 1.6 * max(half_w, half_h);
                    area_contrib = area_diff + area_spec;
                    is_area = true;
                    has_direct = true;
                }
            }

            if (has_direct) {
                float vis = evalLightShadow(sh_slot, sh_params, pos_type.xyz, v_world_pos, n, shade_l, soft_boost);
                if (is_area) {
                    lo += area_contrib * vis;
                } else {
                    lo += evalCookTorranceLobe(n, v, shade_l, shade_rad * vis, albedo, f0, metallic, roughness, k_direct);
                    if (ltype == 0 && i == 0 && u_light_meta.z > 0.5) {
                        vec3 fill_l = normalize(vec3(-0.55, 0.35, -0.65));
                        lo += evalCookTorranceLobe(n, v, fill_l, radiance * 0.22, albedo, f0, metallic, roughness, k_direct);
                    }
                }
            }
        }

        if (u_env_params.x > 0.5 && u_env_params.z > 1.0e-5) {
            float env_i = u_env_params.z;
            if (u_env_params.y > 0.5) {
                float cr = u_env_rot_scales.x;
                float sr = u_env_rot_scales.y;
                vec3 n_rot = vec3(n.x * cr + n.z * sr, n.y, -n.x * sr + n.z * cr);
                vec3 r_w = reflect(-v, n);
                vec3 r_rot = vec3(r_w.x * cr + r_w.z * sr, r_w.y, -r_w.x * sr + r_w.z * cr);

                vec3 f_ibl = fresnelSchlickRoughness(ndotv, f0, roughness);
                vec3 kd_ibl = (vec3(1.0) - f_ibl) * (1.0 - metallic);
                vec3 sh_irr = evalSh9Irradiance(n_rot) * (env_i * u_env_rot_scales.z);
                vec3 diff_ibl = kd_ibl * albedo * sh_irr;

                float lod = roughness * u_env_params.w;
                vec3 prefiltered = textureLod(u_ibl_cubemap, r_rot, lod).rgb * (env_i * u_env_rot_scales.w);
                vec2 brdf = envBrdfApprox(roughness, ndotv);
                vec3 spec_ibl = prefiltered * (f0 * brdf.x + vec3(brdf.y));
                ambient_term = (diff_ibl + spec_ibl) * ao;
            } else {
                float sky = n.y * 0.5 + 0.5;
                vec3 env_irr = vec3(0.18, 0.20, 0.24) * ((0.65 + 0.70 * sky) * env_i);
                vec3 f_env = fresnelSchlick(ndotv, f0);
                vec3 kd_env = (vec3(1.0) - f_env) * (1.0 - metallic);
                vec3 env_spec = f_env * env_irr * ((1.0 - 0.75 * roughness) * u_env_rot_scales.w);
                ambient_term = (kd_env * albedo * env_irr * u_env_rot_scales.z + env_spec) * ao;
            }
        }
        if (dot(u_overflow_irradiance, u_overflow_irradiance) > 1.0e-8) {
            vec3 kd_ov = (vec3(1.0) - f0) * (1.0 - metallic);
            ambient_term += kd_ov * albedo * u_overflow_irradiance * ao;
        }
    } else {
        vec3 key_l = normalize(u_light_dir);
        vec3 fill_l = normalize(vec3(-0.55, 0.35, -0.65));
        lo = evalCookTorranceLobe(n, v, key_l, u_light_radiance, albedo, f0, metallic, roughness, k_direct)
           + evalCookTorranceLobe(n, v, fill_l, u_light_radiance * 0.22, albedo, f0, metallic, roughness, k_direct);
        for (int li = 0; li < 3; ++li) {
            vec3 ex_rad = u_extra_light_radiances[li];
            vec3 ex_dir = u_extra_light_dirs[li];
            if (dot(ex_rad, ex_rad) > 1.0e-8 && dot(ex_dir, ex_dir) > 1.0e-8) {
                lo += evalCookTorranceLobe(n, v, normalize(ex_dir), ex_rad, albedo, f0, metallic, roughness, k_direct);
            }
        }
        float sky = n.y * 0.5 + 0.5;
        vec3 env_irradiance = u_ambient * (0.65 + 0.70 * sky);
        vec3 f_env = fresnelSchlick(ndotv, f0);
        vec3 kd_env = (vec3(1.0) - f_env) * (1.0 - metallic);
        vec3 env_spec = f_env * env_irradiance * (1.0 - 0.75 * roughness);
        ambient_term = (kd_env * albedo * env_irradiance + env_spec) * ao;
    }

    vec3 hdr = ambient_term + lo + u_emissive_radiance;
    float rim = 1.0 - ndotv;
    hdr += u_selection_tint * (0.30 + 0.80 * rim * rim);

    vec3 mapped = hdr / (vec3(1.0) + max(hdr, vec3(0.0)));
    vec3 srgb = pow(clamp(mapped, 0.0, 1.0), vec3(1.0 / 2.2));
    o_frag_color = vec4(srgb, opacity);
}
)";

std::string buildFragmentShaderGlslEs300() {
    std::string src;
    src.reserve(18000);
    src += kFragmentShaderGlslEs300PartA;
    src += kFragmentShaderGlslEs300PartB;
    src += kFragmentShaderGlslEs300PartC;
    return src;
}

constexpr const char* kGridVertexShaderGlslEs300 = R"(#version 300 es
precision highp float;
precision highp int;

layout(location = 0) in vec3 a_position;
out vec2 v_ndc;

void main() {
    v_ndc = a_position.xy;
    gl_Position = vec4(a_position.xy, 0.0, 1.0);
}
)";

constexpr const char* kGridFragmentShaderGlslEs300 = R"(#version 300 es
precision highp float;
precision highp int;

in vec2 v_ndc;

uniform vec3 u_grid_cam_pos;
uniform vec3 u_grid_cam_right;
uniform vec3 u_grid_cam_up;
uniform vec3 u_grid_cam_fwd;
uniform vec3 u_grid_frustum;
uniform vec3 u_grid_depth;
uniform vec3 u_grid_steps;
uniform vec3 u_grid_fade;
uniform vec3 u_grid_phase_x;
uniform vec3 u_grid_phase_z;
uniform vec3 u_grid_axis_px;
uniform vec3 u_grid_ndc_step;

out vec4 o_frag_color;

const float kGridDepthBias = 0.000025;

float evalCascadeLine(float delta_c, float phase_c, float step_sz, float footprint, float half_w_px) {
    if (step_sz <= 1.0e-6) {
        return 0.0;
    }
    float nyq = 1.0 - smoothstep(0.22 * step_sz, 0.48 * step_sz, footprint);
    if (nyq <= 1.0e-4) {
        return 0.0;
    }
    float u = (delta_c + phase_c) / step_sz;
    float dist_world = abs(fract(u + 0.5) - 0.5) * step_sz;
    float dist_px = dist_world / max(footprint, 1.0e-6);
    float cov = 1.0 - smoothstep(half_w_px - 0.55, half_w_px + 0.55, dist_px);
    return cov * nyq;
}

void main() {
    bool is_ortho = (u_grid_frustum.z > 0.5);
    bool zero_to_one = (u_grid_depth.z > 0.5);
    vec3 R = u_grid_cam_right;
    vec3 U = u_grid_cam_up;
    vec3 F = u_grid_cam_fwd;

    vec3 o_rel = vec3(0.0);
    vec3 ray_d = F;
    if (is_ortho) {
        o_rel = R * (v_ndc.x * u_grid_frustum.x) + U * (v_ndc.y * u_grid_frustum.y);
    } else {
        ray_d = F + R * (v_ndc.x * u_grid_frustum.x) + U * (v_ndc.y * u_grid_frustum.y);
    }

    float origin_y = u_grid_cam_pos.y + o_rel.y;

    if (is_ortho && abs(ray_d.y) <= 1.0e-4) {
        float px_world_y = max(
            abs(u_grid_ndc_step.y * u_grid_frustum.y * U.y) + abs(u_grid_ndc_step.x * u_grid_frustum.x * R.y),
            1.0e-5);
        float line_px = abs(origin_y) / px_world_y;
        if (line_px > 1.85) {
            gl_FragDepth = 1.0;
            discard;
        }
        float t_mid = max(-dot(u_grid_cam_pos, F), 0.25);
        float ndc_z_h = clamp(-u_grid_depth.x * t_mid + u_grid_depth.y, -0.999, 0.999);
        float win_d_h = zero_to_one ? clamp(ndc_z_h, 0.0, 0.999) : (ndc_z_h * 0.5 + 0.5);
        float cov_h = 1.0 - smoothstep(0.65, 1.80, line_px);
        float a_h = clamp(cov_h * 0.84 * u_grid_fade.z, 0.0, 1.0);
        if (a_h <= 0.004) {
            gl_FragDepth = 1.0;
            discard;
        }
        float z_w = abs(F.z) / max(abs(F.x) + abs(F.z), 1.0e-4);
        vec3 h_col = mix(vec3(0.26, 0.58, 0.96), vec3(0.92, 0.30, 0.33), z_w);
        gl_FragDepth = clamp(win_d_h + kGridDepthBias, 0.0, 1.0);
        o_frag_color = vec4(h_col, a_h);
        return;
    }

    if (abs(ray_d.y) <= 1.0e-6) {
        gl_FragDepth = 1.0;
        discard;
    }

    float t = -origin_y / ray_d.y;
    if (t <= 0.0) {
        gl_FragDepth = 1.0;
        discard;
    }

    float ndc_z = is_ortho ? (-u_grid_depth.x * t + u_grid_depth.y) : (-u_grid_depth.x + u_grid_depth.y / t);
    float min_ndc_z = zero_to_one ? 0.0 : -1.0;
    if (ndc_z < min_ndc_z || ndc_z > 1.0) {
        gl_FragDepth = 1.0;
        discard;
    }

    float delta_x = o_rel.x + t * ray_d.x;
    float delta_z = o_rel.z + t * ray_d.z;
    float radial_dist = length(vec2(delta_x, delta_z));
    if (radial_dist >= u_grid_fade.y) {
        gl_FragDepth = 1.0;
        discard;
    }

    float radial_fade = 1.0 - smoothstep(u_grid_fade.y * 0.36, u_grid_fade.y, radial_dist);
    float angle_fade = 1.0;
    if (!is_ortho) {
        float abs_sin_pitch = abs(ray_d.y) / max(length(ray_d), 1.0e-6);
        angle_fade = smoothstep(0.012, 0.13, abs_sin_pitch);
    }
    float global_fade = radial_fade * angle_fade * u_grid_fade.z;
    if (global_fade <= 0.004) {
        gl_FragDepth = 1.0;
        discard;
    }

    float sx = is_ortho ? (u_grid_ndc_step.x * u_grid_frustum.x) : (u_grid_ndc_step.x * t * u_grid_frustum.x);
    float sy = is_ortho ? (u_grid_ndc_step.y * u_grid_frustum.y) : (u_grid_ndc_step.y * t * u_grid_frustum.y);
    float inv_dy = 1.0 / ray_d.y;

    float jx_x = sx * (R.x - R.y * inv_dy * ray_d.x);
    float jx_z = sx * (R.z - R.y * inv_dy * ray_d.z);
    float jy_x = sy * (U.x - U.y * inv_dy * ray_d.x);
    float jy_z = sy * (U.z - U.y * inv_dy * ray_d.z);

    float fw_x = length(vec2(jx_x, jy_x));
    float fw_z = length(vec2(jx_z, jy_z));

    float cov_fine = max(
        evalCascadeLine(delta_x, u_grid_phase_x.x, u_grid_steps.x, fw_x, 0.65),
        evalCascadeLine(delta_z, u_grid_phase_z.x, u_grid_steps.x, fw_z, 0.65));
    float cov_minor = max(
        evalCascadeLine(delta_x, u_grid_phase_x.y, u_grid_steps.y, fw_x, 0.78),
        evalCascadeLine(delta_z, u_grid_phase_z.y, u_grid_steps.y, fw_z, 0.78));
    float cov_major = max(
        evalCascadeLine(delta_x, u_grid_phase_x.z, u_grid_steps.z, fw_x, 1.06),
        evalCascadeLine(delta_z, u_grid_phase_z.z, u_grid_steps.z, fw_z, 1.06));

    float blend = u_grid_fade.x;
    float a_fine = cov_fine * 0.28 * (1.0 - blend);
    float a_minor = cov_minor * mix(0.56, 0.28, blend);
    float a_major = cov_major * 0.56;

    float alpha = max(a_fine, max(a_minor, a_major));
    float major_mix = (alpha > 1.0e-5) ? clamp(a_major / alpha, 0.0, 1.0) : 0.0;
    vec3 color = mix(vec3(0.35, 0.40, 0.48), vec3(0.50, 0.56, 0.66), major_mix);

    float world_x = delta_x + u_grid_axis_px.x;
    float world_z = delta_z + u_grid_axis_px.y;
    float px_dist_x_axis = abs(world_z) / max(fw_z, 1.0e-6);
    float px_dist_z_axis = abs(world_x) / max(fw_x, 1.0e-6);

    float x_axis_nyq = 1.0 - smoothstep(0.30 * u_grid_steps.z, 0.90 * u_grid_steps.z, fw_z);
    float z_axis_nyq = 1.0 - smoothstep(0.30 * u_grid_steps.z, 0.90 * u_grid_steps.z, fw_x);
    float cov_x_axis = (1.0 - smoothstep(0.75, 1.85, px_dist_x_axis)) * x_axis_nyq;
    float cov_z_axis = (1.0 - smoothstep(0.75, 1.85, px_dist_z_axis)) * z_axis_nyq;

    if (cov_x_axis > 1.0e-3 || cov_z_axis > 1.0e-3) {
        float axis_cov = max(cov_x_axis, cov_z_axis);
        float w_z = cov_z_axis / max(cov_x_axis + cov_z_axis, 1.0e-5);
        vec3 axis_col = mix(vec3(0.92, 0.30, 0.33), vec3(0.26, 0.58, 0.96), w_z);
        color = mix(color, axis_col, clamp(axis_cov, 0.0, 1.0));
        alpha = max(alpha, axis_cov * 0.88);
    }

    if (px_dist_x_axis < 4.2 && px_dist_z_axis < 4.2) {
        float origin_r = length(vec2(px_dist_x_axis, px_dist_z_axis));
        float origin_cov = (1.0 - smoothstep(1.65, 3.25, origin_r)) * min(x_axis_nyq, z_axis_nyq);
        if (origin_cov > 1.0e-3) {
            color = mix(color, vec3(0.96, 0.98, 1.00), origin_cov);
            alpha = max(alpha, origin_cov * 0.95);
        }
    }

    float final_alpha = clamp(alpha * global_fade, 0.0, 1.0);
    if (final_alpha <= 0.004) {
        gl_FragDepth = 1.0;
        discard;
    }

    float win_depth = zero_to_one ? ndc_z : (ndc_z * 0.5 + 0.5);
    gl_FragDepth = clamp(win_depth + kGridDepthBias, 0.0, 1.0);
    o_frag_color = vec4(color, final_alpha);
}
)";

} // namespace

struct WebGL2RenderDevice::GlEs3DispatchTable {
    const GLubyte* (FE_GLES_APIENTRY* GetString)(GLenum) = nullptr;
    void (FE_GLES_APIENTRY* GetIntegerv)(GLenum, GLint*) = nullptr;
    GLenum (FE_GLES_APIENTRY* GetError)() = nullptr;
    void (FE_GLES_APIENTRY* Viewport)(GLint, GLint, GLsizei, GLsizei) = nullptr;
    void (FE_GLES_APIENTRY* Scissor)(GLint, GLint, GLsizei, GLsizei) = nullptr;
    void (FE_GLES_APIENTRY* ClearColor)(GLfloat, GLfloat, GLfloat, GLfloat) = nullptr;
    void (FE_GLES_APIENTRY* ClearDepthf)(GLfloat) = nullptr;
    void (FE_GLES_APIENTRY* Clear)(GLbitfield) = nullptr;
    void (FE_GLES_APIENTRY* Enable)(GLenum) = nullptr;
    void (FE_GLES_APIENTRY* Disable)(GLenum) = nullptr;
    void (FE_GLES_APIENTRY* BlendFunc)(GLenum, GLenum) = nullptr;
    void (FE_GLES_APIENTRY* DepthMask)(GLboolean) = nullptr;
    void (FE_GLES_APIENTRY* ColorMask)(GLboolean, GLboolean, GLboolean, GLboolean) = nullptr;
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
    void (FE_GLES_APIENTRY* Uniform4fv)(GLint, GLsizei, const GLfloat*) = nullptr;
    void (FE_GLES_APIENTRY* Uniform1i)(GLint, GLint) = nullptr;

    void (FE_GLES_APIENTRY* ActiveTexture)(GLenum) = nullptr;
    void (FE_GLES_APIENTRY* GenTextures)(GLsizei, GLuint*) = nullptr;
    void (FE_GLES_APIENTRY* BindTexture)(GLenum, GLuint) = nullptr;
    void (FE_GLES_APIENTRY* DeleteTextures)(GLsizei, const GLuint*) = nullptr;
    void (FE_GLES_APIENTRY* TexImage2D)(
        GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*) = nullptr;
    void (FE_GLES_APIENTRY* TexParameteri)(GLenum, GLenum, GLint) = nullptr;

    void (FE_GLES_APIENTRY* GenFramebuffers)(GLsizei, GLuint*) = nullptr;
    void (FE_GLES_APIENTRY* BindFramebuffer)(GLenum, GLuint) = nullptr;
    void (FE_GLES_APIENTRY* DeleteFramebuffers)(GLsizei, const GLuint*) = nullptr;
    void (FE_GLES_APIENTRY* FramebufferTexture2D)(GLenum, GLenum, GLenum, GLuint, GLint) = nullptr;
    GLenum (FE_GLES_APIENTRY* CheckFramebufferStatus)(GLenum) = nullptr;
    void (FE_GLES_APIENTRY* DrawBuffers)(GLsizei, const GLenum*) = nullptr;
    void (FE_GLES_APIENTRY* ReadBuffer)(GLenum) = nullptr;
};

WebGL2RenderDevice::WebGL2RenderDevice(platform::GlProcLoaderFn proc_loader)
    : proc_loader_(proc_loader),
      gl_(std::make_unique<GlEs3DispatchTable>()) {
    capabilities_.backend = BackendType::WebGL2;
    capabilities_.backend_name = "WebGL 2.0 (OpenGL ES 3.0)";
    capabilities_.device_name = "Uninitialized WebGL 2.0 Device";
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
            "Initial WebGL2 framebuffer dimensions must be non-zero");
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
            "Failed to resolve required WebGL 2.0 / OpenGL ES 3.0 entry point",
            missing_symbol);
    }

    loadOptSym(gl_->BufferSubData, "glBufferSubData");
    loadOptSym(gl_->Disable, "glDisable");
    loadOptSym(gl_->BlendFunc, "glBlendFunc");
    loadOptSym(gl_->DepthMask, "glDepthMask");
    loadOptSym(gl_->ColorMask, "glColorMask");
    loadOptSym(gl_->Scissor, "glScissor");
    loadOptSym(gl_->Uniform4fv, "glUniform4fv");
    loadOptSym(gl_->Uniform1i, "glUniform1i");
    loadOptSym(gl_->ActiveTexture, "glActiveTexture");
    loadOptSym(gl_->GenTextures, "glGenTextures");
    loadOptSym(gl_->BindTexture, "glBindTexture");
    loadOptSym(gl_->DeleteTextures, "glDeleteTextures");
    loadOptSym(gl_->TexImage2D, "glTexImage2D");
    loadOptSym(gl_->TexParameteri, "glTexParameteri");
    loadOptSym(gl_->GenFramebuffers, "glGenFramebuffers");
    loadOptSym(gl_->BindFramebuffer, "glBindFramebuffer");
    loadOptSym(gl_->DeleteFramebuffers, "glDeleteFramebuffers");
    loadOptSym(gl_->FramebufferTexture2D, "glFramebufferTexture2D");
    loadOptSym(gl_->CheckFramebufferStatus, "glCheckFramebufferStatus");
    loadOptSym(gl_->DrawBuffers, "glDrawBuffers");
    loadOptSym(gl_->ReadBuffer, "glReadBuffer");

    if (const GLubyte* renderer_str = gl_->GetString(kGlRenderer); renderer_str != nullptr) {
        capabilities_.device_name = reinterpret_cast<const char*>(renderer_str);
    } else {
        capabilities_.device_name = "WebGL 2.0 Device";
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
    in_shadow_pass_ = false;
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
    const std::string frag_shader_src = buildFragmentShaderGlslEs300();
    auto fs = compileStage(kGlFragmentShader, frag_shader_src.c_str(), "fragment");
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

    auto resolveArrayLoc = [&](const char* with_idx, const char* bare) -> GLint {
        GLint l = gl_->GetUniformLocation(program, with_idx);
        if (l < 0) {
            l = gl_->GetUniformLocation(program, bare);
        }
        return l;
    };

    shader_program_ = program;
    loc_model_ = gl_->GetUniformLocation(shader_program_, "u_model");
    loc_normal_mat_ = gl_->GetUniformLocation(shader_program_, "u_normal_matrix");
    loc_view_proj_ = gl_->GetUniformLocation(shader_program_, "u_view_proj");
    loc_camera_pos_ = gl_->GetUniformLocation(shader_program_, "u_camera_pos");
    loc_light_dir_ = gl_->GetUniformLocation(shader_program_, "u_light_dir");
    loc_light_radiance_ = gl_->GetUniformLocation(shader_program_, "u_light_radiance");
    loc_extra_light_dirs_ = resolveArrayLoc("u_extra_light_dirs[0]", "u_extra_light_dirs");
    loc_extra_light_radiances_ = resolveArrayLoc("u_extra_light_radiances[0]", "u_extra_light_radiances");
    loc_ambient_ = gl_->GetUniformLocation(shader_program_, "u_ambient");
    loc_selection_tint_ = gl_->GetUniformLocation(shader_program_, "u_selection_tint");
    loc_base_color_ = gl_->GetUniformLocation(shader_program_, "u_base_color");
    loc_pbr_params_ = gl_->GetUniformLocation(shader_program_, "u_pbr_params");
    loc_emissive_radiance_ = gl_->GetUniformLocation(shader_program_, "u_emissive_radiance");
    loc_surface_flags_ = gl_->GetUniformLocation(shader_program_, "u_surface_flags");

    loc_light_meta_ = gl_->GetUniformLocation(shader_program_, "u_light_meta");
    loc_light_mask_ = gl_->GetUniformLocation(shader_program_, "u_light_mask");
    loc_light_pos_type_ = resolveArrayLoc("u_light_pos_type[0]", "u_light_pos_type");
    loc_light_dir_range_ = resolveArrayLoc("u_light_dir_range[0]", "u_light_dir_range");
    loc_light_radiance_atten_ = resolveArrayLoc("u_light_radiance_atten[0]", "u_light_radiance_atten");
    loc_light_shape_ = resolveArrayLoc("u_light_shape[0]", "u_light_shape");
    loc_light_tangent_shadow_ = resolveArrayLoc("u_light_tangent_shadow[0]", "u_light_tangent_shadow");
    loc_light_shadow_params_ = resolveArrayLoc("u_light_shadow_params[0]", "u_light_shadow_params");
    loc_shadow_2d_vp_ = resolveArrayLoc("u_shadow_2d_vp[0]", "u_shadow_2d_vp");
    loc_shadow_2d_rect_ = resolveArrayLoc("u_shadow_2d_rect[0]", "u_shadow_2d_rect");
    loc_point_shadow_params_ = resolveArrayLoc("u_point_shadow_params[0]", "u_point_shadow_params");
    loc_point_shadow_far_ = gl_->GetUniformLocation(shader_program_, "u_point_shadow_far");
    loc_env_params_ = gl_->GetUniformLocation(shader_program_, "u_env_params");
    loc_env_rot_scales_ = gl_->GetUniformLocation(shader_program_, "u_env_rot_scales");
    loc_overflow_irradiance_ = gl_->GetUniformLocation(shader_program_, "u_overflow_irradiance");
    loc_ibl_sh_ = resolveArrayLoc("u_ibl_sh[0]", "u_ibl_sh");
    loc_shadow_atlas_ = gl_->GetUniformLocation(shader_program_, "u_shadow_atlas");
    loc_ibl_cubemap_ = gl_->GetUniformLocation(shader_program_, "u_ibl_cubemap");

    if (gl_->Uniform1i != nullptr) {
        gl_->UseProgram(shader_program_);
        if (loc_shadow_atlas_ >= 0) gl_->Uniform1i(loc_shadow_atlas_, 0);
        if (loc_ibl_cubemap_ >= 0) gl_->Uniform1i(loc_ibl_cubemap_, 1);
        gl_->UseProgram(0);
    }

    return core::Status::ok();
}

core::Status WebGL2RenderDevice::ensureShadowProgram() {
    if (shadow_program_ != 0) {
        return core::Status::ok();
    }
    auto compileStage = [&](GLenum stage, const char* source, const char* label) -> core::Result<GLuint> {
        const GLuint sh = gl_->CreateShader(stage);
        if (sh == 0) {
            return core::Result<GLuint>::fail(core::ErrorCode::ResourceCreationFailed, "glCreateShader failed", label);
        }
        gl_->ShaderSource(sh, 1, &source, nullptr);
        gl_->CompileShader(sh);
        GLint compiled = 0;
        gl_->GetShaderiv(sh, kGlCompileStatus, &compiled);
        if (compiled == 0) {
            GLint log_len = 0;
            gl_->GetShaderiv(sh, kGlInfoLogLength, &log_len);
            std::string log(static_cast<std::size_t>(log_len > 1 ? log_len : 1), '\0');
            if (log_len > 1) gl_->GetShaderInfoLog(sh, log_len, nullptr, log.data());
            gl_->DeleteShader(sh);
            return core::Result<GLuint>::fail(core::ErrorCode::ShaderCompileFailed, log, label);
        }
        return core::Result<GLuint>::ok(sh);
    };

    auto vs = compileStage(kGlVertexShader, kShadowVertexShaderGlslEs300, "shadow_vs");
    if (!vs) return vs.error();
    auto fs = compileStage(kGlFragmentShader, kShadowFragmentShaderGlslEs300, "shadow_fs");
    if (!fs) {
        gl_->DeleteShader(vs.value());
        return fs.error();
    }

    const GLuint prog = gl_->CreateProgram();
    if (prog == 0) {
        gl_->DeleteShader(vs.value());
        gl_->DeleteShader(fs.value());
        return core::Status::fail(core::ErrorCode::ResourceCreationFailed, "glCreateProgram failed for shadow pass");
    }
    gl_->AttachShader(prog, vs.value());
    gl_->AttachShader(prog, fs.value());
    gl_->LinkProgram(prog);
    gl_->DeleteShader(vs.value());
    gl_->DeleteShader(fs.value());

    GLint linked = 0;
    gl_->GetProgramiv(prog, kGlLinkStatus, &linked);
    if (linked == 0) {
        gl_->DeleteProgram(prog);
        return core::Status::fail(core::ErrorCode::ShaderCompileFailed, "WebGL2 shadow program link failed");
    }

    shadow_program_ = prog;
    loc_shadow_mvp_ = gl_->GetUniformLocation(shadow_program_, "u_light_mvp");
    return core::Status::ok();
}

void WebGL2RenderDevice::ensureFallbackTextures() noexcept {
    if (gl_->GenTextures == nullptr || gl_->BindTexture == nullptr ||
        gl_->TexImage2D == nullptr || gl_->TexParameteri == nullptr) {
        return;
    }
    if (fallback_tex_2d_ == 0) {
        gl_->GenTextures(1, &fallback_tex_2d_);
        if (fallback_tex_2d_ != 0) {
            const std::uint8_t white[4] = {255u, 255u, 255u, 255u};
            gl_->BindTexture(kGlTexture2D, fallback_tex_2d_);
            gl_->TexImage2D(kGlTexture2D, 0, static_cast<GLint>(kGlRgba8), 1, 1, 0, kGlRgba, kGlUnsignedByte, white);
            gl_->TexParameteri(kGlTexture2D, kGlTextureMinFilter, static_cast<GLint>(kGlNearest));
            gl_->TexParameteri(kGlTexture2D, kGlTextureMagFilter, static_cast<GLint>(kGlNearest));
            gl_->TexParameteri(kGlTexture2D, kGlTextureWrapS, static_cast<GLint>(kGlClampToEdge));
            gl_->TexParameteri(kGlTexture2D, kGlTextureWrapT, static_cast<GLint>(kGlClampToEdge));
            gl_->BindTexture(kGlTexture2D, 0);
        }
    }
    if (fallback_cubemap_ == 0) {
        gl_->GenTextures(1, &fallback_cubemap_);
        if (fallback_cubemap_ != 0) {
            const std::uint8_t black[4] = {0u, 0u, 0u, 255u};
            gl_->BindTexture(kGlTextureCubeMap, fallback_cubemap_);
            for (GLuint f = 0; f < 6u; ++f) {
                gl_->TexImage2D(
                    kGlTextureCubeMapPositiveX + f, 0, static_cast<GLint>(kGlRgba8),
                    1, 1, 0, kGlRgba, kGlUnsignedByte, black);
            }
            gl_->TexParameteri(kGlTextureCubeMap, kGlTextureMinFilter, static_cast<GLint>(kGlLinear));
            gl_->TexParameteri(kGlTextureCubeMap, kGlTextureMagFilter, static_cast<GLint>(kGlLinear));
            gl_->TexParameteri(kGlTextureCubeMap, kGlTextureWrapS, static_cast<GLint>(kGlClampToEdge));
            gl_->TexParameteri(kGlTextureCubeMap, kGlTextureWrapT, static_cast<GLint>(kGlClampToEdge));
            gl_->TexParameteri(kGlTextureCubeMap, kGlTextureWrapR, static_cast<GLint>(kGlClampToEdge));
            gl_->BindTexture(kGlTextureCubeMap, 0);
        }
    }
}

core::Status WebGL2RenderDevice::syncEnvironmentMap(
    const scene::HdriEnvironmentAsset* asset,
    std::uint64_t revision) {
    if (!initialized_) {
        return core::Status::fail(core::ErrorCode::InvalidState, "Device not initialized");
    }
    if (asset == nullptr) {
        if (ibl_cubemap_tex_ != 0 && gl_->DeleteTextures != nullptr) {
            gl_->DeleteTextures(1, &ibl_cubemap_tex_);
        }
        ibl_cubemap_tex_ = 0;
        environment_revision_ = 0;
        environment_gpu_bytes_ = 0;
        return core::Status::ok();
    }
    if (environment_revision_ == revision && ibl_cubemap_tex_ != 0) {
        return core::Status::ok();
    }
    if (gl_->GenTextures == nullptr || gl_->BindTexture == nullptr ||
        gl_->TexImage2D == nullptr || gl_->TexParameteri == nullptr) {
        environment_revision_ = revision;
        environment_gpu_bytes_ = asset->prefiltered_cubemap.gpuByteSize();
        return core::Status::ok();
    }

    if (ibl_cubemap_tex_ == 0) {
        gl_->GenTextures(1, &ibl_cubemap_tex_);
        if (ibl_cubemap_tex_ == 0) {
            return core::Status::fail(core::ErrorCode::ResourceCreationFailed, "Failed to allocate WebGL2 IBL cubemap");
        }
    }

    gl_->BindTexture(kGlTextureCubeMap, ibl_cubemap_tex_);
    const auto& cube = asset->prefiltered_cubemap;
    for (std::size_t mip = 0; mip < scene::PrefilteredEnvCubemap::kMipCount; ++mip) {
        const auto& level = cube.mips[mip];
        const auto sz = static_cast<GLsizei>(level.size);
        const std::size_t face_u16_stride = static_cast<std::size_t>(level.size) * level.size * 4u;
        for (GLuint face = 0; face < 6u; ++face) {
            const std::uint16_t* face_ptr = level.rgba16f_half.data() + face * face_u16_stride;
            gl_->TexImage2D(
                kGlTextureCubeMapPositiveX + face,
                static_cast<GLint>(mip),
                static_cast<GLint>(kGlRgba16f),
                sz,
                sz,
                0,
                kGlRgba,
                kGlHalfFloat,
                face_ptr);
        }
    }
    gl_->TexParameteri(kGlTextureCubeMap, kGlTextureBaseLevel, 0);
    gl_->TexParameteri(
        kGlTextureCubeMap,
        kGlTextureMaxLevel,
        static_cast<GLint>(scene::PrefilteredEnvCubemap::kMipCount - 1u));
    gl_->TexParameteri(kGlTextureCubeMap, kGlTextureMinFilter, static_cast<GLint>(kGlLinearMipmapLinear));
    gl_->TexParameteri(kGlTextureCubeMap, kGlTextureMagFilter, static_cast<GLint>(kGlLinear));
    gl_->TexParameteri(kGlTextureCubeMap, kGlTextureWrapS, static_cast<GLint>(kGlClampToEdge));
    gl_->TexParameteri(kGlTextureCubeMap, kGlTextureWrapT, static_cast<GLint>(kGlClampToEdge));
    gl_->TexParameteri(kGlTextureCubeMap, kGlTextureWrapR, static_cast<GLint>(kGlClampToEdge));
    gl_->BindTexture(kGlTextureCubeMap, 0);

    environment_revision_ = revision;
    environment_gpu_bytes_ = cube.gpuByteSize();
    return core::Status::ok();
}

core::Status WebGL2RenderDevice::ensureShadowAtlas(std::uint32_t atlas_size) {
    if (!initialized_) {
        return core::Status::fail(core::ErrorCode::InvalidState, "Device not initialized");
    }
    if (atlas_size == 0u) {
        if (shadow_fbo_ != 0 && gl_->DeleteFramebuffers != nullptr) {
            gl_->DeleteFramebuffers(1, &shadow_fbo_);
        }
        if (shadow_depth_tex_ != 0 && gl_->DeleteTextures != nullptr) {
            gl_->DeleteTextures(1, &shadow_depth_tex_);
        }
        shadow_fbo_ = 0;
        shadow_depth_tex_ = 0;
        shadow_atlas_size_ = 0;
        shadow_gpu_bytes_ = 0;
        return core::Status::ok();
    }
    if (shadow_atlas_size_ == atlas_size && (shadow_fbo_ != 0 || gl_->GenFramebuffers == nullptr)) {
        return core::Status::ok();
    }

    if (gl_->GenFramebuffers == nullptr || gl_->BindFramebuffer == nullptr ||
        gl_->GenTextures == nullptr || gl_->TexImage2D == nullptr ||
        gl_->FramebufferTexture2D == nullptr) {
        shadow_atlas_size_ = atlas_size;
        shadow_gpu_bytes_ = static_cast<std::size_t>(atlas_size) * atlas_size * 4u;
        return core::Status::ok();
    }

    if (auto st = ensureShadowProgram(); !st) {
        return st;
    }

    if (shadow_fbo_ == 0) {
        gl_->GenFramebuffers(1, &shadow_fbo_);
    }
    if (shadow_depth_tex_ == 0) {
        gl_->GenTextures(1, &shadow_depth_tex_);
    }
    if (shadow_fbo_ == 0 || shadow_depth_tex_ == 0) {
        return core::Status::fail(core::ErrorCode::ResourceCreationFailed, "Failed to allocate WebGL2 shadow FBO");
    }

    gl_->BindTexture(kGlTexture2D, shadow_depth_tex_);
    gl_->TexImage2D(
        kGlTexture2D,
        0,
        static_cast<GLint>(kGlDepthComponent24),
        static_cast<GLsizei>(atlas_size),
        static_cast<GLsizei>(atlas_size),
        0,
        kGlDepthComponent,
        kGlUnsignedInt,
        nullptr);
    gl_->TexParameteri(kGlTexture2D, kGlTextureMinFilter, static_cast<GLint>(kGlNearest));
    gl_->TexParameteri(kGlTexture2D, kGlTextureMagFilter, static_cast<GLint>(kGlNearest));
    gl_->TexParameteri(kGlTexture2D, kGlTextureWrapS, static_cast<GLint>(kGlClampToEdge));
    gl_->TexParameteri(kGlTexture2D, kGlTextureWrapT, static_cast<GLint>(kGlClampToEdge));
    gl_->BindTexture(kGlTexture2D, 0);

    gl_->BindFramebuffer(kGlFramebuffer, shadow_fbo_);
    gl_->FramebufferTexture2D(kGlFramebuffer, kGlDepthAttachment, kGlTexture2D, shadow_depth_tex_, 0);
    if (gl_->DrawBuffers != nullptr) {
        const GLenum none_buf = kGlNone;
        gl_->DrawBuffers(1, &none_buf);
    }
    if (gl_->ReadBuffer != nullptr) {
        gl_->ReadBuffer(kGlNone);
    }

    if (gl_->CheckFramebufferStatus != nullptr) {
        const GLenum fb_status = gl_->CheckFramebufferStatus(kGlFramebuffer);
        if (fb_status != kGlFramebufferComplete) {
            gl_->BindFramebuffer(kGlFramebuffer, 0);
            return core::Status::fail(core::ErrorCode::ResourceCreationFailed, "WebGL2 shadow FBO incomplete");
        }
    }
    gl_->BindFramebuffer(kGlFramebuffer, 0);

    shadow_atlas_size_ = atlas_size;
    shadow_gpu_bytes_ = static_cast<std::size_t>(atlas_size) * atlas_size * 4u;
    return core::Status::ok();
}

core::Status WebGL2RenderDevice::beginShadowTilePass(
    std::uint32_t tile_x,
    std::uint32_t tile_y,
    std::uint32_t tile_width,
    std::uint32_t tile_height) {
    if (!initialized_ || in_frame_) {
        return core::Status::fail(core::ErrorCode::InvalidState, "beginShadowTilePass called in invalid state");
    }
    in_shadow_pass_ = true;
    if (shadow_fbo_ == 0 || shadow_program_ == 0 || gl_->BindFramebuffer == nullptr) {
        return core::Status::ok();
    }

    if (gl_->ActiveTexture != nullptr && gl_->BindTexture != nullptr) {
        gl_->ActiveTexture(kGlTexture0);
        gl_->BindTexture(kGlTexture2D, 0);
    }
    gl_->BindFramebuffer(kGlFramebuffer, shadow_fbo_);
    gl_->Viewport(
        static_cast<GLint>(tile_x),
        static_cast<GLint>(tile_y),
        static_cast<GLsizei>(tile_width),
        static_cast<GLsizei>(tile_height));
    if (gl_->Scissor != nullptr) {
        gl_->Enable(kGlScissorTest);
        gl_->Scissor(
            static_cast<GLint>(tile_x),
            static_cast<GLint>(tile_y),
            static_cast<GLsizei>(tile_width),
            static_cast<GLsizei>(tile_height));
    }
    if (gl_->ColorMask != nullptr) {
        gl_->ColorMask(kGlFalse, kGlFalse, kGlFalse, kGlFalse);
    }
    if (gl_->Disable != nullptr) {
        gl_->Disable(kGlCullFace);
    }
    if (gl_->DepthMask != nullptr) {
        gl_->DepthMask(kGlTrue);
    }
    gl_->ClearDepthf(1.0f);
    gl_->Clear(kGlDepthBufferBit);
    gl_->UseProgram(shadow_program_);
    return core::Status::ok();
}

core::Status WebGL2RenderDevice::drawShadowMesh(
    GpuMeshHandle mesh_handle,
    const math::Mat4& light_mvp,
    std::uint32_t index_offset,
    std::uint32_t index_count) {
    if (!initialized_ || !in_shadow_pass_) {
        return core::Status::fail(core::ErrorCode::InvalidState, "drawShadowMesh called outside shadow pass");
    }
    const auto it = meshes_.find(mesh_handle.value);
    if (it == meshes_.end()) {
        return core::Status::fail(core::ErrorCode::NotFound, "Unknown GpuMeshHandle in drawShadowMesh");
    }
    if (shadow_fbo_ == 0 || shadow_program_ == 0 || gl_->BindFramebuffer == nullptr) {
        return core::Status::ok();
    }

    const GpuMeshRecord& rec = it->second;
    const auto total_idx = static_cast<std::uint32_t>(rec.index_count);
    if (index_offset >= total_idx) {
        return core::Status::ok();
    }
    const std::uint32_t max_rem = total_idx - index_offset;
    const std::uint32_t draw_count = (index_count == 0u) ? max_rem : std::min(index_count, max_rem);

    if (loc_shadow_mvp_ >= 0) {
        gl_->UniformMatrix4fv(loc_shadow_mvp_, 1, kGlFalse, light_mvp.raw());
    }
    const auto byte_offset = static_cast<std::uintptr_t>(index_offset) * sizeof(std::uint32_t);
    gl_->BindVertexArray(rec.vao);
    gl_->DrawElements(
        kGlTriangles,
        static_cast<GLsizei>(draw_count),
        kGlUnsignedInt,
        reinterpret_cast<const void*>(byte_offset));
    gl_->BindVertexArray(0);
    return core::Status::ok();
}

core::Status WebGL2RenderDevice::endShadowPass() {
    if (!initialized_ || !in_shadow_pass_) {
        return core::Status::fail(core::ErrorCode::InvalidState, "endShadowPass called without beginShadowTilePass");
    }
    in_shadow_pass_ = false;
    if (shadow_fbo_ == 0 || gl_->BindFramebuffer == nullptr) {
        return core::Status::ok();
    }
    if (gl_->Disable != nullptr && gl_->Scissor != nullptr) {
        gl_->Disable(kGlScissorTest);
    }
    if (gl_->ColorMask != nullptr) {
        gl_->ColorMask(kGlTrue, kGlTrue, kGlTrue, kGlTrue);
    }
    gl_->Enable(kGlCullFace);
    gl_->CullFace(kGlBack);
    gl_->BindFramebuffer(kGlFramebuffer, 0);
    gl_->UseProgram(0);
    if (width_ > 0 && height_ > 0) {
        gl_->Viewport(0, 0, static_cast<GLsizei>(width_), static_cast<GLsizei>(height_));
    }
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

    if (shadow_fbo_ != 0 && gl_->DeleteFramebuffers != nullptr) {
        gl_->DeleteFramebuffers(1, &shadow_fbo_);
        shadow_fbo_ = 0;
    }
    if (shadow_depth_tex_ != 0 && gl_->DeleteTextures != nullptr) {
        gl_->DeleteTextures(1, &shadow_depth_tex_);
        shadow_depth_tex_ = 0;
    }
    shadow_atlas_size_ = 0;
    shadow_gpu_bytes_ = 0;

    if (ibl_cubemap_tex_ != 0 && gl_->DeleteTextures != nullptr) {
        gl_->DeleteTextures(1, &ibl_cubemap_tex_);
        ibl_cubemap_tex_ = 0;
    }
    environment_revision_ = 0;
    environment_gpu_bytes_ = 0;

    if (fallback_tex_2d_ != 0 && gl_->DeleteTextures != nullptr) {
        gl_->DeleteTextures(1, &fallback_tex_2d_);
        fallback_tex_2d_ = 0;
    }
    if (fallback_cubemap_ != 0 && gl_->DeleteTextures != nullptr) {
        gl_->DeleteTextures(1, &fallback_cubemap_);
        fallback_cubemap_ = 0;
    }

    if (shadow_program_ != 0) {
        gl_->UseProgram(0);
        gl_->DeleteProgram(shadow_program_);
        shadow_program_ = 0;
    }

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

    uploaded_scene_lighting_ = nullptr;
    in_shadow_pass_ = false;
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
    uploaded_scene_lighting_ = nullptr;

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

    if (gl_->BlendFunc != nullptr && gl_->Disable != nullptr && gl_->DepthMask != nullptr) {
        gl_->Enable(kGlBlend);
        gl_->BlendFunc(kGlSrcAlpha, kGlOneMinusSrcAlpha);
        gl_->DepthMask(kGlFalse);
    }

    gl_->BindVertexArray(grid_vao_);
    gl_->DrawElements(kGlTriangles, 3, kGlUnsignedInt, nullptr);
    gl_->BindVertexArray(0);

    if (gl_->BlendFunc != nullptr && gl_->Disable != nullptr && gl_->DepthMask != nullptr) {
        gl_->DepthMask(kGlTrue);
        gl_->Disable(kGlBlend);
    }
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

    // Ensure valid bound textures on units 0 & 1 for WebGL 2.0 sampler completeness.
    ensureFallbackTextures();
    if (gl_->ActiveTexture != nullptr && gl_->BindTexture != nullptr) {
        gl_->ActiveTexture(kGlTexture0);
        gl_->BindTexture(kGlTexture2D, shadow_depth_tex_ != 0 ? shadow_depth_tex_ : fallback_tex_2d_);
        gl_->ActiveTexture(kGlTexture1);
        gl_->BindTexture(kGlTextureCubeMap, ibl_cubemap_tex_ != 0 ? ibl_cubemap_tex_ : fallback_cubemap_);
        gl_->ActiveTexture(kGlTexture0);
    }

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

    if (command.scene_lighting != nullptr && gl_->Uniform4fv != nullptr) {
        const PreparedSceneLighting& sl = *command.scene_lighting;
        if (uploaded_scene_lighting_ != command.scene_lighting) {
            const auto cnt = static_cast<GLsizei>(LightingLimits::kMaxRealtimeLights);
            const float inv_atlas = (sl.shadow_atlas_size > 0u && shadow_depth_tex_ != 0u)
                                        ? (1.0f / static_cast<float>(sl.shadow_atlas_size))
                                        : 0.0f;
            const float meta[4] = {
                1.0f,
                static_cast<float>(sl.light_count),
                sl.apply_primary_studio_fill ? 1.0f : 0.0f,
                inv_atlas,
            };
            if (loc_light_meta_ >= 0) gl_->Uniform4fv(loc_light_meta_, 1, meta);
            if (loc_light_pos_type_ >= 0) gl_->Uniform4fv(loc_light_pos_type_, cnt, sl.gpu_light_pos_type.data());
            if (loc_light_dir_range_ >= 0) gl_->Uniform4fv(loc_light_dir_range_, cnt, sl.gpu_light_dir_range.data());
            if (loc_light_radiance_atten_ >= 0) {
                gl_->Uniform4fv(loc_light_radiance_atten_, cnt, sl.gpu_light_radiance_atten.data());
            }
            if (loc_light_shape_ >= 0) gl_->Uniform4fv(loc_light_shape_, cnt, sl.gpu_light_shape.data());
            if (loc_light_tangent_shadow_ >= 0) {
                gl_->Uniform4fv(loc_light_tangent_shadow_, cnt, sl.gpu_light_tangent_shadow.data());
            }
            if (loc_light_shadow_params_ >= 0) {
                gl_->Uniform4fv(loc_light_shadow_params_, cnt, sl.gpu_light_shadow_params.data());
            }

            if (loc_shadow_2d_vp_ >= 0) {
                gl_->UniformMatrix4fv(loc_shadow_2d_vp_, 4, kGlFalse, sl.shadow_2d_view_proj[0].raw());
            }
            if (loc_shadow_2d_rect_ >= 0) {
                float rects[16]{};
                for (std::size_t s = 0; s < 4u; ++s) {
                    rects[s * 4u + 0u] = sl.shadow_2d_uv_rects[s].x;
                    rects[s * 4u + 1u] = sl.shadow_2d_uv_rects[s].y;
                    rects[s * 4u + 2u] = sl.shadow_2d_uv_rects[s].z;
                    rects[s * 4u + 3u] = sl.shadow_2d_uv_rects[s].w;
                }
                gl_->Uniform4fv(loc_shadow_2d_rect_, 4, rects);
            }
            if (loc_point_shadow_params_ >= 0) {
                const float pparams[8] = {
                    sl.point_shadow_params[0].x, sl.point_shadow_params[0].y,
                    sl.point_shadow_params[0].z, sl.point_shadow_params[0].w,
                    sl.point_shadow_params[1].x, sl.point_shadow_params[1].y,
                    sl.point_shadow_params[1].z, sl.point_shadow_params[1].w,
                };
                gl_->Uniform4fv(loc_point_shadow_params_, 2, pparams);
            }
            if (loc_point_shadow_far_ >= 0) {
                const float pfar[4] = {sl.point_shadow_far[0], sl.point_shadow_far[1], 0.0f, 0.0f};
                gl_->Uniform4fv(loc_point_shadow_far_, 1, pfar);
            }

            const auto& env = sl.environment;
            const float has_gpu_hdri = (env.has_hdri && ibl_cubemap_tex_ != 0u) ? 1.0f : 0.0f;
            const float env_p[4] = {
                env.enabled ? 1.0f : 0.0f,
                has_gpu_hdri,
                env.intensity,
                static_cast<float>(scene::PrefilteredEnvCubemap::kMipCount - 1u),
            };
            const float env_r[4] = {
                env.cos_rotation,
                env.sin_rotation,
                env.diffuse_ibl_scale,
                env.specular_ibl_scale,
            };
            if (loc_env_params_ >= 0) gl_->Uniform4fv(loc_env_params_, 1, env_p);
            if (loc_env_rot_scales_ >= 0) gl_->Uniform4fv(loc_env_rot_scales_, 1, env_r);
            if (loc_overflow_irradiance_ >= 0) {
                const float ov[3] = {env.overflow_irradiance.x, env.overflow_irradiance.y, env.overflow_irradiance.z};
                gl_->Uniform3fv(loc_overflow_irradiance_, 1, ov);
            }
            if (loc_ibl_sh_ >= 0) {
                gl_->Uniform3fv(loc_ibl_sh_, 9, sl.gpu_ibl_sh_rgb.data());
            }
            uploaded_scene_lighting_ = command.scene_lighting;
        }
        if (loc_light_mask_ >= 0 && gl_->Uniform1i != nullptr) {
            gl_->Uniform1i(loc_light_mask_, static_cast<GLint>(command.light_mask & 0x7FFFFFFFu));
        }
    } else {
        uploaded_scene_lighting_ = nullptr;
        if (loc_light_meta_ >= 0 && gl_->Uniform4fv != nullptr) {
            const float meta_off[4] = {0.0f, 0.0f, 0.0f, 0.0f};
            gl_->Uniform4fv(loc_light_meta_, 1, meta_off);
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
            if (loc_extra_light_dirs_ >= 0) gl_->Uniform3fv(loc_extra_light_dirs_, 3, ex_dirs);
            if (loc_extra_light_radiances_ >= 0) gl_->Uniform3fv(loc_extra_light_radiances_, 3, ex_rads);
        }
        if (loc_ambient_ >= 0) {
            const float amb[3] = {
                command.lighting.ambient.x,
                command.lighting.ambient.y,
                command.lighting.ambient.z,
            };
            gl_->Uniform3fv(loc_ambient_, 1, amb);
        }
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
    if (gl_->ActiveTexture != nullptr && gl_->BindTexture != nullptr) {
        gl_->ActiveTexture(kGlTexture1);
        gl_->BindTexture(kGlTextureCubeMap, 0);
        gl_->ActiveTexture(kGlTexture0);
        gl_->BindTexture(kGlTexture2D, 0);
    }
    gl_->UseProgram(0);
    uploaded_scene_lighting_ = nullptr;
    in_frame_ = false;
    return core::Status::ok();
}

} // namespace finaleffect::rendering
