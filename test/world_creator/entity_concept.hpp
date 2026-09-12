#pragma once

#include "glm_transform_hierarchy.hpp"

#include <concepts>
#include <type_traits>

namespace game {
template <typename Entity>
concept entity_concept =
    requires(Entity entity, std::string_view name,
             transform_hierarchy::transform_id_t transform_id) {
      { entity.name() } -> std::convertible_to<std::string_view>;
      { entity.set_name(name) };
      {
        entity.transform_id()
      } -> std::convertible_to<transform_hierarchy::transform_id_t>;
      { entity.set_transform_id(transform_id) };
    };

} // namespace game
