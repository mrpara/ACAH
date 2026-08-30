// math3d.h - minimal 3D math for the SpiderBot ASCII engine.
// Column-major matrices, right-handed coordinates, +Y up.
#pragma once

#include <cmath>
#include <algorithm>
#include <cstdint>

namespace sb {

constexpr float PI = 3.14159265358979323846f;
constexpr float TAU = 6.28318530717958647692f;

inline float deg2rad(float d) { return d * (PI / 180.0f); }
inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
inline float smoothstep01(float t) { t = clampf(t, 0.0f, 1.0f); return t * t * (3.0f - 2.0f * t); }
inline float signf(float v) { return v < 0.0f ? -1.0f : 1.0f; }

// Frame-rate independent exponential approach. rate = "how fast", dt = seconds.
inline float damp(float a, float b, float rate, float dt) {
    return lerpf(a, b, 1.0f - std::exp(-rate * dt));
}

// Shortest signed angular difference from a to b, in radians.
inline float angleDelta(float a, float b) {
    float d = std::fmod(b - a + PI, TAU);
    if (d < 0.0f) d += TAU;
    return d - PI;
}

// ---------------------------------------------------------------- Vec2 / Vec3

struct Vec2 {
    float x = 0.0f, y = 0.0f;
    Vec2() = default;
    Vec2(float a, float b) : x(a), y(b) {}
};

struct Vec3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    Vec3() = default;
    Vec3(float a, float b, float c) : x(a), y(b), z(c) {}
    explicit Vec3(float s) : x(s), y(s), z(s) {}
};

inline Vec3 operator+(const Vec3& a, const Vec3& b) { return Vec3(a.x + b.x, a.y + b.y, a.z + b.z); }
inline Vec3 operator-(const Vec3& a, const Vec3& b) { return Vec3(a.x - b.x, a.y - b.y, a.z - b.z); }
inline Vec3 operator*(const Vec3& a, float s)       { return Vec3(a.x * s, a.y * s, a.z * s); }
inline Vec3 operator*(float s, const Vec3& a)       { return a * s; }
inline Vec3 operator*(const Vec3& a, const Vec3& b) { return Vec3(a.x * b.x, a.y * b.y, a.z * b.z); }
inline Vec3 operator/(const Vec3& a, float s)       { return a * (1.0f / s); }
inline Vec3 operator-(const Vec3& a)                { return Vec3(-a.x, -a.y, -a.z); }
inline Vec3& operator+=(Vec3& a, const Vec3& b) { a.x += b.x; a.y += b.y; a.z += b.z; return a; }
inline Vec3& operator-=(Vec3& a, const Vec3& b) { a.x -= b.x; a.y -= b.y; a.z -= b.z; return a; }
inline Vec3& operator*=(Vec3& a, float s)       { a.x *= s; a.y *= s; a.z *= s; return a; }

inline float dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 cross(const Vec3& a, const Vec3& b) {
    return Vec3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}
inline float lengthSq(const Vec3& v) { return dot(v, v); }
inline float length(const Vec3& v) { return std::sqrt(dot(v, v)); }
inline Vec3 normalize(const Vec3& v) {
    float l = length(v);
    return l > 1e-8f ? v * (1.0f / l) : Vec3(0.0f, 1.0f, 0.0f);
}
inline Vec3 lerp(const Vec3& a, const Vec3& b, float t) { return a + (b - a) * t; }
inline Vec3 minv(const Vec3& a, const Vec3& b) {
    return Vec3(std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z));
}
inline Vec3 maxv(const Vec3& a, const Vec3& b) {
    return Vec3(std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z));
}
inline Vec3 clampv(const Vec3& v, float lo, float hi) {
    return Vec3(clampf(v.x, lo, hi), clampf(v.y, lo, hi), clampf(v.z, lo, hi));
}
// Horizontal (XZ) helpers - used constantly by the gait and steering code.
inline float lengthXZ(const Vec3& v) { return std::sqrt(v.x * v.x + v.z * v.z); }
inline Vec3 flattenY(const Vec3& v) { return Vec3(v.x, 0.0f, v.z); }

