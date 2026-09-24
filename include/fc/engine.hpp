#pragma once
// ForgeCore Engine — application bootstrap, main loop, subsystems.
//
// Typical usage:
//   fc::EngineConfig cfg; cfg.window.title = "Demo";
//   fc::Engine engine(cfg);
//   engine.add_system<fc::TransformSystem>();
//   engine.add_system<fc::Physics2DSystem>();
//   engine.add_render_system<fc::Render3DSystem>();
//   engine.add_render_system<fc::Render2DSystem>();
//   engine.run([&](fc::Engine& e) { /* build scene */ });

#include "audio.hpp"
#include "components.hpp"
#include "input.hpp"
#include "render.hpp"
#include "scene.hpp"
#include "systems.hpp"
#include "window.hpp"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace fc {

struct FrameData {
  FrameLights lights;
  float fps = 0;
  float frame_ms = 0;
  float render_ms = 0;  // raw render-pass time of the previous frame (benchmarks)
  int frame_index = 0;
};

struct EngineConfig {
  WindowConfig window;
  // "auto" | "soft" | "gl"
  std::string renderer = "auto";
  // CPU rasterizer worker hint: 0 = auto (min(4, hardware cores)),
  // 1 = serial. Ignored by non-CPU backends.
  int renderer_threads = 0;
  // Headless preview streaming (/dev/shm frame + input + events).
  bool stream = false;
  std::string stream_dir = "/dev/shm";
  // Stop after N presented frames (0 = run until closed).
  int frame_limit = 0;
  // Write the final frame to a PPM image (for CI / screenshots).
  std::string screenshot_path;
  float target_fps = 60.0f;
};

class Engine {
 public:
  explicit Engine(EngineConfig cfg);
  ~Engine();

  // Update systems (fixed timestep) — order matters.
  template <class T>
  void add_system() {
    updates_.push_back(std::make_unique<T>());
  }
  void add_system(std::unique_ptr<System> s) { updates_.push_back(std::move(s)); }
  // Render systems (draw pass) — order matters.
  template <class T>
  void add_render_system() {
    renders_.push_back(std::make_unique<T>());
  }
  void add_render_system(std::unique_ptr<System> s) { renders_.push_back(std::move(s)); }

  void run(std::function<void(Engine&)> on_init);
  void request_shutdown() { shutdown_requested_ = true; }

  // Append a JSON event to the streaming channel (no-op when not streaming).
  void append_stream_event(const std::string& json);

  Scene& scene() { return scene_; }
  Input& input() { return input_; }
  Audio& audio() { return audio_; }
  FrameData& frame() { return frame_; }
  Renderer* renderer() { return renderer_.get(); }
  const Renderer* renderer() const { return renderer_.get(); }
  int window_width() { return window_ ? window_->width() : cfg_.window.width; }
  int window_height() { return window_ ? window_->height() : cfg_.window.height; }
  const std::string& renderer_name() const { return renderer_name_; }
  const std::string& window_backend_name() const { return window_backend_name_; }

 private:
  void render_frame();

  EngineConfig cfg_;
  Scene scene_;
  Input input_;
  Audio audio_;
  FrameData frame_;
  std::unique_ptr<Renderer> renderer_;
  std::unique_ptr<Window> window_;
  std::string renderer_name_, window_backend_name_;
  std::vector<std::unique_ptr<System>> updates_, renders_;
  bool shutdown_requested_ = false;
  double acc_ = 0;
  class StreamChannel* stream_ = nullptr;
  std::unique_ptr<class StreamChannel> stream_impl_;
};

}  // namespace fc
