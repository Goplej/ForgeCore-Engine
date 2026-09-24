#pragma once
// ForgeCore GL loader — dlopen/dlsym function table.
// No GL headers are needed to build the engine: the GL implementation is
// probed and loaded at runtime (libGL / libGLX), which keeps the engine
// buildable on machines without a graphics stack and lets the same binary
// fall back to the CPU rasterizer.

#include <cstdint>
#include <string>

namespace fc::gl {

// --- minimal GL types (ABI-compatible with the real headers) ---------------
using GLenum = unsigned int;
using GLuint = unsigned int;
using GLint = int;
using GLsizei = int;
using GLboolean = unsigned char;
using GLfloat = float;
using GLchar = char;
using GLvoid = void;
using GLuint64 = unsigned long long;
using GLbitfield = unsigned int;
using GLsizeiptr = long long;

#define GL_TRIANGLES 0x0004
#define GL_UNSIGNED_INT 0x1405
#define GL_ARRAY_BUFFER 0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_TEXTURE0 0x84C0
#define GL_TEXTURE_2D 0x0D17
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_LINEAR 0x2601
#define GL_NEAREST 0x2600
#define GL_REPEAT 0x2901
#define GL_RGBA 0x1908
#define GL_RGB 0x1907
#define GL_UNSIGNED_BYTE 0x1401
#define GL_DEPTH_TEST 0x0B71
#define GL_BLEND 0x0BE2
#define GL_SCISSOR_TEST 0x0C11
#define GL_DEPTH_BUFFER_BIT 0x0100
#define GL_COLOR_BUFFER_BIT 0x00004
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_VERSION 0x1F02
#define GL_VERTEX_ATTRIB_ARRAY_0 0x84E0

#define FC_GL_ENUM 0x1903
#define FC_GL_FLOAT 0x1406
#define FC_GL_INT 0x1404

// --- function pointer table --------------------------------------------------

struct GL {
  // buffer / VAO
  void* glGenBuffers_p = nullptr;
  void* glDeleteBuffers_p = nullptr;
  void* glBindBuffer_p = nullptr;
  void* glBufferData_p = nullptr;
  void* glGenVertexArrays_p = nullptr;
  void* glDeleteVertexArrays_p = nullptr;
  void* glBindVertexArray_p = nullptr;
  // shaders
  void* glCreateShader_p = nullptr;
  void* glDeleteShader_p = nullptr;
  void* glShaderSource_p = nullptr;
  void* glCompileShader_p = nullptr;
  void* glGetShaderiv_p = nullptr;
  void* glGetShaderInfoLog_p = nullptr;
  void* glCreateProgram_p = nullptr;
  void* glDeleteProgram_p = nullptr;
  void* glAttachShader_p = nullptr;
  void* glLinkProgram_p = nullptr;
  void* glUseProgram_p = nullptr;
  void* glGetProgramiv_p = nullptr;
  void* glGetProgramInfoLog_p = nullptr;
  void* glGetUniformLocation_p = nullptr;
  // uniforms
  void* glUniformMatrix4fv_p = nullptr;
  void* glUniform3fv_p = nullptr;
  void* glUniform4fv_p = nullptr;
  void* glUniform1f_p = nullptr;
  void* glUniform1i_p = nullptr;
  // textures
  void* glGenTextures_p = nullptr;
  void* glDeleteTextures_p = nullptr;
  void* glBindTexture_p = nullptr;
  void* glTexImage2D_p = nullptr;
  void* glTexParameteri_p = nullptr;
  void* glActiveTexture_p = nullptr;
  // state / frame
  void* glViewport_p = nullptr;
  void* glScissor_p = nullptr;
  void* glClearColor_p = nullptr;
  void* glClear_p = nullptr;
  void* glEnable_p = nullptr;
  void* glDisable_p = nullptr;
  void* glDrawElements_p = nullptr;
  void* glVertexAttribPointer_p = nullptr;
  void* glEnableVertexAttribArray_p = nullptr;
  void* glReadPixels_p = nullptr;
  void* glGetString_p = nullptr;
  void* glFlush_p = nullptr;
  void* glFinish_p = nullptr;

  // GLX (may be absent on EGL-only systems)
  void* glXChooseFBConfig_p = nullptr;
  void* glXCreateContext_p = nullptr;
  void* glXDestroyContext_p = nullptr;
  void* glXMakeCurrent_p = nullptr;
  void* glXSwapBuffers_p = nullptr;
  void* glXGetVisualFromFBConfig_p = nullptr;

  // Load from a candidate library path; returns true if at least the core
  // GL entry points resolved.
  bool load(const std::string& lib);
  bool core_loaded() const {
    return glClear_p && glDrawElements_p && glCreateProgram_p && glCreateShader_p &&
           glUseProgram_p && glBindBuffer_p && glTexImage2D_p;
  }

  template <class F>
  F fn(const void* p) const { return reinterpret_cast<F>(p); }

  static bool probe(std::string* found_lib, std::string* gl_version);
};

}  // namespace fc::gl