struct Vec4 {
    float x = 0.0f, y = 0.0f, z = 0.0f, w = 0.0f;
    Vec4() = default;
    Vec4(float a, float b, float c, float d) : x(a), y(b), z(c), w(d) {}
    Vec4(const Vec3& v, float d) : x(v.x), y(v.y), z(v.z), w(d) {}
    Vec3 xyz() const { return Vec3(x, y, z); }
};

inline Vec4 operator+(const Vec4& a, const Vec4& b) { return Vec4(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w); }
inline Vec4 operator-(const Vec4& a, const Vec4& b) { return Vec4(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w); }
inline Vec4 operator*(const Vec4& a, float s)       { return Vec4(a.x * s, a.y * s, a.z * s, a.w * s); }

// -------------------------------------------------------------------- Mat4
// Column-major storage: m[col * 4 + row], same convention as OpenGL.

struct Mat4 {
    float m[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};

    float& at(int row, int col) { return m[col * 4 + row]; }
    float at(int row, int col) const { return m[col * 4 + row]; }

    static Mat4 identity() { return Mat4(); }

    static Mat4 translation(const Vec3& t) {
        Mat4 r;
        r.at(0, 3) = t.x; r.at(1, 3) = t.y; r.at(2, 3) = t.z;
        return r;
    }
    static Mat4 scaling(const Vec3& s) {
        Mat4 r;
        r.at(0, 0) = s.x; r.at(1, 1) = s.y; r.at(2, 2) = s.z;
        return r;
    }
    static Mat4 rotationX(float a) {
        Mat4 r; float c = std::cos(a), s = std::sin(a);
        r.at(1, 1) = c; r.at(1, 2) = -s;
        r.at(2, 1) = s; r.at(2, 2) = c;
        return r;
    }
    static Mat4 rotationY(float a) {
        Mat4 r; float c = std::cos(a), s = std::sin(a);
        r.at(0, 0) = c;  r.at(0, 2) = s;
        r.at(2, 0) = -s; r.at(2, 2) = c;
        return r;
    }
    static Mat4 rotationZ(float a) {
        Mat4 r; float c = std::cos(a), s = std::sin(a);
        r.at(0, 0) = c; r.at(0, 1) = -s;
        r.at(1, 0) = s; r.at(1, 1) = c;
        return r;
    }
    // Build a rotation from an orthonormal basis (columns are the new axes).
    static Mat4 basis(const Vec3& x, const Vec3& y, const Vec3& z) {
        Mat4 r;
        r.at(0, 0) = x.x; r.at(1, 0) = x.y; r.at(2, 0) = x.z;
        r.at(0, 1) = y.x; r.at(1, 1) = y.y; r.at(2, 1) = y.z;
        r.at(0, 2) = z.x; r.at(1, 2) = z.y; r.at(2, 2) = z.z;
        return r;
    }
};

inline Mat4 operator*(const Mat4& a, const Mat4& b) {
    Mat4 r;
    for (int c = 0; c < 4; ++c) {
        for (int i = 0; i < 4; ++i) {
            r.m[c * 4 + i] = a.m[0 * 4 + i] * b.m[c * 4 + 0]
                           + a.m[1 * 4 + i] * b.m[c * 4 + 1]
                           + a.m[2 * 4 + i] * b.m[c * 4 + 2]
                           + a.m[3 * 4 + i] * b.m[c * 4 + 3];
        }
    }
    return r;
}

inline Vec4 transform(const Mat4& a, const Vec4& v) {
    return Vec4(
        a.m[0] * v.x + a.m[4] * v.y + a.m[8]  * v.z + a.m[12] * v.w,
        a.m[1] * v.x + a.m[5] * v.y + a.m[9]  * v.z + a.m[13] * v.w,
        a.m[2] * v.x + a.m[6] * v.y + a.m[10] * v.z + a.m[14] * v.w,
        a.m[3] * v.x + a.m[7] * v.y + a.m[11] * v.z + a.m[15] * v.w);
}
inline Vec3 transformPoint(const Mat4& a, const Vec3& v) {
    return Vec3(
        a.m[0] * v.x + a.m[4] * v.y + a.m[8]  * v.z + a.m[12],
        a.m[1] * v.x + a.m[5] * v.y + a.m[9]  * v.z + a.m[13],
        a.m[2] * v.x + a.m[6] * v.y + a.m[10] * v.z + a.m[14]);
}
inline Vec3 transformDir(const Mat4& a, const Vec3& v) {
    return Vec3(
        a.m[0] * v.x + a.m[4] * v.y + a.m[8]  * v.z,
        a.m[1] * v.x + a.m[5] * v.y + a.m[9]  * v.z,
        a.m[2] * v.x + a.m[6] * v.y + a.m[10] * v.z);
}

