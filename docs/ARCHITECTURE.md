# ForgeCore Engine — Architecture

This document explains how the engine is put together. The public API is
documented in the header comments under `include/fc/`; this file covers the
design decisions and the internal contracts between subsystems.

## 1. Layers

```
        ┌─────────────────────────────────────────────────────────┐
        │  application / demo   (examples/techdemo/main.cpp)      │
        │  scene construction, custom systems, UI, audio events   │
        └──────────────┬──────────────────────────────────────────┘
                       │  Engine& (window, input, scene, audio, frame data)
        ┌──────────────▼──────────────────────────────────────────┐
        │  Engine (src/engine.cpp)                                 │
        │  fixed-timestep update loop + render pass + frame I/O    │
        └───────┬────────────────────────────────┬────────────────┘
                │                                │
   ┌────────────▼────────────┐       ┌───────────▼────────────────┐
   │  Systems (src/systems…) │       │  Renderer interface        │
   │  Transform, Camera,     │       │  include/fc/render.hpp     │
   │  Lights, Physics2D/3D,  │       │   SoftRenderer (CPU)       │
   │  Render3D, Render2D     │       │   GLRenderer (GL 3.3)      │
   └────────────┬────────────┘       └───────────┬────────────────┘
                │                                │
        ┌───────▼────────────────────────────────▼───────────────┐
        │  World (ECS) · math · components · geometry · audio    │
        │  Window (GLX/X11 or headless) · StreamChannel (shm)    │
        └────────────────────────────────────────────────────────┘
```

The engine library (`forgecore`) is the only compiled code; demos link
against it. There is exactly one public entry point: `Engine::run(on_init)`.

## 2. Main loop

`Engine::run` (src/engine.cpp):

1. `on_init(engine)` — the application builds the scene and registers
   systems.
2. Per frame:
   - `window.poll_events(input)` + `stream.drain_input(input)` (streamed
     input, when running with `--stream`).
   - **Fixed-timestep updates**: accumulator with `step = 1/120 s`, at most 8
     substeps per frame, `dt` clamped to 0.25 s. All `updates_` systems run
     in registration order with the *same* `step` — simulation is
     deterministic w.r.t. frame timing.
   - **Render pass**: `renderer.begin_frame(w, h)`, then each `renders_`
     system in order, then `renderer.end_frame()`.
   - Frame counters (EMA fps/ms) in `FrameData`.
   - Frame output: `read_frame_rgb8` → shm publish (stream mode) →
     `window.present()` → optional PPM screenshot.
   - Optional `target_fps` sleep (default 60) so the CPU backend doesn't burn
     both cores.

Update vs render split: anything that mutates simulation state belongs in an
update system; anything that draws belongs in a render system. Render systems
must not mutate component state the next update step depends on.

## 3. ECS (`include/fc/ecs.hpp`)

- `Entity` is a 32-bit handle; `World` owns one `Store<T>` per component type:
  `unordered_map<Entity, T>` + a creation-ordered `vector<Entity>`.
- `World::each<Ts...>(fn)` iterates the *smallest* store among `Ts...` and
  verifies the rest (`all_have`). This is O(smallest) with a cheap filter —
  fine at scene scale, no archetype bookkeeping.
- `attach<T>(e, comp)` is by-value; `get<T>(e)` returns a stable pointer
  (stores are never reallocated per-entity).
- `destroy(e)` removes from every store; entity ids are recycled.
- Systems get `Engine&`, from which they obtain `scene().world()` — there is
  one world per engine.

Component catalog lives in `include/fc/components.hpp`. Render-relevant
components (`Mesh`, `Material`, `Sprite`, `CameraOf`) are paired with
transform/camera components at draw time; nothing is pre-baked, so entities
can move, re-parent, or be re-lit any update tick.

## 4. Systems

Built-in systems (`src/systems.cpp`, declared in `include/fc/systems.hpp`):

