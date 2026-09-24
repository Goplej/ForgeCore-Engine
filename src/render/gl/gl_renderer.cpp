#include "gl_renderer.hpp"
#include "glsl.hpp"

#include <cstring>

namespace fc {

namespace {
using gl::GLenum;
using gl::GLuint;
using gl::GLint;
using gl::GLsizei;
using gl::GLboolean;
using gl::GLfloat;
using gl::GLchar;
using gl::GLvoid;
using gl::GLbitfield;
using gl::GLsizeiptr;
constexpr GLenum kStaticDraw = 0x88E4;
constexpr GLint kTrue = 1;
constexpr GLint kFalse = 0;

using glGenBuffersFn = void (*)(GLsizei, GLuint*);
using glDeleteBuffersFn = void (*)(GLsizei, const GLuint*);
using glBindBufferFn = void (*)(GLenum, GLuint);
using glBufferDataFn = void (*)(GLenum, GLsizeiptr, const void*, GLenum);
using glGenVertexArraysFn = void (*)(GLsizei, GLuint*);
using glDeleteVertexArraysFn = void (*)(GLsizei, const GLuint*);
using glBindVertexArrayFn = void (*)(GLuint);
using glCreateShaderFn = GLuint (*)(GLenum);
using glDeleteShaderFn = void (*)(GLuint);
using glShaderSourceFn = void (*)(GLuint, GLsizei, const GLchar* const*, const GLint*);
using glCompileShaderFn = void (*)(GLuint);
using glGetShaderivFn = void (*)(GLuint, GLenum, GLint*);
using glGetShaderInfoLogFn = void (*)(GLuint, GLsizei, GLsizei*, GLchar*);
using glCreateProgramFn = GLuint (*)(void);
using glDeleteProgramFn = void (*)(GLuint);
using glAttachShaderFn = void (*)(GLuint, GLuint);
using glLinkProgramFn = void (*)(GLuint);
using glUseProgramFn = void (*)(GLuint);
using glGetProgramivFn = void (*)(GLuint, GLenum, GLint*);
using glGetProgramInfoLogFn = void (*)(GLuint, GLsizei, GLsizei*, GLchar*);
using glGetUniformLocationFn = GLint (*)(GLuint, const GLchar*);
using glUniformMatrix4fvFn = void (*)(GLint, GLsizei, GLboolean, const GLfloat*);
using glUniform3fvFn = void (*)(GLint, GLsizei, const GLfloat*);
using glUniform4fvFn = void (*)(GLint, GLsizei, const GLfloat*);
using glUniform1fFn = void (*)(GLint, GLfloat);
using glUniform1iFn = void (*)(GLint, GLint);
using glGenTexturesFn = void (*)(GLsizei, GLuint*);
using glDeleteTexturesFn = void (*)(GLsizei, const GLuint*);
using glBindTextureFn = void (*)(GLenum, GLuint);
using glTexImage2DFn = void (*)(GLenum, GLint, GLenum, GLsizei, GLsizei, GLint, GLenum, GLenum,
                                const void*);
using glTexParameteriFn = void (*)(GLenum, GLenum, GLint);
using glActiveTextureFn = void (*)(GLenum);
using glViewportFn = void (*)(GLint, GLint, GLsizei, GLsizei);
using glScissorFn = void (*)(GLint, GLint, GLsizei, GLsizei);
using glClearColorFn = void (*)(GLfloat, GLfloat, GLfloat, GLfloat);
using glClearFn = void (*)(GLbitfield);
using glEnableFn = void (*)(GLenum);
using glDisableFn = void (*)(GLenum);
using glDrawElementsFn = void (*)(GLenum, GLsizei, GLenum, const void*);
using glVertexAttribPointerFn = void (*)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
using glEnableVertexAttribArrayFn = void (*)(GLuint);
using glReadPixelsFn = void (*)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*);
using glGetStringFn = const GLchar* (*)(GLenum);
using glFlushFn = void (*)(void);

template <class F>
F pf(const gl::GL& g, const void* p) {
  return reinterpret_cast<F>(p);
}
}  // namespace

