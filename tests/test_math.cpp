// Math regression tests — the Mat4::inverse() bug (Gauss-Jordan operating
// only on the left half of the augmented matrix) made every ortho camera
// view collapse to identity, silently clipping the entire 2D scene away.
#include "fc/math.hpp"
#include <cmath>
#include <cstdio>

using namespace fc;

static int failures = 0;

static float err_to_identity(const Mat4& m) {
  float e = 0;
  for (int i = 0; i < 16; ++i) {
    float want = (i % 5 == 0) ? 1.0f : 0.0f;
    e = std::max(e, std::fabs(m.m[i] - want));
  }
  return e;
}

#define CHECK_NEAR(name, v, tol)                                   \
  do {                                                             \
    if (std::fabs((v)) > (tol)) {                                  \
      std::printf("FAIL %s: %g (tol %g)\n", name, (v), (tol));     \
      ++failures;                                                  \
    } else {                                                       \
      std::printf("ok   %s\n", name);                              \
    }                                                              \
  } while (0)

int main() {
  // Translation: T * T^-1 == I, and T^-1 must negate the translation.
  {
    Vec3 t{1.5f, -2.0f, 10.0f};
    Mat4 m = Mat4::translation(t);
    Mat4 inv = m.inverse();
    CHECK_NEAR("T^-1.m14 == -t.z", inv.m[14] + t.z, 1e-4f);
    CHECK_NEAR("T * T^-1 == I", err_to_identity(m * inv), 1e-4f);
  }
  // Rotation (non-symmetric) must invert exactly.
  {
    Mat4 r = Mat4::rotationX(0.3f) * Mat4::rotationY(0.7f) * Mat4::rotationZ(-0.2f);
    CHECK_NEAR("R * R^-1 == I", err_to_identity(r * r.inverse()), 1e-4f);
  }
  // Rigid transform (world-matrix shape: T * R * S).
  {
    Mat4 w = Mat4::translation({3, 1, -2}) * Mat4::rotationZ(0.4f) * Mat4::scaling({2, 2, 2});
    Mat4 wi = w.inverse();
    CHECK_NEAR("W * W^-1 == I", err_to_identity(w * wi), 1e-4f);
    CHECK_NEAR("W^-1 * W == I", err_to_identity(wi * w), 1e-4f);
  }
  // LookAt view: camera at (0,0,10) looking at origin; a point at z=0 must
  // land at z_view = -10 (this is what the 2D/3D cameras depend on).
  {
    Mat4 v = Mat4::lookAt({0, 0, 10}, {0, 0, 0}, {0, 1, 0});
    Vec3 p = v.transformPoint({0, 0, 0});
    CHECK_NEAR("lookAt z_view == -10", p.z + 10.0f, 1e-4f);
    // And view^-1 * view == I.
    CHECK_NEAR("V * V^-1 == I", err_to_identity(v * v.inverse()), 1e-4f);
  }
  // Ortho projection: near/far mapping sanity.
  {
    Mat4 o = Mat4::ortho(-320, 320, -345, 345, 0.1f, 100.0f);
    Vec4 n = o.transform(Vec4{0, 0, -0.1f, 1});
    Vec4 f = o.transform(Vec4{0, 0, -100.0f, 1});
    CHECK_NEAR("ortho near -> -1", n.z + 1.0f, 1e-4f);
    CHECK_NEAR("ortho far -> +1", f.z - 1.0f, 1e-4f);
  }
  if (failures) {
    std::printf("test_math: %d FAILURES\n", failures);
    return 1;
  }
  std::printf("test_math: all passed\n");
  return 0;
}
