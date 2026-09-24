#include "fc/window.hpp"
#include "../render/gl/gl_loader.hpp"
#include "glx_window.hpp"

#include <cstdio>
#include <cstdlib>

namespace fc {

std::unique_ptr<Window> create_window(const WindowConfig& cfg, bool allow_gl) {
  const char* display = std::getenv("DISPLAY");
  if (allow_gl && display && *display) {
    gl::GL g;
    std::string lib;
    if (g.load("libGL.so.1") || g.load("libGL.so")) {
      auto win = std::make_unique<GLXWindow>(cfg, g);
      if (win->open()) return win;
      std::fprintf(stderr, "[forgecore] GLX window failed (%s); falling back to headless\n",
                   win->error());
    }
  }
  return std::make_unique<HeadlessWindow>(cfg);
}

}  // namespace fc