GLRenderer::GLRenderer(const gl::GL& g) : g_(g) {
  std::string err;
  ok_ = build_program(prog_lit_, glsl::kLitVertex, glsl::kLitFragment, err) &&
        build_program(prog_unlit_, glsl::kUnlitVertex, glsl::kUnlitFragment, err);
  if (g_.glGetString_p) {
    auto f = g_.fn<const char* (*)(GLenum)>(g_.glGetString_p);
    if (f) gl_version_ = f(GL_VERSION);
  }
}

bool GLRenderer::build_program(GLuint& out, const char* vs_src, const char* fs_src,
                               std::string& err) {
  auto createShader = pf<glCreateShaderFn>(g_, g_.glCreateShader_p);
  auto srcFn = pf<glShaderSourceFn>(g_, g_.glShaderSource_p);
  auto compile = pf<glCompileShaderFn>(g_, g_.glCompileShader_p);
  auto getiv = pf<glGetShaderivFn>(g_, g_.glGetShaderiv_p);
  auto getLog = pf<glGetShaderInfoLogFn>(g_, g_.glGetShaderInfoLog_p);
  auto delShader = pf<glDeleteShaderFn>(g_, g_.glDeleteShader_p);
  auto createProg = pf<glCreateProgramFn>(g_, g_.glCreateProgram_p);
  auto attach = pf<glAttachShaderFn>(g_, g_.glAttachShader_p);
  auto link = pf<glLinkProgramFn>(g_, g_.glLinkProgram_p);
  auto getProgIv = pf<glGetProgramivFn>(g_, g_.glGetProgramiv_p);
  auto getProgLog = pf<glGetProgramInfoLogFn>(g_, g_.glGetProgramInfoLog_p);
  auto delProg = pf<glDeleteProgramFn>(g_, g_.glDeleteProgram_p);

  GLuint vs = createShader(GL_VERTEX_SHADER);
  const GLchar* src1 = vs_src;
  srcFn(vs, 1, &src1, nullptr);
  compile(vs);
  GLint status = 0;
  getiv(vs, GL_COMPILE_STATUS, &status);
  if (status != kTrue) {
    char log[1024] = {0};
    getLog(vs, sizeof(log), nullptr, log);
    err = "vertex shader: ";
    err += log;
    delShader(vs);
    return false;
  }
  GLuint fs = createShader(GL_FRAGMENT_SHADER);
  const GLchar* src2 = fs_src;
  srcFn(fs, 1, &src2, nullptr);
  compile(fs);
  getiv(fs, GL_COMPILE_STATUS, &status);
  if (status != kTrue) {
    char log[1024] = {0};
    getLog(fs, sizeof(log), nullptr, log);
    err = "fragment shader: ";
    err += log;
    delShader(vs);
    delShader(fs);
    return false;
  }
  GLuint prog = createProg();
  attach(prog, vs);
  attach(prog, fs);
  link(prog);
  getiv(prog, GL_LINK_STATUS, &status);
  if (status != kTrue) {
    char log[1024] = {0};
    getProgLog(prog, sizeof(log), nullptr, log);
    err = "link: ";
    err += log;
    delProg(prog);
    delShader(vs);
    delShader(fs);
    return false;
  }
  delShader(vs);
  delShader(fs);
  out = prog;
  return true;
}

GLint GLRenderer::loc(unsigned prog, const char* name) {
  auto f = pf<glGetUniformLocationFn>(g_, g_.glGetUniformLocation_p);
  return f(prog, name);
}

bool GLRenderer::begin_frame(int width, int height) {
  W_ = width;
  H_ = height;
  auto viewport = pf<glViewportFn>(g_, g_.glViewport_p);
  viewport(0, 0, width, height);
  pf<glEnableFn>(g_, g_.glEnable_p)(GL_SCISSOR_TEST);
  return true;
}

void GLRenderer::end_frame() {
  auto flush = pf<glFlushFn>(g_, g_.glFlush_p);
  if (flush) flush();
}

