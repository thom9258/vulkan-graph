#pragma once

#include "../utility/transform_hierarchy.hpp"

#include <glm/glm.hpp>

static constexpr auto transform_child_from_parent(glm::mat4 parent_global,
                                                  glm::mat4 child_local)
    -> glm::mat4 {
  return parent_global * child_local;
}

using glm_transform_hierarchy =
    transform_hierarchy::transform_hierarchy_t<glm::mat4,
                                               transform_child_from_parent>;
