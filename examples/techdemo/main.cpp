// ForgeCore Engine — tech demo
//
// One window, three capability regions:
//   left   : 3D — perspective camera (drag to orbit, wheel to zoom),
//            directional + orbiting point light, animated torus/cube,
//            physics sphere bouncing in a box arena, grid floor
//   right  : 2D — ortho scene with rigid bodies (player + balls),
//            sprite textures, procedural UI (panels, buttons, bars)
//   bottom : full-width status bar (backend, resolution, hotkeys)
//
// Audio: procedural synth on every bounce/click; in headless preview mode
// events are relayed to the browser, which plays them via WebAudio.
//
// Run:
//   techdemo                                      interactive
//   techdemo --stream                             headless + /dev/shm streaming
//   techdemo --frames 120 --screenshot out.ppm    CI mode

#include "fc/audio.hpp"
#include "fc/engine.hpp"
#include "fc/geometry.hpp"
#include "fc/ui.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

using namespace fc;

// ------------------------------------------------------------------ state

struct Demo {
  // cameras / scene handles
  Entity cam3d, cam3dUI, cam2d, cam2dUI, camBar;
  Entity pointLight, torus, cube, ball3d, player;
  std::vector<Entity> balls2d;
  std::vector<Entity> orbs;
  uint32_t texOrb, texOrb2, texGlow, texCheck, texPlayer;

  // toggles / motion
  bool spin = true, gravity = true, lights = true;
  float time = 0, lightAngle = 0.8f, bob = 0;
  bool playerOnGround = true;

  int W = 1280, H = 720;
  int barH = 30;
  int regionH() const { return H - barH; }
};

static void sfx(Engine& e, const std::string& name, float f0, float f1, float dur, float vol,
                Wave w) {
  SoundParams p;
  p.freq0 = f0; p.freq1 = f1; p.duration = dur; p.volume = vol; p.wave = w;
  e.audio().play(p, name);
}

// ------------------------------------------------------------------ build

