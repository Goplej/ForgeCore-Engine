#pragma once
#include "fc/render.hpp"
#include <memory>
#include <unordered_map>
#include <vector>

namespace fc {

class SoftWorkerPool;  // persistent raster worker threads (defined in .cpp)

// CPU rasterizer implementing the ForgeCore Renderer contract.
// Z-buffer, alpha blending, scissor, near-plane clipping, per-fragment
// lighting (same model as the GL shader), nearest-neighbor texturing.
//
// Threading: draw() collects screen-space triangles, then rasterizes them
// with a worker pool over area-balanced scanline bands. Every pixel is
// owned by exactly one band, and triangles are visited in submission order
// within each band, so the output is pixel-identical to the serial path
// (verified by a regression test in tests/test_soft.cpp).
class SoftRenderer : public Renderer {
 public:
  SoftRenderer();  // out of line: holds a unique_ptr<SoftWorkerPool>
  ~SoftRenderer() override;

  const char* name() const override { return "soft (CPU rasterizer)"; }

  bool begin_frame(int width, int height) override;
  void end_frame() override;
  bool read_frame_rgb8(uint8_t* out, int width, int height) override;

  void set_scissor(int x, int y, int w, int h) override;
  void clear(float r, float g, float b, float a) override;
  void set_depth_test(bool on) override { depth_test_ = on; }
  void set_blend(bool on) override { blend_ = on; }
  void use_program(ProgramKind kind) override { prog_ = kind; }
  void set_matrix(const char* name, const Mat4& m) override;
  void set_eye(const Vec3& eye) override { eye_ = eye; }
  void set_lights(const FrameLights& L) override { lights_ = L; }
  void set_material(const Material& m) override { material_ = m; }

  uint32_t create_texture(int w, int h, const uint8_t* rgba, bool srgb = true) override;
  void destroy_texture(uint32_t id) override;
  void bind_texture(uint32_t id) override { bound_tex_ = id; }

  MeshId create_mesh(const Mesh& m) override;
  void destroy_mesh(MeshId id) override;
  void draw(MeshId id) override;
  MeshId builtin_quad() override;

  void set_threads(int n) override;  // 0 = auto (min(4, hardware cores))
  FrameStats frame_stats() const override {
    return {triangles_rasterized_, pixels_shaded_};
  }

  uint64_t triangles_rasterized() const { return triangles_rasterized_; }
  uint64_t pixels_shaded() const { return pixels_shaded_; }

 private:
  struct Tex {
    int w = 0, h = 0;
    std::vector<uint8_t> rgba;
  };
  struct MeshData {
    std::vector<float> v;
    std::vector<uint32_t> idx;
  };

  struct ClipVertex {
    float x, y, z, w;  // clip space
  };
  struct NdcVertex {
    float x, y, z;     // ndc
    float wx, wy, wz;  // world position
    float nx, ny, nz;  // world normal
    float u, v;
  };
  // A fully clipped, screen-space triangle ready for row-band rasterization.
  struct RasterTri {
    NdcVertex a, b, c;
    float s2;         // 2*abs(area), > 0
    bool ccw;
    int ix0, ix1, iy0, iy1;  // screen bbox (clamped to framebuffer)
  };

  bool clip_triangle(const ClipVertex in[3], ClipVertex out[8], int* outn);
  void project(const ClipVertex& c, float& sx, float& sy, float& sz);
  void raster_rows(const RasterTri& t, int ry0, int ry1, uint64_t& px_shaded);
  void flush_batch();
  int resolve_threads() const;
  void ensure_pool(int threads);

  int W_ = 0, H_ = 0;
  std::vector<uint8_t> front_, back_;   // RGBA8
  std::vector<float> depth_;            // NDC z in [0,1], 1 = far
  int scx_ = 0, scy_ = 0, scw_ = 0, sch_ = 0;
  bool has_scissor_ = false;
  bool depth_test_ = true, blend_ = true;
  ProgramKind prog_ = ProgramKind::Lit;
  Mat4 proj_ = Mat4::identity(), view_ = Mat4::identity(), model_ = Mat4::identity();
  Vec3 eye_{0, 0, 5};
  FrameLights lights_;
  Material material_;
  uint32_t bound_tex_ = 0;

  std::unordered_map<uint32_t, Tex> textures_;
  std::unordered_map<MeshId, MeshData> meshes_;
  uint32_t next_id_ = 1;
  MeshId quad_id_ = 0;  // per-instance builtin quad cache
  uint64_t triangles_rasterized_ = 0;
  uint64_t pixels_shaded_ = 0;

  // threaded rasterization
  std::vector<RasterTri> batch_;
  std::vector<int> band_start_;
  std::vector<int> row_weight_;
  std::vector<uint64_t> band_px_, band_tri_;
  std::unique_ptr<SoftWorkerPool> pool_;
  int pool_threads_ = 0;
  int threads_ = 0;  // 0 = auto
};

}  // namespace fc
