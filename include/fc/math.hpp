#pragma once
// ForgeCore math — small, dependency-free, column-major mat4 (GL friendly).

#include <cmath>

namespace fc::math {

inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float lerp(float a, float b, float t) { return a + (b - a) * t; }
inline float radians(float deg) { return deg * 3.14159265358979323846f / 180.0f; }
constexpr float PI = 3.14159265358979323846f;


struct Vec2 {
  float x = 0, y = 0;
  constexpr Vec2() = default;
  constexpr Vec2(float x, float y) : x(x), y(y) {}
  Vec2 operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
  Vec2 operator-(const Vec2& o) const { return {x - o.x, y - o.y}; }
  Vec2 operator*(float s) const { return {x * s, y * s}; }
  Vec2 operator*(const Vec2& o) const { return {x * o.x, y * o.y}; }
  Vec2 operator/(float s) const { return {x / s, y / s}; }
  float dot(const Vec2& o) const { return x * o.x + y * o.y; }
  float length() const { return std::sqrt(dot(*this)); }
  Vec2 normalized() const { float l = length(); return l > 1e-8f ? Vec2{x / l, y / l} : Vec2{0, 0}; }
  static Vec2 cross(const Vec2& a, const Vec2& b) { return {a.x * b.y - a.y * b.x, 0}; }
};
inline Vec2 operator*(float s, const Vec2& v) { return v * s; }

struct Vec3 {
  float x = 0, y = 0, z = 0;
  constexpr Vec3() = default;
  constexpr Vec3(float x, float y, float z) : x(x), y(y), z(z) {}
  constexpr Vec3(float x, float y) : x(x), y(y), z(0) {}
  Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
  Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
  Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
  Vec3 operator/(float s) const { return {x / s, y / s, z / s}; }
  Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
  Vec3& operator-=(const Vec3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
  Vec3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }
  Vec3 operator*(const Vec3& o) const { return {x * o.x, y * o.y, z * o.z}; }
  float dot(const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }
  float length() const { return std::sqrt(dot(*this)); }
  float lengthSq() const { return dot(*this); }
  Vec3 normalized() const { float l = length(); return l > 1e-8f ? Vec3{x / l, y / l, z / l} : Vec3{0, 0, 0}; }
  static Vec3 cross(const Vec3& a, const Vec3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
  }
};
inline Vec3 operator*(float s, const Vec3& v) { return v * s; }

struct Vec4 {
  float x = 0, y = 0, z = 0, w = 1;
  constexpr Vec4() = default;
  constexpr Vec4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
  static Vec4 from(const Vec3& v, float w) { return {v.x, v.y, v.z, w}; }
  Vec3 xyz() const { return {x, y, z}; }
};

// Column-major 4x4 (m[c * 4 + r]).
struct Mat4 {
  float m[16] = {1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1};

  static Mat4 identity() { return Mat4{}; }

  static Mat4 translation(const Vec3& t) {
    Mat4 r; r.m[12] = t.x; r.m[13] = t.y; r.m[14] = t.z; return r;
  }
  static Mat4 scaling(const Vec3& s) {
    Mat4 r; r.m[0] = s.x; r.m[5] = s.y; r.m[10] = s.z; return r;
  }
  static Mat4 rotationX(float a) {
    float c = std::cos(a), s = std::sin(a); Mat4 r;
    r.m[5] = c; r.m[6] = s; r.m[9] = -s; r.m[10] = c; return r;
  }
  static Mat4 rotationY(float a) {
    float c = std::cos(a), s = std::sin(a); Mat4 r;
    r.m[0] = c; r.m[2] = -s; r.m[8] = s; r.m[10] = c; return r;
  }
  static Mat4 rotationZ(float a) {
    float c = std::cos(a), s = std::sin(a); Mat4 r;
    r.m[0] = c; r.m[1] = s; r.m[4] = -s; r.m[5] = c; return r;
  }

