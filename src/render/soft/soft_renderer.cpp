#include "soft_renderer.hpp"

#include <atomic>
#include <condition_variable>
#include <cstring>
#include <algorithm>
#include <functional>
#include <mutex>
#include <thread>

namespace fc {

// Persistent worker pool for the rasterizer. Job 0-style sharing: every
// worker (and the calling thread) pulls band indices from one atomic counter.
class SoftWorkerPool {
 public:
  explicit SoftWorkerPool(int workers) {
    for (int i = 1; i < workers; ++i)
      workers_.emplace_back([this] { worker_loop(); });
  }
  ~SoftWorkerPool() {
    {
      std::lock_guard<std::mutex> lk(m_);
      stop_ = true;
      ++batch_;
    }
    cv_.notify_all();
    for (auto& t : workers_) t.join();
  }

  // Runs fn(0..jobs-1) across all workers plus the calling thread; blocks
  // until every job is finished.
  void run(int jobs, const std::function<void(int)>& fn) {
    if (jobs <= 0 || workers_.empty()) {
      for (int j = 0; j < jobs; ++j) fn(j);
      return;
    }
    {
      std::lock_guard<std::mutex> lk(m_);
      fn_ = &fn;
      jobs_ = jobs;
      next_.store(0, std::memory_order_relaxed);
      finished_ = 0;
      ++batch_;
    }
    cv_.notify_all();
    run_share(fn);  // the calling thread takes a share of the work
    std::unique_lock<std::mutex> lk(m_);
    done_cv_.wait(lk, [this] { return finished_ == (int)workers_.size(); });
    fn_ = nullptr;
  }

 private:
  void run_share(const std::function<void(int)>& fn) {
    for (;;) {
      int j = next_.fetch_add(1, std::memory_order_relaxed);
      if (j >= jobs_) break;
      fn(j);
    }
  }
  void worker_loop() {
    std::unique_lock<std::mutex> lk(m_);
    // Pretend the worker has already "seen" one generation less than the
    // current batch counter: a worker that starts while the first batch is
    // being submitted must still wake up for it.
    uint64_t seen = batch_ - 1;
    for (;;) {
      cv_.wait(lk, [&] { return batch_ != seen || stop_; });
      if (stop_) return;
      seen = batch_;
      const std::function<void(int)>* fn = fn_;
      lk.unlock();
      run_share(*fn);
      lk.lock();
      if (++finished_ == (int)workers_.size()) done_cv_.notify_one();
    }
  }

