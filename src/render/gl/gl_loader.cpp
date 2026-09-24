#include "gl_loader.hpp"

#ifdef FC_PLATFORM_UNIX
#include <dlfcn.h>
#endif
#include <cstring>

namespace fc::gl {

namespace {
struct Sym {
  const char* name;
  void* GL::*member;
};

const Sym kSymbols[] = {
    {"glGenBuffers", &GL::glGenBuffers_p},       {"glDeleteBuffers", &GL::glDeleteBuffers_p},
    {"glBindBuffer", &GL::glBindBuffer_p},       {"glBufferData", &GL::glBufferData_p},
    {"glGenVertexArrays", &GL::glGenVertexArrays_p},
    {"glDeleteVertexArrays", &GL::glDeleteVertexArrays_p},
    {"glBindVertexArray", &GL::glBindVertexArray_p},
    {"glCreateShader", &GL::glCreateShader_p},   {"glDeleteShader", &GL::glDeleteShader_p},
    {"glShaderSource", &GL::glShaderSource_p},   {"glCompileShader", &GL::glCompileShader_p},
    {"glGetShaderiv", &GL::glGetShaderiv_p},     {"glGetShaderInfoLog", &GL::glGetShaderInfoLog_p},
    {"glCreateProgram", &GL::glCreateProgram_p}, {"glDeleteProgram", &GL::glDeleteProgram_p},
    {"glAttachShader", &GL::glAttachShader_p},   {"glLinkProgram", &GL::glLinkProgram_p},
    {"glUseProgram", &GL::glUseProgram_p},       {"glGetProgramiv", &GL::glGetProgramiv_p},
    {"glGetProgramInfoLog", &GL::glGetProgramInfoLog_p},
    {"glGetUniformLocation", &GL::glGetUniformLocation_p},
    {"glUniformMatrix4fv", &GL::glUniformMatrix4fv_p},
    {"glUniform3fv", &GL::glUniform3fv_p},       {"glUniform4fv", &GL::glUniform4fv_p},
    {"glUniform1f", &GL::glUniform1f_p},         {"glUniform1i", &GL::glUniform1i_p},
    {"glGenTextures", &GL::glGenTextures_p},     {"glDeleteTextures", &GL::glDeleteTextures_p},
    {"glBindTexture", &GL::glBindTexture_p},     {"glTexImage2D", &GL::glTexImage2D_p},
    {"glTexParameteri", &GL::glTexParameteri_p}, {"glActiveTexture", &GL::glActiveTexture_p},
    {"glViewport", &GL::glViewport_p},           {"glScissor", &GL::glScissor_p},
    {"glClearColor", &GL::glClearColor_p},       {"glClear", &GL::glClear_p},
    {"glEnable", &GL::glEnable_p},               {"glDisable", &GL::glDisable_p},
    {"glDrawElements", &GL::glDrawElements_p},   {"glVertexAttribPointer", &GL::glVertexAttribPointer_p},
    {"glEnableVertexAttribArray", &GL::glEnableVertexAttribArray_p},
    {"glReadPixels", &GL::glReadPixels_p},       {"glGetString", &GL::glGetString_p},
    {"glFlush", &GL::glFlush_p},                 {"glFinish", &GL::glFinish_p},
    {"glXChooseFBConfig", &GL::glXChooseFBConfig_p},
    {"glXCreateContext", &GL::glXCreateContext_p},
    {"glXDestroyContext", &GL::glXDestroyContext_p},
    {"glXMakeCurrent", &GL::glXMakeCurrent_p},
    {"glXSwapBuffers", &GL::glXSwapBuffers_p},
    {"glXGetVisualFromFBConfig", &GL::glXGetVisualFromFBConfig_p},
};
}  // namespace

namespace {
void try_symbols(void* h, GL* g, const Sym* syms, int n) {
  for (int i = 0; i < n; ++i)
    (g->*syms[i].member) = dlsym(h, syms[i].name);
}
}  // namespace

bool GL::load(const std::string& lib) {
#ifdef FC_PLATFORM_UNIX
  void* h = dlopen(lib.c_str(), RTLD_NOW | RTLD_GLOBAL);
  if (!h) return false;
  int core_n = 0;
  while (core_n < (int)(sizeof(kSymbols) / sizeof(Sym)) &&
         !std::strncmp(kSymbols[core_n].name, "glX", 3))
    ++core_n;
  try_symbols(h, this, kSymbols, core_n);
  // GLX lives in libGLX under libglvnd; retry it from there when missing.
  if (!glXMakeCurrent_p) {
    void* hx = dlopen("libGLX.so.1", RTLD_NOW | RTLD_GLOBAL);
    if (hx) try_symbols(hx, this, kSymbols + core_n, (int)(sizeof(kSymbols) / sizeof(Sym)) - core_n);
  }
  return core_loaded();
#else
  (void)lib;
  return false;
#endif
}

bool GL::probe(std::string* found_lib, std::string* gl_version) {
  GL g;
  const char* candidates[] = {"libGL.so.1", "libGL.so", "libOpenGL.so.0"};
  for (const char* c : candidates) {
    if (g.load(c)) {
      if (found_lib) *found_lib = c;
      if (gl_version && g.glGetString_p) {
        auto f = g.fn<const char* (*)(GLenum)>(g.glGetString_p);
        *gl_version = f ? f(GL_VERSION) : "unknown";
      }
      return true;
    }
  }
  return false;
}

}  // namespace fc::gl