  static Mat4 perspective(float fovyRad, float aspect, float znear, float zfar) {
    Mat4 r{};
    float f = 1.0f / std::tan(fovyRad * 0.5f);
    r.m[0] = f / aspect;
    r.m[5] = f;
    r.m[10] = (zfar + znear) / (znear - zfar);
    r.m[11] = -1.0f;
    r.m[14] = (2.0f * zfar * znear) / (znear - zfar);
    return r;
  }

  static Mat4 ortho(float l, float r_, float b, float t, float zn, float zf) {
    Mat4 m{};
    m.m[0] = 2 / (r_ - l);
    m.m[5] = 2 / (t - b);
    m.m[10] = -2 / (zf - zn);
    m.m[12] = -(r_ + l) / (r_ - l);
    m.m[13] = -(t + b) / (t - b);
    m.m[14] = -(zf + zn) / (zf - zn);
    m.m[15] = 1;
    return m;
  }

  Mat4 operator*(const Mat4& o) const {
    Mat4 r{};
    for (int c = 0; c < 4; ++c)
      for (int rr = 0; rr < 4; ++rr) {
        float acc = 0;
        for (int k = 0; k < 4; ++k) acc += m[k * 4 + rr] * o.m[c * 4 + k];
        r.m[c * 4 + rr] = acc;
      }
    return r;
  }

  Vec4 transform(const Vec4& v) const {
    return {
        m[0] * v.x + m[4] * v.y + m[8] * v.z + m[12] * v.w,
        m[1] * v.x + m[5] * v.y + m[9] * v.z + m[13] * v.w,
        m[2] * v.x + m[6] * v.y + m[10] * v.z + m[14] * v.w,
        m[3] * v.x + m[7] * v.y + m[11] * v.z + m[15] * v.w,
    };
  }
  Vec3 transformPoint(const Vec3& p) const {
    Vec4 v = transform(Vec4::from(p, 1));
    float w = v.w != 0 ? 1.0f / v.w : 0.0f;
    return {v.x * w, v.y * w, v.z * w};
  }
  Vec3 transformDir(const Vec3& d) const {
    return {
        m[0] * d.x + m[4] * d.y + m[8] * d.z,
        m[1] * d.x + m[5] * d.y + m[9] * d.z,
        m[2] * d.x + m[6] * d.y + m[10] * d.z,
    };
  }

  static Mat4 lookAt(const Vec3& eye, const Vec3& center, const Vec3& up) {
    Vec3 f = (center - eye).normalized();
    Vec3 s = Vec3::cross(f, up).normalized();
    Vec3 u = Vec3::cross(s, f);
    Mat4 r;
    r.m[0] = s.x; r.m[4] = s.y; r.m[8] = s.z;
    r.m[1] = u.x; r.m[5] = u.y; r.m[9] = u.z;
    r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z;
    r.m[12] = -s.dot(eye);
    r.m[13] = -u.dot(eye);
    r.m[14] = f.dot(eye);
    return r;
  }

