#pragma once
// ForgeCore ECS — small, clean, component-based world.
//
// Design:
//   * Entity is a compact 32-bit handle.
//   * Each component type has a store: map<entity, T> + ordered entity list.
//   * Queries iterate the smallest matching store and filter the rest —
//     fast at scene-scale entity counts with a trivial memory layout.
//
// This keeps the component model (spawn with components, query by component
// tuple) without archetype bookkeeping overhead.

#include <cstdint>
#include <functional>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace fc {

using Entity = uint32_t;
constexpr Entity kInvalidEntity = 0;

namespace detail {

struct IStore {
  virtual ~IStore() = default;
  virtual void* get(Entity e) = 0;
  virtual bool contains(Entity e) const = 0;
  virtual void remove(Entity e) = 0;
  virtual size_t size() const = 0;
  virtual const std::vector<Entity>& entities() const = 0;
};

template <class T>
struct Store : IStore {
  std::unordered_map<Entity, T> map;
  std::vector<Entity> list;

  T* get(Entity e) override {
    auto it = map.find(e);
    return it == map.end() ? nullptr : &it->second;
  }
  bool contains(Entity e) const override { return map.count(e) > 0; }
  void remove(Entity e) override {
    map.erase(e);
    for (size_t i = 0; i < list.size(); ++i)
      if (list[i] == e) { list[i] = list.back(); list.pop_back(); break; }
  }
  size_t size() const override { return map.size(); }
  const std::vector<Entity>& entities() const override { return list; }
};

}  // namespace detail

class World {
 public:
  Entity create() {
    Entity e;
    if (!free_.empty()) { e = free_.back(); free_.pop_back(); }
    else e = ++next_id_;
    if (e >= alive_.size()) alive_.resize(e + 1, false);
    alive_[e] = true;
    return e;
  }

  void destroy(Entity e) {
    if (!alive(e)) return;
    for (auto& en : entries_) en.store->remove(e);
    alive_[e] = false;
    free_.push_back(e);
  }

  bool alive(Entity e) const { return e != kInvalidEntity && e < alive_.size() && alive_[e]; }

  size_t entity_count() const {
    size_t n = 0;
    for (bool b : alive_) n += b ? 1 : 0;
    return n;
  }

  void destroy_all() {
    entries_.clear();
    alive_.assign(1, false);
    free_.clear();
    next_id_ = 0;
  }

  template <class T>
  T& attach(Entity e, T comp) {
    auto& s = store<T>();
    auto [it, inserted] = s.map.try_emplace(e, std::move(comp));
    if (inserted) s.list.push_back(e);
    return it->second;
  }

  template <class T>
  T* get(Entity e) {
    auto& s = store<T>();
    auto it = s.map.find(e);
    return it == s.map.end() ? nullptr : &it->second;
  }
  template <class T>
  const T* get(Entity e) const {
    auto& s = const_cast<World*>(this)->store<T>();
    auto it = s.map.find(e);
    return it == s.map.end() ? nullptr : &it->second;
  }
  template <class T>
  bool has(Entity e) const {
    auto& s = const_cast<World*>(this)->store<T>();
    return s.map.count(e) > 0;
  }
  template <class T>
  void remove(Entity e) { store<T>().remove(e); }

  // Iterate every entity that has all of Ts, in creation order.
  template <class... Ts, class Fn>
  void each(Fn&& fn) {
    if constexpr (sizeof...(Ts) == 0) {
      for (Entity e = 1; e < alive_.size(); ++e)
        if (alive_[e]) fn(e);
    } else {
      const detail::IStore* chosen = pick_store<Ts...>();
      for (Entity e : chosen->entities()) {
        if (!alive(e)) continue;
        if (all_have<Ts...>(e)) fn(e, get<Ts>(e)...);
      }
    }
  }

 private:
  struct StoreEntry {
    std::type_index type;
    std::unique_ptr<detail::IStore> store;
  };

  template <class T>
  detail::Store<T>& store() {
    for (auto& en : entries_)
      if (en.type == std::type_index(typeid(T)))
        return *static_cast<detail::Store<T>*>(en.store.get());
    entries_.push_back(StoreEntry{std::type_index(typeid(T)), std::make_unique<detail::Store<T>>()});
    return *static_cast<detail::Store<T>*>(entries_.back().store.get());
  }

  const detail::IStore* pick_store() { return nullptr; }
  template <class A, class... R>
  const detail::IStore* pick_store() {
    const detail::Store<A>& a = store<A>();
    const detail::IStore* best = &a;
    size_t bestn = a.size();
    auto consider = [&](const detail::IStore* r) {
      if (r->size() < bestn) { best = r; bestn = r->size(); }
    };
    (consider(&store<R>()), ...);
    return best;
  }

  template <class... Ts>
  bool all_have(Entity e) {
    bool ok = true;
    ((ok = ok && store<Ts>().map.count(e) > 0), ...);
    return ok;
  }

  std::vector<StoreEntry> entries_;
  std::vector<bool> alive_{1, false};
  std::vector<Entity> free_;
  Entity next_id_ = 0;
};

}  // namespace fc