static void build_scene(Engine& e, Demo& d) {
  auto& sc = e.scene();
  Renderer* r = e.renderer();
  int regionH = d.regionH();
  int half = d.W / 2;

  auto as_mesh = [](const Geometry& g) {
    return Renderer::Mesh{g.vertices.data(), (int)g.vertices.size() / 8, g.indices.data(),
                          (int)g.indices.size()};
  };

  // --- textures -----------------------------------------------------------
  auto orb = make_orb(64, 255, 150, 60);
  auto orb2 = make_orb(64, 90, 180, 255);
  auto glow = make_radial_glow(64, 120, 90, 255);
  auto check = make_checker(64, 8, 70, 60, 130, 40, 36, 80);
  d.texOrb = r->create_texture(64, 64, orb.data());
  d.texOrb2 = r->create_texture(64, 64, orb2.data());
  d.texGlow = r->create_texture(64, 64, glow.data());
  d.texCheck = r->create_texture(64, 64, check.data());
  d.texPlayer = r->create_texture(64, 64, make_player(64).data());

  // --- 3D camera + lights ---------------------------------------------------
  Camera cam3dc;
  cam3dc.perspective = true;
  cam3dc.fov = 55;
  d.cam3d = sc.spawn(cam3dc, Viewport{0, 0, half, regionH},
                     OrbitControls{0.8f, 0.45f, 7.5f, {0, 1.2f, 0}});
  {
    Camera ui;
    ui.perspective = false;
    ui.clears = false;
    ui.orthoHalfW = (float)half;
    ui.orthoHalfH = (float)regionH / 2;
    d.cam3dUI = sc.spawn(ui, Viewport{0, 0, half, regionH}, Transform{{0, 0, 10}});
  }
  sc.spawn(Transform{{-0.3f, -1.0f, -0.5f}}, DirLight{{0.4f, -1.0f, 0.5f}, {0.5f, 0.55f, 0.65f}});
  d.pointLight = sc.spawn(Transform{{3, 2.8f, 0}}, PointLight{{1.0f, 0.75f, 0.45f}, 9.0f});

  // --- 3D scene -------------------------------------------------------------
  {
    Mesh gm; gm.geometry = r->create_mesh(as_mesh(make_grid(5.0f, 10)));
    gm.material.baseColor = {0.14f, 0.18f, 0.30f, 1};
    gm.material.specularStrength = 0.08f;
    sc.spawn(Transform{{0, 0, 0}}, gm, CameraOf{d.cam3d});

    Mesh tm; tm.geometry = r->create_mesh(as_mesh(make_torus(1.25f, 0.45f, 48, 20)));
    tm.material.baseColor = {0.10f, 0.78f, 0.88f, 1};
    tm.material.specularStrength = 0.9f;
    tm.material.shininess = 48.0f;
    d.torus = sc.spawn(Transform{{0, 1.8f, 0}}, tm, CameraOf{d.cam3d});

    Mesh cm; cm.geometry = r->create_mesh(as_mesh(make_cube()));
    cm.material.baseColor = {0.95f, 0.55f, 0.20f, 1};
    cm.material.specularStrength = 0.5f;
    d.cube = sc.spawn(Transform{{-2.4f, 0.6f, 1.4f}}, cm, CameraOf{d.cam3d});

    Mesh sm; sm.geometry = r->create_mesh(as_mesh(make_sphere(1.0f, 20, 28)));
    sm.material.baseColor = {0.98f, 0.85f, 0.30f, 1};
    sm.material.emissive = {0.12f, 0.07f, 0.0f, 0};
    sm.material.specularStrength = 0.6f;
    SphereBody sb; sb.radius = 0.35f; sb.restitution = 0.72f; sb.velocity = {-1.5f, 0, 2.0f};
    d.ball3d = sc.spawn(Transform{{2.2f, 5.0f, -1.4f}}, sm, sb, CameraOf{d.cam3d});

    sc.spawn(Box3D{{-4, 0, -4}, {4, 8, 4}});
  }

  // --- 2D camera + stage ------------------------------------------------------
  {
    Camera c2;
    c2.perspective = false;
    c2.orthoHalfW = (float)half;
    c2.orthoHalfH = (float)regionH / 2;
    d.cam2d = sc.spawn(c2, Viewport{half, 0, d.W - half, regionH}, Transform{{0, 0, 10}});
    Camera u2 = c2;
    u2.clears = false;
    d.cam2dUI = sc.spawn(u2, Viewport{half, 0, d.W - half, regionH}, Transform{{0, 0, 10}});
    Camera cb;
    cb.perspective = false;
    cb.orthoHalfW = (float)d.W / 2;
    cb.orthoHalfH = (float)d.barH / 2;
    d.camBar = sc.spawn(cb, Viewport{0, regionH, d.W, d.barH}, Transform{{0, 0, 10}});
  }

  // arena: floor + walls at the visible edges (balls bounce inside the view)
  sc.spawn(Transform{{0, -300}}, Collider2D{{900, 40}});
  sc.spawn(Transform{{-316, 0}}, Collider2D{{24, 900}});
  sc.spawn(Transform{{316, 0}}, Collider2D{{24, 900}});
  const Vec4 kFloor{0.09f, 0.105f, 0.17f, 1};
  const Vec4 kEdge{0.36f, 0.48f, 0.76f, 1};
  { Sprite f; f.size = {700, 40}; f.tint = kFloor; f.depth = 0;
    sc.spawn(Transform{{0, -300}}, f, CameraOf{d.cam2d}); }
  { Sprite f; f.size = {700, 3}; f.tint = kEdge; f.depth = 0;
    sc.spawn(Transform{{0, -280}}, f, CameraOf{d.cam2d}); }
  { Sprite w1; w1.size = {24, 700}; w1.tint = kFloor; w1.depth = 0;
    sc.spawn(Transform{{-316, 0}}, w1, CameraOf{d.cam2d});
    Sprite e1; e1.size = {3, 700}; e1.tint = kEdge; e1.depth = 0;
    sc.spawn(Transform{{-303, 0}}, e1, CameraOf{d.cam2d});
    Sprite w2; w2.size = {24, 700}; w2.tint = kFloor; w2.depth = 0;
    sc.spawn(Transform{{316, 0}}, w2, CameraOf{d.cam2d});
    Sprite e2; e2.size = {3, 700}; e2.tint = kEdge; e2.depth = 0;
    sc.spawn(Transform{{303, 0}}, e2, CameraOf{d.cam2d}); }
  {
    Sprite ps; ps.texture = d.texPlayer; ps.size = {34, 34}; ps.depth = 5;
    RigidBody2D pb; pb.radius = 17; pb.restitution = 0.05f; pb.mass = 2.0f;
    d.player = sc.spawn(Transform{{-140, -260}}, ps, pb, CameraOf{d.cam2d});
  }
  for (int i = 0; i < 3; ++i) {
    Sprite s; s.texture = i == 1 ? d.texOrb2 : d.texOrb; s.size = {54, 54}; s.depth = 2;
    d.orbs.push_back(sc.spawn(Transform{{-160.0f + i * 160.0f, 150.0f + (i % 2) * 60.0f}}, s,
                              CameraOf{d.cam2d}));
  }
  {
    Sprite gs; gs.texture = d.texGlow; gs.size = {160, 160}; gs.tint = {0.5f, 0.4f, 1, 1};
    gs.depth = 1;
    sc.spawn(Transform{{-220, -180}}, gs, CameraOf{d.cam2d});
    Sprite gs2 = gs; gs2.texture = d.texOrb; gs2.size = {90, 90}; gs2.tint = {1, 1, 1, 0.6f};
    sc.spawn(Transform{{180, 200}}, gs2, CameraOf{d.cam2d});
  }
}

