#pragma once

#include "glm_transform_hierarchy.hpp"

#include "entity_concept.hpp"
#include "utility/transform_hierarchy.hpp"
#include <variant>

namespace game {

struct hemisphere_light_t {
  glm::vec3 color{1.0f, 1.0f, 1.0f};
  glm::vec3 sky{0.0f, 1.0f, 0.0f};
  float intensity{0.1f};
};

struct directional_light_t {
  glm::vec3 color{1.0f, 1.0f, 1.0f};
  float intensity{0.1f};
};

struct point_light_t {
  glm::vec3 color{1.0f, 1.0f, 1.0f};
  float range{10.0f};
  float intensity{0.1f};
};

struct spot_light_t {
  glm::vec3 color{1.0f, 1.0f, 1.0f};
  float inner_cutoff{25.0f};
  float outer_cutoff{60.0f};
  float range{10.0f};
  float intensity{0.1f};
};

using light_t = std::variant<hemisphere_light_t, directional_light_t,
                             point_light_t, spot_light_t>;

class light_entity_t {
public:
  template <typename T>
    requires std::is_constructible_v<light_t, T>
  constexpr light_entity_t(std::string_view name, transform_hierarchy::transform_id_t transform_id,
                           T &&v)
      : _name{name}, _transform_id{transform_id}, _light{std::forward<T>(v)} {}

  constexpr auto name() -> std::string_view;
  constexpr auto set_name(std::string_view name) -> void;
  constexpr auto transform_id() -> transform_hierarchy::transform_id_t;
  constexpr auto
  set_transform_id(transform_hierarchy::transform_id_t transform_id) -> void;

  constexpr auto hemisphere() -> hemisphere_light_t *;
  constexpr auto directional() -> directional_light_t *;
  constexpr auto point() -> point_light_t *;
  constexpr auto spot() -> spot_light_t *;

  constexpr auto color() -> glm::vec3&;
  constexpr auto intensity() -> float&;

private:
  template <typename T> constexpr auto access() -> T * {
    return std::get_if<T>(&_light);
  }

  std::string _name;
  transform_hierarchy::transform_id_t _transform_id;
  light_t _light;
};

static_assert(entity_concept<light_entity_t>);

constexpr auto light_entity_t::name() -> std::string_view { return _name; }

constexpr auto light_entity_t::set_name(std::string_view name) -> void {
  _name = name;
}

constexpr auto light_entity_t::transform_id()
    -> transform_hierarchy::transform_id_t {
  return _transform_id;
}

constexpr auto light_entity_t::set_transform_id(
    transform_hierarchy::transform_id_t transform_id) -> void {
  _transform_id = transform_id;
}

constexpr auto light_entity_t::hemisphere() -> hemisphere_light_t * {
  return access<hemisphere_light_t>();
}

constexpr auto light_entity_t::directional() -> directional_light_t * {
  return access<directional_light_t>();
}

constexpr auto light_entity_t::point() -> point_light_t * {
  return access<point_light_t>();
}

constexpr auto light_entity_t::spot() -> spot_light_t * {
  return access<spot_light_t>();
}


constexpr auto light_entity_t::color() -> glm::vec3&
{
    return std::visit([] (auto& l) -> glm::vec3& { return l.color; }, _light);
}

constexpr auto light_entity_t::intensity() -> float&
{
    return std::visit([] (auto& l) -> float& { return l.intensity; }, _light);
}

} // namespace game
