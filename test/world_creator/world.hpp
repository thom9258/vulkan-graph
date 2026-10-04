#pragma once

#include <glaze/json.hpp>

#include "entity.hpp"
#include "glm_transform_hierarchy.hpp"
#include "player.hpp"
#include "resources.hpp"
#include "serialization.hpp"
#include "ui/imgui_context.hpp"
#include "utility/transform_hierarchy.hpp"
#include "world_creator/ui/imgui_context.hpp"

namespace game {

struct background_color_t {
  float color[3]{1.0f, 0.0f, 0.0f};

  constexpr auto r() -> float & { return color[0]; }
  constexpr auto g() -> float & { return color[1]; }
  constexpr auto b() -> float & { return color[2]; }
  constexpr auto data() -> float * { return color; }
};

class world_t {
public:
  world_t(sdl::window_extent_t extent, alex::core_t *core,
          resources_t *resources, imgui_context_t *imgui_context);

  auto clean_world() -> void;

  auto load_world_v1(std::filesystem::path path) -> void;

  auto save_world_v1(std::filesystem::path path) -> void;

  auto find_entity(transform_hierarchy::transform_id_t transform_id)
      -> entity_t *;

  auto add_entity(entity_t entity) -> entity_t *;

  auto delete_entity_by_transform_id(
      transform_hierarchy::transform_id_t transform_id) -> void;

  auto delete_entity_tree_by_transform_id(
      transform_hierarchy::transform_id_t transform_id) -> void;

  auto entities() -> std::span<entity_t>;

  auto transform_hierarchy() -> glm_transform_hierarchy &;

  auto camera() -> camera_t &;

  auto player() -> player_t &;

  auto update_input(std::span<SDL_Event> events) -> void;

  auto update_logic(double deltatime) -> void;

  auto background_color() -> background_color_t &;
  auto set_background_color(background_color_t background_color) -> void;

private:
  auto save_entity_v1(transform_hierarchy::transform_id_t transform_id)
      -> std::optional<serialization::v1::entity_t>;

  auto load_entity_v1(serialization::v1::entity_t &entity,
                      std::optional<transform_hierarchy::transform_id_t> parent)
      -> void;

  alex::core_t *_core{nullptr};
  resources_t *_resources{nullptr};
  imgui_context_t *_imgui_context{nullptr};

  std::vector<entity_t> _entities;
  std::optional<camera_t> _camera;
  std::optional<player_t> _player;
  std::optional<glm_transform_hierarchy> _transform_hierarchy;

  background_color_t _background_color;
};

} // namespace game
