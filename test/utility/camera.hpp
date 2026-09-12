#pragma once

#include <glm/ext/quaternion_geometric.hpp>
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_RADIANS
#include <glm/ext.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class camera_t {
public:
  constexpr camera_t(glm::mat4 projection, glm::vec3 pos, glm::vec3 target,
                     glm::vec3 up)
      : _position{pos}, _forward{glm::normalize(target - pos)}, _up{up},
        _projection{projection} {}

  constexpr auto projection() -> glm::mat4 { return _projection; }

  constexpr auto set_projection(glm::mat4 projection) -> void {
    _projection = projection;
  }

  constexpr auto set_position(glm::vec3 position) -> void {
    _position = position;
  }

  constexpr auto position() -> glm::vec3 { return _position; }

  constexpr auto translate_global(glm::vec3 delta) -> void { _position += delta; }

  constexpr auto view() -> glm::mat4 {
    glm::vec3 const target = _position + _forward;
    return glm::lookAt(_position, target, _up);
  }

private:
  glm::vec3 _position;
  glm::vec3 _forward;
  glm::vec3 _up;
  glm::mat4 _projection;
};
