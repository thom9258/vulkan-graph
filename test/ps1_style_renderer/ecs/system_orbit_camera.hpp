#pragma once

#include "manager.hpp"

namespace ecs::system {

struct orbit_camera_update_info_t {
  manager_t *manager;
  std::span<SDL_Event> sdl_events;
  double deltatime;
};

constexpr auto orbit_camera_update(orbit_camera_update_info_t &info) -> void {
  std::array<entity_id_t, orbit_camera_components_max> entities;
  std::size_t entity_count = info.manager->get_entities(entities);
  entity_count = info.manager->filter_inplace<component_orbit_camera_t>(
      std::span(entities).subspan(0, entity_count));

  auto is_active = [&](ecs::entity_id_t id) {
    auto *orbit_camera =
        info.manager->get_component<component_orbit_camera_t>(id);
    return orbit_camera->active;
  };

  auto active_cameras =
      entities | std::views::take(entity_count) | std::views::filter(is_active) | std::ranges::to<std::vector>();

  auto *orbit_camera =
      info.manager->get_component<component_orbit_camera_t>(active_cameras[0]);

  for (SDL_Event event : info.sdl_events) {
    switch (event.type) {
    case SDL_KEYUP:
      switch (event.key.keysym.sym) {
      case SDLK_w:
        orbit_camera->w.release();
        break;
      case SDLK_s:
        orbit_camera->s.release();
        break;
      case SDLK_a:
        orbit_camera->a.release();
        break;
      case SDLK_d:
        orbit_camera->d.release();
        break;
      case SDLK_e:
        orbit_camera->e.release();
        break;
      case SDLK_q:
        orbit_camera->q.release();
        break;
      }
      break;

    case SDL_KEYDOWN:
      switch (event.key.keysym.sym) {
      case SDLK_w:
        orbit_camera->w.press();
        break;
      case SDLK_s:
        orbit_camera->s.press();
        break;
      case SDLK_a:
        orbit_camera->a.press();
        break;
      case SDLK_d:
        orbit_camera->d.press();
        break;
      case SDLK_e:
        orbit_camera->e.press();
        break;
      case SDLK_q:
        orbit_camera->q.press();
        break;
        break;
      }
    }
  }

  const auto rotatespeed = orbit_camera->rotatespeed * info.deltatime;
  const auto zoomspeed = orbit_camera->zoomspeed * info.deltatime;

  if (orbit_camera->w.is_pressed()) {
    orbit_camera->camera.add_rotation(rotatespeed, 0.0f);
  }
  if (orbit_camera->a.is_pressed()) {
    orbit_camera->camera.add_rotation(0.0f, rotatespeed);
  }
  if (orbit_camera->s.is_pressed()) {
    orbit_camera->camera.add_rotation(-rotatespeed, 0.0f);
  }
  if (orbit_camera->d.is_pressed()) {
    orbit_camera->camera.add_rotation(0.0f, -rotatespeed);
  }
  if (orbit_camera->e.is_pressed()) {
    orbit_camera->camera.set_radius(orbit_camera->camera.radius() + zoomspeed);
  }
  if (orbit_camera->q.is_pressed()) {
    orbit_camera->camera.set_radius(orbit_camera->camera.radius() - zoomspeed);
  }
}

} // namespace ecs::system
