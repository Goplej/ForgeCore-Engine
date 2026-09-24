#pragma once
// ForgeCore UI — immediate-mode 2D overlay (text, panels, buttons, bars).
//
// Positions are in *screen pixels of the camera's viewport*: call begin()
// with the 2D camera's proj*view (1 world unit = 1 pixel) and draw. The
// engine's ortho 2D camera makes this 1:1.

#include "input.hpp"
#include "render.hpp"

namespace fc {

class UI {
 public:
  UI(Renderer* r, Input* input);

  // pv = proj * view of the 2D camera; (ox, oy) = viewport origin so mouse
  // coordinates map to the overlay's local pixel space; (hw, hh) = the
  // camera's ortho half-extents (UI pixel space, 1px = 1 world unit).
  void begin(const Mat4& pv, float ox, float oy, float hw, float hh);
  void end();

  void text(const std::string& s, float x, float y, float px, const Vec4& color);
  float text_width(const std::string& s, float px) const;
  void rect(float x, float y, float w, float h, const Vec4& color);
  void panel(float x, float y, float w, float h, const Vec4& bg, const Vec4& border);
  bool button(const std::string& label, float x, float y, float w, float h, const Vec4& idle,
              const Vec4& hover, float text_px = 12.0f);
  void bar(float x, float y, float w, float h, float frac, const Vec4& fg, const Vec4& bg);

 private:
  void quad_px(float x, float y, float w, float h, const Vec4& color);

  Renderer* r_;
  Input* in_;
  Mat4 pv_ = Mat4::identity();
  float ox_ = 0, oy_ = 0, hw_ = 0, hh_ = 0;
  bool begun_ = false;
};

}  // namespace fc
