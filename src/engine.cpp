#include "fc/engine.hpp"
#include "platform/stream_channel.hpp"
#include "fc/systems.hpp"
#include "render/gl/gl_loader.hpp"
#include "render/soft/soft_renderer.hpp"
#include "render/gl/gl_renderer.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>

namespace fc {

namespace {
double now_s() {
  return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

void write_ppm(const std::string& path, int w, int h, const uint8_t* rgb) {
  FILE* f = fopen(path.c_str(), "wb");
  if (!f) return;
  fprintf(f, "P6\n%d %d\n255\n", w, h);
  fwrite(rgb, 1, (size_t)w * h * 3, f);
  fclose(f);
}
}  // namespace

Engine::Engine(EngineConfig cfg) : cfg_(std::move(cfg)) {
  // 1. Window: try X11/GLX when a display + GL implementation exist.
  const char* display = std::getenv("DISPLAY");
  bool have_gl = false;
  std::string gl_lib;
  if (cfg_.renderer != "soft") {
    gl::GL probe;
    if (probe.load("libGL.so.1") || probe.load("libGL.so")) {
      have_gl = true;
    }
  }
  bool want_gl_window = have_gl && cfg_.renderer != "soft" && display && *display;
  window_ = create_window(cfg_.window, want_gl_window);
  window_backend_name_ = window_->backend_name();
  if (cfg_.frame_limit > 0) {
    if (auto* hw = dynamic_cast<HeadlessWindow*>(window_.get())) hw->set_frame_limit(cfg_.frame_limit);
  }

  // 2. Renderer: GL only makes sense with a GLX window (current context).
  bool use_gl = have_gl && window_backend_name_ == std::string("glx/x11");
  if (cfg_.renderer == "gl" && !use_gl)
    std::fprintf(stderr,
                 "[forgecore] GL backend requested but no X11 display + GL context is "
                 "available; using the CPU rasterizer instead\n");
  if (use_gl) {
    gl::GL g;
    if (!g.load("libGL.so.1")) g.load("libGL.so");
    renderer_ = std::make_unique<GLRenderer>(g);
    if (!static_cast<GLRenderer*>(renderer_.get())->ok()) {
      std::fprintf(stderr, "[forgecore] GL program build failed; using CPU rasterizer\n");
      renderer_.reset();
      use_gl = false;
    }
  }
  if (!use_gl) renderer_ = std::make_unique<SoftRenderer>();
  renderer_->set_threads(cfg_.renderer_threads);
  renderer_name_ = renderer_->name();

  // 3. Audio + streaming
  audio_.init();
  if (const char* wav = std::getenv("FC_AUDIO_WAV"); wav && *wav)
    audio_.set_sink(std::make_unique<WavSink>(wav));
  if (cfg_.stream) {
    stream_impl_ = std::make_unique<StreamChannel>();
    stream_ = stream_impl_.get();
    if (!stream_->init(cfg_.stream_dir, window_->width(), window_->height())) {
      std::fprintf(stderr, "[forgecore] streaming unavailable in %s; continuing without\n",
                   cfg_.stream_dir.c_str());
      stream_ = nullptr;
    }
  }
}

Engine::~Engine() {
  audio_.shutdown();
  if (stream_) stream_->shutdown();
  if (window_) window_->close();
}

void Engine::append_stream_event(const std::string& json) {
  if (stream_) stream_->append_event(json);
}

void Engine::render_frame() {
  int w = window_->width(), h = window_->height();
  if (!renderer_->begin_frame(w, h)) return;
  for (auto& s : renders_) s->update(*this, 0.0f);
  renderer_->end_frame();
}

void Engine::run(std::function<void(Engine&)> on_init) {
  if (on_init) on_init(*this);

  const double step = 1.0 / 120.0;  // fixed update rate
  double last = now_s();
  acc_ = step;  // run one update step before the first frame is rendered
  std::vector<uint8_t> frame_buf;

  for (;;) {
    if (shutdown_requested_) break;
    window_->poll_events(input_);
    if (stream_) stream_->drain_input(input_);
    input_.begin_frame();

    double t = now_s();
    double dt = t - last;
    last = t;
    if (dt < 0) dt = 0;
    if (dt > 0.25) dt = 0.25;
    acc_ += dt;
    int steps = 0;
    while (acc_ >= step && steps < 8) {
      for (auto& s : updates_) s->update(*this, (float)step);
      acc_ -= step;
      ++steps;
    }
    input_.end_frame();

    auto t0 = std::chrono::steady_clock::now();
    auto r0 = std::chrono::steady_clock::now();
    render_frame();
    double render_ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - r0)
            .count();
    audio_.update((float)dt);
    double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0)
                    .count();
    frame_.frame_index++;
    frame_.render_ms = (float)render_ms;  // raw last-frame render time (benchmarks)
    frame_.frame_ms = frame_.frame_ms * 0.9f + (float)ms * 0.1f;
    frame_.fps = frame_.fps * 0.9f + (float)(1.0 / std::max(dt, 1e-6)) * 0.1f;

    int fw = window_->width(), fh = window_->height();
    if (frame_buf.size() != (size_t)fw * fh * 3) frame_buf.resize((size_t)fw * fh * 3);
    bool have_frame = renderer_->read_frame_rgb8(frame_buf.data(), fw, fh);
    if (stream_ && have_frame) stream_->publish_frame(frame_buf.data());

    window_->present();

    bool finished = cfg_.frame_limit > 0 && window_->should_close();
    if (!cfg_.screenshot_path.empty() && (finished || shutdown_requested_) && have_frame)
      write_ppm(cfg_.screenshot_path, fw, fh, frame_buf.data());

    if (window_->should_close() || shutdown_requested_) break;
    if (cfg_.target_fps > 0) {
      double elapsed = now_s() - t;
      double sleep_s = 1.0 / cfg_.target_fps - elapsed;
      if (sleep_s > 0.001)
        std::this_thread::sleep_for(std::chrono::duration<double>(sleep_s));
    }
  }

  if (window_) window_->close();
}

}  // namespace fc
