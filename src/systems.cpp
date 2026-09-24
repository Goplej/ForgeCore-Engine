#include "fc/systems.hpp"
#include "fc/engine.hpp"

#include <algorithm>
#include <unordered_map>
#include <vector>

namespace fc {

// ---------------------------------------------------------------- Transform

void TransformSystem::update(Engine& e, float) {
  auto& world = e.scene().world();
  std::unordered_map<Entity, Mat4> local;
  std::unordered_map<Entity, Entity> parent;
  world.each<Transform>([&](Entity en, Transform* t) { local[en] = t->local_matrix(); });
  world.each<Parent>([&](Entity en, Parent* p) { parent[en] = p->parent; });

  auto resolve = [&](Entity en) -> Mat4 {
    // walk the parent chain (cycle-safe) and fold local matrices
    Mat4 acc = Mat4::identity();
    Entity cur = en;
    for (int depth = 0; depth < 64 && local.count(cur); ++depth) {
      acc = local[cur] * acc;
      auto pit = parent.find(cur);
      cur = pit == parent.end() ? kInvalidEntity : pit->second;
    }
    return acc;
  };

  world.each<Transform>([&](Entity en, Transform*) {
    auto& wt = e.scene().world().attach<WorldTransform>(en, WorldTransform{});
    wt.world = resolve(en);
  });
}

// ------------------------------------------------------------------ Camera

void CameraSystem::update(Engine& e, float dt) {
  (void)dt;
  auto& world = e.scene().world();
  auto& input = e.input();
  int screen_w = e.window_width();
  int screen_h = e.window_height();

  // pass 1: orbit input (drag / scroll) on cameras with OrbitControls
  world.each<Camera>([&](Entity cam, Camera* c) {
    OrbitControls* oc = world.get<OrbitControls>(cam);
    Viewport* vp = world.get<Viewport>(cam);
    if (!oc || !vp || !c->perspective) return;
    Vec2 m = input.mouse();
    bool in_vp = m.x >= vp->x && m.x < vp->x + vp->w && m.y >= vp->y && m.y < vp->y + vp->h;
    if (!in_vp) return;
    if (input.mouse_down(MouseButton::Left)) {
      Vec2 d = input.mouse_delta();
      oc->yaw += d.x * 0.008f;
      oc->pitch = math::clampf(oc->pitch + d.y * 0.008f, 0.05f, 1.5f);
    }
    if (input.scroll() != 0.0f)
      oc->distance = math::clampf(oc->distance * (1.0f - input.scroll() * 0.08f), 2.0f, 30.0f);
  });

  // pass 2: materialize orbit position + proj/view/eye matrices
  world.each<Camera>([&](Entity cam, Camera* c) {
    OrbitControls* oc = world.get<OrbitControls>(cam);
    if (oc) {
      Transform& t = world.attach<Transform>(cam, Transform{});
      float r = oc->distance;
      t.position = oc->target +
                   Vec3{r * std::cos(oc->pitch) * std::cos(oc->yaw),
                        r * std::sin(oc->pitch),
                        r * std::cos(oc->pitch) * std::sin(oc->yaw)};
      t.rotZ = 0;
      t.orientation = {};
      // look-at is applied via view matrix below
    }
    WorldTransform* wt = world.get<WorldTransform>(cam);
    Viewport* vp = world.get<Viewport>(cam);    if (!wt) return;
    auto& cm = world.attach<CameraMatrices>(cam, CameraMatrices{});    float aspect = (vp && vp->h > 0) ? (float)vp->w / (float)vp->h : 16.0f / 9.0f;
    if (c->perspective) {
      cm.proj = Mat4::perspective(math::radians(c->fov), aspect, c->znear, c->zfar);
      OrbitControls* o2 = world.get<OrbitControls>(cam);
      Vec3 eye = wt->world.transformPoint(Vec3{0, 0, 0});
      Vec3 center = o2 ? o2->target : eye + Vec3{0, 0, -1};
      cm.view = Mat4::lookAt(eye, center, Vec3{0, 1, 0});
      cm.eye = eye;
    } else {
      cm.proj = Mat4::ortho(-c->orthoHalfW, c->orthoHalfW, -c->orthoHalfH, c->orthoHalfH, 0.1f,
                            100.0f);
      cm.view = wt->world.inverse();
      cm.eye = wt->world.transformPoint(Vec3{0, 0, 0});
    }
    (void)screen_w;
    (void)screen_h;
  });
}

// ------------------------------------------------------------------ Lights

void LightsSystem::update(Engine& e, float) {
  FrameLights L{};
  L.numPoints = 0;
  auto& world = e.scene().world();
  world.each<DirLight>([&](Entity, DirLight* d) {
    L.dir.direction = d->direction;
    L.dir.color = d->color;
  });
  int n = 0;
  world.each<PointLight>([&](Entity en, PointLight* pl) {
    if (n >= 4) return;
    Transform* t = world.get<Transform>(en);
    L.points[n].position = t ? t->position : Vec3{0, 0, 0};
    L.points[n].color = pl->color;
    L.points[n].radius = pl->radius;
    ++n;
  });
  L.numPoints = n;
  e.frame().lights = L;
}

// --------------------------------------------------------------- Physics2D

namespace {
struct Body2D {
  Entity e;
  Transform* t;
  RigidBody2D* b;
};

void audio_bounce(Engine& e, float speed, const char* what) {
  float v = math::clampf(speed * 0.02f, 0.03f, 0.5f);
  SoundParams p;
  p.freq0 = 120.0f + math::clampf(speed, 0.0f, 400.0f) * 0.6f;
  p.freq1 = p.freq0 * 0.5f;
  p.duration = 0.08f;
  p.volume = v;
  p.wave = Wave::Sine;
  e.audio().play(p, what);
}
}  // namespace

void Physics2DSystem::update(Engine& e, float dt) {
  auto& world = e.scene().world();
  std::vector<Body2D> bodies;
  world.each<Transform>([&](Entity en, Transform* t) {
    if (world.has<RigidBody2D>(en)) bodies.push_back({en, t, world.get<RigidBody2D>(en)});
  });
  std::vector<std::pair<Entity, Collider2D*>> statics;
  world.each<Collider2D>([&](Entity en, Collider2D* c) { statics.push_back({en, c}); });
  std::vector<std::pair<Entity, Transform*>> static_pos;
  for (auto& [en, c] : statics) {
    (void)c;
    static_pos.push_back({en, world.get<Transform>(en)});
  }

  for (auto& bd : bodies) {
    RigidBody2D* b = bd.b;
    Transform* t = bd.t;
    if (b->gravity) b->velocity.y -= b->gravityAccel * dt;
    t->position.x += b->velocity.x * dt;
    t->position.y += b->velocity.y * dt;

    // circle vs static AABBs
    for (size_t si = 0; si < statics.size(); ++si) {
      Transform* sp = static_pos[si].second;
      Vec2 c = sp ? Vec2{sp->position.x, sp->position.y} : Vec2{0, 0};
      Vec2 hs = statics[si].second->size * 0.5f;
      Vec2 p = Vec2{t->position.x, t->position.y};
      float cx = math::clampf(p.x, c.x - hs.x, c.x + hs.x);
      float cy = math::clampf(p.y, c.y - hs.y, c.y + hs.y);
      float dx = p.x - cx, dy = p.y - cy;
      float d2 = dx * dx + dy * dy;
      if (d2 >= b->radius * b->radius) continue;
      float d = std::sqrt(d2);
      Vec2 n;
      float push;
      if (d > 1e-5f) {
        n = Vec2{dx / d, dy / d};
        push = b->radius - d;
      } else {
        // center inside box: push out along smallest penetration axis
        float pl = p.x - (c.x - hs.x), pr = (c.x + hs.x) - p.x;
        float pt = p.y - (c.y - hs.y), pb = (c.y + hs.y) - p.y;
        float m = std::min({pl, pr, pt, pb});
        if (m == pl) { n = {-1, 0}; push = pl + b->radius; }
        else if (m == pr) { n = {1, 0}; push = pr + b->radius; }
        else if (m == pt) { n = {0, 1}; push = pt + b->radius; }
        else { n = {0, -1}; push = pb + b->radius; }
      }
      t->position.x += n.x * push;
      t->position.y += n.y * push;
      float vn = b->velocity.x * n.x + b->velocity.y * n.y;
      if (vn < 0) {
        b->velocity.x -= (1.0f + b->restitution) * vn * n.x;
        b->velocity.y -= (1.0f + b->restitution) * vn * n.y;
        if (-vn > 60.0f) audio_bounce(e, -vn, "2d-bounce");
      }
      b->velocity.x *= (1.0f - b->friction);
    }
  }

  // circle vs circle
  for (size_t i = 0; i < bodies.size(); ++i) {
    for (size_t j = i + 1; j < bodies.size(); ++j) {
      Body2D& A = bodies[i];
      Body2D& B = bodies[j];
      float dx = B.t->position.x - A.t->position.x;
      float dy = B.t->position.y - A.t->position.y;
      float r = A.b->radius + B.b->radius;
      float d2 = dx * dx + dy * dy;
      if (d2 >= r * r || d2 < 1e-9f) continue;
      float d = std::sqrt(d2);
      Vec2 n{dx / d, dy / d};
      float overlap = r - d;
      float invA = 1.0f / A.b->mass, invB = 1.0f / B.b->mass;
      float inv = invA + invB;
      A.t->position.x -= n.x * overlap * (invA / inv);
      A.t->position.y -= n.y * overlap * (invA / inv);
      B.t->position.x += n.x * overlap * (invB / inv);
      B.t->position.y += n.y * overlap * (invB / inv);
      float rvx = B.b->velocity.x - A.b->velocity.x;
      float rvy = B.b->velocity.y - A.b->velocity.y;
      float vn = rvx * n.x + rvy * n.y;
      if (vn < 0) {
        float rest = std::min(A.b->restitution, B.b->restitution);
        float jimp = -(1.0f + rest) * vn / inv;
        A.b->velocity.x -= jimp * invA * n.x;
        A.b->velocity.y -= jimp * invA * n.y;
        B.b->velocity.x += jimp * invB * n.x;
        B.b->velocity.y += jimp * invB * n.y;
        if (-vn > 60.0f) audio_bounce(e, -vn, "2d-collision");
      }
    }
  }
}

// ---------------------------------------------------------- SpherePhysics3D

void SpherePhysics3DSystem::update(Engine& e, float dt) {
  auto& world = e.scene().world();
  Box3D* box = nullptr;
  world.each<Box3D>([&](Entity, Box3D* b) { box = b; });
  world.each<Transform>([&](Entity en, Transform* t) {
    SphereBody* b = world.get<SphereBody>(en);
    if (!b) return;
    if (b->gravity) b->velocity.y -= b->gravityAccel * dt;
    t->position += b->velocity * dt;
    if (!box) return;
    for (int axis = 0; axis < 3; ++axis) {
      float& p = axis == 0 ? t->position.x : axis == 1 ? t->position.y : t->position.z;
      float& v = axis == 0 ? b->velocity.x : axis == 1 ? b->velocity.y : b->velocity.z;
      float mn = axis == 0 ? box->min.x : axis == 1 ? box->min.y : box->min.z;
      float mx = axis == 0 ? box->max.x : axis == 1 ? box->max.y : box->max.z;
      float lo = mn + b->radius, hi = mx - b->radius;
      if (p < lo) {
        p = lo;
        if (v < 0) {
          if (-v > 2.0f) audio_bounce(e, (-v) * 40.0f, "3d-bounce");
          v = -v * b->restitution;
        }
      } else if (p > hi) {
        p = hi;
        if (v > 0) {
          if (v > 2.0f) audio_bounce(e, v * 40.0f, "3d-bounce");
          v = -v * b->restitution;
        }
      }
    }
  });
}

// ----------------------------------------------------------------- Render

void Render3DSystem::update(Engine& e, float) {
  Renderer* r = e.renderer();
  if (!r) return;
  auto& world = e.scene().world();
  world.each<Camera>([&](Entity cam, Camera* c) {
    if (!c->perspective) return;
    CameraMatrices* cm = world.get<CameraMatrices>(cam);
    Viewport* vp = world.get<Viewport>(cam);
    if (!cm || !vp) return;
    r->set_scissor(vp->x, vp->y, vp->w, vp->h);
    r->clear(clearColor.x, clearColor.y, clearColor.z, 1.0f);
    r->set_depth_test(true);
    r->set_blend(true);
    r->use_program(ProgramKind::Lit);
    r->set_matrix("proj", cm->proj);
    r->set_matrix("view", cm->view);
    r->set_eye(cm->eye);
    r->set_lights(e.frame().lights);
    std::vector<Entity> list;
    world.each<Mesh>([&](Entity en, Mesh*) { list.push_back(en); });    for (Entity en : list) {
      CameraOf* co = world.get<CameraOf>(en);
      if (!co || co->camera != cam) continue;
      Mesh* m = world.get<Mesh>(en);
      WorldTransform* wt = world.get<WorldTransform>(en);
      if (!wt) continue;
      r->set_matrix("model", wt->world);
      r->set_material(m->material);
      r->bind_texture(m->texture);
      r->draw(m->geometry);
    }
  });
}

void Render2DSystem::update(Engine& e, float) {
  Renderer* r = e.renderer();
  if (!r) return;
  auto& world = e.scene().world();
  world.each<Camera>([&](Entity cam, Camera* c) {
    if (c->perspective) return;
    CameraMatrices* cm = world.get<CameraMatrices>(cam);
    Viewport* vp = world.get<Viewport>(cam);
    if (!cm || !vp) return;
    r->set_scissor(vp->x, vp->y, vp->w, vp->h);    if (c->clears) r->clear(clearColor.x, clearColor.y, clearColor.z, 1.0f);
    r->set_depth_test(false);
    r->set_blend(true);
    r->use_program(ProgramKind::Unlit);
    r->set_matrix("proj", cm->proj);
    r->set_matrix("view", cm->view);

    std::vector<std::pair<Entity, Sprite*>> list;
    world.each<Sprite>([&](Entity en, Sprite* s) { list.push_back({en, s}); });    std::stable_sort(list.begin(), list.end(),
                     [](auto& a, auto& b) { return a.second->depth < b.second->depth; });
    for (auto& [en, s] : list) {
      CameraOf* co = world.get<CameraOf>(en);
      if (!co || co->camera != cam) continue;
      Transform* t = world.get<Transform>(en);
      if (!t) continue;
      Mat4 model = Mat4::translation({t->position.x, t->position.y, t->position.z}) *
                   Mat4::rotationZ(t->rotZ) * Mat4::scaling({s->size.x, s->size.y, 1.0f});
      r->set_matrix("model", model);
      Material mat;
      mat.baseColor = s->tint;
      r->set_material(mat);
      r->bind_texture(s->texture);
      r->draw(r->builtin_quad());
    }
  });
}

}  // namespace fc
