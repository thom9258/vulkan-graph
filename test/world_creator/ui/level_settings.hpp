#pragma once

#include "../glm_transform_hierarchy.hpp"
#include "../include_glm.hpp"
#include "../resources.hpp"
#include "../static_mesh_entity.hpp"
#include "../static_render.hpp"
#include "../world.hpp"
#include "ImGuizmo.h"
#include "imgui.h"

#include "imgui_context.hpp"
#include "utility/transform_hierarchy.hpp"

#include <SDL_keycode.h>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm> // for std::swap
#include <print>
#include <ranges>
#include <span>
#include <string>
#include <string_view>

namespace game::ui {

class level_settings_t {
public:
  constexpr level_settings_t() = default;

  constexpr level_settings_t(world_t *world);

  constexpr auto draw() -> void;

private:
  world_t *_world{nullptr};
};

constexpr level_settings_t::level_settings_t(world_t *world) : _world{world} {}

constexpr auto level_settings_t::draw() -> void {
  ImGui::Begin("Level Settings");

  ImGuiColorEditFlags colorEdiFlags = ImGuiColorEditFlags_PickerHueBar;
  ImGui::Text("Background Color");
  if (ImGui::ColorPicker3("##Background Color",
                          _world->background_color().data(), colorEdiFlags)) {
  }

  ImGui::Separator();

  ImGui::End();
}

} // namespace game::ui