| System | Phase | Contract |
|---|---|---|
| `TransformSystem` | update | For every `Transform`, walks `Parent` links (≤ 64 deep) and writes `WorldTransform::world` (local × ancestors). |
| `CameraSystem` | update | Pass 1: orbit input (drag → yaw/pitch, wheel → distance) on `OrbitControls` cameras. Pass 2: materializes orbit position into the camera's `Transform`, then computes `CameraMatrices{proj, view, eye}` (perspective: `perspective()×lookAt`; ortho: `ortho()×world⁻¹`). |
| `LightsSystem` | update | Copies `DirLight` / `PointLight` components into `FrameData::lights` (1 directional + 4 point slots). |
| `Physics2DSystem` | update | 2D circles vs AABB + circle-circle; restitution; writes `RigidBody2D` velocity/position; emits contact audio events. |
| `SpherePhysics3DSystem` | update | 3D spheres with gravity vs a static `Box3D` arena; bouncing. |
| `Render3DSystem` | render | For each perspective `Camera`: scissor to its `Viewport`, clear (if `Camera.clears`), depth test on, then every `Mesh` whose `CameraOf == cam` with its `WorldTransform`. |
| `Render2DSystem` | render | Same, for ortho cameras: sprites (`builtin_quad` + model matrix), depth-sorted by `Sprite.depth`, blending on, depth test off. |

The viewport pattern: **every camera owns a `Viewport` (screen rect)** and the
render systems set the scissor to it before drawing. That is the entire
mechanism behind split screens and HUD overlays — no engine support code.
Overlay cameras set `Camera.clears = false` so they composite over existing
content (the demo uses this for the two HUD panels and the status bar).

## 5. Rendering

### 5.1 The interface

`fc::Renderer` (include/fc/render.hpp) is a pure virtual frame protocol:

```
begin_frame(w, h) → set_scissor / clear / set_depth_test / set_blend
  → use_program(Lit|Unlit) / set_matrix("proj"|"view"|"model")
  / set_eye / set_lights / set_material / bind_texture
  → draw(MeshId) → end_frame → read_frame_rgb8
```

`MeshId` is an engine-owned mesh (interleaved `pos3,nrm3,uv2` float vertices +
uint32 indices). `builtin_quad()` is a shared unit quad (y-flipped UVs) that
is the sprite/UI primitive.

### 5.2 SoftRenderer (CPU)

`src/render/soft/soft_renderer.cpp`:

- Double-buffered RGBA8 + float depth (`1.0` = far, LESS-equal test on
  `(z_ndc+1)·0.5`).
- **Clipping**: each triangle is clipped against the near (`z+w ≥ 0`) and far
  (`w−z ≥ 0`) half-spaces in clip space. Barycentric coordinates are computed
  from the *clip-space* (x,y) before projection, so attributes are recovered
  exactly for interpolated (clipped) vertices — no per-attribute clipping.
- Perspective divide → screen space inside the scissor; edge-function scanline
  raster with per-pixel Blinn lighting via the shared `light_fragment()`.
- Front/back winding: GL convention (CCW front) is normalized by swapping on
  negative signed area so both backends draw the same faces.
- Textures: nearest-neighbour 8-bit RGBA; `bind_texture(0)` = solid material.

### 5.3 GLRenderer

`src/render/gl/gl_renderer.cpp`:

- **No GL headers, no link-time dependency**: `gl_loader.cpp` `dlopen`s
  `libGL.so.1` (then `libGLX.so.1`, then `libOpenGL.so.0`) and resolves ~40
  entry points via `dlsym`. If anything is missing the factory falls back.
- OpenGL 3.3 core profile, `#version 130` GLSL (shared model, see below),
  one VBO/element buffer per draw, uniforms by location (no VAO — the
  attribute layout is fixed per program).
- `set_scissor` y-flip (`H−y−h`) for GL's bottom-left origin.

### 5.4 Shared lighting

`fc::light_fragment(N, V, P, mat, lights, emissive)` in
`include/fc/render.hpp` and `light_fragment()` in `src/render/gl/glsl.hpp`
implement the *same* model (kept in lockstep by convention):

```
col = ambient (0.12, 0.13, 0.16) · albedo
    + dirLight.color · (N·L · albedo + BlinnSpec)
    + Σ pointLights: color · (1−d/r)² · (N·L · albedo + BlinnSpec)
    + emissive
```

One directional + up to 4 point lights per frame, quadratic radius falloff,
Blinn-Phong specular with per-material shininess/strength.

## 6. Math (`include/fc/math.hpp`)

