#pragma once

#include "ImGuizmo.h"
#include "glm_transform_hierarchy.hpp"
#include "imgui.h"
#include "object.hpp"
#include "ps1_style_renderer/include_glm.hpp"

#include "imgui_context.hpp"
#include "utility/transform_hierarchy.hpp"

#include <SDL_keycode.h>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm> // for std::swap
#include <print>
#include <span>

namespace game {

class scene_ui_t {
public:
  using transform_id_t = transform_hierarchy::transform_id_t;

  constexpr scene_ui_t() = default;

  constexpr explicit scene_ui_t(glm_transform_hierarchy *transform_hierarchy);

  constexpr auto update_input(std::span<SDL_Event> events) -> void;

  constexpr auto draw_node(std::size_t &id_index, std::span<object_t> objects,
                           transform_id_t id) -> void;

  constexpr auto draw_begin() -> void;

  constexpr auto draw_end() -> void;

  constexpr auto draw_edit_mode() -> void;

  constexpr auto draw_hierarchy(std::span<object_t> objects) -> void;

  constexpr auto draw_selected(glm::mat4 view, glm::mat4 projection) -> void;

  constexpr auto draw(std::span<object_t> objects, glm::mat4 view,
                      glm::mat4 projection) -> void;

  constexpr auto find(std::span<object_t> objects, transform_id_t id)
      -> object_t *;

private:
  glm_transform_hierarchy *_transform_hierarchy{nullptr};
  object_t *_selected{nullptr};
  int _selected_operation{0};
  int _selected_locale{0};
};

static const char *operations[]{"Translate", "Rotate", "Scale"};

constexpr auto index_to_operation(int selected) -> ImGuizmo::OPERATION {
  switch (selected) {
  case 1:
    return ImGuizmo::ROTATE;
  case 2:
    return ImGuizmo::SCALE;
  default:
    break;
  }
  return ImGuizmo::TRANSLATE;
}

constexpr auto operation_to_index(ImGuizmo::OPERATION op) -> int {
  switch (op) {
  case ImGuizmo::ROTATE:
    return 1;
  case ImGuizmo::SCALE:
    return 2;
  default:
    break;
  }
  return 0;
}

static const char *locales[]{"World", "Local"};

constexpr auto index_to_locale(int selected) -> ImGuizmo::MODE {
  switch (selected) {
  case 1:
    return ImGuizmo::LOCAL;
  default:
    break;
  }
  return ImGuizmo::WORLD;
}

constexpr auto mode_to_index(ImGuizmo::MODE mode) -> int {
  switch (mode) {
  case ImGuizmo::LOCAL:
    return 1;
  default:
    break;
  }
  return 0;
}

constexpr scene_ui_t::scene_ui_t(glm_transform_hierarchy *transform_hierarchy)
    : _transform_hierarchy{transform_hierarchy} {}

constexpr auto scene_ui_t::update_input(std::span<SDL_Event> events) -> void {
  //auto shift_pressed = false;
  for (SDL_Event event : events) {
    switch (event.type) {
   //case SDL_PRESSED:
   //  switch (event.key.keysym.sym) {
   //  case SDLK_LSHIFT:
   //    shift_pressed = true;
   //    break;
   //  }
   //  break;

    case SDL_KEYDOWN:
      switch (event.key.keysym.sym) {
      case SDLK_ESCAPE: {
        _selected = nullptr;
        break;
      }
      case SDLK_1: {
        //if (shift_pressed) {
          _selected_operation = operation_to_index(ImGuizmo::TRANSLATE);
        //}
      } break;
      case SDLK_2: {
        //if (shift_pressed) {
          _selected_operation = operation_to_index(ImGuizmo::ROTATE);
        //}
      } break;
      case SDLK_3: {
        //if (shift_pressed) {
          _selected_operation = operation_to_index(ImGuizmo::SCALE);
        //}
      } break;
      }
    }
  }
}

constexpr auto scene_ui_t::find(std::span<object_t> objects, transform_id_t id)
    -> object_t * {
  for (object_t &object : objects) {
    if (object.transform_id == id) {
      return &object;
    }
  }

  return nullptr;
}

constexpr auto scene_ui_t::draw_node(std::size_t &id_index,
                                     std::span<object_t> objects,
                                     transform_id_t id) -> void {

  object_t *object = find(objects, id);
  if (object == nullptr) {
    return;
  }

  ImGuiTreeNodeFlags flags =
      ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_Selected |
      ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth;

  ImGui::PushID(id_index);
  bool const open = ImGui::TreeNodeEx(object->name.c_str(), flags);
  if (ImGui::IsItemClicked()) {
    _selected = object;
  }

  if (open) {
    std::vector<transform_id_t> children = _transform_hierarchy->children(id);
    for (transform_id_t child : children) {
      draw_node(id_index, objects, child);
    }

    ImGui::TreePop();
  }

  ImGui::PopID();
  id_index++;
}

constexpr auto scene_ui_t::draw_begin() -> void {
  ImGui::Begin("Level Editor");
}

constexpr auto scene_ui_t::draw_end() -> void { ImGui::End(); }

constexpr auto scene_ui_t::draw_hierarchy(std::span<object_t> objects) -> void {
  // https://kahwei.dev/2022/06/20/imgui-tree-node/

  std::vector<transform_id_t> roots = _transform_hierarchy->roots();
  std::size_t id_index = 0;
  for (transform_id_t root : roots) {
    draw_node(id_index, objects, root);
  }
}

constexpr auto scene_ui_t::draw_edit_mode() -> void {
  if (ImGui::Combo("Operation", &_selected_operation, operations,
                   IM_ARRAYSIZE(operations))) {
  }

  if (ImGui::Combo("Mode", &_selected_locale, locales, IM_ARRAYSIZE(locales))) {
  }
}

constexpr auto scene_ui_t::draw_selected(glm::mat4 view, glm::mat4 projection)
    -> void {
  if (_selected != nullptr) {
    if (_transform_hierarchy->is_valid(_selected->transform_id)) {
      ImGui::Text("%s", _selected->name.c_str());
      auto transform =
          _transform_hierarchy->global_location(_selected->transform_id);
      glm::vec3 scale;
      glm::quat orientation;
      glm::vec3 orientationEulerRadian = glm::eulerAngles(orientation);
      glm::vec3 orientationeulerDegree = glm::degrees(orientationEulerRadian);
      glm::vec3 translation(0.0f);
      glm::vec3 skew;
      glm::vec4 perspective;
      glm::decompose(transform.value(), scale, orientation, translation, skew,
                     perspective);

      ImGui::Text("Pos:   %f %f %f", translation.x, translation.y,
                  translation.z);
      ImGui::Text("Rot:   %f %f %f", orientationeulerDegree.x,
                  orientationeulerDegree.y, orientationeulerDegree.z);
      ImGui::Text("Scale: %f %f %f", scale.x, scale.y, scale.z);

      ImGuiIO &io = ImGui::GetIO();
      ImGuizmo::SetOrthographic(false);
      ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);

      glm::mat4 model = transform.value();
      // imguizmo uses opengl style projection but since we are in vulkan,
      // we need to revert the projection matrix flip we do when calculating
      // the projection matrix
      projection[1][1] *= -1.0f;

      ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(projection),
                           index_to_operation(_selected_operation),
                           index_to_locale(_selected_locale),
                           glm::value_ptr(model));

      if (ImGuizmo::IsUsing()) {
        _transform_hierarchy->set_global_location(_selected->transform_id,
                                                  model);
      }
    }
  }
}

constexpr auto scene_ui_t::draw(std::span<object_t> objects, glm::mat4 view,
                                glm::mat4 projection) -> void {
  draw_begin();
  draw_edit_mode();
  ImGui::Separator();
  draw_hierarchy(objects);
  ImGui::Separator();
  draw_selected(view, projection);
  draw_end();
}

} // namespace game
