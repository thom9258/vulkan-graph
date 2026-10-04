#include "world.hpp"
#include "alex/log.hpp"
#include "glm_transform_hierarchy.hpp"
#include "include_glm.hpp"
#include "static_mesh_entity.hpp"
#include "utility/transform_hierarchy.hpp"

#include "json.hpp"

#include <glm/gtc/quaternion.hpp>

#include "slurp_file.hpp"
#include "world_creator/light_entity.hpp"
#include "world_creator/serialization.hpp"
#include "world_creator/ui/imgui_context.hpp"

#include <iostream>

namespace game {

namespace util {
constexpr auto serialize(glm::vec3 v) -> serialization::v1::vec3_t {
  return {v[0], v[1], v[2]};
}

constexpr auto deserialize(serialization::v1::vec3_t v) -> glm::vec3 {
  return {v[0], v[1], v[2]};
}

constexpr auto serialize(hemisphere_light_t &light)
    -> serialization::v1::light_hemisphere_t {
  serialization::v1::light_hemisphere_t out;
  out.color = serialize(light.color);
  out.sky = serialize(light.sky);
  out.intensity = light.intensity;
  return out;
}

constexpr auto serialize(directional_light_t &light)
    -> serialization::v1::light_directional_t {
  serialization::v1::light_directional_t out;
  out.color = serialize(light.color);
  out.intensity = light.intensity;
  return out;
}

constexpr auto serialize(spot_light_t &light)
    -> serialization::v1::light_spot_t {
  serialization::v1::light_spot_t out;
  out.color = serialize(light.color);
  out.inner_cutoff = light.inner_cutoff;
  out.outer_cutoff = light.outer_cutoff;
  out.intensity = light.intensity;
  return out;
}

constexpr auto serialize(point_light_t &light)
    -> serialization::v1::light_point_t {
  serialization::v1::light_point_t out;
  out.color = serialize(light.color);
  out.range = light.range;
  out.intensity = light.intensity;
  return out;
}

constexpr auto deserialize(serialization::v1::light_hemisphere_t &light)
    -> hemisphere_light_t {
  hemisphere_light_t out;
  out.color = deserialize(light.color);
  out.sky = deserialize(light.sky);
  out.intensity = light.intensity;
  return out;
}

constexpr auto deserialize(serialization::v1::light_directional_t &light)
    -> directional_light_t {
  directional_light_t out;
  out.color = deserialize(light.color);
  out.intensity = light.intensity;
  return out;
}

constexpr auto deserialize(serialization::v1::light_spot_t &light)
    -> spot_light_t {
  spot_light_t out;
  out.color = deserialize(light.color);
  out.inner_cutoff = light.inner_cutoff;
  out.outer_cutoff = light.outer_cutoff;
  out.intensity = light.intensity;
  return out;
}

constexpr auto deserialize(const serialization::v1::light_point_t &light)
    -> point_light_t {
  point_light_t out;
  out.color = deserialize(light.color);
  out.range = light.range;
  out.intensity = light.intensity;
  return out;
}

constexpr auto serialize(static_mesh_entity_t &static_mesh)
    -> serialization::v1::static_mesh_t {
  serialization::v1::static_mesh_t out;
  out.model_source = static_mesh.renderable_name();
  out.has_mesh_collider = static_mesh.has_mesh_collider();
  return out;
}

constexpr auto serialize(glm::mat4 mat) -> serialization::v1::transform_t {
  serialization::v1::transform_t out;
  glm::vec3 translation(0.0f);
  glm::quat rotation;
  glm::vec3 scale(0.0f);
  glm::vec3 skew(0.0f);
  glm::vec4 perspective(0.0f);
  glm::decompose(mat, scale, rotation, translation, skew, perspective);
  glm::vec3 rotation_euler = glm::eulerAngles(rotation);
  out.translation = util::serialize(translation);
  out.rotation = util::serialize(rotation_euler);
  out.scale = util::serialize(scale);
  return out;
}

constexpr auto deserialize(serialization::v1::transform_t &transform)
    -> glm::mat4 {
  const auto translation = deserialize(transform.translation);
  const auto rotation = deserialize(transform.rotation);
  const auto scale = deserialize(transform.scale);
  return glm::translate(glm::mat4(1.0f), translation) *
         glm::eulerAngleXYZ(rotation[0], rotation[1], rotation[2]) *
         glm::scale(glm::mat4(1.0f), scale);
}

} // namespace util

world_t::world_t(sdl::window_extent_t extent, alex::core_t *core,
                 resources_t *resources, imgui_context_t *imgui_context)
    : _core{core}, _resources{resources},
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

auto world_t::delete_entity_by_transform_id(
    transform_hierarchy::transform_id_t transform_id) -> void {
  entity_t *found = find_entity(transform_id);
  if (found) {
    std::ranges::swap(*found, _entities.back());
    _entities.pop_back();
  }

  _transform_hierarchy->remove_and_preserve_children(transform_id);
}

auto world_t::delete_entity_tree_by_transform_id(
    transform_hierarchy::transform_id_t transform_id) -> void {

  auto children = _transform_hierarchy->children(transform_id);
  for (transform_hierarchy::transform_id_t child : children) {
    delete_entity_tree_by_transform_id(child);
  }

  delete_entity_by_transform_id(transform_id);
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

  const auto transform = util::deserialize(entity.transform);
  std::optional<transform_hierarchy::transform_id_t> transform_id;
  if (parent.has_value()) {
    transform_id = _transform_hierarchy->add_child_local_location(
        transform, parent.value());
  } else {
    transform_id = _transform_hierarchy->add(transform);
  }

  std::optional<entity_t> deserialized;

  if (auto *static_mesh =
          std::get_if<serialization::v1::static_mesh_t>(&entity.kind)) {
    static_mesh_entity_t kind(entity.name, transform_id.value());
    if (static_mesh->model_source.has_value() &&
        static_mesh->model_source != "") {
      auto renderable =
          _resources->get_renderable(static_mesh->model_source.value());
      if (!renderable) {
        ALEX_WARN("Could not find model source for name '{}'",
                  static_mesh->model_source.value());
        return;
      }

//     kind.set_renderable(static_mesh->model_source.value(), _core,
//                         _renderer, *renderable);
    }

    kind.set_has_mesh_collider(static_mesh->has_mesh_collider);
    deserialized = entity_t(std::move(kind));
  } else if (auto *hemisphere =
                 std::get_if<serialization::v1::light_hemisphere_t>(
                     &entity.kind)) {
    deserialized = entity_t(light_entity_t(entity.name, transform_id.value(),
                                           util::deserialize(*hemisphere)));
  } else if (auto *directional =
                 std::get_if<serialization::v1::light_directional_t>(
                     &entity.kind)) {
    deserialized = entity_t(light_entity_t(entity.name, transform_id.value(),
                                           util::deserialize(*directional)));
  } else if (auto *spot =
                 std::get_if<serialization::v1::light_spot_t>(&entity.kind)) {
    deserialized = entity_t(light_entity_t(entity.name, transform_id.value(),
                                           util::deserialize(*spot)));
  } else if (auto *point =
                 std::get_if<serialization::v1::light_point_t>(&entity.kind)) {
    deserialized = entity_t(light_entity_t(entity.name, transform_id.value(),
                                           util::deserialize(*point)));
  } else if (auto *hemisphere =
                 std::get_if<serialization::v1::light_hemisphere_t>(
                     &entity.kind)) {
    deserialized = entity_t(light_entity_t(entity.name, transform_id.value(),
                                           util::deserialize(*hemisphere)));
  }

  if (deserialized.has_value()) {
    add_entity(std::move(deserialized.value()));
  }

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
    serialized.transform = util::serialize(*location);
  }

  if (auto *static_mesh = entity->static_mesh()) {
    serialized.kind = util::serialize(*static_mesh);
  } else if (auto *light = entity->light()) {
    if (auto *hemisphere = light->hemisphere()) {
      serialized.kind = util::serialize(*hemisphere);
    } else if (auto *directional = light->directional()) {
      serialized.kind = util::serialize(*directional);
    } else if (auto *point = light->point()) {
      serialized.kind = util::serialize(*point);
    } else if (auto *spot = light->spot()) {
      serialized.kind = util::serialize(*spot);
    }
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
