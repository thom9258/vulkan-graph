#include "glm_transform_hierarchy.hpp"

auto child_global_from_local(glm::mat4 parent_global, glm::mat4 child_local)
    -> glm::mat4 {
  return parent_global * child_local;
}

auto child_local_from_global(glm::mat4 parent_global, glm::mat4 child_local)
    -> glm::mat4 {
  return glm::inverse(parent_global) * child_local;
}
