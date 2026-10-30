// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Final Effect 3D Contributors

#include "finaleffect/math/mat4.hpp"

#include <cmath>

namespace finaleffect::math {

Mat4 Mat4::translation(const Vec3& t) noexcept {
    Mat4 m = identity();
    m(0, 3) = t.x;
    m(1, 3) = t.y;
    m(2, 3) = t.z;
    return m;
}

Mat4 Mat4::scale(const Vec3& s) noexcept {
    Mat4 m = zero();
    m(0, 0) = s.x;
    m(1, 1) = s.y;
    m(2, 2) = s.z;
    m(3, 3) = 1.0f;
    return m;
}

Mat4 Mat4::fromQuat(const Quat& q_in) noexcept {
    const Quat q = q_in.normalized();
    const float xx = q.x * q.x;
    const float yy = q.y * q.y;
    const float zz = q.z * q.z;
    const float xy = q.x * q.y;
    const float xz = q.x * q.z;
    const float yz = q.y * q.z;
    const float wx = q.w * q.x;
    const float wy = q.w * q.y;
    const float wz = q.w * q.z;

    Mat4 m = identity();
    m(0, 0) = 1.0f - 2.0f * (yy + zz);
    m(1, 0) = 2.0f * (xy + wz);
    m(2, 0) = 2.0f * (xz - wy);

    m(0, 1) = 2.0f * (xy - wz);
    m(1, 1) = 1.0f - 2.0f * (xx + zz);
    m(2, 1) = 2.0f * (yz + wx);

    m(0, 2) = 2.0f * (xz + wy);
    m(1, 2) = 2.0f * (yz - wx);
    m(2, 2) = 1.0f - 2.0f * (xx + yy);
    return m;
}

Mat4 Mat4::trs(const Vec3& t, const Quat& r, const Vec3& s) noexcept {
    Mat4 m = fromQuat(r);
    m(0, 0) *= s.x;
    m(1, 0) *= s.x;
    m(2, 0) *= s.x;

    m(0, 1) *= s.y;
    m(1, 1) *= s.y;
    m(2, 1) *= s.y;

    m(0, 2) *= s.z;
    m(1, 2) *= s.z;
    m(2, 2) *= s.z;

    m(0, 3) = t.x;
    m(1, 3) = t.y;
    m(2, 3) = t.z;
    return m;
}

Mat4 Mat4::lookAtRH(const Vec3& eye, const Vec3& target, const Vec3& up) noexcept {
    Vec3 f = (target - eye).normalized();
    if (f.lengthSquared() <= kStrictEpsilon) {
        f = Vec3{0.0f, 0.0f, -1.0f};
    }

    Vec3 up_dir = up.normalized();
    if (up_dir.lengthSquared() <= kStrictEpsilon) {
        up_dir = Vec3::unitY();
    }

    Vec3 s = f.cross(up_dir).normalized();
    if (s.lengthSquared() <= kStrictEpsilon) {
        const Vec3 fallback_up = (std::fabs(f.y) < 0.99f) ? Vec3::unitY() : Vec3::unitZ();
        s = f.cross(fallback_up).normalized();
    }
    const Vec3 u = s.cross(f);

    Mat4 m = identity();
    m(0, 0) = s.x;
    m(0, 1) = s.y;
    m(0, 2) = s.z;
    m(0, 3) = -s.dot(eye);

    m(1, 0) = u.x;
    m(1, 1) = u.y;
    m(1, 2) = u.z;
    m(1, 3) = -u.dot(eye);

    m(2, 0) = -f.x;
    m(2, 1) = -f.y;
    m(2, 2) = -f.z;
    m(2, 3) = f.dot(eye);

    return m;
}

Mat4 Mat4::perspectiveRH_NO(float fov_y_radians, float aspect, float z_near, float z_far) noexcept {
    const float safe_aspect = (std::fabs(aspect) > kStrictEpsilon) ? aspect : 1.0f;
    const float tan_half_fov = std::tan(0.5f * fov_y_radians);

    Mat4 m = zero();
    m(0, 0) = 1.0f / (safe_aspect * tan_half_fov);
    m(1, 1) = 1.0f / tan_half_fov;
    m(2, 2) = -(z_far + z_near) / (z_far - z_near);
    m(3, 2) = -1.0f;
    m(2, 3) = -(2.0f * z_far * z_near) / (z_far - z_near);
    return m;
}

Mat4 Mat4::perspectiveRH_ZO(float fov_y_radians, float aspect, float z_near, float z_far) noexcept {
    const float safe_aspect = (std::fabs(aspect) > kStrictEpsilon) ? aspect : 1.0f;
    const float tan_half_fov = std::tan(0.5f * fov_y_radians);

    Mat4 m = zero();
    m(0, 0) = 1.0f / (safe_aspect * tan_half_fov);
    m(1, 1) = 1.0f / tan_half_fov;
    m(2, 2) = z_far / (z_near - z_far);
    m(3, 2) = -1.0f;
    m(2, 3) = -(z_far * z_near) / (z_far - z_near);
    return m;
}

Mat4 Mat4::orthographicRH_NO(
    float left, float right, float bottom, float top, float z_near, float z_far) noexcept {
    Mat4 m = identity();
    m(0, 0) = 2.0f / (right - left);
    m(1, 1) = 2.0f / (top - bottom);
    m(2, 2) = -2.0f / (z_far - z_near);
    m(0, 3) = -(right + left) / (right - left);
    m(1, 3) = -(top + bottom) / (top - bottom);
    m(2, 3) = -(z_far + z_near) / (z_far - z_near);
    return m;
}

Mat4 Mat4::orthographicRH_ZO(
    float left, float right, float bottom, float top, float z_near, float z_far) noexcept {
    Mat4 m = identity();
    m(0, 0) = 2.0f / (right - left);
    m(1, 1) = 2.0f / (top - bottom);
    m(2, 2) = -1.0f / (z_far - z_near);
    m(0, 3) = -(right + left) / (right - left);
    m(1, 3) = -(top + bottom) / (top - bottom);
    m(2, 3) = -z_near / (z_far - z_near);
    return m;
}

Mat4 Mat4::operator*(const Mat4& rhs) const noexcept {
    Mat4 out = zero();
    for (std::size_t col = 0; col < 4; ++col) {
        for (std::size_t row = 0; row < 4; ++row) {
            float sum = 0.0f;
            for (std::size_t k = 0; k < 4; ++k) {
                sum += (*this)(row, k) * rhs(k, col);
            }
            out(row, col) = sum;
        }
    }
    return out;
}

Vec4 Mat4::operator*(const Vec4& v) const noexcept {
    return {
        (*this)(0, 0) * v.x + (*this)(0, 1) * v.y + (*this)(0, 2) * v.z + (*this)(0, 3) * v.w,
        (*this)(1, 0) * v.x + (*this)(1, 1) * v.y + (*this)(1, 2) * v.z + (*this)(1, 3) * v.w,
        (*this)(2, 0) * v.x + (*this)(2, 1) * v.y + (*this)(2, 2) * v.z + (*this)(2, 3) * v.w,
        (*this)(3, 0) * v.x + (*this)(3, 1) * v.y + (*this)(3, 2) * v.z + (*this)(3, 3) * v.w,
    };
}

Vec3 Mat4::transformPoint(const Vec3& p) const noexcept {
    const Vec4 r = (*this) * Vec4(p, 1.0f);
    if (std::fabs(r.w) > kStrictEpsilon && !math::approxEqual(r.w, 1.0f, kStrictEpsilon)) {
        const float inv_w = 1.0f / r.w;
        return {r.x * inv_w, r.y * inv_w, r.z * inv_w};
    }
    return r.xyz();
}

Vec3 Mat4::transformDirection(const Vec3& d) const noexcept {
    return ((*this) * Vec4(d, 0.0f)).xyz();
}

Mat4 Mat4::transposed() const noexcept {
    Mat4 out{};
    for (std::size_t col = 0; col < 4; ++col) {
        for (std::size_t row = 0; row < 4; ++row) {
            out(row, col) = (*this)(col, row);
        }
    }
    return out;
}

namespace {
std::array<float, 16> computeAdjugateColumnMajor(const std::array<float, 16>& m) noexcept {
    std::array<float, 16> inv{};

    inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] +
             m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];

    inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] -
             m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];

    inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] +
             m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];

    inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] -
              m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];

    inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] -
             m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];

    inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] +
             m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];

    inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] -
             m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];

    inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] +
              m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];

    inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] +
             m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];

    inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] -
             m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];

    inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] +
              m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];

    inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] -
              m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];

    inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] -
             m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];

    inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] +
             m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];

    inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] -
              m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];

    inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] +
              m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];

    return inv;
}
} // namespace

float Mat4::determinant() const noexcept {
    const auto adj = computeAdjugateColumnMajor(data);
    return data[0] * adj[0] + data[1] * adj[4] + data[2] * adj[8] + data[3] * adj[12];
}

std::optional<Mat4> Mat4::inverted(float min_determinant) const noexcept {
    const auto adj = computeAdjugateColumnMajor(data);
    const float det = data[0] * adj[0] + data[1] * adj[4] + data[2] * adj[8] + data[3] * adj[12];
    if (!math::isFinite(det) || std::fabs(det) <= min_determinant) {
        return std::nullopt;
    }

    const float inv_det = 1.0f / det;
    Mat4 result{};
    for (std::size_t i = 0; i < 16; ++i) {
        result.data[i] = adj[i] * inv_det;
    }
    return result;
}

bool Mat4::isFinite() const noexcept {
    for (const float v : data) {
        if (!math::isFinite(v)) {
            return false;
        }
    }
    return true;
}

bool Mat4::approxEqual(const Mat4& rhs, float epsilon) const noexcept {
    for (std::size_t i = 0; i < 16; ++i) {
        if (!math::approxEqual(data[i], rhs.data[i], epsilon)) {
            return false;
        }
    }
    return true;
}

} // namespace finaleffect::math
