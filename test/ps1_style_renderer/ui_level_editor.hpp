#pragma once

#include "ImGuizmo.h"
#include "glm_transform_hierarchy.hpp"
#include "imgui.h"
#include "object.hpp"
#include "actor.hpp"
#include "ps1_style_renderer/include_glm.hpp"

#include "imgui_context.hpp"
#include "utility/transform_hierarchy.hpp"

#include <SDL_keycode.h>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm> // for std::swap
#include <print>
#include <span>
#include <string_view>

namespace game {

class ui_level_editor_t {
public:
  using transform_id_t = transform_hierarchy::transform_id_t;

  constexpr ui_level_editor_t() = default;

  constexpr explicit ui_level_editor_t(
      glm_transform_hierarchy *transform_hierarchy);

  constexpr auto update_input(std::span<SDL_Event> events) -> void;

  constexpr auto draw_node(std::size_t &id_index, std::span<object_t> objects,
                           transform_id_t id) -> void;

  constexpr auto draw_edit_mode() -> void;

  constexpr auto draw_hierarchy(std::span<object_t> objects, std::span<actor_t> actors) -> void;

  constexpr auto draw_selected(glm::mat4 view, glm::mat4 projection) -> void;

  constexpr auto draw(std::span<object_t> objects, std::span<actor_t> actors, glm::mat4 view,
                      glm::mat4 projection) -> void;

  constexpr auto find(std::span<object_t> objects, transform_id_t id)
      -> object_t *;

private:
  glm_transform_hierarchy *_transform_hierarchy{nullptr};
  bool _show_object_hierarchy{false};
  bool _show_actor_hierarchy{false};

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

constexpr ui_level_editor_t::ui_level_editor_t(
    glm_transform_hierarchy *transform_hierarchy)
    : _transform_hierarchy{transform_hierarchy} {}

constexpr auto ui_level_editor_t::update_input(std::span<SDL_Event> events)
    -> void {
  // auto shift_pressed = false;
  for (SDL_Event event : events) {
    switch (event.type) {
      // case SDL_PRESSED:
      //   switch (event.key.keysym.sym) {
      //   case SDLK_LSHIFT:
      //     shift_pressed = true;
      //     break;
      //   }
      //   break;

    case SDL_KEYDOWN:
      switch (event.key.keysym.sym) {
      case SDLK_ESCAPE: {
        _selected = nullptr;
        break;
      }
      case SDLK_1: {
        // if (shift_pressed) {
        _selected_operation = operation_to_index(ImGuizmo::TRANSLATE);
        //}
      } break;
      case SDLK_2: {
        // if (shift_pressed) {
        _selected_operation = operation_to_index(ImGuizmo::ROTATE);
        //}
      } break;
      case SDLK_3: {
        // if (shift_pressed) {
        _selected_operation = operation_to_index(ImGuizmo::SCALE);
        //}
      } break;
      }
    }
  }
}

constexpr auto ui_level_editor_t::find(std::span<object_t> objects,
                                       transform_id_t id) -> object_t * {
  for (object_t &object : objects) {
    if (object.transform_id == id) {
      return &object;
    }
  }

  return nullptr;
}

static constexpr const std::string_view object_drag_id{"DRAGGED OBJECT"};

constexpr auto ui_level_editor_t::draw_node(std::size_t &id_index,
                                            std::span<object_t> objects,
                                            transform_id_t id) -> void {

  object_t *object = find(objects, id);
  if (object == nullptr) {
    return;
  }

  ImGuiTreeNodeFlags flags =
      ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_Selected;

  ImGui::PushID(id_index);
  bool const open = ImGui::TreeNodeEx(object->name.c_str(), flags);

  if (ImGui::BeginDragDropTarget()) {
    ImGuiDragDropFlags target_flags =
        ImGuiDragDropFlags_AcceptNoDrawDefaultRect;
    if (const ImGuiPayload *payload =
            ImGui::AcceptDragDropPayload(object_drag_id.data(), target_flags)) {
      auto dropped = reinterpret_cast<object_t *>(payload->Data);
      _transform_hierarchy->reparent(dropped->transform_id,
                                     object->transform_id);
    }

    ImGui::EndDragDropTarget();
  }

  if (ImGui::BeginDragDropSource()) {
    ImGui::SetDragDropPayload(object_drag_id.data(), object, sizeof(object_t));
    ImGui::EndDragDropSource();
  }

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

constexpr auto ui_level_editor_t::draw_hierarchy(std::span<object_t> objects, std::span<actor_t> actors)
    -> void {
  // https://kahwei.dev/2022/06/20/imgui-tree-node/
  // https://ruby0x1.github.io/machinery_blog_archive/post/implementing-drag-and-drop-in-an-imgui/index.html
  std::size_t id_index = 0;
  std::vector<transform_id_t> roots = _transform_hierarchy->roots();

  ImGui::Separator();
  ImGui::Checkbox("Objects", &_show_object_hierarchy);
  if (_show_object_hierarchy) {
	//TODO: filter transforms to only contain objects
    for (transform_id_t root : roots) {
      draw_node(id_index, objects, root);
    }
  }

  ImGui::Separator();
  ImGui::Checkbox("Actors", &_show_actor_hierarchy);
  if (_show_actor_hierarchy) {
	//TODO: filter transforms to only contain actors
    //     std::vector<transform_id_t> roots = _transform_hierarchy->roots();
    //     for (transform_id_t root : roots) {
    //       draw_node(id_index, objects, root);
    //     }
  }

  //   ImGui::Separator();
  //   ImGui::Text("Player");
}

constexpr auto ui_level_editor_t::draw_edit_mode() -> void {
  if (ImGui::Combo("Operation", &_selected_operation, operations,
                   IM_ARRAYSIZE(operations))) {
  }

  if (ImGui::Combo("Mode", &_selected_locale, locales, IM_ARRAYSIZE(locales))) {
  }
}

constexpr auto ui_level_editor_t::draw_selected(glm::mat4 view,
                                                glm::mat4 projection) -> void {
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

constexpr auto ui_level_editor_t::draw(std::span<object_t> objects, std::span<actor_t> actors,
                                       glm::mat4 view, glm::mat4 projection)
    -> void {
  ImGui::Begin("Level Editor");
  draw_edit_mode();
  ImGui::Separator();
  draw_hierarchy(objects, actors);
  ImGui::Separator();
  draw_selected(view, projection);
  ImGui::End();
}

} // namespace game