  std::vector<std::thread> workers_;
  std::mutex m_;
  std::condition_variable cv_, done_cv_;
  const std::function<void(int)>* fn_ = nullptr;
  std::atomic<int> next_{0};
  int jobs_ = 0;
  int finished_ = 0;
  uint64_t batch_ = 0;
  bool stop_ = false;
};

bool SoftRenderer::begin_frame(int width, int height) {
  if (width != W_ || height != H_) {
    W_ = width;
    H_ = height;
    front_.assign((size_t)W_ * H_ * 4, 0);
    back_.assign((size_t)W_ * H_ * 4, 0);
    depth_.assign((size_t)W_ * H_, 1.0f);
  }
  triangles_rasterized_ = 0;
  pixels_shaded_ = 0;
  return true;
}

void SoftRenderer::end_frame() { std::swap(front_, back_); }

bool SoftRenderer::read_frame_rgb8(uint8_t* out, int width, int height) {
  if (!front_.size()) return false;
  for (int y = 0; y < H_; ++y)
    for (int x = 0; x < W_; ++x) {
      const uint8_t* s = &front_[(size_t)(y * W_ + x) * 4];
      out[(y * width + x) * 3 + 0] = s[0];
      out[(y * width + x) * 3 + 1] = s[1];
      out[(y * width + x) * 3 + 2] = s[2];
    }
  return true;
}

void SoftRenderer::set_scissor(int x, int y, int w, int h) {
  if (w <= 0 || h <= 0) { x = 0; y = 0; w = W_; h = H_; }
  scx_ = x; scy_ = y; scw_ = w; sch_ = h;
  has_scissor_ = true;
}

void SoftRenderer::clear(float r, float g, float b, float a) {
  int x0 = std::max(0, scx_), y0 = std::max(0, scy_);
  int x1 = std::min(W_, scx_ + scw_), y1 = std::min(H_, scy_ + sch_);
  if (x1 <= x0 || y1 <= y0) return;
  uint8_t cr = (uint8_t)std::clamp((int)(r * 255.0f), 0, 255);
  uint8_t cg = (uint8_t)std::clamp((int)(g * 255.0f), 0, 255);
  uint8_t cb = (uint8_t)std::clamp((int)(b * 255.0f), 0, 255);
  uint8_t ca = (uint8_t)std::clamp((int)(a * 255.0f), 0, 255);
  const uint32_t px32 =
      (uint32_t)cr | ((uint32_t)cg << 8) | ((uint32_t)cb << 16) | ((uint32_t)ca << 24);
  const uint64_t px64 = (uint64_t)px32 | ((uint64_t)px32 << 32);
  for (int y = y0; y < y1; ++y) {
    uint8_t* row = back_.data() + (size_t)y * W_ * 4;
    int x = x0;
    // Align to an 8-byte boundary, then fill two pixels (8 bytes) per store.
    if (x < x1 && ((uintptr_t)(row + (size_t)x * 4)) & 7) {
      std::memcpy(row + (size_t)x * 4, &px32, 4);
      ++x;
    }
    int pairs = (x1 - x) >> 1;
    uint8_t* q = row + (size_t)x * 4;
    for (int i = 0; i < pairs; ++i, q += 8) std::memcpy(q, &px64, 8);
    if ((x1 - x) & 1) std::memcpy(q, &px32, 4);
    std::fill(depth_.begin() + (size_t)y * W_ + x0, depth_.begin() + (size_t)y * W_ + x1, 1.0f);
  }
}

void SoftRenderer::set_matrix(const char* name, const Mat4& m) {
  if (std::strcmp(name, "proj") == 0) proj_ = m;
  else if (std::strcmp(name, "view") == 0) view_ = m;
  else if (std::strcmp(name, "model") == 0) model_ = m;
}

uint32_t SoftRenderer::create_texture(int w, int h, const uint8_t* rgba, bool) {
  uint32_t id = next_id_++;
  Tex t;
  t.w = w; t.h = h;
  t.rgba.assign(rgba, rgba + (size_t)w * h * 4);
  textures_[id] = std::move(t);
  return id;
}

void SoftRenderer::destroy_texture(uint32_t id) { textures_.erase(id); }

MeshId SoftRenderer::create_mesh(const Mesh& m) {
  MeshId id = next_id_++;
  MeshData d;
  d.v.assign(m.vertices, m.vertices + (size_t)m.vertexCount * 8);
  d.idx.assign(m.indices, m.indices + m.indexCount);
  meshes_[id] = std::move(d);
  return id;
}

void SoftRenderer::destroy_mesh(MeshId id) { meshes_.erase(id); }

MeshId SoftRenderer::builtin_quad() {
  static float verts[4 * 8] = {
      -0.5f, -0.5f, 0, 0, 0, 1, 0, 1,
      0.5f, -0.5f, 0, 0, 0, 1, 1, 1,
      0.5f, 0.5f, 0, 0, 0, 1, 1, 0,
      -0.5f, 0.5f, 0, 0, 0, 1, 0, 0,
  };
  static uint32_t idx[6] = {0, 1, 2, 0, 2, 3};
  if (!quad_id_) {
    Mesh m{verts, 4, idx, 6};
    quad_id_ = create_mesh(m);
  }
  return quad_id_;
}

// ---------------------------------------------------------------- clipping

// Clip polygon against half-space: inside iff f(v) >= 0.
// f = (keep_z_minus_w) ? (z - w) : (z + w)
namespace {
struct Clip8 {
  float x, y, z, w;
};
template <bool FarPlane>
int clip_halfspace(const Clip8 in[], int n, Clip8 out[]) {
  int m = 0;
  for (int i = 0; i < n; ++i) {
    const Clip8& a = in[i];
    const Clip8& b = in[(i + 1) % n];
    float fa = FarPlane ? (a.w - a.z) : (a.z + a.w);
    float fb = FarPlane ? (b.w - b.z) : (b.z + b.w);
    bool ain = fa >= 0, bin = fb >= 0;
    if (ain) out[m++] = a;
    if (ain != bin) {
      float t = fa / (fa - fb);
      out[m] = {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t,
                a.w + (b.w - a.w) * t};
      out[m++] = out[m - 1];
    }
  }
  return m;
}
}  // namespace

bool SoftRenderer::clip_triangle(const ClipVertex in[3], ClipVertex out[8], int* outn) {
  Clip8 a[3] = {{in[0].x, in[0].y, in[0].z, in[0].w},
                {in[1].x, in[1].y, in[1].z, in[1].w},
                {in[2].x, in[2].y, in[2].z, in[2].w}};
  Clip8 b[8] = {};
  int n = 3;
  n = clip_halfspace<false>(a, n, b);
  if (n < 3) return false;
  int n2 = clip_halfspace<true>(b, n, a);
  if (n2 < 3) return false;
  for (int i = 0; i < n2; ++i) out[i] = {a[i].x, a[i].y, a[i].z, a[i].w};
  *outn = n2;
  return true;
}

void SoftRenderer::project(const ClipVertex& c, float& sx, float& sy, float& sz) {
  float invw = 1.0f / c.w;
  float ndcx = c.x * invw, ndcy = c.y * invw;
  sz = c.z * invw;
  sx = (ndcx * 0.5f + 0.5f) * scw_ + scx_;
  sy = (0.5f - ndcy * 0.5f) * sch_ + scy_;
}

void SoftRenderer::draw(MeshId id) {
  auto it = meshes_.find(id);
  if (it == meshes_.end()) return;
  const MeshData& m = it->second;
  const float* v = m.v.data();
  const uint32_t* idx = m.idx.data();
  Mat4 mvp = proj_ * view_ * model_;

  ClipVertex clip[8];
  int clipn;
  for (int i = 0; i + 2 < (int)m.idx.size(); i += 3) {
    ClipVertex cv[3];
    for (int k = 0; k < 3; ++k) {
      int vi = idx[i + k] * 8;
      float px = v[vi], py = v[vi + 1], pz = v[vi + 2];
      Vec4 p = mvp.transform(Vec4{px, py, pz, 1});
      cv[k] = {p.x, p.y, p.z, p.w};
    }
    if (!clip_triangle(cv, clip, &clipn) || clipn < 3) continue;

    // Clip-space position is a linear function of the 3D vertex, so
    // barycentrics computed in clip (x,y) recover the exact 3D weights for
    // clipped vertices — attributes carry through clipping for free.
    float x0 = cv[0].x, y0 = cv[0].y;
    float x1 = cv[1].x, y1 = cv[1].y;
    float x2 = cv[2].x, y2 = cv[2].y;
    float den = (y1 - y2) * x0 + (x2 - x1) * y0 + x1 * y2 - x2 * y1;
    if (std::fabs(den) < 1e-12f) continue;

    struct Src {
      float wx, wy, wz, nx, ny, nz, u, vv;
    } s[3];
    for (int k = 0; k < 3; ++k) {
      int vi = idx[i + k] * 8;
      float px = v[vi], py = v[vi + 1], pz = v[vi + 2];
      Vec4 wp = model_.transform(Vec4{px, py, pz, 1});
      Vec3 wn = model_.transformDir({v[vi + 3], v[vi + 4], v[vi + 5]});
      s[k] = {wp.x, wp.y, wp.z, wn.x, wn.y, wn.z, v[vi + 6], v[vi + 7]};
    }
    // Clip-space barycentrics: lam_k is 1 at cv[k], 0 at the other two.
    // (These also weight the *original* 3D vertex attributes, so clipped
    // vertices interpolate correctly.)
    auto attr_for = [&](const ClipVertex& c, NdcVertex& o) {
      float lam0 = ((y1 - y2) * c.x + (x2 - x1) * c.y + x1 * y2 - x2 * y1) / den;
      float lam1 = ((y2 - y0) * c.x + (x0 - x2) * c.y + x2 * y0 - x0 * y2) / den;
      float lam2 = 1.0f - lam0 - lam1;
      o.wx = s[0].wx * lam0 + s[1].wx * lam1 + s[2].wx * lam2;
      o.wy = s[0].wy * lam0 + s[1].wy * lam1 + s[2].wy * lam2;
      o.wz = s[0].wz * lam0 + s[1].wz * lam1 + s[2].wz * lam2;
      o.nx = s[0].nx * lam0 + s[1].nx * lam1 + s[2].nx * lam2;
      o.ny = s[0].ny * lam0 + s[1].ny * lam1 + s[2].ny * lam2;
      o.nz = s[0].nz * lam0 + s[1].nz * lam1 + s[2].nz * lam2;
      o.u = s[0].u * lam0 + s[1].u * lam1 + s[2].u * lam2;
      o.v = s[0].vv * lam0 + s[1].vv * lam1 + s[2].vv * lam2;
      project(c, o.x, o.y, o.z);
    };

    NdcVertex v0;
    attr_for(clip[0], v0);
    NdcVertex vk, vk1;
    attr_for(clip[1], vk);
    for (int k = 1; k + 1 < clipn; ++k) {
      if (k > 1) attr_for(clip[k], vk);
      attr_for(clip[k + 1], vk1);

      // Screen-space setup, then queue for band-parallel rasterization.
      RasterTri t;
      t.a = v0; t.b = vk; t.c = vk1;
      float area2 = (vk.x - v0.x) * (vk1.y - v0.y) - (vk.y - v0.y) * (vk1.x - v0.x);
      t.ccw = area2 > 0;
      t.s2 = t.ccw ? area2 : -area2;
      if (t.s2 < 1e-5f) continue;
      float bx0 = std::min({v0.x, vk.x, vk1.x});
      float by0 = std::min({v0.y, vk.y, vk1.y});
      float bx1 = std::max({v0.x, vk.x, vk1.x});
      float by1 = std::max({v0.y, vk.y, vk1.y});
      t.ix0 = std::max(0, (int)std::floor(bx0));
      t.iy0 = std::max(0, (int)std::floor(by0));
      t.ix1 = std::min(W_, (int)std::ceil(bx1));
      t.iy1 = std::min(H_, (int)std::ceil(by1));
      if (t.ix1 <= t.ix0 || t.iy1 <= t.iy0) continue;
      batch_.push_back(t);
    }
  }
  flush_batch();
}

void SoftRenderer::raster_rows(const RasterTri& t, int ry0, int ry1, uint64_t& px_shaded) {
  const NdcVertex& a = t.a;
  const NdcVertex& b = t.b;
  const NdcVertex& c = t.c;
  float s2 = t.s2;

  auto edge = [](float ax, float ay, float bx, float by, float px, float py) {
    return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
  };

  const Tex* tex = bound_tex_ ? &textures_.at(bound_tex_) : nullptr;
  Vec3 emissive = material_.emissive.xyz();
  bool lit = prog_ == ProgramKind::Lit;

  // Per-triangle point-light culling: every fragment's world position lies
  // inside the triangle's world AABB, so a light farther than its radius
  // from the AABB can never contribute (its attenuation is exactly 0 there).
  bool pmask[kMaxPointLights];
  bool* pmask_ptr = nullptr;
  if (lit && lights_.numPoints > 0) {
    float minx = std::min({a.wx, b.wx, c.wx}), maxx = std::max({a.wx, b.wx, c.wx});
    float miny = std::min({a.wy, b.wy, c.wy}), maxy = std::max({a.wy, b.wy, c.wy});
    float minz = std::min({a.wz, b.wz, c.wz}), maxz = std::max({a.wz, b.wz, c.wz});
    for (int i = 0; i < lights_.numPoints; ++i) {
      const PointLightSource& pl = lights_.points[i];
      float dx = std::max(minx - pl.position.x,
              std::max(0.0f, pl.position.x - maxx));
      float dy = std::max(miny - pl.position.y,
              std::max(0.0f, pl.position.y - maxy));
      float dz = std::max(minz - pl.position.z,
              std::max(0.0f, pl.position.z - maxz));
      pmask[i] = dx * dx + dy * dy + dz * dz < pl.radius * pl.radius;
    }
    pmask_ptr = pmask;
  }

  for (int py = ry0; py < ry1; ++py) {
    float fy = py + 0.5f;
    for (int px = t.ix0; px < t.ix1; ++px) {
      float fx = px + 0.5f;
      float w0 = edge(b.x, b.y, c.x, c.y, fx, fy);
      float w1 = edge(c.x, c.y, a.x, a.y, fx, fy);
      float w2 = edge(a.x, a.y, b.x, b.y, fx, fy);
      if (t.ccw) {
        if (w0 < 0 || w1 < 0 || w2 < 0) continue;
      } else {
        w0 = -w0; w1 = -w1; w2 = -w2;
        if (w0 < 0 || w1 < 0 || w2 < 0) continue;
      }
      float la = w0 / s2, lb = w1 / s2, lc = w2 / s2;
      float depth = (a.z * la + b.z * lb + c.z * lc + 1.0f) * 0.5f;
      size_t i = (size_t)py * W_ + px;
      if (depth_test_ && depth >= depth_[i]) continue;

      float wx = a.wx * la + b.wx * lb + c.wx * lc;
      float wy = a.wy * la + b.wy * lb + c.wy * lc;
      float wz = a.wz * la + b.wz * lb + c.wz * lc;
      float nx = a.nx * la + b.nx * lb + c.nx * lc;
      float ny = a.ny * la + b.ny * lb + c.ny * lc;
      float nz = a.nz * la + b.nz * lb + c.nz * lc;
      float u = a.u * la + b.u * lb + c.u * lc;
      float vv = a.v * la + b.v * lb + c.v * lc;

      float r = 0, g = 0, bl = 0, al = 1;
      if (lit) {
        Vec3 N{nx, ny, nz}, P{wx, wy, wz};
        float nl2 = N.lengthSq();
        float inv = nl2 > 1e-12f ? 1.0f / std::sqrt(nl2) : 0.0f;  // reciprocal normalization
        N = N * inv;
        Vec3 dv = eye_ - P;
        float vl2 = dv.lengthSq();
        Vec3 V = vl2 > 1e-12f ? dv * (1.0f / std::sqrt(vl2)) : Vec3{0, 0, 0};
        Vec3 L = light_fragment(N, V, P, material_, lights_, emissive, pmask_ptr);
        r = L.x; g = L.y; bl = L.z;
        al = material_.baseColor.w;
      } else {
        r = material_.baseColor.x; g = material_.baseColor.y; bl = material_.baseColor.z;
        al = material_.baseColor.w;
      }
      if (tex) {
        float uu = u - std::floor(u);
        float vvv = vv - std::floor(vv);
        int tx = (int)(uu * tex->w) % tex->w;
        int ty = (int)(vvv * tex->h) % tex->h;
        if (tx < 0) tx += tex->w;
        if (ty < 0) ty += tex->h;
        if (tx >= tex->w) tx = tex->w - 1;
        if (ty >= tex->h) ty = tex->h - 1;
        const uint8_t* tp = &tex->rgba[(ty * tex->w + tx) * 4];
        r *= tp[0] / 255.0f;
        g *= tp[1] / 255.0f;
        bl *= tp[2] / 255.0f;
        al *= tp[3] / 255.0f;
      }
      depth_[i] = depth;
      size_t o = i * 4;
      if (blend_) {
        float inv = 1.0f - al;
        float br = back_[o] / 255.0f, bg = back_[o + 1] / 255.0f, bb = back_[o + 2] / 255.0f;
        float ba = back_[o + 3] / 255.0f;
        back_[o] = (uint8_t)std::clamp((int)((r * al + br * inv * ba) * 255.0f), 0, 255);
        back_[o + 1] = (uint8_t)std::clamp((int)((g * al + bg * inv * ba) * 255.0f), 0, 255);
        back_[o + 2] = (uint8_t)std::clamp((int)((bl * al + bb * inv * ba) * 255.0f), 0, 255);
        back_[o + 3] = (uint8_t)std::clamp((int)((al + ba * inv) * 255.0f), 0, 255);
      } else {
        back_[o] = (uint8_t)std::clamp((int)(r * 255.0f), 0, 255);
        back_[o + 1] = (uint8_t)std::clamp((int)(g * 255.0f), 0, 255);
        back_[o + 2] = (uint8_t)std::clamp((int)(bl * 255.0f), 0, 255);
        back_[o + 3] = (uint8_t)std::clamp((int)(al * 255.0f), 0, 255);
      }
      ++px_shaded;
    }
  }
}

int SoftRenderer::resolve_threads() const {
  if (threads_ > 0) return threads_;
  unsigned hc = std::thread::hardware_concurrency();
  int cores = hc ? (int)hc : 1;
  return cores < 4 ? cores : 4;  // auto: min(4, cores)
}

void SoftRenderer::set_threads(int n) {
  n = std::max(n, 0);
  if (n == threads_) return;
  threads_ = n;
  pool_.reset();  // rebuilt (at the right size) on the next parallel batch
  pool_threads_ = 0;
}

void SoftRenderer::ensure_pool(int threads) {
  if (!pool_ || pool_threads_ != threads) {
    pool_.reset();
    pool_ = std::make_unique<SoftWorkerPool>(threads);  // spawns threads-1, caller joins in
    pool_threads_ = threads;
  }
}

void SoftRenderer::flush_batch() {
  if (batch_.empty()) return;

  // Parallelize only when there is enough fragment work to amortize the
  // dispatch: total triangle-span coverage (rows) below the threshold stays
  // on the serial path (typical for small UI sprite batches).
  int64_t work = 0;
  for (const RasterTri& t : batch_) work += t.iy1 - t.iy0;
  int T = resolve_threads();
  if (T > 1 && work >= 256 && H_ > 0) {
    // Area-balanced scanline bands: weight each framebuffer row by the number
    // of triangle spans covering it, then cut bands at equal cumulative
    // weight so every thread gets a similar amount of fragment work.
    row_weight_.assign(H_ + 1, 0);
    for (const RasterTri& t : batch_) {
      ++row_weight_[t.iy0];
      --row_weight_[t.iy1];
    }
    int64_t total = 0, run = 0;
    for (int y = 0; y < H_; ++y) {
      run += row_weight_[y];
      row_weight_[y] = (int)run;
      total += run;
    }
    band_start_.assign(T + 1, H_);
    band_start_[0] = 0;
    int64_t target = (total + T - 1) / T;
    int band = 1;
    int64_t acc = 0;
    for (int y = 0; y < H_ && band < T; ++y) {
      acc += row_weight_[y];
      if (acc >= target && H_ - (y + 1) >= T - band) {
        band_start_[band++] = y + 1;
        acc = 0;
      }
    }
    // band_start_[band..T] stay H_: unused bands get empty row ranges.

    band_px_.assign(T, 0);
    band_tri_.assign(T, 0);
    ensure_pool(T);
    const std::vector<RasterTri>& tris = batch_;
    const int* bs = band_start_.data();
    uint64_t* bpx = band_px_.data();
    uint64_t* btri = band_tri_.data();
    pool_->run(T, [&](int b) {
      int y0 = bs[b], y1 = bs[b + 1];
      if (y0 >= y1) return;
      uint64_t px = 0, tri = 0;
      for (const RasterTri& t : tris) {
        int ry0 = t.iy0 > y0 ? t.iy0 : y0;
        int ry1 = t.iy1 < y1 ? t.iy1 : y1;
        if (ry0 >= ry1) continue;
        // Rows are disjoint across bands, so per-pixel state (depth test,
        // blending) is race-free; submission order is kept within a band,
        // which makes the result pixel-identical to the serial path.
        raster_rows(t, ry0, ry1, px);
        if (t.iy0 >= y0 && t.iy0 < y1) ++tri;  // count each triangle once
      }
      bpx[b] = px;
      btri[b] = tri;
    });
    for (int b = 0; b < T; ++b) {
      pixels_shaded_ += bpx[b];
      triangles_rasterized_ += btri[b];
    }
  } else {
    // Serial path (threads == 1 or tiny batches): identical math, same order
    // as before threading.
    for (const RasterTri& t : batch_) {
      uint64_t px = 0;
      raster_rows(t, t.iy0, t.iy1, px);
      pixels_shaded_ += px;
      ++triangles_rasterized_;
    }
  }
  batch_.clear();
}

SoftRenderer::SoftRenderer() = default;
SoftRenderer::~SoftRenderer() = default;

}  // namespace fc