// ------------------------------------------------------------------ update

static void spawn_ball(Engine& e, Demo& d, float x, float y) {
  auto& sc = e.scene();
  Sprite s; s.texture = (d.balls2d.size() % 2) ? d.texOrb2 : d.texOrb; s.size = {34, 34};
  s.depth = 4;
  RigidBody2D b; b.radius = 17; b.restitution = 0.82f;
  b.velocity = {(float)std::sin(d.time * 7.3) * 120.0f, 60.0f};
  Entity en = sc.spawn(Transform{{x, y}}, s, b, CameraOf{d.cam2d});
  d.balls2d.push_back(en);
  if (d.balls2d.size() > 10) {
    Entity oldest = d.balls2d.front();
    d.balls2d.erase(d.balls2d.begin());
    sc.destroy(oldest);
  }
  sfx(e, "2d-spawn", 660, 990, 0.07f, 0.25f, Wave::Square);
}

static void reset_balls(Engine& e, Demo& d) {
  for (Entity b : d.balls2d) e.scene().destroy(b);
  d.balls2d.clear();
  sfx(e, "ui-reset", 500, 160, 0.2f, 0.3f, Wave::Saw);
}

static void demo_update(Engine& e, Demo& d, float dt) {
  auto& w = e.scene().world();
  auto& in = e.input();
  d.time += dt;
  d.bob += dt;

  // 3D animation
  {
    Transform& t = *w.get<Transform>(d.torus);
    if (d.spin)
      t.orientation = Quat::fromAxisAngle({0, 1, 0}, d.time * 1.1f) *
                      Quat::fromAxisAngle({1, 0, 0}, d.time * 0.55f);
  }
  {
    Transform& t = *w.get<Transform>(d.cube);
    t.orientation = Quat::fromAxisAngle({0, 1, 0}, d.time * 0.7f) *
                    Quat::fromAxisAngle({0, 0, 1}, d.time * 0.3f);
  }
  d.lightAngle += dt * 0.9f;
  Transform& pl = *w.get<Transform>(d.pointLight);
  pl.position = {std::cos(d.lightAngle) * 3.2f, 2.8f + std::sin(d.time * 0.8f) * 0.5f,
                 std::sin(d.lightAngle) * 3.2f};
  w.get<PointLight>(d.pointLight)->color = d.lights ? Vec3{1.0f, 0.75f, 0.45f} : Vec3{0, 0, 0};

  // gravity toggle
  w.each<RigidBody2D>([&](Entity, RigidBody2D* b) { b->gravity = d.gravity; });
  w.each<SphereBody>([&](Entity, SphereBody* b) { b->gravity = d.gravity; });

  // player control
  {
    Transform& t = *w.get<Transform>(d.player);
    RigidBody2D* b = w.get<RigidBody2D>(d.player);
    bool l = in.key_down(Key::A) || in.key_down(Key::Left);
    bool rr = in.key_down(Key::D) || in.key_down(Key::Right);
    if (l) b->velocity.x = std::max(b->velocity.x - 2600 * dt, -300.0f);
    if (rr) b->velocity.x = std::min(b->velocity.x + 2600 * dt, 300.0f);
    if (!l && !rr) b->velocity.x *= (1.0f - 6.0f * dt);
    bool grounded = t.position.y <= -280.0f + b->radius + 3.0f && b->velocity.y <= 1.0f;
    if (grounded && !d.playerOnGround) sfx(e, "2d-land", 110, 70, 0.12f, 0.4f, Wave::Sine);
    if (in.key_pressed(Key::Space) && grounded) {
      b->velocity.y = 560.0f;
      sfx(e, "2d-jump", 280, 720, 0.16f, 0.3f, Wave::Sine);
    }
    d.playerOnGround = grounded;
  }

  // decorative orbs bob
  for (size_t i = 0; i < d.orbs.size(); ++i) {
    Transform& t = *w.get<Transform>(d.orbs[i]);
    t.position.x = -160.0f + (float)i * 160.0f;
    t.position.y = 150.0f + (i % 2) * 60.0f + std::sin(d.bob * 1.4f + (float)i * 2.1f) * 14.0f;
  }

  // spawn ball on click inside the 2D region
  {
    Vec2 m = in.mouse();
    int regionH = d.regionH();
    if (in.mouse_pressed(MouseButton::Left) && m.x >= d.W / 2 && m.x < d.W && m.y >= 0 &&
        m.y < regionH) {
      float wx = m.x - d.W / 2.0f - d.W / 4.0f;  // viewport origin x=W/2, halfW=W/4
      float wy = d.regionH() / 2.0f - m.y;
      spawn_ball(e, d, wx, wy);
    }
  }

  // hotkeys
  auto tick = [&]() { sfx(e, "ui-click", 1250, 1250, 0.04f, 0.2f, Wave::Square); };
  if (in.key_pressed(Key::F1)) { d.spin = !d.spin; tick(); }
  if (in.key_pressed(Key::F2)) { d.gravity = !d.gravity; tick(); }
  if (in.key_pressed(Key::F3)) { d.lights = !d.lights; tick(); }
  if (in.key_pressed(Key::R)) reset_balls(e, d);
}

