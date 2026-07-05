#pragma once

#include "glm_transform_hierarchy.hpp"
#include "static_object.hpp"

#include "imgui.h"
#include "utility/transform_hierarchy.hpp"

#include <span>

namespace game {

class ui_hierarchy_t {
public:
  using transform_id_t = transform_hierarchy::transform_id_t;

  constexpr ui_hierarchy_t() = default;

  constexpr explicit ui_hierarchy_t(
      glm_transform_hierarchy *transform_hierarchy);

  constexpr auto draw_node(std::span<static_object_t> static_objects,
                           std::size_t &index, transform_id_t id) -> void;

  constexpr auto draw(std::span<static_object_t> static_objects) -> void;

  constexpr auto find(std::span<static_object_t> static_objects,
                      transform_id_t id) -> static_object_t *;

private:
  glm_transform_hierarchy *_transform_hierarchy{nullptr};
};

constexpr ui_hierarchy_t::ui_hierarchy_t(
    glm_transform_hierarchy *transform_hierarchy)
    : _transform_hierarchy{transform_hierarchy} {}

constexpr auto ui_hierarchy_t::find(std::span<static_object_t> static_objects,
                                    transform_id_t id) -> static_object_t * {
  for (static_object_t &object : static_objects) {
    if (object.transform_id == id) {
      return &object;
    }
  }

  return nullptr;
}

constexpr auto
ui_hierarchy_t::draw_node(std::span<static_object_t> static_objects,
                          std::size_t &index, transform_id_t id) -> void {

  static_object_t *object = find(static_objects, id);
  if (object == nullptr) {
    return;
  }

  ImGui::PushID(index);
  ImGuiTreeNodeFlags flags =
      ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick |
      ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth;

  if (ImGui::TreeNodeEx(std::format("{}", index).c_str(), flags)) {
    std::vector<transform_id_t> children = _transform_hierarchy->children(id);
    for (transform_id_t child : children) {
      draw_node(static_objects, index, child);
    }

    ImGui::TreePop();
  }

  ImGui::PopID();
  index++;
}

constexpr auto ui_hierarchy_t::draw(std::span<static_object_t> static_objects)
    -> void {
  ImGui::Begin("Hierarchy");
  // https://kahwei.dev/2022/06/20/imgui-tree-node/
  std::size_t index = 0;

  std::vector<transform_id_t> roots = _transform_hierarchy->roots();

  ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen;
  if (ImGui::TreeNodeEx("world", flags)) {
    for (transform_id_t root : roots) {
      draw_node(static_objects, index, root);
    }

    ImGui::TreePop();
  }

  ImGui::End();
}

} // namespace game
