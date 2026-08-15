#pragma once

#include <glaze/json.hpp>

#include "entity.hpp"
#include "player.hpp"
#include "ps1_style_renderer/glm_transform_hierarchy.hpp"
#include "resources.hpp"
#include "static_render.hpp"
#include "utility/transform_hierarchy.hpp"

namespace game {

class world_t {
public:
  world_t(sdl::window_extent_t extent, alex::core_t *core,
          static_render_t *static_render, resources_t *resources);

  auto clean_world() -> void;

  auto load_world(std::filesystem::path path) -> void;

  auto save_world(std::filesystem::path path) -> void;

  auto add_entity(entity_t entity) -> void;

  auto entities() -> std::span<entity_t>;

  auto transform_hierarchy() -> glm_transform_hierarchy &;

  auto camera() -> camera_t &;

  auto player() -> player_t &;

  auto update_input(std::span<SDL_Event> events) -> void;

  auto update_logic(double deltatime) -> void;

private:
  auto find_entity(transform_hierarchy::transform_id_t transform_id)
      -> entity_t *;

  auto save_entity(transform_hierarchy::transform_id_t transform_id)
      -> std::optional<glz::generic>;

  alex::core_t *_core{nullptr};
  static_render_t *_static_render{nullptr};
  resources_t *_resources{nullptr};

  std::vector<entity_t> _entities;
  std::optional<camera_t> _camera;
  std::optional<player_t> _player;
  std::optional<glm_transform_hierarchy> _transform_hierarchy;
};

} // namespace game
