#pragma once

#include "entity.hpp"
#include "player.hpp"
#include "utility/transform_hierarchy.hpp"

namespace game {

class world_t {
public:
  world_t(sdl::window_extent_t extent);

  constexpr auto add_entity(entity_t entity) -> void;

  constexpr auto entities() -> std::span<entity_t>;

  constexpr auto transform_hierarchy() -> glm_transform_hierarchy &;

  constexpr auto camera() -> camera_t &;

  constexpr auto player() -> player_t &;

  constexpr auto update_input(std::span<SDL_Event> events) -> void;

  constexpr auto update_logic(double deltatime) -> void;

private:
  std::vector<entity_t> _entities;
  std::optional<camera_t> _camera;
  std::optional<player_t> _player;
  std::optional<glm_transform_hierarchy> _transform_hierarchy;
};

constexpr auto world_t::transform_hierarchy() -> glm_transform_hierarchy & {
  return _transform_hierarchy.value();
}

constexpr auto world_t::add_entity(entity_t entity) -> void {
  _entities.push_back(std::move(entity));
}

constexpr auto world_t::entities() -> std::span<entity_t> { return _entities; }

constexpr auto world_t::camera() -> camera_t & { return _camera.value(); }

constexpr auto world_t::player() -> player_t & { return _player.value(); }

constexpr auto world_t::update_input(std::span<SDL_Event> events) -> void {
  _player->update_input(events);
}

constexpr auto world_t::update_logic(double deltatime) -> void {
  _player->update_logic(deltatime);
}

world_t::world_t(sdl::window_extent_t extent) {
  const float aspect = extent.aspect();
  const float near_plane = 0.1f, far_plane = 200.0f;
  glm::mat4 const projection = std::invoke([&]() {
    glm::mat4 p =
        glm::perspective(glm::radians(70.f), aspect, near_plane, far_plane);
    p[1][1] *= -1.0f;
    return p;
  });

  glm::vec3 const position(10.0f, 5.0f, 0.0f);
  glm::vec3 const target(0.0f, 0.0f, 0.0f);
  glm::vec3 const up(0.0f, 1.0f, 0.0f);

  _camera.emplace(projection, position, target, up);
  _player.emplace(&_camera.value());
  _transform_hierarchy = glm_transform_hierarchy(256);
}

} // namespace game