// Right-handed perspective projection producing clip space with z in [-w, w].
inline Mat4 perspective(float fovYRadians, float aspect, float zNear, float zFar) {
    Mat4 r;
    const float f = 1.0f / std::tan(fovYRadians * 0.5f);
    r.m[0] = f / aspect; r.m[1] = 0; r.m[2] = 0; r.m[3] = 0;
    r.m[4] = 0; r.m[5] = f; r.m[6] = 0; r.m[7] = 0;
    r.m[8] = 0; r.m[9] = 0; r.m[10] = (zFar + zNear) / (zNear - zFar); r.m[11] = -1.0f;
    r.m[12] = 0; r.m[13] = 0; r.m[14] = (2.0f * zFar * zNear) / (zNear - zFar); r.m[15] = 0;
    return r;
}

inline Mat4 lookAt(const Vec3& eye, const Vec3& target, const Vec3& up) {
    const Vec3 f = normalize(target - eye);
    Vec3 s = cross(f, up);
    if (lengthSq(s) < 1e-10f) s = cross(f, Vec3(1.0f, 0.0f, 0.0f));
    s = normalize(s);
    const Vec3 u = cross(s, f);
    Mat4 r;
    r.at(0, 0) = s.x; r.at(0, 1) = s.y; r.at(0, 2) = s.z; r.at(0, 3) = -dot(s, eye);
    r.at(1, 0) = u.x; r.at(1, 1) = u.y; r.at(1, 2) = u.z; r.at(1, 3) = -dot(u, eye);
    r.at(2, 0) = -f.x; r.at(2, 1) = -f.y; r.at(2, 2) = -f.z; r.at(2, 3) = dot(f, eye);
    return r;
}

// Inverse of a rigid transform (rotation + translation, no scale).
inline Mat4 invertRigid(const Mat4& a) {
    Mat4 r;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            r.at(i, j) = a.at(j, i);
    const Vec3 t(a.at(0, 3), a.at(1, 3), a.at(2, 3));
    r.at(0, 3) = -(r.at(0, 0) * t.x + r.at(0, 1) * t.y + r.at(0, 2) * t.z);
    r.at(1, 3) = -(r.at(1, 0) * t.x + r.at(1, 1) * t.y + r.at(1, 2) * t.z);
    r.at(2, 3) = -(r.at(2, 0) * t.x + r.at(2, 1) * t.y + r.at(2, 2) * t.z);
    return r;
}

// Rotation that takes +Y onto `dir`. Used to place every limb segment mesh,
// all of which are authored as unit-height cylinders along +Y.
inline Mat4 alignYTo(const Vec3& dir) {
    const Vec3 y = normalize(dir);
    Vec3 ref = (std::fabs(y.y) > 0.99f) ? Vec3(1.0f, 0.0f, 0.0f) : Vec3(0.0f, 1.0f, 0.0f);
    const Vec3 x = normalize(cross(ref, y));
    const Vec3 z = cross(x, y);
    return Mat4::basis(x, y, z);
}

// Places a unit cylinder (radius 1, height 1, base at origin, axis +Y) so that
// it spans from `a` to `b` with the given radius.
inline Mat4 segmentTransform(const Vec3& a, const Vec3& b, float radius) {
    const Vec3 d = b - a;
    const float len = length(d);
    return Mat4::translation(a) * alignYTo(d) * Mat4::scaling(Vec3(radius, std::max(len, 1e-4f), radius));
}

// --------------------------------------------------------------- misc helpers

struct Rng {
    uint32_t s;
    explicit Rng(uint32_t seed = 1u) : s(seed ? seed : 1u) {}
    uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float unit() { return (next() >> 8) * (1.0f / 16777216.0f); }        // [0,1)
    float range(float a, float b) { return a + (b - a) * unit(); }
};

} // namespace sb
