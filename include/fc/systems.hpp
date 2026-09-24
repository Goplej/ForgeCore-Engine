#pragma once
// ForgeCore built-in systems.

#include "engine_fwd.hpp"
#include "math.hpp"

namespace fc {

class System {
 public:
  virtual ~System() = default;
  virtual const char* name() const = 0;
  virtual void update(Engine& e, float dt) = 0;
};

// Computes WorldTransform for every Transform (following Parent links).
class TransformSystem : public System {
 public:
  const char* name() const override { return "Transform"; }
  void update(Engine& e, float) override;
};

// Applies OrbitControls (mouse drag / wheel) to a camera's Transform and
// materializes CameraMatrices (proj/view/eye) for render systems.
class CameraSystem : public System {
 public:
  const char* name() const override { return "Camera"; }
  void update(Engine& e, float dt) override;
};

// Collects DirLight / PointLight components into FrameData::lights.
class LightsSystem : public System {
 public:
  const char* name() const override { return "Lights"; }
  void update(Engine& e, float) override;
};

// 2D rigid bodies: gravity, circle/AABB + circle/circle collision,
// restitution, contact audio events.
class Physics2DSystem : public System {
 public:
  const char* name() const override { return "Physics2D"; }
  float bounceVolume = 0.25f;
  void update(Engine& e, float dt) override;
};

// 3D sphere bodies against a Box3D arena: gravity + bounce + audio events.
class SpherePhysics3DSystem : public System {
 public:
  const char* name() const override { return "SpherePhysics3D"; }
  float bounceVolume = 0.2f;
  void update(Engine& e, float dt) override;
};

// 3D lit pass: draws Mesh entities per perspective camera.
class Render3DSystem : public System {
 public:
  const char* name() const override { return "Render3D"; }
  Vec3 clearColor{0.05f, 0.07f, 0.12f};
  void update(Engine& e, float dt) override;
};

// 2D pass: draws Sprite entities (depth-sorted) per ortho camera.
class Render2DSystem : public System {
 public:
  const char* name() const override { return "Render2D"; }
  Vec3 clearColor{0.10f, 0.08f, 0.16f};
  void update(Engine& e, float dt) override;
};

}  // namespace fc
