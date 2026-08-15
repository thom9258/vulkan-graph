#pragma once

#include "ImGuizmo.h"
#include "glm_transform_hierarchy.hpp"
#include "imgui.h"
#include "include_glm.hpp"
#include "resources.hpp"
#include "static_render.hpp"
#include "world.hpp"

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

namespace game {

class ui_level_editor_t {
public:
  using transform_id_t = transform_hierarchy::transform_id_t;

  constexpr ui_level_editor_t() = default;

  constexpr explicit ui_level_editor_t(
      world_t *world, glm_transform_hierarchy *transform_hierarchy,
      resources_t *resources, alex::core_t *core,
      static_render_t *static_render);

  constexpr auto update_input(std::span<SDL_Event> events) -> void;

  constexpr auto draw_entity(std::size_t &id_index,
                             std::span<entity_t> entities, transform_id_t id)
      -> void;

  constexpr auto draw_player(std::size_t &id_index, player_t &player,
                             transform_id_t id) -> void;

  constexpr auto draw_edit_mode() -> void;

  constexpr auto draw_world_manager(world_t &world) -> void;

  constexpr auto draw_hierarchy(world_t &world) -> void;

  constexpr auto draw_selected(glm::mat4 view, glm::mat4 projection) -> void;

  constexpr auto draw(world_t &world) -> void;

  constexpr auto find(std::span<entity_t> entities, transform_id_t id)
      -> entity_t *;

private:
  glm_transform_hierarchy *_transform_hierarchy{nullptr};
  resources_t *_resources{nullptr};
  world_t *_world{nullptr};
  alex::core_t *_core{nullptr};
  static_render_t *_static_render{nullptr};

  std::optional<std::string> _world_path_str;

  std::vector<std::string> _all_models;
  int _add_entity_selected{0};

  bool _show_entity_hierarchy{true};

