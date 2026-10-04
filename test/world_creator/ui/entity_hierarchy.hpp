#pragma once

#include "../glm_transform_hierarchy.hpp"
#include "../include_glm.hpp"
#include "../light_entity.hpp"
#include "../resources.hpp"
#include "../static_mesh_entity.hpp"
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

struct action_spawn_entity_t {
  enum class spawn_type_t { static_mesh, light };

  spawn_type_t spawn_type;
};

struct action_delete_entity_t {
  transform_hierarchy::transform_id_t transform_id;
};

struct action_move_entity_t {};

struct action_select_entity_t {
  transform_hierarchy::transform_id_t transform_id;
};

struct action_deselect_selected_t {};

struct action_transform_entity_t {};

using action_t =
    std::variant<action_spawn_entity_t, action_delete_entity_t,
                 action_move_entity_t, action_select_entity_t,
                 action_deselect_selected_t, action_transform_entity_t>;

class entity_hierarchy_t {
public:
  using transform_id_t = transform_hierarchy::transform_id_t;

  constexpr entity_hierarchy_t() = default;

  constexpr explicit entity_hierarchy_t(
      world_t *world, glm_transform_hierarchy *transform_hierarchy,
      resources_t *resources, alex::core_t *core);

  constexpr auto update_input(std::span<SDL_Event> events) -> void;

  constexpr auto apply_next_action() -> void;

  constexpr auto undo_last_action() -> void;

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

  std::optional<std::string> _world_path_str;

  std::vector<std::string> _all_models;

  bool _show_entity_hierarchy{true};

  entity_t *_selected{nullptr};
  int _selected_operation{0};
  int _selected_locale{0};

  std::optional<action_t> _next_action;
  std::vector<action_t> _last_actions;
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
    resources_t *resources, alex::core_t *core)
    : _world{world}, _transform_hierarchy{transform_hierarchy},
      _resources{resources}, _core{core} {

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
    _next_action = action_deselect_selected_t{};
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
      ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Selected |
      ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_FramePadding;

  ImGui::PushID(id_index);
  auto const name = std::string(entity->name());
  bool const open = ImGui::TreeNodeEx(name.c_str(), flags);

  if (ImGui::BeginPopupContextItem("NodeContextMenu")) {
    if (ImGui::BeginMenu("Add Child")) {
      if (ImGui::MenuItem("Static Mesh")) {
        _next_action = action_spawn_entity_t{
            action_spawn_entity_t::spawn_type_t::static_mesh};
      }

      if (ImGui::MenuItem("Light")) {
        _next_action =
            action_spawn_entity_t{action_spawn_entity_t::spawn_type_t::light};
      }

      ImGui::EndMenu();
    }

    if (ImGui::MenuItem("Copy Subtree")) {
      std::println("copying subtree");
    }

    if (ImGui::MenuItem("Delete")) {
      _next_action = action_delete_entity_t{entity->transform_id()};
    }

    ImGui::EndPopup();
  }

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
    _next_action = action_select_entity_t{entity->transform_id()};
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

    if (ImGui::Button("Add Light")) {
      auto id = _world->transform_hierarchy().add(glm::mat4(1.0f));
      if (id.has_value()) {
        _world->add_entity(light_entity_t("light", id.value(), spot_light_t{}));
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
constexpr auto simple_colorpicker(glm::vec3 &color) -> bool {
  ImGuiColorEditFlags const flags = ImGuiColorEditFlags_PickerHueBar;
  ImGui::Text(name.c_str());
  constexpr auto label = consteval_string("##") + name;
  bool used = ImGui::ColorPicker3(label.c_str(), glm::value_ptr(color), flags);
  return used;
}

template <consteval_string name>
constexpr auto simple_draggable_float(float &v, float speed) -> bool {
  constexpr auto label = consteval_string("##") + name;
  ImGui::Text("%s", name.c_str());
  ImGui::SameLine();
  bool const used = ImGui::DragFloat(label.c_str(), &v, speed);
  return used;
}

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
    }

    if (auto *static_mesh = _selected->static_mesh()) {
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
              ALEX_ERROR("CANNOT SET RENDERABLE ON STATIC MESH");
//           static_mesh->set_renderable(selected_renderable, _core,
//                                       _renderer, *renderable);
          }
        }
      }

      {
        ImGui::Text("Has Mesh Collider");
        ImGui::SameLine();
        bool has_mesh_collider = static_mesh->has_mesh_collider();
        if (ImGui::Checkbox("##HasMeshCollider", &has_mesh_collider)) {
          static_mesh->set_has_mesh_collider(has_mesh_collider);
        }
      }
    }

    if (light_entity_t *light = _selected->light()) {
      ImGuiColorEditFlags flags = ImGuiColorEditFlags_PickerHueBar;
      ImGui::Text("Color");
      if (ImGui::ColorPicker3("##Color", glm::value_ptr(light->color()),
                              flags)) {
      }

      uiutil::simple_draggable_float<"Intensity">(light->intensity(), 0.1f);

      if (spot_light_t *spot = light->spot()) {
        uiutil::simple_draggable_float<"Inner Cutoff">(spot->inner_cutoff,
                                                       0.1f);
        uiutil::simple_draggable_float<"Outer Cutoff">(spot->outer_cutoff,
                                                       0.1f);
        uiutil::simple_draggable_float<"Range       ">(spot->range, 0.1f);
      } else if (hemisphere_light_t *hemisphere = light->hemisphere()) {
        uiutil::simple_colorpicker<"SkyColor">(hemisphere->sky);
      } else if (point_light_t *point = light->point()) {
        uiutil::simple_draggable_float<"Range">(point->range, 0.1f);
      }
    }
  }
}

