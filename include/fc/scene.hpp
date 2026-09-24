#pragma once
// ForgeCore Scene — a World plus convenient spawn/remove helpers.

#include "ecs.hpp"

namespace fc {

class Scene {
 public:
  Entity spawn() { return world_.create(); }

  template <class... Cs>
  Entity spawn(Cs&&... comps) {
    Entity e = world_.create();
    (world_.attach(e, std::forward<Cs>(comps)), ...);
    return e;
  }

  Entity root() {
    if (root_ == kInvalidEntity) {
      root_ = world_.create();
      world_.attach<Transform>(root_, Transform{});
    }
    return root_;
  }

  void destroy(Entity e) {
    if (e == root_) root_ = kInvalidEntity;
    world_.destroy(e);
  }

  void clear() {
    world_.destroy_all();
    root_ = kInvalidEntity;
  }

  World& world() { return world_; }
  const World& world() const { return world_; }

  size_t entity_count() const { return world_.entity_count(); }

 private:
  World world_;
  Entity root_ = kInvalidEntity;
};

}  // namespace fc
