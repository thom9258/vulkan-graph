#pragma once

#include "../utility/transform_hierarchy.hpp"

#include "include_glm.hpp"

#if 1
auto child_global_from_local(glm::mat4 parent_global, glm::mat4 child_local)
    -> glm::mat4;

auto child_local_from_global(glm::mat4 parent_global, glm::mat4 child_local)
    -> glm::mat4;

using glm_transform_hierarchy = transform_hierarchy::transform_hierarchy_t<
    glm::mat4, child_global_from_local, child_local_from_global>;

#else

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

  // TODO: this is missing scale inserted instead of the above multiplication i
  // think
  return global_without_scale;
}

static auto transform_child_from_parent_unity_style_2(glm::mat4 parent_global,
                                                      glm::mat4 child_local)
    -> glm::mat4 {
  // 1. Extract parent translation
  glm::vec3 p_translation = glm::vec3(parent_global[3]);

  // 2. Extract parent scale by measuring the length of each basis column vector
  glm::vec3 p_scale = glm::vec3(glm::length(glm::vec3(parent_global[0])),
                                glm::length(glm::vec3(parent_global[1])),
                                glm::length(glm::vec3(parent_global[2])));

  // 3. Rebuild an unscaled pure rotation/orientation matrix for the parent
  glm::mat4 p_rotation = glm::mat4(1.0f);
  p_rotation[0] = parent_global[0] / p_scale.x;
  p_rotation[1] = parent_global[1] / p_scale.y;
  p_rotation[2] = parent_global[2] / p_scale.z;

  // 4. Extract child local translation and local scale
  glm::vec3 c_translation = glm::vec3(child_local[3]);
  glm::vec3 c_scale = glm::vec3(glm::length(glm::vec3(child_local[0])),
                                glm::length(glm::vec3(child_local[1])),
                                glm::length(glm::vec3(child_local[2])));

  // 5. Extract child local rotation matrix
  glm::mat4 c_rotation = glm::mat4(1.0f);
  c_rotation[0] = child_local[0] / c_scale.x;
  c_rotation[1] = child_local[1] / c_scale.y;
  c_rotation[2] = child_local[2] / c_scale.z;

  // 6. Unity-Style Assembly:
  // Start with the parent's world position
  glm::mat4 global_matrix = glm::translate(glm::mat4(1.0f), p_translation);
  // Rotate the parent space
  global_matrix = global_matrix * p_rotation;
  // Translate by the child's local offset (Unity children rotate with parent
  // but don't shear from scale)
  global_matrix = glm::translate(global_matrix, c_translation);
  // Apply child local rotation
  global_matrix = global_matrix * c_rotation;
  // Apply the final combined scale cleanly to the basis axes using GLM's
  // factory function
  global_matrix = glm::scale(global_matrix, p_scale * c_scale);

  return global_matrix;
}

static auto inverse_location_unity_style_2(glm::mat4 location) -> glm::mat4 {
  // Extract translation column
  glm::vec3 translation = glm::vec3(location[3]);

  // Extract axes and strip scale factors via normalization
  glm::vec3 right = glm::normalize(glm::vec3(location[0]));
  glm::vec3 up = glm::normalize(glm::vec3(location[1]));
  glm::vec3 forward = glm::normalize(glm::vec3(location[2]));

  // Build standard orthogonal inverse rotation matrix (transposing the
  // orientation)
  glm::mat4 inv_rotation = glm::mat4(1.0f);
  inv_rotation[0] = glm::vec4(right.x, up.x, forward.x, 0.0f);
  inv_rotation[1] = glm::vec4(right.y, up.y, forward.y, 0.0f);
  inv_rotation[2] = glm::vec4(right.z, up.z, forward.z, 0.0f);

  // Calculate inverse local translation offset vector
  glm::vec3 inv_translation =
      glm::vec3(inv_rotation * glm::vec4(-translation, 1.0f));
  inv_rotation[3] = glm::vec4(inv_translation, 1.0f);

  return inv_rotation;
}

static auto transform_child_from_parent_unity_style_3(glm::mat4 parent_global,
                                                      glm::mat4 child_local)
    -> glm::mat4 {
  // 1. Clean parent scale extraction
  float p_scaleX = glm::length(glm::vec3(parent_global[0]));
  float p_scaleY = glm::length(glm::vec3(parent_global[1]));
  float p_scaleZ = glm::length(glm::vec3(parent_global[2]));

  // 2. Clean child scale extraction
  float c_scaleX = glm::length(glm::vec3(child_local[0]));
  float c_scaleY = glm::length(glm::vec3(child_local[1]));
  float c_scaleZ = glm::length(glm::vec3(child_local[2]));

  // 3. Strip scale from parent to get pure world orientation/translation
  glm::mat4 p_unscaled = parent_global;
  if (p_scaleX > 0.0f)
    p_unscaled[0] /= p_scaleX;
  if (p_scaleY > 0.0f)
    p_unscaled[1] /= p_scaleY;
  if (p_scaleZ > 0.0f)
    p_unscaled[2] /= p_scaleZ;

  // 4. Strip scale from child to get pure local orientation/translation
  glm::mat4 c_unscaled = child_local;
  if (c_scaleX > 0.0f)
    c_unscaled[0] /= c_scaleX;
  if (c_scaleY > 0.0f)
    c_unscaled[1] /= c_scaleY;
  if (c_scaleZ > 0.0f)
    c_unscaled[2] /= c_scaleZ;

  // 5. Combine the unscaled spaces (Parent * Child)
  glm::mat4 combined_unscaled = p_unscaled * c_unscaled;

  // 6. Inject the final combined scales cleanly back into the matrix data
  glm::vec3 final_scale =
      glm::vec3(p_scaleX * c_scaleX, p_scaleY * c_scaleY, p_scaleZ * c_scaleZ);

  // Using the official GLM function ensures the scale parameters are properly
  // set
  return glm::scale(combined_unscaled, final_scale);
}

static auto inverse_location_unity_style_3(glm::mat4 location) -> glm::mat4 {
  // 1. Extract the scale components cleanly
  float scaleX = glm::length(glm::vec3(location[0]));
  float scaleY = glm::length(glm::vec3(location[1]));
  float scaleZ = glm::length(glm::vec3(location[2]));

  // Prevent division by zero if parent happens to have 0 scale
  if (scaleX == 0.0f)
    scaleX = 1.0f;
  if (scaleY == 0.0f)
    scaleY = 1.0f;
  if (scaleZ == 0.0f)
    scaleZ = 1.0f;

  // 2. Create an unscaled version of the matrix by dividing out the scale
  glm::mat4 unscaled_matrix = location;
  unscaled_matrix[0] /= scaleX;
  unscaled_matrix[1] /= scaleY;
  unscaled_matrix[2] /= scaleZ;

  // 3. Perform a standard GLM inverse on the unscaled matrix
  // This perfectly extracts the 5.0 local translation without scale distortion!
  return glm::inverse(unscaled_matrix);
}

using glm_transform_hierarchy = transform_hierarchy::transform_hierarchy_t<
    glm::mat4, transform_child_from_parent_unity_style_3,
    inverse_location_unity_style_3>;
#endif