// -------------------------------------------------------------------- hud

static void draw_hud(Engine& e, Demo& d) {
  auto& w = e.scene().world();
  Renderer* r = e.renderer();
  UI ui(r, &e.input());
  int regionH = d.regionH();
  int half = d.W / 2;

  auto begin_ui = [&](Entity cam, int vx, int vy) {
    CameraMatrices* cm = w.get<CameraMatrices>(cam);
    Camera* c = w.get<Camera>(cam);
    Viewport* vp = w.get<Viewport>(cam);
    if (!cm || !c || !vp) return;
    r->set_scissor(vp->x, vp->y, vp->w, vp->h);
    ui.begin(cm->proj * cm->view, (float)vx, (float)vy, c->orthoHalfW, c->orthoHalfH);
  };

  const Vec4 kText{0.85f, 0.9f, 1.0f, 1};
  const Vec4 kDim{0.55f, 0.62f, 0.75f, 1};
  const Vec4 kPanel{0.07f, 0.09f, 0.16f, 0.88f};
  const Vec4 kBorder{0.3f, 0.4f, 0.6f, 1};
  const Vec4 kGreen{0.3f, 0.9f, 0.5f, 1};
  const Vec4 kOff{0.45f, 0.45f, 0.5f, 1};
  const Vec4 kBtn{0.14f, 0.2f, 0.34f, 1};
  const Vec4 kBtnHot{0.25f, 0.38f, 0.6f, 1};

  // --- 3D region label -------------------------------------------------------
  begin_ui(d.cam3dUI, 0, 0);
  ui.panel(10, 10, 250, 66, kPanel, kBorder);
  ui.text("FORGECORE 3D", 22, 18, 13, kText);
  ui.text("DRAG ORBIT  WHEEL ZOOM", 22, 40, 10, kDim);
  ui.text(std::string("LIGHTS: ") + (d.lights ? "ON" : "OFF"), 22, 56, 10,
          d.lights ? kGreen : kOff);
  ui.end();

  // --- 2D region: stats + buttons ----------------------------------------------
  begin_ui(d.cam2dUI, half, 0);
  ui.panel(10, 10, 230, 96, kPanel, kBorder);
  ui.text("FORGECORE 2D", 22, 18, 13, kText);
  char buf[128];
  std::snprintf(buf, sizeof(buf), "FPS %3.0f   %.1f MS", e.frame().fps, e.frame().frame_ms);
  ui.text(buf, 22, 40, 10, kText);
  std::snprintf(buf, sizeof(buf), "ENTITIES %zu", e.scene().entity_count());
  ui.text(buf, 22, 56, 10, kDim);
  std::snprintf(buf, sizeof(buf), "BODIES 2D %d  3D 1", (int)d.balls2d.size() + 1);
  ui.text(buf, 22, 72, 10, kDim);
  ui.bar(22, 88, 206, 6, std::min(1.0f, e.frame().frame_ms / 33.0f), kGreen,
         {0.1f, 0.14f, 0.22f, 1});

  float bx = 12, by = 122, bw = 62, bh = 22, gap = 8;
  auto btn = [&](const char* label, bool on, bool* toggle) {
    bool clicked = ui.button(label, bx, by, bw, bh, on ? kBtnHot : kBtn, kBtnHot, 11);
    if (clicked) { *toggle = !*toggle; sfx(e, "ui-click", 1250, 1250, 0.04f, 0.2f, Wave::Square); }
    ui.text(on ? "[ON]" : "[OFF]", bx + 4, by + bh + 3, 9, on ? kGreen : kOff);
    bx += bw + gap;
  };
  btn("SPIN", d.spin, &d.spin);
  btn("GRAV", d.gravity, &d.gravity);
  btn("LIGHT", d.lights, &d.lights);
  bool resetHot = ui.button("RESET", bx, by, bw, bh, kBtn, kBtnHot, 11);
  if (resetHot) reset_balls(e, d);
  ui.text("WASD MOVE  SPACE JUMP  CLICK BALL", 12, by + bh + 22, 10, kDim);
  ui.end();

  // --- bottom bar ----------------------------------------------------------------
  begin_ui(d.camBar, 0, regionH);
  ui.rect(0, 0, d.W, d.barH, {0.05f, 0.06f, 0.10f, 1});
  ui.rect(0, 0, d.W, 1, kBorder);
  ui.text(std::string("FORGECORE ENGINE v0.1.0   |   BACKEND: ") + e.renderer_name() +
              std::string("   |   ") + std::to_string(d.W) + "x" + std::to_string(d.H),
          12, 9, 11, kText);
  ui.text("[F1]SPIN [F2]GRAVITY [F3]LIGHTS [R]RESET", d.W - 340, 9, 11, kDim);
  ui.end();
}

