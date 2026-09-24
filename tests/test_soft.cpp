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
  int ok = at(50, 50) == 0xff0000 && at(2, 2) == 0;

  // Textured sprite: a 2x2 red/blue checker sampled through builtin_quad must
  // land in the correct half of the quad (barycentric label regression).
  {
    uint8_t tex[2 * 2 * 4] = {255, 0, 0, 255,  0, 0, 255, 255,
                              0, 0, 255, 255,  255, 0, 0, 255};
    MeshId tid = r.create_texture(2, 2, tex);
    r.clear(0, 0, 0, 1);
    r.set_matrix("model", Mat4::identity());
    Material m;
    m.baseColor = {1, 1, 1, 1};
    r.set_material(m);
    r.bind_texture(tid);
    r.draw(r.builtin_quad());
    r.end_frame();
    r.read_frame_rgb8(px.data(), 100, 100);
    // quad covers x,y in [25,75); u goes with x, v (flipped) with y:
    // top-left (30,30)  -> texel (0,0) = red
    // top-right (70,30) -> texel (1,0) = blue
    // bottom-right (70,70) -> texel (1,1) = red
    uint32_t tl = at(30, 30), tr = at(70, 30), br = at(70, 70);
    printf("tex tl=%06x tr=%06x br=%06x\n", tl, tr, br);
    ok = ok && tl == 0xff0000 && tr == 0x0000ff && br == 0xff0000;
  }
  printf("tris=%llu px=%llu\n", (unsigned long long)r.triangles_rasterized(),
         (unsigned long long)r.pixels_shaded());
  return ok ? 0 : 1;
}