  entity_t *_selected{nullptr};
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
    world_t *world, glm_transform_hierarchy *transform_hierarchy,
    resources_t *resources, alex::core_t *core, static_render_t *static_render)
    : _world{world}, _transform_hierarchy{transform_hierarchy},
      _resources{resources}, _core{core}, _static_render{static_render} {

  _all_models = _resources->get_all_model_names();
}

constexpr auto ui_level_editor_t::update_input(std::span<SDL_Event>) -> void {

  if (!ImGui::IsAnyItemActive()) {
    if (ImGui::IsKeyDown(ImGuiMod_Alt)) {
      if (ImGui::IsKeyPressed(ImGuiKey_1))
        _selected_operation = operation_to_index(ImGuizmo::TRANSLATE);
      if (ImGui::IsKeyPressed(ImGuiKey_2))
        _selected_operation = operation_to_index(ImGuizmo::ROTATE);
      if (ImGui::IsKeyPressed(ImGuiKey_3))
        _selected_operation = operation_to_index(ImGuizmo::SCALE);
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
      _selected = nullptr;
    }
  }
}

constexpr auto ui_level_editor_t::find(std::span<entity_t> entities,
                                       transform_id_t id) -> entity_t * {
  for (entity_t &entity : entities) {
    if (entity.transform_id() == id) {
      return &entity;
    }
  }

  return nullptr;
}

constexpr auto ui_level_editor_t::draw_entity(std::size_t &id_index,
                                              std::span<entity_t> entities,
                                              transform_id_t id) -> void {

  static constexpr const std::string_view drag_id{"DRAGGED ENTITY"};
  entity_t *entity = find(entities, id);
  if (entity == nullptr) {
    return;
  }

  ImGuiTreeNodeFlags flags =
      ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_Selected;

  ImGui::PushID(id_index);
  auto const name = std::string(entity->name());
  bool const open = ImGui::TreeNodeEx(name.c_str(), flags);

  if (ImGui::BeginDragDropTarget()) {
    ImGuiDragDropFlags target_flags =
        ImGuiDragDropFlags_AcceptNoDrawDefaultRect;
    if (const ImGuiPayload *payload =
            ImGui::AcceptDragDropPayload(drag_id.data(), target_flags)) {
      auto dropped = reinterpret_cast<entity_t *>(payload->Data);
      _transform_hierarchy->reparent(dropped->transform_id(),
                                     entity->transform_id());
    }

    ImGui::EndDragDropTarget();
  }

  if (ImGui::BeginDragDropSource()) {
    ImGui::SetDragDropPayload(drag_id.data(), entity, sizeof(entity_t));
    ImGui::EndDragDropSource();
  }

  if (ImGui::IsItemClicked()) {
    _selected = entity;
  }

  if (open) {
    std::vector<transform_id_t> children = _transform_hierarchy->children(id);
    for (transform_id_t child : children) {
      draw_entity(id_index, entities, child);
    }

    ImGui::TreePop();
  }

  ImGui::PopID();
  id_index++;
}

constexpr auto ui_level_editor_t::draw_world_manager(world_t &world) -> void {
  thread_local static std::array<char, 512> world_path_buffer;
  ImGui::InputText("##world manager path", world_path_buffer.data(),
                   world_path_buffer.size(),
                   ImGuiInputTextFlags_EnterReturnsTrue);

  auto world_path = std::filesystem::path(
      std::string(world_path_buffer.data(), world_path_buffer.size()));

  if (ImGui::Button("Save")) {
    world.save_world(world_path);
  }

  ImGui::SameLine();
  if (ImGui::Button("Load")) {
    world.load_world(world_path);
  }
}

constexpr auto ui_level_editor_t::draw_hierarchy(world_t &world) -> void {
  // https://kahwei.dev/2022/06/20/imgui-tree-node/
  // https://ruby0x1.github.io/machinery_blog_archive/post/implementing-drag-and-drop-in-an-imgui/index.html
  std::size_t id_index = 0;
  std::vector<transform_id_t> roots = _transform_hierarchy->roots();

  ImGui::Separator();
  ImGui::Checkbox("Entities", &_show_entity_hierarchy);
  if (_show_entity_hierarchy) {

    std::vector<const char *> model_ptrs;
    for (std::string &model : _all_models) {
      model_ptrs.push_back(model.c_str());
    }

    if (ImGui::Button("Add Static Object")) {
      auto id = _transform_hierarchy->add(glm::mat4(1.0f));
      if (id.has_value()) {
        std::println("Added entity {}", _all_models[_add_entity_selected]);
      }

      model_source_t *source =
          _resources->get_model(_all_models[_add_entity_selected]);
      if (source) {
        glm::mat4 translation =
            glm::translate(glm::mat4(1.0f), glm::vec3(0.0f));
        glm::mat4 model_matrix = glm::scale(translation, glm::vec3(0.05f));
        auto id = _world->transform_hierarchy().add(model_matrix);
        auto fox_entity = static_mesh_entity_t("fox", id.value());
        fox_entity.set_model_source(_core, _static_render, *source);
        _world->add_entity(std::move(fox_entity));
        _selected = &_world->entities().back();
      }
    }

    ImGui::SameLine();
    if (ImGui::Combo("##Add Entity List", &_add_entity_selected,
                     model_ptrs.data(), model_ptrs.size())) {
    }

    for (transform_id_t root : roots) {
      draw_entity(id_index, world.entities(), root);
    }
  }

  //   ImGui::Separator();
  //   ImGui::Text("Player");
}

constexpr auto ui_level_editor_t::draw_edit_mode() -> void {
  ImGui::Text("Operation");
  ImGui::SameLine();
  if (ImGui::Combo("##Operation", &_selected_operation, operations,
                   IM_ARRAYSIZE(operations))) {
  }

  ImGui::Text("Mode     ");
  ImGui::SameLine();
  if (ImGui::Combo("##Mode", &_selected_locale, locales,
                   IM_ARRAYSIZE(locales))) {
  }
}

constexpr auto ui_level_editor_t::draw_selected(glm::mat4 view,
                                                glm::mat4 projection) -> void {
  if (_selected != nullptr) {
    if (_transform_hierarchy->is_valid(_selected->transform_id())) {
      ImGui::Text("Entity Details");

      std::array<char, 128> name_buffer;
      std::ranges::fill(name_buffer, '\0');

      auto name = std::string(_selected->name());
      for (std::size_t i = 0; i < name.size(); i++) {
        name_buffer[i] = name[i];
      }

      if (ImGui::InputText("##Name", name_buffer.data(), name_buffer.size(),
                           ImGuiInputTextFlags_EnterReturnsTrue)) {
        _selected->set_name(name_buffer.data());
      }

      auto transform =
          _transform_hierarchy->global_location(_selected->transform_id());

      float translation[3];
      float rotation[3];
      float scale[3];
      ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(transform.value()),
                                            translation, rotation, scale);

      ImGui::PushItemWidth(-1.0f);
      ImGui::Text("Translation");
      ImGui::SameLine();
      bool const isUsingPos =
          ImGui::DragFloat3("##Position", translation, 0.1f);
      if (isUsingPos) {
        _selected_operation =
            operation_to_index(ImGuizmo::OPERATION::TRANSLATE);
      }

      ImGui::Text("Rotation   ");
      ImGui::SameLine();
      bool const isUsingRot = ImGui::DragFloat3("##Rotation", rotation, 1.0f);
      if (isUsingRot) {
        _selected_operation = operation_to_index(ImGuizmo::OPERATION::ROTATE);
      }

      ImGui::Text("Scale      ");
      ImGui::SameLine();
      bool const isUsingSc = ImGui::DragFloat3("##Scale", scale, 0.1f);
      if (isUsingSc) {
        _selected_operation = operation_to_index(ImGuizmo::OPERATION::SCALE);
      }

      bool const isUsingComponents = isUsingPos | isUsingRot | isUsingSc;

      ImGui::PopItemWidth();
      if (isUsingComponents) {
        ImGuizmo::RecomposeMatrixFromComponents(
            translation, rotation, scale, glm::value_ptr(transform.value()));
      }

      ImGuiIO &io = ImGui::GetIO();
      ImGuizmo::SetOrthographic(false);
      ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);

      // imguizmo uses opengl style projection but since we are in vulkan,
      // we need to revert the projection matrix flip we do when calculating
      // the projection matrix
      projection[1][1] *= -1.0f;

      ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(projection),
                           index_to_operation(_selected_operation),
                           index_to_locale(_selected_locale),
                           glm::value_ptr(transform.value()));
      if (isUsingComponents || ImGuizmo::IsUsing()) {
        _transform_hierarchy->set_global_location(_selected->transform_id(),
                                                  transform.value());
      }
    }
  }
}

constexpr auto ui_level_editor_t::draw(world_t &world) -> void {
  ImGui::Begin("Level Editor");
  draw_edit_mode();
  ImGui::Separator();
  draw_world_manager(world);
  ImGui::Separator();
  draw_hierarchy(world);
  ImGui::Separator();
  draw_selected(world.camera().view(), world.camera().projection());
  ImGui::End();
}

} // namespace game