// ---------------------------------------------------------------- systems

struct DemoUpdateSystem : System {
  Demo* d;
  const char* name() const override { return "DemoUpdate"; }
  void update(Engine& e, float dt) override { demo_update(e, *d, dt); }
};

struct HudSystem : System {
  Demo* d;
  const char* name() const override { return "Hud"; }
  void update(Engine& e, float) override { draw_hud(e, *d); }
};

struct StatsSystem : System {
  Demo* d;
  int timer = 0;
  const char* name() const override { return "Stats"; }
  void update(Engine& e, float) override {
    if (++timer % 20 != 0) return;
    char buf[256];
    std::snprintf(buf, sizeof(buf),
                  "{\"t\":\"stats\",\"fps\":%.1f,\"ms\":%.2f,\"ents\":%zu,\"backend\":\"%s\"}",
                  e.frame().fps, e.frame().frame_ms, e.scene().entity_count(),
                  e.renderer_name().c_str());
    e.append_stream_event(std::string(buf));
  }
};

// -------------------------------------------------------------------- main

int main(int argc, char** argv) {
  EngineConfig cfg;
  cfg.window.title = "ForgeCore Engine - Tech Demo";
  bool stream = false;
  int frames = 0;
  std::string screenshot;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--stream") stream = true;
    else if (a == "--frames" && i + 1 < argc) frames = std::atoi(argv[++i]);
    else if (a == "--screenshot" && i + 1 < argc) screenshot = argv[++i];
    else if (a == "--renderer" && i + 1 < argc) cfg.renderer = argv[++i];
    else if (a == "--width" && i + 1 < argc) cfg.window.width = std::atoi(argv[++i]);
    else if (a == "--height" && i + 1 < argc) cfg.window.height = std::atoi(argv[++i]);
    else if (a == "--help" || a == "-h") {
      std::printf("usage: techdemo [--width W] [--height H] [--stream] [--frames N] "
                  "[--screenshot out.ppm]\n");
      return 0;
    }
  }
  cfg.stream = stream;
  cfg.frame_limit = frames;
  cfg.screenshot_path = screenshot;

  Engine engine(cfg);
  Demo d;
  d.W = cfg.window.width;
  d.H = cfg.window.height;

  engine.add_system<TransformSystem>();
  engine.add_system<CameraSystem>();
  engine.add_system<LightsSystem>();
  engine.add_system<Physics2DSystem>();
  engine.add_system<SpherePhysics3DSystem>();
  {
    auto s = std::make_unique<DemoUpdateSystem>();
    s->d = &d;
    engine.add_system(std::move(s));
  }
  engine.add_render_system<Render3DSystem>();
  engine.add_render_system<Render2DSystem>();
  {
    auto s = std::make_unique<HudSystem>();
    s->d = &d;
    engine.add_render_system(std::move(s));
    auto st = std::make_unique<StatsSystem>();
    st->d = &d;
    engine.add_render_system(std::move(st));
  }

  // relay audio events to the browser (headless preview)
  engine.audio().set_listener([&](const AudioEvent& ev) {
    const char* wname = ev.params.wave == Wave::Sine ? "sine"
                      : ev.params.wave == Wave::Square ? "square"
                      : ev.params.wave == Wave::Saw ? "saw" : "noise";
    char buf[256];
    std::snprintf(buf, sizeof(buf),
                  "{\"t\":\"audio\",\"name\":\"%s\",\"f0\":%.1f,\"f1\":%.1f,\"dur\":%.3f,"
                  "\"vol\":%.2f,\"wave\":\"%s\"}",
                  ev.name.c_str(), ev.params.freq0, ev.params.freq1, ev.params.duration,
                  ev.params.volume, wname);
    engine.append_stream_event(std::string(buf));
  });

  engine.run([&](Engine& e) { build_scene(e, d); });

  std::printf("ForgeCore tech demo finished: renderer=%s window=%s\n",
              engine.renderer_name().c_str(), engine.window_backend_name().c_str());
  return 0;
}
