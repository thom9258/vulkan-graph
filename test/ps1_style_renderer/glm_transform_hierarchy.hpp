#pragma once

#include "../utility/transform_hierarchy.hpp"

#include "include_glm.hpp"

static auto inverse_location_unity_style(glm::mat4 location) -> glm::mat4 {
  // 1. Extract the translation column safely
  glm::vec3 translation = glm::vec3(location[3]);

  // 2. Extract and normalize the rotation/basis vectors (removes the scale
  // factor)
  glm::vec3 forward = glm::normalize(glm::vec3(location[0]));
  glm::vec3 up = glm::normalize(glm::vec3(location[1]));
  glm::vec3 right = glm::normalize(glm::vec3(location[2]));

  // 3. Rebuild a pure rotation matrix (transposing a pure rotation matrix
  // inverts it)
  glm::mat4 inv_rotation = glm::mat4(1.0f);
  inv_rotation[0] = glm::vec4(forward.x, up.x, right.x, 0.0f);
  inv_rotation[1] = glm::vec4(forward.y, up.y, right.y, 0.0f);
  inv_rotation[2] = glm::vec4(forward.z, up.z, right.z, 0.0f);

  // 4. Invert the translation by multiplying it against the inverted rotation
  glm::vec3 inv_translation =
      glm::vec3(inv_rotation * glm::vec4(-translation, 1.0f));
  inv_rotation[3] = glm::vec4(inv_translation, 1.0f);

  // This matrix will now accurately drop global coordinates into local space
  // while completely ignoring scale pollution!
  return inv_rotation;
}

static auto transform_child_from_parent_unity_style(glm::mat4 parent_global,
                                                    glm::mat4 child_local)
    -> glm::mat4 {
  // 1. Extract pure unscaled orientation/translation from parent
  glm::vec3 p_translation = glm::vec3(parent_global[3]);
  glm::vec3 p_forward = glm::normalize(glm::vec3(parent_global[0]));
  glm::vec3 p_up = glm::normalize(glm::vec3(parent_global[1]));
  glm::vec3 p_right = glm::normalize(glm::vec3(parent_global[2]));

  // Extract parent scale factors
  glm::vec3 p_scale = glm::vec3(glm::length(glm::vec3(parent_global[0])),
                                glm::length(glm::vec3(parent_global[1])),
                                glm::length(glm::vec3(parent_global[2])));

  glm::mat4 p_unscaled = glm::mat4(1.0f);
  p_unscaled[0] = glm::vec4(p_forward, 0.0f);
  p_unscaled[1] = glm::vec4(p_up, 0.0f);
  p_unscaled[2] = glm::vec4(p_right, 0.0f);
  p_unscaled[3] = glm::vec4(p_translation, 1.0f);

  // 2. Extract child local attributes
  glm::vec3 c_translation = glm::vec3(child_local[3]);
  glm::vec3 c_scale = glm::vec3(glm::length(glm::vec3(child_local[0])),
                                glm::length(glm::vec3(child_local[1])),
                                glm::length(glm::vec3(child_local[2])));

  // 3. Unity evaluation: Parent rotation rotates child position,
  // but parent scale does NOT scale child local positions uniformly in standard
  // PRS. Instead, it applies the local translation directly into the unscaled
  // space.
  glm::mat4 global_without_scale =
      p_unscaled * glm::translate(glm::mat4(1.0f), c_translation) *
      glm::mat4_cast(glm::quat_cast(child_local));

  // 4. Re-apply the combined scales at the very end of the column base
  glm::vec3 final_scale = p_scale * c_scale;
  global_without_scale[0] *= final_scale.x;
  global_without_scale[1] *= final_scale.y;
  global_without_scale[2] *= final_scale.z;

  //TODO: this is missing scale inserted instead of the above multiplication i think
  return global_without_scale;
}

static auto transform_child_from_parent(glm::mat4 parent_global,
                                        glm::mat4 child_local) -> glm::mat4 {
  return parent_global * child_local;
}

static auto inverse_location(glm::mat4 location) -> glm::mat4 {
  return glm::inverse(location);
}

using glm_transform_hierarchy = transform_hierarchy::transform_hierarchy_t<
    glm::mat4, transform_child_from_parent,
    inverse_location>;
