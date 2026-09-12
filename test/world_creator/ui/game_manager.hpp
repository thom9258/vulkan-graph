#pragma once

#include "imgui.h"
#include "imgui_context.hpp"

#include <span>

namespace game::ui {

class game_manager_t {
public:
  constexpr game_manager_t() = default;
  constexpr auto draw() -> void;
  constexpr auto update_input(std::span<SDL_Event> events) -> void;
  constexpr auto should_close() const -> bool;
  constexpr auto show_entity_hierarchy() const -> bool;
  constexpr auto show_level_settings() const -> bool;

private:
  bool _should_close{false};
  bool _show_entity_hierarchy{true};
  bool _show_level_settings{true};
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

  ImGui::End();
}

} // namespace game::ui
