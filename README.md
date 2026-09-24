# ForgeCore Engine

[![CI](https://github.com/Goplej/ForgeCore-Engine/actions/workflows/ci.yml/badge.svg)](https://github.com/Goplej/ForgeCore-Engine/actions/workflows/ci.yml)

A small, dependency-light **C++17 game engine** with a clean component-based
(ECS) architecture, supporting **2D and 3D rendering**, a shared lighting
model, 2D/3D physics, procedural audio, and an immediate-mode UI.

The engine is backend-agnostic at the `fc::Renderer` interface level:

| Backend | What it is | Where it runs |
|---|---|---|
| **SoftRenderer** | CPU software rasterizer (near/far clipping, depth test, blending, per-pixel Blinn lighting, textures) | Anywhere — no GPU required (headless, CI, CI sandboxes) |
| **GLRenderer** | OpenGL 3.3 core (loaded at runtime via `dlopen`/`dlsym`, no GL headers needed) | Desktop machines with a GPU + X11/GLX |

The same binary picks its backend at startup (`--renderer auto` by default):
if an X display with a working GLX context is available it uses OpenGL,
otherwise it falls back to the CPU rasterizer. Frame output is identical:
each frame is a 1280×720 RGB8 image you can screenshot or stream.

```
┌───────────────────────────── tech demo (1280×720) ─────────────────────────────┐
│  3D REGION (left)                │  2D REGION (right)                           │
│  orbit camera, lit torus/cube/   │  top-down stage, WASD player + jump,         │
│  sphere, dir + orbiting point    │  click-to-spawn physics balls, glow sprites  │
│  light, floor grid               │                                               │
├──────────────────────────────────┴──────────────────────────────────────────────┤
│  status bar: backend, resolution, hotkeys (F1 spin / F2 gravity / F3 lights)    │
└─────────────────────────────────────────────────────────────────────────────────┘
```

## Build

Requirements: CMake ≥ 3.16, a C++17 compiler (GCC 10+ / Clang 10+), Make or
Ninja. No other dependencies — the engine is header + stdlib only.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build            # unit tests (math, rasterizer)
```

CMake options:

| Option | Values | Default | Meaning |
|---|---|---|---|
| `FC_RENDERER` | `auto`, `soft`, `gl` | `auto` | Backend selection (see above) |
| `FC_BUILD_EXAMPLES` | `ON`, `OFF` | `ON` | Build the tech demo |

## Run the tech demo

```sh
build/bin/techdemo                          # windowed (desktop) or headless (fallback)
build/bin/techdemo --width 1280 --height 720
build/bin/techdemo --frames 300 --screenshot out.ppm   # headless capture
build/bin/techdemo --stream                          # stream via /dev/shm (below)
build/bin/techdemo --renderer gl|soft|auto
```

### Controls

| Input | Action |
|---|---|
| Mouse drag (3D region) | orbit camera |
| Mouse wheel (3D region) | zoom |
| WASD / arrows (2D region) | move player |
| Space | jump |
| Click (2D region) | spawn a physics ball (max 10) |
| F1 / F2 / F3 | toggle spin / gravity / lights |
| R | reset balls |

The in-scene **SPIN / GRAV / LIGHT / RESET** buttons are live UI (immediate
mode), clickable in both windowed and streamed modes.

### Headless live preview (no display needed)

The engine can publish frames over POSIX shared memory and receive input back,
which a tiny Python server turns into a browser page with WebAudio:

```sh
build/bin/techdemo --stream &
python3 tools/stream_server.py --port 8000
# open http://localhost:8000  (click "enable sound" to hear audio)
```

Files used in `/dev/shm`: `fc_frame.rgb` (RGB8 framebuffer), `fc_input`
(JSON-line input), `fc_events` (JSON-line audio/stats events → SSE).

## Engine API tour

```cpp
#include "fc/engine.hpp"
#include "fc/systems.hpp"
#include "fc/scene.hpp"

using namespace fc;

int main() {
  EngineConfig cfg;
  cfg.window.width = 1280;
  cfg.window.height = 720;
  Engine engine(cfg);

  // fixed-timestep update systems, in order
  engine.add_system<TransformSystem>();        // local -> world matrices
  engine.add_system<CameraSystem>();           // orbit input + proj/view/eye
  engine.add_system<LightsSystem>();           // gather light components
  engine.add_system<Physics2DSystem>();        // 2D circles + AABB collision
  engine.add_system<SpherePhysics3DSystem>();  // 3D spheres vs. arena

  // draw-pass systems, in order
  engine.add_render_system<Render3DSystem>();  // per-camera viewports, lit meshes
  engine.add_render_system<Render2DSystem>();  // per-camera viewports, sprites
  // ...your own UI/systems here (see examples/techdemo/main.cpp)

  engine.run([](Engine& e) {
    Scene& sc = e.scene();
    // spawn = create entity + attach components (variadic)
    Entity cam = sc.spawn(
        Camera{true, true, 55.f, 0.1f, 100.f},  // perspective, clears, fov...
        Viewport{0, 0, 640, 690},
        Transform{{0, 0, 10}},
        OrbitControls{});                        // drag + wheel
    Entity torus = sc.spawn(
        Transform{{0, 1.8f, 0}},
        Mesh{geometry::torus(64, 32, 1.6f, 0.55f)},
        Material{{0.2f, 0.5f, 0.9f, 1}},
        CameraOf{cam});
  });
}
```

ECS queries (`World`):

```cpp
world.each<Mesh, WorldTransform>([](Entity en, Mesh* m, WorldTransform* wt) {
  // smallest-store iteration; order = creation order
});
world.get<CameraMatrices>(cam);          // typed lookup (nullptr if absent)
auto& t = world.attach<Transform>(en, Transform{});   // add component
```

Scene content:

- **Components** (`include/fc/components.hpp`): `Transform`, `Camera`,
  `Viewport`, `OrbitControls`, `Mesh`, `Material`, `Sprite`, `Collider2D`,
  `RigidBody2D`, `SphereBody`, `Box3D`, `DirLight`, `PointLight`, `CameraOf`.
- **Geometry** (`fc::geometry`): `cube()`, `torus()`, `sphere()`, `plane()` —
  interleaved `{pos, normal, uv}` + index data.
- **Audio** (`e.audio().play(SoundParams{name, freq0, freq1, duration, volume,
  wave})`): 16-voice procedural mixer (sine/square/saw/noise, attack + decay).
  On desktop this can sink to any device you wire up; in headless mode events
  are emitted for the stream relay (the browser renders them with WebAudio).
- **UI** (`fc::UI`): immediate-mode `text / panel / button / bar / rect` on
  top of any ortho "overlay" camera (`Camera.clears = false`).

## Repository layout

```
include/fc/          public headers (ecs, engine, render, math, components, ...)
src/                 engine library (forgecore)
  engine.cpp         main loop: fixed-timestep updates + render pass
  systems.cpp        built-in systems (transform/camera/lights/physics/render)
  ui.cpp             immediate-mode UI + 5x7 bitmap font
  audio.cpp          procedural mixer
  geometry.cpp       mesh generators
  render/soft/       CPU rasterizer
  render/gl/         OpenGL 3.3 backend (dlopen'd, no headers)
  platform/          window factory, GLX/X11 window, shm stream channel
examples/techdemo/   the demo (scene, HUD, input mapping)
tests/               unit tests (ctest)
tools/stream_server.py  headless preview server (frames/JPEG + input + SSE)
docs/ARCHITECTURE.md   deep dive
```

## Design notes

- One `Renderer` interface, two backends, identical frame protocol
  (`begin_frame → set state → draw → end_frame`). The soft backend makes the
  whole engine testable and runnable without a GPU; the GL backend makes it a
  real-time GPU engine. Lighting is shared: `fc::light_fragment()` (C++) and
  the GLSL fragment shader implement the same model (1 directional + 4 point
  lights, quadratic radius falloff, Blinn specular, ambient fill, emissive).
- Systems are plain objects with `update(Engine&, dt)`; update systems run on a
  fixed 120 Hz timestep (accumulator, max 8 substeps), render systems run once
  per presented frame. Registration order = execution order.
- Cameras own a `Viewport` (screen rect); each render pass sets the scissor to
  the viewport, so multiple cameras compose into one frame (split screen,
  overlay HUDs) with zero engine support code.
- Depth: `z_ndc` → `[0,1]` LESS-equal test in the soft renderer; GL uses the
  standard `[0,1]` depth buffer. Clip-space near/far clipping with
  barycentric attribute reconstruction (attributes survive clipping for free).
