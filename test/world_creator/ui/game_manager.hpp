#pragma once

#include "imgui.h"
#include "imgui_context.hpp"

#include <span>

namespace game::ui {

enum class render_mode_t { lighting, base_color, colliders };

constexpr auto to_render_mode(int mode) -> render_mode_t {
  switch (mode) {
  case 0:
    return render_mode_t::lighting;
  case 1:
    return render_mode_t::base_color;
  case 2:
    return render_mode_t::colliders;
  default:
    break;
  };
  return render_mode_t::lighting;
}

class game_manager_t {
public:
  constexpr game_manager_t() = default;
  constexpr auto draw() -> void;
  constexpr auto update_input(std::span<SDL_Event> events) -> void;
  constexpr auto should_close() const -> bool;
  constexpr auto show_entity_hierarchy() const -> bool;
  constexpr auto show_level_settings() const -> bool;
  constexpr auto render_mode() const -> render_mode_t;

private:
  bool _should_close{false};
  bool _show_entity_hierarchy{true};
  bool _show_level_settings{true};
  int _render_mode_index{0};
};

constexpr auto game_manager_t::update_input(std::span<SDL_Event>) -> void {}

constexpr auto game_manager_t::should_close() const -> bool {
  return _should_close;
}

constexpr auto game_manager_t::show_entity_hierarchy() const -> bool {
  return _show_entity_hierarchy;
}

constexpr auto game_manager_t::show_level_settings() const -> bool {
  return _show_level_settings;
}

constexpr auto game_manager_t::render_mode() const -> render_mode_t {
  return to_render_mode(_render_mode_index);
}

constexpr auto game_manager_t::draw() -> void {

  const int flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                    ImGuiWindowFlags_NoScrollbar;
  ImGui::Begin("Game Manager", 0, flags);
  ImGui::SetWindowPos(ImVec2{0.0f, 0.0f});
  ImGui::SetWindowSize(ImVec2{0.0f, 0.0f});
  if (ImGui::Button("Exit Game")) {
    _should_close = true;
  }

  ImGui::Text("Entity Hierarchy");
  ImGui::SameLine();
  ImGui::Checkbox("##Entity Hierarchy", &_show_entity_hierarchy);

  ImGui::Text("Level Settings");
  ImGui::SameLine();
  ImGui::Checkbox("##Level Settings", &_show_level_settings);

  static const char *render_modes[]{"Lighting", "Base Color"};
  ImGui::Text("Render Mode");
  ImGui::SameLine();
  if (ImGui::Combo("##RenderMode", &_render_mode_index, render_modes,
                   IM_ARRAYSIZE(render_modes))) {
  }

  ImGui::End();
}

} // namespace game::ui
