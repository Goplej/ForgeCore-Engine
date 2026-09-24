#pragma once
// ForgeCore renderer abstraction.
//
// One interface, two backends:
//   * GLRenderer    - OpenGL 3.3 core, loaded at runtime via dlopen
//                     (real GPU / desktop path).
//   * SoftRenderer  - built-in CPU rasterizer (headless, CI, no-GPU).
//
// Both implement the exact same frame protocol, so engine systems, scenes
// and the tech demo are written once and run identically on either backend.

#include "math.hpp"
#include <cstdint>
#include <string>

namespace fc {

inline constexpr int kMaxPointLights = 4;

// ---------------------------------------------------------------- types ---

struct Material {
  Vec4 baseColor{0.8f, 0.8f, 0.8f, 1.0f};
  Vec4 emissive{0.0f, 0.0f, 0.0f, 0.0f};
  float shininess = 16.0f;   // exponent
  float specularStrength = 0.4f;
};

struct DirLightSource {
  Vec3 direction{0.0f, -1.0f, 0.0f};  // light travel direction (normalized)
  Vec3 color{1.0f, 1.0f, 1.0f};
};

struct PointLightSource {
  Vec3 position{0.0f, 0.0f, 0.0f};
  Vec3 color{1.0f, 1.0f, 1.0f};
  float radius = 10.0f;  // falloff distance
};

struct FrameLights {
  DirLightSource dir;
  PointLightSource points[kMaxPointLights];
  int numPoints = 0;
};

enum class ProgramKind { Lit, Unlit };

// Mesh geometry id. Meshes are registered with the renderer; a mesh is an
// interleaved vertex stream {pos(3) normal(3) uv(2)} + uint32 index list.
using MeshId = uint32_t;

// pow(x, e) via exponentiation by squaring on the integer-rounded exponent.
// Visually indistinguishable from libm pow for typical integer shininess
// values (16, 32, 48...) and only a handful of multiplies — no libm call.
inline float fast_blinn_pow(float x, float e) {
  if (x <= 0.0f) return 0.0f;
  int n = (int)(e + 0.5f);
  if (n < 1) n = 1;
  float r = 1.0f, b = x;
  for (;;) {
    if (n & 1) r *= b;
    n >>= 1;
    if (!n) break;
    b *= b;
  }
  return r;
}

// Shared per-fragment lighting model (SoftRenderer executes this on the CPU;
// the GL backend has a GLSL shader implementing the identical model).
// point_mask (when non-null) gates per-point-light contribution; the CPU
// rasterizer passes a per-triangle mask so out-of-reach lights are culled
// before any per-pixel work. Skipped lights contribute exactly zero anyway
// (attenuation reaches 0 at the radius), so results are unchanged.
inline Vec3 light_fragment(const Vec3& N, const Vec3& V, const Vec3& P,
                           const Material& mat, const FrameLights& L,
                           const Vec3& emissiveBoost = Vec3{0, 0, 0},
                           const bool* point_mask = nullptr) {
  Vec3 albedo = mat.baseColor.xyz();
  Vec3 col = Vec3{0.12f, 0.13f, 0.16f} * albedo;  // ambient fill so shadowed faces stay readable
  // directional
  {
    float ndl = -N.dot(L.dir.direction);
    if (ndl > 0.0f) {  // gate: no diffuse -> no specular either
      Vec3 Ld = L.dir.direction * -1.0f;
      Vec3 H = Ld + V;
      float hl2 = H.lengthSq();
      float inv = hl2 > 1e-12f ? 1.0f / std::sqrt(hl2) : 0.0f;  // reciprocal normalization
      float ndh = N.dot(H) * inv;
      float spec = fast_blinn_pow(ndh, mat.shininess) * mat.specularStrength;
      col = col + L.dir.color * (ndl * albedo + Vec3{spec, spec, spec});
    }
  }
  for (int i = 0; i < L.numPoints; ++i) {
    if (point_mask && !point_mask[i]) continue;
    const PointLightSource& pl = L.points[i];
    Vec3 toL = pl.position - P;
    float d2 = toL.lengthSq();
    if (d2 >= pl.radius * pl.radius) continue;  // out of range: would contribute 0
    float d = std::sqrt(d2);
    float atten = 1.0f - d / pl.radius;
    atten *= atten;
    float inv = d > 1e-5f ? 1.0f / d : 0.0f;
    float ndl = std::max(0.0f, N.dot(toL) * inv);
    Vec3 H = toL * inv + V;
    float hl2 = H.lengthSq();
    float hinv = hl2 > 1e-12f ? 1.0f / std::sqrt(hl2) : 0.0f;  // reciprocal normalization
    float ndh = N.dot(H) * hinv;
    float spec = fast_blinn_pow(ndh, mat.shininess) * mat.specularStrength;
    col = col + pl.color * atten * (ndl * albedo + Vec3{spec, spec, spec});
  }
  col = col + emissiveBoost;
  return col;
}

// ---------------------------------------------------------------- iface ---

class Renderer {
 public:
  virtual ~Renderer() = default;

  virtual const char* name() const = 0;

  // Per-frame rasterization counters (reset in begin_frame). The CPU
  // rasterizer fills both; the GL backend reports triangle counts only.
  struct FrameStats {
    uint64_t triangles_rasterized = 0;
    uint64_t pixels_shaded = 0;
  };
  virtual FrameStats frame_stats() const { return {}; }

  // Worker hint for the CPU rasterizer: 1 = serial path, 0 = auto
  // (min(4, hardware cores)). No-op on backends that don't rasterize on CPU.
  virtual void set_threads(int) {}

  // Frame lifecycle. begin returns false if the frame could not start.
  virtual bool begin_frame(int width, int height) = 0;
  virtual void end_frame() = 0;

  // Read the last presented frame as packed RGB8 (3 bytes/px, row-major).
  virtual bool read_frame_rgb8(uint8_t* out, int width, int height) = 0;

  // Draw state -------------------------------------------------------------
  virtual void set_scissor(int x, int y, int w, int h) = 0;
  virtual void clear(float r, float g, float b, float a) = 0;
  virtual void set_depth_test(bool on) = 0;
  virtual void set_blend(bool on) = 0;
  virtual void use_program(ProgramKind kind) = 0;

  virtual void set_matrix(const char* name, const Mat4& m) = 0;  // "proj" "view" "model"
  virtual void set_eye(const Vec3& eye) = 0;
  virtual void set_lights(const FrameLights& L) = 0;
  virtual void set_material(const Material& m) = 0;

  // Textures (RGBA8, uploaded once; id 0 = no texture) ----------------------
  virtual uint32_t create_texture(int w, int h, const uint8_t* rgba, bool srgb = true) = 0;
  virtual void destroy_texture(uint32_t id) = 0;
  virtual void bind_texture(uint32_t id) = 0;  // 0 = none

  // Geometry -----------------------------------------------------------------
  struct Mesh {
    const float* vertices = nullptr;  // interleaved 8 floats
    int vertexCount = 0;
    const uint32_t* indices = nullptr;
    int indexCount = 0;
  };
  virtual MeshId create_mesh(const Mesh& m) = 0;
  virtual void destroy_mesh(MeshId id) = 0;
  virtual void draw(MeshId id) = 0;

  // Shared unit quad (-0.5..0.5 in XY, facing +Z): the sprite primitive.
  virtual MeshId builtin_quad() = 0;
};

}  // namespace fc
