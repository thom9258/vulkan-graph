#pragma once

#include "imgui.h"
#include "imgui_context.hpp"

#include <span>

namespace game {

class ui_game_manager_t {
public:
  constexpr ui_game_manager_t() = default;
  constexpr auto draw() -> void;
  constexpr auto update_input(std::span<SDL_Event> events) -> void;
  constexpr auto should_close() const -> bool;
  constexpr auto show_level_editor() const -> bool;

private:
  bool _should_close{false};
  bool _show_level_editor{true};
};

constexpr auto ui_game_manager_t::update_input(std::span<SDL_Event>) -> void {}

constexpr auto ui_game_manager_t::should_close() const -> bool {
  return _should_close;
}
constexpr auto ui_game_manager_t::show_level_editor() const -> bool {
  return _show_level_editor;
}

constexpr auto ui_game_manager_t::draw() -> void {

  const int flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                    ImGuiWindowFlags_NoScrollbar;
  ImGui::Begin("Game Manager", 0, flags);
  ImGui::SetWindowPos(ImVec2{0.0f, 0.0f});
  ImGui::SetWindowSize(ImVec2{0.0f, 0.0f});
  if (ImGui::Button("Exit Game")) {
    _should_close = true;
  }

  ImGui::Checkbox("Level Selector", &_show_level_editor);
  ImGui::End();
}

} // namespace game
