#include "world.hpp"
#include "alex/log.hpp"
#include "ps1_style_renderer/glm_transform_hierarchy.hpp"
#include "ps1_style_renderer/include_glm.hpp"
#include "ps1_style_renderer/static_mesh_entity.hpp"
#include "utility/transform_hierarchy.hpp"
#include <glaze/json/read.hpp>
#include <glm/gtc/quaternion.hpp>

#include "slurp_file.hpp"

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
  entity_json["name"] = entity->name();
  entity_json["model-source"] = "fox";

  // if (auto *p = entity->get<static_mesh_entity_t>()) {
  //  entity_json["model-source"] = p->;
  // }

  auto location = _transform_hierarchy->global_location(transform_id);
  if (location.has_value()) {

    glm::vec3 translation;
    glm::quat rotation;
    glm::vec3 scale;
    glm::vec3 skew;
    glm::vec4 perspective;
    glm::decompose(*location, scale, rotation, translation, skew, perspective);

    glm::vec3 rotation_euler = glm::eulerAngles(rotation);

    entity_json["translation"] =
        glz::generic::array_t{translation.x, translation.y, translation.z};
    entity_json["rotation"] = glz::generic::array_t{
        rotation_euler.x, rotation_euler.y, rotation_euler.z};
    entity_json["scale"] = glz::generic::array_t{scale.x, scale.y, scale.z};
  }

  glz::generic::array_t children_json;
  auto children = _transform_hierarchy->children(transform_id);
  for (auto child : children) {
    auto child_json = save_entity(child);
    if (child_json.has_value()) {
      children_json.push_back(*child_json);
    }
  }

  entity_json["children"] = children_json;
  return entity_json;
}

auto world_t::save_world(std::filesystem::path path) -> void {

  glz::generic::array_t entities_json;
  auto entity_roots = _transform_hierarchy->roots();
  for (auto child : entity_roots) {
    auto child_json = save_entity(child);
    if (child_json.has_value()) {
      entities_json.push_back(*child_json);
    }
  }

  glz::generic world;
  world["static-entities"] = entities_json;

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

auto world_t::load_world(std::filesystem::path path) -> void {
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

  auto static_entities = json->root()["static-entities"];
  for (auto static_entity : static_entities) {

    auto name = static_entity["name"].get<std::string>();
    if (!name) {
      ALEX_WARN("Could not load entity 'name'");
      continue;
    }

    auto model_source = static_entity["model-source"].get<std::string>();
    if (!model_source.has_value()) {
      ALEX_WARN("Could not load entity 'model-source'");
      continue;
    }

    std::array<float, 3> translation_data;
    auto error = glz::read_json(translation_data, static_entity["translation"]);
    if (error) {
      ALEX_WARN("Could not load entity 'translation'");
      continue;
    }

    std::vector<float> rotation_data;
    error = glz::read_json(rotation_data, static_entity["rotation"]);
    if (error) {
      ALEX_WARN("Could not load entity 'rotation'");
      continue;
    }

    std::vector<float> scale_data;
    error = glz::read_json(scale_data, static_entity["scale"]);
    if (error) {
      ALEX_WARN("Could not load entity 'scale'");
      continue;
    }

    const auto translation = glm::vec3(translation_data[0], translation_data[1],
                                       translation_data[2]);
    const auto rotation =
        glm::vec3(rotation_data[0], rotation_data[1], rotation_data[2]);
    const auto scale = glm::vec3(scale_data[0], scale_data[1], scale_data[2]);

    glm::mat4 const transform =
        glm::translate(glm::mat4(1.0f), translation) *
        glm::eulerAngleXYZ(rotation[0], rotation[1], rotation[2]) *
        glm::scale(glm::mat4(1.0f), scale);

    auto transform_id = _transform_hierarchy->add(transform);
    static_mesh_entity_t entity(*name, *transform_id);

    auto renderable = _resources->get_renderable(*model_source);
    if (!renderable) {
      ALEX_WARN("Could not find model source for name '{}'", *model_source);
      continue;
    }

    entity.set_renderable(_core, _static_render, *renderable);
    _entities.push_back(std::move(entity));
  }

  ALEX_INFO("Loaded world '{}'", path.string());
}

auto world_t::transform_hierarchy() -> glm_transform_hierarchy & {
  return _transform_hierarchy.value();
}

auto world_t::add_entity(entity_t entity) -> void {
  _entities.push_back(std::move(entity));
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

} // namespace game
