#pragma once

#include "glm_transform_hierarchy.hpp"
#include "ps1_style_renderer/include_glm.hpp"
#include "static_object.hpp"

#include "imgui.h"
#include "utility/transform_hierarchy.hpp"

#include <print>
#include <span>

namespace game {

class scene_ui_t {
public:
  using transform_id_t = transform_hierarchy::transform_id_t;

  constexpr scene_ui_t() = default;

  constexpr explicit scene_ui_t(glm_transform_hierarchy *transform_hierarchy);

  constexpr auto draw_node(std::size_t &id_index,
                           std::span<static_object_t> static_objects,
                           transform_id_t id) -> void;

  constexpr auto draw_hierarchy(std::span<static_object_t> static_objects)
      -> void;

  constexpr auto draw_selected() -> void;

  constexpr auto draw(std::span<static_object_t> static_objects) -> void;

  constexpr auto find(std::span<static_object_t> static_objects,
                      transform_id_t id) -> static_object_t *;

private:
  glm_transform_hierarchy *_transform_hierarchy{nullptr};
  static_object_t *_selected{nullptr};
};

constexpr scene_ui_t::scene_ui_t(glm_transform_hierarchy *transform_hierarchy)
    : _transform_hierarchy{transform_hierarchy} {}

constexpr auto scene_ui_t::find(std::span<static_object_t> static_objects,
                                transform_id_t id) -> static_object_t * {
  for (static_object_t &object : static_objects) {
    if (object.transform_id == id) {
      return &object;
    }
  }

  return nullptr;
}

constexpr auto scene_ui_t::draw_node(std::size_t &id_index,
                                     std::span<static_object_t> static_objects,
                                     transform_id_t id) -> void {

  static_object_t *object = find(static_objects, id);
  if (object == nullptr) {
    return;
  }

  ImGuiTreeNodeFlags flags =
      ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_Selected |
      ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth;

  ImGui::PushID(id_index);
  bool const open = ImGui::TreeNodeEx(object->name.c_str(), flags);
  if (ImGui::IsItemClicked()) {
    std::println("Selected {}", object->name);
    _selected = object;
  }

  if (open) {
    std::vector<transform_id_t> children = _transform_hierarchy->children(id);
    for (transform_id_t child : children) {
      draw_node(id_index, static_objects, child);
    }

    ImGui::TreePop();
  }

  ImGui::PopID();
  id_index++;
}

constexpr auto
scene_ui_t::draw_hierarchy(std::span<static_object_t> static_objects) -> void {
  ImGui::Begin("Hierarchy");
  // https://kahwei.dev/2022/06/20/imgui-tree-node/
  std::vector<transform_id_t> roots = _transform_hierarchy->roots();

  std::size_t id_index = 0;

  ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen;
  if (ImGui::TreeNodeEx("World", flags)) {
    for (transform_id_t root : roots) {
      draw_node(id_index, static_objects, root);
    }

    ImGui::TreePop();
  }

  ImGui::End();
}

constexpr auto scene_ui_t::draw_selected() -> void {
  ImGui::Begin("Selected");
  if (_selected != nullptr) {
    if (_transform_hierarchy->is_valid(_selected->transform_id)) {
      ImGui::Text("%s", _selected->name.c_str());
      auto transform =
          _transform_hierarchy->global_location(_selected->transform_id);
      glm::vec3 scale;
      glm::quat orientation;
	  glm::vec3 orientationEulerRadian = glm::eulerAngles(orientation);
	  glm::vec3 orientationeulerDegree = glm::degrees(orientationEulerRadian);
      glm::vec3 translation;
      glm::vec3 skew;
      glm::vec4 perspective;
      glm::decompose(transform.value(), scale, orientation, translation, skew,
                     perspective);

	  ImGui::Text("Pos:   %f %f %f", translation.x, translation.y, translation.z);
	  ImGui::Text("Rot:   %f %f %f", orientationeulerDegree.x, orientationeulerDegree.y, orientationeulerDegree.z);
	  ImGui::Text("Scale: %f %f %f", scale.x, scale.y, scale.z);
    }
  }

  ImGui::End();
}

constexpr auto scene_ui_t::draw(std::span<static_object_t> static_objects)
    -> void {
  draw_hierarchy(static_objects);
  draw_selected();
}

} // namespace game