bool GLRenderer::read_frame_rgb8(uint8_t* out, int width, int height) {
  auto f = pf<glReadPixelsFn>(g_, g_.glReadPixels_p);
  if (!f) return false;
  f(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, out);
  return true;
}

void GLRenderer::set_scissor(int x, int y, int w, int h) {
  if (w <= 0 || h <= 0) { x = 0; y = 0; w = W_; h = H_; }
  // GL scissor origin is bottom-left; ours is top-left.
  auto f = pf<glScissorFn>(g_, g_.glScissor_p);
  f(x, H_ - y - h, w, h);
}

void GLRenderer::clear(float r, float g, float b, float a) {
  pf<glClearColorFn>(g_, g_.glClearColor_p)(r, g, b, a);
  pf<glClearFn>(g_, g_.glClear_p)(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void GLRenderer::set_depth_test(bool on) {
  if (on) pf<glEnableFn>(g_, g_.glEnable_p)(GL_DEPTH_TEST);
  else pf<glDisableFn>(g_, g_.glDisable_p)(GL_DEPTH_TEST);
}

void GLRenderer::set_blend(bool on) {
  if (on) {
    pf<glEnableFn>(g_, g_.glEnable_p)(GL_BLEND);
  } else {
    pf<glDisableFn>(g_, g_.glDisable_p)(GL_BLEND);
  }
}

void GLRenderer::use_program(ProgramKind kind) {
  cur_prog_ = kind == ProgramKind::Lit ? prog_lit_ : prog_unlit_;
  pf<glUseProgramFn>(g_, g_.glUseProgram_p)(cur_prog_);
}

void GLRenderer::set_matrix(const char* name, const Mat4& m) {
  auto f = pf<glUniformMatrix4fvFn>(g_, g_.glUniformMatrix4fv_p);
  f(loc(cur_prog_, name), 1, kFalse, m.m);
}

void GLRenderer::set_eye(const Vec3& eye) {
  auto f = pf<glUniform3fvFn>(g_, g_.glUniform3fv_p);
  f(loc(cur_prog_, "uEye"), 1, &eye.x);
}

void GLRenderer::set_lights(const FrameLights& L) {
  auto f3 = pf<glUniform3fvFn>(g_, g_.glUniform3fv_p);
  auto f1f = pf<glUniform1fFn>(g_, g_.glUniform1f_p);
  auto f1i = pf<glUniform1iFn>(g_, g_.glUniform1i_p);
  f3(loc(cur_prog_, "uDirDir"), 1, &L.dir.direction.x);
  f3(loc(cur_prog_, "uDirColor"), 1, &L.dir.color.x);
  for (int i = 0; i < 4; ++i) {
    f3(loc(cur_prog_, (std::string("uPointPos[") + std::to_string(i) + "]").c_str()), 1,
       &L.points[i].position.x);
    f3(loc(cur_prog_, (std::string("uPointColor[") + std::to_string(i) + "]").c_str()), 1,
       &L.points[i].color.x);
    f1f(loc(cur_prog_, (std::string("uPointRadius[") + std::to_string(i) + "]").c_str()),
        L.points[i].radius);
  }
  f1i(loc(cur_prog_, "uNumPoints"), L.numPoints);
}

void GLRenderer::set_material(const Material& m) {
  auto f3 = pf<glUniform3fvFn>(g_, g_.glUniform3fv_p);
  auto f4 = pf<glUniform4fvFn>(g_, g_.glUniform4fv_p);
  auto f1f = pf<glUniform1fFn>(g_, g_.glUniform1f_p);
  f4(loc(cur_prog_, "uBase"), 1, &m.baseColor.x);
  f4(loc(cur_prog_, "uEmissive"), 1, &m.emissive.x);
  f1f(loc(cur_prog_, "uShininess"), m.shininess);
  f1f(loc(cur_prog_, "uSpecular"), m.specularStrength);
  (void)f3;
}

uint32_t GLRenderer::create_texture(int w, int h, const uint8_t* rgba, bool) {
  auto gen = pf<glGenTexturesFn>(g_, g_.glGenTextures_p);
  auto bind = pf<glBindTextureFn>(g_, g_.glBindTexture_p);
  auto img = pf<glTexImage2DFn>(g_, g_.glTexImage2D_p);
  auto param = pf<glTexParameteriFn>(g_, g_.glTexParameteri_p);
  GLuint t = 0;
  gen(1, &t);
  bind(GL_TEXTURE_2D, t);
  img(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
  param(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  param(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  param(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  param(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  return t;
}

void GLRenderer::destroy_texture(uint32_t id) {
  if (!id) return;
  auto f = pf<glDeleteTexturesFn>(g_, g_.glDeleteTextures_p);
  f(1, &id);
}

void GLRenderer::bind_texture(uint32_t id) {
  bound_tex_ = id;
  auto bind = pf<glBindTextureFn>(g_, g_.glBindTexture_p);
  bind(GL_TEXTURE_2D, id);  // 0 = default (black) texture
}

MeshId GLRenderer::create_mesh(const Mesh& m) {
  auto gen = pf<glGenBuffersFn>(g_, g_.glGenBuffers_p);
  auto bind = pf<glBindBufferFn>(g_, g_.glBindBuffer_p);
  auto buf = pf<glBufferDataFn>(g_, g_.glBufferData_p);
  GLuint vbo = 0, ibo = 0;
  gen(1, &vbo);
  bind(GL_ARRAY_BUFFER, vbo);
  buf(GL_ARRAY_BUFFER, (GLsizeiptr)m.vertexCount * 8 * sizeof(float), m.vertices, kStaticDraw);
  gen(1, &ibo);
  bind(GL_ELEMENT_ARRAY_BUFFER, ibo);
  buf(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)m.indexCount * sizeof(uint32_t), m.indices,
      kStaticDraw);
  MeshId id = next_id_++;
  meshes_[id] = {vbo, ibo, m.indexCount};
  return id;
}

void GLRenderer::destroy_mesh(MeshId id) {
  auto it = meshes_.find(id);
  if (it == meshes_.end()) return;
  auto f = pf<glDeleteBuffersFn>(g_, g_.glDeleteBuffers_p);
  GLuint ids[2] = {it->second.vbo, it->second.ibo};
  f(2, ids);
  meshes_.erase(it);
}

MeshId GLRenderer::builtin_quad() {
  static float verts[4 * 8] = {
      -0.5f, -0.5f, 0, 0, 0, 1, 0, 1,
      0.5f, -0.5f, 0, 0, 0, 1, 1, 1,
      0.5f, 0.5f, 0, 0, 0, 1, 1, 0,
      -0.5f, 0.5f, 0, 0, 0, 1, 0, 0,
  };
  static uint32_t idx[6] = {0, 1, 2, 0, 2, 3};
  static MeshId id = 0;
  if (!id) {
    Mesh m{verts, 4, idx, 6};
    id = create_mesh(m);
  }
  return id;
}

void GLRenderer::draw(MeshId id) {
  auto it = meshes_.find(id);
  if (it == meshes_.end()) return;
  const MeshData& d = it->second;
  auto bind = pf<glBindBufferFn>(g_, g_.glBindBuffer_p);
  auto vptr = pf<glVertexAttribPointerFn>(g_, g_.glVertexAttribPointer_p);
  auto ven = pf<glEnableVertexAttribArrayFn>(g_, g_.glEnableVertexAttribArray_p);
  auto draw = pf<glDrawElementsFn>(g_, g_.glDrawElements_p);
  bind(GL_ARRAY_BUFFER, d.vbo);
  ven(0);
  vptr(0, 3, FC_GL_FLOAT, kFalse, 32, (const void*)0);
  ven(1);
  vptr(1, 3, FC_GL_FLOAT, kFalse, 32, (const void*)12);
  ven(2);
  vptr(2, 2, FC_GL_FLOAT, kFalse, 32, (const void*)20);
  bind(GL_ELEMENT_ARRAY_BUFFER, d.ibo);
  draw(GL_TRIANGLES, d.indexCount, GL_UNSIGNED_INT, (const void*)0);
}

}  // namespace fc
