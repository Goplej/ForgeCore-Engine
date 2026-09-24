#pragma once
#include "fc/window.hpp"
#include "../render/gl/gl_loader.hpp"
#include "xlib_min.hpp"
#include <string>

namespace fc {

// Desktop window: X11 + GLX, both loaded via dlopen (no build-time deps).
// Runs where a real display server and GL implementation exist.
class GLXWindow : public Window {
 public:
  GLXWindow(WindowConfig cfg, const gl::GL& gl) : cfg_(cfg), gl_(gl) {}
  ~GLXWindow() override { close(); }

  bool open() override;
  void close() override;
  bool should_close() const override { return close_requested_; }
  void poll_events(Input& input) override;
  void present() override;
  int width() const override { return w_; }
  int height() const override { return h_; }
  const char* backend_name() const override { return "glx/x11"; }
  const char* error() const { return error_.c_str(); }

 private:
  void handle_event(x11::XEvent& ev, Input& input);
  static Key map_keycode(int code, bool shifted);

  WindowConfig cfg_;
  const gl::GL& gl_;
  x11::Display* dpy_ = nullptr;
  x11::Window root_ = 0, win_ = 0;
  x11::XVisualInfo vis_{};
  x11::GLXContext ctx_ = nullptr;
  int w_ = 0, h_ = 0;
  bool close_requested_ = false;
  std::string error_;
  int pending_width_ = 0, pending_height_ = 0;
};

}  // namespace fc
