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
  // 4 triangles, 50x50 coverage per quad; the 50 shared-diagonal pixels per
  // quad are shaded by both triangles (w == 0 counts), hence 2 * 2550.
  ok = ok && r.triangles_rasterized() == 4 && r.pixels_shaded() == 5100;

  // --------------------------------------------------------------------
  // Threaded rasterizer: the parallel path (scanline bands + worker pool)
  // must be pixel-identical to the serial path, including depth-test and
  // alpha-blend ordering, clipping, texture sampling and per-triangle light
  // culling. Same scene rendered by two renderer instances, one forced to
  // 1 thread, one to 3 threads (>1 band, spans crossing band boundaries).
  {
    auto render_scene = [](SoftRenderer& rr, int threads) {
      std::vector<uint8_t> tex(4 * 4 * 4);
      for (int i = 0; i < 16; ++i) {
        tex[i * 4 + 0] = (uint8_t)(i * 16);
        tex[i * 4 + 1] = (uint8_t)(255 - i * 16);
        tex[i * 4 + 2] = (uint8_t)(i % 2 ? 255 : 60);
        tex[i * 4 + 3] = 255;
      }
      rr.set_threads(threads);
      rr.begin_frame(320, 240);  // big enough that every draw's triangle-row
      rr.set_scissor(0, 0, 320, 240);  // coverage crosses the parallel threshold
      rr.clear(0.05f, 0.06f, 0.09f, 1.0f);
      rr.set_depth_test(true);
      rr.set_blend(true);
      rr.use_program(ProgramKind::Lit);
      rr.set_matrix("proj", Mat4::identity());
      rr.set_matrix("view", Mat4::identity());
      rr.set_matrix("model", Mat4::identity());
      rr.set_eye({0.4f, 0.3f, 4.0f});
      FrameLights L;
      L.dir.direction = {0.3f, -0.8f, -0.5f};
      L.dir.color = {0.7f, 0.7f, 0.8f};
      L.points[0] = PointLightSource{{1.2f, 0.8f, 1.0f}, {1.0f, 0.6f, 0.3f}, 3.0f};
      L.points[1] = PointLightSource{{30.0f, 30.0f, 0.0f}, {5.0f, 5.0f, 5.0f},
                                     2.0f};  // out of range: must be culled
      L.numPoints = 2;
      rr.set_lights(L);
      Material mat;
      mat.baseColor = {0.9f, 0.75f, 0.5f, 0.85f};  // alpha < 1 exercises blending
      mat.specularStrength = 0.9f;
      mat.shininess = 48.0f;
      mat.emissive = {0.02f, 0.01f, 0.0f, 0};
      rr.set_material(mat);
      uint32_t tid = rr.create_texture(4, 4, tex.data());
      rr.bind_texture(tid);

      // builtin_quad drawn from two instances must behave identically (the
      // quad cache used to be a process-wide static and leaked across
      // renderer instances).
      // 1) full-screen quad (rows span every band)
      rr.set_matrix("model", Mat4::scaling({2.2f, 2.2f, 1.0f}));
      rr.draw(rr.builtin_quad());
      // 2) tall rotated quad, closer to the eye (depth-blended over quad 1)
      rr.set_matrix("model", Mat4::translation({-0.3f, 0.1f, -0.5f}) *
                                 Mat4::rotationZ(0.7f) * Mat4::scaling({1.8f, 0.35f, 1.0f}));
      rr.draw(rr.builtin_quad());
      // 3) second instance's quad, nearest (must exist in *this* renderer)
      rr.set_matrix("model", Mat4::translation({0.5f, -0.4f, -0.9f}) *
                                 Mat4::scaling({0.9f, 1.6f, 1.0f}));
      rr.draw(rr.builtin_quad());

      rr.bind_texture(0);
      rr.end_frame();
      std::vector<uint8_t> px(320 * 240 * 3);
      rr.read_frame_rgb8(px.data(), 320, 240);
      rr.destroy_texture(tid);
      return px;
    };

    SoftRenderer serial;
    SoftRenderer threaded;
    std::vector<uint8_t> a = render_scene(serial, 1);
    std::vector<uint8_t> b = render_scene(threaded, 3);
    size_t diff = 0;
    for (size_t i = 0; i < a.size(); ++i)
      if (a[i] != b[i]) ++diff;
    uint64_t tris = serial.triangles_rasterized(), pxs = serial.pixels_shaded();
    bool same_stats = tris == threaded.triangles_rasterized() &&
                      pxs == threaded.pixels_shaded();
    printf("threaded-vs-serial: bytes_diff=%zu tris=%llu px=%llu stats_match=%d "
           "(threaded tris=%llu px=%llu)\n",
           diff, (unsigned long long)tris, (unsigned long long)pxs, (int)same_stats,
           (unsigned long long)threaded.triangles_rasterized(),
           (unsigned long long)threaded.pixels_shaded());
    // The scene must actually shade pixels and run the parallel path:
    ok = ok && diff == 0 && same_stats && pxs > 10000 && tris == 6;
  }

  // builtin_quad across two fresh instances must render (regression for the
  // static-cache leak: the second instance used to draw nothing).
  {
    SoftRenderer r2;
    r2.begin_frame(64, 64);
    r2.set_scissor(0, 0, 64, 64);
    r2.clear(0, 0, 0, 1);
    r2.set_depth_test(false);
    r2.set_blend(false);
    r2.use_program(ProgramKind::Unlit);
    r2.set_matrix("proj", Mat4::identity());
    r2.set_matrix("view", Mat4::identity());
    r2.set_matrix("model", Mat4::scaling({1.9f, 1.9f, 1.0f}));
    Material m;
    m.baseColor = {0, 1, 0, 1};
    r2.set_material(m);
    r2.bind_texture(0);
    r2.draw(r2.builtin_quad());
    r2.end_frame();
    std::vector<uint8_t> px(64 * 64 * 3);
    r2.read_frame_rgb8(px.data(), 64, 64);
    uint8_t c = px[(32 * 64 + 32) * 3];
    printf("quad-second-instance center=%02x%02x%02x\n", c, px[(32 * 64 + 32) * 3 + 1],
           px[(32 * 64 + 32) * 3 + 2]);
    ok = ok && c == 0 && px[(32 * 64 + 32) * 3 + 1] == 255 && px[(32 * 64 + 32) * 3 + 2] == 0;
  }

  return ok ? 0 : 1;
}
