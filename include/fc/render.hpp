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
  PointLightSource points[4];
  int numPoints = 0;
};

enum class ProgramKind { Lit, Unlit };

// Mesh geometry id. Meshes are registered with the renderer; a mesh is an
// interleaved vertex stream {pos(3) normal(3) uv(2)} + uint32 index list.
using MeshId = uint32_t;

// Shared per-fragment lighting model (SoftRenderer executes this on the CPU;
// the GL backend has a GLSL shader implementing the identical model).
inline Vec3 light_fragment(const Vec3& N, const Vec3& V, const Vec3& P,
                           const Material& mat, const FrameLights& L,
                           const Vec3& emissiveBoost = Vec3{0, 0, 0}) {
  Vec3 albedo = mat.baseColor.xyz();
  Vec3 col = Vec3{0.12f, 0.13f, 0.16f} * albedo;  // ambient fill so shadowed faces stay readable
  // directional
  {
    float ndl = std::max(0.0f, -N.dot(L.dir.direction));
    Vec3 H = (L.dir.direction * -1.0f + V).normalized();
    float spec = std::pow(std::max(0.0f, N.dot(H)), mat.shininess) * mat.specularStrength;
    col = col + L.dir.color * (ndl * albedo + Vec3{spec, spec, spec});
  }
  for (int i = 0; i < L.numPoints; ++i) {
    const PointLightSource& pl = L.points[i];
    Vec3 toL = pl.position - P;
    float d = toL.length();
    float atten = std::max(0.0f, 1.0f - d / pl.radius);
    atten *= atten;
    Vec3 Ld = d > 1e-5f ? toL * (1.0f / d) : Vec3{0, 1, 0};
    float ndl = std::max(0.0f, N.dot(Ld));
    Vec3 H = (Ld + V).normalized();
    float spec = std::pow(std::max(0.0f, N.dot(H)), mat.shininess) * mat.specularStrength;
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
