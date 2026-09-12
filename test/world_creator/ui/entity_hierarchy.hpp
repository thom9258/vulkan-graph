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

#include "consteval_string.hpp"

#include <SDL_keycode.h>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm> // for std::swap
#include <print>
#include <ranges>
#include <span>
#include <string>
#include <string_view>

namespace game::ui {

class entity_hierarchy_t {
public:
  using transform_id_t = transform_hierarchy::transform_id_t;

  constexpr entity_hierarchy_t() = default;

  constexpr explicit entity_hierarchy_t(
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
  world_t *_world{nullptr};
  glm_transform_hierarchy *_transform_hierarchy{nullptr};
  resources_t *_resources{nullptr};
  alex::core_t *_core{nullptr};
  static_render_t *_static_render{nullptr};

  std::optional<std::string> _world_path_str;

  std::vector<std::string> _all_models;

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

constexpr entity_hierarchy_t::entity_hierarchy_t(
    world_t *world, glm_transform_hierarchy *transform_hierarchy,
    resources_t *resources, alex::core_t *core, static_render_t *static_render)
    : _world{world}, _transform_hierarchy{transform_hierarchy},
      _resources{resources}, _core{core}, _static_render{static_render} {

  _all_models = _resources->get_all_renderable_names();
}

constexpr auto entity_hierarchy_t::update_input(std::span<SDL_Event>) -> void {
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

constexpr auto entity_hierarchy_t::find(std::span<entity_t> entities,
                                        transform_id_t id) -> entity_t * {
  for (entity_t &entity : entities) {
    if (entity.transform_id() == id) {
      return &entity;
    }
  }

  return nullptr;
}

constexpr auto entity_hierarchy_t::draw_entity(std::size_t &id_index,
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

constexpr auto entity_hierarchy_t::draw_world_manager(world_t &world) -> void {
  thread_local static std::array<char, 512> world_path_buffer;
  ImGui::InputText("##world manager path", world_path_buffer.data(),
                   world_path_buffer.size(),
                   ImGuiInputTextFlags_EnterReturnsTrue);

  auto not_end = [](char ch) { return ch != '\0'; };

  std::string world_path_str =
      world_path_buffer | std::views::take_while(not_end) |
      std::views::as_rvalue | std::ranges::to<std::string>();

  auto world_path = std::filesystem::path(world_path_str);

  if (ImGui::Button("Save")) {
    world.save_world_v1(world_path);
  }

  ImGui::SameLine();
  if (ImGui::Button("Load")) {
    world.load_world_v1(world_path);
  }
}

constexpr auto entity_hierarchy_t::draw_hierarchy(world_t &world) -> void {
  // https://kahwei.dev/2022/06/20/imgui-tree-node/
  // https://ruby0x1.github.io/machinery_blog_archive/post/implementing-drag-and-drop-in-an-imgui/index.html
  ImGui::Separator();
  ImGui::Text("Entity Hierarchy");
  ImGui::SameLine();
  ImGui::Checkbox("##Entities", &_show_entity_hierarchy);
  if (_show_entity_hierarchy) {
    if (ImGui::Button("Add Static Object")) {
      glm::mat4 model_matrix = glm::mat4(1.0f);
      auto id = _world->transform_hierarchy().add(model_matrix);
      if (id.has_value()) {
        auto entity = static_mesh_entity_t("static-object", id.value());
        _world->add_entity(std::move(entity));
        _selected = &_world->entities().back();
      }
    }

    std::size_t id_index = 0;
    std::vector<transform_id_t> roots = _transform_hierarchy->roots();
    for (transform_id_t root : roots) {
      draw_entity(id_index, world.entities(), root);
    }
  }
}

namespace uiutil {

template <consteval_string name>
constexpr auto draggable_vec3(glm::vec3 &vec, float speed) -> bool {
  ImGui::PushItemWidth(-1.0f);
  constexpr auto label = consteval_string("##") + name;
  ImGui::Text("%s", name.c_str());
  ImGui::SameLine();
  bool const used =
      ImGui::DragFloat3(label.c_str(), glm::value_ptr(vec), speed);
  ImGui::PopItemWidth();
  return used;
}

template <consteval_string name>
constexpr auto input_text(std::span<char> buffer) -> bool {

  constexpr auto label = consteval_string("##") + name;
  ImGui::Text(name.c_str());
  ImGui::SameLine();
  return ImGui::InputText(label.c_str(), buffer.data(), buffer.size(),
                          ImGuiInputTextFlags_EnterReturnsTrue);
}

} // namespace uiutil

constexpr auto entity_hierarchy_t::draw_edit_mode() -> void {
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

constexpr auto entity_hierarchy_t::draw_selected(glm::mat4 view,
                                                 glm::mat4 projection) -> void {
  if (_selected != nullptr) {
    if (_transform_hierarchy->is_valid(_selected->transform_id())) {

      std::array<char, 128> name_buffer;
      std::ranges::fill(name_buffer, '\0');

      auto name = std::string(_selected->name());
      for (std::size_t i = 0; i < name.size(); i++) {
        name_buffer[i] = name[i];
      }

      if (uiutil::input_text<"Name">(name_buffer)) {
        _selected->set_name(name_buffer.data());
      }

      {
        auto local_transform =
            _transform_hierarchy->local_location(_selected->transform_id());

        glm::vec3 translation(0.0f);
        glm::vec3 rotation(0.0f);
        glm::vec3 scale(1.0f);
        ImGuizmo::DecomposeMatrixToComponents(
            glm::value_ptr(local_transform.value()),
            glm::value_ptr(translation), glm::value_ptr(rotation),
            glm::value_ptr(scale));

        bool const using_translation =
            uiutil::draggable_vec3<"Translation">(translation, 0.1f);
        if (using_translation) {
          _selected_operation =
              operation_to_index(ImGuizmo::OPERATION::TRANSLATE);
        }

        bool const using_rotation =
            uiutil::draggable_vec3<"Rotation   ">(rotation, 1.0f);
        if (using_rotation) {
          _selected_operation = operation_to_index(ImGuizmo::OPERATION::ROTATE);
        }

        bool const using_scale =
            uiutil::draggable_vec3<"Scale      ">(scale, 0.1f);
        if (using_scale) {
          _selected_operation = operation_to_index(ImGuizmo::OPERATION::SCALE);
        }

        if (using_translation || using_rotation || using_scale) {
          ImGuizmo::RecomposeMatrixFromComponents(
              glm::value_ptr(translation), glm::value_ptr(rotation),
              glm::value_ptr(scale), glm::value_ptr(local_transform.value()));

          _transform_hierarchy->set_local_location(_selected->transform_id(),
                                                   local_transform.value());
        }
      }
      {
        auto global_transform =
            _transform_hierarchy->global_location(_selected->transform_id());

        ImGuiIO &io = ImGui::GetIO();
        ImGuizmo::SetOrthographic(false);
        ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);

        // imguizmo uses opengl style projection but since we are in vulkan,
        // we need to revert the projection matrix flip we do when calculating
        // the projection matrix
        projection[1][1] *= -1.0f;

        bool manipulated = ImGuizmo::Manipulate(
            glm::value_ptr(view), glm::value_ptr(projection),
            index_to_operation(_selected_operation),
            index_to_locale(_selected_locale),
            glm::value_ptr(global_transform.value()));
        if (manipulated)
          _transform_hierarchy->set_global_location(_selected->transform_id(),
                                                    global_transform.value());
      }

      // auto parent = _transform_hierarchy->parent(_selected->transform_id());
      // if (parent == transform_hierarchy::invalid_transform_id) {
      //   _transform_hierarchy->set_global_location(_selected->transform_id(),
      //                                             global_transform.value());
      // } else {
      //   auto parent_global = _transform_hierarchy->global_location(parent);
      //   glm::mat4 const inv_parent_global =
      //       glm::inverse(parent_global.value_or(glm::mat4(1.0f)));
      //   glm::mat4 const child_local =
      //       inv_parent_global * global_transform.value();
      //   _transform_hierarchy->set_local_location(_selected->transform_id(),
      //                                            child_local);
      // }
    }

    if (auto *static_mesh = _selected->get<static_mesh_entity_t>()) {

      std::optional<std::string_view> renderable_name =
          static_mesh->renderable_name();
      std::vector<const char *> model_ptrs;
      if (renderable_name.has_value()) {
        model_ptrs.push_back(renderable_name.value().data());
      } else {
        static const char *none{"<none>"};
        model_ptrs.push_back(none);
      }

      for (std::string &model : _all_models) {
        model_ptrs.push_back(model.c_str());
      }

      int selected_entity{0};
      ImGui::Text("Model");
      ImGui::SameLine();
      if (ImGui::Combo("##Model List", &selected_entity, model_ptrs.data(),
                       model_ptrs.size())) {
        if (selected_entity != 0) {
          std::string selected_renderable = _all_models[selected_entity - 1];
          renderable_t *renderable =
              _resources->get_renderable(selected_renderable);

          if (renderable != nullptr) {
            static_mesh->set_renderable(selected_renderable, _core,
                                        _static_render, *renderable);
          }
        }
      }
    }
  }
}

constexpr auto entity_hierarchy_t::draw(world_t &world) -> void {
  ImGui::Begin("Entity Hierarchy");
  draw_edit_mode();
  ImGui::Separator();
  draw_world_manager(world);
  ImGui::Separator();
  draw_hierarchy(world);
  ImGui::Separator();
  draw_selected(world.camera().view(), world.camera().projection());
  ImGui::End();
}

} // namespace game::ui