Column-major `Mat4` (OpenGL layout), `Vec2/3/4`, `Quat` (Hamilton
convention, `q·p`), `perspective / ortho / lookAt / translation / scaling /
rotationXYZ`, and a Gauss-Jordan `inverse()` (augmented matrix transcribed
row-major, both halves operated on, written back column-major — the original
implementation operated only on the left half and silently returned the
identity for translations; `tests/test_math.cpp` guards this).

## 7. Input & window

- `Window` factory (`src/platform/platform.cpp`): if an X display is
  available, `GLXWindow` (dlopen'd X11 via `xlib_min.hpp`, GLX context when
  GL is present, `XLookupKeysym` key mapping); otherwise a `HeadlessWindow`.
- `Input` is per-frame: `begin_frame` snapshots pressed/just-pressed keys and
  mouse state; `end_frame` clears edge state. Streamed input
  (`StreamChannel::drain_input`) feeds the same `Input` with JSON lines
  (`{"t":"key"|"mouse"|"scroll", …}`), so the browser preview drives the
  engine exactly like a local window does.

## 8. Streaming & headless preview

`--stream` swaps the presentation path for POSIX shared memory
(`StreamChannel`, `/dev/shm` by default, `FC_SHM_DIR` to override):

| File | Direction | Format |
|---|---|---|
| `fc_frame.rgb` | engine → out | raw RGB8, `W·H·3` bytes, rewritten each frame |
| `fc_input` | in → engine | NDJSON: `{"t":"key","k":"a","d":1}`, `{"t":"mouse","x":..,"y":..,"b":"l","d":1}`, `{"t":"scroll","dy":1}` |
| `fc_events` | engine → out | NDJSON: audio events `{"t":"audio","name","f0","f1","dur","vol","wave"}` and periodic stats |

`tools/stream_server.py` (stdlib + Pillow) serves the page, converts the raw
frame to JPEG for `/frame` (polled with `requestAnimationFrame`), appends
browser input to `fc_input`, and relays `fc_events` over SSE. The page
synthesizes audio events with WebAudio (one click to unlock the
`AudioContext` for autoplay policy). This is how the engine is viewable and
playable in a display-less sandbox.

## 9. Audio

`Audio` (src/audio.cpp): 16-voice procedural mixer at 44.1 kHz (sine / square
/ saw / noise, two-note glide `freq0 → freq1`, attack + exponential decay).
`play(SoundParams, name)` routes through a listener callback — the demo
relays events to the stream channel (browser WebAudio) and can also sink to a
WAV file via `FC_AUDIO_WAV=/path/file.wav` for offline inspection. Physics
contacts (2D landings, 3D bounces) and UI clicks are wired to `sfx(...)` in
the demo.

## 10. UI

`fc::UI` (src/ui.cpp) is immediate-mode, drawn as textured-less quads through
the `Renderer` (so it works on both backends): `text` (5×7 bitmap font,
pixel-sized), `rect`, `panel`, `button` (hover/click from `Input` in local
pixel space), `bar`. `begin(pv, ox, oy, hw, hh)` binds it to an ortho camera
(`pv = proj×view`): 1 UI pixel = 1 world unit, `(ox, oy)` maps mouse
coordinates into the overlay's local space. Convention: overlay cameras have
`clears = false`, and the *draw code* sets the scissor to the camera's
`Viewport` before drawing (render systems do this automatically; custom HUD
code must too, since the previous camera's scissor is still in effect).

## 11. Testing

- `tests/test_math.cpp` — matrix inverse (translation / rotation / rigid /
  lookAt), ortho near-far mapping. This is the regression home for the
  `inverse()` bug class.
- `tests/test_soft.cpp` — rasterizer sanity: full-screen quad renders the
  expected pixels; also guards the near/far clip half-spaces.
- Run with `ctest --test-dir build`.

## 12. Known constraints

- The CPU rasterizer is a quality backend, not a fast one: ~15–25 ms/frame at
  1280×720 on modest hardware (hence the 60 fps cap). The GL backend is the
  performance path on real hardware.
- Nearest-neighbour sampling, no MSAA in the soft backend (temporal aliasing
  on the torus is visible — it is a CPU reference rasterizer).
- GLRenderer is compile-checked everywhere but only exercised where a GLX
  context exists; the `auto` probe guarantees a graceful fallback.
- ECS stores grow-only (no swap-remove), fine for scene-scale entity counts.
