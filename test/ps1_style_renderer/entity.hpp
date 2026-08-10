#pragma once

#include "glm_transform_hierarchy.hpp"

#include "entity_concept.hpp"
#include "static_mesh_entity.hpp"
#include <type_traits>
#include <variant>

namespace game {

class entity_t {
public:
  using underlying_entity_t = std::variant<static_mesh_entity_t>;

  template <typename t_value>
    requires std::is_constructible_v<underlying_entity_t, t_value>
  constexpr entity_t(t_value &&v)
      : _underlying_entity{std::forward<t_value>(v)} {}

  constexpr auto name() -> std::string_view {
    return std::visit([](auto &underlying) { return underlying.name(); },
                      _underlying_entity);
  }

  constexpr auto set_name(std::string_view name) -> void {
    std::visit([name](auto &underlying) { underlying.set_name(name); },
               _underlying_entity);
  }

  constexpr auto transform_id() -> transform_hierarchy::transform_id_t {
    return std::visit(
        [](auto &underlying) { return underlying.transform_id(); },
        _underlying_entity);
  }

  constexpr auto
  set_transform_id(transform_hierarchy::transform_id_t transform_id) -> void {
    std::visit(
        [transform_id](auto &underlying) {
          underlying.set_transform_id(transform_id);
        },
        _underlying_entity);
  }

  template <typename t_underlying>
    requires requires(underlying_entity_t &entity) {
      { std::get<t_underlying>(entity) } -> std::same_as<t_underlying &>;
    }
  constexpr auto get() -> t_underlying * {
    return std::get_if<t_underlying>(&_underlying_entity);
  }

  template <typename t_underlying>
    requires requires(underlying_entity_t &entity) {
      { std::get<t_underlying>(entity) } -> std::same_as<t_underlying &>;
    }
  constexpr auto is() -> bool {
    return get<t_underlying>() != nullptr;
  }

private:
  underlying_entity_t _underlying_entity;
};

} // namespace game
