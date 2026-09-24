#include "fc/geometry.hpp"

#include <cmath>

namespace fc {

namespace {
void push_vertex(std::vector<float>& v, float x, float y, float z, float nx, float ny, float nz,
                 float u, float vv) {
  v.insert(v.end(), {x, y, z, nx, ny, nz, u, vv});
}
}  // namespace

Geometry make_cube() {
  Geometry g;
  auto face = [&](float nx, float ny, float nz,
                  float ax, float ay, float az, float bx, float by, float bz,
                  float cx, float cy, float cz, float dx, float dy, float dz) {
    uint32_t base = (uint32_t)g.vertices.size() / 8;
    push_vertex(g.vertices, ax, ay, az, nx, ny, nz, 0, 0);
    push_vertex(g.vertices, bx, by, bz, nx, ny, nz, 1, 0);
    push_vertex(g.vertices, cx, cy, cz, nx, ny, nz, 1, 1);
    push_vertex(g.vertices, dx, dy, dz, nx, ny, nz, 0, 1);
    g.indices.insert(g.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
  };
  face(-1, 0, 0, -1, -1, -1, -1, 1, -1, -1, 1, 1, -1, -1, 1);   // -X
  face(1, 0, 0, 1, -1, -1, 1, -1, 1, 1, 1, 1, 1, 1, -1);        // +X
  face(0, -1, 0, -1, -1, -1, -1, -1, 1, 1, -1, 1, 1, -1, -1);   // -Y
  face(0, 1, 0, -1, 1, -1, -1, 1, 1, 1, 1, 1, 1, 1, -1);        // +Y
  face(0, 0, -1, -1, -1, -1, -1, 1, -1, 1, 1, -1, 1, -1, -1);   // -Z
  face(0, 0, 1, -1, -1, 1, -1, 1, 1, 1, 1, 1, 1, -1, 1);        // +Z
  return g;
}

Geometry make_sphere(float radius, int stacks, int slices) {
  Geometry g;
  for (int i = 0; i <= stacks; ++i) {
    float phi = math::radians(90.0f) - math::radians(180.0f) * i / stacks;  // +Y to -Y
    float y = radius * std::sin(phi);
    float r = radius * std::cos(phi);
    for (int j = 0; j <= slices; ++j) {
      float theta = math::PI * 2.0f * j / slices;
      float x = r * std::cos(theta);
      float z = r * std::sin(theta);
      float ny = std::sin(phi);
      float nx = std::cos(phi) * std::cos(theta);
      float nz = std::cos(phi) * std::sin(theta);
      push_vertex(g.vertices, x, y, z, nx, ny, nz, (float)j / slices, (float)i / stacks);
    }
  }
  for (int i = 0; i < stacks; ++i) {
    for (int j = 0; j < slices; ++j) {
      uint32_t a = (uint32_t)(i * (slices + 1) + j);
      uint32_t b = (uint32_t)((i + 1) * (slices + 1) + j);
      uint32_t c = (uint32_t)((i + 1) * (slices + 1) + j + 1);
      uint32_t d = (uint32_t)(i * (slices + 1) + j + 1);
      g.indices.insert(g.indices.end(), {a, b, d, b, c, d});
    }
  }
  return g;
}

Geometry make_torus(float major, float minor, int major_seg, int minor_seg) {
  Geometry g;
  for (int i = 0; i <= major_seg; ++i) {
    float u = math::PI * 2.0f * i / major_seg;
    for (int j = 0; j <= minor_seg; ++j) {
      float v = math::PI * 2.0f * j / minor_seg;
      float x = (major + minor * std::cos(v)) * std::cos(u);
      float y = minor * std::sin(v);
      float z = (major + minor * std::cos(v)) * std::sin(u);
      float nx = std::cos(v) * std::cos(u);
      float ny = std::sin(v);
      float nz = std::cos(v) * std::sin(u);
      push_vertex(g.vertices, x, y, z, nx, ny, nz, (float)i / major_seg, (float)j / minor_seg);
    }
  }
  for (int i = 0; i < major_seg; ++i) {
    for (int j = 0; j < minor_seg; ++j) {
      uint32_t a = (uint32_t)(i * (minor_seg + 1) + j);
      uint32_t b = (uint32_t)(((i + 1) % major_seg) * (minor_seg + 1) + j);
      uint32_t c = (uint32_t)(((i + 1) % major_seg) * (minor_seg + 1) + j + 1);
      uint32_t d = (uint32_t)(i * (minor_seg + 1) + j + 1);
      g.indices.insert(g.indices.end(), {a, b, c, a, c, d});
    }
  }
  return g;
}

Geometry make_grid(float half_size, int divisions) {
  Geometry g;
  float step = 2.0f * half_size / divisions;
  for (int i = 0; i < divisions; ++i) {
    for (int j = 0; j < divisions; ++j) {
      float x0 = -half_size + i * step, x1 = x0 + step;
      float z0 = -half_size + j * step, z1 = z0 + step;
      uint32_t base = (uint32_t)g.vertices.size() / 8;
      push_vertex(g.vertices, x0, 0, z0, 0, 1, 0, 0, 0);
      push_vertex(g.vertices, x1, 0, z0, 0, 1, 0, 1, 0);
      push_vertex(g.vertices, x1, 0, z1, 0, 1, 0, 1, 1);
      push_vertex(g.vertices, x0, 0, z1, 0, 1, 0, 0, 1);
      g.indices.insert(g.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    }
  }
  return g;
}

Geometry make_quad(float half_w, float half_h) {
  Geometry g;
  push_vertex(g.vertices, -half_w, -half_h, 0, 0, 0, 1, 0, 1);
  push_vertex(g.vertices, half_w, -half_h, 0, 0, 0, 1, 1, 1);
  push_vertex(g.vertices, half_w, half_h, 0, 0, 0, 1, 1, 0);
  push_vertex(g.vertices, -half_w, half_h, 0, 0, 0, 1, 0, 0);
  g.indices = {0, 1, 2, 0, 2, 3};
  return g;
}

// --- textures -----------------------------------------------------------------

namespace {
void put_pixel(std::vector<uint8_t>& t, int x, int y, int size, uint8_t r, uint8_t g, uint8_t b,
               uint8_t a = 255) {
  size_t i = ((size_t)y * size + x) * 4;
  t[i] = r; t[i + 1] = g; t[i + 2] = b; t[i + 3] = a;
}
}  // namespace

std::vector<uint8_t> make_checker(int size, int tiles, uint8_t r0, uint8_t g0, uint8_t b0,
                                  uint8_t r1, uint8_t g1, uint8_t b1) {
  std::vector<uint8_t> t((size_t)size * size * 4);
  int tile = size / tiles;
  for (int y = 0; y < size; ++y)
    for (int x = 0; x < size; ++x) {
      bool on = ((x / tile) + (y / tile)) % 2 == 0;
      put_pixel(t, x, y, size, on ? r0 : r1, on ? g0 : g1, on ? b0 : b1);
    }
  return t;
}

std::vector<uint8_t> make_radial_glow(int size, uint8_t r, uint8_t g, uint8_t b) {
  std::vector<uint8_t> t((size_t)size * size * 4);
  float c = (size - 1) / 2.0f;
  float maxd = std::sqrt(2.0f) * c;
  for (int y = 0; y < size; ++y)
    for (int x = 0; x < size; ++x) {
      float d = std::sqrt((x - c) * (x - c) + (y - c) * (y - c)) / maxd;
      float f = std::pow(std::max(0.0f, 1.0f - d), 1.8f);
      put_pixel(t, x, y, size, (uint8_t)(r * f), (uint8_t)(g * f), (uint8_t)(b * f),
                (uint8_t)(255 * f));
    }
  return t;
}

std::vector<uint8_t> make_gradient(int size, uint8_t tr, uint8_t tg, uint8_t tb, uint8_t br,
                                   uint8_t bg, uint8_t bb) {
  std::vector<uint8_t> t((size_t)size * size * 4);
  for (int y = 0; y < size; ++y) {
    float f = (float)y / (size - 1);
    for (int x = 0; x < size; ++x)
      put_pixel(t, x, y, size, (uint8_t)(tr + (br - tr) * f), (uint8_t)(tg + (bg - tg) * f),
                (uint8_t)(tb + (bb - tb) * f));
  }
  return t;
}

std::vector<uint8_t> make_orb(int size, uint8_t r, uint8_t g, uint8_t b) {
  std::vector<uint8_t> t((size_t)size * size * 4);
  float c = (size - 1) / 2.0f;
  float maxd = c * 0.98f;
  for (int y = 0; y < size; ++y)
    for (int x = 0; x < size; ++x) {
      float dx = x - c, dy = y - c;
      float d = std::sqrt(dx * dx + dy * dy) / maxd;
      if (d > 1.0f) { put_pixel(t, x, y, size, 0, 0, 0, 0); continue; }
      float sh = std::pow(1.0f - d, 0.6f);  // fake 3D shading toward center-top
      float hl = std::max(0.0f, 1.0f - std::sqrt((dx + c * 0.35f) * (dx + c * 0.35f) +
                                                 (dy + c * 0.35f) * (dy + c * 0.35f)) /
                                                     (maxd * 0.9f));
      float f = sh * 0.75f + hl * 0.55f;
      put_pixel(t, x, y, size, (uint8_t)std::min(255.0f, r * f + 40 * hl),
                (uint8_t)std::min(255.0f, g * f + 40 * hl),
                (uint8_t)std::min(255.0f, b * f + 40 * hl), 255);
    }
  return t;
}

std::vector<uint8_t> make_player(int size) {
  std::vector<uint8_t> t((size_t)size * size * 4);
  float u = (size - 1.0f) / 64.0f;  // design space is 64x64
  auto inside_rrect = [&](float x, float y, float x0, float y0, float x1, float y1, float rad) {
    if (x < x0 || x > x1 || y < y0 || y > y1) return false;
    float cx = std::max(x0 + rad, std::min(x, x1 - rad));
    float cy = std::max(y0 + rad, std::min(y, y1 - rad));
    float dx = x - cx, dy = y - cy;
    return dx * dx + dy * dy <= rad * rad;
  };
  for (int py = 0; py < size; ++py) {
    for (int px = 0; px < size; ++px) {
      float x = (px + 0.5f) / u, y = (py + 0.5f) / u;
      // antenna
      if (x >= 29 && x < 35 && y >= 4 && y < 11)
        put_pixel(t, px, py, size, 40, 110, 120);
      else if (x >= 28 && x < 36 && y >= 1 && y < 6)
        put_pixel(t, px, py, size, 120, 240, 255);
      // feet
      else if (inside_rrect(x, y, 14, 49, 27, 59, 3) || inside_rrect(x, y, 37, 49, 50, 59, 3))
        put_pixel(t, px, py, size, 34, 58, 72);
      // body (rounded rect with top-light shading)
      else if (inside_rrect(x, y, 8, 9, 56, 51, 11)) {
        float f = 1.0f - ((y - 9) / 42.0f) * 0.4f;
        put_pixel(t, px, py, size, (uint8_t)(70 * f + 20), (uint8_t)(190 * f + 15),
                  (uint8_t)(210 * f + 15));
      }
    }
  }
  // visor + eyes on top
  for (int py = 0; py < size; ++py) {
    for (int px = 0; px < size; ++px) {
      float x = (px + 0.5f) / u, y = (py + 0.5f) / u;
      if (inside_rrect(x, y, 14, 17, 50, 33, 5))
        put_pixel(t, px, py, size, 12, 20, 38);
      if ((x >= 19 && x < 27 && y >= 21 && y < 30) || (x >= 37 && x < 45 && y >= 21 && y < 30))
        put_pixel(t, px, py, size, 150, 245, 255);
    }
  }
  return t;
}

std::vector<uint8_t> make_grid_tex(int size, uint8_t br, uint8_t bg, uint8_t bb, uint8_t lr,
                                   uint8_t lg, uint8_t lb, int lines) {
  std::vector<uint8_t> t((size_t)size * size * 4);
  int step = size / lines;
  for (int y = 0; y < size; ++y)
    for (int x = 0; x < size; ++x) {
      bool line = (x % step < 2) || (y % step < 2);
      put_pixel(t, x, y, size, line ? lr : br, line ? lg : bg, line ? lb : bb);
    }
  return t;
}

}  // namespace fc
