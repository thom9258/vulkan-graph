#pragma once

#include <concepts>
#include <cstdint>
#include <functional>
#include <numeric>
#include <optional>
#include <ranges>
#include <type_traits>
#include <vector>

namespace transform_hierarchy {

class transform_id_t {
public:
  using index_t = std::size_t;
  using generation_t = std::uint32_t;

  static constexpr const index_t invalid_index_v =
      std::numeric_limits<index_t>::max();

  static constexpr const generation_t invalid_generation_v =
      std::numeric_limits<generation_t>::max();

  constexpr transform_id_t() = delete;
  constexpr transform_id_t(index_t index, generation_t generation) noexcept
      : _index(index), _generation(generation) {}

  constexpr transform_id_t(const transform_id_t &) noexcept = default;
  constexpr transform_id_t(transform_id_t &&) noexcept = default;
  constexpr transform_id_t &
  operator=(const transform_id_t &) noexcept = default;
  constexpr transform_id_t &operator=(transform_id_t &&) noexcept = default;

  constexpr auto operator==(const transform_id_t &other) const noexcept
      -> bool {
    return _index == other.index() && _generation == other._generation;
  }

  constexpr auto index() const -> index_t { return _index; }
  constexpr auto generation() const -> generation_t { return _generation; }

private:
  index_t _index{invalid_index_v};
  generation_t _generation{invalid_generation_v};
};

static constexpr const transform_id_t
    invalid_transform_id(transform_id_t::invalid_index_v,
                         transform_id_t::invalid_generation_v);

// child_local_from_global

namespace detail {

template <typename t_location>
using child_global_from_local_t = t_location (*)(t_location parent_global,
                                                 t_location child_local);

template <typename t_location>
using child_local_from_global_t = t_location (*)(t_location parent_global,
                                                 t_location child_global);

} // namespace detail

template <class t_location,
          detail::child_global_from_local_t<t_location>
              child_global_from_local_location,
          detail::child_local_from_global_t<t_location>
              child_local_from_global_location>
class transform_hierarchy_t {
public:
  using location_t = std::remove_cvref_t<t_location>;
  using index_t = transform_id_t::index_t;

  struct transform_t {
    using generation_t = transform_id_t::generation_t;

    transform_id_t parent{invalid_transform_id};
    location_t local_location{};
    location_t global_location{};
    generation_t generation{0};
    bool dirty{false};
    bool active{false};
  };

  constexpr transform_hierarchy_t(std::size_t count) {
    _transforms.resize(count);
  }

  [[nodiscard]] constexpr auto add(location_t location)
      -> std::optional<transform_id_t> {
    return add_child_local_location(location, invalid_transform_id);
  }

  [[nodiscard]] constexpr auto add_child_global_location(location_t location,
                                                         transform_id_t parent)
      -> std::optional<transform_id_t> {

    auto parent_global = global_location(parent);
    if (!parent_global.has_value()) {
      return std::nullopt;
    }

    location_t child_local =
        std::invoke(child_local_from_global_location, *parent_global, location);

    return add_child_local_location(child_local, parent);
  }

  [[nodiscard]] constexpr auto add_child_local_location(location_t location,
                                                        transform_id_t parent)
      -> std::optional<transform_id_t> {
    // TODO: make simpler by making a find_next_inactive() function.
    for (auto [index, transform] : _transforms | std::views::enumerate) {
      if (!transform.active) {
        transform.active = true;
        transform.local_location = location;

        if (transform_t *parent_transform = find(parent)) {
          transform.parent = parent;
          transform.dirty = true;
        } else {
          transform.parent = invalid_transform_id;
          transform.global_location = transform.local_location;
        }

        return transform_id_t(index, transform.generation);
      }
    }

    return std::nullopt;
  }

  [[nodiscard]] constexpr auto children(transform_id_t &id)
      -> std::vector<transform_id_t> {
    transform_t *transform = find(id);
    if (transform == nullptr) {
      return {};
    }

    std::vector<transform_id_t> children;
    for (auto [index, child] : _transforms | std::views::enumerate) {
      if (child.active && child.parent == id) {
        children.emplace_back(index, child.generation);
      }
    }

    return children;
  }

  [[nodiscard]] constexpr auto roots() -> std::vector<transform_id_t> {
    std::vector<transform_id_t> roots;
    for (auto [index, child] : _transforms | std::views::enumerate) {
      if (child.active && child.parent == invalid_transform_id) {
        roots.emplace_back(index, child.generation);
      }
    }

    return roots;
  }

