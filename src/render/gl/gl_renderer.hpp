#pragma once
#include "fc/render.hpp"
#include "gl_loader.hpp"
#include <string>
#include <unordered_map>

namespace fc {

namespace {
using gl::GLuint;
using gl::GLint;
} // anon

// OpenGL 3.3 core backend. Requires a current GL context (provided by the
// window layer). All GL entry points come from the runtime loader.
class GLRenderer : public Renderer {
 public:
  explicit GLRenderer(const gl::GL& g);
  const char* name() const override { return gl_version_.c_str(); }
  const std::string& gl_version() const { return gl_version_; }
  bool ok() const { return ok_; }

  bool begin_frame(int width, int height) override;
  void end_frame() override;
  bool read_frame_rgb8(uint8_t* out, int width, int height) override;

  void set_scissor(int x, int y, int w, int h) override;
  void clear(float r, float g, float b, float a) override;
  void set_depth_test(bool on) override;
  void set_blend(bool on) override;
  void use_program(ProgramKind kind) override;
  void set_matrix(const char* name, const Mat4& m) override;
  void set_eye(const Vec3& eye) override;
  void set_lights(const FrameLights& L) override;
  void set_material(const Material& m) override;

  uint32_t create_texture(int w, int h, const uint8_t* rgba, bool srgb = true) override;
  void destroy_texture(uint32_t id) override;
  void bind_texture(uint32_t id) override;

  MeshId create_mesh(const Mesh& m) override;
  void destroy_mesh(MeshId id) override;
  void draw(MeshId id) override;
  MeshId builtin_quad() override;

 private:
  struct MeshData {
    GLuint vbo = 0, ibo = 0;
    int indexCount = 0;
  };
  bool build_program(GLuint& out, const char* vs, const char* fs, std::string& err);
  GLint loc(unsigned prog, const char* name);

  const gl::GL& g_;
  bool ok_ = false;
  std::string gl_version_;
  GLuint prog_lit_ = 0, prog_unlit_ = 0;
  GLuint cur_prog_ = 0;
  uint32_t bound_tex_ = 0;
  int W_ = 0, H_ = 0;
  std::unordered_map<MeshId, MeshData> meshes_;
  uint32_t next_id_ = 1;
};

}  // namespace fc
