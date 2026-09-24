// Minimal rasterizer sanity test (no engine loop).
#include "fc/render.hpp"
#include "../src/render/soft/soft_renderer.hpp"
#include <cstdio>

using namespace fc;

int main() {
  SoftRenderer r;
  r.begin_frame(100, 100);
  r.set_scissor(0, 0, 100, 100);
  r.clear(0, 0, 0, 1);
  r.set_depth_test(false);
  r.set_blend(false);
  r.use_program(ProgramKind::Unlit);
  r.set_matrix("proj", Mat4::identity());
  r.set_matrix("view", Mat4::identity());
  // screen-space-ish quad at z=0 in ndc: x in [-1,1], y in [1,-1]
  float v[4 * 8] = {
      -0.5f, 0.5f, 0, 0, 0, 1, 0, 0,
      0.5f, 0.5f, 0, 0, 0, 1, 1, 0,
      0.5f, -0.5f, 0, 0, 0, 1, 1, 1,
      -0.5f, -0.5f, 0, 0, 0, 1, 0, 1,
  };
  uint32_t idx[6] = {0, 1, 2, 0, 2, 3};
  Renderer::Mesh m{v, 4, idx, 6};
  MeshId id = r.create_mesh(m);
  Material mat;
  mat.baseColor = {1, 0, 0, 1};
  r.set_material(mat);
  r.bind_texture(0);
  r.draw(id);
  r.end_frame();
  std::vector<uint8_t> px(100 * 100 * 3);
  r.read_frame_rgb8(px.data(), 100, 100);
  auto at = [&](int x, int y) { return (px[(y * 100 + x) * 3] << 16) | (px[(y * 100 + x) * 3 + 1] << 8) |
                                        px[(y * 100 + x) * 3 + 2]; };
  printf("center(50,50)=%06x corner(2,2)=%06x\n", at(50, 50), at(2, 2));
  printf("tris=%llu px=%llu\n", (unsigned long long)r.triangles_rasterized(),
         (unsigned long long)r.pixels_shaded());
  return (at(50, 50) == 0xff0000 && at(2, 2) == 0) ? 0 : 1;
}
