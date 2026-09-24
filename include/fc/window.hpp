#pragma once
// ForgeCore window backends.

#include "input.hpp"
#include <memory>
#include <string>

namespace fc {

struct WindowConfig {
  int width = 1280;
  int height = 720;
  std::string title = "ForgeCore Engine";
  bool vsync = true;
};

class Window {
 public:
  virtual ~Window() = default;
  virtual bool open() = 0;
  virtual void close() = 0;
  virtual bool should_close() const = 0;
  virtual void poll_events(Input& input) = 0;
  virtual void present() = 0;
  virtual int width() const = 0;
  virtual int height() const = 0;
  virtual const char* backend_name() const = 0;
};

// Headless window: no display needed; frames land in the software
// framebuffer (readable via Renderer::read_frame_rgb8). Input arrives
// through the streaming channel when enabled.
class HeadlessWindow : public Window {
 public:
  explicit HeadlessWindow(WindowConfig cfg) : cfg_(cfg) {}
  bool open() override { return true; }
  void close() override { closed_ = true; }
  bool should_close() const override { return closed_ || (frame_limit_ > 0 && frame_count_ >= frame_limit_); }
  void poll_events(Input&) override {}
  void present() override { ++frame_count_; }
  int width() const override { return cfg_.width; }
  int height() const override { return cfg_.height; }
  const char* backend_name() const override { return "headless"; }

  void set_frame_limit(int n) { frame_limit_ = n; }
  int frames_presented() const { return frame_count_; }

 private:
  WindowConfig cfg_;
  bool closed_ = false;
  int frame_limit_ = 0;
  int frame_count_ = 0;
};

// Factory: X11/GLX window when a display + GL implementation are present,
// otherwise headless.
std::unique_ptr<Window> create_window(const WindowConfig& cfg, bool allow_gl);

}  // namespace fc
