#include "world.hpp"
#include "alex/log.hpp"
#include "ps1_style_renderer/glm_transform_hierarchy.hpp"
#include "ps1_style_renderer/include_glm.hpp"
#include "ps1_style_renderer/static_mesh_entity.hpp"
#include "utility/transform_hierarchy.hpp"

#include "json.hpp"

#include <glm/gtc/quaternion.hpp>

#include "slurp_file.hpp"

#include <iostream>

namespace game {

world_t::world_t(sdl::window_extent_t extent, alex::core_t *core,
                 static_render_t *static_render, resources_t *resources)
    : _core{core}, _static_render{static_render}, _resources{resources} {
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

auto world_t::save_entity(transform_hierarchy::transform_id_t transform_id)
    -> std::optional<glz::generic> {

  entity_t *entity = find_entity(transform_id);
  if (!entity) {
    return std::nullopt;
  }

  glz::generic entity_json;
  entity_json[serialization::entity::name] = entity->name();

  auto location = _transform_hierarchy->local_location(transform_id);
  if (location.has_value()) {

    glm::vec3 translation(0.0f);
    glm::quat rotation;
    glm::vec3 scale(0.0f);
    glm::vec3 skew(0.0f);
    glm::vec4 perspective(0.0f);
    glm::decompose(*location, scale, rotation, translation, skew, perspective);

    glm::vec3 rotation_euler = glm::eulerAngles(rotation);

    entity_json[serialization::entity::translation] =
        glz::generic::array_t{translation.x, translation.y, translation.z};
    entity_json[serialization::entity::rotation] = glz::generic::array_t{
        rotation_euler.x, rotation_euler.y, rotation_euler.z};
    entity_json[serialization::entity::scale] =
        glz::generic::array_t{scale.x, scale.y, scale.z};
  }

  if (auto *static_mesh = entity->get<static_mesh_entity_t>()) {
    entity_json[serialization::entity::type] =
        serialization::static_mesh_entity::type_name;

    entity_json[serialization::static_mesh_entity::model_source] =
        static_mesh->renderable_name();
  }

  glz::generic::array_t children_json;
  auto children = _transform_hierarchy->children(transform_id);
  for (auto child : children) {
    auto child_json = save_entity(child);
    if (child_json.has_value()) {
      children_json.push_back(*child_json);
    }
  }

  entity_json[serialization::entity::children] = children_json;
  return entity_json;
}

auto world_t::save_world(std::filesystem::path path) -> void {
  path += serialization::file_type;

  glz::generic settings_json;
  settings_json[serialization::settings::background_color] =
      glz::generic::array_t{_background_color.r(), _background_color.g(),
                            _background_color.b()};

  glz::generic::array_t entities_json;
  auto entity_roots = _transform_hierarchy->roots();
  for (auto root : entity_roots) {
    auto child_json = save_entity(root);
    if (child_json.has_value()) {
      entities_json.push_back(*child_json);
    }
  }

  glz::generic world;
  world[serialization::settings::name] = settings_json;
  world[serialization::entity::hierarchy] = entities_json;

  std::string output;
  glz::error_ctx error = glz::write_json(world, output);
  if (error) {
    ALEX_ERROR("Could not write world to json buffer '{}'",
               error.custom_error_message);
  }

  std::string pretty_output = glz::prettify_json(output);
  std::ofstream fs(path, std::ios::out);
  fs << pretty_output;

  ALEX_INFO("Saved world '{}'", path.string());
}

auto world_t::clean_world() -> void { _entities.clear(); }

auto world_t::load_entity(
    glz::lazy_json_view<glz::opts{}> json,
    std::optional<transform_hierarchy::transform_id_t> parent) -> void {

  auto name = json[serialization::entity::name].get<std::string>();
  if (!name) {
    ALEX_WARN("Could not load entity '{}'", serialization::entity::name);
    return;
  }

  auto translation = serialization::utility::read_vec3(
      json, serialization::entity::translation);
  if (!translation.has_value()) {
    ALEX_WARN("Could not load entity '{}'", serialization::entity::translation);
    return;
  }

  auto rotation =
      serialization::utility::read_vec3(json, serialization::entity::rotation);
  if (!rotation.has_value()) {
    ALEX_WARN("Could not load entity '{}'", serialization::entity::rotation);
    return;
  }

  auto scale =
      serialization::utility::read_vec3(json, serialization::entity::scale);
  if (!scale.has_value()) {
    ALEX_WARN("Could not load entity '{}'", serialization::entity::scale);
    return;
  }

  glm::vec3 const euler_angle = rotation.value_or(glm::vec3(0.0f));

  glm::mat4 const transform =
      glm::translate(glm::mat4(1.0f), translation.value_or(glm::vec3(0.0f))) *
      glm::eulerAngleXYZ(euler_angle[0], euler_angle[1], euler_angle[2]) *
      glm::scale(glm::mat4(1.0f), scale.value_or(glm::vec3(1.0f)));

  std::optional<transform_hierarchy::transform_id_t> transform_id;
  if (parent.has_value()) {
    transform_id = _transform_hierarchy->add_child_local_location(
        transform, parent.value());
  } else {
    transform_id = _transform_hierarchy->add(transform);
  }

  auto type = json[serialization::entity::type].get<std::string>();
  if (!type.has_value()) {
    ALEX_WARN("Could not load entity '{}'", serialization::entity::type);
    return;
  }

  if (*type == serialization::static_mesh_entity::type_name) {
    auto model_source = json[serialization::static_mesh_entity::model_source]
                            .get<std::string>();
    if (!model_source.has_value()) {
      ALEX_WARN("Could not load entity 'model-source'");
      return;
    }

    static_mesh_entity_t entity(*name, *transform_id);
    if (*model_source != "") {
      auto renderable = _resources->get_renderable(*model_source);
      if (!renderable) {
        ALEX_WARN("Could not find model source for name '{}'", *model_source);
        return;
      }

      entity.set_renderable(*model_source, _core, _static_render, *renderable);
    }

    std::println("Loaded entity {}", *name);
    add_entity(std::move(entity));
  }

  auto children_json = json[serialization::entity::children];
  for (auto child : children_json) {
    load_entity(child, transform_id);
  }
}

auto world_t::load_world(std::filesystem::path path) -> void {
  path += serialization::file_type;
  clean_world();
  auto world_source = slurp_file(path);
  if (!world_source.has_value()) {
    ALEX_ERROR("Could not load world '{}'", path.string());
    return;
  }

  auto json = glz::lazy_json(*world_source);
  if (!json.has_value()) {
    ALEX_ERROR("Could not parse world '{}'", path.string());
    return;
  }

  auto settings = json->root()[serialization::settings::name];

  std::vector<float> background_color_data;
  auto error =
      glz::read_json(background_color_data,
                     settings[serialization::settings::background_color]);
  if (error) {
    ALEX_WARN("Could not load settings '{}'",
              serialization::settings::background_color);
    return;
  }

  _background_color.color[0] = background_color_data[0];
  _background_color.color[1] = background_color_data[1];
  _background_color.color[2] = background_color_data[2];

  auto roots = json->root()[serialization::entity::hierarchy];
  for (glz::lazy_json_view root : roots) {
    load_entity(root, std::nullopt);
  }

  ALEX_INFO("Loaded world '{}'", path.string());
}

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
  _player->update_logic(deltatime);
}

auto world_t::background_color() -> background_color_t & {
  return _background_color;
}

auto world_t::set_background_color(background_color_t background_color)
    -> void {
  _background_color = background_color;
}

} // namespace game
