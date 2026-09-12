#pragma once

#include <glaze/json.hpp>

#include "ui/imgui_context.hpp"
#include "entity.hpp"
#include "glm_transform_hierarchy.hpp"
#include "player.hpp"
#include "resources.hpp"
#include "static_render.hpp"
#include "utility/transform_hierarchy.hpp"
#include "world_creator/ui/imgui_context.hpp"

namespace game {

namespace serialization {

namespace v1 {

using vec3_t = std::array<float, 3>;

struct transform_t {
  vec3_t translation{0.0f, 0.0f, 0.0f};
  vec3_t rotation{0.0f, 0.0f, 0.0f};
  vec3_t scale{1.0f, 1.0f, 1.0f};
};

using tags_t = std::vector<std::pair<std::string, std::string>>;

struct entity_t {
  std::string name{"<unnamed>"};
  transform_t transform{};
  std::optional<std::string> model_source;
  bool has_mesh_collider{false};
  tags_t tags;
  std::vector<entity_t> children;
};

struct color_t {
  float r;
  float g;
  float b;
};

struct camera_t {
  vec3_t translation{0.0f, 0.0f, 0.0f};
  vec3_t rotation{0.0f, 0.0f, 0.0f};
};

struct settings_t {
  color_t background_color{1.0f, 0.0f, 0.0f};
};

struct world_t {
  settings_t settings;
  camera_t camera;
  std::vector<entity_t> entities;
};

} // namespace v1

static constexpr const std::string_view file_type{".alexworld"};

namespace settings {
static constexpr const std::string_view name{"settings"};
static constexpr const std::string_view background_color{"background-color"};
} // namespace settings

namespace entity {
static constexpr const std::string_view hierarchy{"hierarchy"};
static constexpr const std::string_view type{"type"};
static constexpr const std::string_view name{"name"};
static constexpr const std::string_view translation{"translation"};
static constexpr const std::string_view rotation{"rotation"};
static constexpr const std::string_view scale{"scale"};
static constexpr const std::string_view children{"children"};
} // namespace entity

namespace static_mesh_entity {
static constexpr const std::string_view type_name{"static-mesh"};
static constexpr const std::string_view model_source{"model-source"};
} // namespace static_mesh_entity

} // namespace serialization

struct background_color_t {
  float color[3]{1.0f, 0.0f, 0.0f};

  constexpr auto r() const -> float { return color[0]; }
  constexpr auto g() const -> float { return color[1]; }
  constexpr auto b() const -> float { return color[2]; }
  constexpr auto data() -> float * { return color; }
};

class world_t {
public:
  world_t(sdl::window_extent_t extent, alex::core_t *core,
          static_render_t *static_render, resources_t *resources,
          imgui_context_t *imgui_context);

  auto clean_world() -> void;

  auto load_world_v1(std::filesystem::path path) -> void;

  auto save_world_v1(std::filesystem::path path) -> void;

  auto add_entity(entity_t entity) -> entity_t *;

  auto entities() -> std::span<entity_t>;

  auto transform_hierarchy() -> glm_transform_hierarchy &;

  auto camera() -> camera_t &;

  auto player() -> player_t &;

  auto update_input(std::span<SDL_Event> events) -> void;

  auto update_logic(double deltatime) -> void;

  auto background_color() -> background_color_t &;
  auto set_background_color(background_color_t background_color) -> void;

private:
  auto find_entity(transform_hierarchy::transform_id_t transform_id)
      -> entity_t *;

  auto save_entity_v1(transform_hierarchy::transform_id_t transform_id)
      -> std::optional<serialization::v1::entity_t>;

  auto load_entity_v1(serialization::v1::entity_t &entity,
                      std::optional<transform_hierarchy::transform_id_t> parent)
      -> void;

  alex::core_t *_core{nullptr};
  static_render_t *_static_render{nullptr};
  resources_t *_resources{nullptr};
  imgui_context_t *_imgui_context{nullptr};

  std::vector<entity_t> _entities;
  std::optional<camera_t> _camera;
  std::optional<player_t> _player;
  std::optional<glm_transform_hierarchy> _transform_hierarchy;

  background_color_t _background_color;
};

} // namespace game
