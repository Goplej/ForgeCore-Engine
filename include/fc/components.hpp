#pragma once
// ForgeCore standard components.

#include "ecs.hpp"
#include "math.hpp"
#include "render.hpp"

namespace fc {

// --- hierarchy / transforms ------------------------------------------------

struct Transform {
  Vec3 position{0, 0, 0};
  Vec3 scale{1, 1, 1};
  float rotZ = 0;      // radians, 2D convenience (rotates in the XY plane)
  Quat orientation{};  // 3D rotation; if w==0 && x==y==z==0 only rotZ is used
  bool use_quat() const { return orientation.x != 0 || orientation.y != 0 || orientation.z != 0 || orientation.w != 1; }
  Mat4 local_matrix() const {
    Mat4 rot = use_quat() ? orientation.toMat4() : Mat4::rotationZ(rotZ);
    return Mat4::translation(position) * rot * Mat4::scaling(scale);
  }
};

struct Parent {
  Entity parent;
};

struct WorldTransform {
  Mat4 world = Mat4::identity();
};

// --- 3D --------------------------------------------------------------------

struct Mesh {
  MeshId geometry = 0;
  Material material;
  uint32_t texture = 0;  // 0 = solid material
};

struct PointLight {
  Vec3 color{1.0f, 0.9f, 0.7f};
  float radius = 8.0f;
};

struct DirLight {
  Vec3 direction{0.0f, -1.0f, -0.4f};
  Vec3 color{0.9f, 0.95f, 1.0f};
};

// --- 2D --------------------------------------------------------------------

struct Sprite {
  uint32_t texture = 0;  // 0 = solid tint quad
  Vec2 size{32, 32};
  Vec4 tint{1, 1, 1, 1};
  float depth = 0;       // render order
};

struct Collider2D {
  // Static AABB (world space, centered on the entity's position).
  Vec2 size{0, 0};
};

// --- physics ----------------------------------------------------------------

struct RigidBody2D {
  Vec2 velocity{0, 0};
  float radius = 0;      // >0: circle body
  float mass = 1.0f;
  float restitution = 0.75f;
  float friction = 0.02f;
  bool gravity = true;
  float gravityAccel = 9.8f * 60.0f;  // px/s^2 demo-tuned
  bool asleep = false;
};

struct SphereBody {
  Vec3 velocity{0, 0, 0};
  float radius = 0.2f;
  float restitution = 0.7f;
  bool gravity = true;
  float gravityAccel = 9.8f;  // units/s^2
};

// --- cameras ----------------------------------------------------------------

struct Camera {
  bool perspective = true;
  bool clears = true;  // false = overlay camera (draws over existing content)
  float fov = 60.0f;   // degrees
  float znear = 0.1f;
  float zfar = 100.0f;
  float orthoHalfW = 6.0f;  // 2D cameras: half extents in world units
  float orthoHalfH = 4.0f;
};

struct Viewport {
  int x = 0, y = 0, w = 0, h = 0;  // screen rect for this camera
};

// Orbit-style mouse control for a 3D camera (applied by CameraSystem).
struct OrbitControls {
  float yaw = 0.6f;
  float pitch = 0.45f;
  float distance = 6.0f;
  Vec3 target{0, 0.8f, 0};
};

// Computed by CameraSystem; consumed by render systems.
struct CameraMatrices {
  Mat4 proj = Mat4::identity();
  Mat4 view = Mat4::identity();
  Vec3 eye{0, 0, 10};
};

// Static 3D arena for sphere bodies (axis-aligned box).
struct Box3D {
  Vec3 min{-4, 0, -4};
  Vec3 max{4, 8, 4};
};

// Associates a renderable with the camera that draws it.
struct CameraOf {
  Entity camera;
};

}  // namespace fc