  Mat4 inverse() const {
    // Gauss-Jordan on [M | I] (transcribed row-major; storage is column-major).
    float a[64];
    for (int r = 0; r < 4; ++r)
      for (int c = 0; c < 4; ++c) {
        a[r * 4 + c] = m[c * 4 + r];
        a[16 + r * 4 + c] = (r == c) ? 1.0f : 0.0f;
      }
    for (int col = 0; col < 4; ++col) {
      int pivot = col;
      for (int r = col + 1; r < 4; ++r)
        if (std::fabs(a[r * 4 + col]) > std::fabs(a[pivot * 4 + col])) pivot = r;
      if (std::fabs(a[pivot * 4 + col]) < 1e-12f) return Mat4::identity();
      if (pivot != col)
        for (int i = 0; i < 4; ++i) {
          std::swap(a[col * 4 + i], a[pivot * 4 + i]);
          std::swap(a[16 + col * 4 + i], a[16 + pivot * 4 + i]);
        }
      float d = a[col * 4 + col];
      for (int i = 0; i < 4; ++i) { a[col * 4 + i] /= d; a[16 + col * 4 + i] /= d; }
      for (int r = 0; r < 4; ++r) {
        if (r == col) continue;
        float f = a[r * 4 + col];
        for (int i = 0; i < 4; ++i) {
          a[r * 4 + i] -= f * a[col * 4 + i];
          a[16 + r * 4 + i] -= f * a[16 + col * 4 + i];
        }
      }
    }
    Mat4 r{};
    for (int rw = 0; rw < 4; ++rw)
      for (int c = 0; c < 4; ++c) r.m[c * 4 + rw] = a[16 + rw * 4 + c];
    return r;
  }
};

struct Quat {
  float x = 0, y = 0, z = 0, w = 1;
  static Quat fromAxisAngle(const Vec3& axis, float angle) {
    Vec3 a = axis.normalized();
    float h = angle * 0.5f, s = std::sin(h);
    return {a.x * s, a.y * s, a.z * s, std::cos(h)};
  }
  Quat operator*(const Quat& q) const {
    return Quat{
        w * q.x + x * q.w + y * q.z - z * q.y,
        w * q.y + y * q.w + z * q.x - x * q.z,
        w * q.z + z * q.w + x * q.y - y * q.x,
        w * q.w - x * q.x - y * q.y - z * q.z}.normalized();
  }
  Quat normalized() const {
    float l = std::sqrt(x * x + y * y + z * z + w * w);
    return l > 1e-8f ? Quat{x / l, y / l, z / l, w / l} : Quat{};
  }
  static Quat slerp(const Quat& a, const Quat& b, float t) {
    float d = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    Quat bb = d < 0 ? Quat{-b.x, -b.y, -b.z, -b.w} : b;
    d = d < 0 ? -d : d;
    if (d > 0.9995f)
      return Quat{a.x + (bb.x - a.x) * t, a.y + (bb.y - a.y) * t, a.z + (bb.z - a.z) * t,
                  a.w + (bb.w - a.w) * t}.normalized();
    float th0 = std::acos(clampf(d, -1.0f, 1.0f)), th = th0 * t;
    float s0 = std::sin(th0);
    float sa = std::sin(th0 - th) / s0, sb = std::sin(th) / s0;
    return Quat{a.x * sa + bb.x * sb, a.y * sa + bb.y * sb, a.z * sa + bb.z * sb,
                a.w * sa + bb.w * sb}.normalized();
  }
  Vec3 rotate(const Vec3& v) const {
    // v' = v + 2*w*(q x v) + 2*(q x (q x v))  (xyz part of q)
    Vec3 qv{x, y, z};
    Vec3 t = Vec3::cross(qv, v) * 2.0f;
    return v + t * w + Vec3::cross(qv, t);
  }
  Mat4 toMat4() const {
    float xx = x * x, yy = y * y, zz = z * z;
    float xy = x * y, xz = x * z, yz = y * z;
    float wx = w * x, wy = w * y, wz = w * z;
    Mat4 m;
    m.m[0] = 1 - 2 * (yy + zz); m.m[4] = 2 * (xy - wz);     m.m[8] = 2 * (xz + wy);
    m.m[1] = 2 * (xy + wz);     m.m[5] = 1 - 2 * (xx + zz); m.m[9] = 2 * (yz - wx);
    m.m[2] = 2 * (xz - wy);     m.m[6] = 2 * (yz + wx);     m.m[10] = 1 - 2 * (xx + yy);
    return m;
  }
};

}  // namespace fc::math

// Convenience: engine-wide math aliases (fc::Vec3 etc.)
namespace fc {
using math::Vec2;
using math::Vec3;
using math::Vec4;
using math::Mat4;
using math::Quat;
using math::clampf;
using math::lerp;
using math::radians;
constexpr float PI = math::PI;
}
