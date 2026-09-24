#pragma once
// ForgeCore built-in geometry + procedural textures.
// Vertex layout: {pos xyz, normal xyz, uv (u,v)} — 8 floats, uint32 indices.

#include "render.hpp"
#include <string>
#include <vector>

namespace fc {

struct Geometry {
  std::vector<float> vertices;
  std::vector<uint32_t> indices;
};

Geometry make_cube();
Geometry make_sphere(float radius, int stacks, int slices);
Geometry make_torus(float major, float minor, int major_seg, int minor_seg);
Geometry make_grid(float half_size, int divisions);  // XZ plane at y=0
Geometry make_quad(float half_w, float half_h);       // XY plane facing +Z (2D sprite)

// --- procedural RGBA textures (power-of-two) ---------------------------------

std::vector<uint8_t> make_checker(int size, int tiles, uint8_t r0, uint8_t g0, uint8_t b0,
                                  uint8_t r1, uint8_t g1, uint8_t b1);
std::vector<uint8_t> make_radial_glow(int size, uint8_t r, uint8_t g, uint8_t b);
std::vector<uint8_t> make_gradient(int size, uint8_t top_r, uint8_t top_g, uint8_t top_b,
                                   uint8_t bot_r, uint8_t bot_g, uint8_t bot_b);
std::vector<uint8_t> make_orb(int size, uint8_t r, uint8_t g, uint8_t b);
std::vector<uint8_t> make_player(int size);  // little 2D bot (body, visor, eyes, feet)
std::vector<uint8_t> make_grid_tex(int size, uint8_t bg_r, uint8_t bg_g, uint8_t bg_b,
                                   uint8_t line_r, uint8_t line_g, uint8_t line_b, int lines);

}  // namespace fc