template <consteval_string name>
constexpr auto draggable_float(float &v, float speed) -> bool {
  ImGui::PushItemWidth(-1.0f);
  constexpr auto label = consteval_string("##") + name;
  ImGui::Text("%s", name.c_str());
  ImGui::SameLine();
  bool const used = ImGui::DragFloat(label.c_str(), &v, speed);
  ImGui::PopItemWidth();
  return used;
}

#if 0
struct action_spawn_entity_t {};

struct action_delete_entity_t {};

struct action_move_entity_t {};

struct action_select_entity_t {};

struct action_deselect_selected_t {};

struct action_transform_entity_t {};

using action_t =
    std::variant<action_spawn_entity_t, action_delete_entity_t,
                 action_move_entity_t, action_select_entity_t,
                 action_deselect_selected_t, action_transform_entity_t>;
#endif

constexpr auto entity_hierarchy_t::apply_next_action() -> void {
  if (!_next_action.has_value()) {
    return;
  }

  if (auto *spawn_entity =
          std::get_if<action_spawn_entity_t>(&_next_action.value())) {
    std::println("Spawn entity");
  } else if (auto *delete_entity =
                 std::get_if<action_delete_entity_t>(&_next_action.value())) {
    if (_selected != nullptr) {
      if (_selected->transform_id() == delete_entity->transform_id) {
        _selected = nullptr;
      }
    }

    _world->delete_entity_tree_by_transform_id(delete_entity->transform_id);

  } else if (auto *select_entity =
                 std::get_if<action_select_entity_t>(&_next_action.value())) {
    _selected = _world->find_entity(select_entity->transform_id);
  } else if (std::get_if<action_deselect_selected_t>(&_next_action.value())) {
    _selected = nullptr;
  } else {
    ALEX_ERROR("Undefined Scene Hierarchy Action");
  }

  _last_actions.push_back(_next_action.value());
  _next_action = std::nullopt;
}

constexpr auto entity_hierarchy_t::undo_last_action() -> void {}

constexpr auto entity_hierarchy_t::draw(world_t &world) -> void {
  if (ImGui::Begin("Entity Hierarchy")) {
    float child_height = ImGui::GetContentRegionAvail().y * 0.5f;
    if (ImGui::BeginChild("Hierarchy", ImVec2(0.0f, child_height))) {
      draw_edit_mode();
      draw_world_manager(world);
      draw_hierarchy(world);
    }
    ImGui::EndChild();

    ImGui::Separator();
    if (ImGui::BeginChild("Selected")) {
      draw_selected(world.camera().view(), world.camera().projection());
    }
    ImGui::EndChild();
  }
  ImGui::End();

  apply_next_action();
}

} // namespace game::ui