  constexpr auto remove_and_preserve_children(transform_id_t &id) -> void {
    transform_t *transform = find(id);
    if (transform == nullptr) {
      return;
    }

    transform->active = false;
    transform->generation++;

    for (transform_t &child : _transforms) {
      if (child.parent == id) {
        child.parent = invalid_transform_id;
        child.dirty = true;
      }
    }
  }

  [[nodiscard]] constexpr auto is_valid(transform_id_t &id) -> bool {
    if (id.index() == transform_id_t::invalid_index_v ||
        id.generation() == transform_id_t::invalid_generation_v) {
      return false;
    }

    if (id.index() >= _transforms.size()) {
      return false;
    }

    transform_t &transform = _transforms[id.index()];
    return transform.generation == id.generation() && transform.active == true;
  }

  constexpr auto global_location(transform_id_t &id)
      -> std::optional<location_t> {
    transform_t *transform = find(id);
    if (transform == nullptr) {
      return std::nullopt;
    }

    if (transform->parent == invalid_transform_id) {
      transform->dirty = false;
      transform->global_location = transform->local_location;
      return transform->global_location;
    }

    recalculate_parent_location_chain(*transform, id.index());
    return transform->global_location;
  }

  constexpr auto local_location(transform_id_t &id)
      -> std::optional<location_t> {
    if (!is_valid(id)) {
      return std::nullopt;
    }

    transform_t *transform = find(id);
    if (transform == nullptr) {
      return std::nullopt;
    }

    return transform->local_location;
  }

  constexpr auto set_local_location(transform_id_t &id, location_t location)
      -> void {
    transform_t *transform = find(id);
    if (transform == nullptr) {
      return;
    }

    transform->local_location = location;
    transform->dirty = true;
  }

  constexpr auto set_global_location(transform_id_t &id, location_t location)
      -> void {
    transform_t *transform = find(id);
    if (transform == nullptr) {
      return;
    }

    auto parent_global = global_location(transform->parent);
    if (!parent_global.has_value()) {
      set_local_location(id, location);
      return;
    }

    location_t child_local =
        std::invoke(child_local_from_global_location, *parent_global, location);

    transform->local_location = child_local;
    transform->dirty = true;
  }

  constexpr auto parent(transform_id_t &id) -> transform_id_t {
    transform_t *transform = find(id);
    if (transform == nullptr) {
      return invalid_transform_id;
    }

    return transform->parent;
  }

  constexpr auto unparent(transform_id_t &id) -> void {
    transform_t *transform = find(id);
    if (transform == nullptr) {
      return;
    }

    transform->parent = invalid_transform_id;
    transform->dirty = true;
  }

  constexpr auto reparent(transform_id_t &id, transform_id_t &parent) -> void {
    transform_t *transform = find(id);
    if (transform == nullptr) {
      return;
    }

    if (!is_valid(parent)) {
      return;
    }

    transform->parent = parent;
    transform->dirty = true;
  }

  [[nodiscard]] constexpr auto unsafe_find(transform_id_t &id)
      -> transform_t * {
    return find(id);
  }

private:
  constexpr auto recalculate_parent_location_chain(transform_t &transform,
                                                   index_t index) -> void {
    std::vector<index_t> parent_indices;
    parent_indices.push_back(index);

    transform_id_t parent = transform.parent;
    while (is_valid(parent)) {
      parent_indices.push_back(parent.index());
      parent = _transforms[parent.index()].parent;
    }

    // TODO: additional optimization can be made here, currently we naively
    //       recalculate the entire chain, even though the top of the chain
    //       can be perfectly fine, to fix this we just pop parents until we
    //       find the first dirty one, then proceed with updating from there.

    {
      index_t parent_index = parent_indices.back();
      if (_transforms[parent_index].dirty) {
        transform_t &parent = _transforms[parent_index];
        parent.global_location = parent.local_location;
        parent.dirty = false;
      }

      for (auto child_index :
           parent_indices | std::views::reverse | std::views::drop(1)) {
        transform_t &parent = _transforms[parent_index];
        transform_t &child = _transforms[child_index];
        child.global_location =
            std::invoke(child_global_from_local_location,
                        parent.global_location, child.local_location);

        child.dirty = false;
        parent_index = child_index;
      }
    }
  }

  [[nodiscard]] constexpr auto find(transform_id_t &id) -> transform_t * {
    if (!is_valid(id)) {
      return nullptr;
    }

    return &(_transforms[id.index()]);
  }

  std::vector<transform_t> _transforms;
};

} // namespace transform_hierarchy
