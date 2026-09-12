#include "world.hpp"
#include "alex/log.hpp"
#include "glm_transform_hierarchy.hpp"
#include "include_glm.hpp"
#include "static_mesh_entity.hpp"
#include "utility/transform_hierarchy.hpp"

#include "json.hpp"

#include <glm/gtc/quaternion.hpp>

#include "slurp_file.hpp"
#include "world_creator/ui/imgui_context.hpp"

#include <iostream>

namespace game {

world_t::world_t(sdl::window_extent_t extent, alex::core_t *core,
                 static_render_t *static_render, resources_t *resources,
                 imgui_context_t *imgui_context)
    : _core{core}, _static_render{static_render}, _resources{resources},
      _imgui_context{imgui_context} {
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

auto world_t::find_entity(transform_hierarchy::transform_id_t transform_id)
    -> entity_t * {
  for (entity_t &entity : _entities) {
    if (entity.transform_id() == transform_id) {
      return &entity;
    }
  }

  return nullptr;
}

auto world_t::clean_world() -> void { _entities.clear(); }

auto world_t::transform_hierarchy() -> glm_transform_hierarchy & {
  return _transform_hierarchy.value();
}

auto world_t::add_entity(entity_t entity) -> entity_t * {
  _entities.push_back(std::move(entity));
  return &_entities.back();
}

auto world_t::entities() -> std::span<entity_t> { return _entities; }

auto world_t::camera() -> camera_t & { return _camera.value(); }

auto world_t::player() -> player_t & { return _player.value(); }

auto world_t::update_input(std::span<SDL_Event> events) -> void {
  _player->update_input(events);
}

auto world_t::update_logic(double deltatime) -> void {
  if (!_imgui_context->is_imgui_hovered()) {
    _player->update_movement(deltatime);
  }
}

auto world_t::background_color() -> background_color_t & {
  return _background_color;
}

auto world_t::set_background_color(background_color_t background_color)
    -> void {
  _background_color = background_color;
}

auto world_t::load_entity_v1(
    serialization::v1::entity_t &entity,
    std::optional<transform_hierarchy::transform_id_t> parent) -> void {

  const auto translation = glm::vec3{entity.transform.translation[0],
                                     entity.transform.translation[1],
                                     entity.transform.translation[2]};

  const auto rotation =
      glm::vec3{entity.transform.rotation[0], entity.transform.rotation[1],
                entity.transform.rotation[2]};

  const auto scale =
      glm::vec3{entity.transform.scale[2], entity.transform.scale[1],
                entity.transform.scale[2]};

  glm::mat4 const transform =
      glm::translate(glm::mat4(1.0f), translation) *
      glm::eulerAngleXYZ(rotation[0], rotation[1], rotation[2]) *
      glm::scale(glm::mat4(1.0f), scale);

  std::optional<transform_hierarchy::transform_id_t> transform_id;
  if (parent.has_value()) {
    transform_id = _transform_hierarchy->add_child_local_location(
        transform, parent.value());
  } else {
    transform_id = _transform_hierarchy->add(transform);
  }

  static_mesh_entity_t deserialized(entity.name, *transform_id);
  if (entity.model_source.has_value()) {
    if (entity.model_source.value() != "") {
      auto renderable = _resources->get_renderable(entity.model_source.value());
      if (!renderable) {
        ALEX_WARN("Could not find model source for name '{}'",
                  entity.model_source.value());
        return;
      }

      deserialized.set_renderable(entity.model_source.value(), _core,
                                  _static_render, *renderable);
    }
  }

  add_entity(std::move(deserialized));

  for (serialization::v1::entity_t &child : entity.children) {
    load_entity_v1(child, transform_id);
  }
}

auto world_t::load_world_v1(std::filesystem::path path) -> void {
  path += ".alexworld.json";
  clean_world();

  auto json = slurp_file(path);
  if (!json.has_value()) {
    ALEX_ERROR("Could not load world '{}'", path.string());
    return;
  }

  serialization::v1::world_t world;
  auto error = glz::read_json(world, *json);
  if (error) {
    ALEX_ERROR("Could not load world json '{}'", path.string());
    return;
  }

  _background_color.color[0] = world.settings.background_color.r;
  _background_color.color[1] = world.settings.background_color.g;
  _background_color.color[2] = world.settings.background_color.b;

  _camera->set_position(glm::vec3{world.camera.translation[0],
                                  world.camera.translation[1],
                                  world.camera.translation[2]});

  for (serialization::v1::entity_t &root : world.entities) {
    load_entity_v1(root, std::nullopt);
  }

  ALEX_INFO("Loaded world '{}'", path.string());
}

auto world_t::save_entity_v1(transform_hierarchy::transform_id_t transform_id)
    -> std::optional<serialization::v1::entity_t> {

  entity_t *entity = find_entity(transform_id);
  if (!entity) {
    return std::nullopt;
  }

  serialization::v1::entity_t serialized;
  serialized.name = entity->name();

  auto location = _transform_hierarchy->local_location(transform_id);
  if (location.has_value()) {

    glm::vec3 translation(0.0f);
    glm::quat rotation;
    glm::vec3 scale(0.0f);
    glm::vec3 skew(0.0f);
    glm::vec4 perspective(0.0f);
    glm::decompose(*location, scale, rotation, translation, skew, perspective);

    glm::vec3 rotation_euler = glm::eulerAngles(rotation);

    serialized.transform.translation =
        serialization::v1::vec3_t{translation.x, translation.y, translation.z};
    serialized.transform.rotation = serialization::v1::vec3_t{
        rotation_euler.x, rotation_euler.y, rotation_euler.z};
    serialized.transform.scale =
        serialization::v1::vec3_t{scale.x, scale.y, scale.z};
  }

  if (auto *static_mesh = entity->get<static_mesh_entity_t>()) {
    serialized.model_source = static_mesh->renderable_name();
  }

  auto children = _transform_hierarchy->children(transform_id);
  for (auto child : children) {
    auto serialized_child = save_entity_v1(child);
    if (serialized_child.has_value()) {
      serialized.children.push_back(*serialized_child);
    }
  }

  return serialized;
}

auto world_t::save_world_v1(std::filesystem::path path) -> void {
  path += ".alexworld.json";

  serialization::v1::world_t world;
  world.settings.background_color = serialization::v1::color_t{
      _background_color.color[0], _background_color.color[1],
      _background_color.color[2]};

  world.camera.translation = serialization::v1::vec3_t{
      _camera->position().x, _camera->position().y, _camera->position().z};

  auto entity_roots = _transform_hierarchy->roots();
  for (auto root : entity_roots) {
    auto serialized_root = save_entity_v1(root);
    if (serialized_root.has_value()) {
      world.entities.push_back(*serialized_root);
    }
  }

  auto json = glz::write_json(world);
  if (!json.has_value()) {
    ALEX_ERROR("Could not save world '{}'", path.string());
    return;
  }

  std::string const pretty_json = glz::prettify_json(*json);
  std::ofstream fs(path, std::ios::out);
  fs << pretty_json;
  ALEX_INFO("Saved world '{}'", path.string());
}

} // namespace game
